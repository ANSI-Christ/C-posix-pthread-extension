#include <time.h>
#include <stdio.h>
#include <stddef.h>


#define PTHREAD_EXT_IMPL
#include "pthread_ext.h"

void sleepf(double sec){
    struct timespec t={(time_t)sec,0}; t.tv_nsec=(sec-t.tv_sec)*1000000000;
    while(nanosleep(&t,&t));
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




struct f1_task{pthread_pool_task_t base; int cnt;};
static void f1(pthread_pool_t * const pool,struct f1_task * const args){
    if(pool){
        if(args->cnt){
            struct f1_task *t=malloc(sizeof(*t));
            t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f1;
            t->cnt=args->cnt-1;
            pthread_pool_task(pool,t,0);
        }
    }
    free(args);
}

void benchmark(void){
    struct timespec ts[2];
    pthread_pool_t *p;
    const unsigned int cores=12;
    unsigned int i, c;

    pthread_pool_create(&p,NULL,cores,0);
    pthread_pool_batch(p,4);

    for(c=0;c<10;++c){
        i=cores*4;
        timespec_current(ts+0);
        while(i--){
            struct f1_task *t=malloc(sizeof(*t));
            t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f1;
            t->cnt=10000;
            pthread_pool_task(p,t,0);
        }
        pthread_pool_wait(p);
        timespec_current(ts+1);
        printf("rt: %f ms\n",(ts[1].tv_sec-ts[0].tv_sec)*1000. + (ts[1].tv_nsec-ts[0].tv_nsec)/1000000.);
    }

    pthread_pool_destroy(p,1);
}




struct f2_task{pthread_pool_task_t base;float fp; char prio;};

static void f2(void *pool,struct f2_task *args){
    if(pool) printf("prio[%d]: %f\n",args->prio,args->fp);
}

void test_prio(void){
    struct f2_task t[10];
    pthread_pool_t *p;
    int i;
    pthread_pool_create(&p,NULL,1,4);
    for(i=0;i<5;++i){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f2;
        t[i].fp=i+0.3;
        t[i].prio=i;
        pthread_pool_task(p,t+i,i);
    }
    pthread_pool_destroy(p,0);
}




void test_channel(void){
    struct{int a; double b;} a={1,2.2}, b;
    pthread_channel_t c;

    pthread_channel_open(&c);
    pthread_channel_push(&c,&a,sizeof(a));
    pthread_channel_pop(&c,&b,sizeof(b));
    pthread_channel_close(&c);

    printf("channel: %d, %f\n",b.a,b.b);
}




struct f3_task{pthread_pool_task_t base;int id;};

static void f3(void *pool,struct f3_task *args){
    if(pool){sleepf(0.02);printf("task %d done\n",args->id);}
    else printf("task %d rejected\n",args->id);
}

void test_reject(void){
    struct f3_task t[5];
    pthread_pool_t *p;
    int i;
    pthread_pool_create(&p,NULL,1,0);

    for(i=0;i<5;++i){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f3;
        t[i].id=i;
        pthread_pool_task(p,t+i,0);
    }
    sleepf(0.05);
    pthread_pool_reject(p); // nonblocking rejection of queue
    pthread_pool_wait(p);

    for(i=0;i<5;++i){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f3;
        t[i].id=i+5;
        pthread_pool_task(p,t+i,0);
    }
    sleepf(0.05);
    pthread_pool_clear(p); // blocking clearing of pool

    for(i=0;i<5;++i){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f3;
        t[i].id=i+10;
        pthread_pool_task(p,t+i,0);
    }
    sleepf(0.05);
    pthread_pool_destroy(p,1); // destroing pool with rejecting tasks by now param
}





struct f4_task{pthread_pool_task_t base; pthread_group_t *g; unsigned  int n;};

static void f4(void *p,struct f4_task * const args){
    const int rejected=pthread_group_rejected(args->g);

    if(rejected==ENOENT){
        free(args->g);
        printf("group destroy at place 1\n\n");
        return;
    }else if(rejected==EINTR)
        return;

    if(!p){
        if(pthread_group_reject(args->g,1)==ENOENT){free(args->g); printf("group destroy at place 2\n\n");}
        return;
    }

    if(args->n==5){
        if(pthread_group_reject(args->g,1)==ENOENT){free(args->g); printf("group destroy at place 3\n\n");}
    }else{
        printf("task begin %u\n",args->n);
        sleepf(0.1);
        printf("task end %u\n\n",args->n);
        if(pthread_group_progress(args->g,0)==ENOENT){free(args->g); printf("group destroy at place 4\n\n");}
    }
}

static void test_group(void){
    struct f4_task t[10];
    pthread_pool_t *p;
    pthread_group_t *g=malloc(sizeof(*g));
//    pthread_group_t g[1];
    struct timespec ts[1];
    unsigned int i, done, all;

    pthread_pool_create(&p,NULL,1,0);
    pthread_group_init(g,NULL,4);

    for(i=0;i<10;++i){
        t[i].base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f4;
        t[i].g=g;
        t[i].n=i;
    }

    for(i=0;i<4;++i) pthread_pool_task(p,t+i,0);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,7);
    for(i=0;i<7;++i) pthread_pool_task(p,t+i,0);
    i=pthread_group_wait(g,&done,&all);
    printf("group wait = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,t+i,0);
    timespec_future(ts,0,300*1000*1000);
    i=pthread_group_timedwait(g,&done,&all,ts);
    printf("group timedwait = %d [%u / %u]\n\n",i,done,all);
    pthread_group_reject(g,0);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,t+i,0);
    sleepf(0.15);
    pthread_pool_reject(p);
    i=pthread_group_wait(g,&done,&all);
    printf("group reject & wait = %d [%u / %u]\n\n",i,done,all);


    pthread_group_progress(g,4);
    for(i=0;i<4;++i) pthread_pool_task(p,t+i,0);
    timespec_future(ts,0,250*1000*1000);
    i=pthread_group_timedwait(g,&done,&all,ts);

    if(sizeof(g)!=sizeof(void*)){ //if group placed on stack
        pthread_group_reject(g,0); // blocking rejection
        if(pthread_group_destroy(g)==ENOENT)
            printf("group destroy at place 0\n\n");
        else printf("cant get here\n");
    }else if(pthread_group_destroy(g)==ENOENT){
        free(g);
        printf("group destroy at place 0\n\n");
    }

    pthread_pool_destroy(p,0);
}




