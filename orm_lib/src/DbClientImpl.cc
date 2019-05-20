/**
 *
 *  DbClientImpl.cc
 *  An Tao
 *
 *  Copyright 2018, An Tao.  All rights reserved.
 *  https://github.com/an-tao/drogon
 *  Use of this source code is governed by a MIT license
 *  that can be found in the License file.
 *
 *  Drogon
 *
 */

#include "DbClientImpl.h"
#include "DbConnection.h"
#if USE_POSTGRESQL
#include "postgresql_impl/PgConnection.h"
#endif
#if USE_MYSQL
#include "mysql_impl/MysqlConnection.h"
#endif
#if USE_SQLITE3
#include "sqlite3_impl/Sqlite3Connection.h"
#endif
#include "TransactionImpl.h"
#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Exception.h>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdio.h>
#include <sys/select.h>
#include <thread>
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <unistd.h>
#include <unordered_set>
#include <vector>

using namespace drogon::orm;

DbClientImpl::DbClientImpl(const std::string &connInfo,
                           const size_t connNum,
                           ClientType type)
    : _connectNum(connNum),
      _loops(type == ClientType::Sqlite3
                 ? 1
                 : (connNum < std::thread::hardware_concurrency()
                        ? connNum
                        : std::thread::hardware_concurrency()),
             "DbLoop")
{
    _type = type;
    _connInfo = connInfo;
    LOG_TRACE << "type=" << (int)type;
    // LOG_DEBUG << _loops.getLoopNum();
    assert(connNum > 0);
    _loops.start();
    if (type == ClientType::PostgreSQL)
    {
        std::thread([this]() {
            for (size_t i = 0; i < _connectNum; i++)
            {
                auto loop = _loops.getNextLoop();
                loop->runInLoop([this, loop]() {
                    std::lock_guard<std::mutex> lock(_connectionsMutex);
                    _connections.insert(newConnection(loop));
                });
            }
        }).detach();
    }
    else if (type == ClientType::Mysql)
    {
        std::thread([this]() {
            for (size_t i = 0; i < _connectNum; i++)
            {
                auto loop = _loops.getNextLoop();
                loop->runAfter(0.1 * (i + 1), [this, loop]() {
                    std::lock_guard<std::mutex> lock(_connectionsMutex);
                    _connections.insert(newConnection(loop));
                });
            }
        }).detach();
    }
    else if (type == ClientType::Sqlite3)
    {
        _sharedMutexPtr = std::make_shared<SharedMutex>();
        assert(_sharedMutexPtr);
        auto loop = _loops.getNextLoop();
        loop->runInLoop([this]() {
            std::lock_guard<std::mutex> lock(_connectionsMutex);
            for (size_t i = 0; i < _connectNum; i++)
            {
                _connections.insert(newConnection(nullptr));
            }
        });
    }
}

DbClientImpl::~DbClientImpl() noexcept
{
    std::lock_guard<std::mutex> lock(_connectionsMutex);
    for (auto const &conn : _connections)
    {
        conn->disconnect();
    }
    _connections.clear();
    _readyConnections.clear();
    _busyConnections.clear();
}

