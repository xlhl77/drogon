/**
 *
 *  WebSocketConnectionImpl.h
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

#include "impl_forwards.h"
#include <drogon/WebSocketConnection.h>
#include <trantor/utils/NonCopyable.h>
#include <trantor/net/TcpConnection.h>

namespace drogon
{
class WebSocketConnectionImpl;
typedef std::shared_ptr<WebSocketConnectionImpl> WebSocketConnectionImplPtr;

class WebSocketMessageParser
{
  public:
    bool parse(trantor::MsgBuffer *buffer);
    bool gotAll(std::string &message, WebSocketMessageType &type)
    {
        assert(message.empty());
        if (!_gotAll)
            return false;
        message.swap(_message);
        type = _type;
        return true;
    }

  private:
    std::string _message;
    WebSocketMessageType _type;
    bool _gotAll = false;
};

class WebSocketConnectionImpl
    : public WebSocketConnection,
      public std::enable_shared_from_this<WebSocketConnectionImpl>,
      public trantor::NonCopyable
{
  public:
    explicit WebSocketConnectionImpl(const trantor::TcpConnectionPtr &conn,
                                     bool isServer = true);

    virtual void send(
        const char *msg,
        uint64_t len,
        const WebSocketMessageType &type = WebSocketMessageType::Text) override;
    virtual void send(
        const std::string &msg,
        const WebSocketMessageType &type = WebSocketMessageType::Text) override;

    virtual const trantor::InetAddress &localAddr() const override;
    virtual const trantor::InetAddress &peerAddr() const override;

    virtual bool connected() const override;
    virtual bool disconnected() const override;

    virtual void shutdown() override;    // close write
    virtual void forceClose() override;  // close

    virtual void setPingMessage(
        const std::string &message,
        const std::chrono::duration<long double> &interval) override;

    void setMessageCallback(
        const std::function<void(std::string &&,
                                 const WebSocketConnectionImplPtr &,
                                 const WebSocketMessageType &)> &callback)
    {
        _messageCallback = callback;
    }

    void setCloseCallback(
        const std::function<void(const WebSocketConnectionImplPtr &)> &callback)
    {
        _closeCallback = callback;
    }

    void onNewMessage(const trantor::TcpConnectionPtr &connPtr,
                      trantor::MsgBuffer *buffer);

    void onClose()
    {
        if (_pingTimerId != trantor::InvalidTimerId)
            _tcpConn->getLoop()->invalidateTimer(_pingTimerId);
        _closeCallback(shared_from_this());
    }

  private:
    trantor::TcpConnectionPtr _tcpConn;
    trantor::InetAddress _localAddr;
    trantor::InetAddress _peerAddr;
    bool _isServer = true;
    WebSocketMessageParser _parser;
    trantor::TimerId _pingTimerId = trantor::InvalidTimerId;

    std::function<void(std::string &&,
                       const WebSocketConnectionImplPtr &,
                       const WebSocketMessageType &)>
        _messageCallback = [](std::string &&,
                              const WebSocketConnectionImplPtr &,
                              const WebSocketMessageType &) {};
    std::function<void(const WebSocketConnectionImplPtr &)> _closeCallback =
        [](const WebSocketConnectionImplPtr &) {};
    void sendWsData(const char *msg, uint64_t len, unsigned char opcode);
};

}  // namespace drogon
