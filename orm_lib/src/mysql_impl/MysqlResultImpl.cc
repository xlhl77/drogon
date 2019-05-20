/**
 *
 *  MysqlResultImpl.cc
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

#include "MysqlResultImpl.h"
#include <algorithm>
#include <assert.h>

using namespace drogon::orm;

Result::size_type MysqlResultImpl::size() const noexcept
{
    return _rowsNum - 1;
}
Result::row_size_type MysqlResultImpl::columns() const noexcept
{
    return _fieldNum;
}
const char *MysqlResultImpl::columnName(row_size_type number) const
{
    assert(number < _fieldNum);
    if (_fieldArray)
        return _fieldArray[number].name;
    return "";
}
Result::size_type MysqlResultImpl::affectedRows() const noexcept
{
    return _affectedRows;
}
Result::row_size_type MysqlResultImpl::columnNumber(const char colName[]) const
{
    if (!_fieldMapPtr)
        return -1;
    std::string col(colName);
    std::transform(col.begin(), col.end(), col.begin(), tolower);
    if (_fieldMapPtr->find(col) != _fieldMapPtr->end())
        return (*_fieldMapPtr)[col];
    return -1;
}
const char *MysqlResultImpl::getValue(size_type row, row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return NULL;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    const char *name = columnName(column);
    return (_rowData[row][name].type()==JSON::value_t::string)
    ? _rowData[row][name].get<std::string>().c_str()
    : _rowData[row][name].dump().c_str();
}
bool MysqlResultImpl::isNull(size_type row, row_size_type column) const
{
    return getValue(row, column) == NULL;
}
Result::field_size_type MysqlResultImpl::getLength(size_type row,
                                                   row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return 0;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    const char *name = columnName(column);
    return (_rowData[row][name].type()==JSON::value_t::string)
    ? _rowData[row][name].get<std::string>().size()
    : _rowData[row][name].dump().size();
}
unsigned long long MysqlResultImpl::insertId() const noexcept
{
    return _insertId;
}

void MysqlResultImpl::processRow(int nRow)
{
    auto bind = _binds.get();
    JSON &last = _rowData[_rowsNum - 1];
    for (auto &[name,idx] :*_fieldMapPtr.get())
    {
        // 是否为null？
        if (_isNULL[idx])
        {
            last[name] = json();
            continue;
        }

        // 是否日期？
        switch (bind[idx].buffer_type)
        {
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP2:
		case MYSQL_TYPE_DATETIME2:
		case MYSQL_TYPE_TIME2:
        {
            std::string *s = last[name].get_ptr<JSON::string_t*>();
            MYSQL_TIME *tm = (MYSQL_TIME *)s->data();
            std::sprintf(s->data(), "%d-%d-%d %d:%d:%d.%ld", tm->year, tm->month, tm->day, tm->hour, tm->minute, tm->second, tm->second_part);
            s->resize(std::strlen(s->data()));
            break;
        }
        default:
        // 清理没有使用的空间
        if (last[name].type() == JSON::value_t::string)
        {
            std::string *s = last[name].get_ptr<JSON::string_t*>();
            s->resize(_len[idx]);
        }
        }

    }
}

MYSQL_BIND *MysqlResultImpl::addRow()
{
    auto bind = _binds.get();

    _rowData.push_back(JSON::object());
    JSON &row = _rowData[_rowsNum];
    if (_rowsNum > 0) 
    {
        processRow(_rowsNum - 1);
    }

    std::memset(bind, 0, _fieldNum);
    for (auto &[name,i] :*_fieldMapPtr.get())
    {
        switch (_fieldArray[i].type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_LONGLONG:
        {
			row[name] = 0;
            bind[i].buffer = (char *)row[name].get_ptr<JSON::number_integer_t*>();
	        bind[i].buffer_type = MYSQL_TYPE_LONG;
	        bind[i].buffer_length = sizeof(json::number_integer_t);
			break;
        }
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
        {
			row[name] = 0.0;
            bind[i].buffer = (char *)row[name].get_ptr<JSON::number_float_t*>();
	        bind[i].buffer_type = MYSQL_TYPE_DOUBLE;
	        bind[i].buffer_length = sizeof(double);
			break;
        }
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP2:
		case MYSQL_TYPE_DATETIME2:
		case MYSQL_TYPE_TIME2:
        {
			row[name] = std::string(sizeof(MYSQL_TIME),'\0');
            bind[i].buffer = (char *) (row[name].get_ptr<JSON::string_t*>())->c_str();
            bind[i].buffer_type = MYSQL_TYPE_DATETIME;
            break;            
        }
		case MYSQL_TYPE_NULL:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_DECIMAL:
        case MYSQL_TYPE_NEWDECIMAL:
		case MYSQL_TYPE_ENUM:
		case MYSQL_TYPE_SET:
		case MYSQL_TYPE_TINY_BLOB:
		case MYSQL_TYPE_MEDIUM_BLOB:
		case MYSQL_TYPE_LONG_BLOB:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
        {
			row[name] = std::string(_fieldArray[i].length,'\0');
            bind[i].buffer = (char *) (row[name].get_ptr<JSON::string_t*>())->c_str();
            break;
        }
		case MYSQL_TYPE_GEOMETRY:
		    break;
	    }
        bind[i].length = &_len[i];
        bind[i].is_null = &_isNULL[i];

    }
    
    _rowsNum++;
    return bind;
}

bool MysqlResultImpl::toJson(json &result) noexcept
{
    // result = json::array();
    
    for (size_t i = 0; i < _rowsNum - 1; i++)
    {
        result.emplace_back(std::move(_rowData[i]));
    }
    return true;
}
