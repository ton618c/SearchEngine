#include "network/SearchClient.h"

#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>

#include <functional>
#include <iostream>

#include "network/TlvCodec.h"

using namespace muduo;
using namespace muduo::net;
using namespace std::placeholders;

SearchClient::SearchClient(EventLoop* loop, const InetAddress& serverAddr)
    : _client(loop, serverAddr, "SearchClient"),
      _loop(loop),
      _codec(std::bind(&SearchClient::onIntactMessage, this, _1, _2, _3, _4)) {
    _client.setConnectionCallback(std::bind(&SearchClient::onConnection, this, _1));

    _client.setMessageCallback(std::bind(&TlvCodec::onMessage, &_codec, _1, _2, _3));
}

void SearchClient::connect() { _client.connect(); }

void SearchClient::disconnect() { _client.disconnect(); }

void SearchClient::send(uint8_t type, const std::string& message) {
    if (_conn) _codec.send(_conn, type, message);
}

void SearchClient::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        _conn = conn;
        std::cout << "connected to server" << std::endl;
    } else {
        _conn.reset();
        std::cout << "disconnected from server" << std::endl;
        _loop->quit();
    }
}

void SearchClient::onIntactMessage(
    const TcpConnectionPtr& conn, uint8_t type, const std::string& message, Timestamp ts) {
    std::cout << message << std::endl;
}
