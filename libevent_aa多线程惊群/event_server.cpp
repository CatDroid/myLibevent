
/* 
clang++
g++ event_server.cpp -o event_server  -lpthread -levent  -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 

说明:服务器监听在本地19870端口, 等待udp client连接,有惊群现象: 
当有数据到来时, 每个线程都被唤醒, 但是只有一个线程可以读到数据 

对于操作系统来说，多个进程/线程在等待同一资源时，也会产生类似的效果，其结 果就是每当资源可用，所有的进程/线程都来竞争资源，造成的后果：
1）系统对用户进程/线程频繁的做无效的调度、上下文切换，系统系能大打折扣。
2）为了确保只有一个线程得到资源，用户必须对资源操作进行加锁保护，进一步加大了系统开销。

最常见的例子就是对于socket描述符的accept操作，

当多个用户进程/线程监听在同一个端口上时,由于实际只可能accept一次,因此就会产生惊群现象.

这个问题是一个古老的问题,新的操作系统内核已经解决了这一问题。

打印：
setup_event_base 0xb0e010 
setup_event_base 0xb0e0a8 
setup_event_base 0xb0e140 
setup_event_base 0xb0e1d8 
setup_event_base 0xb0e270 
setup_event_base 0xb0e308 
setup_event_base 0xb0e3a0 
setup_event_base 0xb0e438 
setup_event_base 0xb0e4d0 
setup_event_base 0xb0e568 
worker_libevent enter 0xb0e010
worker_libevent enter 0xb0e0a8
worker_libevent enter 0xb0e140
worker_libevent enter 0xb0e1d8
worker_libevent enter 0xb0e270
worker_libevent enter 0xb0e308
worker_libevent enter 0xb0e3a0
worker_libevent enter 0xb0e438
worker_libevent enter 0xb0e4d0
worker_libevent enter 0xb0e568
main loop enter ... 
I am in the thread: [140157083244288] 0xb0e4d0
I am in the thread: [140157074851584] 0xb0e568		hhl>其实全部都被唤醒了,但是因为有一个线程read了,其他线程在内核中poll时候就没有资源了
1.read num is: [2]12							
-process done-


*/ 
 
#include <iostream> 
#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <unistd.h> 
#include <pthread.h> 
#include <event.h> 
#include <netinet/in.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <sys/time.h> 
#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
 
using namespace std; 
 
int init_count = 0; 
pthread_mutex_t init_lock; 	// 保护 init_count
pthread_cond_t init_cond; 
 
typedef struct { 
	pthread_t thread_id; 			/* unique ID of this thread			*/ 
	struct event_base *base; 		/* libevent handle this thread uses */ 
	struct event notify_event; 		/* listen event for notify pipe 	*/ 
} mythread; 
 
void *worker_libevent(void *arg) {
	
	mythread *p = (mythread *)arg; 
	pthread_mutex_lock(&init_lock); 
	init_count++; 
	pthread_cond_signal(&init_cond); 
	pthread_mutex_unlock(&init_lock); 
	
	printf("worker_libevent enter %p\n", arg );
	event_base_loop(p->base, 0); 				// epoll_wait 
	printf("worker_libevent exit  %p\n", arg );
	return NULL;
} 
 
int create_worker(void*(*func)(void *), void *arg) {
	
	mythread *p = (mythread *)arg; 
	pthread_t tid; 
	pthread_attr_t attr; 
	 
	pthread_attr_init(&attr); 
	pthread_create(&tid, &attr, func, arg); 
	p->thread_id = tid; 									 
	pthread_attr_destroy(&attr); 
	return 0; 
} 
 
void process(int fd, short which, void *arg) 
{ 
	mythread *p = (mythread *)arg; 
	printf("I am in the thread: [%lu] %p\n", p->thread_id, p ); 	 
	 
	char buffer[100]; 
	memset(buffer, 0, 100); 
 
	int ilen = read(fd, buffer, 2); 
	printf("1.read num is: [%d]%s\n", ilen , buffer); // 基于数据报的 如果buffer不够大 会被裁剪掉
	//sleep(1);
	//memset(buffer, 0, 100); 
	//ilen = read(fd, buffer, 5); 
	//printf("2.read num is: [%d]%s\n", ilen , buffer); // event_client只发了一个数据报 
 
	printf("-process done-\n");
} 
 
//	设置libevent事件回调 
int setup_event_base(mythread *p, int fd) 
{ 
	p->base = event_init(); 					//	 epoll_create 
	event_set(&p->notify_event, fd, EV_READ|EV_PERSIST, process, p);  //	回调函数	回调函数参数 
	event_base_set(p->base, &p->notify_event);	
	event_add(&p->notify_event, 0); 			//	epoll_ctrl  
	printf("setup_event_base %p \n", p );
	return 0; 
} 
 
int main() 
{ 
	struct sockaddr_in in; 
	int fd; 
 
	fd = socket(AF_INET, SOCK_DGRAM, 0); 	//	在127.0.0.1:19870处监听	数据报 不用 listen accept 

	struct in_addr s; 
	bzero(&in, sizeof(in)); 
	in.sin_family = AF_INET; 
	inet_pton(AF_INET, "127.0.0.1", (void *)&s); 
	in.sin_addr.s_addr = s.s_addr; 
	in.sin_port = htons(19870); 
 
	bind(fd, (struct sockaddr*)&in, sizeof(in)); 
	

	pthread_mutex_init(&init_lock, NULL); 
	pthread_cond_init(&init_cond, NULL); 
	int threadnum = 10; 								//	创建10个线程 
	int i; 
 
	//	10个线程都监听同一个socket描述符, 检查是否产生惊群现象? 
	mythread *g_thread; 
	g_thread = (mythread *)malloc(sizeof(mythread)*10); 
	for(i=0; i<threadnum; i++) { 
		setup_event_base(&g_thread[i], fd); 				//	创建 libevent 
	} 
	for(i=0; i<threadnum; i++) { 
		create_worker(worker_libevent, &g_thread[i]); 	//	创建线程 
	} 
 
	 
	pthread_mutex_lock(&init_lock); 
	while(init_count < threadnum) { 					//	等待全部子线程起来 
		pthread_cond_wait(&init_cond, &init_lock); 
	} 
	pthread_mutex_unlock(&init_lock); 
 
 
	printf("main loop enter ... \n"); 
	while( getchar() ) { 
		sleep(1); 
	} 
	printf("main loop exit ... \n"); 
	 
	
	free(g_thread); 									//	没有回收线程的代码 
	return 0; 
} 

