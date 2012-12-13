/*
 * thread.h
 * tcp_proxy
 *
 *  Thread management 
 *  Created by Alexandre Kalendarev on 03.12.12.
 */
#include <pthread.h>


#ifndef __PROXY_THREADS__ 
#define __PROXY_THREADS__

typedef struct {
	int id;						/* number of this thread */
    pthread_t thread_id;        /* unique ID of this thread */
	struct ev_loop *loop;		/* libev loop thread uses */
	struct ev_io  io;					/* libev io thread uses */
    int receive_fd;				/* receiving fd */
	struct server_cxt_t * ctx;		/* the pointer to the server context*/
	struct ev_async  async;
//	static ev_thread_t *threads_ctx;
//	struct event *timeout;		/* libevent handle timeout */	
} ev_thread_t;

//static ev_thread_t *threads;


extern void
on_connect(EV_P_ ev_io *io, int revents);

extern void 
thread_init(int fd, void* ctx);

#endif
