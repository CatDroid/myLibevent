

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
  
  
static void timeout_cb(evutil_socket_t fd, short event, void *arg)
{
	// fd = -1 event = 0x1  // #define EV_TIMEOUT	0x01  
	printf("timeout fd = %d event = 0x%x \n" , fd ,event );
	free(arg);
}
	
  
void cmd_msg_cb(int fd, short events/*事件的类型*/, void* arg){
	

	printf("Got an event on socket %d:%s%s%s%s\n",
		(int) fd,
		(events&EV_TIMEOUT) ? " timeout" : "",
		(events&EV_READ)    ? " read" : "",
		(events&EV_WRITE)   ? " write" : "",
		(events&EV_SIGNAL)  ? " signal" : "" );
			
    char msg[1024];

    int ret = read(fd, msg, sizeof(msg));
    if( ret < 0 )  {
        perror("read fail ");
        exit(1);
    }

    struct bufferevent* bev = (struct bufferevent*)arg;
    //把终端的消息发送给服务器端
    bufferevent_write(bev, msg, ret);
	
	
	// 添加定时时间 
	struct event_base * base = bufferevent_get_base(bev); 
	struct event* timeout = (struct event*)malloc(sizeof(struct event) );
	event_assign(timeout, base, -1, 0 /*EV_PERSIST*/, timeout_cb, (void*)timeout);
	struct timeval tv;
    evutil_timerclear(&tv);
    tv.tv_sec = 2;
    event_add(timeout, &tv);
	/*
	
	正常情况下，一般不希望使用队列管理所有的超时时间值，
	
	因为队列仅对于恒定的超时时间来说是快速的。
	(假设有一万个事件，每一个event的超时时间都是在他们被添加之后的5秒钟 O(1) )
	(大量相同时间的events)
	
	
	如果一些超时时间或多或少的随机分布的话，
	那添加这些超时时间到队列将会花费O(n)的时间，这样的性能要比二叉堆差多了
	
	对于有序的添加和删除event超时时间的操作，二叉堆算法可以提供O(lg n)的性能
	对于添加随机分布的超时时间来说，性能是最优的 
	
	
	Libevent解决这种问题的方法是将一些超时时间值放置在队列中，其他的则放入二叉堆中
	
	可以向Libevent请求一个“公用超时时间”的时间值，然后使用该时间值进行事件的添加
	如果存在大量的event，它们的超时时间都是这种"单一公用超时时间"的情况，那么使用这种优化的方法可以明显提高超时事件的性能
	
	const struct  timeval * event_base_init_common_timeout(  struct event_base *base,  const  struct  timeval* duration);
	返回的timeval 仅使用它们来指明使用哪个队列 
	后面的事件 如果超时也是 duration  那么 event_add时候 使用 这里的返回值 
	
	*/
}


void server_msg_cb(struct bufferevent* bev, void* arg){
    char msg[1024];
    size_t len = bufferevent_read(bev, msg, sizeof(msg));
    msg[len] = '\0';
    printf("recv %s from server\n", msg);
}  


