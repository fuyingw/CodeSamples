#include <map>
#include <list>
#include <queue>
#include <string>
#include <string_view>
#include <vector>
#include <cassert>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

using namespace std;

enum class OrderSide {
    BUY,
    SELL,
    UNKNOWN
};

inline OrderSide getOrderSide(string_view sstr) {
    if(sstr == "BUY") {
        return OrderSide::BUY;
    }
    else if(sstr == "SELL") {
        return OrderSide::SELL;
    }
    return OrderSide::UNKNOWN;
}

enum class OrderType {
    IOC,
    GFD,
    MKT,
    UNKNOWN
};

inline OrderType getOrderType(string_view otstr) {
    if(otstr == "IOC") {
        return OrderType::IOC;
    }
    else if(otstr == "GFD") {
        return OrderType::GFD;
    }
    return OrderType::UNKNOWN;
}

enum class MessageType {
    NEW,
    CANCEL,
    MODIFY,
    PRINT,
    UNKNOWN
};

inline MessageType getMessageType(string_view tstr) {
        if(tstr == "NEW") {
            return MessageType::NEW;
        }
        else if (tstr == "CANCEL") {
            return MessageType::CANCEL;
        }
        else if (tstr == "MODIFY") {
            return MessageType::MODIFY;
        }
        else if (tstr == "PRINT") {
            return MessageType::PRINT;
        }        
        return MessageType::UNKNOWN;
}

struct Order {
    OrderSide side;
    OrderType type;
    unsigned long price;
    int quantity;   // the original order quantity, assigned when initialized
    int leaves;     // leaves initialized with original quantity. will change when trade happens. 
    string orderId;
    bool doneFlag;  // when doneFlag is true (set when order is canceled or fully filled), order is finished. and no further action
};

class TradeDetail {
public:
    TradeDetail(const std::string& orderIdBook_, unsigned long priceBook_, const std::string& orderIdComing_,
                unsigned long priceComing_, int fillQty_) :
               orderIdBook(orderIdBook_)
               , priceBook(priceBook_)
               , orderIdComing(orderIdComing_)
               , priceComing(priceComing_)
               , fillQty(fillQty_)
               {}

private:
    std::string orderIdBook;
    unsigned long priceBook;
    std::string orderIdComing;
    unsigned long priceComing;
    int fillQty;

friend void printTrade (ostream& os, const TradeDetail& trade);

};

void printTrade(ostream& os, const TradeDetail& trade) {
    os << "TRADE"
       << " " << trade.orderIdBook
       << " " << trade.priceBook
       << " " << trade.fillQty
       << " " << trade.orderIdComing
       << " " << trade.priceComing
       <<"\n";
}

class PriceLevel {
public:
    PriceLevel(unsigned long price_): price(price_), quantity(0) {}
    unsigned long getPrice() const {return price;}
    int getQuantity() const { return quantity;}
    
    list<Order*>::iterator addOrder(Order* porder) { // add to the end of list and return the iterator
        assert(porder != nullptr);
        assert(porder->price == price);
        orderList.push_back(porder);
        quantity += porder->leaves;
        return --(orderList.end());
    }
    
    int executeLevel(const std::string& incomingOrderId, unsigned long price, int qty, vector<TradeDetail>& trades) {
        // the logic to decide whether to execute at this level is in OrderBook tryMatchOrder function
        assert(qty >= 0 );
        auto it = orderList.begin();
        while(it!=orderList.end() && qty>0) {
            auto& order = *(orderList.front());
            if(order.doneFlag) {
                orderList.pop_front();
            }
            else {
                auto execQty = min (order.leaves, qty);
                trades.push_back(TradeDetail{order.orderId, order.price, incomingOrderId, price, execQty});
                quantity -= execQty;
                qty -= execQty;
                order.leaves -= execQty;
                if(order.leaves == 0) {
                    order.doneFlag = true;
                    orderList.pop_front();
                }
            }
            it = orderList.begin();
        }
        return qty;  // return leaves
    } 

private:
    unsigned long price;
    int quantity;
    list<Order*> orderList;

friend class OrderBook;
        
};


