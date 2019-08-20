/**
 *
 *  DbClientLockFree.h
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

#include "DbConnection.h"
#include <drogon/orm/DbClient.h>
#include <trantor/net/EventLoopThreadPool.h>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>

namespace drogon
{
namespace orm
{
class DbClientLockFree : public DbClient,
                         public std::enable_shared_from_this<DbClientLockFree>
{
  public:
    DbClientLockFree(const std::string &connInfo,
                     trantor::EventLoop *loop,
                     ClientType type,
                     size_t connectionNumberPerLoop);
    virtual ~DbClientLockFree() noexcept;
    virtual void execSql(std::string &&sql,
                         size_t paraNum,
                         std::vector<const char *> &&parameters,
                         std::vector<int> &&length,
                         std::vector<int> &&format,
                         ResultCallback &&rcb,
                         std::function<void(const std::exception_ptr &)>
                             &&exceptCallback) override;
    virtual std::shared_ptr<Transaction> newTransaction(
        const std::function<void(bool)> &commitCallback = nullptr) override;
    virtual void newTransactionAsync(
        const std::function<void(const std::shared_ptr<Transaction> &)>
            &callback) override;

  private:
    std::string _connInfo;
    trantor::EventLoop *_loop;
    DbConnectionPtr newConnection();
    const size_t _connectionNum;
    std::vector<DbConnectionPtr> _connections;
    std::vector<DbConnectionPtr> _connectionHolders;
    std::unordered_set<DbConnectionPtr> _transSet;
    std::deque<std::shared_ptr<SqlCmd>> _sqlCmdBuffer;

    std::queue<std::function<void(const std::shared_ptr<Transaction> &)>>
        _transCallbacks;

    void makeTrans(
        const DbConnectionPtr &conn,
        std::function<void(const std::shared_ptr<Transaction> &)> &&callback);

    void handleNewTask(const DbConnectionPtr &conn);
    size_t _connectionPos = 0;  // Used for pg batch mode.
};

}  // namespace orm
}  // namespace drogon
