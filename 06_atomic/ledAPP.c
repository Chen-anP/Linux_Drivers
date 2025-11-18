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
    int fd, retvalue;
    char *filename;
    unsigned char cnt = 0;
    unsigned char databuf[1];

    if(argc != 3)
    {
        printf("Usage: %s <filename> <ledstat>\n", argv[0]);
        return -1;
    }

    filename = argv[1];
    
    fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        printf("Can't open device file: %s\n", filename);
        return -1;
    }

    databuf[0] = atoi(argv[2]);

    retvalue = write(fd, databuf, sizeof(databuf));
    if(retvalue < 0)
    {
        printf("ledapp write failed!\n");
        close(fd);
        return -1;
    }

    while(1)
    {
        sleep(5);
        cnt++;
        printf("ledapp running time: %d s\n", cnt*5);
        if(cnt == 5)
            break;  
    }

    printf("ledapp exit!\n");
    close(fd);
    return 0;
}