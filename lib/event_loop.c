#include <assert.h>
#include "event_loop.h"
#include "common.h"
#include "log.h"
#include "event_dispatcher.h"
#include "channel.h"
#include "utils.h"

// in the i/o thread
/** 更新channel对象列表*/
int event_loop_handle_pending_channel(struct event_loop *eventLoop) {
    /*线程加锁*/
    pthread_mutex_lock(&eventLoop->mutex);
    /*更新处理channel对象列表的标记*/
    eventLoop->is_handle_pending = 1;

    struct channel_element *channelElement = eventLoop->pending_head;
    /*从链表头部到尾部进行遍历channel对象和与之对应的fd*/
    while (channelElement != NULL) {
        //save into event_map
        struct channel *channel = channelElement->channel;
        int fd = channel->fd;
        if (channelElement->type == 1) {
            event_loop_handle_pending_add(eventLoop, fd, channel);
        } else if (channelElement->type == 2) {
            event_loop_handle_pending_remove(eventLoop, fd, channel);
        } else if (channelElement->type == 3) {
            event_loop_handle_pending_update(eventLoop, fd, channel);
        }
        channelElement = channelElement->next;
    }
    /*更新链表*/
    eventLoop->pending_head = eventLoop->pending_tail = NULL;
    /*将标记复位*/
    eventLoop->is_handle_pending = 0;

    //释放互斥锁资源
    pthread_mutex_unlock(&eventLoop->mutex);

    return 0;
}

/** 向子线程增加需要处理的channel event对象*/
void event_loop_channel_buffer_nolock(struct event_loop *eventLoop, int fd, struct channel *channel1, int type) {
    //add channel into the pending list
    struct channel_element *channelElement = malloc(sizeof(struct channel_element));
    channelElement->channel = channel1;
    channelElement->type = type;
    channelElement->next = NULL;
    //第一个元素
    if (eventLoop->pending_head == NULL) {
        eventLoop->pending_head = eventLoop->pending_tail = channelElement;
    } else {
        eventLoop->pending_tail->next = channelElement;
        eventLoop->pending_tail = channelElement;
    }
}

/** 处理channel event事件列表*/
int event_loop_do_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1, int type) {
    /*互斥锁上锁*/
    pthread_mutex_lock(&eventLoop->mutex);
    assert(eventLoop->is_handle_pending == 0);
    /*向子线程的数据中增加需要处理的channel event对象*/
    // 所有增加的 channel 对象以列表的形式维护在子线程的数据结构中
    event_loop_channel_buffer_nolock(eventLoop, fd, channel1, type);
    //release the lock
    pthread_mutex_unlock(&eventLoop->mutex);

    /*如果增加channel event的不是当前event loop自己，就调用event_loop_wake函数把event_loop子线程唤醒*/
    // 唤醒方法：往socketPair[0]上写一个字节
    if (!isInSameThread(eventLoop)) {
        event_loop_wakeup(eventLoop);
    } else {
        /*如果增加channel event的是当前event loop自己，就调用event_loop_handle_pending_channel处理新增加的channel event事件列表*/
        event_loop_handle_pending_channel(eventLoop);
    }

    return 0;

}

/** 新增channel event,入口函数*/
int event_loop_add_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1) {
    return event_loop_do_channel_event(eventLoop, fd, channel1, 1);
}

/** 删除channel event,入口函数*/
int event_loop_remove_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1) {
    return event_loop_do_channel_event(eventLoop, fd, channel1, 2);
}

/** 更新channel event,入口函数*/
int event_loop_update_channel_event(struct event_loop *eventLoop, int fd, struct channel *channel1) {
    return event_loop_do_channel_event(eventLoop, fd, channel1, 3);
}

// in the i/o thread
int event_loop_handle_pending_add(struct event_loop *eventLoop, int fd, struct channel *channel) {
    yolanda_msgx("add channel fd == %d, %s", fd, eventLoop->thread_name);
    struct channel_map *map = eventLoop->channelMap;

    if (fd < 0)
        return 0;

    if (fd >= map->nentries) {
        if (map_make_space(map, fd, sizeof(struct channel *)) == -1)
            return (-1);
    }

    //第一次创建，增加
    if ((map)->entries[fd] == NULL) {
        map->entries[fd] = channel;
        //add channel
        struct event_dispatcher *eventDispatcher = eventLoop->eventDispatcher;
        eventDispatcher->add(eventLoop, channel);
        return 1;
    }

    return 0;
}

// in the i/o thread
int event_loop_handle_pending_remove(struct event_loop *eventLoop, int fd, struct channel *channel1) {
    struct channel_map *map = eventLoop->channelMap;
    assert(fd == channel1->fd);

    if (fd < 0)
        return 0;

    if (fd >= map->nentries)
        return (-1);

    struct channel *channel2 = map->entries[fd];

    //update dispatcher(multi-thread)here
    struct event_dispatcher *eventDispatcher = eventLoop->eventDispatcher;

    int retval = 0;
    if (eventDispatcher->del(eventLoop, channel2) == -1) {
        retval = -1;
    } else {
        retval = 1;
    }

    map->entries[fd] = NULL;
    return retval;
}

// in the i/o thread
int event_loop_handle_pending_update(struct event_loop *eventLoop, int fd, struct channel *channel) {
    yolanda_msgx("update channel fd == %d, %s", fd, eventLoop->thread_name);
    struct channel_map *map = eventLoop->channelMap;

    if (fd < 0)
        return 0;

    if ((map)->entries[fd] == NULL) {
        return (-1);
    }

    //update channel
    struct event_dispatcher *eventDispatcher = eventLoop->eventDispatcher;
    eventDispatcher->update(eventLoop, channel);
}

