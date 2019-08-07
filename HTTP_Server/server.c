#include "server.h"
#include <pthread.h>
#include "fast-cgi.h"
#include "rio.h"

// 本文件中的部分函数声明
void HandlerRequest(int64_t);
int send_to_cli(int, int, char *, int, char *, FCGI_EndRequestBody *);

int epoll_fd;
int listenfd;
int sockfd;
char buf[MAXLINE];
int n = 0;


int isDir_t(char file_path[])
{
  struct stat st;
  int ret = stat(file_path, &st);
  if(ret < 0)
    return 0;

  if(S_ISDIR(st.st_mode))
    return -1;
  return 0;
}


void handlerFilePath(const char *url_path, char file_path[])
{
  sprintf(file_path, "./root%s", url_path);
  if(file_path[(strlen(file_path))-1] == '/'){
    strcat(file_path, "index.html");
  }
  if(isDir_t(file_path)){
    strcat(file_path, "index.html");
  }
}


ssize_t getFileSize(char *file_path)
{
  struct stat st;
  int ret = stat(file_path, &st);
  if(ret < 0)
    return 0;
  return st.st_size;
}


int writeStaticFile(int64_t new_sock, char file_path[])
{
  //从filepath读文件内容。文件内容写到new sock返回给客户端
  int fd = open(file_path, O_RDONLY);
  if(fd < 0){
    perror("writeStaticFile -> open:");
    return 404;
  }

  const char *first_line = "HTTP/1.1 200 OK\n";
  const char *blank_line = "\n";

  //body
  send(new_sock, first_line, strlen(first_line), 0);
  send(new_sock, blank_line, strlen(blank_line), 0);
  
  //文件中的内容写入body
  ssize_t file_size = getFileSize(file_path);
  sendfile(new_sock, fd, NULL, file_size);
  
  close(fd);
  return 200;
}



//静态页面，根据URL，直接返回事先存储的文件
int staticResponsePages(int64_t new_sock, Request *req)
{
  char file_path[MAXLINE] = {0};
  handlerFilePath(req->url_path, file_path);
  //读取文件，文件内容写到socket
  int status_code = 200;
  status_code = writeStaticFile(new_sock, file_path);
  return status_code;
}



void setnonblocking(int sock)    //将socket设置为非阻塞模式
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



void* CreateWorker(void * arg)    //第一次用的多线程
{
  int64_t new_sock = (int64_t)arg;
  pthread_mutex_lock(&mutex);
  printf("新线程, \n");
  HandlerRequest(new_sock);
  pthread_mutex_unlock(&mutex);
  return NULL;
}



int readLine(int64_t new_sock, char first_line[], size_t size)  //读首行
{
  /*
   *读取首行，读到换行结束 \r  \n  \r\n
   */
  char ch = '\0';
  size_t count = 0;

  while(count < size-1 && ch != '\n')
  {
    size_t read_num = recv(new_sock, &ch, 1, 0);
    if(read_num < 0)
      return -1; 
    else if(0 == read_num){
      printf("readLine -> recv EOF:");
      return -1;
    }
    if(ch == '\r'){
      recv(new_sock, &ch, 1, MSG_PEEK);
      if(ch == '\n'){
        //当前换行为\r\n
        recv(new_sock, &ch, 1, 0);
       }
      else {
        ch = '\n';
      }
     }
    first_line [count ++] = ch;
   }
  first_line[count] = '\0';
  return count;
}



void printRequest(Request *req)    //打印Request内容
{
  time_t t;
  char buf[MAXLINE];
  time(&t);
  ctime_r(&t, buf);

  printf("Method: %s\n",req->method);
  printf("URL: %s\n", req->url);
  printf("Url_Path: %s\n", req->url_path);
  printf("Query_String: %s\n", req->query_string);
  printf("Data: %s", buf);
  printf("Content_Length: %d\n",req->content_length);
}



int split(char *first_line, const char *sp_ch, char *tok[], int size)
{
  char *ptr;
  int i = 0;
  char *temp = NULL;
  ptr = strtok_r(first_line, sp_ch, &temp);   //线程安全函数
  while(NULL != ptr)
  {
    if(i >= size)
    {
      return i;
    }
    tok[i++] = ptr;
    ptr = strtok_r(NULL, sp_ch, &temp);
  }
  return i;
}



