#ifndef EVENT_LOOP_H
#define EVENT_LOOP_H

#include <pthread.h>
#include "channel.h"
#include "event_dispatcher.h"
#include "common.h"

extern const struct event_dispatcher poll_dispatcher;
extern const struct event_dispatcher epoll_dispatcher;

struct channel_element {
    int type; //1: add  2: delete
    struct channel *channel;
    struct channel_element *next;
};

struct event_loop {
    int quit;

    /* 类似poll或epoll, 可以让线程挂起，等待事件发生 */
    const struct event_dispatcher *eventDispatcher;

    /* 对应的event_dispatcher的数据. */
    // 注：声明为void*，poll和epoll的数据对象都能放入
    void *event_dispatcher_data;
    struct channel_map *channelMap;

    /*标志位，event_loop_handle_pending_channel()函数处理链表时设为1*/
    int is_handle_pending;
    /*子线程内需要处理的新事件,组织成channel_element链表*/
    struct channel_element *pending_head;
    struct channel_element *pending_tail;

    /*每个event_loop的线程id*/
    pthread_t owner_thread_id;

    /*线程同步用*/
    pthread_mutex_t mutex;
    pthread_cond_t cond;

    /*线程用来通知子线程有新的数据要处理*/
    int socketPair[2];
    /*线程名*/
    char *thread_name;
};

struct event_loop *event_loop_init();

struct event_loop *event_loop_init_with_name(char *thread_name);

int event_loop_run(struct event_loop *eventLoop);

void event_loop_wakeup(struct event_loop *eventLoop);

int event_loop_add_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1);

int event_loop_remove_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1);

int event_loop_update_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1);

int event_loop_handle_pending_add(struct event_loop *eventLoop, int fd, struct channel *channel);

int event_loop_handle_pending_remove(struct event_loop *eventLoop, int fd, struct channel *channel);

int event_loop_handle_pending_update(struct event_loop *eventLoop, int fd, struct channel *channel);

// dispather派发完事件之后，调用该方法通知event_loop执行对应事件的相关callback方法
// res: EVENT_READ | EVENT_READ等
int channel_event_activate(struct event_loop *eventLoop, int fd, int res);

#endif