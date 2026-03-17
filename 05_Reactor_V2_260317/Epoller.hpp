#include <sys/epoll.h>
#include <unistd.h>
#include <string.h>
#include "Log.hpp"
#include <cerrno>

static const int timeout = 1000;

class Epoller
{
public:
    Epoller()
    {
        _epfd = epoll_create(704);
        if (_epfd == -1)
        {
            lg(Error, "epoll_create error: %s", strerror(errno));
        }
        else
        {
            lg(Info, "epoll_create success: %d", _epfd);
        }
    }
    ~Epoller()
    {
        close(_epfd);
    }

public:
    int EpollerUpdate(int fd, int op, uint32_t event)
    {
        int n = 0;
        if (op == EPOLL_CTL_DEL)
        {
            n = epoll_ctl(_epfd, op, fd, 0);
            if (n != 0)
            {
                lg(Error, "epoll_ctl delete error!");
            }
            close(fd);
        }
        else
        {
            struct epoll_event ev;
            ev.data.fd = fd;
            ev.events = event;
            n = epoll_ctl(_epfd, op, fd, &ev);
            if (n != 0)
            {
                lg(Error, "epoll_ctl error!");
            }
            //lg(Info, "epoll_ctl success!");
        }
        return n;
    }

    int EpollerWait(struct epoll_event revents[], int num)
    {

        int n = epoll_wait(_epfd, revents, num, timeout);
        return n;
    }

private:
    int _epfd;

public:
};