struct f5_task{pthread_pool_task_t base; int state; int id; int release;};

static void f5(pthread_pool_t *pool,struct f5_task * const args,unsigned int index){
    if(pool){
        switch(args->state){
            #define CASE(_n_) case _n_:\
                printf("{%u} statemachine[%d]: %d\n",index,args->id,args->state);\
                ++args->state;\
                pthread_pool_task(pool,args,0);\
                return
            CASE(0);
            CASE(1);
            CASE(2);
            CASE(3);
            CASE(4);
            CASE(5);
            CASE(6);
            #undef CASE
        }
    }
    printf("{%u} statemachine end[%d]: %d\n",index,args->id,args->state);
    if(args->release) free(args);
}

static void test_statemachine(void){
    pthread_pool_t *p;
    struct f5_task stack_tasks[5], *t;
    int i=10;

    pthread_pool_create(&p,NULL,4,0);
    while(i--){
        const int stack=(i<(sizeof(stack_tasks)/sizeof(*stack_tasks)));
        if(stack) t=stack_tasks+i;
        else t=malloc(sizeof(*t));
        if(t){
            t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f5;
            t->state=0;
            t->id=i;
            t->release=!stack;
            pthread_pool_task(p,t,0);
        }
    }
    pthread_pool_wait(p);
    pthread_pool_destroy(p,0);
}




struct f6_task{pthread_pool_task_t base; pthread_barrier_t barrier; pthread_pool_t *pool; int count; unsigned char batch;};

static void f6(void * const pool,struct f6_task * const args,unsigned int index){
    printf("thread %u paused\n",index);
    if(--args->count) pthread_pool_urgent(args->pool,(args));
    else pthread_pool_batch(args->pool,args->batch);
    pthread_barrier_wait(&args->barrier);
    printf("thread %u continue\n",index);
}

struct f7_task{pthread_pool_task_t base;int id;};

static void f7(void * const pool,struct f7_task * const args,unsigned int index){
    if(pool){
        sleepf(0.0001); printf("task %u\n",args->id);
    }
    free(args);
}

static void test_urgent(void){
    pthread_pool_t *p;
    unsigned int i=8;
    pthread_pool_create(&p,NULL,4,0);
    while(i--){
        struct f7_task *t=malloc(sizeof(*t));
        t->base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f7;
        t->id=i;
        pthread_pool_task(p,t,0);
    }
    sleepf(0.0003);

    {
        struct f6_task t;
        t.base.task=(void(*)(pthread_pool_t*,void*,unsigned int))f6;
        t.pool=p;
        t.count=pthread_pool_count(p);
        t.batch=pthread_pool_batch(p,1);
        pthread_barrier_init(&t.barrier,NULL,t.count+1);

        pthread_pool_urgent(p,&t);

        puts("thread main paused on getchar");
        getchar(); pthread_barrier_wait(&t.barrier);
        puts("thread main continue");

        pthread_barrier_destroy(&t.barrier);
    }

    pthread_pool_wait(p);
    pthread_pool_destroy(p,1);
}




int main(int argc, char **argv){
#define TEST(_f_) do{puts("------" #_f_ "------\n"); _f_; puts("\n\n");}while(0)
    TEST(benchmark());
    TEST(test_prio());
    TEST(test_channel());
    TEST(test_reject());
    TEST(test_group());
    TEST(test_statemachine());
    TEST(test_urgent());
    return 0;
}
