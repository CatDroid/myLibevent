
/*

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export LIBRARY_PATH=$LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include
export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include

clang++ event_add_moreAndMoe.cpp -o event_add_moreAndMoe -levent -levent_pthreads

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

void cmd_msg_cb1(int fd, short events, void* arg)  {  
	char msg[1024]; 
	printf("cmd_msg_cb1 events 0x%x\n", events );
    int ret = read(fd, msg, sizeof(msg) - 1 ); 
	if( ret <= 0 )  {  
        perror("read fail ");  
        exit(1);				 
    }
	msg[ret] = '\0' ;	
	
	// 也就是跟这个回调函数相关的event 
	// 找到当前正在运行的event
	// 在其他线程调用时不支持的，而且会导致未定义的行为
	struct event * current = event_base_get_running_event( (struct event_base*)arg );// 2.1.8  2.0还没有这个接口 
	printf("cmd_msg_cb1 event_pending 0x%x \n" , event_pending(current ,EV_READ|EV_WRITE , NULL) ); // 0x1
	
	printf("cmd_msg_cb1 = %s\n" , msg );
}

void cmd_msg_cb(int fd, short events, void* arg)  {  
    
	//	EV_TIMEOUT	0x01
	// 
	printf("cmd_msg_cb Entry ! events 0x%x\n", events );	
	if(events & EV_READ){ 	//	注意区分事件 
		char msg[1024];
		int ret = read(fd, msg, sizeof(msg) - 1 ); 
		if( ret <= 0 )  {  
			perror("read fail ");  
			exit(1);				 
		}
		msg[ret] = '\0' ;	
		printf("cmd_msg_cb = %s\n" , msg );		
	}
	
	if(events & EV_TIMEOUT){
	    event_base *base =  (event_base *) arg ;
		struct event* ev_cmd = event_new(base, 
										STDIN_FILENO,  // 同一个fd 不断地添加
										EV_READ | EV_PERSIST, 
										cmd_msg_cb1,  
										base );  
		event_add(ev_cmd, NULL);
		static int counter = 0 ;
		printf("cmd_msg_cb add one more %d \n" , counter++ );
	}

	printf("cmd_msg_cb Exit ! \n");	
	return ;
}  
  
  
int main(int argc  , char** argv )    
{    

	evthread_use_pthreads();			//	使用这个 并且 event_base创建的时候  没有使用 EVENT_BASE_FLAG_NOLOCK  event_base就是带锁的
	event_enable_debug_mode(); 			//	调试模式 
	
    event_base *base = event_base_new();
	
	
    struct event* ev_cmd = event_new(base, 
										STDIN_FILENO,  
										EV_READ | EV_PERSIST,  // EV_TIMEOUT 是不需要 加入的 只需要event_add的时候 加上超时
										cmd_msg_cb,  
										base );  
	
	
	struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_usec = 1000 ;		// 1ms 
    event_add(ev_cmd,&tv); // 如果 EV_PERSIST  超时会不断调用 
	
	/*
	
		在“非挂起”状态的events上执行event_add操作，则会使得该event在配置的event_base上变为“挂起”状态。
			该函数返回0表示成功，返回-1表示失败。  
　　	
		(修改超时事件!!!)
		如果在已经是“挂起”状态的event进行event_add操作，则会保持其“挂起”状态，并且会重置其超时时间。
		如果event已经是“挂起”状态，而且以NULL为超时时间对其进行re-add操作，则event_add没有任何作用。 
		
	*/
	
    event_base_dispatch(base); 
    event_base_free(base);
    
    return 0;    
}    
 
