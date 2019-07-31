/*
 *实现了一个简单的socket回显服务器，后期可以更改为HTTP服务器。
 * 使用了epoll + 线程池，支持并发处理客户端请求。
 */


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
#include </home/hjh/APUE/master_7-27/test/server_final/pool.h> //线程池
#include <signal.h>

#define MAXLINE 2048
#define SERVER_PORT 8080
#define OPEN_MAX 512

int tcpStart();
void setnonblocking(int);
void *myProcess(int);

int epoll_fd;
int listenfd;

int sockfd;
char buf[MAXLINE];
int n = 0;

int main()
{
  int res;
  struct epoll_event tep;
  struct epoll_event ep[OPEN_MAX];
  int nready = 0;
  int i = 0;
  int clilen = 0;
  struct sockaddr_in cliaddr;
  int connfd = 0;
  int Client_num = 0;


  pool_init(2);  //可配置的线程数

  listenfd = tcpStart();    //拿到监听的文件描述符
  epoll_fd = epoll_create(OPEN_MAX);
  if(-1 == epoll_fd){
    perror("epoll_create error:");
    exit(EXIT_FAILURE);
  }

  tep.events = EPOLLIN;    //tep文件描述符可读
  tep.data.fd = listenfd;
  res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listenfd, &tep); //将listenfd结构对应到epollfd
  if(-1 == res){
    perror("epoll_ctl:");
    exit(EXIT_FAILURE);
  }
  printf("\n服务器启动成功.\n\n");

  for(;;)
  {
    nready = epoll_wait(epoll_fd, ep, OPEN_MAX, -1);  //等待就绪
    if(-1 == nready){
      perror("epoll_wait:");
      exit(EXIT_FAILURE);
    }
    printf("\n nready: %d \n", nready);
    
    for(i = 0; i < nready; ++i)
    {
      if(!(ep[i].events & EPOLLIN))  //不是读事件，继续循环
        continue;

      if(ep[i].data.fd == listenfd){ 
        //满足事件的fd是listenfd
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd, (struct sockaddr *) &cliaddr, 
                        (socklen_t*) &clilen); 

        printf("\nconnect fd: %d, client count: %d\n", connfd, 
            ++ Client_num);

        tep.events = EPOLLET|EPOLLIN;
        tep.data.fd = connfd;
        res = epoll_ctl(epoll_fd, EPOLL_CTL_ADD, connfd, &tep);
        if(-1 == res){
          perror("epoll_ctl");
          exit(EXIT_FAILURE);
        }
      }

      else {
        sockfd = ep[i].data.fd;      //ep是epoll event类型数组
        pool_add_worker(myProcess, sockfd);
        //等待任务完成
      }    
    }
  }
  close(listenfd);
  close(epoll_fd);
  return 0;
}




int num = 0;
void *myProcess(int fd)    //f为传进来的event。data结构，需要对它进行遍历读
{
  printf("myProcess行任务中…\n");
  printf("拿到的fd : %d\n", fd);
  printf("poll_fd: %d\n", epoll_fd);

  int n = 0;
  n = read(fd, buf, MAXLINE);
  
  if(0 == n){  // 0,说明客户端关闭连接
    int res = 0;
    num--;
    res = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    if(-1 == res){
      perror("epoll_ctl 3");
      exit(EXIT_FAILURE);
    }

    //close(socked);
    num --;
    printf("\nclient [%d] close conntion\n\n", sockfd);
  }
  else if(n < 0){
    perror("read n < 0 :");
    int res = 0;
    num--;
    res = epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
    //close(socked);
    //break;
  }

  else {
    printf("接收到了: %s\n", buf);
    write(fd, buf, n);
    printf("\n");
    memset(buf, 0, sizeof(buf));
  }

  //signal(SIGINT, SIG_IGN);
return NULL;
}

//----------------------------------------------

void setnonblocking(int sock)
{
  int opts;
  opts = fcntl(sock, F_GETFL);
  if(opts < 0){
    perror("fcnntl:");
    exit(EXIT_FAILURE);
  }
  opts = opts|O_NONBLOCK;
  if(fcntl(sock, F_GETFL, opts) < 0){
    perror("fcntl 2 :");
    exit(EXIT_FAILURE);
  }
}


int tcpStart()
{
  int listenfd = 0;
  struct sockaddr_in servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);    //SOCK_STREAM
  if(listenfd < 0){
    perror("socket");
    exit(EXIT_FAILURE);
  }
  setnonblocking(listenfd);  //把socket设置成非阻塞模式

  int opt = 1;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  //端口复用
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port = htons(SERVER_PORT);
  bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
  if(listen(listenfd, 15) < 0){
    perror("listen");
    exit(EXIT_FAILURE);
  }
  return listenfd;
}