/** 当EventDispatcher检测到新的事件时，就会调用channel_event_activate函数触发和channel对象绑定的回调函数*/
int channel_event_activate(struct event_loop *eventLoop, int fd, int revents) {
    struct channel_map *map = eventLoop->channelMap;

    /*日志函数*/
    yolanda_msgx("activate channel fd == %d, revents=%d, %s", fd, revents, eventLoop->thread_name);

    if (fd < 0)
        return 0;

    if (fd >= map->nentries)return (-1);

    /*根据fd从ChannelMap中选出对应的Channel对象出来*/
    struct channel *channel = map->entries[fd];
    /*保证激活的channel中fd和传递进来的fd是同一个*/
    assert(fd == channel->fd);

    if (revents & (EVENT_READ)) {
        /*一个channel对象就有个readCallback函数与之对应*/
        if (channel->eventReadCallback) channel->eventReadCallback(channel->data);
    }
    if (revents & (EVENT_WRITE)) {
        /*一个channel对象有一个writeCallback与之对应*/
        if (channel->eventWriteCallback) channel->eventWriteCallback(channel->data);
    }

    return 0;

}

void event_loop_wakeup(struct event_loop *eventLoop) {
    char one = 'a';
    ssize_t n = write(eventLoop->socketPair[0], &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERR("wakeup event loop thread failed");
    }
}

/** 从socketPair[1]读取一个字符*/
int handleWakeup(void *data) {
    struct event_loop *eventLoop = (struct event_loop *) data;
    char one;
    ssize_t n = read(eventLoop->socketPair[1], &one, sizeof one);
    if (n != sizeof one) {
        LOG_ERR("handleWakeup  failed");
    }
    yolanda_msgx("wakeup, %s", eventLoop->thread_name);
}

/** 线程初始化函数入口*/
struct event_loop *event_loop_init() {
    return event_loop_init_with_name(NULL);
}

/** 线程初始化函数*/
struct event_loop *event_loop_init_with_name(char *thread_name) {
    struct event_loop *eventLoop = malloc(sizeof(struct event_loop));

    /*线程锁:加锁*/
    pthread_mutex_init(&eventLoop->mutex, NULL);
    /*初始化条件变量*/
    pthread_cond_init(&eventLoop->cond, NULL);

    /*线程命名*/
    if (thread_name != NULL) {
        eventLoop->thread_name = thread_name;
    } else {
        eventLoop->thread_name = "main thread";
    }

    eventLoop->quit = 0;
    eventLoop->channelMap = malloc(sizeof(struct channel_map));
    /*初始化channel*/
    map_init(eventLoop->channelMap);

#ifdef EPOLL_ENABLE
    /*如epoll可用，则使用epoll作为事件分发器*/
    yolanda_msgx("set epoll as dispatcher, %s", eventLoop->thread_name);
    eventLoop->eventDispatcher = &epoll_dispatcher;
#else
    /*如epoll不可用，则使用poll*/
    yolanda_msgx("set poll as dispatcher, %s", eventLoop->thread_name);
    eventLoop->eventDispatcher = &poll_dispatcher;
#endif
    /*初始化该线程的epoll(或poll)*/
    eventLoop->event_dispatcher_data = eventLoop->eventDispatcher->init(eventLoop);

    /*将socketfd加入到epoll的event中*/
    eventLoop->owner_thread_id = pthread_self();
    /*建立一对无名套接字*/
    // socketPair[0] socketPair[1] 既可读也可写
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, eventLoop->socketPair) < 0) {
        LOG_ERR("socketpair set fialed");
    }
    eventLoop->is_handle_pending = 0;
    eventLoop->pending_head = NULL;
    eventLoop->pending_tail = NULL;

    /*让字线程从dispatch的阻塞中苏醒*/
    /*如果有READ事件，就调用handlewakeup函数完成事件处理*/
    struct channel *channel = channel_new(eventLoop->socketPair[1], EVENT_READ, handleWakeup, NULL, eventLoop);
    event_loop_add_channel_event(eventLoop, eventLoop->socketPair[1], channel);

    return eventLoop;
}

/**
 *
 * 1.参数验证
 * 2.调用dispatcher来进行事件分发,分发完回调事件处理函数
 */
int event_loop_run(struct event_loop *eventLoop) {
    assert(eventLoop != NULL);

    struct event_dispatcher *dispatcher = eventLoop->eventDispatcher;

    /*安全性校验，防止当前EventLoop对象中的线程ID和EventLoop绑定的线程ID不一致*/
    if (eventLoop->owner_thread_id != pthread_self()) {
        exit(1);
    }

    yolanda_msgx("event loop run, %s", eventLoop->thread_name);

    /*每1秒进行一次事件监测*/
    struct timeval timeval;
    timeval.tv_sec = 1;

    while (!eventLoop->quit) {
        /*阻塞在这里等待IO事件，并将eventLoop指针对象丢给dispatcher事件分发器,得到激活的channels*/
        dispatcher->dispatch(eventLoop, &timeval);

        /*处理pending channel更新事件列表，如果子线程被唤醒，这个部分也会立刻执行到*/
        event_loop_handle_pending_channel(eventLoop);
    }

    yolanda_msgx("event loop end, %s", eventLoop->thread_name);
    return 0;
}


