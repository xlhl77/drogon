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
#include <assert.h>
#include <algorithm>

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
    return _rowData[row][column].value<string>();
}
bool MysqlResultImpl::isNull(size_type row, row_size_type column) const
{
    return getValue(row, column) == NULL;
}
Result::field_size_type MysqlResultImpl::getLength(size_type row, row_size_type column) const
{
    if (_rowsNum == 0 || _fieldNum == 0)
        return 0;
    assert(row < _rowsNum);
    assert(column < _fieldNum);
    return _rowData[row][column].size();
}
unsigned long long MysqlResultImpl::insertId() const noexcept
{
    return _insertId;
}

MYSQL_BIND *MysqlResultImpl::addRow()
{
    _rowData.push_back(JSON::array());
    JSON &row = _rowData[_rowsNum];

    for (row_size_type i = 0; i < _fieldNum; i++)
    {
        switch (type) {
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
			row[i] = 0;
            _binds.get()[i].buffer = (char *)row[i].get_ptr<JSON::number_integer_t*>();
			break;
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
			row[i] = 0.0;
            _binds.get()[i].buffer = (char *)row[i].get_ptr<JSON::number_float_t*>();
			break;
		case MYSQL_TYPE_NULL:
		case MYSQL_TYPE_TIMESTAMP:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_YEAR:
		case MYSQL_TYPE_NEWDATE:
		case MYSQL_TYPE_VARCHAR:
		case MYSQL_TYPE_BIT:
		case MYSQL_TYPE_TIMESTAMP2:
		case MYSQL_TYPE_DATETIME2:
		case MYSQL_TYPE_TIME2:
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
			row[i] = string(_fieldArray[i].length,'\0');
            _binds.get()[i].buffer = (char *) (row[i].get_ptr<JSON::string_t*>())->c_str();
			return;
		case MYSQL_TYPE_GEOMETRY:
		    break;
	    }
    }

    _rowsNum++;
    return _binds.get();
}