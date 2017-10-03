
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
    
    bufferevent_free(bev);    
}       

void socket_read_cb(bufferevent *bev, void *arg){
	printf("socket_read_cb tid = %lu \n",  pthread_self() );
    bufferevent_write_buffer(bev, bufferevent_get_input(bev));// evbuffer --> evbuffer 
}



void listener_cb(evconnlistener *listener, evutil_socket_t fd,    
                 struct sockaddr *sock, int socklen,  
				 void *arg)    
{    
	// 需要判断  sock.family  AF_INET AF_INET6
	printf("listener_cb tid = %lu \n",  pthread_self() );
	printf("sizeof(struct sockaddr_in) = %zd  socklen = %d \n" , sizeof(struct sockaddr_in) , socklen );
	printf("client addr %s port %d \n" , inet_ntoa( ((struct sockaddr_in*)sock)->sin_addr ) ,
											ntohs( ((struct sockaddr_in*)sock)->sin_port  ));
    printf("accept a client %d\n", fd);
	
    event_base *base = (event_base*)arg;    
    bufferevent *bev =  bufferevent_socket_new(base, fd,BEV_OPT_CLOSE_ON_FREE);    
    bufferevent_setcb(bev, socket_read_cb, NULL, socket_event_cb, NULL);
    bufferevent_enable(bev, EV_READ | EV_PERSIST ); 
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
	evthread_use_pthreads();
    
    event_base *base = event_base_new();
	
	const char ** all_methods = event_get_supported_methods();
	const char ** one_methods = all_methods;
	while( *one_methods != NULL ){
		printf("method '%s' support\n", *one_methods);
		one_methods++;
	}
	const char* current_method = event_base_get_method(base);
	int current_feature = event_base_get_features(base);
	printf("current_method = %s current_feature = 0x%x \n", current_method , current_feature ); 	

	
	struct sockaddr_in sin;    
    memset(&sin, 0, sizeof(struct sockaddr_in));    
    sin.sin_family = AF_INET;    
    sin.sin_port = htons(9999);   
    evconnlistener *listener  = evconnlistener_new_bind(base, listener_cb, base,    
                                      LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,    
                                      10, // backlog 
									  (struct sockaddr*)&sin,    
                                      sizeof(struct sockaddr_in));

	evconnlistener_set_error_cb(listener, accept_error_cb);	
 

    event_base_dispatch(base);    
    
 	if( event_base_got_exit(base) ){
		printf("exit event_base \n");
	}
	if( event_base_got_break(base) ){
		printf("break event_base \n");
	}
  
    evconnlistener_free(listener);    
    event_base_free(base);    
    
    return 0;    
}    
 
 