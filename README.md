# mymuduo
解耦合boost库的mymuduo
1.根据autobuild.sh，可以自动实现编译，创建build目录，编译的结果在lib目录

# 介绍：
重现muduo库核心组件，解耦boost库，用c++11重构，主要的组件如下：
1.Channel组件：
 Channel理解为通道，封装了socket和其感兴趣的event,如EPOLLIN，EPOLLOUT等事件，还绑定了poller返回的具体事件
 简要介绍在muduo当中是利用Channel把fd封装注册到loop的poller当中，
 muduo当中就有个两种Channel的fd一种监听的fd，一种是连接的connfd，通过Channel封装成Channelfd的形式注册到loop的poller上去。
 2.Poller和EPollPoller
 封装了epoll，网络事件的IO检测，检测有对应的事件之后会向上层报告。相当于事件分发器的功能
 3.EventLOOP
 事件循环类，主要包括两大模块Channel、Poller(epoll的抽象)
 Channel和Poller是相互分开没有关联的，通过EventLoop把两者进行了粘合，
 (1)ChannelList activeChannels_;当中包含了所有的channel，
 (2)std::unique_ptr<Poller> poller_; 对应的poller
 (3)int wakeupFd_; std::unique_ptr<Channel> wakeupChannel_;
主要的作用，当mainloop获取一个新用户的channel，
通过轮询算法选择一个subloop，通过该成员唤醒subloop处理channel
(4) std::vector<Functor> pendingFunctors_;存储loop需要执行的所有回调操作

4.Thread和EventLoopThread、EventLoopThreadPool
muduo支持多线程操作，可以设置多个Loop线程，muduo是典型的one loop per thread的reactor网络模型
如果设置了多个线程，可以通过getNextLOOP()通过轮询算法获取下一个subloop，如果没有设置就一个Mainloop
一个线程对应一个loop。
5.Acceptor
主要封装了socket，bind，linstenfd,设置socket的参数，设置linsten的回调函数，有新用户连接
执行TCP::newconnetion,把linsentfd打包成acceptChannelfd最后注册到poller上
6.Buffer
数据首发的缓冲区，引用层写数据->缓冲区->tcp协议栈的缓冲区->真正发送给对端。
7.TcpConnection
TcpConnection设置回调函数，最终设置到Channel里边，mainloop通过轮询算法选择一个subloop，
通过该成员唤醒subloop处理channel。
8，TcpServer
统领全局的，设置loop，进行server.start()相当于listen  loopthread  listenfd => acceptChannel => mainLoop =>
loop.loop(); // 启动mainLoop的底层Poller

