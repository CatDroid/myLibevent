
/*
 
g++ server.cpp -o server  -lpthread -levent -levent_pthreads  -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 

*/


#include<netinet/in.h>    
#include<sys/socket.h>    
#include<unistd.h>
    
#include<stdio.h>    
#include<string.h>    

#include <pthread.h>
 
#include <stdint.h>		// int64_t
#include <inttypes.h> 	// PRId64
 
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
	
	/*
	区别:
	
	实现上两者不一样 导致了作用效果不一样
	
	loopexit	event_base 当前所有激活的事件回调完之后
							当前没有loop的话 在下一次进入Loop的时候 会立刻退出 
							支持时间
	loopbreak	event_base 当前激活的事件的回调函数返回之后 就立即退出 event_base->event_break=1 (每次event_base_loop的时候都会清除)
							当前没有loop的话 没有作用 ; 之后再进入loop的时候 会清除这个标记
	
	*/
}




int main()    
{    
	evthread_use_pthreads();//enable threads      需要 -levent_pthreads
    
	// 定时器函数 兼容性
	// 标准timeval操作函数
	struct timeval tv1, tv2, tv3;
	tv1.tv_sec = 5; tv1.tv_usec = 500*1000;
	evutil_gettimeofday(&tv2, NULL);
	evutil_timeradd(&tv1, &tv2, &tv3); 		//	时间加减预算 tv1+tv2=tv3  
	if (evutil_timercmp(&tv1, &tv1, ==)){
		printf("evutil_timercmp == \n");
	}
	if (evutil_timercmp(&tv3, &tv1, >=)){	//	时间比较
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
	
	//	种子则来自操作系统的熵池 entropy pool linux中是/dev/urandom
	//	evutil_secure_rng_init 不需要手动初始化安全随机数发生器，但是如果要确认已经成功初始化
	//							函数返回-1则表示libevent无法在操作系统中找到合适的熵源（source of entropy）
	#define RAND_SIZE 3 
	char buf[RAND_SIZE];
	evutil_secure_rng_get_bytes( buf, RAND_SIZE);// 3 是 3个字节 
	printf("rand %u %u %u\n", 0xFF&buf[0], 0xFF&buf[1], 0xFF&buf[2] );
	
	// 结构体偏移(可移植性函数):从type类型开始处到field字段的字节数 标准 offsetof 宏
	// evutil_offsetof		
	//int offset = evutil_offsetof(struct event_base , tv_cache );
	// struct event_base 是内部结构体 外部不能范围内部
	
	
	// 落后于21世纪的C系统  常常没有实现C99标准规定的stdint.h头文件
	// C99 stdint.h int32_t  int64_t  宽度确定（bit-width-specific）的整数类型
	ev_uint64_t test = 12;
	printf("cv_uint64_t = %" PRId64  "\n", test );
	int64_t test1 = 12 ;
	printf("int64_t = %" PRId64  "\n", test1 );
	
	/*
		类型兼容性
	  	ssize_t	有符号的size_t				ev_ssize_t 	EV_SSIZE_MAX
	 	off_t	文件或者内存块中的偏移量	ev_off_t						 
	 	socklen_t 							ev_socklen_t
	  	int*	足够容纳指针类型而不会截断	ev_intptr_t
	 	int/指针 套接字socket				evutil_socket_t
	*/
	
	/*
		const char *evutil_inet_ntop(int af, const void *src, char *dst, size_t len);
		int evutil_inet_pton(int af, const char *src, void *dst);
			与标准inet_ntop()和inet_pton()函数行为相同
		
		int evutil_parse_sockaddr_port(const char *str, struct sockaddr *out, int *outlen);
			解析来自字符串地址描述str，将结果写入到 struct sockaddr out中 
			outlen参数应该指向一个表示out中可用字节数的整数；函数返回时这个整数将表示实际使用了的字节数
			如果没有给出端口号，结果中的端口号将被设置为0
			函数识别下列地址格式
			[ipv6]:端口号（如[ffff::]:80）
			ipv6（如ffff::）
			[ipv6]（如[ffff::]）
			ipv4:端口号（如1.2.3.4:80）
			ipv4（如1.2.3.4）
		
		IPv4:
			struct sockaddr {
				unsigned short sa_family;  	地址族, AF_xxx  
				char sa_data[14];   		14字节的协议地址 (IPv6已经超过!)
			};
			struct sockaddr_in {
				short int sin_family;   	地址族  
				unsigned short int sin_port;端口号  2个字节
				struct in_addr sin_addr;    Internet地址   32bit 4个字节长度 
				unsigned char sin_zero[8];	与struct sockaddr一样的长度  
			};
		
		IPv6
			2000:0:0:0:0:0:0:1 		2*8 = 16个字节
			
			struct sockaddr_in6 {
			   sa_family_t     sin6_family;		AF_INET6  
			   in_port_t       sin6_port;     	端口号
			   uint32_t        sin6_flowinfo;   IPv6 flow information  
			   struct in6_addr sin6_addr;       IPv6 address  
			   uint32_t        sin6_scope_id;  	Scope ID (new in 2.4)  
			};

			struct in6_addr {
			   unsigned char   s6_addr[16];     IPv6 address 
			};
			
			
			tcp6_socket = socket(AF_INET6, SOCK_STREAM, 0);
			raw6_socket = socket(AF_INET6, SOCK_RAW, protocol);
			udp6_socket = socket(AF_INET6, SOCK_DGRAM, protocol);
	*/
	
	struct sockaddr_in addr ;
	int out_len = sizeof(struct sockaddr_in ) ; 
	int actlen = evutil_parse_sockaddr_port("192.168.1.124:80" , (struct sockaddr*)&addr , &out_len );
	printf("parse: addr = %s port = %d actlen = %d out_len = %d\n" , 
			inet_ntoa(addr.sin_addr), ntohs(addr.sin_port),
			actlen, out_len );
	
	/*
	地址转换函数:
	
		inet_aton  inet_addr 只支持IPv4
	
		int       inet_aton (const char *cp, struct in_addr *inp); 	// sockaddr_in.sin_addr
		in_addr_t inet_addr	(const char *cp);						// sockaddr_in.sin_addr.s_addr
		
		char 	   *inet_ntoa(struct in_addr in);
		
		支持IPv4 IPv6的
		int inet_pton(int family, const char *src, void *dst);
		如果函数出错将返回一个负值，并将errno设置为EAFNOSUPPORT
		如果参数af指定的地址族和src格式不对，函数将返回0 	

		const char *inet_ntop(int family, const void *src, char *dst, socklen_t cnt); 网络二进制结构到ASCII类型的地址
		socklen_t cnt 	所指向缓存区dst的大小 避免溢出 
						如果缓存区太小无法存储地址的值 则返回一个空指针 并将errno置为ENOSPC
	*/
	struct sockaddr_in6 addr6 ;
	out_len = sizeof(struct sockaddr_in6 ) ; 
	actlen = evutil_parse_sockaddr_port("[2000:0:0:0:0:0:0:1]:80" , (struct sockaddr*)&addr6 , &out_len );
	char straddr[INET6_ADDRSTRLEN];
	inet_ntop(AF_INET6, &addr6.sin6_addr, straddr,  sizeof(straddr));
	printf("parse: addr = %s port = %d actlen = %d out_len = %d\n" ,
			straddr , ntohs(addr6.sin6_port), 
			actlen, out_len );
	// parse: addr = 192.168.1.124 port = 80 actlen = 0 out_len = 16
	// parse: addr = 2000::1 port = 80 actlen = 0 out_len = 28
	
    event_base_dispatch(base);    
    
	// 检查退出的原因: event_base->event_gotterm  event_base->event_break(再下一次loop的时候重置)
	if( event_base_got_exit(base) ){
		printf("exit event_base \n");
	}
	if( event_base_got_break(base) ){
		printf("break event_base \n");
	}
  

    evconnlistener_free(listener);   // 会释放 监听socket 
    event_base_free(base);    
    
    return 0;    
}    
 
// listener.c 
// 改变回调函数			void evconnlistener_set_cb(struct evconnlistener *lev, evconnlistener_cb cb, void *arg);
// 开启和关闭连接监听	int evconnlistener_enable(struct evconnlistener *lev);
