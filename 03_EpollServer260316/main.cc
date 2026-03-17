#include <iostream>
#include <memory>
#include "EpollServer.hpp"

int main()
{
    // std::cout <<"fd_set bits num : " << sizeof(fd_set) * 8 << std::endl;

    std::unique_ptr<EpoolServer> epsvr(new EpoolServer(8888));
    epsvr->Init();
    epsvr->Start();

    return 0;
}