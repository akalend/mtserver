/*
 * main.c
 * tcp_proxy
 *
 *  The main block
 *  Created by Alexandre Kalendarev on 05.12.12.
 */
#ifndef __proxy_main_c_
#define __proxy_main_c_
#endif
#ifdef __proxy_main_c_

#include <sys/time.h>
#include <libgen.h>
#include  <netdb.h>

#include "ini.h"
#include "main.h"
#include "client.h"
#include "thread.h"


#define CONFIGFILE  "config.ini"

#define PROXY_VERSION "0.2"

extern void 
parse(const char* fname, server_ctx_t *server_ctx);

static const char *listen_addr;
server_ctx_t server_ctx;
const char *confilename = NULL;

FILE 			*flog = NULL;
int				max_clients = 0;
fd_ctx			*clients = NULL;
int 			is_finish = 0;
int 			is_trace = 0;

struct timeval t_start; 				// start timeinterval
struct timeval t_end;					// finish timeinterval


struct {
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

} stats = {0,0,0,0,0,0,0,0,0,0,0,0,0};



static void usage(const char *binary,int exitcode)
{
	const char *name = strrchr(binary, '/');
	name = name ? name+1 : binary;
	printf("Usage: %s [options]\n"
			"default port: %s\n"
			"Options are:\n"
			"  -n, --max-clients=limit maximum number of open files (like ulimit -n)\n" 
			"  -V, --version           show version\n"
			"  -h, --help              this help message\n"
			"  -t, --trace             trace the commends\n"
			"  -c configfile           config filename or config.ini in the current dir",
			name, listen_addr);
	exit(exitcode);
};

void perror_fatal(const char *what)
{
	perror(what);
	exit(EXIT_FAILURE);
}




void free_config() {

	if (server_ctx.logfile) free(server_ctx.logfile);
	server_ctx.logfile = NULL;
	if (server_ctx.datadir) free(server_ctx.datadir);
	server_ctx.datadir = NULL;
	if (server_ctx.username) free(server_ctx.username);
	server_ctx.username = NULL;
	if (server_ctx.pidfile) free(server_ctx.pidfile);
	server_ctx.pidfile = NULL;
	if (server_ctx.listen) free(server_ctx.listen);
	server_ctx.listen = NULL;
	
	
	
}


void ignore_sigpipe() {
	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sa.sa_flags = 0;
	if (sigemptyset(&sa.sa_mask) == -1 || sigaction(SIGPIPE, &sa, 0) == -1) {
		perror("failed to ignore SIGPIPE; sigaction");
		exit(EXIT_FAILURE);
	}
}

static void 
set_rlimit() {
	struct rlimit r;
	if (max_clients) {
		r.rlim_cur = r.rlim_max = max_clients;
		if (setrlimit(RLIMIT_NOFILE, &r) == -1) {
			perror_fatal("setrlimit");
		}
	} else {
		if (getrlimit(RLIMIT_NOFILE, &r) == -1) {
			perror_fatal("getrlimit");
		}
		max_clients = r.rlim_cur;
	}
	
}

static int
init_addr( addr_t *addr, const char *astring) {

	addr->a_addr = strdup(astring);
	/* Make correct sockaddr */
	if (strncmp(astring,"file:",5) == 0) {
		/* local namespace */
		struct sockaddr_un *name = &addr->un_name;
		addr->pf_family = PF_LOCAL;
		name->sun_family = AF_LOCAL;
		strncpy (name->sun_path, astring+5, sizeof(name->sun_path));
		name->sun_path[sizeof(name->sun_path)-1] = '\0';
		addr->namelen = SUN_LEN(name);
	} else {
		/* inet namespace */
		struct sockaddr_in *name = &addr->in_name;
		char *colon = strchr(addr->a_addr, ':');
		addr->pf_family = PF_INET;
		if (colon || isdigit(*addr->a_addr)) {
			if (colon && colon != addr->a_addr) {
				struct hostent *hostinfo;
				*colon = 0;
				hostinfo = gethostbyname (addr->a_addr);
				if (hostinfo == NULL) {
					free (addr->a_addr);
					addr->a_addr=NULL;
					return 1;
				}
				*colon = ':';
				name->sin_addr = *(struct in_addr*)hostinfo->h_addr;
			} else {
				name->sin_addr.s_addr = htonl(INADDR_ANY);
			}
			name->sin_family = AF_INET;
			name->sin_port = htons(atoi(colon ? colon+1 : addr->a_addr));
			addr->namelen = sizeof(struct sockaddr_in);
		} else {
			free (addr->a_addr);
			addr->a_addr=NULL;
			return 1;
		}
	}
	return 0;
}

