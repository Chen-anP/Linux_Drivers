#include "stdio.h" 
#include "unistd.h" 
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h" 
#include "string.h"


#define KEY0VALUE	0xF0		/* 按键0按下对应的键值 */
#define INVAKEY     0x00		/* 无效的键值 */

int main(int argc, char *argc[])
{
    int fd, retvalue;
    char *filename;
    int keyvalue;

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
        retvalue = read(fd, &keyvalue, sizeof(keyvalue));
        if(retvalue < 0)
        {
            printf("keyapp read failed!\n");
            close(fd);
            return -1;
        }

        if(keyvalue == KEY0VALUE)
        {
            printf("key0 pressed!\n");
        }
        else
        {
            printf("no key pressed!\n");
        }
        sleep(1);
    }

    printf("keyapp exit!\n");
    close(fd);
    return 0;
}