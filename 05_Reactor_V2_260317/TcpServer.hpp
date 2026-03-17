#include <iostream>
#include <functional>
#include <iostream>
#include <string>
#include <memory>
#include <cerrno>
#include <unordered_map>
#include "Log.hpp"
#include "Common.hpp"
#include "Epoller.hpp"
#include "Socket.hpp"

class Connection;
class TcpServer;
using callback_t = std::function<void(std::shared_ptr<Connection>)>;

// ET模式
uint32_t EVENT_IN = (EPOLLIN | EPOLLET);
uint32_t EVENT_OUT = (EPOLLOUT | EPOLLET);
static const int nums = 64;
static const int g_buffer_size = 128;

class Connection
{
public:
    Connection(int fd, TcpServer *tcp_server_ptr) : fd_(fd),
                                                    tcp_server_ptr_(tcp_server_ptr)
    {
    }
    ~Connection()
    {
        // close(fd_);
    }

    void Register(callback_t recv_cb, callback_t send_cb, callback_t except_cb)
    {
        recv_cb_ = recv_cb;
        send_cb_ = send_cb;
        except_cb_ = except_cb;
    }

    int SockFd() { return fd_; }

    void AppendInBuffer(const std::string &info)
    {
        inbuffer_ += info;
    }
    void AppendOutBuffer(const std::string &info)
    {
        outbuffer_ += info;
    }
    std::string &Inbuffer() // for debug
    {
        return inbuffer_;
    }
    std::string &OutBuffer()
    {
        return outbuffer_;
    }

private:
    int fd_;
    std::string inbuffer_; // string 二进制流，vector
    std::string outbuffer_;

public:
    callback_t recv_cb_;
    callback_t send_cb_;
    callback_t except_cb_;

    TcpServer *tcp_server_ptr_;
};

class TcpServer
{
public:
    TcpServer(uint16_t port, callback_t OnMessage)
        : port_(port),
          OnMessage_(OnMessage),
          online(true),
          epoller_ptr_(new Epoller()),
          listensockfd_ptr_(new Sock())
    {
    }

    ~TcpServer()
    {
    }

public:
    // 创建listensockfd，绑定,监听,创建listensock对应的connection
    // 加入到connections_中统一管理，并把listensockfd通过EpollerUpdate加入到红黑树中
    void Init()
    {
        listensockfd_ptr_->Socket();

        // SetNonBlock(listensockfd_ptr_->Fd());
        listensockfd_ptr_->Bind(port_);
        listensockfd_ptr_->Listen();
        // 这里的bind的作用要知道
        AddConnection(listensockfd_ptr_->Fd(), EVENT_IN,
                      std::bind(&TcpServer::Accept, this, std::placeholders::_1),
                      nullptr,
                      nullptr);
    }

    void AddConnection(int fd, uint32_t events, callback_t recv_cb, callback_t send_cb, callback_t except_cb)
    {
        // 1.我们要关心的fd和响应的事件除了要让内核监视
        // 2.给listensockfd建立一个Connection对象
        // 3.还要将fd和对应的Connection对象加入到unordered_map中统一管理

        // ET模式下的fd一定要設置為非阻塞
        SetNonBlock(fd);
        // 创建fd对应的Connection对象
        std::shared_ptr<Connection> new_connection = std::make_shared<Connection>(fd, this);
        // 通过操作逻辑的不同设置不同的回调函数即listensockfd对应的的读事件是Accept的链接管理
        // 而正常的fd对的读事件是Recv，即真正的读缓冲区数据
        new_connection->Register(recv_cb, send_cb, except_cb);
        connections_[fd] = new_connection;

        epoller_ptr_->EpollerUpdate(fd, EPOLL_CTL_ADD, events);
    }

    void EnableEvent(int sockfd, bool readable, bool writeable)
    {
        uint32_t events = 0;
        events |= ((readable ? EPOLLIN : 0) | (writeable ? EPOLLOUT : 0) | EPOLLET);
        epoller_ptr_->EpollerUpdate(sockfd, EPOLL_CTL_MOD, events);
    }

    void Recver(std::shared_ptr<Connection> connection)
    {
        int sockfd = connection->SockFd();
        while (true)
        {
            char buffer[g_buffer_size];
            memset(buffer, 0, sizeof(buffer));
            ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0); // 非阻塞读取
            if (n > 0)
            {
                connection->AppendInBuffer(buffer);
            }
            else if (n == 0)
            {
                lg(Info, "sockfd: %d, client quit...", sockfd);
                connection->except_cb_(connection);
                return;
            }
            else
            {
                if (errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                {
                    lg(Warning, "sockfd: %d, client recv error...", sockfd);
                    connection->except_cb_(connection);
                    return;
                }
            }
        }

        // 服务器只负责读，具体的处理逻辑交给上层
        // 1.需要检验报文是否完整 2.完整了之后就处理
        OnMessage_(connection); // 数据在connection中
    }

