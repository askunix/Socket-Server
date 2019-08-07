#ifndef __SERVER_H__
#define __SERVER_H__ 

#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <errno.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
//#include </home/hjh/APUE/master_7-31/HTTP_Server/pool.h>//线程池
#include <signal.h>
#include <memory.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/sendfile.h>
#include <pthread.h>
#include <string.h>
#include "rio.h"
#include "fast-cgi.h"

#define MAXLINE 1024
#define SERVER_PORT 9090
#define OPEN_MAX 512

#define MAX_FIRST_LINE 1024
pthread_mutex_t mutex;

#if 1
typedef struct Request
{
  char first_line[MAX_FIRST_LINE];
  char *method;
  char *url;
  char *url_path;
  char *query_string;
  //char *content_type;
  int content_length;
}Request;
#endif 


void setnonblocking(int);  //设置非阻塞模式

int tcpStart(char *, char *);  //服务器初始化

int staticResponsePages(int64_t, Request *);  //静态页面解析

void *CreateWorker(void *); //目前线程创建

int readLine(int64_t, char *, size_t);  //读取first_line

void printRequest(Request *);   //打印日志

int alalysisFirstLine(char *, char **, char **);  //从首行解析url、method 

int alalysis_url(char *, char **, char **);  //query_string解析

int readHandler(int64_t, int *);  //去content_length

void errorStatus(int64_t);  //错误页面

int get_cpu_number();  //获取CPU数量

void myProcess(int);  //epoll

int open_clientfd();  //创建fast cgi套接字

int send_fastcgi(rio_t *,Request *, int, int); //发送HTTP请求

int serverDynamicPages(rio_t *, Request *, int); //动态页面

int recv_fastcgi(int, int, int); //从PHP-fpm接收数据



#endif