class OrderBook {
    // only GFD order goes into an order book
public:
    list<Order*>::iterator addOrder(Order* porder) {
        PriceLevel& level = getLevelCreate(porder->price);
        return level.addOrder(porder);
    }
    
    bool cancelOrder(Order* porder) {
        auto plevel = getLevel(porder->price);
        if(plevel) {
            plevel->quantity -= porder->leaves; 
            if(plevel->quantity == 0) {
                levelMap.erase(porder->price);
            }
            return true;
        }
        return false;
    }
    
    bool reduceOrder(Order* porder, int delta) {
        assert(delta >= 0);
        auto plevel = getLevel(porder->price);
        if(plevel) {
            plevel->quantity -= delta;
            return true;
        }
        return false;
    }
 
    void printBook(ostream& os) {
        // print book from higher price to lower price
        for(auto it=levelMap.rbegin(); it!=levelMap.rend(); ++it) {
            os << it->second.price
               << " " << it->second.quantity
               << "\n";
        }
    }
    pair<bool, int> tryMatchOrder(Order& order, vector<TradeDetail>& trades); //bool to indicate if match happened, and unsigned long the matched quantity
    
private:
    map<unsigned long, PriceLevel> levelMap;   // price to PriceLevel map. alternatively different data structure can be used 
    PriceLevel* getLevel(unsigned long price) {
        auto it = levelMap.find(price);
        if(it != levelMap.end()) {
            return &(it->second); 
        }
        return nullptr;
    }
    PriceLevel& getLevelCreate(unsigned long price) { //if level for the price does not exist, create a new Pricelevel
        auto plevel = getLevel(price);
        if(plevel) {
            return *plevel;
        }
        auto [it, flag] = levelMap.insert(make_pair(price, PriceLevel(price)));
        if(! flag) { // insert fail. something really bad happened
            throw runtime_error("LevelMap insert failure");
        }
        return it->second;
    }
    
    PriceLevel* getTopOfBook(OrderSide side) {
        if(!levelMap.empty()) {
            return side==OrderSide::BUY ? &((--(levelMap.end()))->second) : &(levelMap.begin()->second);
        }
        return nullptr;
    }
    
    void removeTopOfBook(OrderSide side) {
        if(!levelMap.empty()) {
            if(side == OrderSide::BUY) { 
                levelMap.erase(--(levelMap.end()));
            }
            else {
                levelMap.erase(levelMap.begin());
            }
        }
    }
    
    template<OrderSide side>
    std::pair<bool, int> tryMatchOrderImpl(Order& order, vector<TradeDetail>& trades) {
        auto levelPriceGoodForMatch = [] (PriceLevel* plevel, Order& order) { //side is the opposite of order side
            if constexpr (side == OrderSide::SELL) {
                return plevel->price <= order.price;
            }
            if constexpr (side == OrderSide::BUY) {
                return plevel->price >= order.price;
            }
        };
        auto plevel = getTopOfBook(side);
        auto leaves = order.leaves;
        while (leaves>0 && plevel && levelPriceGoodForMatch(plevel, order)) {
            leaves = plevel->executeLevel(order.orderId, order.price, leaves, trades);
            if(plevel->quantity == 0) {
                removeTopOfBook(side);
            }
            if(leaves >0) {
                plevel = getTopOfBook(side);
            }
        }
        if(leaves == order.leaves) { // no fill happened
            return {false, 0};
        }
        auto filled = order.leaves - leaves;
        assert(filled > 0);
        order.leaves = leaves;
        return {true, filled};           
    } 
        
};

std::pair<bool, int> OrderBook::tryMatchOrder(Order& order, vector<TradeDetail>& trades) {
    // The order is on the side of the book
    switch(order.side) {
        case OrderSide::BUY:
            return tryMatchOrderImpl<OrderSide::SELL>(order, trades);
        case OrderSide::SELL:
            return tryMatchOrderImpl<OrderSide::BUY>(order, trades);
        default:
            break;
    }
    return {false, 0};
}

// OrderMemoryPool provide memory to hold Orders.
// This MemoryPool only hand out allocated memory to create a new order. and does not reclaim until the pool is cleaned 
class OrderMemoryPool {
public:
    OrderMemoryPool(size_t sz = 1024) : currIdx{0} {
        allocations.push_back(pair<void*, size_t> {malloc(sizeof(Order)*sz), sz});
    }
    