void event_cb(struct bufferevent *bev, short event, void *arg)
{

    if (event & BEV_EVENT_EOF){
        printf("connection closed\n");  
    }else if (event & BEV_EVENT_ERROR){
        printf("some other error\n");  
    }else if( event & BEV_EVENT_CONNECTED){  
        printf("the client has connected to server\n");  
        return ;  
    }  
  
    
    bufferevent_free(bev);  					//  close套接字 和 free读写缓冲区 bufferevent  
  
    struct event *ev = (struct event*)arg;  
    event_free(ev);  							//	event_free 					
}  

  
int main(int argc, char** argv){

	if( argc < 3 ){
		printf("please input 2 parameter  ./client <server_ip_addr>  <server_port> \n"); 
		return -1;
    }

    struct event_base *base = event_base_new();
    struct bufferevent* bev = bufferevent_socket_new(base, -1,   BEV_OPT_CLOSE_ON_FREE);

	// #define EV_TIMEOUT 0x01 		/*定时事件*/
	// #define EV_READ 0x02 		/*I/O事件*/
	// #define EV_WRITE 0x04 		/*I/O事件*/
	// #define EV_SIGNAL 0x08 		/*信号*/
	// #define EV_PERSIST 0x10 		/*永久事件 */
	// #define EV_ET 0x20 			/*边沿触发*/

    //	监听终端输入事件
    struct event* ev_cmd = event_new(base, STDIN_FILENO,  EV_READ | EV_PERSIST,  cmd_msg_cb, (void*)bev);
   
	//	给事件重新赋值 
	//int event_assign(struct event *event, 
	//					struct event_base *base,
	//					evutil_socket_t fd, 
	//					short what,
	//					void (*callback)(evutil_socket_t, short, void *), 
	//					void *arg);

	//	虽然已经初始化了事件，但是该事件并不会被触发，原因在于我们并没有激活该事件
	//	激活事件的功能 / 注册事件 
	// 	int event_add(struct event *ev, const struct timeval *tv);
	//	如果是一个（non-pending）未注册`ev`，调用`event_add`函数会注册该事件（变为pending状态）
	//	如果是一个（pending）注册过的`ev`，调用该函数会在tv时间后重新注册该事件。
	event_add(ev_cmd, NULL);

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(argv[2]));
    inet_aton(argv[1], &server_addr.sin_addr);

	// socket的创建和连接都交给bufferevent 
    bufferevent_socket_connect(bev, (struct sockaddr *)&server_addr,  sizeof(server_addr));
    bufferevent_setcb(bev, server_msg_cb, NULL, event_cb, (void*)ev_cmd);
    bufferevent_enable(bev, EV_READ | EV_PERSIST);
	/*
		bufferevent_socket_connect 
		1.目前仅能工作在流式协议上，比如TCP  未来可能会支持数据报协议 比如UDP
		2.connect的时候也是非阻塞的  所以要注意事件回调函数 
		3.水位数(读写 高低 )
		
		基于socket的bufferevent： 	bufferevent_socket_new
			在底层流式socket上发送和接收数据，使用event_*接口作为其后端
		过滤型的bufferevent： 		bufferevent_filter_new
			在数据传送到底层bufferevent对象之前，对到来和外出的数据进行前期处理的bufferevent，比如对数据进行压缩或者转换
		成对的bufferevent：			bufferevent_pair_new	
	*/
	

	// 循环监听
	// 当已经拥有注册了IO复用方法的`event_base`后，可以通过`event_loop`来监听并接受IO事件
	// #define EVLOOP_ONCE	0x01	  等待一个就绪的事件
	// #define EVLOOP_NONBLOCK	0x02  不阻塞 没有就绪事件就退出 有就执行最高优先级的那些 
	// int event_base_loop(struct event_base *, int); 
	// event_base_dispatch(base) -> event_base_loop(base,0)
    event_base_dispatch(base);

    printf("finished \n");
    return 0;  
} 

/*
	设置事件执行一次 (也就是在loop上回调指定的函数一次) 见 event_base_loopexit 的实现 
	int event_base_once(struct event_base *, 
						evutil_socket_t,
						short,
						void (*)(evutil_socket_t, short, void *), void *, 
						const struct timeval *);
	对一个非持久的event，在add之后就会delete,不用自己delete event 
	不支持EV_SIGNAL或EV_PERSIST标志
	不能被删除或者手动激活
	当event_base释放时，即使events还没有被激活，它们的内存也会被释放(注意形参关联内存要自己释放)
	
	
	如果想自己激活某个事件，那么可以执行下面的函数
	void event_active(struct event *ev, int what, short ncalls);
	* what可以为EV_READ, EV_WRITE, and EV_TIMEOUT
	* ncalls为激活次数
	
	调试：
	这个函数可以把`event_base`中信息和状态写入文件中
	void event_base_dump_events(struct event_base *base, FILE *f);
	
*/
  
  
  