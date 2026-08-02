# MySearchEngine

C++ 搜索引擎，基于 muduo 网络库、cppjieba 分词、SimHash 去重、TF-IDF 排序。

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
│   └── client.cc      # 客户端入口
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

### 3. 启动客户端

```bash
./client 127.0.0.1 7777
```

交互格式：

```
1 花生              # 关键字推荐（type=1）
2 冬奥中国队          # 网页搜索（type=2）
/quit               # 退出
```

## 三期功能

| 阶段 | 功能 |
|---|---|
| 第一期 离线 | 关键字推荐词典+索引、网页搜索网页库+偏移库+倒排索引 |
| 第二期 在线 | 基于 muduo 的服务端/客户端框架、TLV 编解码、关键字推荐、网页搜索 |
| 第三期 缓存 | 分片 LRU 缓存，减少重复计算 |
