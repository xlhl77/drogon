/**
 *
 *  HttpControllersRouter.cc
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

#include "HttpControllersRouter.h"
#include "AOPAdvice.h"
#include "HttpRequestImpl.h"
#include "HttpResponseImpl.h"
#include "StaticFileRouter.h"
#include "HttpAppFrameworkImpl.h"
#include "FiltersFunction.h"
#include <algorithm>

using namespace drogon;

void HttpControllersRouter::doWhenNoHandlerFound(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->path() == "/" &&
        !HttpAppFrameworkImpl::instance().getHomePage().empty())
    {
        req->setPath("/" + HttpAppFrameworkImpl::instance().getHomePage());
        HttpAppFrameworkImpl::instance().forward(req, std::move(callback));
        return;
    }
    _fileRouter.route(req, std::move(callback));
}

void HttpControllersRouter::init(
    const std::vector<trantor::EventLoop *> &ioLoops)
{
    std::string regString;
    for (auto &router : _ctrlVector)
    {
        std::regex reg("\\(\\[\\^/\\]\\*\\)");
        std::string tmp =
            std::regex_replace(router._pathParameterPattern, reg, "[^/]*");
        router._regex = std::regex(router._pathParameterPattern,
                                   std::regex_constants::icase);
        regString.append("(").append(tmp).append(")|");
        for (auto &binder : router._binders)
        {
            if (binder)
            {
                binder->_filters =
                    filters_function::createFilters(binder->_filterNames);
            }
        }
    }
    if (regString.length() > 0)
        regString.resize(regString.length() - 1);  // remove the last '|'
    LOG_TRACE << "regex string:" << regString;
    _ctrlRegex = std::regex(regString, std::regex_constants::icase);
}

std::vector<std::tuple<std::string, HttpMethod, std::string>>
HttpControllersRouter::getHandlersInfo() const
{
    std::vector<std::tuple<std::string, HttpMethod, std::string>> ret;
    for (auto &item : _ctrlVector)
    {
        for (size_t i = 0; i < Invalid; i++)
        {
            if (item._binders[i])
            {
                auto description =
                    item._binders[i]->_handlerName.empty()
                        ? std::string("Handler: ") +
                              item._binders[i]->_binderPtr->handlerName()
                        : std::string("HttpController: ") +
                              item._binders[i]->_handlerName;
                auto info = std::tuple<std::string, HttpMethod, std::string>(
                    item._pathPattern, (HttpMethod)i, std::move(description));
                ret.emplace_back(std::move(info));
            }
        }
    }
    return ret;
}
void HttpControllersRouter::addHttpPath(
    const std::string &path,
    const internal::HttpBinderBasePtr &binder,
    const std::vector<HttpMethod> &validMethods,
    const std::vector<std::string> &filters,
    const std::string &handlerName)
{
    // Path is like /api/v1/service/method/{1}/{2}/xxx...
    std::vector<size_t> places;
    std::string tmpPath = path;
    std::string paras = "";
    std::regex regex = std::regex("\\{([^/]*)\\}");
    std::smatch results;
    auto pos = tmpPath.find('?');
    if (pos != std::string::npos)
    {
        paras = tmpPath.substr(pos + 1);
        tmpPath = tmpPath.substr(0, pos);
    }
    std::string originPath = tmpPath;
    size_t placeIndex = 1;
    while (std::regex_search(tmpPath, results, regex))
    {
        if (results.size() > 1)
        {
            auto result = results[1].str();
            if (!result.empty() &&
                std::all_of(result.begin(), result.end(), [](const char c) {
                    return std::isdigit(c);
                }))
            {
                size_t place = (size_t)std::stoi(result);
                if (place > binder->paramCount() || place == 0)
                {
                    LOG_ERROR << "Parameter placeholder(value=" << place
                              << ") out of range (1 to " << binder->paramCount()
                              << ")";
                    LOG_ERROR << "Path pattern: " << path;
                    exit(1);
                }
                if (!std::all_of(places.begin(),
                                 places.end(),
                                 [place](size_t i) { return i != place; }))
                {
                    LOG_ERROR << "Parameter placeholders are duplicated: index="
                              << place;
                    LOG_ERROR << "Path pattern: " << path;
                    exit(1);
                }
                places.push_back(place);
            }
            else
            {
                std::regex regNumberAndName("([0-9]+):.*");
                std::smatch regexResult;
                if (std::regex_match(result, regexResult, regNumberAndName))
                {
                    assert(regexResult.size() == 2 && regexResult[1].matched);
                    auto num = regexResult[1].str();
                    size_t place = (size_t)std::stoi(num);
                    if (place > binder->paramCount() || place == 0)
                    {
                        LOG_ERROR << "Parameter placeholder(value=" << place
                                  << ") out of range (1 to "
                                  << binder->paramCount() << ")";
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [place](size_t i) { return i != place; }))
                    {
                        LOG_ERROR
                            << "Parameter placeholders are duplicated: index="
                            << place;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    places.push_back(place);
                }
                else
                {
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [placeIndex](size_t i) {
                                         return i != placeIndex;
                                     }))
                    {
                        LOG_ERROR
                            << "Parameter placeholders are duplicated: index="
                            << placeIndex;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    places.push_back(placeIndex);
                }
            }
            ++placeIndex;
        }
        tmpPath = results.suffix();
    }
    std::map<std::string, size_t> parametersPlaces;
    if (!paras.empty())
    {
        std::regex pregex("([^&]*)=\\{([^&]*)\\}&*");
        while (std::regex_search(paras, results, pregex))
        {
            if (results.size() > 2)
            {
                auto result = results[2].str();
                if (!result.empty() &&
                    std::all_of(result.begin(), result.end(), [](const char c) {
                        return std::isdigit(c);
                    }))
                {
                    size_t place = (size_t)std::stoi(result);
                    if (place > binder->paramCount() || place == 0)
                    {
                        LOG_ERROR << "Parameter placeholder(value=" << place
                                  << ") out of range (1 to "
                                  << binder->paramCount() << ")";
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    if (!std::all_of(places.begin(),
                                     places.end(),
                                     [place](size_t i) {
                                         return i != place;
                                     }) ||
                        !all_of(parametersPlaces.begin(),
                                parametersPlaces.end(),
                                [place](const std::pair<std::string, size_t>
                                            &item) {
                                    return item.second != place;
                                }))
                    {
                        LOG_ERROR << "Parameter placeholders are "
                                     "duplicated: index="
                                  << place;
                        LOG_ERROR << "Path pattern: " << path;
                        exit(1);
                    }
                    parametersPlaces[results[1].str()] = place;
                }
                else
                {
                    std::regex regNumberAndName("([0-9]+):.*");
                    std::smatch regexResult;
                    if (std::regex_match(result, regexResult, regNumberAndName))
                    {
                        assert(regexResult.size() == 2 &&
                               regexResult[1].matched);
                        auto num = regexResult[1].str();
                        size_t place = (size_t)std::stoi(num);
                        if (place > binder->paramCount() || place == 0)
                        {
                            LOG_ERROR << "Parameter placeholder(value=" << place
                                      << ") out of range (1 to "
                                      << binder->paramCount() << ")";
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        if (!std::all_of(places.begin(),
                                         places.end(),
                                         [place](size_t i) {
                                             return i != place;
                                         }) ||
                            !all_of(parametersPlaces.begin(),
                                    parametersPlaces.end(),
                                    [place](const std::pair<std::string, size_t>
                                                &item) {
                                        return item.second != place;
                                    }))
                        {
                            LOG_ERROR << "Parameter placeholders are "
                                         "duplicated: index="
                                      << place;
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        parametersPlaces[results[1].str()] = place;
                    }
                    else
                    {
                        if (!std::all_of(places.begin(),
                                         places.end(),
                                         [placeIndex](size_t i) {
                                             return i != placeIndex;
                                         }) ||
                            !all_of(parametersPlaces.begin(),
                                    parametersPlaces.end(),
                                    [placeIndex](
                                        const std::pair<std::string, size_t>
                                            &item) {
                                        return item.second != placeIndex;
                                    }))
                        {
                            LOG_ERROR << "Parameter placeholders are "
                                         "duplicated: index="
                                      << placeIndex;
                            LOG_ERROR << "Path pattern: " << path;
                            exit(1);
                        }
                        parametersPlaces[results[1].str()] = placeIndex;
                    }
                }
                ++placeIndex;
            }
            paras = results.suffix();
        }
    }
    auto pathParameterPattern =
        std::regex_replace(originPath, regex, "([^/]*)");
    auto binderInfo = std::make_shared<CtrlBinder>();
    binderInfo->_filterNames = filters;
    binderInfo->_handlerName = handlerName;
    binderInfo->_binderPtr = binder;
    binderInfo->_parameterPlaces = std::move(places);
    binderInfo->_queryParametersPlaces = std::move(parametersPlaces);
    drogon::app().getLoop()->queueInLoop([binderInfo]() {
        // Recreate this with the correct number of threads.
        binderInfo->_responseCache = IOThreadStorage<HttpResponsePtr>();
    });
    {
        std::lock_guard<std::mutex> guard(_ctrlMutex);
        for (auto &router : _ctrlVector)
        {
            if (router._pathParameterPattern == pathParameterPattern)
            {
                if (validMethods.size() > 0)
                {
                    for (auto const &method : validMethods)
                    {
                        router._binders[method] = binderInfo;
                        if (method == Options)
                            binderInfo->_isCORS = true;
                    }
                }
                else
                {
                    binderInfo->_isCORS = true;
                    for (int i = 0; i < Invalid; i++)
                        router._binders[i] = binderInfo;
                }
                return;
            }
        }
    }
    struct HttpControllerRouterItem router;
    router._pathParameterPattern = pathParameterPattern;
    router._pathPattern = path;
    if (validMethods.size() > 0)
    {
        for (auto const &method : validMethods)
        {
            router._binders[method] = binderInfo;
            if (method == Options)
                binderInfo->_isCORS = true;
        }
    }
    else
    {
        binderInfo->_isCORS = true;
        for (int i = 0; i < Invalid; i++)
            router._binders[i] = binderInfo;
    }
    {
        std::lock_guard<std::mutex> guard(_ctrlMutex);
        _ctrlVector.push_back(std::move(router));
    }
}

void HttpControllersRouter::route(
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    // Find http controller
    if (_ctrlRegex.mark_count() > 0)
    {
        std::smatch result;
        if (std::regex_match(req->path(), result, _ctrlRegex))
        {
            for (size_t i = 1; i < result.size(); i++)
            {
                // TODO: Is there any better way to find the sub-match index
                // without using loop?
                if (!result[i].matched)
                    continue;
                if (i <= _ctrlVector.size())
                {
                    size_t ctlIndex = i - 1;
                    auto &routerItem = _ctrlVector[ctlIndex];
                    assert(Invalid > req->method());
                    req->setMatchedPathPattern(routerItem._pathPattern);
                    auto &binder = routerItem._binders[req->method()];
                    if (!binder)
                    {
                        // Invalid Http Method
                        auto res = drogon::HttpResponse::newHttpResponse();
                        if (req->method() != Options)
                        {
                            res->setStatusCode(k405MethodNotAllowed);
                        }
                        else
                        {
                            res->setStatusCode(k403Forbidden);
                        }
                        callback(res);
                        return;
                    }
                    if (!_postRoutingObservers.empty())
                    {
                        for (auto &observer : _postRoutingObservers)
                        {
                            observer(req);
                        }
                    }
                    if (_postRoutingAdvices.empty())
                    {
                        if (!binder->_filters.empty())
                        {
                            auto &filters = binder->_filters;
                            auto callbackPtr = std::make_shared<
                                std::function<void(const HttpResponsePtr &)>>(
                                std::move(callback));
                            filters_function::doFilters(
                                filters,
                                req,
                                callbackPtr,
                                [=, &binder, &routerItem]() {
                                    doPreHandlingAdvices(binder,
                                                         routerItem,
                                                         req,
                                                         std::move(
                                                             *callbackPtr));
                                });
                        }
                        else
                        {
                            doPreHandlingAdvices(binder,
                                                 routerItem,
                                                 req,
                                                 std::move(callback));
                        }
                    }
                    else
                    {
                        auto callbackPtr = std::make_shared<
                            std::function<void(const HttpResponsePtr &)>>(
                            std::move(callback));
                        doAdvicesChain(_postRoutingAdvices,
                                       0,
                                       req,
                                       callbackPtr,
                                       [&binder,
                                        callbackPtr,
                                        req,
                                        this,
                                        &routerItem]() mutable {
                                           if (!binder->_filters.empty())
                                           {
                                               auto &filters = binder->_filters;
                                               filters_function::doFilters(
                                                   filters,
                                                   req,
                                                   callbackPtr,
                                                   [=, &binder, &routerItem]() {
                                                       doPreHandlingAdvices(
                                                           binder,
                                                           routerItem,
                                                           req,
                                                           std::move(
                                                               *callbackPtr));
                                                   });
                                           }
                                           else
                                           {
                                               doPreHandlingAdvices(
                                                   binder,
                                                   routerItem,
                                                   req,
                                                   std::move(*callbackPtr));
                                           }
                                       });
                    }
                }
            }
        }
        else
        {
            // No handler found
            doWhenNoHandlerFound(req, std::move(callback));
        }
    }
    else
    {
        // No handler found
        doWhenNoHandlerFound(req, std::move(callback));
    }
}

void HttpControllersRouter::doControllerHandler(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpControllerRouterItem &routerItem,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    auto &responsePtr = *(ctrlBinderPtr->_responseCache);
    if (responsePtr)
    {
        if (responsePtr->expiredTime() == 0 ||
            (trantor::Date::now() <
             responsePtr->creationDate().after(responsePtr->expiredTime())))
        {
            // use cached response!
            LOG_TRACE << "Use cached response";
            invokeCallback(callback, req, responsePtr);
            return;
        }
        else
        {
            responsePtr.reset();
        }
    }

    std::vector<std::string> params(ctrlBinderPtr->_parameterPlaces.size());
    std::smatch r;
    if (std::regex_match(req->path(), r, routerItem._regex))
    {
        for (size_t j = 1; j < r.size(); j++)
        {
            size_t place = ctrlBinderPtr->_parameterPlaces[j - 1];
            if (place > params.size())
                params.resize(place);
            params[place - 1] = r[j].str();
            LOG_TRACE << "place=" << place << " para:" << params[place - 1];
        }
    }
    if (ctrlBinderPtr->_queryParametersPlaces.size() > 0)
    {
        auto qureyPara = req->getParameters();
        for (auto const &parameter : qureyPara)
        {
            if (ctrlBinderPtr->_queryParametersPlaces.find(parameter.first) !=
                ctrlBinderPtr->_queryParametersPlaces.end())
            {
                auto place =
                    ctrlBinderPtr->_queryParametersPlaces.find(parameter.first)
                        ->second;
                if (place > params.size())
                    params.resize(place);
                params[place - 1] = parameter.second;
            }
        }
    }
    std::list<std::string> paraList;
    for (auto &p : params)  /// Use reference
    {
        LOG_TRACE << p;
        paraList.push_back(std::move(p));
    }
    ctrlBinderPtr->_binderPtr->handleHttpRequest(
        paraList,
        req,
        [=, callback = std::move(callback)](const HttpResponsePtr &resp) {
            if (resp->expiredTime() >= 0 && resp->statusCode() != k404NotFound)
            {
                // cache the response;
                static_cast<HttpResponseImpl *>(resp.get())->makeHeaderString();
                auto loop = req->getLoop();
                if (loop->isInLoopThread())
                {
                    ctrlBinderPtr->_responseCache.setThreadData(resp);
                }
                else
                {
                    req->getLoop()->queueInLoop([resp, &ctrlBinderPtr]() {
                        ctrlBinderPtr->_responseCache.setThreadData(resp);
                    });
                }
            }
            invokeCallback(callback, req, resp);
        });
    return;
}

void HttpControllersRouter::doPreHandlingAdvices(
    const CtrlBinderPtr &ctrlBinderPtr,
    const HttpControllerRouterItem &routerItem,
    const HttpRequestImplPtr &req,
    std::function<void(const HttpResponsePtr &)> &&callback)
{
    if (req->method() == Options)
    {
        auto resp = HttpResponse::newHttpResponse();
        resp->setContentTypeCode(ContentType::CT_TEXT_PLAIN);
        std::string methods = "OPTIONS,";
        if (routerItem._binders[Get] && routerItem._binders[Get]->_isCORS)
        {
            methods.append("GET,HEAD,");
        }
        if (routerItem._binders[Post] && routerItem._binders[Post]->_isCORS)
        {
            methods.append("POST,");
        }
        if (routerItem._binders[Put] && routerItem._binders[Put]->_isCORS)
        {
            methods.append("PUT,");
        }
        if (routerItem._binders[Delete] && routerItem._binders[Delete]->_isCORS)
        {
            methods.append("DELETE,");
        }
        methods.resize(methods.length() - 1);
        resp->addHeader("ALLOW", methods);
        auto &origin = req->getHeader("Origin");
        if (origin.empty())
        {
            resp->addHeader("Access-Control-Allow-Origin", "*");
        }
        else
        {
            resp->addHeader("Access-Control-Allow-Origin", origin);
        }
        resp->addHeader("Access-Control-Allow-Methods", methods);
        resp->addHeader("Access-Control-Allow-Headers",
                        "x-requested-with,content-type");
        callback(resp);
        return;
    }
    if (!_preHandlingObservers.empty())
    {
        for (auto &observer : _preHandlingObservers)
        {
            observer(req);
        }
    }
    if (_preHandlingAdvices.empty())
    {
        doControllerHandler(ctrlBinderPtr,
                            routerItem,
                            req,
                            std::move(callback));
    }
    else
    {
        auto callbackPtr =
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                std::move(callback));
        doAdvicesChain(
            _preHandlingAdvices,
            0,
            req,
            std::make_shared<std::function<void(const HttpResponsePtr &)>>(
                [req, callbackPtr](const HttpResponsePtr &resp) {
                    HttpAppFrameworkImpl::instance().callCallback(req,
                                                                  resp,
                                                                  *callbackPtr);
                }),
            [this, ctrlBinderPtr, &routerItem, req, callbackPtr]() {
                doControllerHandler(ctrlBinderPtr,
                                    routerItem,
                                    req,
                                    std::move(*callbackPtr));
            });
    }
}

void HttpControllersRouter::invokeCallback(
    const std::function<void(const HttpResponsePtr &)> &callback,
    const HttpRequestImplPtr &req,
    const HttpResponsePtr &resp)
{
    for (auto &advice : _postHandlingAdvices)
    {
        advice(req, resp);
    }
    HttpAppFrameworkImpl::instance().callCallback(req, resp, callback);
}
