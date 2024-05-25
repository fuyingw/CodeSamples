#pragma once

#include <iostream>
#include <unordered_map>
#include <sstream>
#include <set>

using std::string;

constexpr char DELIM = ',';

struct Trade {
    long timestamp;
    string symbol;
    int quantity;
    long price;
    explicit Trade(const string& trade_msg);
};


class TradeStat {
public:
    void addTrade(const Trade& trade);
    void addTrade(const string& msg) {
        Trade trade{msg};
        addTrade(trade);
    }

private:
    struct Stat {
        long lastTime;
        long maxTimeGap;
        long value;
        long maxPrice;
        int volume;
    };

    std::unordered_map<string, Stat> stats;
    std::set<string> symset; //keep the symbol in order

friend std::ostream& operator<< (std::ostream& os, const TradeStat&);

};


Trade::Trade(const string& trade_msg){
    std::istringstream iss(trade_msg);
    std::string s;
    int idx = 0;
    while(getline(iss, s, DELIM)) {
        switch(idx) {
            case 0:
                timestamp = stol(s);
                break;
            case 1:
                symbol = s;
                break;
            case 2:
                quantity = stoi(s);
                break;
            case 3:
                price = stoi(s);
                break;
            default:
                break;
        }
        ++idx;
    }
}

void TradeStat::addTrade(const Trade& trade) {
    auto it = stats.find(trade.symbol);
    if(it!= stats.end()) {
        auto& stat = it->second;
        stat.maxTimeGap = std::max(stat.maxTimeGap, trade.timestamp - stat.lastTime);
        stat.lastTime = trade.timestamp;
        stat.value += trade.quantity * trade.price;
        stat.volume += trade.quantity;
        if(stat.maxPrice < trade.price)
            stat.maxPrice = trade.price;
    }
    else {
        symset.insert(trade.symbol);
        stats[trade.symbol] = Stat{trade.timestamp, 0, trade.quantity*trade.price,
                                   trade.price, trade.quantity};
    }
}

std::ostream& operator<< (std::ostream& os, const TradeStat& tstat) {
    // format: <symbol>,<MaxTimeGap>,<Volume>,<WeightedAveragePrice>,<MaxPrice>
    for(auto& sym: tstat.symset) {
        auto& stat = tstat.stats.at(sym);
        os << sym <<","
           << stat.maxTimeGap <<","
           << stat.volume << ","
           << stat.value/stat.volume << ","
           << stat.maxPrice << "\n";
    }
    return os;
}
