#include <event2/event.h>
#include <event2/bufferevent.h>
#include <event2/buffer.h>
#include <event2/util.h>


#include <netinet/in.h> 
#include <arpa/inet.h>

#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>



struct info {
    const char *name;
    size_t total_drained;
};

void read_cmd_callback(int fd, short events/*事件的类型*/, void* arg){
	
	printf("Got an event on fd %d:%s%s%s%s\n",
		(int) fd, 	(events&EV_TIMEOUT) ? " timeout" : "",
					(events&EV_READ)    ? " read" : "",
					(events&EV_WRITE)   ? " write" : "",
					(events&EV_SIGNAL)  ? " signal" : "" );
			
    char msg[1024];

    int ret = read(fd, msg, sizeof(msg)-1 ); // non-block
    if( ret < 0 )  {
        perror("Read Fail!");
        return ;
    }
	msg[ret] = '\0' ;
    struct bufferevent* bev = (struct bufferevent*)arg;

	struct evbuffer* output = bufferevent_get_output(bev);
	
	if( ! strncmp(msg,"write" , strlen("write") ) ){
		printf("INFO:enable write buffer now!\n");
		bufferevent_enable(bev,EV_WRITE); // enable之后会把之前的write的数据一次写到fd 
	}
	
	if( ! strncmp(msg,"read" , strlen("read") ) ){
		printf("INFO:enable read buffer now!\n");
		bufferevent_enable(bev,EV_READ);  
	}
	
    //bufferevent_write(bev, msg, ret);
	evbuffer_add_printf(output,">>%s", msg); // 这里会加多两个字节
	
	/* 	
	
	应用程序	只能从input buffer中移走(而不是添加)数据
				只能向output buffer添加(而不是移走)数据
				
	如果bufferevent上的写操作因为数据太少而停滞(或者读操作因为数据太多而停滞)，
	则向output buffer中添加数据(或者从input buffer中移走数据)
	可以自动重启写(读)操作 

	
	我们可以在bufferevent上enable或者disable事件 EV_READ, EV_WRITE, or EV_READ|EV_WRITE 
	
	当disable了读写操作 则bufferevent不会读取或写入数据 

	当output buffer 为空时 没必要disable写动作 bufferevent会自动禁止掉写动作 
	当输入缓冲区达到它的高水位线的时候 没必要禁止读操作 bufferevent会自动停止读操作 而且在有空间读取的时候 又重新开启读操作 

	默认情况下 新创建的bufferevent会enable写操作 而禁止读操作 

	
	size_t bufferevent_read(struct bufferevent *bufev, void *data, size_t size);
	-- data缓冲区必须有足够的空间保存size个字节
	
	int  bufferevent_write_buffer(struct  bufferevent *bufev,  struct  evbuffer*buf);
	-- 将buf中所有数据都移动到output buffer的末尾

	while (1) {
		n = bufferevent_read(bev, tmp, sizeof(tmp));
		if (n <= 0) // 一直读到没有 
				break;  
		for (i=0; i<n; ++i)
				tmp[i] = toupper(tmp[i]);
		bufferevent_write(bev, tmp, n);
	}
		
	*/
	
}


void read_callback(struct bufferevent *bev, void *ctx)
{
    struct info *inf = (struct info *)ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
    size_t len = evbuffer_get_length(input);
    if (len) {
        inf->total_drained += len;
        //devbuffer_drain(input, len);
		char buf[1024];
		int n = 0 ;
		 while ((n = evbuffer_remove(input, buf, sizeof(buf))) > 0) {
			fwrite(buf, 1, n, stdout);
		}
        printf("\nDrained %lu bytes from %s\n",  (unsigned long) len, inf->name);
    }
}

void event_callback(struct bufferevent *bev, short events, void *ctx)
{
    struct info *inf = (struct info *)ctx;
    struct evbuffer *input = bufferevent_get_input(bev);
    int finished = 0;

	if (events & BEV_EVENT_CONNECTED) { // connect socket 已经连接上 
         printf("Connect okay.\n");
    } 
	
	if (events &(BEV_EVENT_TIMEOUT|BEV_EVENT_READING) ){ // 读超时 
		printf("read timeout !\n");
	}
	
	if (events & (BEV_EVENT_TIMEOUT|BEV_EVENT_WRITING) ){ // 写超时
		printf("write timeout !\n");
	}
	
    if (events & BEV_EVENT_EOF) {
        size_t len = evbuffer_get_length(input);
        printf("Got a close from %s.  We drained %lu bytes from it, "
            "and have %lu left.\n", inf->name,
            (unsigned long)inf->total_drained, (unsigned long)len);
		// 关闭的时候 里面还有多少数据没有读取完毕
        finished = 1;
    }
    if (events & BEV_EVENT_ERROR) {
        printf("Got an error from %s: %s\n",
            inf->name, evutil_socket_error_to_string(EVUTIL_SOCKET_ERROR()));
        finished = 1;
    }
    if (finished) {
        free(ctx);
        bufferevent_free(bev);
    }
}

