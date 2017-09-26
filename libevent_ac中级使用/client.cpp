

/*

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
#include<event2/bufferevent.h>  
#include<event2/buffer.h>  
#include<event2/util.h>  


void cmd_msg_cb(int fd, short events, void* arg)
{
	char msg[1024];
	int ret = read(fd, msg, sizeof(msg));
	if( ret < 0 )  {
		perror("read fail ");
		exit(1);
	}
	struct bufferevent* bev = (struct bufferevent*)arg;
	bufferevent_write(bev, msg, ret); // 使用了bufferevent 代替直接写到socket 
}


void server_msg_cb(struct bufferevent* bev, void* arg)
{
	char msg[1024];
	size_t len = bufferevent_read(bev, msg, sizeof(msg));
	msg[len] = '\0';
	printf("recv %s from server\n", msg);
}
  
  
void event_cb(struct bufferevent *bev, short event, void *arg)// socket可能关闭了 
{  
    if (event & BEV_EVENT_EOF){
		printf("connection closed\n");  
    }else if (event & BEV_EVENT_ERROR){
		printf("some other error\n");
	}
	
    bufferevent_free(bev);  				//这将自动close套接字和free读写缓冲区  
    struct event *ev = (struct event*)arg;  
    event_free(ev);  						//因为socket已经没有，所以这个event也没有存在的必要了  
}  
  
  
 
int tcp_connect_server(const char* server_ip, int port)  
{  
    int sockfd, status, save_errno;  
    struct sockaddr_in server_addr;  
  
    memset(&server_addr, 0, sizeof(server_addr) );  
  
    server_addr.sin_family = AF_INET;  
    server_addr.sin_port = htons(port);  
    status = inet_aton(server_ip, &server_addr.sin_addr);  
  
    if( status == 0 ){ //the server_ip is not valid value  
        errno = EINVAL;  
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

    //两个参数依次是服务器端的IP地址、端口号
    int sockfd = tcp_connect_server(argv[1], atoi(argv[2]));
    if( sockfd == -1)  {
        perror("tcp_connect error ");
        return -1;
    }

    printf("connect to server successful\n");

    struct event_base* base = event_base_new();

	// 用 bufferevent 代替了 sockfd 
    struct bufferevent* bev = bufferevent_socket_new(base, sockfd , BEV_OPT_CLOSE_ON_FREE);
  
    //监听终端输入事件  
    struct event* ev_cmd = event_new(base, STDIN_FILENO,EV_READ | EV_PERSIST, cmd_msg_cb,(void*)bev);  
    event_add(ev_cmd, NULL);  
  
    //当socket关闭时会用到回调参数  
    bufferevent_setcb(bev, server_msg_cb/*读回调*/, NULL/*写回调*/, event_cb/*事件回调*/, (void*)ev_cmd/*回调参数*/);  
    bufferevent_enable(bev, EV_READ | EV_PERSIST); // bufferevent_socket_new 创建时候 与 event_base关联了 
  
  
    event_base_dispatch(base);  
  
    printf("finished \n");  
    return 0;  
}  

