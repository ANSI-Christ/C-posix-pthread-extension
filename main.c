#include <time.h>
#include <stdio.h>

#define PTHREAD_EXT_IMPL
#include "pthread_ext.h"


#define RUNTIME(...) ({\
    struct timespec _rt1={0},_rt2={0};\
    clock_gettime(CLOCK_REALTIME,&_rt1);\
    { __VA_ARGS__ ;}\
    clock_gettime(CLOCK_REALTIME,&_rt2);\
    timespec_sub(&_rt2,_rt1.tv_sec,_rt1.tv_nsec);\
    (_rt2.tv_sec + _rt2.tv_nsec/1000000000.);\
})

static void timespec_norm(struct timespec *tm){
    tm->tv_sec+=(tm->tv_nsec/1000000000);
    tm->tv_nsec%=1000000000;
}

static void timespec_sub(struct timespec *tm,int sec,int nanosec){
    sec+=nanosec/1000000000;
    nanosec%=1000000000;
    timespec_norm(tm);
    if(tm->tv_sec >= sec){
        tm->tv_sec-=sec;
        if(tm->tv_nsec>nanosec){
            tm->tv_nsec-=nanosec;
        }else if(tm->tv_sec--){
            tm->tv_nsec+=1000000000-nanosec;
        }else tm->tv_sec=tm->tv_nsec=0;
    }else tm->tv_sec=tm->tv_nsec=0;
}




static void repost(struct{int cnt;} *args,int index,void *pool){
    if(args->cnt) pthread_pool_task(pool,repost,args->cnt-1);
}

static void benchmark(void){
    const unsigned int cores=4;
    pthread_pool_t p=pthread_pool_create(cores,0,0);
    printf("rt: %f\n",RUNTIME(
        unsigned int i=cores*4;
        while(i--)
            pthread_pool_task(p,repost,1000000);
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(&p);
}




static void f2(struct{int a; double b; const char *c;} *args){
    printf("args: %d, %f, %s\n",args->a,args->b,args->c);
}

static void test_task_arguments(void){
    pthread_pool_t p=pthread_pool_create(1,0,0);
    pthread_pool_task(p, f_args, 1, 1.1, (const char*)"str_1" );
    pthread_pool_task(p, f_args, 2, 2.2, (const char*)"str_2" );
    pthread_pool_destroy_later(&p);
}




static void *f3(int *run){
    struct timespec t={0,5000000};
    pthread_pausable(1);
    while(*run){
        puts("run...");
        nanosleep(&t,NULL);
    }
    return NULL;
}

static void test_pause_resume(void){
    int run=1;
    pthread_t t;
    pthread_create(&t,0,(void*)f3,&run);
    while(run)
        switch(getchar()){
            case 'p': pthread_pause(t); puts("pause"); break;
            case 'c': pthread_resume(t); puts("resume"); break;
            case 'q':
                pthread_pause(t);
                run=0;
                pthread_resume(t);
                puts("quit");
                break;
        }
    pthread_join(t,NULL);
}




int main(int argc, char **argv){
    benchmark();
    test_task_arguments();
//    test_pause_resume();
    return 0;
}
