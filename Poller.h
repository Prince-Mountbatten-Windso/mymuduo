#pragma once
#include "noncopyable.h"
#include "EventLoop.h"
#include "Timestamp.h"
#include "Channel.h"
#include <vector>
#include <unordered_map>

/**
*
*muduo库中多路事件分发器的核心IO复用模块
**/
class Poller:noncopyable
{
public:
    using ChannelList = std::vector<Channel *>;
    Poller(EventLoop *loop);
    virtual ~Poller() = default;

    //给所有IO复用保留统一的接口//epool_wait
    virtual Timestamp poll(int timeouts,ChannelList *activeChannles) = 0;
    virtual void updateChannel(Channel *channel) = 0;//epoll_ctr
    virtual void removeChannel(Channel *channel) = 0;//epoll_remove
    
    
    //判断参数channel是否在当前的poller当中
    bool hasChannel(Channel *channel) const;

    //EventLoop 可以通过接口获取默认的IO复用的具体实现
    static Poller* newDefaultPoller(EventLoop *loop);

protected:
    //map的key：sockfd value：sockfd所属的channel通道类型
    using ChannelMap = std::unordered_map<int,Channel*>;
    ChannelMap channels_;

private:
    EventLoop *ownerLoop_;//定义Poller所属的事件循环EventLoop
};


