
/*
 
g++ server.cpp -o server  -lpthread -levent -levent_pthreads  -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 

*/


#include<netinet/in.h>    
#include<sys/socket.h>    
#include<unistd.h>
    
#include<stdio.h>    
#include<string.h>    
 
#include <pthread.h>
 
 
#include<event.h>
#include<event2/listener.h>
#include<event2/bufferevent.h>

#include<event2/thread.h>

#include<arpa/inet.h> // inet_addr inet_ntoa

#include<errno.h>
 
void socket_event_cb(bufferevent *bev, short events, void *arg)    
{    

	printf("socket_event_cb tid = %lu \n",  pthread_self() );
    if (events & BEV_EVENT_EOF)    
        printf("connection closed\n");    
    else if (events & BEV_EVENT_ERROR)    
        printf("some other error\n");    
    
    //这将自动close套接字和free读写缓冲区    
    bufferevent_free(bev);    
}       

void socket_read_cb(bufferevent *bev, void *arg){
	
	// 即使缓冲区未读完，事件也不会再次被激活（除非再次有数据）。因此此处需反复读取直到全部读取完毕。
	// 对于TCP来说 如果没有读取完毕 将会再下次收到触发时候 读取出来 
	printf("socket_read_cb tid = %lu \n",  pthread_self() );
    char msg[10];// 4096
    size_t len = bufferevent_read(bev, msg, sizeof(msg)-1 );	//	bufferevent_read 读
    msg[len] = '\0';
    printf("server read the data %s\n", msg);

    char reply[] = "I has read your data";
    bufferevent_write(bev, reply, strlen(reply) );				//	bufferevent_write 写
}



//一个新客户端连接上服务器了    
//当此函数被调用时，libevent已经帮我们accept了这个客户端。该客户端的文件描述符为fd    
void listener_cb(evconnlistener *listener, evutil_socket_t fd,    
                 struct sockaddr *sock, int socklen, // 当前accpet客户端的ip地址和端口
				 void *arg)    
{    
	printf("listener_cb tid = %lu \n",  pthread_self() );
	
	printf("sizeof(struct sockaddr_in) = %zd  socklen = %d \n" , sizeof(struct sockaddr_in) , socklen );
	printf("client addr %s port %d \n" , inet_ntoa( ((struct sockaddr_in*)sock)->sin_addr ) ,
											ntohs( ((struct sockaddr_in*)sock)->sin_port  ));
	
    printf("accept a client %d\n", fd);    
    event_base *base = (event_base*)arg;    
    //	为这个客户端分配一个bufferevent 
	//	基于套接字的bufferevent是最简单的  BEV_OPT_CLOSE_ON_FREE: bufferevent在free时候关闭socket 
    bufferevent *bev =  bufferevent_socket_new(base, fd,BEV_OPT_CLOSE_ON_FREE);    
    
	//	设置读写回调函数
    bufferevent_setcb(bev, socket_read_cb, NULL, socket_event_cb, NULL);

	// 启用事件
    bufferevent_enable(bev, EV_READ | EV_PERSIST ); 
	
	// 	线程安全
	//	多线程同时访问evbuffer是不安全
	//	如果您需要执行此操作 您可以在evbuffer上调用 evbuffer_enable_locking() 函数 
	//	lock参数为 NULL，则 Libevent 通过evthread_set_lock_creation_callback函数分配一把新锁。否则，它所使用的参数作为该锁
	//	int evbuffer_enable_locking(struct evbuffer *buf, void *lock);
	//	void evbuffer_lock(struct evbuffer *buf);
	//	void evbuffer_unlock(struct evbuffer *buf);

	
}    

static void accept_error_cb(struct evconnlistener *listener, void *ctx)
{
    struct event_base *base = evconnlistener_get_base(listener);
    int err = EVUTIL_SOCKET_ERROR(); // 需要 #include<errno.h>
    fprintf(stderr, "Got an error %d (%s) on the listener. ""Shutting down.\n", err, evutil_socket_error_to_string(err));
    event_base_loopexit(base, NULL);
}


