/**
 *
 *  ResultImpl.h
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

#include <trantor/utils/NonCopyable.h>
#include <drogon/orm/Result.h>
namespace drogon
{
namespace orm
{

class ResultImpl : public trantor::NonCopyable, public Result
{
  public:
    ResultImpl(const std::string &query) : _query(query) {}
    virtual size_type size() const noexcept = 0;
    virtual row_size_type columns() const noexcept = 0;
    virtual const char *columnName(row_size_type Number) const = 0;
    virtual size_type affectedRows() const noexcept = 0;
    virtual row_size_type columnNumber(const char colName[]) const = 0;
    virtual const char *getValue(size_type row, row_size_type column) const = 0;
    virtual bool isNull(size_type row, row_size_type column) const = 0;
    virtual field_size_type getLength(size_type row, row_size_type column) const = 0;
    virtual const std::string &sql() const { return _query; }
    virtual unsigned long long insertId() const noexcept { return 0; }
    virtual int oid(row_size_type column) const { return 0; }
    virtual bool toJson(json &result) const noexcept {return false; }
    virtual ~ResultImpl() {}

  private:
    std::string _query;
};

} // namespace orm
} // namespace drogon
