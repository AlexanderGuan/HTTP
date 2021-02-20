#ifndef CHANNEL_H
#define CHANNEL_H

#include "common.h"
#include "event_loop.h"
#include "buffer.h"

#define EVENT_TIMEOUT    0x01
/** 等待一个套接字变为可读*/
#define EVENT_READ        0x02
/** 等待一个套接字变为可写*/
#define EVENT_WRITE    0x04
/** Wait for a POSIX signal to be raised*/
#define EVENT_SIGNAL    0x08


typedef int (*event_read_callback)(void *data);

typedef int (*event_write_callback)(void *data);

struct channel {
    int fd;
    int events;   //表示event类型

    event_read_callback eventReadCallback;//读回调函数
    event_write_callback eventWriteCallback;//写回调函数
    void *data; //callback data, 可能是event_loop，也可能是tcp_server或者tcp_connection
};


struct channel *
channel_new(int fd, int events, event_read_callback eventReadCallback, event_write_callback eventWriteCallback,
            void *data);

int channel_write_event_is_enabled(struct channel *channel);

int channel_write_event_enable(struct channel *channel);

int channel_write_event_disable(struct channel *channel);


#endif