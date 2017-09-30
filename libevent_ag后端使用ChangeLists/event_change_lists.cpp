
/*

export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export LIBRARY_PATH=$LIBRARY_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/lib
export C_INCLUDE_PATH=$C_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include
export CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH:/media/sf_E_DRIVE/EclipseSource/libevent-2.1.8-stable/install/include

clang++ event_change_lists.cpp -o event_change_lists -levent -lpthread -levent_pthreads

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

pthread_t thisThread; 


static void write_cb(evutil_socket_t fd, short event, void *arg){
	printf("write_cb fd = %d event = 0x%x free \n" , fd , event );
	event_del ( (struct event*)arg);
	event_free( (struct event*)arg);
}


static void* async_thread(void* arg ){
	
	printf("async_thread sleep begin \n");
	sleep(3);
	printf("async_thread sleep done  \n");
	
	
	event_base *base = (event_base *)arg ;	
	struct event* ready_in =  event_new(base, -1, 0, NULL, NULL );
	event_assign(ready_in, base, STDOUT_FILENO ,  EV_WRITE /*只执行一次*/ , write_cb, (void*)ready_in); 
    event_add(ready_in,NULL); // 异步线程  添加事件  会由 eventfd 唤醒 epoll_dispatch 
	
	
	printf("add Write Event Done !\n");
	return NULL;
}

void cmd_msg_cb(int fd, short events, void* arg)  {  
    
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
		pthread_create(&thisThread,  NULL,  async_thread,  arg);
	}
	printf("cmd_msg_cb Exit ! \n");	
	return ;
}  
  
  
int main(int argc  , char** argv )    
{    

	evthread_use_pthreads();			//	使用这个 并且 event_base创建的时候  没有使用 EVENT_BASE_FLAG_NOLOCK  event_base就是带锁的
	event_enable_debug_mode(); 			//	调试模式 
	
	#if 1 
		struct event_config * confg = event_config_new();
		event_config_set_flag(confg , EVENT_BASE_FLAG_EPOLL_USE_CHANGELIST );
		event_base *base = event_base_new_with_config(confg); // 需要evthread_use_pthreads配合 否则其他线程event_add不会立刻调用epoll_ctrl 
		event_config_free(confg);
	#else
		event_base *base = event_base_new();	
	#endif 
	
	// current method = epoll (with changelist)
	const char* method = event_base_get_method(base);
	printf("current method = %s\n" , method );
	
	
    struct event* ev_cmd = event_new(	base, 
										STDIN_FILENO,  
										EV_READ | EV_PERSIST,  // EV_TIMEOUT 是不需要 加入的 只需要event_add的时候 加上超时
										cmd_msg_cb,  
										base );  

	event_add(ev_cmd, NULL); 
	
	/*
		lrwx------ 1 hhl hhl 64 9月  30 17:20 3 -> anon_inode:[eventpoll]	<<= 创建的eventpoll 
		lr-x------ 1 hhl hhl 64 9月  30 17:20 4 -> pipe:[280888]
		l-wx------ 1 hhl hhl 64 9月  30 17:20 5 -> pipe:[280888]			<<= 创建管道
		lrwx------ 1 hhl hhl 64 9月  30 17:20 6 -> anon_inode:[eventfd]		<<= 创建eventfd  仅在evthread_use_pthreads会创建
																					若 后端使用epool+changelist 但 不设置evthread_use_pthreads 在其他线程event_add不能立刻加入队列 除非有其他事件触发epoll_dispatch循环一次  
		
		an anonymous inode : 
		an anonymous inode is an inode without an attached directory entry
		数据在disk上 但是在文件系统中没有目录项 不能被其他进程打开  进程可以:  mktemp()  open with O_TMPFILE
		int fd = open( "/tmp/file", O_CREAT | O_RDWR, 0666 );
		unlink( "/tmp/file" );

	*/
    event_base_dispatch(base);  // 1.如果没有事件,直接退出 2.运行到没有pending和active事件 3.event_base_loopexit调用
    event_base_free(base);
    
	printf("End of Main\n");
    return 0;    
}    
 
