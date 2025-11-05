#include "stdio.h" 
#include "unistd.h" 
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h" 
#include "string.h"

#define LEDON 1
#define LEDOFF 0

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    unsigned char ledstat;

    if(argc != 2)
    {
        printf("Usage: %s <ledstat>\n", argv[0]);
        return -1;
    }

    ledstat = atoi(argv[1]);

    fd = open("/dev/led", O_RDWR);
    if(fd < 0)
    {
        printf("Can't open device file: /dev/led\n");
        return -1;
    }

    ret = write(fd, &ledstat, sizeof(ledstat));
    if(ret < 0)
    {
        printf("ledapp write failed!\n");
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}