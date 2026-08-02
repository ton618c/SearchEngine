#pragma once
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Callbacks.h>

#include <functional>
class TlvCodec {
public:
    using MyMessageCallback = std::function<void(const muduo::net::TcpConnectionPtr&, uint8_t type,
        const std::string& value, muduo::Timestamp)>;

    explicit TlvCodec(const MyMessageCallback& cb) : _messageCallback(cb) {}

    // 编码：type(1字节) + length(4字节大端) + value
    void send(const muduo::net::TcpConnectionPtr& conn, uint8_t type, const std::string& value);

    // 解码：从 Buffer 里读出完整的 type + length + value，回调
    void onMessage(
        const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp ts);

private:
    MyMessageCallback _messageCallback;
};
