#include <time.h>
#include <stdio.h>
#include <stddef.h>


#define PTHREAD_EXT_IMPL
#include "pthread_ext.h"

extern unsigned int sleep(unsigned int);

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



/*
The correct way is:

static void f1(void *pool,struct{int cnt;} *args,int index){
    if(pool && args->cnt) pthread_pool_task(pool,f1,args->cnt-1);
    return PTHREAD_TASK_AUTORELEASE;
}

But this library will not check the return code without custom extensions from user!
*/

static void f1(void *pool,struct{int cnt;} *args,int index){
    if(!pool) return;
    if(args->cnt) pthread_pool_task(pool,f1,args->cnt-1);
}

static void benchmark(void){
    const unsigned int cores=pthread_cores();
    pthread_pool_t *p=pthread_pool_create(cores,0);
    printf("rt: %f\n",RUNTIME(
        unsigned int i=cores*4;
        while(i--)
            pthread_pool_task(p,f1,100000);
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(p,1);
}




static void f2(void *pool,struct{int prio; double fp; const char *str;} *args){
    if(!pool) return;
    printf("prio[%d]: %f %s\n",args->prio,args->fp,args->str);
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




static void f4(void *pool,struct{pthread_channel_t *c;} *args){
    if(!pool) return;
    sleep(1);
    if(args->c){
        const struct{int a; double b;}value={1,2.2};
        pthread_channel_push(args->c,&value,sizeof(value));
    }
}

static void test_channel(void){
    struct{int a; double b;}value;
    pthread_pool_t *p=pthread_pool_create(4,0);
    pthread_channel_t c;
    pthread_channel_open(&c);
    int i=10;

    while(i--) pthread_pool_task(p,f4,NULL);
    pthread_pool_task(p,f4,&c);

    pthread_channel_pop(&c,&value,sizeof(value));
    printf("%d, %f\n",value.a,value.b);

    pthread_pool_destroy(p,0);
    pthread_channel_close(&c);
}


static void f5(void *pool){
    if(!pool){ printf("task f5 was rejected\n"); return;}
    sleep(2);
}

static void f6(void *pool,struct{pthread_channel_t *c;} *args){
    pthread_channel_push(args->c,&pool,sizeof(pool));
}

static void test_reject(void){
    void *value;
    pthread_channel_t c;
    pthread_pool_t *p=pthread_pool_create(1,0);

    pthread_channel_open(&c);

    pthread_pool_task(p,f5);
    pthread_pool_task(p,f5);
    pthread_pool_task(p,f5);
    pthread_pool_task(p,f5);
    pthread_pool_task(p,f5);
    pthread_pool_task(p,f6,&c);
    sleep(1);
    pthread_pool_destroy(p,1); // pthread_pool_clear(p);

    pthread_channel_pop(&c,&value,sizeof(value));
    if(value) printf("task f6 was done\n");
    else printf("task f6 was rejected\n");

    pthread_channel_close(&c);
}

#define VAR 3

static void f7(void *p,struct{pthread_group_t *g; unsigned  int n;} * const args){
    const int rejected=pthread_group_rejected(args->g);
    if(rejected){
        if(rejected==1) printf("group destroy at place 1\n\n");
        else if(rejected==-1){/* nothing to do */}
        return;
    }
    if(!p){
        if(pthread_group_reject(args->g,1)==1) printf("group destroy at place 2\n\n");
        return;
    }
    if(args->n==5){
        if(pthread_group_reject(args->g,1)==1) printf("group destroy at place 3\n\n");
    }else{
        printf("task begin %u\n",args->n);
        sleep(1);
        printf("task end %u\n\n",args->n);
        if(pthread_group_progress(args->g,0)==1) printf("group destroy at place 4\n\n");
    }
}

static void test_group(void){
    pthread_pool_t * const p=pthread_pool_create(1,0);
    pthread_group_t *g=malloc(sizeof(*g));
    struct timespec t[1];
    unsigned int i, done, all;
    pthread_pool_banch(p,0);

    pthread_group_init(g,4,NULL,free); // can be destroy inside task, in nonblocking mode cause group placed not in stack
    for(i=0;i<4;++i) pthread_pool_task(p,f7,(pthread_group_t*)g,i);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,7);
    for(i=0;i<7;++i) pthread_pool_task(p,f7,(pthread_group_t*)g,i);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f7,(pthread_group_t*)g,i);
    timespec_future(t,3,0);
    i=pthread_group_timedwait(g,&done,&all,t);
    printf("group timedwait end = %d [%u / %u]\n\n",i,done,all);
    pthread_group_reject(g,0);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f7,(pthread_group_t*)g,i);
    sleep(1);
    pthread_pool_unpending(p);
    i=pthread_group_wait(g,&done,&all);
    printf("group timedwait end = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,f7,(pthread_group_t*)g,i);
    timespec_future(t,3,0);
    i=pthread_group_timedwait(g,&done,&all,t);
    if(pthread_group_destroy(g)==1)
        printf("group destroy at place 0\n\n");

    pthread_pool_destroy(p,0);
}
#undef VAR


int main(int argc, char **argv){

    test_prio();
    test_channel();
    test_reject();
    test_group();
    benchmark();

    return 0;
}
