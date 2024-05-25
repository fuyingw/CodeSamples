#include <iostream>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

#include "TradeStat.hpp"

using namespace std;

TEST(TradeStatGTest, trades) {
    TradeStat ts;
    vector<string> trades{"52924702,aaa,13,1136",
                          "52924702,aaa,100,1136"};
 
    for(auto&s: trades)
        ts.addTrade(s);
    string expected = "aaa,0,113,1136,1136\n";
    ostringstream myoss;
    myoss << ts;
    ASSERT_EQ(expected, myoss.str());

}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

