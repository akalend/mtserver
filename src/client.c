/*
 * thread.c
 * tcp_server
 *
 *  Connect management 
 *  Created by Alexandre Kalendarev on 01.12.12.
 */

#ifndef __proxe_mc__
#define __proxe_mc__
#endif
#ifdef __proxe_mc__

#include <sys/stat.h>
#include <sys/time.h>
#include "main.h"
#include "client.h"


#define BUFSIZE 512

#define OK 0
#define ERR 1
#define END 2

extern FILE 			*flog;
extern int				max_clients;
extern fd_ctx			*clients;
extern int 				is_finish;
extern int 				is_trace;

extern struct timeval t_start; 				// start timeinterval
extern struct timeval t_end;				// finish timeinterval

extern 	 struct {
	/* some stat data */
	unsigned	connections;				//  active connections (memcache clients) 
	unsigned	cnn_count;				//  count connectionss 
	unsigned	cmd_per;					//  count of commands into period	
	unsigned	cmd_count;					//  count of commands
	float		rps;						//  last count of commands per second
	float		rps_peak;					//  peak of commands per second	
	unsigned	get;						//  count of get commands
	unsigned	set;						//  count of set/append/prepend/incr/decr  commands
	unsigned	del;						//  count of delete  commands
	unsigned	inc;						//  count of increment/decrement  commands		
	unsigned	miss;						//  count of miss keys (key not found)
	time_t 		uptime;						// uptime server
	unsigned	err;						//  count of errors
} stats;

void periodic_watcher(EV_P_ ev_timer *t, int revents);

static void memcached_client(EV_P_ ev_io *io, int revents);
static void memcached_client_free(memcache_ctx *ctx);
static int setup_socket(int sock);

int num_digits(unsigned x)  
{  
    return (x < 10 ? 1 :   
        (x < 100 ? 2 :   
        (x < 1000 ? 3 :   
        (x < 10000 ? 4 :   
        (x < 100000 ? 5 :   
        (x < 1000000 ? 6 :   
        (x < 10000000 ? 7 :  
        (x < 100000000 ? 8 :  
        (x < 1000000000 ? 9 :  
        10)))))))));  
}


void close_io(EV_P_ ev_io *io)
{
	ev_io_stop(EV_A_ io);
	close(io->fd);
}

void close_all(EV_P) {
	int i;
	for (i=0; i < max_clients; i++) {
		if (clients[i].flags & FD_ACTIVE) {
			close_io(EV_A_ clients[i].io);
		}

		if(clients[i].mc_ctx) {
			if(clients[i].mc_ctx->value)
				free(clients[i].mc_ctx->value);
			clients[i].mc_ctx->value = NULL;
		
			free(clients[i].mc_ctx);
			clients[i].mc_ctx = NULL;
		}
	}
}

void cllear_mc_all()
{
	int i;
	for (i=0; i < max_clients; i++) {
		if (clients[i].mc_ctx) {
			if (clients[i].mc_ctx->value) {
				free(clients[i].mc_ctx->value);
				clients[i].mc_ctx->value = NULL;
			}	
			free(clients[i].mc_ctx);
			clients[i].mc_ctx = NULL;
		}
		//if(FD_ACTIVE) close_io(EV_A_ clients[i].io);
	}

}

void
periodic_watcher(EV_P_ ev_timer *t, int revents)
{
	gettimeofday(&t_end, NULL);
	long mtime, seconds, useconds;    

    seconds  = t_end.tv_sec  - t_start.tv_sec;
    useconds = t_end.tv_usec - t_start.tv_usec;
    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
	stats.rps = stats.cmd_per * 1000 /mtime ;
	if(stats.rps_peak < stats.rps)	stats.rps_peak = stats.rps;
	stats.cmd_per = 0;

	gettimeofday(&t_start, NULL);
}


/* Set misc socket parameters: non-blocking mode, linger, enable keep alive. */
static int 
setup_socket(int sock)
{
	int keep_alive = 1;
	struct linger l = {0, 0};
	if (set_nonblock(sock, 1) == -1) return -1;
	if (setsockopt(sock, SOL_SOCKET, SO_LINGER, &l, sizeof(l)) == -1) return -1;
	if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keep_alive, sizeof(keep_alive)) == -1) return -1;
	return 0;
}

