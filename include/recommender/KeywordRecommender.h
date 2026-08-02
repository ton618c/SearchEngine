#pragma once
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>
// 函数声明
// 把字符串转为字符列表
std::vector<std::string> splitChars(const std::string &s);

class KeywordRecommender {
public:
    KeywordRecommender(const std::string &dictDir);
    // 返回的是json格式 ["aa" , "bb" , "cc" , "dd" , "ee"]
    // 没有指定的话，默认返回前5个最匹配的候选词
    // 返回格式就是一个json数组
    // 反序列化后可以直接按下标访问
    std::string recommend(const std::string &keyword, int k = 5) const;

private:
    // 为了优化关键词推荐
    // 这里引入前缀树
    struct Trie {
        // 树里面的结点
        struct Node {
            // children：单个UTF-8字符，子节点指针
            std::map<std::string, Node *> children;
            bool isEnd = false;  // 是否为某个词的结尾
            int freq = 0;  // 词频，仅isEnd = true时有效
        };

        // 创建根结点
        Node *root = new Node{};
        // 比较器：如果都是前缀的话，应该保留词频较大的，字典序较小的
        // 这里写一个比较器
        struct Cmp {
            bool operator()(
                const std::pair<int, std::string> &a, const std::pair<int, std::string> &b) const {
                if (a.first != b.first) return a.first > b.first;
                return a.second < b.second;
            }
        };
        void insert(const std::string &word, int freq);
        // 搜索前缀prefix , 返回子树中词频最高的前k个词
        std::vector<std::pair<std::string, int>> search(const std::string &prefix, int k) const;
        ~Trie();

    private:
        // 递归释放内存
        void destroy(Node *node);
        // 递归收集子树的词，按词频装入小根堆
        static void collect(const KeywordRecommender::Trie::Node *node, const std::string &word,
            std::priority_queue<std::pair<int, std::string>,
                std::vector<std::pair<int, std::string>>, Cmp> &prior,
            int k);
    };

    struct Candidate {
        // 用优先队列实现候选词排序 ，这里创建一个结构体 ， 并重载小于号运算符
        // 比较逻辑：选fromPrefix = true的，编辑距离更小的，候选词词频高的，字典序更小的
        // 现在的关键词推荐有两层
        // 第一层：选前缀树里面匹配的，词频更高的，第一层返回的词语不够，才会转为第二层
        // 第二层：原方法，即编辑距离更小的，候选词词频更高的，字典序更小的
        std::string word;
        int freq;
        int distance;
        bool fromPrefix = false;

        // 此时堆顶放的是最差的匹配词
        bool operator<(const Candidate &rhs) const {
            // 不是前缀的更大，默认大根堆，让他前往堆顶
            if (fromPrefix != rhs.fromPrefix) return fromPrefix;
            if (distance != rhs.distance) return distance < rhs.distance;
            if (freq != rhs.freq) return freq > rhs.freq;
            return word < rhs.word;
        }
    };

    // 读取词典库
    void loadDict(const std::string &path, std::vector<std::pair<std::string, int>> &dict) const;

    // 读取索引库
    void loadIndex(const std::string &path, std::map<std::string, std::set<int>> &index) const;

    // 中文词典库和中文索引库
    std::vector<std::pair<std::string, int>> _cnDict;
    std::map<std::string, std::set<int>> _cnIndex;

    // 英文词典库和英文索引库
    std::vector<std::pair<std::string, int>> _enDict;
    std::map<std::string, std::set<int>> _enIndex;

    // 中文和英文各自一棵前缀树
    Trie _cnTrie;
    Trie _enTrie;
};