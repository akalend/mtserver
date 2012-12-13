// example
//  https://github.com/ellzey/libevhtp/blob/master/examples/thread_design.c
//

#ifndef __proxy_main__
#define __proxy_main__
#endif
#ifdef __proxy_main__


#include <unistd.h>
#include <fcntl.h>

#include <ev.h>
#include <sys/un.h>


#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <unistd.h>
#include <sys/types.h> 

#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <ctype.h>
#include <signal.h>
#include <sys/resource.h>
#include <syslog.h>
#include <sys/types.h>
#include <pwd.h>
#include <getopt.h>

#include <assert.h>
#include <string.h>

#include <tchdb.h>
#include <tcutil.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <strings.h>
#include <time.h>

#include "io_buffer.h"

#define MAX_COMMAND_LEN		128


void perror_fatal(const char *what);

#endif
