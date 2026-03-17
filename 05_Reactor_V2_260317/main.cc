#include <iostream>
#include <memory>
#include "TcpServer.hpp"
#include "Calculator.hpp"

Calculator calculator;

void DefaultOnMessage(std::shared_ptr<Connection> connection_ptr)
{
    // if(conn.expired()) return;
    // auto connection_ptr = conn.lock();
    // // 对报文进行处理，有bug
    std::cout << "上层得到了数据： " << connection_ptr->Inbuffer() << std::endl;
    std::string response_str = calculator.Handler(connection_ptr->Inbuffer()); // 我们的业务逻辑比较简单，没有特别耗时的操作
    if (response_str.empty())
        return;
    lg(Debug, "%s", response_str.c_str());
    // // response_str 发送出去
    connection_ptr->AppendOutBuffer(response_str);
    // // 正确的理解发送？
    // // connection_ptr->_send_cb(connection_ptr);

    auto tcpserver = connection_ptr->tcp_server_ptr_;
    tcpserver->Sender(connection_ptr);
}

int main()
{
    // 将业务逻辑函数指针注册进 Reactor 服务器
    std::unique_ptr<TcpServer> tcpsvr(new TcpServer(8080, DefaultOnMessage));
    tcpsvr->Init();
    tcpsvr->Start();

    return 0;
}

// 用户的业务逻辑：收到消息后的处理（我们实现一个 Echo 服务器）
// void OnMessage(Connection* conn)
// {
//     std::cout << "Application layer received: " << conn->in_buffer_ << std::endl;
//
//     // 1. 业务处理：将响应数据放入该连接的输出缓冲区
//     conn->out_buffer_ += "Server Echo: ";
//     conn->out_buffer_ += conn->in_buffer_;
//
//     // 2. 清空接收缓冲 (在实际应用中，这里应处理粘包/半包问题)
//     conn->in_buffer_.clear();
//
//     // 3. 注册 EPOLLOUT 写事件，让 Reactor 帮我们在内核就绪时异步发送数据
//     conn->server_->EnableReadWrite(conn->fd_, true, true);
// }