struct bufferevent * setup_bufferevent(struct event_base*base , const short port , const char* addr )
{
    struct bufferevent *bev = NULL;
    struct info *info1;

    info1 = (struct info *)malloc(sizeof(struct info));
    info1->name = "buffer 1";
    info1->total_drained = 0;

    /* ... Here we should set up the bufferevent and make sure it gets
       connected... */

	struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr) );
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons( port );
    inet_aton(addr, &server_addr.sin_addr);
	
	printf("connect %s:%d\n",addr , port  );
	
	bev = bufferevent_socket_new(base, -1,   BEV_OPT_CLOSE_ON_FREE);
	bufferevent_socket_connect(bev, (struct sockaddr *)&server_addr,  sizeof(server_addr));
		   
    /* Trigger the read callback only whenever there is at least 128 bytes
       of data in the buffer. */
    //bufferevent_setwatermark(bev, EV_READ, 36, 0); // 只有读取到36个字节 才会回调!

    bufferevent_setcb(bev, read_callback, NULL, event_callback, info1);
    bufferevent_enable(bev, EV_READ);    // 默认情况 没使能写功能
	//bufferevent_disable(bev,EV_WRITE);// 默认情况 使能写功能  所以可以通过evbuffer_add_printf等写数据到fd 
    
	//const struct timeval timeout = {2,0};
	//bufferevent_set_timeouts(bev ,&timeout, &timeout);
	
	/*
		只有在bufferevent读或写的时候，才会对超时时间进行计时。
		

		如果bufferevent上禁止了读操作，或者当输入缓冲区满（达到高水位线）时，
				则读超时时间不会使能。
		
		如果写操作未被使能，或者没有数据可写，
				则写超时时间也会被禁止。
				
		也就是超时 是 fd/socket 没有数据过来(读超时) 或者 数据不能传送出去(写超时)
	
		当读或写超时发生的时候，则bufferevent上相应的读写操作就会被禁止
		
		event回调函数就会以
		BEV_EVENT_TIMEOUT|BEV_EVENT_READING
		或
		BEV_EVENT_TIMEOUT|BEV_EVENT_WRITING
		进行调用
	*/
	
	struct timeval tick = {1,0};
	struct ev_token_bucket_cfg * cfg = ev_token_bucket_cfg_new(
        20/*平均*/, 30/*一个tick的最大传输数量*/, 
        20, 30,// 这样会导致  很长的数据  对方会先收到前面的30个字节 再收到后面的数据
        &tick );
	if( cfg == NULL) printf("ERROR ev_token_bucket_cfg !\n");
	bufferevent_set_rate_limit(bev, cfg );	
	ev_token_bucket_cfg_free( cfg ); 
	/*
		速率限制模型
		
		libevent的速率限制使用记号存储器(token bucket)算法确定在某时刻可以写入或者读取多少字节
		
		每个速率限制对象在任何给定时刻都有一个读存储器(read bucket)和一个写存储器(write bucket)
		其大小决定了对象允许立即读取或者写入多少字节
		
		每个bucket有一个填充速率，一个最大突发尺寸，和一个时间单位，或者说“滴答 tick ”
		
		填充速率决定了对象发送或者接收字节的最大平均速率
		突发尺寸决定了在单次突发中可以发送或者接收的最大字节数
		时间单位则确定了传输的平滑程度
		
		如果tick_len参数为NULL，则默认的滴答长度为一秒。 
		read_rate和write_rate参数的单位是字节每滴答。
		
		也就是说，如果滴答长度是十分之一秒，read_rate是300，则最大平均读取速率是3000字节每秒

	*/
	
	
	return bev;
}

int main(int argc, char** argv){

	if( argc < 3 ){
		printf("please input 2 parameter  ./client <server_ip_addr>  <server_port> \n"); 
		return -1;
    }

	struct event_base* base = event_base_new();
	
	struct bufferevent* bev = setup_bufferevent(base, atoi(argv[2]), argv[1] );
	
	evutil_make_socket_nonblocking(STDIN_FILENO);
	struct event* ev_cmd = event_new(base, STDIN_FILENO,  EV_READ | EV_PERSIST,  read_cmd_callback, (void*)bev);
    event_add(ev_cmd,NULL);
	event_base_dispatch(base);
	
	void* arg = NULL;
	bufferevent_getcb(bev,NULL,NULL,NULL,&arg);
	free(arg); arg=NULL;
	
	event_free(ev_cmd);
	event_base_free(base);
	
	return 0;
}


