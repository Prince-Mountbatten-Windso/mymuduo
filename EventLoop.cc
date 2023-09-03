#include "EventLoop.h"
#include "Logger.h"
#include "Poller.h"
#include "Channel.h"
#include <sys/eventfd.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <memory>

//防止一个线程创建多个Eventloop  thread_local
__thread EventLoop * t_loopInThisThread = nullptr;

//定义默认的Poller IO复用的超时时间
const int KPollTimeMs = 10000;
//创建wakeupfd，用来notify唤醒subReactor处理新来的channel
int createEventfd()
{
    int evtfd = ::eventfd(0,EFD_NONBLOCK|EFD_CLOEXEC);
    if(evtfd < 0)
    {
        LOG_FATAL("eventfd error:%d\n",errno);
    }
    return evtfd;
}

EventLoop::EventLoop()
    :looping_(false)
    ,quit_(false)
    ,callingPendingFunctors_(false)
    ,threadId_(CurrentThread::tid())
    ,poller_(Poller::newDefaultPoller(this))
    ,wakeupFd_(createEventfd())
    ,wakeupChannel_(new Channel(this,wakeupFd_))
{
    LOG_DEBUG("EventLoop created %p in thread %d\n",this,threadId_);
    if( t_loopInThisThread)
    {
        LOG_FATAL("Another EventLoop  %p exists n thread %d\n",t_loopInThisThread,threadId_);
    }
    else
    {
        t_loopInThisThread = this;
    }
    //设置wakeupfd的事件类型以及发生事件后的回调操作
    wakeupChannel_->setReadCallback(
      std::bind(&EventLoop::handleRead, this));
   // we are always reading the wakeupfd
   //每一个eventloop都将监听wakeupchannel的EPOLLIN读事件
   wakeupChannel_->enableReading();
}


EventLoop::~EventLoop()
{
    wakeupChannel_->disableAll();
    wakeupChannel_->remove();
    ::close(wakeupFd_);
    t_loopInThisThread = nullptr;
}

void EventLoop::handleRead()
{
  uint64_t one = 1;
  ssize_t n = read(wakeupFd_, &one, sizeof one);
  if (n != sizeof one)
  {
    LOG_ERROR("EventLoop::handleRead() reads %ld bytes instead of 8",n);
  }
}

void EventLoop::loop()
{
    looping_ = true;
    quit_ = false;

    LOG_INFO("EventLoop %p start looping\n",this);
    while (!quit_)
    {
        activeChannels_.clear();
        //监听两类fd 一种时clientfd，一种时wakeupfd
        pollReturnTime_ = poller_->poll(KPollTimeMs, &activeChannels_);

        for(Channel *channel :activeChannels_)
        {
            //Poller监听那些channel发生事件，然后上报给EventLOOP，
            //通知channel处理相应的事件
            channel->handleEvent(pollReturnTime_);
        }
   
        //执行当前EventLoop事件循环需要处理的回调操作
        /**
         * IO线程 mainLoop accept fd《=channel subloop
         * mainLoop 事先注册一个回调cb（需要subloop来执行）    wakeup subloop后，执行下面的方法，执行之前mainloop注册的cb操作
         */ 
        doPendingFunctors();
    }
    LOG_INFO("EventLoop %p stop looping. \n", this);
    looping_ = false;
}
//退出事件循环
void EventLoop::quit()
{
    quit_ = true;
    if(!isInLoopThread())
    {
        wakeup();
    }
}

 //在当前loop中执行
void EventLoop::runInLoop(Functor cb)
{
    //在当前的loop线程中执行cb
    if(isInLoopThread())
    {
        cb();
    }
    else //在非当前loop线程中执行cb，需要唤醒loop所在线程，执行cb
    {
        queueInLoop(cb);
    }
}
//把cb放入队列中，唤醒loop所在的线程，执行cb
void EventLoop::queueInLoop(Functor cb)
{
    {
        std::unique_lock<std::mutex> lock(mutex_);
        pendingFunctors_.emplace_back(cb);
    }
    //唤醒相应的，需要执行上面回调操作的loop线程
    //callingPendingFunctors_ 当前loop正在执行回调，但是loop又有了新的回调
    //
    if(!isInLoopThread() || callingPendingFunctors_)
    {
        wakeup();//唤醒loop所在线程
    }
}


void EventLoop::wakeup()
{
    uint64_t one = 1;
    size_t n = write(wakeupFd_,&one,sizeof one);
    if(n != sizeof one)
    {
        LOG_ERROR("EventLoop::wakeup() writes %lu bytes instead of 8\n",n);
    }
}

void EventLoop::updateChannel(Channel *channel)
{
    poller_->updateChannel(channel);
}

void EventLoop::removeChannel(Channel *channel)
{
    poller_->removeChannel(channel);
}

bool EventLoop::hasChannel(Channel *channel)
{
    poller_->hasChannel(channel);
}

void EventLoop::doPendingFunctors() //执行回调
{
    std::vector<Functor> functors;
    callingPendingFunctors_ = true;
    {
        std::unique_lock<std::mutex> lock(mutex_);
        functors.swap(pendingFunctors_);
    }

    for(const Functor &functor :functors)
    {
        functor(); //执行当前loop需要执行的回调操作
    }
    callingPendingFunctors_ = false;
}