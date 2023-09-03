#pragma once 
#include <vector>
#include <string>
#include <algorithm>
#include <sys/uio.h>
#include <unistd.h>


class Buffer
{
public:
    static const size_t kCheapPrepend = 8;
    static const size_t kInitialSize = 1024;

explicit Buffer(size_t initialSize = kInitialSize)
:buffer_(kCheapPrepend + initialSize),
  readerIndex_(kCheapPrepend),
  writerIndex_(kCheapPrepend)
  {

  }

  size_t readableBytes() const
  {
    return writerIndex_ - readerIndex_;
  }

  size_t writableBytes() const
  {
    return buffer_.size() - writerIndex_;
  }

  size_t prependableBytes() const
  { 
    return readerIndex_; 
  }
  //返回缓存取中可读数据的起始地址
  const char *peek() const
  {
      return begin() + readerIndex_;
  }

  void retrieve(size_t len)
  {
      if(len < readableBytes())
      {
          readerIndex_ += len;// 应用只读取了刻度缓冲区数据的一部分，就是len，还剩下readerIndex_ += len -> writerIndex_
      }
      else  //len == readbleBytes()
      {
        retrieveAll();
      }
  }

  void retrieveAll()
  {
      readerIndex_ = writerIndex_ = kCheapPrepend;
  }
// 把onMessage函数上报的Buffer数据，转成string类型的数据返回
std::string retrieveAllAsString()
{
    return retrieveAsString(readableBytes());
}
std::string retrieveAsString(size_t len)
{
    std::string result(peek(), len);
    retrieve(len);// 上面一句把缓冲区中可读的数据，已经读取出来，这里肯定要对缓冲区进行复位操作
    return result;
}

void ensureWriteableBytes(size_t len)
{
    if(writableBytes() < len)
    {
        makeSpace(len); //扩容函数
    }
}


void append(const char *data,size_t len)
{
  ensureWriteableBytes(len);
  std::copy(data,data +len,beginWrite());
  writerIndex_ += len;
}

char *beginWrite()
{
  return begin() + writerIndex_;
}

const char *beginWrite() const
{
  return begin() + writerIndex_;
}

// 从fd上读取数据
ssize_t readFd(int fd, int* saveErrno);
// 通过fd发送数据
ssize_t writeFd(int fd, int* saveErrno);

private:

char* begin()
{
    return &*buffer_.begin(); //vector底层首元素的地址，也就是数组的起始地址
}

const char* begin() const
{ 
  return &*buffer_.begin(); 
}

void makeSpace(size_t len)
{
  if (writableBytes() + prependableBytes() < len + kCheapPrepend)
  {
      // FIXME: move readable data
      buffer_.resize(writerIndex_ + len);
  }
  else
  {
    size_t readable = readableBytes();
    std::copy(begin() + readerIndex_,
              begin() + writerIndex_,
              begin() + kCheapPrepend);
    readerIndex_ = kCheapPrepend;
    writerIndex_ = readerIndex_ + readable;
  }
}




std::vector<char> buffer_;
size_t readerIndex_;
size_t writerIndex_;
};


