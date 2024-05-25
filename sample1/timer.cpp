/*
Coding sample

Generator:
Every 0.1 second generate a timer event : current time + random number of millisecond (0 - 200)
Wrap it in a timer struct {timer_id and scheduled_time} and hand over to Worker.
When a timer event is generated, log a line with the timer_id, scheduled_time and current_time (event generated)
20 events should be enough to demonstrate

Worker:
When a timer’s scheduled_time is reached:
log a line with timer_id, scheduled_time, current_time (event fired), and throw away this timer.
Try to make the timer fire as close to the scheduled time as possible

Generator and Worker should run in its own threads.
*/

#include <iostream>
#include <type_traits>
#include <list>
#include <thread>
#include <chrono>
#include <random>
#include <optional>
#include <atomic>
#include <mutex>

using namespace std;
using namespace std::chrono;

constexpr auto generate_interval = 100ms;
constexpr auto no_sleep_duration = 1000ns;
constexpr auto threshold_before_sleep = 100000ns;

enum class Event {GenerateTimer, FireTimer};

struct timer {
    int timer_id; // positive number for valid timer. -1 indicates end. 
    long scheduled_time;  // nanoseconds since apoch
};


/*
 * CircularQueue definition
 */

template<typename T>
void cleanup(T* pt) {
    pt->~T();
}

template<>
void cleanup<timer>(timer* pt) {
}

template<typename T, size_t N=100>
class CircularQueue {
    atomic<int> start;
    atomic<int> end;
    char arr[(N+1)*sizeof(T)];
    
public:
    typedef T value_type;
    CircularQueue(): start{0}, end{0} {
        static_assert(N>0, "queue size must be positive");
    }
    ~CircularQueue() {
        auto startv = start.load(memory_order_relaxed);
        auto endv = end.load(memory_order_relaxed);
        for(auto i=startv; i!=endv;){
            cleanup((T*)(arr+sizeof(T)*i));
            ++i;
            if(i==N+1) i=0;
        }
    }

    bool enQueue(const T& t) {
        int startv = start.load(memory_order_acquire);
        int endv = end.load(memory_order_relaxed);
        if((endv+1)%(N+1) != start) {
            new(arr+endv*sizeof(T)) T(t); 
            ++endv;
            if(endv == N+1) endv = 0;
            end.store(endv, memory_order_release);
            return true;
        }
        return false;
    }
    
    optional<T> deQueue() {
        auto res = optional<T> {};
        int startv = start.load(memory_order_relaxed);
        int endv = end.load(memory_order_relaxed);
        if(startv!=endv) {
            T t = std::move(*(T*)(arr + startv*sizeof(T)));
            cleanup((T*)(arr+startv*sizeof(T)));
            ++startv;
            if(startv == N+1) startv = 0;
            start.store(startv, memory_order_relaxed);
            res = std::move(optional<T>{t});
        }
        return res;
    }
};

using Queue = CircularQueue<timer, 10>;
Queue queue;

string log_msg(timer tmr, long now_nano, Event ev){
    char msg[256];
    if(ev == Event::GenerateTimer) {
        sprintf(msg,
        "timer_id:%d,scheduled_time:%ld,current_time(generate):%ld",
                tmr.timer_id, tmr.scheduled_time, now_nano);
    }
    else {
        sprintf(msg,
        "timer_id:%d,scheduled_time:%ld,current_time(fire):%ld,slippage:%ld",
                tmr.timer_id, tmr.scheduled_time, now_nano,
                now_nano-tmr.scheduled_time);
    }
    return msg;
}

void print_msgs(const vector<string>& msgs, string header="") {
    static mutex w_mut;
    lock_guard<mutex> lck(w_mut);
    cout << header << "\n";
    for(int i=0; i<msgs.size(); ++i)
        cout << msgs[i] << "\n";
    cout << endl;
}

int genera(int repeats) {
    random_device rd;
    std::default_random_engine gen(rd());
    std::uniform_int_distribution<int> dis(0, 200);
    int id = 1;
    int count = 0;
    vector<string> msgs;
    msgs.reserve(repeats);
    auto start = high_resolution_clock::now();
    auto start_epoch = duration_cast<nanoseconds>(start.time_since_epoch());
    long curr_time = start_epoch.count();
    do{
        ++count;
        long milli = dis(gen);
        timer tmr{id++, curr_time + milli*1000000};
        msgs.emplace_back(log_msg(tmr, curr_time, Event::GenerateTimer));
        while(!queue.enQueue(tmr));
        if(count >= repeats) {
            timer end_timer{-1, 0L};
            while(!queue.enQueue(end_timer));
            break;
        }
        start += generate_interval;
        auto now = high_resolution_clock::now();
        do {
           auto nanodiff = duration_cast<nanoseconds>(start-now).count();
           if(nanodiff <=0) break;
           nanoseconds dur{nanodiff};
           if(dur > threshold_before_sleep) {
               this_thread::sleep_for(dur - no_sleep_duration);
           }
           now = high_resolution_clock::now();
        } while(true);
        curr_time = duration_cast<nanoseconds>(now.time_since_epoch()).count();
    }while(true);
    print_msgs(msgs, "Generator Events:");
    return 0;
}

void add_timer_tolist(list<timer>& timers, timer tmr){
    auto it = timers.begin();
    for(; it!=timers.end(); ++it) {
        if((*it).scheduled_time > tmr.scheduled_time) break; 
    }
    timers.insert(it, tmr);
}


int work(int repeats){
    list<timer> timers;
    vector<string> msgs;
    msgs.reserve(repeats);
    bool flag = false;
    auto now_nano =
    duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
    while(true) {
        while(!timers.empty()){
            auto tmr = timers.front();
            now_nano = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
            if(tmr.scheduled_time > now_nano) break;
            msgs.emplace_back(log_msg(tmr,now_nano, Event::FireTimer));
            timers.pop_front();
        }
        if (flag){
            if(timers.empty()) break;
            else {
                continue;
            }
        }

        while(auto tim = queue.deQueue())
        {
            if(tim->timer_id == -1) {
                flag = true;
                break;
            }    
            now_nano = duration_cast<nanoseconds>(high_resolution_clock::now().time_since_epoch()).count();
            if ((*tim).scheduled_time <= now_nano) {
                msgs.emplace_back(log_msg(*tim, now_nano, Event::FireTimer));
            }
            else {
                add_timer_tolist(timers, *tim);
            }
        }
    }
    print_msgs(msgs, "Worker Events:");
    return 0;
}


int main() {
    int repeats = 20; // try 20 events
    thread generator(genera, repeats);
    thread worker(work, repeats);
    generator.join();
    worker.join();
    return 0;
}
