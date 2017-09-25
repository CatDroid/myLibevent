
/*

纯粹就是读完之后就写  

端口是  9999

地址是  任意 

g++ server.cpp -o server -levent  -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 

	
*/

#include<stdio.h>  
#include<string.h>  
#include<errno.h>    
#include<unistd.h> 
 
#include<event.h>  
  
 
void socket_read_cb(int fd, short events, void *arg)  
{  
    char msg[4096];  
    struct event *ev = (struct event*)arg;  
    int len = read(fd, msg, sizeof(msg) - 1);  
  
    if( len <= 0 )  {  
        printf("some error happen when read\n");  
        event_free(ev);  
        close(fd);  
        return ;  
    }  
  
    msg[len] = '\0';  
    printf("recv the client msg: %s", msg);		//  msg包含了回车符 但是没有\0
  
    char reply_msg[4096] = "I have recvieced the msg: ";  
    strcat(reply_msg + strlen(reply_msg), msg);  
  
    write(fd, reply_msg, strlen(reply_msg) );  
}  

void accept_cb(int fd, short events, void* arg)  
{  
    evutil_socket_t sockfd;  
  
    struct sockaddr_in client;  
    socklen_t len = sizeof(client);  
  
    sockfd = ::accept(fd, (struct sockaddr*)&client, &len );  
    evutil_make_socket_nonblocking(sockfd);  
  
    printf("accept a client %d\n", sockfd);  
  
    struct event_base* base = (event_base*)arg;  
  
    // 仅仅是为了动态创建一个event结构体  
    struct event *ev = event_new(NULL, -1, 0, NULL, NULL);  
    // 将动态创建的结构体作为event的回调参数  
    event_assign(ev, 
					base, sockfd, 
					EV_READ | EV_PERSIST,  
					socket_read_cb, (void*)ev);  
  
    event_add(ev, NULL);  
}  
  

 
int tcp_server_init(int port, int listen_num)  
{  
    int errno_save;  
    evutil_socket_t listener;  
  
    listener = ::socket(AF_INET, SOCK_STREAM, 0);  
    if( listener == -1 ){
		perror("::socket(AF_INET, SOCK_STREAM error\n");
		return -1;  
	} 
        
  
    //允许多次绑定同一个地址 要用在socket和bind之间  
    evutil_make_listen_socket_reuseable(listener);  
  
    struct sockaddr_in sin;  
    sin.sin_family = AF_INET;  
    sin.sin_addr.s_addr = 0;  
    sin.sin_port = htons(port);  
  
    if( ::bind(listener, (struct sockaddr*)&sin, sizeof(sin)) < 0 )
        goto error;  
  
    if( ::listen(listener, listen_num) < 0)  
        goto error;  
  
  
    //跨平台统一接口，将套接字设置为非阻塞状态  
    evutil_make_socket_nonblocking(listener);  
  
    return listener;  
  
error:  
	errno_save = errno; 
	evutil_closesocket(listener);  
	errno = errno_save;
	printf("error %s\n",strerror(errno) );
  
	return -1;  
} 
 

int main(int argc, char** argv)  
{  
    int listener = tcp_server_init(9999, 10);  
    if( listener == -1 )  {  
        perror(" tcp_server_init error ");  
        return -1;  
    }  
  
	/*
		main_base = event_init();
		event_init 一个event_base对象 全局变量  现在已经 deprecated
		
		struct event_base
		struct event_config
		
		每一个event_base结构提包含了events集合并选择事件类型
		
		如果选择locking方式，会保证交互是线程安全的
		如果需要使用多线程模型的话，需要为每一个线程建立一个event_base
		
		http://www.cnblogs.com/coder2012/p/4259118.html
		
		struct event_base *event_base_new(void);
		struct event_base *event_base_new_with_config(const struct event_config *cfg);
		
		配置：
		struct event_config {
			TAILQ_HEAD(event_configq, event_config_entry) entries;
			int n_cpus_hint; 							//	cpu数量
			enum event_method_feature require_features;	//	指定IO复用的条件
			enum event_base_config_flag flags;
		};
		
		
		enum event_method_feature {
			EV_FEATURE_ET = 0x01,		//	支持边沿触发		
			EV_FEATURE_O1 = 0x02,		//	添加、删除、或者确定哪个事件激活这些动作的 时间复杂度都为O(1)
										//	select、poll是不能满足这个特征的.epoll则满足
									
			EV_FEATURE_FDS = 0x04		//	支持任意的文件描述符，而不能仅仅支持套接字
		};
		
		enum event_base_config_flag {
    
			EVENT_BASE_FLAG_NOLOCK = 0x01,		//	不分配锁（如果设置，保证线程安全）
    
			EVENT_BASE_FLAG_IGNORE_ENV = 0x02,	//	不检测EVENT_*环境变量？
    
			EVENT_BASE_FLAG_STARTUP_IOCP = 0x04,//	win下的iocp
    
			EVENT_BASE_FLAG_NO_CACHE_TIME = 0x08,	//?
		 
			EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST = 0x10,	//	如果决定使用epoll这个多路IO复用函数
															// 	可以安全地使用更快的,基于changelist的多路IO复用函数
			EVENT_BASE_FLAG_PRECISE_TIMER = 0x20			//?
		};

		event_config_set_num_cpus_hint(	struct event_config *cfg, 	int cpus						) `
		event_config_require_features (	struct event_config *cfg, 	int features					) 
		event_config_set_flag		  (	struct event_config *cfg, 	enum event_base_config_flag flag)

		获取配置:
		
		const char **event_get_supported_methods(void);					//	获取当前系统支持的IO复用方法

		const char * event_base_get_method(const struct event_base *);	//	获取配置的IO复用方法
		int event_base_get_features		  (const struct event_base *);

		static int event_config_is_avoided_method(const struct event_config *cfg, const char *method)
																		// 	指明某个参数是否被禁用
																		
		
		设置IO优先级:
		(默认情况下会设置相同的优先级)		
		int event_base_priority_init(struct event_base *base, int n_priorities);
		
		Fork:
		fork使用parent进程中的event_base
		event_reinit(base); // In child   
		
		关闭socket:
		int evutil_closesocket(evutil_socket_t s);
		
		获取socket错误信息:
		#define evutil_socket_geterror(sock)
		#define evutil_socket_error_to_string(errcode)
		
		设置非阻塞模式
		int evutil_make_socket_nonblocking(evutil_socket_t sock);
		
		地址重用
		int evutil_make_listen_socket_reuseable(evutil_socket_t sock);
		
		
		socketpair:
																// 它会产生两个连接socket，一个用于输入，一个用于输出
		int evutil_socketpair(int family, int type, int protocol,evutil_socket_t sv[2]);
		
		随机数生成:
		void evutil_secure_rng_get_bytes(void *buf, size_t n); // 生成n长度的buf数据
		
	*/
    struct event_base* base = event_base_new();  
  
    // 添加监听客户端请求连接事件  
    struct event* ev_listen = event_new(base, listener, 
										EV_READ | EV_PERSIST,  
                                        accept_cb, 
										base);  
										
	// int event_add(struct event *ev, const struct timeval *timeout);
	// 
    event_add(ev_listen, NULL);  
  
  
    event_base_dispatch(base);  
  
    return 0;  
}  
  
 