int main()    
{    
	evthread_use_pthreads();//enable threads      需要 -levent_pthreads
    
	// 多个平台的兼容性 定时函数
	struct timeval tv1, tv2, tv3;
	tv1.tv_sec = 5; tv1.tv_usec = 500*1000;
	evutil_gettimeofday(&tv2, NULL);
	evutil_timeradd(&tv1, &tv2, &tv3); // tv1+tv2=tv3  
	if (evutil_timercmp(&tv1, &tv1, ==)){
		printf("evutil_timercmp == \n");
	}
	if (evutil_timercmp(&tv3, &tv1, >=)){
		printf("evutil_timeradd >= \n");
	}
	
	
	// 系统支持的方法 比如 epoll select poll等 跟编译时有关 
	const char ** all_methods = event_get_supported_methods();
	const char ** one_methods = all_methods;
	while( *one_methods != NULL ){
		printf("method '%s' support\n", *one_methods);
		one_methods++;
	}
 

    struct sockaddr_in sin;    
    memset(&sin, 0, sizeof(struct sockaddr_in));    
    sin.sin_family = AF_INET;    
    sin.sin_port = htons(9999);    
    
	/*
		服务端 使用 evconnlistener_new_bind 完成 创建socket 绑定制定地址  监听  并且在连接请求时完成accept 
		客户端 使用 bufferevent_socket_connect 完成 创建socket 绑定地址 连接服务器 
	*/
    event_base *base = event_base_new();
	
	const char* current_method = event_base_get_method(base);
	printf("current_method = %s\n", current_method ); 		// epoll 
	
	int current_feature = event_base_get_features(base); 
	printf("current_feature = 0x%x\n", current_feature );	// 0x3  
	// EV_FEATURE_ET = 0x01  EV_FEATURE_O1 = 0x02  
	
	// 侦听和接受传入的 TCP 连接的方法 
    evconnlistener *listener    
            = evconnlistener_new_bind(base, listener_cb, base,    
                                      LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,    
                                      10, (struct sockaddr*)&sin,    
                                      sizeof(struct sockaddr_in));

	/* 	struct evconnlistener *evconnlistener_new( struct event_base *base,
													evconnlistener_cb cb,  void *ptr, 
													unsigned flags, 
													int backlog,
													evutil_socket_t fd);					// 给定bind好的socket
		struct evconnlistener *evconnlistener_new_bind(struct event_base *base,
													evconnlistener_cb cb, void *ptr, 
													unsigned flags, 
													int backlog,
													const struct sockaddr *sa, int socklen // 给定地址
													);
		其中第一个函数需要自己绑定套接字 而第二个函数会自动绑定套接字
		`backlog`指定了`listen`的个数  
		`flag`参数如下 
		* LEV_OPT_LEAVE_SOCKETS_BLOCKING 	设置阻塞(对accpet没有影响 只是对accpet返回的socket有影响)
		* LEV_OPT_CLOSE_ON_FREE 			释放掉 `evconnlistener`会关闭socket(监听socket)
		* LEV_OPT_CLOSE_ON_EXEC 			(对于 evconnlistener_new_bind 有用 )
		* LEV_OPT_REUSEABLE 				`socket`重用(对于 evconnlistener_new_bind 有用 )
		* LEV_OPT_THREADSAFE 				为socket增加锁，用于多线程/多进程操作 
											ubuntu实现见 evthread_pthread.c --> evthread.c evthread_set_lock_callbacks
											要在使用libevent函数之前 调用 evthread_use_pthreads(不同平台有不同的实现) 来初始化 mutex cond 的函数集合 
		* LEV_OPT_DISABLED					
		* LEV_OPT_DEFERRED_ACCEPT
	*/		

	// `listener`发生错误都会触发回调函数  
	evconnlistener_set_error_cb(listener, accept_error_cb);	
    
	// 在linux上就是errno 
	// #define evutil_socket_geterror(sock) (errno) 
	// #define evutil_socket_error_to_string(errcode)  (strerror(errcode))
	
	#define RAND_SIZE 3 
	char buf[RAND_SIZE];
	evutil_secure_rng_get_bytes( buf, RAND_SIZE);// 3 是 3个字节 
	printf("rand %u %u %u\n", 0xFF&buf[0], 0xFF&buf[1], 0xFF&buf[2] );
	
    event_base_dispatch(base);    
    
    evconnlistener_free(listener);   // 会释放 监听socket 
    event_base_free(base);    
    
    return 0;    
}    
 
// listener.c 
// 改变回调函数			void evconnlistener_set_cb(struct evconnlistener *lev, evconnlistener_cb cb, void *arg);
// 开启和关闭连接监听	int evconnlistener_enable(struct evconnlistener *lev);
