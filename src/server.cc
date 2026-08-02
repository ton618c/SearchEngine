#include <muduo/base/Logging.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>

#include <iostream>

#include "network/SearchServer.h"

using namespace std;
using namespace muduo::net;
using namespace muduo;

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "Usage: " << argv[0] << " <port>" << endl;
        return 1;
    }

    Logger::setLogLevel(Logger::INFO);

    EventLoop loop;
    uint16_t port = static_cast<uint16_t>(atoi(argv[1]));
    InetAddress serverAddr(port);

    SearchServer server(&loop, serverAddr, "data/", "data/");
    server.start();

    loop.loop();
    return 0;
}
