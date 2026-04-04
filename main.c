#include <time.h>
#include <stdio.h>
#include <stddef.h>


#define PTHREAD_EXT_IMPL
#include "pthread_ext.h"

void sleepf(double sec){
    struct timespec t={sec,0}; t.tv_nsec=(sec-t.tv_sec)*1000000000;
    while(nanosleep(&t,&t));
}

double timespec_seconds(const struct timespec * const t){
    return t->tv_sec + t->tv_nsec/1000000000.;
}

void timespec_normalize(struct timespec * const t){
    t->tv_sec+=t->tv_nsec/1000000000;
    if( (t->tv_nsec%=1000000000)<0){
        t->tv_nsec += 1000000000;
        --t->tv_sec;
    }
}

void timespec_change(struct timespec * const t,const long sec,const long nanosec){
    t->tv_sec+=sec;
    t->tv_nsec+=nanosec;
    timespec_normalize(t);
}

#define timespec_current(_t_) clock_gettime(CLOCK_REALTIME,(_t_))
#define timespec_future(_t_,_s_,_ns_) do{\
    struct timespec * const _1_=(_t_);\
    timespec_current(_1_);\
    timespec_change(_1_,(_s_),(_ns_));\
}while(0)

#define RUNTIME(...) ({\
    struct timespec _rt1[1], _rt2[1];\
    timespec_current(_rt1); {__VA_ARGS__} timespec_current(_rt2);\
    timespec_change(_rt2,-_rt1->tv_sec,-_rt1->tv_nsec);\
    timespec_seconds(_rt2);\
})





static int f1(void *pool,struct{pthread_pool_task_base_t _base; int cnt;} *args,int index){
    if(pool && args->cnt) pthread_pool_task(pool,f1,args->cnt-1);
    return 0;
}

