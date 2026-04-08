# Thread Pool Benchmark Comparison

## Platform
- **OS:** WSL2 Ubuntu
- **CPU:** Intel i5-10600KF (6*2) @ 4.10 GHz
- **RAM:** 16GB DDR4 @ 2400 MHz

## Results

| Metric | pthread_pool_noalloc | pthread_pool_batch | pthread_pool_default | boost::asio::thread_pool | BS::thread_pool | TBB |
|--------|---------------------|-------------------|---------------------|-------------|-----------------|-----|
| **Runtime (ms)** | 64.80 | 82.60 | 102.90 | 145.82 | 144.74 | 21.72 |
| **CPU Instructions (Ir)** | 107,279,783 | 162,153,147 | 196,017,821 | 270,084,159 | 243,526,272 | 165,683,337 |
| **L1 D-cache Misses (D1mr)** | 2,449 | 2,739 | 2,520 | 16,196 | 60,542 | 17,538 |
| **LL-cache Misses (DLmr)** | 1,516 | 1,517 | 1,518 | 7,802 | 7,807 | 9,098 |
| **Peak Heap Memory (Massif)** | 4.2 KB | 4.2 KB | 5.8 KB | 84.4 KB | 84 KB | 80 KB |
| **Total Allocations** | 26 | 480,074 | 480,074 | 185 | 487,676 | 39 |
| **Memory Leaks** | 0 bytes | 0 bytes | 0 bytes | 0 bytes | 0 bytes | 1,152 bytes |

## Code

```c
static int f1(void * const pool,struct{pthread_pool_task_base_t _base; int cnt;} * const args){
    if(pool && args->cnt) pthread_pool_task(pool,f1,args->cnt-1);
    return 0;
}

static void pthread_pool_default(void){
    const unsigned int cores=12;//pthread_cores();
    pthread_pool_t *p=pthread_pool_create(cores,0);
    printf("pthread_pool_default rt: %f ms\n",RUNTIME_MS(
        unsigned int i=cores*4;
        while(i--){
            const int d=10000;
            pthread_pool_task(p,f1,d);
        }
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(p,1);
}
```

```c
static int f2(void * const pool,struct{pthread_pool_task_base_t _base; int cnt;} * const args){
    if(pool && args->cnt) pthread_pool_task(pool,f2,args->cnt-1);
    return 0;
}

static void pthread_pool_batch(void){
    const unsigned int cores=12;//pthread_cores();
    pthread_pool_t *p=pthread_pool_create(cores,0);
    pthread_pool_batch(p,4);
    printf("pthread_pool_batch rt: %f ms\n",RUNTIME_MS(
        unsigned int i=cores*4;
        while(i--){
            const int d=10000;
            pthread_pool_task(p,f2,d);
        }
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(p,1);
}
```

```c
static int f3(void * const pool,struct{pthread_pool_task_base_t _base; int cnt;} * const args){
    if(pool && args->cnt--) pthread_pool_task_queue(pool,(args),0);
    return 1;
}

static void pthread_pool_noalloc(void){
    const unsigned int cores=12;//pthread_cores();
    pthread_pool_t *p=pthread_pool_create(cores,0);
    struct{pthread_pool_task_base_t t; int cnt;} task[48];
    pthread_pool_batch(p,4);
    printf("pthread_pool_noalloc: %f ms\n",RUNTIME_MS(
        unsigned int i=cores*4;
        while(i--){
            if(pthread_pool_task_create(p,0)){
                task[i].t.task=(int(*)(pthread_pool_t*,void*,unsigned int))f3;
                task[i].cnt=10000;
                pthread_pool_task_queue(p,task+i,0);
            }
        }
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(p,1);
}
```

```cpp
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

static void boost_asio_thread_pool(void){
    const unsigned int cores=12;//std::thread::hardware_concurrency();
    boost::asio::thread_pool p(cores);
    std::promise<void> waiters[cores*4];

    auto t=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for(auto& w : waiters)
        boost::asio::post(p,TASK(&p,&w,10000));
    for(auto& w : waiters)
        w.get_future().wait();

    printf("boost::asio::thread_pool rt: %f ms\n",(std::chrono::high_resolution_clock::now().time_since_epoch().count()-t)/1000000.);
}
```

```cpp
static void BS_thread_pool(void){
    const unsigned int cores = 12;
    BS::thread_pool<> pool(cores);  // <> означает BS::thread_pool<std::function<void()>>
    std::promise<void> waiters[cores*4];

    std::function<void(BS::thread_pool<>*, std::promise<void>*, int)> recursive;
    recursive = [&](BS::thread_pool<>* p, std::promise<void>* w, int cnt) {
        if (cnt) {
            p->detach_task([=]() {
                recursive(p, w, cnt - 1);
            });
        } else {
            w->set_value();
        }
    };

    auto t=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    for(auto& w : waiters)
        pool.detach_task( [&](){recursive(&pool, &w, 10000);} );
    for(auto& w : waiters)
        w.get_future().wait();
    printf("BS::thread_pool rt: %f ms\n",(std::chrono::high_resolution_clock::now().time_since_epoch().count()-t)/1000000.);
}
```

```cpp
struct TBB_TASK{
    TBB_TASK(tbb::task_group *pool,unsigned int count):p(pool), c(count){}
    void operator()() const {
        if(c)p->run(TBB_TASK(p,c-1));
    }
private:
    tbb::task_group *p;
    unsigned int c;
};

static void TBB(void){
    const unsigned int cores=12;//std::thread::hardware_concurrency();
    tbb::task_arena p(cores);

    auto t=std::chrono::high_resolution_clock::now().time_since_epoch().count();
    p.execute([](){
        tbb::task_group tg;
        unsigned int j=cores*4;
        while(j--) tg.run(TBB_TASK(&tg,10000));
        tg.wait();

    });
    printf("TBB rt: %f ms\n",(std::chrono::high_resolution_clock::now().time_since_epoch().count()-t)/1000000.);
}
```
