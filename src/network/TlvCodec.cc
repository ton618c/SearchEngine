#include "network/TlvCodec.h"

#include <endian.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/TcpServer.h>

using namespace muduo::net;
using namespace muduo;

void TlvCodec::send(const TcpConnectionPtr& conn, uint8_t type, const std::string& value) {
    Buffer buf;
    uint32_t length = htobe32(value.size());

    buf.append(&type, sizeof(type));  // 1 字节 type
    buf.append(&length, sizeof(length));  // 4 字节长度（大端）
    buf.append(value.c_str(), value.size());  // payload

    conn->send(&buf);
}

void TlvCodec::onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp ts) {
    // 至少需要 5 字节：1(type) + 4(length)
    while (buf->readableBytes() >= 5) {
        const void* data = buf->peek();

        uint8_t type;
        uint32_t length;

        memcpy(&type, static_cast<const char*>(data), 1);
        memcpy(&length, static_cast<const char*>(data) + 1, 4);
        length = be32toh(length);

        // 缓冲区里完整消息还没到齐，等下一次
        if (buf->readableBytes() - 5 < length) break;

        buf->retrieve(5);  // 跳过 type + length
        std::string value(buf->peek(), length);  // 读出 payload
        buf->retrieve(length);

        _messageCallback(conn, type, value, ts);
    }
}
