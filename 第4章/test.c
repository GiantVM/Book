#include <sys/types.h>    
#include <sys/stat.h>    
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

#define MAGIC 'k'
#define DMA_READ_CMD    _IO(MAGIC,0x1a)
#define DMA_WRITE_CMD    _IO(MAGIC,0x1b)
#define PRINT_EDUINFO_CMD    _IO(MAGIC,0x1c)
#define SEND_INTERRUPT_CMD    _IO(MAGIC,0x1d)
#define FACTORIAL_CMD    _IO(MAGIC,0x1e)



int main(void)
{
    int fd, num, arg, ret;
    fd = open("/dev/edu",O_RDWR,S_IRUSR | S_IWUSR);
    
    if(fd == -1) {
        printf("open edu device failure/n");
        return -1; 
    }
    ret = ioctl(fd, PRINT_EDUINFO_CMD);
    if (ret < 0) {
        printf("ioctl: %d\n", ret);
    }
    
    ret = ioctl(fd, SEND_INTERRUPT_CMD);
    if (ret < 0) {
        printf("ioctl: %d\n", ret);
    }
    
    ret = ioctl(fd, FACTORIAL_CMD);
    if (ret < 0) {
        printf("ioctl: %d\n", ret);
    }
    
    ret = ioctl(fd, DMA_WRITE_CMD);
    if (ret < 0) {
        printf("ioctl: %d\n", ret);
    }
    
    sleep(1);
    close(fd);
    fd = open("/dev/edu",O_RDWR,S_IRUSR | S_IWUSR);
    
    if(fd == -1) {
        printf("open edu device failure/n");
        return -1; 
    }
    
    ret = ioctl(fd, DMA_READ_CMD);
    if (ret < 0) {
        printf("ioctl: %d\n", ret);
    }

    sleep(1);
    close(fd);
    return 0;
    
}
    