void DbClientImpl::execSql(
    const DbConnectionPtr &conn,
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    if (!conn)
    {
        try
        {
            throw BrokenConnection("There is no connection to PG server!");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
        return;
    }
    conn->execSql(std::move(sql),
                  paraNum,
                  std::move(parameters),
                  std::move(length),
                  std::move(format),
                  std::move(rcb),
                  std::move(exceptCallback));
}
void DbClientImpl::execSql(
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    DbConnectionPtr conn;
    {
        std::lock_guard<std::mutex> guard(_connectionsMutex);

        if (_readyConnections.size() == 0)
        {
            if (_busyConnections.size() == 0)
            {
                try
                {
                    throw BrokenConnection("No connection to database server");
                }
                catch (...)
                {
                    exceptCallback(std::current_exception());
                }
                return;
            }
        }
        else
        {
            auto iter = _readyConnections.begin();
            _busyConnections.insert(*iter);
            conn = *iter;
            _readyConnections.erase(iter);
        }
    }
    if (conn)
    {
        execSql(conn,
                std::move(sql),
                paraNum,
                std::move(parameters),
                std::move(length),
                std::move(format),
                std::move(rcb),
                std::move(exceptCallback));
        return;
    }
    bool busy = false;
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        if (_sqlCmdBuffer.size() > 200000)
        {
            // too many queries in buffer;
            busy = true;
        }
    }
    if (busy)
    {
        try
        {
            throw Failure("Too many queries in buffer");
        }
        catch (...)
        {
            exceptCallback(std::current_exception());
        }
        return;
    }
    // LOG_TRACE << "Push query to buffer";
    std::shared_ptr<SqlCmd> cmd =
        std::make_shared<SqlCmd>(std::move(sql),
                                 paraNum,
                                 std::move(parameters),
                                 std::move(length),
                                 std::move(format),
                                 std::move(rcb),
                                 std::move(exceptCallback));
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        _sqlCmdBuffer.push_back(std::move(cmd));
    }
}
void DbClientImpl::newTransactionAsync(
    const std::function<void(const std::shared_ptr<Transaction> &)> &callback)
{
    DbConnectionPtr conn;
    {
        std::lock_guard<std::mutex> lock(_connectionsMutex);
        if (!_readyConnections.empty())
        {
            auto iter = _readyConnections.begin();
            _busyConnections.insert(*iter);
            conn = *iter;
            _readyConnections.erase(iter);
        }
    }
    if (conn)
    {
        makeTrans(conn,
                  std::function<void(const std::shared_ptr<Transaction> &)>(
                      callback));
    }
    else
    {
        std::lock_guard<std::mutex> lock(_transMutex);
        _transCallbacks.push(callback);
    }
}
void DbClientImpl::makeTrans(
    const DbConnectionPtr &conn,
    std::function<void(const std::shared_ptr<Transaction> &)> &&callback)
{
    std::weak_ptr<DbClientImpl> weakThis = shared_from_this();
    auto trans = std::shared_ptr<TransactionImpl>(new TransactionImpl(
        _type, conn, std::function<void(bool)>(), [weakThis, conn]() {
            auto thisPtr = weakThis.lock();
            if (!thisPtr)
                return;
            if (conn->status() == ConnectStatus_Bad)
            {
                return;
            }
            {
                std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
                if (thisPtr->_connections.find(conn) ==
                        thisPtr->_connections.end() &&
                    thisPtr->_busyConnections.find(conn) ==
                        thisPtr->_busyConnections.find(conn))
                {
                    // connection is broken and removed
                    return;
                }
            }
            conn->loop()->queueInLoop([weakThis, conn]() {
                auto thisPtr = weakThis.lock();
                if (!thisPtr)
                    return;
                std::weak_ptr<DbConnection> weakConn = conn;
                conn->setIdleCallback([weakThis, weakConn]() {
                    auto thisPtr = weakThis.lock();
                    if (!thisPtr)
                        return;
                    auto connPtr = weakConn.lock();
                    if (!connPtr)
                        return;
                    thisPtr->handleNewTask(connPtr);
                });
                thisPtr->handleNewTask(conn);
            });
        }));
    trans->doBegin();
    conn->loop()->queueInLoop(
        [callback = std::move(callback), trans]() { callback(trans); });
}
std::shared_ptr<Transaction> DbClientImpl::newTransaction(
    const std::function<void(bool)> &commitCallback)
{
    std::promise<std::shared_ptr<Transaction>> pro;
    auto f = pro.get_future();
    newTransactionAsync([&pro](const std::shared_ptr<Transaction> &trans) {
        pro.set_value(trans);
    });
    auto trans = f.get();
    trans->setCommitCallback(commitCallback);
    return trans;
}

