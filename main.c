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





static void repost(struct{int cnt;} *args,int index,void *pool){
    if(args->cnt) pthread_pool_task(pool,repost,args->cnt-1);
}

static void benchmark(void){
    const unsigned int cores=pthread_cores();
    pthread_pool_t p=pthread_pool_create(cores,0,0);
    printf("rt: %f\n",RUNTIME(
        unsigned int i=cores*4;
        while(i--)
            pthread_pool_task(p,repost,100000);
        pthread_pool_wait(p);
    ));
    pthread_pool_destroy(&p);
}




static void f2(struct{int prio; double fp; const char *str;} *args){
    printf("prio[%d]: %f %s\n",args->prio,args->fp,args->str);
}

static void test_prio(void){
    pthread_pool_t p=pthread_pool_create(1,8,0);
    pthread_pool_task(p, f2, 0, 0.1, (const char*)"aaa" ); /* prio 0 */
    pthread_pool_task(p, f2, 0, 0.2, (const char*)"bbb" ); /* prio 0 */
    pthread_pool_task_prio(p,1, f2, 1, 1., (const char*)"ccc" ); /* prio 1 */
    pthread_pool_task_prio(p,2, f2, 2, 2., (const char*)"ddd" ); /* prio 2 */
    pthread_pool_task_prio(p,3, f2, 3, 3., (const char*)"eee" ); /* prio 3 */
    pthread_pool_task_prio(p,8, f2, 8, 8., (const char*)"fff" ); /* prio 4 */
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


static void f4(struct{pthread_channel_t *c} *args){
    sleep(1);
    if(args->c){
        const struct{int a; double b;}value={1,2.2};
        pthread_channel_push(args->c,&value,sizeof(value));
    }
}

static void test_channel(void){
    struct{int a; double b;}value;
    pthread_pool_t p=pthread_pool_create(4,0,0);
    pthread_channel_t c=pthread_channel_init();
    int i=10;

    while(i--) pthread_pool_task(p,f4,NULL);
    pthread_pool_task(p,f4,&c);

    pthread_channel_pop(&c,&value,sizeof(value));
    printf("%d, %f\n",value.a,value.b);

    pthread_pool_destroy_later(&p);
    pthread_channel_close(&c);
}



int main(int argc, char **argv){
    test_prio();
    test_channel();
    benchmark();
//    test_pause_resume();
    return 0;
}
