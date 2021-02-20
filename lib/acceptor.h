#ifndef ACCEPTOR_H
#define ACCEPTOR_H

#include "common.h"

struct acceptor{
    int listen_port;// 端口
    int listen_fd;// 套接字
} ;

/*初始化acceptor*/
struct acceptor *acceptor_init(int port);


#endif