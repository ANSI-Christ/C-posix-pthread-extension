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
    pthread_pool_task(p, f2, 1, 1.1, (const char*)"str_1" );
    pthread_pool_task(p, f2, 2, 2.2, (const char*)"str_2" );
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
