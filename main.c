#include <time.h>
#include <stdio.h>
#include <stddef.h>


#define PTHREAD_EXT_IMPL
#include "pthread_ext.h"


#define RUNTIME(...) ({\
    const clock_t _rts=clock();\
    __VA_ARGS__;\
    ((size_t)(clock()-_rts))/(double)CLOCKS_PER_SEC;\
})




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
    pthread_pool_destroy(p,1,0);
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
    pthread_pool_destroy(p,0,0);
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

    pthread_pool_destroy(p,0,0);
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
    pthread_pool_destroy(p,1,0); // pthread_pool_clear(p);

    pthread_channel_pop(&c,&value,sizeof(value));
    if(value) printf("task f6 was done\n");
    else printf("task f6 was rejected\n");

    pthread_channel_close(&c);
}


int main(int argc, char **argv){

    test_prio();
    test_channel();
    test_reject();
    benchmark();

    return 0;
}