int alalysisFirstLine(char *first_line, char **url, char **method)  //从首行解析url\method 
{
  char *tok[10];
  int tok_size = split(first_line, " ", tok, 10);
  if(tok_size != 3){
    printf("tok_size = %d ;alalysisFirstLine -> split failef\n", tok_size);
    return -1;
  }
  *method = tok[0];
  *url = tok[1];
  return tok_size;
}



int alalysis_url(char *url, char **url_path, char **query_string) //url_path ? query_string 
{
  char *ptr = url;
  *url_path = url;
  for(; *ptr != '\0'; ++ptr){
    if(*ptr == '?'){
      *ptr = '\0';
      *query_string = ptr+1;
      return 0;
    }
  }
  *query_string = NULL;
  return 0;
}



int readHandler(int64_t new_sock, int *p_con_len)
{
  //如果是就读取，否则就不读，循环从socket读取一行判定当前行是不是content_length
  //读到空，循环结束
  char buffer[MAXLINE] = {0};
  while(1)
  {
    size_t read_num = readLine(new_sock, buffer, sizeof(buffer));
    if(read_num <= 0)
      return -1;
    if(strcmp(buffer, "\n") == 0)
      return 0;
  }
  //判断当前行是不是content,是就读取，不是就丢弃
  const char *pPtr = "content_Length";
  if(p_con_len != NULL && strncmp(buffer, pPtr, strlen(pPtr)) == 0){
    *p_con_len = atoi(buffer + strlen(pPtr));
  }
  return 0;
}



void errorStatus(int64_t new_sock)
{
  const char *first_line = "HTTP/1.1 404 Not Found\n";
  const char *type_line  = "Content-Type: text/html; charset=utf-8\n";
  const char *blank_line = "\n";
  const char *html_line = "<head><meta http-equiv=\"content-Type\" content=\"text/html;charset=utf-8\"></head>""<br><br><br><center><h1>页面无法找到!根目录下没有此文件？</h1></center>";
  send(new_sock, first_line, strlen(first_line), 0);
  send(new_sock, type_line, strlen(type_line), 0);
  send(new_sock, blank_line, strlen(blank_line), 0);
  send(new_sock, html_line, strlen(html_line), 0);
}



int get_cpu_number()
{
  char buf[16] = {0};
  int num = 0;
  FILE* fp = popen("cat /proc/cpuinfo |grep processor|wc -l", "r");
  if(fp){
    fread(buf, 1, sizeof(buf)-1, fp);
    pclose(fp);
  }
  num = atoi(buf);
  if(num <= 0){
    num = 1;
  }
  return num;
}



int open_clientfd()
{
  int sock;
  struct sockaddr_in serv_addr;

  sock = socket(PF_INET, SOCK_STREAM, 0);
  if(sock == -1){
    perror("open_clientfd -> socket:");
    exit(EXIT_FAILURE);
  }
  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = inet_addr(FCGI_HOST);
  serv_addr.sin_port = htons(FCGI_PORT);
  //连接服务器
  int fd = 0;
  if(-1 == (fd = connect(sock, (struct sockaddr *)&serv_addr, 
        sizeof(serv_addr)))){
    perror("open_clientfd -> connect:");
    exit(EXIT_FAILURE);
  }
  printf("连接 PHP-fpm 成功\n\n");
  return sock;
}




