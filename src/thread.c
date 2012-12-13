/*
 * thread.h
 * tcp_proxy
 *
 *  Thread management 
 *  Created by Alexandre Kalendarev on 03.12.12.
 */
#include <sys/socket.h>
#include <ev.h>

#include <stdio.h>
#include "main.h"
#include "thread.h"
#include "client.h"

//void on_connect(EV_P_ ev_io *io, int revents); 

//static void on_timeout(EV_P_ ev_io *io, int revents);
//static void * on_signal(void *arg);

extern FILE 			*flog;
extern int				max_clients;
extern fd_ctx			*clients;

static ev_thread_t *threads;

/*
 * Number of worker threads that have finished setting themselves up.
 */
static int init_count = 0;

static pthread_mutex_t init_lock;
static pthread_mutex_t accept_lock;
static pthread_cond_t init_cond;
static pthread_mutex_t isset_lock;
static pthread_mutex_t set_lock;
static pthread_mutex_t stat_lock;


void
on_connect(EV_P_ ev_io *io, int revents) {
		
	while (1) {
		
				//int sock = accept(fd, (sockaddr*)(&client_addr), &len);
				
		pthread_mutex_lock(&accept_lock);		
		int client = accept(io->fd, NULL, NULL);		
		 pthread_mutex_unlock(&accept_lock);
		 
		if (client >= 0) {				
			ev_io *mctx = client_new(client);
			if (mctx) {
				ev_io_start(EV_A_ mctx);
			} else {
				printf( "failed to create connections context %s", strerror(errno));
				close(client);
			}
		} else {
			if (errno == EAGAIN)
				return;
			if (errno == EMFILE || errno == ENFILE) {
				printf("out of file descriptors, dropping all clients. %s", strerror(errno));
	//			close_all(EV_A);
			} else if (errno != EINTR) {
				printf("accept_connections error: %s", strerror(errno));
			}
		}
		
	}
	
	
}


/*
 * Worker thread: main event loop
 */
static void *libev_worker(void *arg) {
    ev_thread_t *me = (ev_thread_t *)arg;

    /* Any per-thread setup can happen here; thread_init() will block until
     * all threads have finished initializing.
     */

    pthread_mutex_lock(&init_lock);
    init_count++;
    pthread_cond_signal(&init_cond);
    pthread_mutex_unlock(&init_lock);

	printf("%s init_count=%d\n", __FUNCTION__, init_count);

	
	me->loop = ev_default_loop(0);
    //event_base_loop(me->base, 0);
	ev_io_init(&me->io, on_connect, me->receive_fd, EV_READ);
	ev_io_start(me->loop, &me->io);

//	ev_async_init(&me->async, on_connect, me->receive_fd, EV_READ);
//	ev_async_start(me->loop, &me->async);
	
//	  ev_async_init (&me->async, on_connect);
//      ev_async_start (me->loop, &me->async);	

//	ev_loop(EV_A_ 0);
	ev_loop(me->loop, 0);
		
    return NULL;
}


/*
 * Creates a worker thread.
 */
static void create_worker(void *(*func)(void *), void *arg) {
    pthread_t       thread;
    pthread_attr_t  attr;
    int             ret;

    pthread_attr_init(&attr);

    if ((ret = pthread_create(&thread, &attr, func, arg)) != 0) {
        perror( "Can't create thread: %s\n");
        exit(1);
    }
	
	printf("worker created\n");
}

/*
 * Set up a thread's information.
 */
static void setup_thread(ev_thread_t *me) {
/*
    me->base = event_init(); // 
    if (! me->base) {
        fprintf(stderr, "Can't allocate event base\n");
        exit(1);
    }

    // Listen for notifications from other threads 
    event_set(&me->notify_event, me->notify_receive_fd,
              EV_READ | EV_PERSIST, on_connect, me);
    
	event_base_set(me->base, &me->notify_event);

*/


//	if ((pthread_mutex_init(&me->mutex, NULL) != 0)) {
//        printf(stderr, "Failed to initialize mutex: %s\n", strerror(errno));
//        exit(EXIT_FAILURE);
//    }	

}


