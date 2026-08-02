#include "recommender/KeywordRecommender.h"

#include <utfcpp/utf8.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <queue>
#include <sstream>
// 工具函数

// 判断该字符是不是中文
static bool isChineseChar(char32_t cp) { return cp >= 0x4E00 && cp <= 0x9FFF; }

// 判断是否全为中文
static bool isAllChinese(const std::string &s) {
    if (s.empty()) return false;
    utf8::iterator it(s.begin(), s.begin(), s.end());
    const utf8::iterator end(s.end(), s.begin(), s.end());
    for (; it != end; ++it) {
        if (!isChineseChar(*it)) return false;
    }
    return true;
}

// 把字符串拆成 UTF-8 字符列表
// 因为编辑距离应该是比较字符与字符之间的距离，这里写一个函数
// 把字符串拆成一个个字符,而不是比较字节
std::vector<std::string> splitChars(const std::string &s) {
    std::vector<std::string> chars;
    const char *curr = s.c_str();
    const char *end = s.c_str() + s.size();
    while (curr != end) {
        auto start = curr;
        utf8::next(curr, end);
        chars.emplace_back(start, curr);
    }
    return chars;
}

// 状态压缩版编辑距离
// 返回的是两个单词的最小距离
// 字符级编辑距离
static int editDistance(const std::string &a, const std::string &b) {
    auto ca = splitChars(a);
    auto cb = splitChars(b);

    const auto &row = ca.size() >= cb.size() ? ca : cb;
    const auto &col = ca.size() >= cb.size() ? cb : ca;
    const int m = static_cast<int>(col.size());
    std::vector<int> f(m + 1, 0);
    for (int j = 0; j < m; ++j) {
        f[j + 1] = j + 1;
    }

    for (const auto &x : row) {
        int pre = f[0];
        f[0]++;
        for (int j = 0; j < m; ++j) {
            int tmp = f[j + 1];
            if (col[j] == x) {
                f[j + 1] = pre;
            } else {
                f[j + 1] = std::min({f[j], f[j + 1], pre}) + 1;
            }
            pre = tmp;
        }
    }
    return f[m];
}

// 前缀树的实现
void KeywordRecommender::Trie::insert(const std::string &word, int freq) {
    Node *cur = root;
    for (const auto &ch : splitChars(word)) {
        if (!cur->children[ch]) {
            cur->children[ch] = new Node{};
        }
        cur = cur->children[ch];
    }
    cur->isEnd = true;
    cur->freq = freq;
}

// 递归收集子树中的所有词，按freq装进小根堆
// 堆顶freq最低，超k就pop
// pair内部重载了小于运算符
// 留下的是前缀且词频更大的，若都是前缀且词频也一样，留下字典序更小的
// 这样可以与第二层保持一致
void KeywordRecommender::Trie::collect(const Trie::Node *node, const std::string &word,
    std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, Cmp>
        &prior,
    int k) {
    if (node->isEnd) {
        prior.push({node->freq, word});

        // 大于要求的数量就弹出堆顶
        if (prior.size() > k) {
            prior.pop();
        }
    }
    for (const auto &[ch, child] : node->children) {
        collect(child, word + ch, prior, k);
    }
}

std::vector<std::pair<std::string, int>> KeywordRecommender::Trie::search(
    const std::string &prefix, int k) const {
    // 为了避免输入花生酱，只看完整前缀而错过花生的情况
    // 原实现是只找完整前缀词，
    // 现在的实现是从最后走通的节点收集子树词
    const Node *curChar = root;
    const Node *lastChar = root;
    std::string matched;  // 走通的前缀
    for (const auto &ch : splitChars(prefix)) {
        auto it = curChar->children.find(ch);
        if (it == curChar->children.end()) {
            break;  // 第一个就找不到，不往下走了
        }
        curChar = it->second;
        lastChar = curChar;
        matched += ch;
    }
    // 一个前缀都找不到，直接进入第二层
    if (lastChar == root) {
        return {};
    }

    // 从最后走通的节点开始收集子树词
    std::priority_queue<std::pair<int, std::string>, std::vector<std::pair<int, std::string>>, Cmp>
        prior;
    collect(lastChar, matched, prior, k);

    // 堆弹出是低频到高频，反装成为高频到低频
    std::vector<std::pair<std::string, int>> result;
    while (!prior.empty()) {
        result.emplace_back(prior.top().second, prior.top().first);
        prior.pop();
    }
    std::reverse(result.begin(), result.end());
    return result;
}

void KeywordRecommender::Trie::destroy(Node *node) {
    if (!node) {
        return;
    }
    for (auto &[_, child] : node->children) {
        destroy(child);
    }
    delete node;
}

