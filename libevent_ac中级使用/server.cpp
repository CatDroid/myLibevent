
/*
 
g++ server.cpp -o server  -levent -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 


*/
#include<stdio.h>  
#include<string.h>  
#include<errno.h>  
  
#include<event.h>  
#include<event2/bufferevent.h>  
 
 
#include <pthread.h>
/*

bufferevent_socket_new	

bufferevent_setcb

bufferevent_enable

bufferevent_free

使用 bufferevent_read 和 bufferevent_write 来读取数据 代替原来直接读写socket 

*/

void socket_read_cb(bufferevent* bev, void* arg)  
{  
	printf("tid = %lu \n",  pthread_self() );
    char msg[4096];  
    size_t len = bufferevent_read(bev, msg, sizeof(msg));  
  
    msg[len] = '\0';  
    printf("recv the client msg: %s", msg);  
    char reply_msg[4096] = "I have recvieced the msg: ";  
  
    strcat(reply_msg + strlen(reply_msg), msg);  
    bufferevent_write(bev, reply_msg, strlen(reply_msg));  
}  


void event_cb(struct bufferevent *bev, short event, void *arg)  
{  
	printf("tid = %lu \n",  pthread_self() );
    if (event & BEV_EVENT_EOF)  
        printf("connection closed\n");  
    else if (event & BEV_EVENT_ERROR)  
        printf("some other error\n");  
  
   
    bufferevent_free(bev);   //这将自动close套接字和free读写缓冲区  
}  

void accept_cb(int fd, short events, void* arg)  
{  
	printf("tid = %lu \n",  pthread_self() ); 
    evutil_socket_t sockfd;  
  
    struct sockaddr_in client;  
    socklen_t len = sizeof(client);  
  
    sockfd = ::accept(fd, (struct sockaddr*)&client, &len );  
    evutil_make_socket_nonblocking(sockfd);  
  
    printf("accept a client %d\n", sockfd);  
  
    struct event_base* base = (event_base*)arg;  
  
	// 取代之前建立 struct event  这里创建 struct bufferevent  并封装了socket  设置了读写的回调函数
    bufferevent* bev = bufferevent_socket_new(base, sockfd, BEV_OPT_CLOSE_ON_FREE);  
    bufferevent_setcb(bev, socket_read_cb/*读回调*/., NULL/*写回调*/, event_cb/*事件回调*/, arg /*回调函数参数 event_base*/);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
	
}


int tcp_server_init(int port, int listen_num)  {
	
	int errno_save;
	evutil_socket_t listener;

    listener = ::socket(AF_INET, SOCK_STREAM, 0);
	if( listener == -1 )
		return -1;

	evutil_make_listen_socket_reuseable(listener);

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = 0;
	sin.sin_port = htons(port);

	if( ::bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0 )
		goto error;

    if( ::listen(listener, listen_num) < 0)
        goto error;

    evutil_make_socket_nonblocking(listener);

	return listener;
  
error:
	errno_save = errno;
	evutil_closesocket(listener);
	/*	
		套接字API兼容性:
		
		int evutil_closesocket(evutil_socket_t s);
		Unix:	close()
		Win :	closesocket() 不能用close关闭套接字
		
		都是通过socket创建 
	
		evutil_socket_error_to_string 	strerror
		evutil_socket_geterror   		errno 
		Windows套接字错误,与从errno看到的标准C错误是不同的
		
		evutil_make_socket_nonblocking
			关闭监听套接字后，它使用的地址可以立即被另一个套接字使用 TIME_WAIT 
			在Unix中它设置SO_REUSEADDR标志，
			在Windows中则不做任何操作。不能在Windows中使用SO_REUSEADDR标志：它有另外不同的含义（译者注：多个套接字绑定到相同地址）
		evutil_make_socket_closeonexec 
			调用了exec()，应该关闭指定的套接字
			在Unix中函数设置FD_CLOEXEC标志(fcntl) 在Windows上则没有操作 
		int evutil_socketpair(int family, int type, int protocol, evutil_socket_t sv[2]);
		这个函数的行为跟Unix的socketpair()调用相同：创建两个相互连接起来的套接字，可对其使用普通套接字IO调用。函数将两个套接字存储在sv[0]和sv[1]中，成功时返回0，失败时返回-1。 
在Windows中，这个函数仅能支持AF_INET协议族、SOCK_STREAM类型和0协议的套接字。注意：在防火墙软件明确阻止127.0.0.1，禁止主机与自身通话的情况下，函数可能失败。

	*/
	errno = errno_save;

	return -1;
}
  
int main(int argc, char** argv)  
{  
	printf("main tid = %lu \n",  pthread_self() );  
    int listener = tcp_server_init(9999, 10);  
    if( listener == -1 )  {  
        perror(" tcp_server_init error ");  
        return -1;  
    }  
  
    struct event_base* base = event_base_new();  
    //添加监听客户端请求连接事件  
    struct event* ev_listen = event_new(base, listener, EV_READ | EV_PERSIST,  
                                        accept_cb, base);  
    event_add(ev_listen, NULL);  
  
    event_base_dispatch(base);  		//  所有event bufferevent 回调都在主线程回调 !!
    event_base_free(base);  
	
    return 0;  
}  