    ~OrderMemoryPool() {
        int N=allocations.size();
        for(int i=0; i<N-1; ++i) {
            auto [p, sz] = allocations[i];
            for(size_t j=0; j<sz; ++j) {
                ((Order*)p + j)->~Order();
            }
            free(p);
        }
        auto [p, sz] = allocations.back();
        for(size_t j=0; j<currIdx; ++j) {
            ((Order*)p + j)->~Order();
        }
        free(p);
    }
    
    void* getNext() {
        auto p = allocations.back();
        if(currIdx == p.second) {  // memory used out, need more allocation 
            auto newsz = p.second * 2;
            allocations.push_back(pair<void*, size_t> {malloc(sizeof(Order)*newsz), newsz});
            currIdx = 0;
            p = allocations.back();
        }
        return (void*)((Order*)(p.first) + (currIdx++));
    }
    
private:
    size_t currIdx;
    vector<pair<void*, size_t>> allocations;    
};


class MatchEngine {
public:

    MatchEngine(istream& is_=std::cin, ostream& os_=std::cout): is(is_), os(os_), ordpool(256) {}

    bool processInputLine(const string& msg) {
        // parse the msg first then process them
        vector<string> inputFields;
        size_t pos = 0;
        size_t start = 0;
        while((pos=msg.find(' ', start)) != string::npos) {
            inputFields.push_back(msg.substr(start, pos-start));
            start = pos + 1;
        }
        inputFields.push_back(msg.substr(start));
        auto mtype = getMessageType(inputFields[0]);
        switch(mtype) {
            case MessageType::NEW:
                {
                    if(inputFields.size() != 6) {
                        std::cerr << "Bad input for new order: " << msg<< " Ignored.\n";
                        return false;
                    }
                    auto oside = getOrderSide(inputFields[1]);
                    auto otype = getOrderType(inputFields[2]);
                    long price = stol(inputFields[3]);
                    int qty = stoi(inputFields[4]);
                    if(price <=0 || qty <=0 || inputFields[5].empty()) {
                        return false; //bailout if price/qty/orderId is invalid
                    }
                    if(oside == OrderSide::BUY) {
                        processBuyOrder(inputFields[5], otype, price, qty);
                    }
                    else if(oside == OrderSide::SELL) {
                        processSellOrder(inputFields[5], otype, price, qty);
                    }
                }
                break;
            case MessageType::CANCEL:
                {
                    if(inputFields.size() != 2) {
                        std::cerr << "Bad input for Cancel Order: " << msg << " Ignored.\n";
                        return false;
                    }
                    cancelOrder(inputFields[1]);
                }
                break;
            case MessageType::MODIFY:
                {
                    if(inputFields.size() != 5) {
                        std::cerr << "Bad input for Modify Order: " << msg << " Ignored.\n";
                        return false;     
                    }
                    auto side = getOrderSide(inputFields[2]);
                    auto price = stol(inputFields[3]);
                    auto qty = stoi(inputFields[4]);
                    if(price <=0 || qty <=0 || inputFields[1].empty()) {
                        return false;
                    }    
                    modifyOrder(inputFields[1], side, price, qty);
                }
                break;
            case MessageType::PRINT:
                {
                    if(inputFields.size() != 1) {
                        std::cerr << "Bad input for PRINT: " << msg << " Ignored.\n";
                        return false;     
                    }
                    printBook();
                }
                break;
            default:
                return false;
        }       
        return true;
    } 
    
    void printBook() {
        os << "SELL:\n";
        sellBook.printBook(os);
        os << "BUY:\n";
        buyBook.printBook(os);
    }
    
    void run() {
        std::string input;
        while(getline(is, input)) {
            processInputLine(input);
        }
    }
    
private:
    istream& is;
    ostream& os;
    OrderMemoryPool ordpool;    
    OrderBook buyBook;
    OrderBook sellBook;
    unordered_map<string, Order*> orderMap;
    
