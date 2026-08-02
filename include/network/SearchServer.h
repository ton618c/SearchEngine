#pragma once
#include <muduo/base/Timestamp.h>
#include <muduo/net/Buffer.h>
#include <muduo/net/Callbacks.h>
#include <muduo/net/InetAddress.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/net/TcpServer.h>

#include "cache/HashCaches.h"
#include "recommender/KeywordRecommender.h"
#include "recommender/PageRecommender.h"
#include "TlvCodec.h"

class SearchServer {
public:
    SearchServer(muduo::net::EventLoop* loop, const muduo::net::InetAddress& listenAddr,
        const std::string& dictDir, const std::string& pageDir);

    void start() { _server.start(); }

private:
    void onConnection(const muduo::net::TcpConnectionPtr& conn);
    void onIntactMessage(const muduo::net::TcpConnectionPtr& conn, uint8_t type,
        const std::string& value, muduo::Timestamp ts);

    muduo::net::TcpServer _server;
    TlvCodec _codec;
    KeywordRecommender _keywordRecommender;
    PageRecommender _pageRecommender;
    HashCaches<std::string, std::string> _keywordCache;  // key=查询词,   value=推荐结果JSON
    HashCaches<std::string, std::string> _pageCache;  // key=查询语句, value=搜索结果JSON
};
