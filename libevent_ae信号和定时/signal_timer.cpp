
/*

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export LIBRARY_PATH=$LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include
export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include

clang++ signal_timer.cpp -o signal_timer -levent

*/

#include <netinet/in.h>    
#include <sys/socket.h>    
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>    
#include <pthread.h>
#include <errno.h> 
#include <signal.h>
 
#include <event.h>
#include <event2/listener.h>
#include <event2/bufferevent.h>
#include <event2/thread.h>


static void timeout_cb(evutil_socket_t fd, short event, void *arg){
	// fd = -1 event = 0x1  // #define EV_TIMEOUT	0x01  
	printf("timeout fd = %d event = 0x%x \n" , fd ,event );
	//free(arg); // Error in `./signal_timer': double free or corruption (!prev)
}


static int int_called = 0 ;
static void signal_cb(int fd /*信号*/, short event, void *arg){
	
	struct event *signal = (struct event *)arg;
	//  signal_cb: got signal=2 fd=2 event=8
	printf("%s: got signal=%d fd=%d event=%d \n", __func__, event_get_signal(signal),fd , event);
	if (int_called >= 2){ 
		printf("Del Signal! \n");
		event_del(signal);
		event_free(signal);
	}

	int_called++;
}

static int user1_called = 0 ;
static void signal_user1_cb(int fd, short event, void *arg){
	
	struct event *signal = (struct event *)arg;
	printf("%s: got signal=%d fd=%d event=%d \n", __func__, event_get_signal(signal),fd , event);
	if (user1_called >= 5){ 
		printf("Del Signal! \n"); 
		event_del(signal);
		event_free(signal);
	}

	user1_called++;
}



int main()    
{    
    event_base *base = event_base_new();
	
	
	// 	添加定时事件
	struct event* timeout = (struct event*)malloc(sizeof(struct event) );
	event_assign(timeout, base, -1, EV_PERSIST/*这样会每隔两秒产生一次*/ , timeout_cb, (void*)timeout);
	struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 2;
    event_add(timeout,&tv);
	
	/*
	　	注意：
			1.信号回调函数是在信号发生之后，在eventloop中调用的。
				所以，它们可以调用那些，对于普通POSIX信号处理函数来说不是信号安全的函数
				
　　		2.不要在一个信号event上设置超时，不支持这样做。

			3. 当前版本的Libevent，
				对于大多数的后端方法来说，同一时间，每个进程仅能有一个event_base可以用来监听信号。
				如果一次向两个event_base添加event，即使是不同的信号，(!!!)
				也仅仅会只有一个event_base可以接收到信号。
				对于kqueue来说，不存在这样的限制。
	*/
	// 	添加信号事件 
	struct event* signal_int = event_new(base, -1, 0, NULL, NULL); 	 
	//	event_set(signal_int, SIGINT, EV_SIGNAL|EV_PERSIST, signal_cb, signal_int); 
	// 	不要用这接口 默认用全局的event_base(event_init @deprecated  )
	event_assign(signal_int, base, SIGINT , EV_SIGNAL|EV_PERSIST  , signal_cb, signal_int );
	
	event_add(signal_int, NULL);
	struct event* signal_user1 = evsignal_new(base,SIGUSR1,signal_user1_cb,  NULL ); 
	event_assign(signal_user1, base, SIGUSR1, EV_SIGNAL|EV_PERSIST , signal_user1_cb,  signal_user1 ); 
	event_add(signal_user1, NULL);
	printf("SIGUSR1 = %u\n", SIGUSR1 );
	// the first one is usually valid for alpha and sparc, the middle one for x86, arm, and most other architectures,
	//	  
	//  SIGINT		2  					Ctrl+C   kill -s 2  
	//	SIGPIPE   	13
	//  SIGUSR1   	30,10,16    Term    User-defined signal 1 (中止运行)
    //	SIGUSR2   	31,12,17    Term    User-defined signal 2
	//
	//	

    event_base_dispatch(base);    
    event_base_free(base);    
    
    return 0;    
}    
 
// listener.c 
// 改变回调函数			void evconnlistener_set_cb(struct evconnlistener *lev, evconnlistener_cb cb, void *arg);
// 开启和关闭连接监听	int evconnlistener_enable(struct evconnlistener *lev);
