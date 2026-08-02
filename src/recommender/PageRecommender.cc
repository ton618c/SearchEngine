#include "recommender/PageRecommender.h"

#include <tinyxml2.h>
#include <utfcpp/utf8.h>

#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <unordered_map>
using namespace std;
using namespace tinyxml2;
using namespace nlohmann;

static string getAbstract(const string &content, int maxChars) {
    const char *curr = content.c_str();
    const char *end = content.c_str() + content.size();
    int count = 0;
    while (curr != end && count < maxChars) {
        utf8::next(curr, end);
        count++;
    }
    return string(content.c_str(), curr - content.c_str());
}

PageRecommender::PageRecommender(const string &pageDir) {
    loadStopWords("stopwords/cn_stopwords.txt", _cnStopWords);
    loadDocuments(pageDir + "web_library.dat", _documents);
    loadInvertedIndex(pageDir + "inverted_index.dat", _invertedIndex);
}

void PageRecommender::loadStopWords(const string &filePath, unordered_set<string> &stopWords) {
    ifstream ifs{filePath};
    if (!ifs) {
        cerr << "open file failed : " << filePath << endl;
        return;
    }
    string line;
    while (ifs >> line) {
        stopWords.insert(line);
    }
    ifs.close();
}

void PageRecommender::loadDocuments(const string &filePath, vector<Document> &documents) {
    XMLDocument XMLdoc;
    if (XMLdoc.LoadFile(filePath.c_str()) != XML_SUCCESS) {
        cerr << "loadfile failed : " << filePath << endl;
        return;
    }
    auto getText = [](XMLElement* e) -> string {
        return (e && e->GetText()) ? e->GetText() : "";
    };

    for (XMLElement *doc = XMLdoc.FirstChildElement("doc"); doc != nullptr;
         doc = doc->NextSiblingElement("doc")) {
        int id = stoi(getText(doc->FirstChildElement("id")));
        string link    = getText(doc->FirstChildElement("link"));
        string title   = getText(doc->FirstChildElement("title"));
        string content = getText(doc->FirstChildElement("content"));
        documents.emplace_back(id, link, title, content);
    }
}

void PageRecommender::loadInvertedIndex(
    const string &filePath, map<string, vector<pair<int, double>>> &invertedIndex) {
    ifstream ifs{filePath};
    if (!ifs) {
        cerr << "open file failed : " << filePath << endl;
        return;
    }
    string line;
    while (getline(ifs, line)) {
        istringstream iss{line};
        string word;
        iss >> word;
        int id;
        double weight;
        while (iss >> id >> weight) {
            invertedIndex[word].emplace_back(id, weight);
        }
    }
    ifs.close();
}

string PageRecommender::recommend(const string &docment, int n) {
    vector<string> words;
    _tokenizer.Cut(docment, words);
    unordered_map<string, int> filtered;
    int sum = 0;
    // 将用户输入的句子看作文本，分词并过滤
    // 用filtered来存储过滤后的关键词，可以存储出现次数
    // 以便后面计算tf
    for (const auto &x : words) {
        if (_cnStopWords.find(x) == _cnStopWords.end()) {
            filtered[x]++;
            ++sum;
        }
    }

    // 使用 TF-IDF 算法计算出每个关键字的权重，将其组成一个向量 X = (x1, x2, . . . , xn)，
    // weights里面的double组合起来就是向量X
    vector<pair<string, double>> weights;
    double docSum = 0;
    for (const auto &term : filtered) {
        double tf = 1.0 * term.second / sum;
        int df = _invertedIndex[term.first].size();
        double idf = log2(_documents.size() / (df + 1));
        double w = tf * idf;
        weights.emplace_back(term.first, w);
        docSum += w * w;
    }
    for (auto &weight : weights) {
        double x = weight.second;
        weight.second = x / sqrt(docSum);
    }

    // 存储包含所有关键字的网页
    // 统计每个文档命中了多少个查询关键词
    unordered_map<int, int> docCount;
    for (const auto &[term, _] : weights) {
        auto it = _invertedIndex.find(term);
        if (it == _invertedIndex.end()) {
            continue;
        }
        for (const auto &[docId, w] : it->second) {
            docCount[docId]++;
        }
    }
    // 只保留包含所有关键词的文档 ，计算余弦相似度
    int wordCount = weights.size();
    // docId -> 文档向量中对于关键词的权重
    unordered_map<int, unordered_map<string, double>> docTermWeights;
    for (const auto &[term, _] : weights) {
        auto it = _invertedIndex.find(term);
        if (it == _invertedIndex.end()) continue;
        for (const auto &[docId, w] : it->second) {
            docTermWeights[docId][term] = w;
        }
    }

    vector<pair<int, double>> candidates;
    for (const auto &[docId, cnt] : docCount) {
        // 如果该文档没有包含所有的关键词，就跳过该文档
        if (cnt != wordCount) {
            continue;
        }
        double cosine = 0;
        // 因为已经归一化 ， 所以余弦值实际就是 x*y
        for (const auto &[term, x] : weights) {
            double y = docTermWeights[docId][term];
            cosine += x * y;
        }
        candidates.emplace_back(docId, cosine);
    }
    if (candidates.size() == 0) {
        return json::array().dump();
    }

    sort(candidates.begin(), candidates.end(),
        [](pair<int, double> &a, pair<int, double> &b) { return a.second > b.second; });
    json js = json::array();

    for (const auto &[docId, cosine] : candidates) {
        int idx = docId - 1;
        json jj;
        jj["id"] = _documents[idx]._id;
        jj["title"] = _documents[idx]._title;
        jj["link"] = _documents[idx]._link;
        string abstract = getAbstract(_documents[idx]._content, n);
        jj["abstract"] = abstract;
        js.push_back(jj);
    }
    return js.dump();
}
