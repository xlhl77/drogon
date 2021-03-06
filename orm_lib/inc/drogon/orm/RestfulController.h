/**
 *
 *  RestfulController.h
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

#include <drogon/drogon.h>
#include <drogon/orm/DbClient.h>
#include <drogon/orm/Mapper.h>
#include <trantor/utils/NonCopyable.h>
#include <string>
#include <functional>
#include <vector>

namespace drogon
{
/**
 * @brief this class is a helper class for the implementation of restful api
 * controllers generated by the drogon_ctl command.
 */
class RestfulController : trantor::NonCopyable
{
  public:
    void enableMasquerading(const std::vector<std::string> &pMasqueradingVector)
    {
        _masquerading = true;
        _masqueradingVector = pMasqueradingVector;
        for (size_t i = 0; i < _masqueradingVector.size(); ++i)
        {
            _masqueradingMap.insert(
                std::pair<std::string, size_t>{_masqueradingVector[i], i});
        }
    }
    void disableMasquerading()
    {
        _masquerading = false;
        _masqueradingVector = _columnsVector;
        for (size_t i = 0; i < _masqueradingVector.size(); ++i)
        {
            _masqueradingMap.insert(
                std::pair<std::string, size_t>{_masqueradingVector[i], i});
        }
    }
    void registerAJsonValidator(
        const std::string &fieldName,
        const std::function<bool(const Json::Value &, std::string &)>
            &validator)
    {
        _validators.emplace_back(fieldName, validator);
    }
    void registerAJsonValidator(
        const std::string &fieldName,
        std::function<bool(const Json::Value &, std::string &)> &&validator)
    {
        _validators.emplace_back(fieldName, std::move(validator));
    }

    /**
     * @brief make a criteria object for searching by ORM.
     *
     * @param pJson the json object presenting search criterias.
     * The json object must be an array of depth 3.
     * for example:
     * [
     *    [
     *       ["color","=","red"], //AND
     *       ["price","<",1000]
     *    ], //OR
     *    [
     *        ["color","=","white"], //AND
     *        ["price","<",800],  //AND
     *        ["brand","!=",null]
     *    ]
     * ]
     * @return orm::Criteria
     */
    orm::Criteria makeCriteria(const Json::Value &pJson) noexcept(false);

  protected:
    RestfulController(const std::vector<std::string> &columnsVector)
        : _columnsVector(columnsVector)
    {
    }
    std::vector<std::string> fieldsSelector(const std::set<std::string> &fields)
    {
        std::vector<std::string> ret;
        for (auto &field : _masqueradingVector)
        {
            if (!field.empty() && fields.find(field) != fields.end())
            {
                ret.emplace_back(field);
            }
            else
            {
                ret.emplace_back(std::string{});
            }
        }
        return ret;
    }
    template <typename T>
    Json::Value makeJson(const HttpRequestPtr &req, const T &obj)
    {
        auto &queryParams = req->parameters();
        auto iter = queryParams.find("fields");
        if (_masquerading)
        {
            if (iter != queryParams.end())
            {
                auto fields = utils::splitStringToSet(iter->second, ",");
                return obj.toMasqueradedJson(fieldsSelector(fields));
            }
            else
            {
                return obj.toMasqueradedJson(_masqueradingVector);
            }
        }
        else
        {
            if (iter != queryParams.end())
            {
                auto fields = utils::splitString(iter->second, ",");
                return obj.toMasqueradedJson(fields);
            }
            else
            {
                return obj.toJson();
            }
        }
    }
    bool doCustomValidations(const Json::Value &pJson, std::string &err)
    {
        for (auto &validator : _validators)
        {
            if (pJson.isMember(validator.first))
            {
                if (!validator.second(pJson[validator.first], err))
                {
                    return false;
                }
            }
        }
        return true;
    }
    bool isMasquerading() const
    {
        return _masquerading;
    }
    const std::vector<std::string> &masqueradingVector() const
    {
        return _masqueradingVector;
    }

  private:
    bool _masquerading = true;
    std::vector<std::string> _masqueradingVector;
    std::vector<
        std::pair<std::string,
                  std::function<bool(const Json::Value &, std::string &)>>>
        _validators;
    std::unordered_map<std::string, size_t> _masqueradingMap;
    const std::vector<std::string> _columnsVector;
};
}  // namespace drogon