/**
 *
 *  WebSocketConnection.h
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

#include <drogon/config.h>
#include <memory>
#include <string>
#include <trantor/net/InetAddress.h>
#include <trantor/utils/NonCopyable.h>
namespace drogon
{
enum class WebSocketMessageType
{
    Text,
    Binary,
    Ping,
    Pong,
    Close,
    Unknown
};

class WebSocketConnection
{
  public:
    WebSocketConnection() = default;
    virtual ~WebSocketConnection(){};

    /// Send a message to the peer
    virtual void send(
        const char *msg,
        uint64_t len,
        const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;
    virtual void send(
        const std::string &msg,
        const WebSocketMessageType &type = WebSocketMessageType::Text) = 0;

    /// Return the local IP address and port number of the connection
    virtual const trantor::InetAddress &localAddr() const = 0;
    /// Return the remote IP address and port number of the connection
    virtual const trantor::InetAddress &peerAddr() const = 0;

    /// Return true if the connection is open
    virtual bool connected() const = 0;
    /// Return true if the connection is closed
    virtual bool disconnected() const = 0;

    /// Shut down the write direction, which means that further send operations
    /// are disabled.
    virtual void shutdown() = 0;
    /// Close the connection
    virtual void forceClose() = 0;

    /// Set custom data on the connection
    virtual void setContext(const any &context) = 0;

    /// Get custom data from the connection
    virtual const any &getContext() const = 0;
    virtual any *getMutableContext() = 0;

    /// Set the heartbeat(ping) message sent to the peer.
    /**
     * NOTE:
     * Both the server and the client in Drogon automatically send the pong
     * message after receiving the ping message.
     */
    virtual void setPingMessage(
        const std::string &message,
        const std::chrono::duration<long double> &interval) = 0;
};
typedef std::shared_ptr<WebSocketConnection> WebSocketConnectionPtr;
}  // namespace drogon