int
set_nonblock(int sock,int value)
{
	long fl = fcntl(sock, F_GETFL);
	if (fl == -1) return fl;
	if (value) fl |= O_NONBLOCK;
	else fl &= ~O_NONBLOCK;
	return fcntl (sock, F_SETFL, fl);
}

/**
*	signal SIGHUP callback
*/
  static void
  sighup_cb (struct ev_loop *loop, struct ev_signal *w, int revents)
  {
	stats.cmd_count = 0;
	stats.get = 0;
	stats.set = 0;
	stats.del = 0;
	stats.miss = 0;
	stats.err = 0;

	time(&stats.uptime);

	is_finish = 1;
	if (!max_clients) ev_unloop (loop, EVUNLOOP_ALL);
	if (!stats.connections) ev_unloop (loop, EVUNLOOP_ONE);
	is_finish = 0;

// read config again and setup

	ev_loop(loop,0);
	
  }

/**
*	signal SIGINT callback
*/
  static void
  sigint_cb (struct ev_loop *loop, struct ev_signal *w, int revents)
  {
  //  printf("recv signal SIGINT  cnn=%d\n", stats.connections);	
	is_finish = 1;

	if (!stats.connections) {
//		printf("cnn = 0\n");
		ev_unloop (loop, EVUNLOOP_ALL);
		return;
	}
  }
  

static void 
listen_sock(int mc_sock, addr_t *mc_addr) {
	{
		// set sock option
		int flag = 1;
		setsockopt(mc_sock, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(flag));
		long fl = fcntl(mc_sock, F_GETFL);
		if (fl == -1) perror_fatal("can't get fcntl sock option");

		set_nonblock(mc_sock,1);
	}
	
	
	if ( bind(mc_sock, &mc_addr->name, mc_addr->namelen) < 0) perror_fatal("bind");	
	if (listen(mc_sock, 10) < 0) perror_fatal("publisher listen");	
}  
  
