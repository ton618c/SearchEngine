#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <iostream>

#include "network/SearchClient.h"

using namespace std;
using namespace muduo;
using namespace muduo::net;

void* inputThreadFunc(void* args) {
    SearchClient* client = static_cast<SearchClient*>(args);
    string line;
    while (getline(cin, line)) {
        if (line == "/quit") {
            client->disconnect();
            break;
        }
        if (line.empty()) continue;

        // 输入格式: "1 花生" → type=1, value="花生"
        //           "2 冬奥中国队" → type=2, value="冬奥中国队"
        uint8_t type = 0;
        if (line[0] == '1')
            type = 1;
        else if (line[0] == '2')
            type = 2;
        else {
            cout << "usage: 1 <keyword>  or  2 <query>" << endl;
            continue;
        }

        if (line.size() < 3 || line[1] != ' ') {
            cout << "usage: 1 <keyword>  or  2 <query>" << endl;
            continue;
        }

        string value = line.substr(2);
        client->send(type, value);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <ip> <port>" << endl;
        return 1;
    }

    Logger::setLogLevel(Logger::INFO);

    EventLoop loop;
    InetAddress serverAddr(argv[1], static_cast<uint16_t>(atoi(argv[2])));
    SearchClient client(&loop, serverAddr);
    client.connect();

    pthread_t tid;
    pthread_create(&tid, NULL, &inputThreadFunc, (void*)&client);

    loop.loop();

    return 0;
}
