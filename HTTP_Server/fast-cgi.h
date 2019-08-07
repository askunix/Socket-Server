#ifndef __FAST_CGI_H__
#define __FAST_CGI_H__ 
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FCGI_MAX_LENGTH 65535
#define FCGI_HOST       "127.0.0.1"
#define FCGI_PORT       9000

#define FCGI_VERSION 1

#define FCGI_HEADER_LEN 8  //协议包长度


/*
 * 用于FCGI_Header的type的值
 */ 
#define FCGI_BEGIN_REQUEST  1   // 请求开始记录类型
#define FCGI_ABORT_REQUEST  2
#define FCGI_END_REQUEST    3   // 响应结束记录类型
#define FCGI_PARAMS         4   // 传输名值对数据
#define FCGI_STDIN          5   // 传输输入数据，例如post数据
#define FCGI_STDOUT         6   // php-fpm响应数据输出
#define FCGI_STDERR         7   // php-fpm错误输出
#define FCGI_DATA           8


/*
 * 期望PHP-fpm扮演的角色值
 */ 
#define FCGI_RESPONDER  1
#define FCGI_AUTHORIZER 2
#define FCGI_FILTER     3



// 1 表示php-fpm响应结束不会关闭该请求连接
#define FCGI_KEEP_CONN 1




/*
 *协议级别状态码的值
 */
#define FCGI_REQUEST_COMPLETE   1   // 正常结束
#define FCGI_CANT_MPX_CONN      2   // 拒绝新请求，无法并发处理
#define FCGI_OVERLOADED         3   // 拒绝新请求，资源超负载
#define FCGI_UNKNOWN_ROLE       4   // 不能识别的角色


/*
 * fast cgi协议报头
 */ 
typedef struct 
{
  unsigned char version;  //版本号
  unsigned char type;  //协议类型 
  unsigned char requestIdB1;
  unsigned char requestIdB0;
  unsigned char contentLengthB1;  //内容长度
  unsigned char contentLengthB0; 
  unsigned char paddingLength; //填充字节长度
  unsigned char reserved; //保留字节
}FCGI_Header;


/*
 * 请求开始记录的协议
 */ 
typedef struct 
{
  unsigned char roleB1;  //web服务器期望PHP-fpm扮演的角色
  unsigned char roleB0;
  unsigned char flags;   //控制连接响应后是否立即关闭
  unsigned char reserved[5];
}FCGI_BeginRequestBody;


/*
 * 开始请求记录结构，包含开始请求协议头和协议体
 */ 
typedef struct 
{
  FCGI_Header header;
  FCGI_BeginRequestBody body;
}FCGI_BeginRequestRecord;



/*
 * 结束请求记录的协议体
 */ 
typedef struct 
{
  unsigned char appStatusB3;
  unsigned char appStatusB2;
  unsigned char appStatusB1;
  unsigned char appStatusB0;
  unsigned char protocolStatus;   // 协议级别的状态码
  unsigned char reserved[3];
}FCGI_EndRequestBody;


/*
 * 结束请求记录
 */ 
typedef struct 
{
  FCGI_Header header;
  FCGI_EndRequestBody body;
}FCGI_EndRequestRecord;



typedef struct 
{
  FCGI_Header header;
  unsigned char nameLength;
  unsigned char valueLength;
  unsigned char  data[0];
}FCGI_ParamsRecord;



//构造协议记录头部
FCGI_BeginRequestBody makeBeginRequestBody(int role, int keepConn);

// 发送协议记录函数指针
typedef ssize_t (*write_record)(int, void *, size_t); 

//发送开始请求记录
int sendBeginRequestRecord(write_record wr, int fd, int requestId);

//发送键值对参数
int sendParamsRecord(
            write_record wr, int fd, int requestId,
            char *name, int nlen, char *value, int vlen
                    );

//发送空的params记录
int sendEmptyParamsRecord(write_record wr, int fd, int requestId);

//发送FCGI_STDIN数据
int sendStdinRecord(
            write_record wr, int fd, int requestId, char *data, int len
                   );

//发送空的FCGI_STDIN记录
int sendEmptyStdinRecord(write_record wr, int fd, int requestId);

// 读取协议记录函数指针
typedef ssize_t (*read_record)(int, void *, size_t); 

// 发送php结果给客户端回调函数声明
typedef ssize_t (*send_to_client)(int, int, char *, int, char *, FCGI_EndRequestBody *);


/*
 *读取php-fpm处理结果
 */
//int recvRecord(read_record rr, send_to_client stc, int ,int ,int );



#endif














