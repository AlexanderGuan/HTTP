#include <assert.h>
#include "acceptor.h"

/**
 * 作用：初始化acceptor
 * 返回：acceptor实例
 * 注：
 * 1.acceptor对象表示服务端监听器
 * 2.acceptor对象最终会作为一个channel对象，注册到event_loop上，以便进行连接完成的事件分发和检测*/

struct acceptor *acceptor_init(int port) {
    /*分配内存及初始化*/
    struct acceptor *acceptor1 = malloc(sizeof(struct acceptor));
    acceptor1->listen_port = port;
    acceptor1->listen_fd = socket(AF_INET, SOCK_STREAM, 0);

    /*设置套接字为非阻塞*/
    make_nonblocking(acceptor1->listen_fd);

    /*初始化套接字*/
    struct sockaddr_in server_addr;
    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(port);

    /*允许重用本地地址和端口*/
    int on = 1;
    setsockopt(acceptor1->listen_fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));


    /*绑定套接字*/
    int rt1 = bind(acceptor1->listen_fd, (struct sockaddr *) &server_addr, sizeof(server_addr));
    if (rt1 < 0) {
        error(1, errno, "bind failed ");
    }

    /*开启监听*/
    int rt2 = listen(acceptor1->listen_fd, LISTENQ);
    if (rt2 < 0) {
        error(1, errno, "listen failed ");
    }

//    signal(SIGPIPE, SIG_IGN);

    return acceptor1;
}