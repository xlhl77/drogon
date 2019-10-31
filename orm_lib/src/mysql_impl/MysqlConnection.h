/**
 *
 *  MysqlConnection.h
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

#pragma once

#include "../DbConnection.h"
#include "MysqlResultImpl.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mysql.h>
#include <string>

namespace drogon
{
namespace orm
{
class MysqlConnection;
typedef std::shared_ptr<MysqlConnection> MysqlConnectionPtr;
class MysqlConnection : public DbConnection,
                        public std::enable_shared_from_this<MysqlConnection>
{
  public:
    MysqlConnection(trantor::EventLoop *loop, const std::string &connInfo);
    ~MysqlConnection()
    {
    }
    virtual bool changeDb(const std::string &dbName) override;
    virtual void execSql(const std::string &name,
                         std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)>
                             &&exceptCallback) override
    {
        if (_loop->isInLoopThread())
        {
            LOG_TRACE << "exec sql inloop\n";
            execSqlInLoop(name,
                          std::move(sql),
                          paraNum,
                          std::move(parameters),
                          std::move(length),
                          std::move(format),
                          std::move(rcb),
                          std::move(exceptCallback));
        }
        else
        {
            LOG_TRACE << "put sql in queue\n";
            auto thisPtr = shared_from_this();
            _loop->queueInLoop(
                [thisPtr,
                 dbname = name,
                 sql = std::move(sql),
                 paraNum,
                 parameters = std::move(parameters),
                 length = std::move(length),
                 format = std::move(format),
                 rcb = std::move(rcb),
                 exceptCallback = std::move(exceptCallback)]() mutable {
                    thisPtr->execSqlInLoop(dbname,
                                           std::move(sql),
                                           paraNum,
                                           std::move(parameters),
                                           std::move(length),
                                           std::move(format),
                                           std::move(rcb),
                                           std::move(exceptCallback));
                });
        }
    }
    virtual void batchSql(std::deque<std::shared_ptr<SqlCmd>> &&) override
    {
        LOG_FATAL << "The mysql library does not support batch mode";
        exit(1);
    }
    virtual void disconnect() override;

  private:
    void execSqlInLoop(const std::string &name,
                       std::string &&sql,
                       size_t paraNum,
                       std::vector<const char *> &&parameters,
                       std::vector<int> &&length,
                       std::vector<int> &&format,
                       ResultCallback &&rcb,
                       std::function<void(const std::exception_ptr &)> &&exceptCallback);

    bool onEventConnect(int status);
    bool onEventPrepare(int status);
    bool onEventExecute(int status);
    bool onEventResult(int status);
    bool onEventFetchRow(int status);
    bool onEventPrepareStart();
    bool onEventExecuteStart();
    bool onEventResultStart();
    bool onEventFetchRowStart();
    bool onEventQuery(int status);
    bool onEventStoreResult(int status);
    void initResult();

    void bind_param(const char * param, size_t idx, int format, int length);

    std::unique_ptr<trantor::Channel> _channelPtr;
    std::shared_ptr<MYSQL> _mysqlPtr;
    std::shared_ptr<MYSQL_STMT> _stmtPtr;

    void handleTimeout();

    void handleClosed();
    void handleEvent();
    void setChannel();
    void getResult(MYSQL_RES *res = nullptr);
    int _waitStatus;
    enum ExecStatus
    {
        ExecStatus_None = 0,
        ExecStatus_Prepare,
        ExecStatus_Execute,
        ExecStatus_StoreResult,
        ExecStatus_FetchRow,
        ExecStatus_RealQuery,
        ExecStatus_Result
    };
    ExecStatus _execStatus = ExecStatus_None;

    void outputError();
    std::array<MYSQL_BIND, 64> _binds;
    std::shared_ptr<MysqlResultImpl> _resultPtr = nullptr;
    std::string dbUser;
    std::string dbPwd;
};

}  // namespace orm
}  // namespace drogon