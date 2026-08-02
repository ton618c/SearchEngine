#include <iostream>

#include "offline/KeywordProcessor.h"
#include "offline/PageProcessor.h"
using namespace std;

int main() {
    KeywordProcessor keyp;
    keyp.process("corpus/CN", "corpus/EN");

    PageProcessor pagep;
    pagep.process("corpus/webpages");
    return 0;
}