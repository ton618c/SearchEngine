// ============================================================================
// http_gateway.cc — wfrest HTTP 网关
//
// 角色：浏览器 ↔ muduo 搜索引擎之间的协议转换器
//   浏览器  ←→  HTTP/JSON   ←→  本网关  ←→  TLV/TCP  ←→  muduo server(:7777)
//
// 运行方式:
//   ./http_gateway 8080 127.0.0.1 7777
//   参数1: HTTP 监听端口（浏览器访问这个端口）
//   参数2: muduo 服务端 IP
//   参数3: muduo 服务端端口
// ============================================================================

#include <arpa/inet.h>
#include <endian.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <string>

#include <wfrest/CodeUtil.h>
#include <wfrest/HttpServer.h>

using namespace std;
using namespace wfrest;

// ── 连接 muduo server，发送 TLV 请求，接收 TLV 响应，返回 value 部分 ──
//
// TLV 协议格式（与 TlvCodec.cc 保持一致）:
//   ┌──────┬────────────┬─────────────────┐
//   │ type │   length   │     value       │
//   │ 1字节 │ 4字节(大端) │ length字节      │
//   └──────┴────────────┴─────────────────┘
//
// 请求:  type=1 → 关键字推荐    type=2 → 网页搜索
// 响应:  type 不变, value = JSON 字符串
//
string sendTlvAndRecv(const string& muduoIp, uint16_t muduoPort,
                      uint8_t type, const string& value) {
    // ── 1. 创建 TCP socket ──
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        return R"({"status":"error","message":"创建socket失败"})";
    }

    // ── 2. 设置接收超时 3 秒（防止 muduo server 无响应时永久阻塞）──
    struct timeval tv;
    tv.tv_sec = 3;
    tv.tv_usec = 0;
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // ── 3. 连接 muduo server ──
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htobe16(muduoPort);   // 主机序 → 网络序
    inet_pton(AF_INET, muduoIp.c_str(), &addr.sin_addr);

    if (connect(fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        close(fd);
        return R"({"status":"error","message":"无法连接搜索引擎服务"})";
    }

    // ── 4. 编码并发送 TLV 请求 ──
    //      格式: [type 1B] [length 4B 大端] [payload]
    uint32_t netLen = htobe32(value.size());   // 主机序 → 网络序(大端)

    ::send(fd, &type,   sizeof(type),   0);    // Tag:   1 字节
    ::send(fd, &netLen, sizeof(netLen), 0);    // Length: 4 字节大端
    ::send(fd, value.data(), value.size(), 0); // Value: payload

    // ── 5. 接收 TLV 响应 ──
    uint8_t  recvType;
    uint32_t recvLen;

    ssize_t n = recv(fd, &recvType, 1, MSG_WAITALL);
    if (n <= 0) { close(fd); return R"({"status":"error","message":"搜索引擎无响应"})"; }

    n = recv(fd, &recvLen, 4, MSG_WAITALL);
    if (n <= 0) { close(fd); return R"({"status":"error","message":"搜索引擎响应异常"})"; }

    recvLen = be32toh(recvLen);   // 网络序 → 主机序

    // ── 6. 读取 payload ──
    string result(recvLen, '\0');
    n = recv(fd, result.data(), recvLen, MSG_WAITALL);
    if (n <= 0) { close(fd); return R"({"status":"error","message":"搜索引擎响应异常"})"; }

    // ── 7. 关闭连接，返回结果 ──
    close(fd);
    return result;   // 就是 muduo server 返回的 JSON 字符串
}


// ====================================================================
// 主函数: 启动 wfrest HTTP 网关
// ====================================================================
int main(int argc, char* argv[]) {
    if (argc < 4) {
        cerr << "Usage: " << argv[0]
             << " <http_port> <muduo_ip> <muduo_port>" << endl;
        cerr << "Example: " << argv[0] << " 8080 127.0.0.1 7777" << endl;
        return 1;
    }

    int      httpPort  = atoi(argv[1]);
    string   muduoIp   = argv[2];
    uint16_t muduoPort = atoi(argv[3]);

    HttpServer server;

    // ── 路由1: 静态页面 ──
    //  浏览器访问 http://ip:8080/ → 返回 static/index.html
    //  wfrest 的 Static 会自动处理 MIME 类型
    server.Static("/", "static/index.html");
    server.Static("/static", "static/");

    // ── 路由2: 关键字推荐 API ──
    //  浏览器: GET /api/suggest?q=花
    //  网关:   打包成 TLV type=1 发给 muduo，拿回 JSON 返回给浏览器
    server.GET("/api/suggest", [muduoIp, muduoPort]
               (const HttpReq* req, HttpResp* resp) {
        // 注意: wfrest 的 req->query() 返回的是未解码的原始串
        // 浏览器 encodeURIComponent 会把中文编码成 %E8%8A%B1 之类
        // 必须先用 url_decode 还原，否则 muduo server 会把 "%E8%8A%B1"
        // 当成英文字符串处理（isAllChinese 判断失败 → 走英文词典）
        string q = CodeUtil::url_decode(req->query("q"));
        if (q.empty()) {
            resp->set_status(400);
            resp->String(R"({"status":"error","message":"缺少参数 q"})");
            return;
        }

        // 通过 TLV/TCP 发 type=1（关键字推荐）到 muduo server
        string jsonResult = sendTlvAndRecv(muduoIp, muduoPort, 1, q);

        resp->add_header_pair("Content-Type", "application/json; charset=utf-8");
        resp->add_header_pair("Access-Control-Allow-Origin", "*");
        resp->String(jsonResult);
    });

    // ── 路由3: 网页搜索 API ──
    //  浏览器: GET /api/search?q=冬奥中国队
    //  网关:   打包成 TLV type=2 发给 muduo，拿回 JSON 返回给浏览器
    server.GET("/api/search", [muduoIp, muduoPort]
               (const HttpReq* req, HttpResp* resp) {
        // 同上：query 参数需先做 URL 解码，否则中文会被当成英文处理
        string q = CodeUtil::url_decode(req->query("q"));
        if (q.empty()) {
            resp->set_status(400);
            resp->String(R"({"status":"error","message":"缺少参数 q"})");
            return;
        }

        // 通过 TLV/TCP 发 type=2（网页搜索）到 muduo server
        string jsonResult = sendTlvAndRecv(muduoIp, muduoPort, 2, q);

        resp->add_header_pair("Content-Type", "application/json; charset=utf-8");
        resp->add_header_pair("Access-Control-Allow-Origin", "*");
        resp->String(jsonResult);
    });

    // ── 启动 HTTP 服务 ──
    if (server.start(httpPort) == 0) {
        cout << "HTTP 网关已启动: http://0.0.0.0:" << httpPort << endl;
        cout << "  → 关键字推荐: GET /api/suggest?q=xxx  (type=1 → muduo)" << endl;
        cout << "  → 网页搜索:   GET /api/search?q=xxx   (type=2 → muduo)" << endl;
        cout << "  → 后端引擎:   " << muduoIp << ":" << muduoPort << " (TLV/TCP)" << endl;
        getchar();   // 按回车停止
        server.stop();
    } else {
        cerr << "HTTP 网关启动失败!" << endl;
        return 1;
    }

    return 0;
}
