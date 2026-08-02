#include "network/SearchServer.h"

#include <muduo/base/Logging.h>
#include <muduo/base/Timestamp.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>

#include <string>

using namespace muduo::net;
using namespace muduo;
using namespace std::placeholders;

SearchServer::SearchServer(EventLoop* loop, const InetAddress& listenAddr,
    const std::string& dictDir, const std::string& pageDir)
    : _server(loop, listenAddr, "SearchServer"),
      _codec(std::bind(&SearchServer::onIntactMessage, this, _1, _2, _3, _4)),
      _keywordRecommender(dictDir),
      _pageRecommender(pageDir),
      _keywordCache(10000, 5),
      _pageCache(10000, 5) {
    _server.setConnectionCallback(std::bind(&SearchServer::onConnection, this, _1));

    _server.setMessageCallback(std::bind(&TlvCodec::onMessage, &_codec, _1, _2, _3));

    _server.setThreadNum(4);
}

void SearchServer::onConnection(const TcpConnectionPtr& conn) {
    if (conn->connected()) {
        LOG_INFO << conn->peerAddress().toIpPort() << " connected";
    } else {
        LOG_INFO << conn->peerAddress().toIpPort() << " disconnected";
    }
}

void SearchServer::onIntactMessage(
    const TcpConnectionPtr& conn, uint8_t type, const std::string& value, Timestamp ts) {
    std::string result;

    switch (type) {
        case 1:  // 关键字推荐
            if (_keywordCache.get(value, result)) {
                _codec.send(conn, type, result);
                return;
            }
            result = _keywordRecommender.recommend(value);
            _keywordCache.put(value, result);
            break;

        case 2:  // 网页搜索
            if (_pageCache.get(value, result)) {
                _codec.send(conn, type, result);
                return;
            }
            result = _pageRecommender.recommend(value);
            _pageCache.put(value, result);
            break;

        default:
            LOG_WARN << "unknown type: " << static_cast<int>(type);
            return;
    }

    _codec.send(conn, type, result);
}
