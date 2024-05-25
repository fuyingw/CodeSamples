#include <iostream>
#include <sstream>
#include <vector>

#include "TradeStat.hpp"

using namespace std;

#define CHECK(expr) \
    do { \
        if(!(expr)) { \
             std::ostringstream oss; \
             oss << "Failed at line: " << __LINE__ << "`" #expr "` "; \
             return oss.str(); \
        } \
     } while(0)

string test1() {
    TradeStat ts;
    vector<string> trades{"52924702,aaa,13,1136",
                          "52924702,aac,20,477",
                          "52925641,aab,31,907",
                          "52927350,aab,29,724",
                          "52927783,aac,21,638",
                          "52930489,aaa,18,1222",
                          "52931654,aaa,9,1077",
                          "52933453,aab,9,756"};
 
    for(auto&s: trades)
        ts.addTrade(s);
    string expected = "aaa,5787,40,1161,1222\naab,6103,69,810,907\naac,3081,41,559,638\n";
    ostringstream myoss;
    myoss << ts;
    CHECK(myoss.str() == expected);
    return "test1 OK";
}

string test2() {
    TradeStat ts;
    vector<string> trades{"52924702,aaa,13,1136",
                          "52924702,aaa,100,1136"};
 
    for(auto&s: trades)
        ts.addTrade(s);
    string expected = "aaa,0,113,1136,1136\n";
    ostringstream myoss;
    myoss << ts;
    CHECK(myoss.str() == expected);
    return "test2 OK";
}

int main() {
    cout << test1() << endl;;
    cout << test2() << endl;;
}

