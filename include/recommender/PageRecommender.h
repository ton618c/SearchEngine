#pragma once
#include <cppjieba/Jieba.hpp>
#include <string>

class PageRecommender {
public:
    PageRecommender(const std::string &pageDir);

    std::string recommend(const std::string &docment, int n = 50);

private:
    struct Document {
        int _id;
        std::string _link;
        std::string _title;
        std::string _content;
        Document(int id, std::string link, std::string title, std::string content)
            : _id(id), _link(move(link)), _title(move(title)), _content(move(content)) {}
    };

private:
    // 加载停用词 ，全部文档 ， 倒排索引库
    void loadStopWords(const std::string &filePath, std::unordered_set<std::string> &stopWords);
    void loadDocuments(const std::string &filePath, std::vector<Document> &documents);
    void loadInvertedIndex(const std::string &filePath,
        std::map<std::string, std::vector<std::pair<int, double>>> &invertedIndex);

private:
    cppjieba::Jieba _tokenizer;
    std::unordered_set<std::string> _cnStopWords;
    std::vector<Document> _documents;
    std::map<std::string, std::vector<std::pair<int, double>>> _invertedIndex;
};