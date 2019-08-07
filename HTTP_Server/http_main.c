/** 
 * @brief
 * time：2019-8-6
 * 基于多进程、epoll的web server；
 * 可以访问静态页面：存放在服务器根目录下的index.html文件
 * 动态页面：实现了运行.php文件的方法，php-fpm版本：5.4.16
 * 问题：
 * 1、web server有时会接收不到php-fpm返回的数据
 * 
 */

#include "server.h"
#include <pthread.h>
#include "fast-cgi.h"

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;

int main(int argc, char *argv[])
{
  if(3 != argc)
  {
    printf("格式: ./http_main ip port");
    return -1;
  }
  printf("服务器启动中^^^^\n");
  tcpStart(argv[1], argv[2]);
  return 0;
}




int tcpStart(char *ip, char *port)     //服务器初始化
{
  printf("------------web server init---------\n");
  int listenfd = 0;
  struct sockaddr_in servaddr;

  listenfd = socket(AF_INET, SOCK_STREAM, 0);
  if(listenfd < 0){
    perror("tcpStart -> socket:");
    exit(EXIT_FAILURE);
  }
  setnonblocking(listenfd);  //把socket设置成非阻塞模式
  int opt = 1;
  int ret = 0;
  setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));  //端口复用
  //bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);    //ip   INADDR_ANY
  servaddr.sin_port = htons(SERVER_PORT);  //port
  ret = bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));
  if(ret < 0){
    perror("tcpStart -> bind:");
    exit(EXIT_FAILURE);
  }
  if(listen(listenfd, 15) < 0){
    perror("tcpStart -> listen:");
    exit(EXIT_FAILURE);
  }
  printf("服务器初始化完成.\n");
  //获取CPU数量
  int cpu_number = 0;
  cpu_number = get_cpu_number();
  printf("\n--  CPU_num: %d   --\n", cpu_number);
  int i = 0;
  for(i = 0; i < cpu_number; ++i)
  {
    pid_t pid = fork();
    if(0 == pid){
      myProcess(listenfd);
      exit(EXIT_SUCCESS);
    }
    if(0 > pid){
      perror("myProcess -> fork error:");
      continue;    //
    }
  }
  //return listenfd;
  getchar();
  exit(EXIT_SUCCESS);
}



void HandlerRequest(int64_t new_sock)   //请求分析
{
  printf("进入Handler Reauest\n\n");
  Request req;
  rio_t rio;
  int status_code = 200;   //默认成功
  /*
   * 静态页面：GET  返回事先存储的index.html页面
   * 动态页面：GET(query_string), POST    通过fastCGI获取数据
   */ 
  //读取首行,将读取的结果放入first_line
  if(readLine(new_sock, req.first_line, sizeof(req.first_line)) < 0){
    printf("readLine req.first_line failed\n");
    fflush(stdout);
    status_code = 404;
    goto END;
  }
  
  //从首行读取url 和 method 
  if(alalysisFirstLine(req.first_line, &req.url, &req.method) < 0){
    printf("alalysisFirstLine first_line failed\n");
    fflush(stdout);
    status_code = 404;
    goto END;
  }

  //从url中取出url_path, query_string 
  if(alalysis_url(req.url, &req.url_path, &req.query_string) < 0){
    printf("analysis_url url failed\n");
    fflush(stdout);
    status_code = 404; 
    goto END;
  }

  //读取Headler中的content_length
  if(readHandler(new_sock, &req.content_length) < 0){
    status_code = 404;
    goto END;

  }
  printRequest(&req);
  //根据读到的req.method判断是静态响应，还是动态响应
  if(strcasecmp(req.method, "get") == 0 && NULL == req.query_string){
    //静态响应
    status_code = staticResponsePages(new_sock, &req);
    goto END;
  }

  else if(strcasecmp(req.method, "get") == 0 && NULL != req.query_string){
    //动态响应
    //printf("动态响应 fd : %d \n", )
    status_code = serverDynamicPages(&rio, &req, new_sock);
    goto END;
  }

  else if(strcasecmp(req.method, "post") == 0){
    //动态响应
    status_code = serverDynamicPages(&rio, &req, new_sock);
    goto END;
  }
  else {
    status_code = 404;
    goto END;
  }
END:
  if(200 != status_code){
    errorStatus(new_sock);
  }
  close(new_sock);
}


#if 1

//处理动态文件请求
int serverDynamicPages(rio_t *rp, Request *req, int connfd)
{
  printf("*****  动态文件请求  *****\n");
  //sock 是sccept后的fd，
  //创建PHP-fpm服务器套接字
  int fast_cgi_sock;
  fast_cgi_sock = open_clientfd();
  printf("php-fpm 服务器启动成功-------\n\n");
 
  //发送HTTP请求数据
  
  printf("发送HTTP请求: rp->fd: %d\n", rp->rio_fd); 
  send_fastcgi(rp, req, fast_cgi_sock, connfd);

  //接收处理结果
  printf("接收处理结果\n");
  recv_fastcgi(rp->rio_fd, fast_cgi_sock, connfd);
  
  //关闭fastcgi服务器连接的套接字
  close(fast_cgi_sock);
  return 200;
}
#endif 


void myProcess(int listen_fd)   //epoll处理
{
  int conn_fd;
  int ready_fd_num;
  struct sockaddr_in client_addr;
  socklen_t client_addr_size = sizeof(client_addr);
  char buf[256];

  int epoll_fd;
  struct epoll_event ev, events[MAXLINE];
  //创建epoll实例
  epoll_fd = epoll_create(MAXLINE);  // 1024
  ev.data.fd = listen_fd;
  ev.events = EPOLLIN;

  //将listen_fd注册到epoll中
  if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev) == -1){
    perror("myProcess -> epoll_ctl:");
    exit(EXIT_FAILURE);
  }
  //循环等待
  while(1)
  {
    //等待事件发生
    ready_fd_num = epoll_wait(epoll_fd, events, MAXLINE, 0);   //阻塞，返回——
    //printf("[pid: %d] 唤醒-----\n", getpid());
    if(ready_fd_num == -1){
      perror("myProcess -> epoll_wait:");
      continue;
    }

    if(pthread_mutex_init(&mut, NULL) != 0){
       //free(&mut);
       perror("lock init error:");
    }

    int i = 0;
    for(i = 0; i < ready_fd_num; ++i){
      if(events[i].data.fd == listen_fd){  //新的连接
        pthread_mutex_lock(&mut);
        conn_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_addr_size);
        pthread_mutex_unlock(&mut);
        if(conn_fd == -1){
          sprintf(buf, "[pid %d] accept 时出错了\n", getpid());
          perror(buf);
          continue;
        }

        //设置conn_fd为非阻塞
        if(fcntl(conn_fd, F_SETFL, fcntl(conn_fd, F_GETFD, 0) | O_NONBLOCK) == -1){
          continue;
        }

        ev.data.fd = conn_fd;
        ev.events = EPOLLIN;
        if(epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conn_fd, &ev) == -1){
          perror("epoll_ctl error   2--");
          close(conn_fd);
        }

        printf("***** [pid %d]收到来自 %s:%d 的请求 *****\n\n", getpid(), 
               inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
      }
      else if(events[i].events & EPOLLIN){   //某个socket已准备好，可以读取了
        printf("[pid %d]处理来自 %s:%d 的请求\n", getpid(),
                inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
        conn_fd = events[i].data.fd;
        //调用web sever
        HandlerRequest(conn_fd);
        
        close(conn_fd);
      }
      else if(events[i].events & EPOLLERR){
        fprintf(stderr, "epoll error\n");
        close(conn_fd);
      }
    }
  }
}



