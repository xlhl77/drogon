/**
 *
 *  MysqlConnection.cc
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

#include "MysqlConnection.h"
#include "MysqlResultImpl.h"
#include <algorithm>
#include <drogon/utils/Utilities.h>
#include <poll.h>
#include <regex>

using namespace drogon::orm;
namespace drogon
{
namespace orm
{
Result makeResult(
    const std::shared_ptr<MYSQL_RES> &r = std::shared_ptr<MYSQL_RES>(nullptr),
    const std::string &query = "",
    Result::size_type affectedRows = 0,
    unsigned long long insertId = 0)
{
    return Result(std::shared_ptr<MysqlResultImpl>(
        new MysqlResultImpl(r, query, affectedRows, insertId)));
}

}  // namespace orm
}  // namespace drogon

MysqlConnection::MysqlConnection(trantor::EventLoop *loop,
                                 const std::string &connInfo)
    : DbConnection(loop),
      _mysqlPtr(
          std::shared_ptr<MYSQL>(new MYSQL, [](MYSQL *p) { mysql_close(p); }))
{
    mysql_init(_mysqlPtr.get());
    mysql_options(_mysqlPtr.get(), MYSQL_OPT_NONBLOCK, 0);

    // Get the key and value
    std::regex r(" *= *");
    auto tmpStr = std::regex_replace(connInfo, r, "=");
    std::string host, user, passwd, dbname, port;
    auto keyValues = utils::splitString(tmpStr, " ");
    for (auto const &kvs : keyValues)
    {
        auto kv = utils::splitString(kvs, "=");
        assert(kv.size() == 2);
        auto key = kv[0];
        auto value = kv[1];
        if (value[0] == '\'' && value[value.length() - 1] == '\'')
        {
            value = value.substr(1, value.length() - 2);
        }
        std::transform(key.begin(), key.end(), key.begin(), tolower);
        // LOG_TRACE << key << "=" << value;
        if (key == "host")
        {
            host = value;
        }
        else if (key == "user")
        {
            user = value;
        }
        else if (key == "dbname")
        {
            // LOG_DEBUG << "database:[" << value << "]";
            dbname = value;
        }
        else if (key == "port")
        {
            port = value;
        }
        else if (key == "password")
        {
            passwd = value;
        }
    }
    _loop->queueInLoop([=]() {
        MYSQL *ret;
        _status = ConnectStatus_Connecting;
        _waitStatus =
            mysql_real_connect_start(&ret,
                                     _mysqlPtr.get(),
                                     host.empty() ? NULL : host.c_str(),
                                     user.empty() ? NULL : user.c_str(),
                                     passwd.empty() ? NULL : passwd.c_str(),
                                     dbname.empty() ? NULL : dbname.c_str(),
                                     port.empty() ? 3306 : atol(port.c_str()),
                                     NULL,
                                     0);
        // LOG_DEBUG << ret;
        auto fd = mysql_get_socket(_mysqlPtr.get());
        _channelPtr =
            std::unique_ptr<trantor::Channel>(new trantor::Channel(loop, fd));
        _channelPtr->setCloseCallback([=]() {
            perror("sock close");
            handleClosed();
        });
        _channelPtr->setEventCallback([=]() { handleEvent(); });
        setChannel();
    });
}

void MysqlConnection::setChannel()
{
    if ((_waitStatus & MYSQL_WAIT_READ) || (_waitStatus & MYSQL_WAIT_EXCEPT))
    {
        if (!_channelPtr->isReading())
            _channelPtr->enableReading();
    }
    if (_waitStatus & MYSQL_WAIT_WRITE)
    {
        if (!_channelPtr->isWriting())
            _channelPtr->enableWriting();
    }
    else
    {
        if (_channelPtr->isWriting())
            _channelPtr->disableWriting();
    }
    if (_waitStatus & MYSQL_WAIT_TIMEOUT)
    {
        auto timeout = mysql_get_timeout_value(_mysqlPtr.get());
        auto thisPtr = shared_from_this();
        _loop->runAfter(timeout, [thisPtr]() { thisPtr->handleTimeout(); });
    }
}

void MysqlConnection::handleClosed()
{
    _loop->assertInLoopThread();
    if (_status == ConnectStatus_Bad)
        return;
    _status = ConnectStatus_Bad;
    _channelPtr->disableAll();
    _channelPtr->remove();
    assert(_closeCb);
    auto thisPtr = shared_from_this();
    _closeCb(thisPtr);
}
void MysqlConnection::disconnect()
{
    auto thisPtr = shared_from_this();
    std::promise<int> pro;
    auto f = pro.get_future();
    _loop->runInLoop([thisPtr, &pro]() {
        thisPtr->_status = ConnectStatus_Bad;
        thisPtr->_channelPtr->disableAll();
        thisPtr->_channelPtr->remove();
        thisPtr->_mysqlPtr.reset();
        pro.set_value(1);
    });
    f.get();
}
void MysqlConnection::handleTimeout()
{
    LOG_TRACE << "channel index:" << _channelPtr->index();
    int status = 0;
    status |= MYSQL_WAIT_TIMEOUT;

    if (_status == ConnectStatus_Connecting)
    {
        if (!onEventConnect(status)) return;
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
    }
}
void MysqlConnection::handleEvent()
{
    int status = 0;
    auto revents = _channelPtr->revents();
    if (revents & POLLIN)
        status |= MYSQL_WAIT_READ;
    if (revents & POLLOUT)
        status |= MYSQL_WAIT_WRITE;
    if (revents & POLLPRI)
        status |= MYSQL_WAIT_EXCEPT;
    status = (status & _waitStatus);
    if (status == 0)
        return;
    if (_status == ConnectStatus_Connecting)
    {
        if (!onEventConnect(status)) return;
        setChannel();
    }
    else if (_status == ConnectStatus_Ok)
    {
        switch (_execStatus)
        {
        case ExecStatus_Prepare:
        {
            onEventPrepare(status);
            setChannel();
            break;
        }
        case ExecStatus_Execute:
        {
            onEventExecute(status);
            setChannel();
            break;
        }
        case ExecStatus_StoreResult:
        {
            onEventResult(status);
            setChannel();
            break;
        }
        case ExecStatus_FetchRow:
        {
            onEventFetchRow(status);
            setChannel();
            break;
        }        
        case ExecStatus_None:
        {
            //Connection closed!
            if (_waitStatus == 0)
                handleClosed();
            break;
        }
        default:
            return;
        }
    }
}

void MysqlConnection::execSqlInLoop(
    std::string &&sql,
    size_t paraNum,
    std::vector<const char *> &&parameters,
    std::vector<int> &&length,
    std::vector<int> &&format,
    ResultCallback &&rcb,
    std::function<void(const std::exception_ptr &)> &&exceptCallback)
{
    LOG_TRACE << sql;
    assert(paraNum == parameters.size());
    assert(paraNum == length.size());
    assert(paraNum == format.size());
    assert(rcb);
    assert(!_isWorking);
    assert(!sql.empty());

    _cb = std::move(rcb);
    _isWorking = true;
    _exceptCb = std::move(exceptCallback);
    _sql = sql;

    LOG_TRACE << _sql;

    // 生成参数绑定
    for(size_t i = 0; i< paraNum; i++)
        bind_param(parameters.at(i), i, format[i], length[i]);

    LOG_TRACE << "prepare start";
    
    if (!onEventPrepareStart()) return;
    setChannel();
    return;
}

void MysqlConnection::outputError()
{
    _channelPtr->disableAll();
    LOG_ERROR << "Error(" << mysql_errno(_mysqlPtr.get()) << ") ["
              << mysql_sqlstate(_mysqlPtr.get()) << "] \""
              << mysql_error(_mysqlPtr.get()) << "\"";
    if (_isWorking)
    {
        try
        {
            // TODO: exception type
            throw SqlError(mysql_error(_mysqlPtr.get()), _sql);
        }
        catch (...)
        {
            _exceptCb(std::current_exception());
            _exceptCb = nullptr;
        }

        _cb = nullptr;
        _isWorking = false;
        _idleCb();
    }
}

void MysqlConnection::getResult()
{
    // mysql_stmt_affected_rows(_stmtPtr.get()), mysql_stmt_insert_id(_stmtPtr.get()));
    if (_isWorking)
    {
        _cb(Result(_resultPtr));
        _cb = nullptr;
        _exceptCb = nullptr;
        _isWorking = false;
        _idleCb();
    }
}

bool MysqlConnection::onEventConnect(int status)
{
    MYSQL *ret;
    _waitStatus = mysql_real_connect_cont(&ret, _mysqlPtr.get(), status);
    if (_waitStatus == 0)
    {
        if (!ret)
        {
            handleClosed();
            //perror("");
            LOG_ERROR << "Failed to mysql_real_connect()";
            return false;
        }
        _status = ConnectStatus_Ok;

        //设置字符集
        mysql_set_character_set(_mysqlPtr.get(), "utf8");
        
        if (_okCb)
        {
            auto thisPtr = shared_from_this();
            _okCb(thisPtr);
        }
    }
    return true;
}

bool MysqlConnection::onEventPrepareStart()
{
    _execStatus = ExecStatus_Prepare;
    int err;
    _stmtPtr.reset(mysql_stmt_init(_mysqlPtr.get()), [](MYSQL_STMT *p) {
          if (p) mysql_stmt_close(p);
      });
    if (!_stmtPtr)
    {
            LOG_ERROR << "error init stmt!";
            outputError();
            return false;        
    }
    _waitStatus = mysql_stmt_prepare_start(&err, _stmtPtr.get(), _sql.c_str(), _sql.length());
    LOG_TRACE << "stmt_prepare_start:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            LOG_ERROR << "error";
            outputError();
            return false;
        }
        return onEventExecuteStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventPrepare(int status)
{
    int err = 0;
    _waitStatus = mysql_stmt_prepare_cont(&err, _stmtPtr.get(), status);
    LOG_TRACE << "stmt_prepare_cont:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            _execStatus = ExecStatus_None;
            LOG_ERROR << "error:" << err << " status:" << status;
            outputError();
            return false;
        }
        return onEventExecuteStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventExecuteStart()
{
    _execStatus = ExecStatus_Execute;

    mysql_stmt_bind_param(_stmtPtr.get(), _binds.data());

    int err = 0;
    _waitStatus = mysql_stmt_execute_start(&err, _stmtPtr.get());
    LOG_TRACE << "stmt_execute_start:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            _execStatus = ExecStatus_None;
            LOG_ERROR << "error:" << err;
            outputError();
            return false;
        }
        return onEventResultStart();
    }
    setChannel();
    return true;    
}

bool MysqlConnection::onEventExecute(int status)
{
    int err = 0;
    _waitStatus = mysql_stmt_execute_cont(&err, _stmtPtr.get(), status);
    LOG_TRACE << "stmt_execute:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            _execStatus = ExecStatus_None;
            LOG_ERROR << "error:" << err;
            outputError();
            return false;
        }
        return onEventResultStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventResultStart()
{
    _execStatus = ExecStatus_StoreResult;
    //绑定结果
    auto resultPtr = std::shared_ptr<MYSQL_RES>(mysql_stmt_result_metadata(_stmtPtr.get()), [](MYSQL_RES *r) {
        mysql_free_result(r);
    });

    _resultPtr = std::make_shared<MysqlResultImpl>(resultPtr, _sql, 0, 0);

    int err;
    _waitStatus = mysql_stmt_store_result_start(&err, _stmtPtr.get());
    LOG_TRACE << "stmt_store_result_start:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            LOG_ERROR << "error";
            outputError();
            return false;
        }
        return onEventFetchRowStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventResult(int status)
{
    //绑定结果
    int err;
    _waitStatus = mysql_stmt_store_result_cont(&err, _stmtPtr.get(), status);
    LOG_TRACE << "stmt_store_result:" << err;
    if (_waitStatus == 0)
    {
        if (err)
        {
            LOG_ERROR << "error";
            outputError();
            return false;
        }
        return onEventFetchRowStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventFetchRowStart()
{
    _execStatus = ExecStatus_FetchRow;
    if (mysql_stmt_bind_result(_stmtPtr.get(), _resultPtr->addRow()))
    {
        LOG_ERROR << "bind_result error!" << mysql_stmt_error(_stmtPtr.get());
        outputError();
        return false;
    }

    int err;
    _waitStatus = mysql_stmt_fetch_start(&err, _stmtPtr.get());
    LOG_TRACE << "fetch_row_start!";
    if (_waitStatus == 0)
    {
        if (err == MYSQL_NO_DATA)
        {
            _execStatus = ExecStatus_None;
            getResult();
            return true;
        }
        else if (err)
        {
            LOG_ERROR << "error " << mysql_stmt_error(_stmtPtr.get());
            outputError();
            return false;
        }
        return onEventFetchRowStart();
    }
    setChannel();
    return true;
}

bool MysqlConnection::onEventFetchRow(int status)
{
    int err;
    _waitStatus = mysql_stmt_fetch_cont(&err, _stmtPtr.get(), status);
    LOG_TRACE << "stmt_fetch_row:" << status;
    if (_waitStatus == 0)
    {
        if (err)
        {
            LOG_ERROR << "error";
            outputError();
            return false;
        }
        return onEventFetchRowStart();
    }
    setChannel();
    return true;
}

void MysqlConnection::bind_param(const char *param, size_t idx, int format, int length)
{
    assert(idx < _binds.size());
    auto &bind = _binds[idx];
    bind.buffer = (char*)param;
    bind.buffer_type = (enum_field_types)format;
    switch (format)
    {
    case MYSQL_TYPE_TINY:
    case MYSQL_TYPE_SHORT:
    case MYSQL_TYPE_LONG:
    case MYSQL_TYPE_LONGLONG:
    case MYSQL_TYPE_NULL:
        break;
    case MYSQL_TYPE_STRING:
    {
        bind.buffer_length = std::strlen(param);
        // bind.length = &length;
    }
    }    
}
