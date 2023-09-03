#include "Channel.h"
#include "EventLoop.h"
#include "Logger.h"
#include <sys/epoll.h>


const int Channel::KNoneEvent = 0;
const int Channel::KReadEvent = EPOLLIN | EPOLLPRI;
const int Channel::KWriteEvent = EPOLLOUT;

Channel::Channel(EventLoop *loop, int fd)
    : loop_(loop), fd_(fd), events_(0), revents_(0), index_(-1), tied_(false)
{
}
Channel::~Channel()
{

}
//channel的tie方法什么时候调用
void Channel::tie(const std::shared_ptr<void>&obj)
{
    tie_ = obj;
    tied_ = true;
}

/**
 * 当改变channel所有表示fd的event事件后，
 * update负责在poller里边更改fd相应的事件epoll_ctl;
 * EventLoop==>Channelist Poller
 * */
void Channel::update()
{
    //通过channel所属的EventLoop，调用poller的相应方法。
    //注册fd的events事件
    //add code...
    loop_->updateChannel(this);


}

//在channel所属的EventLoop中，把当前的channel删掉
void Channel::remove()
{
    //add code 
    loop_->removeChannel(this);
}

void Channel::handleEvent(Timestamp receiveTime)
{
    LOG_INFO("channel handleEvent revents:%d\n",revents_);
    if(tied_)
    {
        std::shared_ptr<void> guard = tie_.lock();
        if(guard)
        {
            handleEventWithGuard(receiveTime);
        }
    }
    else
    {
       handleEventWithGuard(receiveTime);
    }
}

void Channel::handleEventWithGuard(Timestamp receiveTime)
{
    if(revents_ & EPOLLHUP &&!(revents_ & EPOLLIN))
    {
        if(closeCallback_)
        {
            closeCallback_();
        }
    }

    if(revents_  & EPOLLERR)
    {
        if(errorCallback_)
        {
            errorCallback_();
        }
    }

    if(revents_ & (EPOLLIN | EPOLLPRI))
    {
        if(readCallback_)
        {
            readCallback_(receiveTime);
        }
    }

    if(revents_ &EPOLLOUT)
    {
        if(writeCallback_)
        {
            writeCallback_();
        }
    }

}