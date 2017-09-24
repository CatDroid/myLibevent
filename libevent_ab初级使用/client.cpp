
/*
./client  服务器地址  服务器端口 

两个输入:

把终端/标准输入 的数据  发送给服务器   	cmd_msg_cb

把服务器的回复 打印到终端				socket_read_cb


g++ client.cpp -o client -levent  -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 


*/

#include<sys/types.h>  
#include<sys/socket.h>  
#include<netinet/in.h>  
#include<arpa/inet.h>  
#include<errno.h>  
#include<unistd.h>  
  
#include<stdio.h>  
#include<string.h>  
#include<stdlib.h>  
  
#include<event.h>  
#include<event2/util.h>  
  

void cmd_msg_cb(int fd, short events, void* arg)  {  
    char msg[1024];  
  
    int ret = read(fd, msg, sizeof(msg)); // 从终端读取(标准输入) 
    if( ret <= 0 )  {  
        perror("read fail ");  
        exit(1);				//	整个进程退出
    }  
    int sockfd = *((int*)arg);  
    write(sockfd, msg, ret);	//	把终端的消息发送给服务器端  为了简单起见，不考虑写一半数据的情况 
}  
  
  
void socket_read_cb(int fd, short events, void *arg)  
{  
    char msg[1024];  
    int len = read(fd, msg, sizeof(msg)-1);  //	为了简单起见，不考虑读一半数据的情况  
    if( len <= 0 )  {  
        perror("read fail ");  
        exit(1);  
    }  
    msg[len] = '\0';  
    printf("recv from server:%s\n", msg);  
}  
  
  
int tcp_connect_server(const char* server_ip, int port)  
{  
    int sockfd, status, save_errno;  
    struct sockaddr_in server_addr;  
  
    memset(&server_addr, 0, sizeof(server_addr) );  
  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(port);  
    status = inet_aton(server_ip, &server_addr.sin_addr);  
  
    if( status == 0 ) {  
        errno = EINVAL;  //the server_ip is not valid value  
        return -1;  
    }  
  
    sockfd = ::socket(PF_INET, SOCK_STREAM, 0);  
    if( sockfd == -1 )
        return sockfd;  
  
    status = ::connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr) );  
    if( status == -1 )  {  
        save_errno = errno;  
        ::close(sockfd);  
        errno = save_errno; //the close may be error  
        return -1;  
    }  
    evutil_make_socket_nonblocking(sockfd);  
    return sockfd;  
}  


int main(int argc, char** argv)  
{  
    if( argc < 3 )  { 
        printf("please input 2 parameter\n");  
        return -1;  
    }  
  
  
    //	两个参数依次是服务器端的IP地址、端口号  
    int sockfd = tcp_connect_server(argv[1], atoi(argv[2]));  
    if( sockfd == -1)  {  
        perror("tcp_connect error ");  
        return -1;  
    }  
    printf("connect to server successful\n");  
  
    struct event_base* base = event_base_new();  
  
    struct event* ev_sockfd = event_new(base, sockfd,  
                                        EV_READ | EV_PERSIST,  
                                        socket_read_cb, NULL);  
    event_add(ev_sockfd, NULL);  
  
    
    struct event* ev_cmd = event_new(base, STDIN_FILENO,  //	监听终端输入事件(标准输入)
										EV_READ | EV_PERSIST, 
										cmd_msg_cb,  
										(void*)&sockfd);  
  
  
    event_add(ev_cmd, NULL);  
  
	printf("client ENTER event dispatch loop\n");
    event_base_dispatch(base);  
	printf("client EXIT  event dispatch loop\n");
  
    return 0;  
}  
