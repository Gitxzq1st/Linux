#pragma once

#include<fcntl.h>
#include<unistd.h>
#include<stdlib.h>

void SetNonBlock(int fd)
{
    int fl = fcntl(fd, F_GETFL);
    if (fl < 0)
    {
        exit(111);
    }

    // 設置的時候不能忘了fl
    fcntl(fd, F_SETFL, fl | O_NONBLOCK);
    //lg(Info,"SetnonBlock success");
}