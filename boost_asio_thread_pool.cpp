#include <chrono>
#include <ostream>
#include <thread>
#include <future>

#include <boost/asio/thread_pool.hpp>
#include <boost/asio/post.hpp>

extern int printf(const char*,...);

struct TASK{
    TASK(boost::asio::thread_pool *pool,std::promise<void> *waiter,unsigned int count):p(pool),w(waiter),c(count){}
    void operator()(){
        if(c){boost::asio::post(*p,TASK(p,w,c-1)); return;}
        w->set_value();
    }
private:
    boost::asio::thread_pool *p;
    std::promise<void> *w;
    unsigned int c;
};

auto current_time(void){
    return std::chrono::high_resolution_clock::now().time_since_epoch().count();
}

int main(void){
    const unsigned int cores=std::thread::hardware_concurrency();
    std::promise<void> waiters[cores*4];
    boost::asio::thread_pool p(cores);

    printf("cores:%u\n",cores);

    unsigned int i=sizeof(waiters)/sizeof(*waiters);
    auto t1=current_time();
    for(auto& w : waiters)
        boost::asio::post(p,TASK(&p,&w,100000));
    for(auto& w : waiters)
        w.get_future().wait();
    auto t2=current_time();
    printf("rt: %f\n",(t2-t1)/1000000000.0);

    return 0;
}
