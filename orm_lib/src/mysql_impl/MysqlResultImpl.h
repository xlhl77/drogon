/**
 *
 *  MysqlResultImpl.h
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
#include <trantor/utils/Logger.h>
#include <mysql.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <algorithm>
#include <vector>
#include <cstring>
#include "json.hpp"
using JSON = nolohmann::json;
namespace drogon
{
namespace orm
{

class MysqlResultImpl : public ResultImpl
{
  public:
    explicit MysqlResultImpl(const std::shared_ptr<MYSQL_RES> &r,
                    const std::string &query,
                    size_type affectedRows,
                    unsigned long long insertId) noexcept
        : ResultImpl(query),
          _result(r),
          _rowsNum(0),
          _fieldArray(r ? mysql_fetch_fields(r.get()) : nullptr),
          _fieldNum(r ? mysql_num_fields(r.get()) : 0),
          _affectedRows(affectedRows),
          _insertId(insertId)
    {
        if (_fieldNum > 0)
        {
            _binds = std::shared_ptr<MYSQL_BIND>(new MYSQL_BIND[_fieldNum], [](MYSQL_BIND* p){
              delete[] p;
            }); 
            unsigned long len = 0;
            _fieldMapPtr = std::make_shared<std::unordered_map<std::string, row_size_type>>();
            for (row_size_type i = 0; i < _fieldNum; i++)
            {
                std::string fieldName = _fieldArray[i].name;
                std::transform(fieldName.begin(), fieldName.end(), fieldName.begin(), tolower);
                (*_fieldMapPtr)[fieldName] = i;
                // _offset.push_back({_fieldArray[i].length, len});
                // len += _fieldArray[i].length;
		            std::memset(&(_binds.get()[i]), 0 , sizeof(MYSQL_BIND));
                _binds.get()[i].buffer_type = _fieldArray[i].type;
	            	_binds.get()[i].buffer_length = _fieldArray[i].length;
            }

        }
    }
    virtual size_type size() const noexcept override;
    virtual row_size_type columns() const noexcept override;
    virtual const char *columnName(row_size_type number) const override;
    virtual size_type affectedRows() const noexcept override;
    virtual row_size_type columnNumber(const char colName[]) const override;
    virtual const char *getValue(size_type row, row_size_type column) const override;
    virtual bool isNull(size_type row, row_size_type column) const override;
    virtual field_size_type getLength(size_type row, row_size_type column) const override;
    virtual unsigned long long insertId() const noexcept override;

    MYSQL_BIND* addRow();
  private:
    const std::shared_ptr<MYSQL_RES> _result;
    Result::size_type _rowsNum;
    const MYSQL_FIELD *_fieldArray;
    const Result::row_size_type _fieldNum;
    const size_type _affectedRows;
    const unsigned long long _insertId;
    std::shared_ptr<std::unordered_map<std::string, row_size_type>> _fieldMapPtr;

    JSON _rowData = JSON::array();
    std::shared_ptr<MYSQL_BIND> _binds;
};

} // namespace orm
} // namespace drogon
