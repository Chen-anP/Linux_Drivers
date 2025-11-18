#include "stdio.h" 
#include "unistd.h" 
#include "sys/types.h" 
#include "sys/stat.h" 
#include "fcntl.h" 
#include "stdlib.h" 
#include "string.h"

#define CLOSE_CMD 	0x1	/* 关闭定时器命令 */
#define OPEN_CMD 	0x2	/* 打开定时器命令 */
#define SETPERIOD_CMD 	0x3	/* 设置定时器周期命令 */

#define LEDON 1
#define LEDOFF 0

int main(int argc, char *argv[])
{
    int fd;
    int ret;
    char *filename;

    unsigned int cmd;
    unsigned int arg;
    unsigned char str[100];

    if(argc != 2)
    {
        printf("Usage: %s <ledstat>\n", argv[0]);
        return -1;
    }

    arg = atoi(argv[1]);

    fd = open("/dev/newchrled", O_RDWR);
    if(fd < 0)
    {
        printf("Can't open device file: /dev/newchrled\n");
        return -1;
    }

    while (1) {
        printf("Please input command:\n1: OPEN_CMD\n2: CLOSE_CMD\n3: SETPERIOD_CMD\n");
        scanf("%d", &ret);
        switch (ret) {
            case 1:
                ret = ioctl(fd, OPEN_CMD);
                if (ret < 0) {
                    printf("ioctl OPEN_CMD failed!\n");
                } else {        
                    printf("ioctl OPEN_CMD success!\n");
                }
                break;
            case 2:
                ret = ioctl(fd, CLOSE_CMD);
                if (ret < 0) {
                    printf("ioctl CLOSE_CMD failed!\n");
                } else {        
                    printf("ioctl CLOSE_CMD success!\n");
                }
                break;
            case 3:
                ret = ioctl(fd, SETPERIOD_CMD);
                if (ret < 0) {
                    printf("ioctl SETPERIOD_CMD failed!\n");
                } else {        
                    printf("ioctl SETPERIOD_CMD success!\n");
                }
                break;
            default:
                printf("Invalid command!\n");
                break;
        }
        ioctl(fd, cmd, arg);
    }

    

    close(fd);
    return 0;
}