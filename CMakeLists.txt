# 设置CMake的最小版本
cmake_minimum_required(VERSION 3.15)

project(MySearchEngine)

# 指定C++的版本: -std=c++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置可执行程序生成的路径
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR})

include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    /usr/local/include
)

# ── 离线构建：生成词典库和索引库 ──
add_executable(offline
    src/main.cc
    src/offline/DirectoryScanner.cc
    src/offline/KeywordProcessor.cc
    src/offline/PageProcessor.cc
)
target_compile_options(offline PRIVATE -g)
target_link_libraries(offline PRIVATE tinyxml2)

# ── 服务端 ──
add_executable(server
    src/server.cc
    src/network/SearchServer.cc
    src/network/TlvCodec.cc
    src/offline/DirectoryScanner.cc
    src/offline/KeywordProcessor.cc
    src/offline/PageProcessor.cc
    src/recommender/KeywordRecommender.cpp
    src/recommender/PageRecommender.cc
)
target_compile_options(server PRIVATE -g)
target_link_libraries(server PRIVATE tinyxml2 muduo_net muduo_base pthread)

# ── 客户端 ──
add_executable(client
    src/client.cc
    src/network/SearchClient.cc
    src/network/TlvCodec.cc
)
target_compile_options(client PRIVATE -g)
target_link_libraries(client PRIVATE muduo_net muduo_base pthread)

# ── HTTP 网关（浏览器 ↔ muduo 的桥梁）──
# 浏览器发 HTTP → 本网关编码成 TLV → 发给 muduo server
# muduo server 返回 TLV → 本网关解码 → HTTP JSON 返回浏览器
add_executable(http_gateway
    src/http_gateway.cc
)
target_compile_options(http_gateway PRIVATE -g)
target_link_libraries(http_gateway PRIVATE wfrest pthread)