    void Sender(std::shared_ptr<Connection> connection)
    {
        auto &outbuffer = connection->OutBuffer();
        while (true)
        {
            ssize_t n = send(connection->SockFd(), outbuffer.c_str(), outbuffer.size(), 0);
            if (n > 0)
            {
                outbuffer.erase(0, n);
                if (outbuffer.empty())
                    break;
            }
            else if (n == 0)
            {
                return;
            }
            else
            {
                if (errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                {

                    lg(Warning, "sockfd: %d, client recv error...", connection->SockFd());
                    connection->except_cb_(connection);
                    return;
                }
            }
        }
        if (!outbuffer.empty())
        {
            // 开启对写事件的关心
            EnableEvent(connection->SockFd(), true, true);
        }
        else
        {
            // 关闭对写事件的关心
            EnableEvent(connection->SockFd(), true, false);
        }
    }
    void Excepter(std::shared_ptr<Connection> connection)
    {
        int fd = connection->SockFd();
        lg(Warning, "Excepter sockfd: %d,",connection->SockFd());
        // 1. 移除对特定fd的关心
        // EnableEvent(connection->SockFd(), false, false);
        epoller_ptr_->EpollerUpdate(fd,EPOLL_CTL_DEL, 0);
        // 2. 关闭异常的文件描述符
        lg(Debug, "close %d done...\n", fd);
        close(fd);
        // 3. 从unordered_map中移除
        lg(Debug, "remove %d from _connections...\n", fd);
        // TODO bug
        // auto iter = _connections.find(fd);
        // if(iter == _connections.end()) return;
        // _connections.erase(iter);
        // _connections[fd].reset();
        connections_.erase(fd);
    }

    void Accept(std::shared_ptr<Connection> connection)
    {
        while (true)
        {
            struct sockaddr_in peer;
            socklen_t len = sizeof(peer);
            int sockfd = ::accept(connection->SockFd(), (struct sockaddr *)&peer, &len);
            if (sockfd > 0)
            {
                uint16_t peerport = ntohs(peer.sin_port);
                char ipbuf[128];
                inet_ntop(AF_INET, &peer.sin_addr.s_addr, ipbuf, sizeof(ipbuf));
                lg(Debug, "get a new client, get info-> [%s:%d], sockfd : %d", ipbuf, peerport, sockfd);

                // listensock只需要设置_recv_cb, 而其他sock，读，写，异常
                // AddConnection(sockfd, EVENT_IN,
                //               nullptr,
                //               nullptr,
                //               nullptr);

                AddConnection(sockfd, EVENT_IN,
                              std::bind(&TcpServer::Recver, this, std::placeholders::_1),
                              std::bind(&TcpServer::Sender, this, std::placeholders::_1),
                              std::bind(&TcpServer::Excepter, this, std::placeholders::_1));
            }
            else
            {
                if (errno == EWOULDBLOCK)
                    break;
                else if (errno == EINTR)
                    continue;
                else
                    break;
            }
        }
    }

    bool IsConnectionSafe(int fd)
    {
        auto iter = connections_.find(fd);
        if (iter == connections_.end())
            return false;
        else
            return true;
    }

    void Dispatcher()
    {
        int n = epoller_ptr_->EpollerWait(rev, nums);
        for (int i = 0; i < n; i++)
        {
            uint32_t event = rev[i].events;
            int fd = rev[i].data.fd;
            // 任务派发  判断不同的条件

            // 统一把事件异常转换成为读写问题
            // 只需要处理EPOLLIN EPOLLOUT
            if (event & EPOLLERR)
                event |= (EPOLLIN | EPOLLOUT);
            if (event & EPOLLHUP)
                event |= (EPOLLIN | EPOLLOUT);

            if ((event & EPOLLIN) && IsConnectionSafe(fd))
            {
                if (connections_[fd]->recv_cb_)
                    connections_[fd]->recv_cb_(connections_[fd]);
            }
            if ((event & EPOLLOUT) && IsConnectionSafe(fd))
            {
                if (connections_[fd]->send_cb_)
                    connections_[fd]->send_cb_(connections_[fd]);
            }
        }
    }

    void Start()
    {

        while (online)
        {
            // 事件派发
            Dispatcher();
            PrintConnection();
        }

        online = false;
    }

    void PrintConnection()
    {
        std::cout << "connections fd list:";
        for (auto &connection : connections_)
        {
            std::cout << connection.second->SockFd() << " ";
        }
        std::cout << std::endl;
    }

private:
    uint16_t port_;
    std::shared_ptr<Sock> listensockfd_ptr_;
    std::shared_ptr<Epoller> epoller_ptr_;
    std::unordered_map<int, std::shared_ptr<Connection>> connections_;
    bool online;

    struct epoll_event rev[nums]; // 就绪队列的集合

    // 上层处理信息
    callback_t OnMessage_;
};