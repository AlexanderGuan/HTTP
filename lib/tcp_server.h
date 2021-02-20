#ifndef TCP_SERVER_H
#define TCP_SERVER_H

typedef int (*connection_completed_call_back)(struct tcp_connection *tcpConnection);

typedef int (*message_call_back)(struct buffer *buffer, struct tcp_connection *tcpConnection);

typedef int (*write_completed_call_back)(struct tcp_connection *tcpConnection);

typedef int (*connection_closed_call_back)(struct tcp_connection *tcpConnection);

#include "acceptor.h"
#include "event_loop.h"
#include "thread_pool.h"
#include "buffer.h"
#include "tcp_connection.h"

struct TCPserver {
    int port;
    struct event_loop *eventLoop;
    struct acceptor *acceptor;
    connection_completed_call_back connectionCompletedCallBack; // 连接建立之后的callback
    message_call_back messageCallBack;                          // 数据读到buffer之后的callback
    write_completed_call_back writeCompletedCallBack;           // 数据通过buffer写完之后的callback
    connection_closed_call_back connectionClosedCallBack;       // 连接关闭之后的callback
    int threadNum;                                              // 线程数量
    struct thread_pool *threadPool;                             // 线程池
    void * data;                                                // 回调数据, for callback use: http_server
};


//准备监听套接字
struct TCPserver *
tcp_server_init(struct event_loop *eventLoop, struct acceptor *acceptor,
                connection_completed_call_back connectionCallBack,
                message_call_back messageCallBack,
                write_completed_call_back writeCompletedCallBack,
                connection_closed_call_back connectionClosedCallBack,
                int threadNum);

//开启监听
void tcp_server_start(struct TCPserver *tcpServer);

//设置callback数据
void tcp_server_set_data(struct TCPserver *tcpServer, void * data);

#endif
