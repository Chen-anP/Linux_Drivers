#include "stdio.h" 
#include "unistd.h" 
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h" 
#include "string.h"


static int fd;

static void sigio_signal_func(int signum)
{
    int key_val;

    read(fd, &key_val, sizeof(key_val));
    if(key_val == 0)
    {
        printf("key press!\n");
    }
    else if(key_val == 1)
    {
        printf("key release!\n");
    }
    else
    {
        printf("no key action!\n");
    }
}

int main(int argc, char *argc[])
{
    int flags = 0;

    if(argc != 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    filename = argv[1];
    
    fd = open(filename, O_RDWR | O_NONBLOCK);
    if(fd < 0)
    {
        printf("Can't open device file: %s\n", filename);
        return -1;
    }

    signal(SIGIO, sigio_signal_func);
    /* 设置当前进程为文件描述符fd的所有者 */
    fcntl(fd, F_SETOWN, getpid());
    /* 获取文件状态标志 */
    flags = fcntl(fd, F_GETFL);
    /* 设置文件状态标志, 异步通知属性 */
    fcntl(fd, F_SETFL, flags | FASYNC);

    while(1)
    {
        ret = select(fd + 1, &readfds, NULL, NULL, NULL);
        if (ret < 0)
        {
            printf("select failed!\n");
            break;
        }
        else if (ret == 0)
        {
            continue;
        }

        if (FD_ISSET(fd, &readfds))
        {
            read(fd, &key_val, sizeof(key_val));
            if(key_val == 0)
            {
                printf("key press!\n");
            }
            else if(key_val == 1)
            {
                printf("key release!\n");
            }
            else
            {
                printf("no key action!\n");
            }
        }
    }

    printf("keyapp exit!\n");
    close(fd);
    return 0;
}