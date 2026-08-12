# SearchEngine

C++17 搜索引擎，基于 muduo 网络库、cppjieba 分词、SimHash 去重、TF-IDF 排序、wfrest HTTP 网关。

## 项目结构

```
MySearchEngine/
├── include/
│   ├── offline/       # 离线构建头文件
│   ├── recommender/   # 在线推荐头文件
│   ├── network/       # 网络层头文件
│   └── cache/         # 缓存头文件
├── src/
│   ├── offline/       # 离线构建源码
│   ├── recommender/   # 在线推荐源码
│   ├── network/       # 网络层源码
│   ├── main.cc        # 离线构建入口
│   ├── server.cc      # 服务端入口
│   ├── client.cc      # 客户端入口
│   └── http_gateway.cc # HTTP 网关入口
├── static/
│   └── index.html     # 浏览器前端页面
├── corpus/            # 原始语料库
├── stopwords/         # 停用词表
├── docs/              # 项目文档
├── CMakeLists.txt
├── build.sh
└── README.md
```

## 依赖

- C++17
- muduo 网络库
- cppjieba 中文分词
- tinyxml2 XML 解析
- simhash 文档去重
- utfcpp UTF-8 字符处理
- nlohmann/json JSON 序列化
- wfrest HTTP 框架

## 编译

```bash
./build.sh
```

## 运行

### 1. 生成离线数据

```bash
./offline
```

生成的数据文件位于 `data/` 目录：

| 文件 | 说明 |
|---|---|
| `dict_cn.dat` / `dict_en.dat` | 中/英文词典 |
| `index_cn.dat` / `index_en.dat` | 中/英文索引（字→词典行号） |
| `web_library.dat` | 网页库 |
| `offset_library.dat` | 网页偏移库 |
| `inverted_index.dat` | 倒排索引库 |

### 2. 启动服务端

```bash
./server 7777
```

### 3a. 启动命令行客户端

```bash
./client 127.0.0.1 7777
```

交互格式：

```
1 花生              # 关键字推荐（type=1）
2 冬奥中国队          # 网页搜索（type=2）
/quit               # 退出
```

### 3b. 启动 HTTP 网关（浏览器访问）

```bash
./http_gateway 8080 127.0.0.1 7777
```

然后浏览器打开 `http://localhost:8080`，在页面上输入关键词即可搜索。

注意：网关用 `getchar()` 等待回车退出，后台运行时需配合管道（如 `sleep 300 | ./http_gateway ... &`）或重定向 stdin。

### TLV 协议

client/server/gateway 之间通过 TLV 帧格式通信：

```
type(1字节) + length(4字节大端) + value
```

| type | 含义 |
|---|---|
| 1 | 关键字推荐 |
| 2 | 网页搜索 |

`http_gateway.cc` 用裸 socket 手工复刻了与 `TlvCodec.cc` 相同的编解码逻辑。改协议格式时必须同步更新两处。

### 架构分层

```
corpus/ ──(offline)──▶ data/*.dat ──▶ server(muduo TCP) ◀─(TLV)── client / http_gateway(wfrest HTTP) ◀── 浏览器(static/index.html)
```

## 四期功能

| 阶段 | 功能 |
|---|---|
| 第一期 | 离线构建：关键字推荐词典+索引、网页搜索网页库+偏移库+倒排索引 |
| 第二期 | 在线服务：基于 muduo 的 TCP 服务端/客户端框架、TLV 编解码、关键字推荐、网页搜索 |
| 第三期 | LRU 缓存：模板分片 LRU 缓存，减少重复计算 |
| 第四期 | HTTP 网关：wfrest HTTP 服务 + 浏览器前端，协议转换（HTTP/JSON ↔ TLV/TCP），支持中文关键字推荐和网页搜索 |