static void 
daemonize(int do_daemonize, const char *pidfile, const char *run_as)
{
	int fd;

	/* Daemonize part 1 */
	if (do_daemonize) {
		switch (fork()) {
			case -1:
				perror("fork");
				exit(1);
			case 0:
				break;
			default:
				exit(EXIT_SUCCESS); 
		}

		if (setsid() == -1) perror_fatal("setsid");
	}

	/* Change effective uid/gid if requested */
	if (run_as) {
		struct passwd *pw = getpwnam(run_as);
		if (!pw) {
			fprintf(stderr, "No such user: \"%s\"\n", run_as);
			exit(EXIT_FAILURE);
		}
		if (setgid(pw->pw_gid) < 0 || setuid(pw->pw_uid) < 0) {
			fprintf(stderr, "Can't switch to user \"%s\": %s\n", run_as, strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	/* Save PID file if requested */
	if (pidfile) {		
		FILE *fpid = fopen(pidfile, "w");
		if (!fpid) perror_fatal("Can't create pid file");
		fprintf(fpid, "%ld", (long)getpid());
		fclose(fpid);
	}



	/* Daemonize part 2 */
	if (do_daemonize) {
		if(chdir("/") != 0) perror_fatal("chdir");

		if ((fd = open("/dev/null", O_RDWR, 0)) != -1) {
			if(dup2(fd, STDIN_FILENO) < 0) perror_fatal("dup2 stdin");
			if(dup2(fd, STDOUT_FILENO) < 0) perror_fatal("dup2 stdout");
			if(dup2(fd, STDERR_FILENO) < 0) perror_fatal("dup2 stderr");

			if (fd > STDERR_FILENO && close(fd) < 0) perror_fatal("close");
		} else {
			perror_fatal("open(\"/dev/null\")");			
		}
	}
}
  
  
  
int main(int argc, char **argv){

	int mc_sock;
	addr_t mc_addr;	
//	ev_io  mc_io;
	int c;	
	
	
	while (1) {
		int option_index = 0;
		static struct option long_options[] = {
			{"help",		no_argument,		0, 'h'},
			{"version",		no_argument,		0, 'V'},
			{"max-clients",	required_argument,	0, 'l'},
			{0, 0, 0, 0}
		};
		c = getopt_long(argc, argv, "n:Vc:t", long_options, &option_index);
		if (c == -1)
			break;
		switch (c) {
			case 'c':
				confilename = optarg;
				break;
			case 'n':
				max_clients = atoi(optarg);
				break;
			case 'V':
				printf("Version %s\n",  PROXY_VERSION);
				exit(0);
			case 't':
				is_trace = 1;
				break;			default:
				usage(argv[0], c == 'h' ? EXIT_SUCCESS : EXIT_FAILURE);
		}
	}
	

	if (confilename) 
		parse(confilename, &server_ctx);
	else	
		parse( CONFIGFILE, &server_ctx);

	assert(server_ctx.logfile);	
	
	if (!server_ctx.listen) {
		perror("undefined listen port");
		exit(1);
	}
	
	flog =  server_ctx.logfile ? fopen(server_ctx.logfile, "a+") : fopen("error.log", "a+");
	
	if (flog) {		
		time_t lt;
  		lt = time(NULL);
  		fprintf(flog, "server started at %s\n",ctime(&lt));	
	}	else {
			perror("can not create log file");
			exit(1);
		}

	daemonize(server_ctx.is_demonize, server_ctx.pidfile, server_ctx.username);

	set_rlimit();	
	ignore_sigpipe();
		
	init_addr(&mc_addr, server_ctx.listen);

	mc_sock = socket(mc_addr.pf_family, SOCK_STREAM, 0);
	if (mc_sock < 0) {
		perror("can't create socket");
		exit(1);
	}

	listen_sock(mc_sock, &mc_addr);
	
	//TODO	
	if (is_trace)
		printf("pid=%ld\n\n", (long)getpid());
	
	clients = calloc(max_clients, sizeof(fd_ctx));
	if (!clients) {
		perror_fatal("Cannot allocate array for client descriptors!");		
	}

	
	//struct ev_loop *loop = ev_default_loop(0);	
	//assert(loop);
	
	
	thread_init(mc_sock, &server_ctx); 
	
//	ev_set_userdata(loop,(void*)like_ctx);	


		
//	ev_io_init(  &mc_io, memcached_on_connect, mc_sock, EV_READ);		
//	ev_io_start(loop, &mc_io);

//	struct ev_timer timeout_watcher;	
//
//	ev_init(&timeout_watcher, periodic_watcher);
//	timeout_watcher.repeat = TIME_CHECK_INTERVAL;
//	ev_timer_again(loop, &timeout_watcher);

//	struct ev_signal signal_watcher,signal_watcher2;
	
//	ev_signal_init (&signal_watcher, sigint_cb, SIGINT);
//	ev_signal_start (loop,  &signal_watcher);
	
//	ev_signal_init (&signal_watcher, sigint_cb, SIGTERM);
//	ev_signal_start (loop,  &signal_watcher);
//
//	ev_signal_init (&signal_watcher2, sighup_cb, SIGHUP);
//	ev_signal_start (loop,  &signal_watcher2);
	

	//start server
	time(&stats.uptime);
	gettimeofday(&t_start, NULL);

	// event loop
//	ev_loop(loop, 0);
	
	cllear_mc_all();	
	close(mc_sock);	
	
	if (clients) 
		free(clients);
	
	//ev_loop_destroy(loop);	
	
	if (mc_addr.a_addr) free(mc_addr.a_addr);
	
	if (server_ctx.pidfile) {		
		if( unlink(server_ctx.pidfile))
			printf("cannot delete pid file %s %s\n",server_ctx.pidfile, strerror(errno));
	}	

	free_config(); 
	if (flog) {		
		time_t lt;
  		lt = time(NULL);
  		
  		fprintf(flog, "server finis Ok at %s\n",ctime(&lt));
		fclose(flog);	
	}

	return 0;
	 
}

#endif
