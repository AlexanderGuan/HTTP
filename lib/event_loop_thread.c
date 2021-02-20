#include <assert.h>
#include "event_loop_thread.h"
#include "event_loop.h"

void *event_loop_thread_run(void *arg) {
    struct event_loop_thread *eventLoopThread = (struct event_loop_thread *) arg;

    pthread_mutex_lock(&eventLoopThread->mutex);

    // 初始化化event loop，之后通知主线程
    eventLoopThread->eventLoop = event_loop_init_with_name(eventLoopThread->thread_name);
    yolanda_msgx("event loop thread init and signal, %s", eventLoopThread->thread_name);
    /* 激活一个等待该条件的线程 */
    pthread_cond_signal(&eventLoopThread->cond);

    pthread_mutex_unlock(&eventLoopThread->mutex);

    //子线程event loop run,进入无限循环的事件分发执行体中，等待子线程reactor上注册过的事情发生
    event_loop_run(eventLoopThread->eventLoop);
}

//初始化已经分配内存的event_loop_thread
int event_loop_thread_init(struct event_loop_thread *eventLoopThread, int i) {
    pthread_mutex_init(&eventLoopThread->mutex, NULL);
    pthread_cond_init(&eventLoopThread->cond, NULL);
    eventLoopThread->eventLoop = NULL;
    eventLoopThread->thread_count = 0;
    eventLoopThread->thread_tid = 0;

    char *buf = malloc(16);
    sprintf(buf, "Thread-%d\0", i + 1);
    eventLoopThread->thread_name = buf;

    return 0;
}


//由主线程调用，初始化一个子线程，并且让子线程开始运行event_loop
struct event_loop *event_loop_thread_start(struct event_loop_thread *eventLoopThread) {
    /*创建子线程，边调用event_loop_thread_run()函数*/
    pthread_create(&eventLoopThread->thread_tid, NULL, &event_loop_thread_run, eventLoopThread);

    /*互斥锁：加锁*/
    assert(pthread_mutex_lock(&eventLoopThread->mutex) == 0);

    /*守候 eventLoopThread 中的 eventLoop 的变量,等待event_loop初始化完成后的signal通知,收到通知激活线程*/
    /*激活后，主线程从wait中苏醒，代码继续执行;子线程也通过event_loop_run进入了一个无限循环的事件分发器中*/
    while (eventLoopThread->eventLoop == NULL) {
        assert(pthread_cond_wait(&eventLoopThread->cond, &eventLoopThread->mutex) == 0);
    }

    /*互斥锁：解锁*/
    assert(pthread_mutex_unlock(&eventLoopThread->mutex) == 0);

    yolanda_msgx("event loop thread started, %s", eventLoopThread->thread_name);
    return eventLoopThread->eventLoop;
}