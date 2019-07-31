#include<stdio.h>
#include<sys/socket.h>
#include<netinet/in.h>
#include<errno.h>
#include<unistd.h>
#include<string.h>
#include<sys/types.h>
#include<arpa/inet.h>
/*
1、创建socket
2、和服务器建立连接
3、给服务器发送数据
4、从服务器读取返回的结果
5、和服务器断开连接
*/
//客户端不需要固定的端口号，因此不必调用bind（），客户端的端口号由内核自动分配
#define SERVER_PORT_ 8080
#define SERVER_IP "118.24.7681"

int main(int argc, char **argv)
{
  if(argc != 2)
  {
    printf("Usage : client IP\n");
    return 1;
  }
  
  //char *argv[1] = argv[1];
  char buf[1024];
  memset(buf, '\0', sizeof(buf));

  struct sockaddr_in server_sock;
  int sock = socket(AF_INET, SOCK_STREAM, 0);
  bzero(&server_sock, sizeof(server_sock));
  server_sock.sin_family = AF_INET;
  inet_pton(AF_INET, SERVER_IP, &server_sock.sin_addr);
  server_sock.sin_port = htons(SERVER_PORT_);

  int ret = connect(sock, (struct sockaddr *)&server_sock, sizeof(server_sock));
/*
调用connect连接服务器
connect和bind的参数形式一致，区别在于bind参数是自己的地址，connect参数是对方地址。
成功返回0
失败返回-1
*/
  if(ret < 0)
  {
    printf("connect failed !!!\n");
    return 1;
  }
  printf("connect server successed ……\n");

  while(1)
  {
    printf("客户端: ");
    fgets(buf, sizeof(buf), stdin);
    buf[strlen(buf) - 1] = '\0';
    write(sock, buf, sizeof(buf));

    if(strncasecmp(buf, "quit", 4) == 0)
    {
      printf("quit!\n");
      break;
    }

    printf("wait ---\n");

    memset(buf, 0, sizeof(BUFSIZ));
    read(sock, buf, sizeof(buf));
    printf("服务器echo : %s\n\n", buf);
  }
  close(sock);
  return 0;
}