void DbClientImpl::handleNewTask(const DbConnectionPtr &connPtr)
{
    std::function<void(const std::shared_ptr<Transaction> &)> transCallback;
    {
        std::lock_guard<std::mutex> guard(_transMutex);
        if (!_transCallbacks.empty())
        {
            transCallback = std::move(_transCallbacks.front());
            _transCallbacks.pop();
        }
    }
    if (transCallback)
    {
        makeTrans(connPtr, std::move(transCallback));
        return;
    }
    // Then check if there are some sql queries in the buffer
    std::shared_ptr<SqlCmd> cmd;
    {
        std::lock_guard<std::mutex> guard(_bufferMutex);
        if (!_sqlCmdBuffer.empty())
        {
            cmd = std::move(_sqlCmdBuffer.front());
            _sqlCmdBuffer.pop_front();
        }
    }
    if (cmd)
    {
        execSql(connPtr,
                std::move(cmd->_sql),
                cmd->_paraNum,
                std::move(cmd->_parameters),
                std::move(cmd->_length),
                std::move(cmd->_format),
                std::move(cmd->_cb),
                std::move(cmd->_exceptCb));
        return;
    }
    // Connection is idle, put it into the _readyConnections set;
    {
        std::lock_guard<std::mutex> guard(_connectionsMutex);
        _busyConnections.erase(connPtr);
        _readyConnections.insert(connPtr);
    }
}

DbConnectionPtr DbClientImpl::newConnection(trantor::EventLoop *loop)
{
    DbConnectionPtr connPtr;
    if (_type == ClientType::PostgreSQL)
    {
#if USE_POSTGRESQL
        connPtr = std::make_shared<PgConnection>(loop, _connInfo);
#else
        return nullptr;
#endif
    }
    else if (_type == ClientType::Mysql)
    {
#if USE_MYSQL
        connPtr = std::make_shared<MysqlConnection>(loop, _connInfo);
#else
        return nullptr;
#endif
    }
    else if (_type == ClientType::Sqlite3)
    {
#if USE_SQLITE3
        connPtr = std::make_shared<Sqlite3Connection>(loop,
                                                      _connInfo,
                                                      _sharedMutexPtr);
#else
        return nullptr;
#endif
    }
    else
    {
        return nullptr;
    }

    std::weak_ptr<DbClientImpl> weakPtr = shared_from_this();
    connPtr->setCloseCallback([weakPtr](const DbConnectionPtr &closeConnPtr) {
        // Erase the connection
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
            thisPtr->_readyConnections.erase(closeConnPtr);
            thisPtr->_busyConnections.erase(closeConnPtr);
            assert(thisPtr->_connections.find(closeConnPtr) !=
                   thisPtr->_connections.end());
            thisPtr->_connections.erase(closeConnPtr);
        }
        // Reconnect after 1 second
        auto loop = closeConnPtr->loop();
        loop->runAfter(1, [weakPtr, loop] {
            auto thisPtr = weakPtr.lock();
            if (!thisPtr)
                return;
            std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
            thisPtr->_connections.insert(thisPtr->newConnection(loop));
        });
    });
    connPtr->setOkCallback([weakPtr](const DbConnectionPtr &okConnPtr) {
        LOG_TRACE << "connected!";
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        {
            std::lock_guard<std::mutex> guard(thisPtr->_connectionsMutex);
            thisPtr->_busyConnections.insert(
                okConnPtr);  // For new connections, this sentence is necessary
        }
        thisPtr->handleNewTask(okConnPtr);
    });
    std::weak_ptr<DbConnection> weakConn = connPtr;
    connPtr->setIdleCallback([weakPtr, weakConn]() {
        auto thisPtr = weakPtr.lock();
        if (!thisPtr)
            return;
        auto connPtr = weakConn.lock();
        if (!connPtr)
            return;
        thisPtr->handleNewTask(connPtr);
    });
    // std::cout<<"newConn end"<<connPtr<<std::endl;
    return connPtr;
}
