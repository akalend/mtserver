/*
 * mc.h
 * tcp_server
 *
 *  Connect management 
 *  Created by Alexandre Kalendarev on 01.12.12.
 */

#ifndef __likes_mch__
#define __likes_mch__
#endif
#ifdef __likes_mch__

#include <sys/socket.h>
#include <ev.h>

#define set_client_timeout(io,t_o) clients[(io)->fd].timeout = ev_now(EV_A) + t_o
#define reset_client_timeout(io) clients[(io)->fd].timeout = 0


#define	FD_ACTIVE		1					/**< this descriptor belongs to active connection (publisher or subscriber) */
#define FD_BROADCAST	2					/**< this is active subscriber connection (swf or long polling) */
#define FD_LPOLL		4					/**< this is long polling connection */
#define FD_SWF			8					/**< this is swf connection */
#define FD_WAIT			16					/**< this is long polling connection that awaits for data */

#define	swf_hashsize	65537

#define	TIMEOUT_CHECK_INTERVAL	10
#define	TIME_CHECK_INTERVAL	5

#define	RECV_TIMEOUT	5

#define	LPOLL_TIMEOUT			25		/* interval of time after which client will get 304 not modified response */


typedef void (*cleanup_proc)(ev_io *data);

typedef struct {
	void * next;
	int number;
	char* comment; 	// the data type information
	int type;		// 
	int link;
} datatype_t;

typedef struct {
	char* 		logfile;
	int 		level; // error output level	
	char* 		listen;
	char* 		pidfile;
	char * 		username;
	short 		is_demonize;	
	short 		trace;
	char* 		datadir;
	int 		list_size;
	int 		max_num;
	int 		thread_count;
	datatype_t* list_datatypes;
	int 		recsize;
	int 		counter_bucket;
	int 		index_bucket;
} server_ctx_t;


typedef struct {
	union {
		struct sockaddr		name;
		struct sockaddr_in	in_name;
		struct sockaddr_un	un_name;
	};
	int			pf_family;
	socklen_t	namelen;
	char		*a_addr;
} addr_t;

typedef struct {
	ev_io		io;							/**< io descriptor */
	char		cmd[MAX_COMMAND_LEN];		/**< buffer for line-buffered input */
	int			cmd_len;					/**< bytes in line buffer */
	struct		obuffer response;			/**< response data */
	char		*value;						/**< key value from last set command */
	int			value_len;					/**< number of bytes in value buffer */
	int			value_size;					/**< capacity of value buffer */	
	int			free_value;					/**< response value needs to be freed */
	int			data_size;					/**< value   size into cmd */	
	unsigned	flag;					/**< request flag read */
	unsigned	exptime;					/**< request expire read */
	char*		key;					/**< request key read */
	int 		mode;
} memcache_ctx;

typedef struct  {
	ev_tstamp					timeout;		/**< time, when this socked should be closed. */
	int							flags;			/**< flags mask */
	cleanup_proc				cleanup;		/**< cleanup handler */
	struct timeval				time;
	union {
		ev_io					*io;			/**< private data */
		memcache_ctx			*mc_ctx;		/**< publisher */
	};
} fd_ctx;


void cllear_mc_all();

void 
close_io(EV_P_ ev_io *io);

void 
close_all(EV_P);


/* Serve  */
extern ev_io* 
client_new(int sock);


void
periodic_watcher(EV_P_ ev_timer *t, int revents);


/*  declared server.c */
int
set_nonblock(int sock,int value);

#endif
