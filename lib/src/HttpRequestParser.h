/**
 *
 *  HttpRequestParser.h
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

#include "HttpRequestImpl.h"
#include "WebSocketConnectionImpl.h"
#include <deque>
#include <drogon/HttpResponse.h>
#include <mutex>
#include <trantor/net/TcpConnection.h>
#include <trantor/utils/MsgBuffer.h>

using namespace trantor;
namespace drogon
{
class HttpRequestParser
{
  public:
    enum HttpRequestParseState
    {
        HttpRequestParseState_ExpectMethod,
        HttpRequestParseState_ExpectRequestLine,
        HttpRequestParseState_ExpectHeaders,
        HttpRequestParseState_ExpectBody,
        HttpRequestParseState_GotAll,
    };

    explicit HttpRequestParser(const trantor::TcpConnectionPtr &connPtr);

    // return false if any error
    bool parseRequest(MsgBuffer *buf);

    bool gotAll() const
    {
        return _state == HttpRequestParseState_GotAll;
    }

    void reset()
    {
        _state = HttpRequestParseState_ExpectMethod;
        _request.reset(new HttpRequestImpl(_loop));
    }

    const HttpRequestImplPtr &requestImpl() const
    {
        return _request;
    }

    bool firstReq()
    {
        if (_firstRequest)
        {
            _firstRequest = false;
            return true;
        }
        return false;
    }
    const WebSocketConnectionImplPtr &webSocketConn() const
    {
        return _websockConnPtr;
    }
    void setWebsockConnection(const WebSocketConnectionImplPtr &conn)
    {
        _websockConnPtr = conn;
    }
    // to support request pipelining(rfc2616-8.1.2.2)
    void pushRquestToPipelining(const HttpRequestPtr &req);
    HttpRequestPtr getFirstRequest() const;
    HttpResponsePtr getFirstResponse() const;
    void popFirstRequest();
    void pushResponseToPipelining(const HttpRequestPtr &req,
                                  const HttpResponsePtr &resp);
    size_t numberOfRequestsInPipelining() const
    {
        return _requestPipelining.size();
    }
    bool isStop() const
    {
        return _stopWorking;
    }
    void stop()
    {
        _stopWorking = true;
    }
    size_t numberOfRequestsParsed() const
    {
        return _requestsCounter;
    }

  private:
    void shutdownConnection(HttpStatusCode code);
    bool processRequestLine(const char *begin, const char *end);
    HttpRequestParseState _state;
    trantor::EventLoop *_loop;
    HttpRequestImplPtr _request;
    bool _firstRequest = true;
    WebSocketConnectionImplPtr _websockConnPtr;
    std::deque<std::pair<HttpRequestPtr, HttpResponsePtr>> _requestPipelining;
    size_t _requestsCounter = 0;
    std::weak_ptr<trantor::TcpConnection> _conn;
    bool _stopWorking = false;
};

}  // namespace drogon