KeywordRecommender::Trie::~Trie() { destroy(root); }

// 读取词典库和索引库
void KeywordRecommender::loadDict(
    const std::string &path, std::vector<std::pair<std::string, int>> &dict) const {
    std::ifstream ifs{path};
    if (!ifs) {
        std::cerr << "open file failed : " << path << std::endl;
        return;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss{line};
        std::string word;
        int freq = 0;
        iss >> word >> freq;
        dict.emplace_back(word, freq);
    }
}

void KeywordRecommender::loadIndex(
    const std::string &path, std::map<std::string, std::set<int>> &index) const {
    std::ifstream ifs{path};
    if (!ifs) {
        std::cerr << "open file failed : " << path << std::endl;
        return;
    }
    std::string line;
    while (std::getline(ifs, line)) {
        std::istringstream iss{line};
        std::string word;
        iss >> word;
        int i = 0;
        while (iss >> i) {
            index[word].insert(i);
        }
    }
}

// 构造函数，在构造函数里面加载中文和英文的
// 词典库和索引库
// 这里传进来的dictDir是可以从环境变量中拿的
// SE_DATA_DIR1=./data/generated/dictionary
KeywordRecommender::KeywordRecommender(const std::string &dictDir) {
    loadDict(dictDir + "/dict_cn.dat", _cnDict);
    loadIndex(dictDir + "/index_cn.dat", _cnIndex);
    loadDict(dictDir + "/dict_en.dat", _enDict);
    loadIndex(dictDir + "/index_en.dat", _enIndex);

    // 构建前缀树
    for (const auto &[word, freq] : _cnDict) {
        _cnTrie.insert(word, freq);
    }
    for (const auto &[word, freq] : _enDict) {
        _enTrie.insert(word, freq);
    }
}

// 关键字推荐
std::string KeywordRecommender::recommend(const std::string &keyword, int k) const {
    if (keyword.empty() || k <= 0) {
        std::cerr << "keyword is empty or k <= 0" << std::endl;
        return nlohmann::json::array().dump();
    }

    // 判断keyword是不是全中文，如果是全中文，就查询中文库
    // 如果不是全中文，全英文或者中英混合，就走英文查询
    // 中英混合的情况暂时不考虑，关键先实现全中和全英的推荐
    const bool allChinese = isAllChinese(keyword);
    const auto &dict = allChinese ? _cnDict : _enDict;
    const auto &index = allChinese ? _cnIndex : _enIndex;
    const auto &trie = allChinese ? _cnTrie : _enTrie;

    std::set<std::string> removing;  // 用于第一层和第二层的去重
    std::priority_queue<Candidate> prior;
    // 第一层逻辑：Trie前缀匹配
    auto prefixResults = trie.search(keyword, k);
    for (const auto &[word, freq] : prefixResults) {
        removing.insert(word);
        // 前缀匹配的词也需要比较编辑距离
        // 比如花生和花生米都能和前缀花生匹配
        // 我们需要一个手段来区别谁的优先级大
        prior.push({word, freq, editDistance(keyword, word), true});
    }

    // 第二层：编辑距离，当前缀不足k个的时候会走这个逻辑
    if (prefixResults.size() < k) {
        // 将关键字拆分为一个一个的字符
        std::vector<std::string> characters;
        const char *curr = keyword.c_str();
        const char *end = keyword.c_str() + keyword.size();
        while (curr != end) {
            auto start = curr;
            utf8::next(curr, end);
            characters.emplace_back(start, curr);
        }

        // 查索引
        std::set<int> lines;
        for (const auto &ch : characters) {
            auto it = index.find(ch);
            if (it != index.end()) {
                lines.insert(it->second.begin(), it->second.end());
            }
        }

        for (int line : lines) {
            int idx = line - 1;
            const auto &[word, freq] = dict[idx];
            // 跳过已由Trie返回的
            if (removing.count(word)) {
                continue;
            }
            removing.insert(word);
            // 结构体里面已经重载了小于号运算符
            // 也就是说现在的堆顶就是最不匹配的字符，把他删掉即可
            prior.push({word, freq, editDistance(keyword, word), false});

            if (prior.size() > k) {
                prior.pop();
            }
        }
    }
    std::vector<std::string> result;
    while (!prior.empty()) {
        result.push_back(prior.top().word);
        prior.pop();
    }
    // 这时候再逆序result , 得到的就是前k个最匹配的候选词
    std::reverse(result.begin(), result.end());
    nlohmann::json arr = result;
    return arr.dump();
}