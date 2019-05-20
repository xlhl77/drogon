/**
 *
 *  Row.h
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

// Taken from libpqxx and modified.
// The license for libpqxx can be found in the COPYING file.

#pragma once

#include <drogon/orm/Result.h>
#include <string>
namespace drogon
{
namespace orm
{
class Field;
class ConstRowIterator;
class ConstReverseRowIterator;

/// Reference to one row in a result.
/**
 * A row represents one row (also called a row) in a query result set.
 * It also acts as a container mapping column numbers or names to field
 * values (see below):
 *
 * @code
 *	cout << row["date"].as<std::string>() << ": " <<
 *row["name"].as<std::string>() << endl;
 * @endcode
 *
 * The row itself acts like a (non-modifyable) container, complete with its
 * own const_iterator and const_reverse_iterator.
 */
class Row
{
  public:
    using size_type = unsigned long;
    using reference = Field;
    using ConstIterator = ConstRowIterator;
    using Iterator = ConstIterator;
    using ConstReverseIterator = ConstReverseRowIterator;

    using difference_type = long;

    reference operator[](size_type index) const;
    reference operator[](const char columnName[]) const;
    reference operator[](const std::string &columnName) const;
    size_type size() const;
    size_type capacity() const noexcept
    {
        return size();
    }
    ConstIterator begin() const noexcept;
    ConstIterator cbegin() const noexcept;
    ConstIterator end() const noexcept;
    ConstIterator cend() const noexcept;

    ConstReverseIterator rbegin() const;
    ConstReverseIterator crbegin() const;
    ConstReverseIterator rend() const;
    ConstReverseIterator crend() const;

  private:
    const Result _result;

  protected:
    friend class Field;
    /**
     * Row number
     * You'd expect this to be a size_t, but due to the way reverse iterators
     * are related to regular iterators, it must be allowed to underflow to -1.
     */
    long _index = 0;
    size_t _end = 0;
    friend class Result;
    Row(const Result &r, size_type index) noexcept;
};

}  // namespace orm
}  // namespace drogon
