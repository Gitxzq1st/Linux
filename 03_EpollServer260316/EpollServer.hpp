#pragma once

#include <iostream>
#include <memory>
#include <sys/epoll.h>
#include "Socket.hpp"
#include "Epoller.hpp"
#include "Log.hpp"


using namespace std;

static const uint16_t defaultport = 8888;
uint32_t EVENT_IN = (EPOLLIN);
static const int num = 64;

class EpoolServer
{
public:
    EpoolServer(uint16_t port)
        : port_(port),
          listensocket_ptr_(new Sock()),
          epoller_ptr_(new Epoller())
    {
    }

    ~EpoolServer()
    {
        listensocket_ptr_->Close();
    }

public:
    void Init()
    {
        listensocket_ptr_->Socket();
        listensocket_ptr_->Bind(port_);
        listensocket_ptr_->Listen();

        lg(Info, "create listen socket success: %d\n", listensocket_ptr_->Fd());
    }

    void Accepter()
    {
        // 我们的连接事件就绪了
        std::string clientip;
        uint16_t clientport = 0;
        int sockfd = listensocket_ptr_->Accept(&clientip, &clientport); // 会不会阻塞在这里？不会  因为此时accept不需要等了，epool进行了等的工作了
        if (sockfd < 0)
            return;
        
        //将connect过来的连接放入epoll的红黑树中统一管理
        epoller_ptr_->EpollerUpdate(EPOLL_CTL_ADD,sockfd,EVENT_IN);
         lg(Info, "get a new link, client info@ %s:%d", clientip.c_str(), clientport);
    
    }

    void Recver(int fd)
    {
        // demo
        char buffer[1024];
        ssize_t n = read(fd, buffer, sizeof(buffer) - 1); // 
        if (n > 0)
        {
            buffer[n] = 0;
            cout << "get a messge: " << buffer << endl;
        }
        else if (n == 0)
        {
            lg(Info, "client quit, me too, close fd is : %d", fd);
            
            epoller_ptr_->EpollerUpdate(EPOLL_CTL_DEL,fd,0);// 这里本质是从红黑树中移除
            //不能先关fd
            close(fd);
        }
        else
        {
            lg(Warning, "recv error: fd is : %d", fd);
            epoller_ptr_->EpollerUpdate(EPOLL_CTL_DEL,fd,0);// 这里本质是从红黑树中移除
            close(fd);
        }
    }

    void Dispatcher(struct epoll_event rev[],int num)
    {
        // 此时需要处理新事件，我们需要根据rev中events的内容进行处理判断
        for (int i = 0; i < num; i++) // 这是第三个循环
        {
            uint32_t events = rev[i].events;
            int fd = rev[i].data.fd;

            if (events & EVENT_IN)
            {
                if (fd == listensocket_ptr_->Fd()) // 此时有新连接到来了
                {
                    Accepter(); // 连接管理器
                }
                else // non listenfd
                {
                    Recver(fd); // 此时需要我们去读取文件了
                }
            }
        }
    }

    void Start()
    {

        //将listensockfd交给epoller管理，就是把他放进红黑树中
        int listensockfd = listensocket_ptr_->Fd();
        epoller_ptr_->EpollerUpdate(EPOLL_CTL_ADD,listensockfd,EVENT_IN);

        struct epoll_event revs[num];

        for (;;)
        {
           
            // 如果事件就绪，上层不处理，poll会一直通知你
            int n = epoller_ptr_->EpollerWait(revs,num);
            switch (n)
            {
            case 0:
                cout << "time out" << endl;
                break;
            case -1:
                cerr << "epll wait error" << endl;
                break;
            default:
                //就绪队列中有新元素了
                lg(Debug, "event happened, fd is : %d", revs[0].data.fd);
                Dispatcher(revs,n);  
                break;
            }
        }
    }


private:
    std::shared_ptr<Sock> listensocket_ptr_;
    std::shared_ptr<Epoller> epoller_ptr_;
    uint16_t port_;
};