    template<OrderSide side> 
    bool processNewOrder(Order* porder) {
        auto matchBook = [this]() -> auto& {
            if constexpr (side == OrderSide::BUY) {
                return sellBook;
            }
            if constexpr (side == OrderSide::SELL) {
                return buyBook;
            }
        };
        auto resBook = [this]() -> auto& {
            if constexpr (side == OrderSide::BUY) {
                return buyBook;
            }
            if constexpr (side == OrderSide::SELL) {
                return sellBook;
            }
        };
        vector<TradeDetail> trades;
        auto ret = matchBook().tryMatchOrder(*porder, trades);
        if(ret.first) { // match happened
            for(auto& trade: trades) {
                printTrade(os, trade);
            }
        }
        if(porder->leaves > 0 && porder->type == OrderType::GFD) { // only GFD order goes to the the orderbook
            auto it = resBook().addOrder(porder);
        }
        else {
            porder->doneFlag = true;
        }
        return true;
        
    }
    
    bool processBuyOrder(const std::string& orderId, OrderType otype, unsigned long price, int qty){
        if(orderMap.count(orderId)) { //already exists
            return false;
        }
        Order* porder = new(ordpool.getNext()) Order{OrderSide::BUY, otype, price, qty, qty, orderId, false};
        orderMap[orderId] = porder; //Any order passed validation check has a place in the map
        return processNewOrder<OrderSide::BUY>(porder);
    }        

    bool processSellOrder(const std::string& orderId, OrderType otype, unsigned long price, int qty) {
        if(orderMap.count(orderId)) {
            return false;
        }
        Order* porder = new(ordpool.getNext()) Order{OrderSide::SELL, otype, price, qty, qty, orderId, false};
        orderMap[orderId] = porder;
        return processNewOrder<OrderSide::SELL>(porder);
    }

    bool modifyOrder(const std::string& orderId, OrderSide side, unsigned long price, int qty) {
        //find the order, delete it and add the new order
        //new order has to be on the same side of the old order
        //if new order qty is less than or equal to filled qty. new order will not be created
        //if qty is smaller while price no change, it does not change the order priority
        auto it = orderMap.find(orderId);
        if(it == orderMap.end() || it->second->doneFlag || it->second->side != side) { //either order not exist or order is done already or side changed, do nothing
            return false;
        }
        
        // mark the old order as Done
        auto fillQty = it->second->quantity - it->second->leaves;
        if(fillQty < qty && qty <= it->second->quantity && it->second->price == price) {
            auto delta = it->second->quantity - qty;
            it->second->quantity = qty;
            it->second->leaves = qty - fillQty;
            // update level
            if(side == OrderSide::BUY) {
                buyBook.reduceOrder(it->second, delta); 
            }
            else {
                sellBook.reduceOrder(it->second, delta);
            }
            return true;
        }

        it->second->doneFlag = true;
        if(it->second->side == OrderSide::BUY) {
            buyBook.cancelOrder(it->second);
        }
        else {
            sellBook.cancelOrder(it->second);
        }
        
        if(fillQty >= qty ) {
            return false;
        }

        Order* porder = new(ordpool.getNext()) Order(*(it->second));

        // adding new order into map:
        orderMap[orderId] = porder;
        
        // set the attributes of the new order
        porder->quantity = qty;
        porder->price = price;
        porder->leaves = qty - fillQty;
        porder->doneFlag = false;
        
        if(side == OrderSide::BUY) {
            return processNewOrder<OrderSide::BUY>(porder);
        }
        else {
            return processNewOrder<OrderSide::SELL>(porder);
        }
        return false;   
    }
    bool cancelOrder(const std::string& orderId) {
        // return true if no error happens
        auto it = orderMap.find(orderId);
        if(it == orderMap.end() || it->second->doneFlag) { // order not found or already done
            return false;
        }
        it->second->doneFlag = true;
        if(it->second->side == OrderSide::BUY) {
            buyBook.cancelOrder(it->second);
        }
        else {
            sellBook.cancelOrder(it->second);
        }
        return true;        
    } 
};

int main() {
    /* Read input from STDIN. Print output to STDOUT */
    MatchEngine engine;
    engine.run();
    return 0;
}