int send_fastcgi(rio_t *rp, Request *req, int sock, int connfd)
{
  int requestID;
  requestID = sock;

  //params参数名
  char *paname[] =       //////////////////
  {
      "FIRST_LINE",
      "METHOD",
      "URL",
      "URL_PATH",
      "QUERY_STRING",
      "CONTENT_LENGTH",
  };

  //对应上面的params参数名，具体参数值在Request结构体中的便宜
  size_t paoffset[] = 
  {
    (size_t) &(((Request *)0) ->first_line),
    (size_t) &(((Request *)0) ->method),
    (size_t) &(((Request *)0) ->url),
    (size_t) &(((Request *)0) ->url_path),
    (size_t) &(((Request *)0) ->query_string),
    (size_t) &(((Request *)0) ->content_length),
  };

  //发送开始请求记录
  printf("*** 发送开始请求记录,打印 rp->fd = %d\n", rp->rio_fd);
  if(sendBeginRequestRecord(rio_writen, sock, requestID) < 0){
    printf("发送开始请求记录失败\n");
    perror("sendBeginRequestRecord:");
    exit(EXIT_FAILURE);
  }

  //发送params参数
  printf("***发送params参数\n");
  int lon = 0;
  lon = sizeof(paoffset) / sizeof(paoffset[0]);
  int i = 0;
  for(i = 0; i < lon; ++i)
  {
    //params参数不为空时才发送
    if(strlen((char *)(((long)req) + paoffset[i])) > 0)
    {
      if(sendParamsRecord(rio_writen, sock, requestID, paname[i], 
            strlen(paname[i]), (char *)(((long)req) + paoffset[i]), 
            strlen((char *)(((long)req) + paoffset[i]))) < 0)
      {
        printf("sendParamsRecord error\n\n");
        exit(EXIT_FAILURE);
      }
    }
  }
  //发送空的params 
  if(sendEmptyParamsRecord(rio_writen, sock, requestID) < 0)
  {
    printf("sendEmptyParamsRecord error\n");
    exit(EXIT_FAILURE);
  }
  //继续读取请求体数据
  char *buf;
  lon = req->content_length;
  if(lon > 0)
  {
    buf = (char *)malloc(lon + 1);
    memset(buf, '\0', lon);
    //if(rio_readnb(rp, buf, lon) < 0){     
    printf("开始读：rp->rio_fd: %d,  lon:%d \n", rp->rio_fd, lon);
    if(read(sock, buf, lon) < 0) {
      printf("rio_readnb error\n");
      free(buf);
      exit(EXIT_FAILURE);
    }
    //发送stdio数据
    printf("***发送stdio数据\n");
    if(sendStdinRecord(rio_writen, sock, requestID, buf, lon) < 0){
      printf("sendStdinRecord error\n");
      free(buf);
      exit(EXIT_FAILURE);
    }
    free(buf);
  }
  //发送空的stdin数据
  if(sendEmptyStdinRecord(rio_writen, sock, requestID) < 0){
    printf("sendEmptyStdinRecord error\n");
    exit(EXIT_FAILURE);
  }
  printf("已经将请求文件按照一定格式发送给php-fpm\n");
  exit(EXIT_SUCCESS);
}





int recv_fastcgi(int fd ,int sock, int connfd)
{
  int requestID;
  char *ptr;
  int num = 0;

  requestID = sock;
  printf("读取处理结果\n");
  //读取处理结果
  printf("读取处理结果\n");
  if (recvRecord(rio_readn, send_to_cli, fd, sock, requestID) < 0) {
    printf("recvRecord error\n");
    exit(EXIT_FAILURE);
  }
  printf("循环读取\n");
  exit(EXIT_SUCCESS);
}



//PHP处理结果发送给客户端
int send_to_cli(int fd, int outlen, char *out, int errlen, 
                char *err, FCGI_EndRequestBody *r)
{
  char *p;
  int n = 0;
  char buf[MAXLINE];
  sprintf(buf, "HTTP/1.1 200 OK\r\n");
  sprintf(buf, "%sServer: Web Server\r\n", buf);
  sprintf(buf, "%sContent-Length: %d\r\n", buf, outlen + errlen);
  //sprintf(buf, "%sContent-Type: %s\r\n\r\n", buf, "text/html");
  if(rio_writen(fd, buf, strlen(buf)) < 0){
    printf("PHP处理结果发给客户端发生错误\n");
  }
  if(outlen > 0){
    p = index(out, '\r');
    n = (int)(p - out);
    if(rio_writen(fd, p + 3, outlen -n - 3) < 0){
      printf("rio_writen error PHP处理结果发送给客户端模块\n");
      exit(EXIT_FAILURE);
    }
  }
  if(errlen > 0){
    if(rio_writen(fd, err, errlen) < 0){
      printf("rio_writen error 2, PHP处理结果发送给客户端模块\n");
      exit(EXIT_FAILURE);
    }
  }
}




