/**
 *
 *  DbConnection.h
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

#include <drogon/config.h>
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoop.h>
#include <trantor/net/inner/Channel.h>
#include <trantor/utils/NonCopyable.h>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

namespace drogon
{
namespace orm
{
#if (CXX_STD > 14)
typedef std::shared_mutex SharedMutex;
#else
typedef std::shared_timed_mutex SharedMutex;
#endif

enum ConnectStatus
{
    ConnectStatus_None = 0,
    ConnectStatus_Connecting,
    ConnectStatus_Ok,
    ConnectStatus_Bad
};

struct SqlCmd
{
    std::string _sql;
    size_t _paraNum;
    std::vector<const char *> _parameters;
    std::vector<int> _length;
    std::vector<int> _format;
    QueryCallback _cb;
    ExceptPtrCallback _exceptCb;
    std::string _preparingStatement;
    SqlCmd(std::string &&sql,
           const size_t paraNum,
           std::vector<const char *> &&parameters,
           std::vector<int> &&length,
           std::vector<int> &&format,
           QueryCallback &&cb,
           ExceptPtrCallback &&exceptCb)
        : _sql(std::move(sql)),
          _paraNum(paraNum),
          _parameters(std::move(parameters)),
          _length(std::move(length)),
          _format(std::move(format)),
          _cb(std::move(cb)),
          _exceptCb(std::move(exceptCb))
    {
    }
};

class DbConnection;
typedef std::shared_ptr<DbConnection> DbConnectionPtr;
class DbConnection : public trantor::NonCopyable
{
  public:
    typedef std::function<void(const DbConnectionPtr &)> DbConnectionCallback;
    DbConnection(trantor::EventLoop *loop) : _loop(loop)
    {
    }
    void setOkCallback(const DbConnectionCallback &cb)
    {
        _okCb = cb;
    }
    void setCloseCallback(const DbConnectionCallback &cb)
    {
        _closeCb = cb;
    }
    void setIdleCallback(const std::function<void()> &cb)
    {
        _idleCb = cb;
    }
    virtual void execSql(
        std::string &&sql,
        size_t paraNum,
        std::vector<const char *> &&parameters,
        std::vector<int> &&length,
        std::vector<int> &&format,
        ResultCallback &&rcb,
        std::function<void(const std::exception_ptr &)> &&exceptCallback) = 0;
    virtual void batchSql(
        std::deque<std::shared_ptr<SqlCmd>> &&sqlCommands) = 0;
    virtual ~DbConnection()
    {
        LOG_TRACE << "Destruct DbConn" << this;
    }
    ConnectStatus status() const
    {
        return _status;
    }
    trantor::EventLoop *loop()
    {
        return _loop;
    }
    virtual void disconnect() = 0;
    bool isWorking()
    {
        return _isWorking;
    }

  protected:
    QueryCallback _cb;
    trantor::EventLoop *_loop;
    std::function<void()> _idleCb;
    ConnectStatus _status = ConnectStatus_None;
    DbConnectionCallback _closeCb = [](const DbConnectionPtr &) {};
    DbConnectionCallback _okCb = [](const DbConnectionPtr &) {};
    std::function<void(const std::exception_ptr &)> _exceptCb;
    bool _isWorking = false;
    std::string _sql = "";
};

}  // namespace orm
}  // namespace drogon
