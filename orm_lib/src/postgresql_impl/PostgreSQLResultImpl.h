/**
 *
 *  PostgreSQLResultImpl.h
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

#include "../ResultImpl.h"

#include <libpq-fe.h>
#include <memory>
#include <string>

namespace drogon
{
namespace orm
{
class PostgreSQLResultImpl : public ResultImpl
{
  public:
    PostgreSQLResultImpl(const std::shared_ptr<PGresult> &r,
                         const std::string &query) noexcept
        : ResultImpl(query), _result(r)
    {
    }
    virtual size_type size() const noexcept override;
    virtual row_size_type columns() const noexcept override;
    virtual const char *columnName(row_size_type number) const override;
    virtual size_type affectedRows() const noexcept override;
    virtual row_size_type columnNumber(const char colName[]) const override;
    virtual const char *getValue(size_type row,
                                 row_size_type column) const override;
    virtual bool isNull(size_type row, row_size_type column) const override;
    virtual field_size_type getLength(size_type row,
                                      row_size_type column) const override;
    virtual int oid(row_size_type column) const override;

  private:
    std::shared_ptr<PGresult> _result;
};

}  // namespace orm
}  // namespace drogon
