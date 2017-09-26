
/*
 
g++ server.cpp -o server  -levent -L/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib -I/home/hanlon/Cat6/libevent-2.0.22-stable/install/include -Wl,-rpath=/home/hanlon/Cat6/libevent-2.0.22-stable/install/lib 

*/


#include<netinet/in.h>    
#include<sys/socket.h>    
#include<unistd.h>
    
#include<stdio.h>    
#include<string.h>    
 
#include<event.h>
#include<event2/listener.h>
#include<event2/bufferevent.h>
 
void socket_event_cb(bufferevent *bev, short events, void *arg)    
{    
    if (events & BEV_EVENT_EOF)    
        printf("connection closed\n");    
    else if (events & BEV_EVENT_ERROR)    
        printf("some other error\n");    
    
    //�⽫�Զ�close�׽��ֺ�free��д������    
    bufferevent_free(bev);    
}       

void socket_read_cb(bufferevent *bev, void *arg){
	
    char msg[4096];
    size_t len = bufferevent_read(bev, msg, sizeof(msg)-1 );	//	bufferevent_read ��
    msg[len] = '\0';
    printf("server read the data %s\n", msg);

    char reply[] = "I has read your data";
    bufferevent_write(bev, reply, strlen(reply) );				//	bufferevent_write д
}



//һ���¿ͻ��������Ϸ�������    
//���˺���������ʱ��libevent�Ѿ�������accept������ͻ��ˡ��ÿͻ��˵��ļ�������Ϊfd    
void listener_cb(evconnlistener *listener, evutil_socket_t fd,    
                 struct sockaddr *sock, int socklen, void *arg)    
{    
    printf("accept a client %d\n", fd);    
    event_base *base = (event_base*)arg;    
    //Ϊ����ͻ��˷���һ��bufferevent    
    bufferevent *bev =  bufferevent_socket_new(base, fd,BEV_OPT_CLOSE_ON_FREE);    
    
    bufferevent_setcb(bev, socket_read_cb, NULL, socket_event_cb, NULL);    
    bufferevent_enable(bev, EV_READ | EV_PERSIST);    
}    


int main()    
{    
    //evthread_use_pthreads();//enable threads    
    
    struct sockaddr_in sin;    
    memset(&sin, 0, sizeof(struct sockaddr_in));    
    sin.sin_family = AF_INET;    
    sin.sin_port = htons(9999);    
    
	/*
		����� ʹ�� evconnlistener_new_bind ��� ����socket ���ƶ���ַ  ����  ��������������ʱ���accept 
		�ͻ��� ʹ�� bufferevent_socket_connect ��� ����socket �󶨵�ַ ���ӷ����� 
	*/
    event_base *base = event_base_new();    
    evconnlistener *listener    
            = evconnlistener_new_bind(base, listener_cb, base,    
                                      LEV_OPT_REUSEABLE|LEV_OPT_CLOSE_ON_FREE,    
                                      10, (struct sockaddr*)&sin,    
                                      sizeof(struct sockaddr_in));    
    
    event_base_dispatch(base);    
    
    evconnlistener_free(listener);    
    event_base_free(base);    
    
    return 0;    
}    
    
    
