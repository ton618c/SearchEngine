#pragma once
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpClient.h>
#include <muduo/net/TcpConnection.h>

#include "TlvCodec.h"

class SearchClient {
public:
    SearchClient(muduo::net::EventLoop* loop, const muduo::net::InetAddress& serverAddr);

    void connect();
    void disconnect();
    void send(uint8_t type, const std::string& message);

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onIntactMessage(const muduo::net::TcpConnectionPtr& conn, uint8_t type,
        const std::string& message, muduo::Timestamp ts);

    muduo::net::TcpClient _client;
    muduo::net::TcpConnectionPtr _conn;
    muduo::net::EventLoop* _loop;
    TlvCodec _codec;
};
