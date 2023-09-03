#include "EPollPoller.h"
#include "Channel.h"
#include "Logger.h"
#include <errno.h>
#include <unistd.h>
 #include <string.h>

//channel未添加到poller中
const int KNew = -1; //channel的成员index_ =-1
//channel已添加到poller中
const int KAdded = 1;
//channel从poller中删除
const int KDeleted = 2;

EPollPoller::EPollPoller(EventLoop *loop)
    :Poller(loop)
    ,epollfd_(::epoll_create1(EPOLL_CLOEXEC))
    ,events_(KInitEventListSize) //epoll_events
{
    if(epollfd_ < 0)
    {
        LOG_FATAL("epoll_create error :%d\n",errno);
    }
}


EPollPoller::~EPollPoller()
{
    ::close(epollfd_);
}

Timestamp EPollPoller::poll(int timeouts,ChannelList *activeChannels)
{
    // 实际上应该用LOG_DEBUG输出日志更为合理
    LOG_INFO("func=%s => fd total count:%lu \n", __FUNCTION__, channels_.size());
    
    int numEvents = ::epoll_wait(epollfd_,&*events_.begin(),static_cast<int>(events_.size()),timeouts);
    int saveErrno = errno;

    Timestamp now(Timestamp::now());

    if(numEvents > 0)
    {
        LOG_INFO("%d events happened\n",numEvents);
        fillActiveChannels(numEvents,activeChannels);

        if(numEvents == events_.size())
        {
            events_.resize(events_.size() *2);
        }
    }
    else if(numEvents == 0)
    {

        LOG_DEBUG("%s timeout!\n",__FUNCTION__);
    }
    else
    {
       if(saveErrno != EINTR)
       {
           errno = saveErrno;
           LOG_ERROR("EPollPoller::poll() err!");
       }
    }
    return now;
}
//channel update remove ==>EventLoop up
/**
 *          EventLoop
 * ChannelList          Poller
 *                         ChannelMap <fd, Chennel*>                     
 * */
void EPollPoller::updateChannel(Channel *channel) 
{
    const int index = channel->index();
    LOG_INFO("func=%s => fd=%d events=%d index=%d\n",__FUNCTION__,channel->fd(),channel->events(),index);

    if(index ==KNew || index == KDeleted)
    {
        if(index == KNew)
        {
            int fd = channel->fd();
            channels_[fd] = channel;
        }
        channel->set_index(KAdded);
        update(EPOLL_CTL_ADD,channel);
    }
    else //channel已经在poller上注册过了
    {
        int fd = channel->fd();
        if(channel->isNoneEvent())
        {
            update(EPOLL_CTL_DEL,channel);
            channel->set_index(KDeleted);
        }
        else
        {
            update(EPOLL_CTL_MOD,channel);
        }
    }
}

//从poller中删除channel
void EPollPoller::removeChannel(Channel *channel)
{
    int fd  = channel->fd();
    
    channels_.erase(fd);
    LOG_INFO("func=%s => fd=%d \n",__FUNCTION__,fd);

    int index = channel->index();
    if(index == KAdded)
    {
        update(EPOLL_CTL_DEL,channel);
    }
    channel->set_index(KNew);
}

void EPollPoller::fillActiveChannels(int numEvents,ChannelList *activeChannels) const
{
    for(int i = 0;i < numEvents; ++i)
    {
        Channel *channel = static_cast<Channel*>(events_[i].data.ptr);

        channel->set_revents(events_[i].events);
        activeChannels->emplace_back(channel);
    }
}
//更新channel通道 epoll_ctl,add/mod/del
void EPollPoller::update(int operation,Channel *channel)
{
    epoll_event event;
    memset(&event,0,sizeof(event));

    int fd = channel->fd();
    event.events = channel->events();
    event.data.fd = fd;
    event.data.ptr = channel;
    

    if(::epoll_ctl(epollfd_,operation,fd,&event) < 0)
    {
        if(operation == EPOLL_CTL_DEL)
        {
            LOG_ERROR("epoll_ctl del error:%d\n",errno);
        }
        else
        {
            LOG_FATAL("epoll_ctl add/mod error:%d\n",errno);
        }
    }
}