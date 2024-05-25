#include <iostream>
#include <fstream>
#include "TradeStat.hpp"

int main(int argc, char* argv[]) {
    if(argc != 3) {
        std::cerr << "Usage: " << argv[0]
                  << " <inputfile> <outputfile>" << std::endl;
        return -1;
    }

    std::ifstream ifs(argv[1]);
    std::ofstream ofs(argv[2]);
    TradeStat tstat;
    string s;
    while(getline(ifs, s)) {
        tstat.addTrade(s);
    }

    ofs << tstat;
}
