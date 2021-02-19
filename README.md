本项目是一个基于事件驱动的TCP并发服务器框架

>说明：本项目的学习资料为蚂蚁金服高级技术专家盛延敏博士所提出的基于epoll的rpc，作者在仔细研读源码后，梳理了技术路线，为代码逐句添加注释，并修改了源码中的部分bug，最后复现了本框架。本框架采用了非阻塞I/O + I/O多路复用 + Readiness notifications + 线程池 四者相结合的方法，代码简洁，便于让更多的开发者迅速的了解框架的内脏细节并且可以快速基于本框架设计一款因地制宜的模块。

开发者：
盛延敏(原作者)
关力铭(复现、修改及注释)

# 一、前言
## 1.1 为什么要做本框架  
目前服务器的框架有很多，但是适合本科生研究学习、适合作为向高阶网络编程技术发展的跳板的框架甚少，无论是百度的BRPC还是陈硕老师的muduo，都是功能众多的RPC，逻辑比较复杂，难以解耦，对于初级网络编程工作者来说学习难度大、成本高。本项目的目的是我们可以通过本框架来了解基于事件驱动编写一个TCP服务器的整体轮廓，让更多的网络编程爱好者能够深入浅出地认识这个领域。  
## 1.2 我所做的工作  
本项目在复现了源码的基础上，添加了设计本框架的逻辑思维导图与代码逐句注释，以帮助读者理解设计服务器框架的思路。
最后，希望读者能够提出宝贵的意见，感谢您的关注！

# 二、服务器运行试验及运行环境
机器环境: Ubuntu 18.04

# 三、需求分析
TCP高性能网络框架需要满足的需求有如下三点。  
- 第一，使用ractor模型，使用epoll作为事件分发实现。  
- 第二，需要支持多线程，从而可以支持单线程的但reactor模式，也可以支持多线程主-从readtor模式。可以将套接字上的I/O事件分离到多个线程上。  
- 第三，封装读写操作到Buffer对象中。  
根据以上三个需求，可以将整体思路划分为三部分，分别是反应堆模式设计、I/O模型和多线程模型设计、数据读写封装和buffer。

## 3.1 反应堆模式设计
反应堆模式，主要是设计一个基于事件分发和回调的反应堆框架。这个框架中的主要对象包括：
- **event_loop**  
event_loop和一个线程绑定，它是一个无限循环着的事件分发器，一旦有事件发生，它就会执行回调函数，完成事件的处理。具体来说，event_loop使用epoll方法将一个线程阻塞，直到某个I/O事件发生。

- **channel**  
我们将各种注册到event_loop上的对象抽象为channel来表示，例如注册到event_loop上的监听事件，注册到event_loop上的套接字读写事件等

- **acceptor**  
acceptor对象是服务器端监听器，acceptor对象最终会最为一个channel对象，注册到event_loop上，以便进行连接完成事件分发和检测。

- **event_dispatcher**  
event_dispatcher是对事件分发机制的抽象。

- **channel_map**  
channel_map保存了描述字到channel的映射，这样就可以在事件发生时，根据事件类型对应的套接字快速找到channel对象里的事件处理函数。

## 3.2 I/O模型和多线程模型设计
I/O线程和多线程模型，主要解决event_loop的线程运行问题，以及事件分发和回调的线程执行问题。

- **thread_pool**  
thread_pool维护了一个sub-reator的线程池，它对主reactor负责，每当有新的连接建立时，主readtor从thread_pool中获取一个线程，以便用它来完成对新连接套接字的read/write事件注册，将I/O线程和主reator线程分离.

- **event_loop_thread**  
event_loop_thread是reactor的线程实现，连接套接字的read/write事件检测便是在这个线程中完成的。

## 3.3 Buffer和数据读写
- **buffer**  
如果没有buffer对象，连接套接字的read/write事件都需要和字节流直接打交道。那么我们提供一个buffer对象，其屏蔽了对套接字进行读写的操作，用来表示从连接套接字收取的数据，以及应用程序即将需要发出的数据。

- **tcp_connection**  
tcp_connection对象描述已经建立的TCP连接。它的属性包括接收缓冲区、发送缓冲区、channel对象等。这些是TCP连接的天然属性。tcp_connection是上层应用程序和高性能框架直接打交道的数据结构,在设计框架时不应该把最下层的channel对象直接暴露给应用程序，因为channel对象不仅仅可以表示tcp_connection，而且监听套接字也是一个channel对象，用来唤醒线程的sockpair也是一个channel对象，因此设计了tcp_connection作为一个比较清晰的编程入口。

# 四、框架逻辑关系  
## 4.1 反应堆模式设计  
![event_loop运行详图](https://github.com/AlexanderGuan/HTTP/blob/main/%E5%8F%8D%E5%BA%94%E5%A0%86%E6%A8%A1%E5%BC%8F%E8%AE%BE%E8%AE%A1.JPG)  
当调用event_loop_run后，线程会进入循环，首先执行dispatch事件分发，然后阻塞在这里，一旦有事件发生，就会调用channel_event_activate函数，在这个函数中进行事件回调函数eventReadcallback和eventwritecallback的调用，最后在通过event_loop_handle_pending_channel修改当前监听的事件列表，完成这个部分后，又进入事件分发循环。  
## 4.2 多线程模式设计  
![线程运行关系-文字版](https://github.com/AlexanderGuan/HTTP/blob/main/%E7%BA%BF%E7%A8%8B%E8%BF%90%E8%A1%8C%E5%85%B3%E7%B3%BB-%E6%96%87%E5%AD%97%E7%89%88.JPG)  
![线程运行关系-函数版](https://github.com/AlexanderGuan/HTTP/blob/main/%E7%BA%BF%E7%A8%8B%E8%BF%90%E8%A1%8C%E5%85%B3%E7%B3%BB-%E5%87%BD%E6%95%B0%E7%89%88.JPG)  
在本框架中，main reactor线程是一个acceptor线程，这个线程一旦创建，会以event_loop形式阻塞在event_dispatcher的dispatch方法上，它在监听套接字上的事件发生，也就是已完成的连接，一旦有连接完成，就会创建出连接对象tcp_connection及channel对象等。  
当用户期望使用多个sub-reactor子线程时，主线程会创建多个子线程，每个子线程在创建之后会按照主线程指定的启动函数立即运行，并进行初始化。在多线程的情况下，需要将新创建的已连接套接字对应的读写事件交给一个sub-reactor线程处理，这时从线程池中取出一个线程，通知这个线程有新的事件加入。  
子线程是一个阻塞在dispatch上的event_loop线程，当有事件发生时，它会查找channel_map,找对对应的处理函数并执行它，之后它会增加、删除或修改pending事件，并进入下一轮的dispatch。  
## 4.3 buffer对象  
buffer是一个缓冲区对象，缓存了从套接字中接收的数据及待发往套接字的数据。  
- **从套接字中接收数据时**  
事件回调函数不断地向buffer对象增加数据，同时应用程序需要不断地把buffer中的数据处理掉，这样，buffer对象才可以空出新的位置容纳更多数据。  
- **向套接字发送数据时**  
应用程序不断向buffer对象增加数据，同时事件回调函数不断调用套接字上的发送函数将数据发送出去，减少buffer中的数据。  