static void setup_time(ev_thread_t *me) {
//	struct timeval tv;
//	//struct event timeout;
//	
//	me->loop = event_init();
//    
//	if (! me->base) {
//        fprintf(stderr, "Can't allocate event base\n");
//        exit(1);
//    }
//
//	me->timeout = (event *)malloc(sizeof(struct event));
//	if (! me->timeout) {
//        fprintf(stderr, "Can't allocate struct event timeout\n");
//        exit(1);
//    }
//
//    event_set(me->timeout, -1,0 , on_timeout, me);
//    
//	evutil_timerclear(&tv);
//	tv.tv_sec = 1;
//
//    if (event_add(me->timeout, &tv) == -1) {
//        fprintf(stderr, "Can't add libevent timer\n");
//        exit(1);
//    }

}

/**
* Initialize the signal mask
*/
void setup_signal(void * arg) {

	sigset_t mask;
	struct sigaction * sigact = (struct sigaction *) arg;
	struct sigaction sa = *sigact;

	sigemptyset(&sa.sa_mask);
	sigact->sa_flags = 0;
	sigact->sa_handler = SIG_IGN;
	
	if (sigaction(SIGPIPE,sigact,NULL)<0) {
		perror("sigaction error");
	}
	
	sigemptyset(&mask);
	sigaddset(&mask, SIGHUP);	
	sigaddset(&mask, SIGTERM);	
	
	if ( pthread_sigmask(SIG_BLOCK, &mask,NULL) != 0) {
		perror("pthread_sigmask error");
	}

}

/*
 * Initializes the thread subsystem, creating various worker threads.
 *
 * nthreads  Number of worker event handler threads to spawn
 * main_base Event base for main thread
 */
//void thread_init( struct event_base *main_base, context * ctx) {
void 
thread_init(int fd, void * ctx) {
    int         i;
	struct sigaction sigact;

    pthread_mutex_init(&init_lock, NULL);
	pthread_mutex_init(&accept_lock, NULL);
	pthread_mutex_init(&isset_lock, NULL);
	pthread_mutex_init(&set_lock, NULL);
	pthread_mutex_init(&stat_lock, NULL);
		
    pthread_cond_init(&init_cond, NULL);
	
	int nthreads = 4;	//ctx->threads;
    threads = (ev_thread_t*) calloc(nthreads+1, sizeof(ev_thread_t));
    if (! threads) {
        perror("Can't allocate thread descriptors");
        exit(1);
    }

//	ctx->threads_ctx = threads;
    for (i = 0; i < nthreads; i++) {
		threads[i].id = i;
//		threads[i].ctx = ctx ;

        threads[i].receive_fd = fd;
//        setup_thread(&threads[i]);				
    }
	
	/* setup the threads context for timer */
//		threads[nthreads].id = i;
//		threads[nthreads].ctx = ctx;
//		setup_time(&threads[nthreads]);				
	
	/* set signal */
	setup_signal(&sigact);
		
    /* Create threads after we've done all the libevent setup. */
    for (i = 0; i < nthreads; i++) {
        create_worker(libev_worker, &threads[i]);
    }

	printf("all workers are created\n");
//	create_worker(on_signal, ctx);


    /* Wait for all the threads to set themselves up before returning. */
    pthread_mutex_lock(&init_lock);
    while (init_count < nthreads) {
		printf("waiting the thread %d\n", init_count );
		pthread_cond_wait(&init_cond, &init_lock);
    }
    pthread_mutex_unlock(&init_lock);
	printf("waiting finish the thread creating\n");

}

  /* delete all threads allocations*/	
void free_ctx(void * ctx) {
	int i=0;
//	for (i=0; i < ctx->thread_count; i++) {
//		//event_free(ctx->threads_ctx[i]);
//	}
	//free(ctx->threads_ctx);
  }