static void
memcached_client_free(memcache_ctx *ctx)
{
	stats.connections--;	
	if(is_trace)
		printf("connection fd=%d free [%d]\n", ctx->io.fd, stats.connections);

	if (!ctx) return;

	if (ctx->value) free(ctx->value);
	free(ctx);
	clients[ctx->io.fd].mc_ctx = NULL;
}


/* Handle line-based  protocol */
static void
memcached_client(EV_P_ ev_io *io, int revents) {
	
	memcache_ctx *mctx = ( memcache_ctx*)io;
	if (revents & EV_READ) {
		int end = 0;
		int i = mctx->cmd_len ? mctx->cmd_len - 1 : 0;
				
		/* Read command till '\r\n' (actually -- till '\n') */
		size_t bytes =0;
		while (mctx->cmd_len < MAX_COMMAND_LEN && !end) {
			bytes = read(io->fd, mctx->cmd + mctx->cmd_len, MAX_COMMAND_LEN - mctx->cmd_len);
			if (bytes > 0) {

				if (bytes > BUFSIZE) {
					fprintf( flog, "%s readed=%d more as BUFSIZE\n", __FUNCTION__, (int)bytes);
					goto send_error;
				}
				mctx->cmd_len += bytes;
				while (i < mctx->cmd_len - 1) {
					if (mctx->cmd[i] == '\r' && mctx->cmd[i+1] == '\n') {
						end = i + 2;						
						mctx->cmd[i] = 0;
						break;
					}
					i++;
				}
			} else if (bytes == -1) {
				if (errno == EAGAIN) break;
				if (errno == EINTR) continue;
				goto disconnect;
			} else goto disconnect;
		}
		
		/* If there is no EOL but string is too long, disconnect client */
		if (mctx->cmd_len >= MAX_COMMAND_LEN && !end) goto disconnect;
		/* If we haven't read whole command, set timeout */
		if (!end) {
			set_client_timeout(io, RECV_TIMEOUT);
			return;
		}
		
		stats.cmd_count++;
		stats.cmd_per++;
		/* handle set command */
	
		goto send_end;	
							
	} else if (revents & EV_WRITE) {
		
		switch (obuffer_send(&mctx->response, io->fd)) {
			case 0:	
				mctx->cmd_len = 0;			
				memset(mctx->cmd, 0, MAX_COMMAND_LEN);				
				reset_client_timeout(io);
				ev_io_stop(EV_A_ io);
				if (is_finish) goto exit;
				ev_io_set(io, io->fd, EV_READ);
				ev_io_start(EV_A_ io);
				break;
			case -1:
				goto disconnect;
		}
	}
	return;
send_error:
	stats.err ++;
	obuffer_init(&mctx->response, "ERROR\r\n", sizeof("ERROR\r\n") - 1);
	goto send_reply;

send_end:

	obuffer_init(&mctx->response, "END\r\n", sizeof("END\r\n") - 1);

send_reply:
	set_client_timeout(io, RECV_TIMEOUT);
	mctx->cmd_len = 0;
	ev_io_stop(EV_A_ io);
	ev_io_set(io, io->fd, EV_WRITE);
	ev_io_start(EV_A_ io);
	return;
disconnect:
	close_io(EV_A_ io);
	memcached_client_free(mctx);

	return;
exit:
	close_all(EV_A );	
	ev_unloop(EV_A_ EVUNLOOP_ALL); 	
}

/* Create connections context */
ev_io*
client_new(int sock) {

	if(is_trace)
		printf("%s: new connection [%d]", __FUNCTION__, sock);

	if (setup_socket(sock) != -1) {
		memcache_ctx *mctx = calloc(1, sizeof(memcache_ctx));

		if (!mctx) {
			fprintf(flog, "%s: allocate error size=%d\n", __FUNCTION__, (int)sizeof(memcache_ctx));
			return NULL;
		}
		
		mctx->value = malloc(BUFSIZE);		
		if (!mctx->value) {
			fprintf(flog, "%s: allocate error size=%d\n", __FUNCTION__, BUFSIZE);
			return NULL;
		}

		ev_io_init(&mctx->io, memcached_client, sock, EV_READ);
		clients[sock].io = &mctx->io;
		clients[sock].flags = FD_ACTIVE;
		clients[sock].cleanup = (cleanup_proc)memcached_client_free;
		clients[sock].mc_ctx = mctx;

		stats.connections++;
		stats.cnn_count++;
		if(is_trace)
			printf(" Ok\n");
		return &mctx->io;
	} else {
		if(is_trace)
			printf(" Fail\n");
		return NULL;
	}
}

/* Serve connectionss */

#endif
