#pragma once

#include "Log.hpp"
#include <cerrno>
#include <cstring>
#include <sys/epoll.h>

class Epoller
{
public:
    // 构造
    Epoller()
    {
        epfd_ = epoll_create(704); // 直接在构造函数中epoll_create初始化epfd
        if (epfd_ == -1)
        {
            lg(Error, "epoll_create error: %s", strerror(errno));
        }
        else
        {
            lg(Info, "epoll_create success: %d", epfd_);
        }
    }
    ~Epoller()
    {
        close(epfd_);
    }

public:
    // 操作函数
    int EpollerUpdate(int op, int sockfd, uint32_t event)
    {
        // 要先对操作op进行识别   ADD，MOD，DEL

        int n = 0;
        if (op == EPOLL_CTL_DEL)
        {
            n = epoll_ctl(epfd_, op, sockfd, nullptr); // 删除就不关注event的动作了
            if (n != 0)
            {
                lg(Error, "epoll_ctl delete error!");
            }
        }
        else // ADD 和 MOD操作
        {
            struct epoll_event ev;
            ev.events = event;
            ev.data.fd = sockfd;
            n = epoll_ctl(epfd_, op, sockfd, &ev);
            if (n != 0)
            {
                lg(Error, "epoll_ctl error!");
            }
        }
        return n;
    }

    int EpollerWait(struct epoll_event revents[], int num)
    {
        int n = epoll_wait(epfd_, revents, num, timeout_);  
        //第二个和第三个参数都是输出型参数，代表就绪的进程(revents把我们填进去的数据原封不动的还回来)和进程的个数
        return n;
    }

private:
    int epfd_;
    int timeout_{2000};
};