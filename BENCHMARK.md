# Thread Pool Benchmark Comparison

## Platform
- **OS:** WSL2 Ubuntu
- **CPU:** Intel i5-10600KF (6*2) @ 4.10 GHz
- **RAM:** 16GB DDR4 @ 2400 MHz
- **CC:** clang++ (14.0.0-1ubuntu1.1) -O3
- **valgrind** 3.18.1

## Results

| Benchmark | Runtime (ms) | CPU instructions (Ir) | Allocations |
|------------|---------|-----------------------|-------------|
| **pthread_pool_noalloc** | ~65 | 90,753,256 | 14 |
| **pthread_pool_batch** | ~82 | 151,993,058 | 480,062 |
| **pthread_pool_default** | ~100 | 180,217,625 | 480,062 |
| boost::asio::thread_pool | ~145 | 270,084,159  | 185 |
| BS::thread_pool | ~144 | 243,526,272 | 487,676 |
| Intel TBB | ~22 | 165,683,337 | 39 |

## Code

```c
struct f1_task{pthread_pool_task_t base; int cnt;};
static void f1(pthread_pool_t * const pool,struct f1_task * const args){
    if(pool){
        if(args->cnt){
            struct f1_task *t=(struct f1_task *)malloc(sizeof(*t));
            t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f1;
            t->cnt=args->cnt-1;
            pthread_pool_task(pool,t,0);
        }
    }
    free(args);
}

static void pthread_pool_default(void){
    struct timespec ts[2];
    pthread_pool_t *p;
    const unsigned int cores=12;
    unsigned int i=cores*4;

    pthread_pool_create(&p,NULL,cores,0);

    clock_gettime(CLOCK_REALTIME,ts+0);
    while(i--){
        struct f1_task *t=(struct f1_task *)malloc(sizeof(*t));
        t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f1;
        t->cnt=10000;
        pthread_pool_task(p,t,0);
    }
    pthread_pool_wait(p);
    clock_gettime(CLOCK_REALTIME,ts+1);

    pthread_pool_destroy(p,1);
    printf("pthread_pool_default rt: %f ms\n",(ts[1].tv_sec-ts[0].tv_sec)*1000. + (ts[1].tv_nsec-ts[0].tv_nsec)/1000000.);
}
```

```c
struct f2_task{pthread_pool_task_t base; int cnt;};
static void f2(pthread_pool_t * const pool,struct f2_task * const args){
    if(pool){
        if(args->cnt){
            struct f1_task *t=(struct f2_task *)malloc(sizeof(*t));
            t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f2;
            t->cnt=args->cnt-1;
            pthread_pool_task(pool,t,0);
        }
    }
    free(args);
}

static void pthread_pool_batch(void){
    struct timespec ts[2];
    pthread_pool_t *p;
    const unsigned int cores=12;
    unsigned int i=cores*4;

    pthread_pool_create(&p,NULL,cores,0);
    pthread_pool_batch(p,4);

    clock_gettime(CLOCK_REALTIME,ts+0);
    while(i--){
        struct f2_task *t=(struct f2_task *)malloc(sizeof(*t));
        t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f2;
        t->cnt=10000;
        pthread_pool_task(p,t,0);
    }
    pthread_pool_wait(p);
    clock_gettime(CLOCK_REALTIME,ts+1);

    pthread_pool_destroy(p,1);
    printf("pthread_pool_batch rt: %f ms\n",(ts[1].tv_sec-ts[0].tv_nsec)*1000. + (ts[1].tv_sec-ts[0].tv_nsec)/1000000.);
}
```

```c
struct f3_task{pthread_pool_task_t base; int cnt;};
static void f3(pthread_pool_t * const pool,struct f3_task * const args){
    if(pool && args->cnt--) pthread_pool_task(pool,args,0);
}

static void pthread_pool_noalloc(void){
    struct timespec ts[2];
    struct f3_task t[48];
    pthread_pool_t *p;
    const unsigned int cores=12;
    unsigned int i=cores*4;

    pthread_pool_create(&p,NULL,cores,0);
    pthread_pool_batch(p,4);

    clock_gettime(CLOCK_REALTIME,ts+0);
    while(i--){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f3;
        t[i].cnt=10000;
        pthread_pool_task(p,t+i,0);
    }
    pthread_pool_wait(p);
    clock_gettime(CLOCK_REALTIME,ts+1);

    pthread_pool_destroy(p,1);
    printf("pthread_pool_noalloc rt: %f ms\n",(ts[1].tv_sec-ts[0].tv_sec)*1000. + (ts[1].tv_nsec-ts[0].tv_nsec)/1000000.);
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
    const unsigned int cores=12;
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
    const unsigned int cores=12;
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