static void benchmark(void){
    const unsigned int cores=pthread_cores();
    pthread_pool_t *p=pthread_pool_create(cores,0);
    pthread_pool_banch(p,3);
    printf("rt: %f\n",RUNTIME(
        unsigned int i=cores*4;
        while(i--)
            pthread_pool_task(p,f1,100000);
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(p,1);
}




static int f2(void *pool,struct{pthread_pool_task_base_t _base; int prio; double fp; const char *str;} *args){
    if(pool) printf("prio[%d]: %f %s\n",args->prio,args->fp,args->str);
    return 0;
}

static void test_prio(void){
    pthread_pool_t *p=pthread_pool_create(1,8);
    pthread_pool_task(p, f2, 0, 0.1, (const char*)"aaa" ); /* prio 0 */
    pthread_pool_task(p, f2, 0, 0.2, (const char*)"bbb" ); /* prio 0 */
    pthread_pool_task_prio(p,1, f2, 1, 1., (const char*)"ccc" ); /* prio 1 */
    pthread_pool_task_prio(p,2, f2, 2, 2., (const char*)"ddd" ); /* prio 2 */
    pthread_pool_task_prio(p,3, f2, 3, 3., (const char*)"eee" ); /* prio 3 */
    pthread_pool_task_prio(p,8, f2, 8, 8., (const char*)"fff" ); /* prio 4 */
    pthread_pool_destroy(p,0);
}




static int f3(void *pool,struct{pthread_pool_task_base_t _base; pthread_channel_t *c;} *args){
    if(pool){
        sleepf(0.1);
        if(args->c){
            const struct{int a; double b;}value={1,2.2};
            pthread_channel_push(args->c,&value,sizeof(value));
        }
    }
    return 0;
}

static void test_channel(void){
    struct{int a; double b;}value;
    pthread_pool_t *p=pthread_pool_create(4,0);
    pthread_channel_t c;
    pthread_channel_open(&c);
    int i=10;

    while(i--) pthread_pool_task(p,f3,NULL);
    pthread_pool_task(p,f3,&c);

    pthread_channel_pop(&c,&value,sizeof(value));
    printf("%d, %f\n",value.a,value.b);

    pthread_pool_destroy(p,0);
    pthread_channel_close(&c);
}


static int f4(void *pool){
    if(pool){sleepf(0.1);printf("task f4 done\n");}
    else printf("task f4 rejected\n");
    return 0;
}

static int f5(void *pool,struct{pthread_pool_task_base_t _base; pthread_channel_t *c;} *args){
    pthread_channel_push(args->c,&pool,sizeof(pool));
    return 0;
}

static void test_reject(void){
    void *value;
    pthread_channel_t c;
    pthread_pool_t *p=pthread_pool_create(1,0);

    pthread_channel_open(&c);

    pthread_pool_task(p,f4);
    pthread_pool_task(p,f4);
    pthread_pool_task(p,f5,&c);
    sleepf(0.05);
    pthread_pool_destroy(p,1); // pthread_pool_clear(p);

    pthread_channel_pop(&c,&value,sizeof(value));
    if(value) printf("task f5 done\n");
    else printf("task f5 rejected\n");

    pthread_channel_close(&c);
}


static int f6(void *p,struct{pthread_pool_task_base_t _base; pthread_group_t *g; unsigned  int n;} * const args){
    const int rejected=pthread_group_rejected(args->g);
    if(rejected){
        if(rejected==1) printf("group destroy at place 1\n\n");
        else if(rejected==-1){/* nothing to do */}
        return 0;
    }
    if(!p){
        if(pthread_group_reject(args->g,1)==1) printf("group destroy at place 2\n\n");
        return 0;
    }
    if(args->n==5){
        if(pthread_group_reject(args->g,1)==1) printf("group destroy at place 3\n\n");
    }else{
        printf("task begin %u\n",args->n);
        sleepf(0.1);
        printf("task end %u\n\n",args->n);
        if(pthread_group_progress(args->g,0)==1) printf("group destroy at place 4\n\n");
    }
    return 0;
}

static void test_group(void){
    pthread_pool_t * const p=pthread_pool_create(1,0);
    pthread_group_t *g=malloc(sizeof(*g));
    struct timespec t[1];
    unsigned int i, done, all;
    pthread_pool_banch(p,0);

    pthread_group_init(g,4,NULL,free); // can be destroy inside task, in nonblocking mode cause group placed not in stack
    for(i=0;i<4;++i) pthread_pool_task(p,f6,(pthread_group_t*)g,i);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,7);
    for(i=0;i<7;++i) pthread_pool_task(p,f6,(pthread_group_t*)g,i);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f6,(pthread_group_t*)g,i);
    timespec_future(t,0,300*1000*1000);
    i=pthread_group_timedwait(g,&done,&all,t);
    printf("group timedwait end = %d [%u / %u]\n\n",i,done,all);
    pthread_group_reject(g,0);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f6,(pthread_group_t*)g,i);
    sleepf(0.15);
    pthread_pool_unpending(p);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait unpending end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f6,(pthread_group_t*)g,i);
    timespec_future(t,0,250*1000*1000);
    i=pthread_group_timedwait(g,&done,&all,t);
    if(pthread_group_destroy(g)==1)
        printf("group destroy at place 0\n\n");

    pthread_pool_destroy(p,0);
}



static int f7(void *pool,struct{pthread_pool_task_base_t _base; int state; const int id;} * const args){
    if(!pool) return 0;
    switch(args->state){
        #define CASE(_n_) case _n_:\
            printf("statemachine[%d]: %d\n",args->id,args->state);\
            ++args->state;\
            pthread_pool_task_queue(pool,args,0);\
            return 1
        CASE(0);
        CASE(1);
        CASE(2);
        CASE(3);
        CASE(4);
        CASE(6);
        default:
            printf("statemachine[%d]: %d\n",args->id,args->state);
        #undef CASE
    }
    return 0;
}

static void test_statemachine(void){
    pthread_pool_t * const p=pthread_pool_create(2,0);
    int i=10;
    while(i--)
        pthread_pool_task(p,f7,0,i);
    pthread_pool_destroy(p,0);
}


int main(int argc, char **argv){

    test_prio();
    test_channel();
    test_reject();
    test_group();
    test_statemachine();
    benchmark();

    return 0;
}
