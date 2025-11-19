#include "stdio.h" 
#include "unistd.h" 
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h" 
#include "string.h"




int main(int argc, char *argc[])
{
    int fd, ret;
    int key_val;

    if(argc != 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    filename = argv[1];
    
    fd = open(filename, O_RDWR);
    if(fd < 0)
    {
        printf("Can't open device file: %s\n", filename);
        return -1;
    }

    while(1)
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
        sleep(1);
    }

    printf("keyapp exit!\n");
    close(fd);
    return 0;
}