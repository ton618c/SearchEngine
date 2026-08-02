#pragma once
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

// LruCache：单个LRU分片，内部有自己独立的锁
// 一个双向链表_list存key-value，链表头 = 最近使用
// 一个hash map(_map) 存key->链表节点的迭代器，O（1）查找

template <typename Key, typename Value>
class LruCache {
public:
    explicit LruCache(int capacity) : _capacity(capacity) {}
    // 查询：命中则返回 true + value , 并且把该节点移到链表头部
    bool get(const Key &key, Value &value) {
        // 创建时内部会调用_mutex.lock() , 自动上锁
        // 利用RAII思想，获取即初始化，离开作用域会自动解锁
        std::unique_lock<std::mutex> lock(_mutex);

        auto it = _map.find(key);
        if (it == _map.end()) {
            return false;
        }
        // 命中后，将该自由移到双向链表的头部(即最近使用过)
        _list.splice(_list.begin(), _list, it->second);
        value = it->second->second;
        return true;
    }

    // 写入：key已存在则更新值并移动到头部
    // 不存在则插入头部
    // 超过容量时淘汰链表尾部
    void put(const Key &key, const Value &value) {
        std::unique_lock<std::mutex> lock(_mutex);
        auto it = _map.find(key);
        if (it != _map.end()) {
            // 更新值加移到头部
            _list.splice(_list.begin(), _list, it->second);
            it->second->second = value;
            return;
        }
        // 不存在就先检查容量
        if (_list.size() > _capacity) {
            auto last = _list.back();
            _map.erase(last.first);
            _list.pop_back();
        }
        // 插入到链表头部
        _list.emplace_front(key, value);
        _map[key] = _list.begin();
    }

private:
    int _capacity;  // 每个分片的容量
    std::list<std::pair<Key, Value>> _list;  // 存数据的双向链表
    std::unordered_map<Key, decltype(_list.begin())> _map;  // key映射链表位置
    std::mutex _mutex;  // 本分片专用锁
};

// Hashes:分片缓存的外壳
// 对key做std::hash 路由到某个LruCache分片
// 分片数等于slices , 不同分片有独立的锁，减少线程竞争
template <typename Key, typename Value>
class HashCaches {
public:
    HashCaches(int capacity, int slices = 5) : _capacity(capacity), _slices(slices) {
        int perSlices = capacity / slices;  // 每一个分片的容量等于总容量 / 分片的总数量
        for (int i = 0; i < slices; ++i) {
            _caches.push_back(std::make_unique<LruCache<Key, Value>>(perSlices));
        }
    }

    // 写入: hash(key) % slices 路由到对应的分片
    void put(const Key &key, const Value &value) {
        size_t idx = _hashFunc(key) % _slices;
        _caches[idx]->put(key, value);
    }

    // 查询: hash(key) % slices 路由到对应分片
    bool get(const Key &key, Value &value) {
        size_t idx = _hashFunc(key) % _slices;
        return _caches[idx]->get(key, value);
    }

private:
    int _capacity;  // 不同分片加起来的总容量
    int _slices;  // 分片数量 参考哈希表 设置为 线程数目 / 0.75
                  // muduo网络库会把线程数目设为4
    std::vector<std::unique_ptr<LruCache<Key, Value>>> _caches;  // 分片数组
    std::hash<Key> _hashFunc;  // 路由函数
};