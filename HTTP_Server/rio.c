#include "rio.h"

/*
 * 从描述符fd中读取n个字节到存储器位置usrbuf
 */
ssize_t rio_readn(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n; // 剩下的未读字节数
    ssize_t nread;
    char *bufp =(char *)usrbuf;

    while (nleft > 0) {
        if ((nread = read(fd, bufp, nleft)) < 0) {
            if (errno == EINTR) { // 被信号处理函数中断返回
                nread = 0;
            } else {
                return -1;  // read出错 
            }
        } else if (nread == 0) { // EOF
            break;
        }
        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft); // 返回已经读取的字节数
}

/*
 * 将usrbuf缓冲区中的前n个字节数据写入fd中
 * 该函数会保证n个字节都会写入fd中
 */
ssize_t rio_writen(int fd, void *usrbuf, size_t n)
{
    size_t nleft = n; // 剩下的未写入字节数
    ssize_t nwritten;
    char *bufp = (char *)usrbuf;

    while (nleft > 0) {
        if ((nwritten = write(fd, bufp, nleft)) <= 0) {
            if (errno == EINTR) { // 被信号处理函数中断返回
                nwritten = 0;
            } else { // write函数出错
                return -1;
            }
        }
        nleft -= nwritten;
        bufp += nwritten;
    }

    return n;
}

/*
 * 初始化内部缓冲区rio_t结构
 */
void rio_readinitb(rio_t *rp, int fd)
{
    rp->rio_fd = fd;
    rp->rio_cnt = 0;
    rp->rio_bufptr = rp->rio_buf;
}




/*
 * 系统调用read函数的包装函数
 * 相对于read，增加了内部缓冲区
 */
static ssize_t rio_read(rio_t *rp, char *usrbuf, size_t n)
{
  printf("       进入rio_read  : rio_cnt: %d   \n", rp->rio_cnt);
    int cnt;

    // 内部缓冲区为空，从缓冲区对应的描述符中继续读取字节填满内部缓冲区
    while (rp->rio_cnt <= 0) { 
        int lon = 0;
        lon = sizeof(rp->rio_buf);
        printf("*****读取, lon = %d\n\n", lon);
        printf("rp->fd: %d\n\n", rp->rio_fd);
        rp->rio_cnt = read(rp->rio_fd, rp->rio_buf, lon);
        printf("*****读取了\n");
        if(rp->rio_cnt < 0){    // 返回-1
            if (errno != EINTR) 
            { 
                printf("read返回-1\n");
                return -1;
            }
        } 
        else if (rp->rio_cnt == 0) { // EOF
          printf("read返回0， 读到EOF\n");  
          return 0;
        } 
        else { 
            printf("缓冲区拷贝\n");
            rp->rio_bufptr = rp->rio_buf;
        }
    }

    printf("比较调用所需的字节数n与内部缓冲区可读字节数rp->rio_cnt\n");
    // 取其中最小值
    cnt = n;
    if (rp->rio_cnt < (int)n) {
        cnt = rp->rio_cnt;
    }
    memcpy(usrbuf, rp->rio_bufptr, cnt);
    rp->rio_bufptr += cnt;
    rp->rio_cnt -= cnt;

    return cnt;
}

/*
 * 从文件rp中读取一行数据（包括结尾的换行符），拷贝到usrbuf
 * 并用0字符来结束这行数据
 */
ssize_t rio_readlineb(rio_t *rp, void *usrbuf, size_t maxlen)
{
    int rc;
    size_t n;
    char c, *bufp = (char *)usrbuf;

    for (n = 1; n < maxlen; n++) {
        if ((rc = rio_read(rp, &c, 1)) == 1) {
            *bufp++ = c;
            if (c == '\n') { // 读完了一行
                break;
            }
        } else if (rc == 0) {
            if (n == 1) {
                return 0; // EOF,但没有读取任何数据
            } else {
                break; //EOF,但已经读取了一些数据
            }
        } else { // 出错
            return -1;
        }
    }

    *bufp = 0;
    return n;
}

/*
 * 从文件rp中读取n字节数据到usrbuf
 */
ssize_t rio_readnb(rio_t *rp, void *usrbuf, size_t n)
{
    printf("         进入rio-readnb,描述符rp->fd = %d  \n", rp->rio_fd);
    size_t nleft = n; // 剩下的未读取字节数
    ssize_t nread;
    char *bufp = (char *)usrbuf;

    while (nleft > 0) {
        if ((nread = rio_read(rp, bufp, nleft)) < 0) {
            if(errno == EINTR) { // 被信号处理程序中断返回
                nread = 0;
            } else {
                return -1; // 读取数据出错
            }
        } else if(nread == 0) { // EOF
            break; 
        } 
        nleft -= nread;
        bufp += nread;
    }

    return (n - nleft);
}
