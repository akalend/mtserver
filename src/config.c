#include "main.h"
#include "ini.h"
#include "client.h"

extern void parse(const char* fname, server_ctx_t *server_ctx);

int dumper(void* pctx, const char* section, const char* name,
           const char* value) {
	server_ctx_t* ctx = (server_ctx_t*) pctx;
	int tmp_int = 0;
	
    if (strcmp(section, "daemon") == 0) {
//		printf("[%s] %s=%s;\n", section, name, value);
		
		if(strcmp("daemon", name)==0) {
			sscanf(value,"%d",&tmp_int);
			ctx->is_demonize = tmp_int;			
		}
		
		if(strcmp("trace" , name)==0) {
			sscanf(value,"%d",&tmp_int);
			ctx->trace = tmp_int;
		}
		
		if(strcmp("level", name)==0) {			
			//sscanf(value,"%d",&tmp_int);
			ctx->level = 0;
			if (strcmp("error",value) ) 	ctx->level = 1;
			if (strcmp("warning",value) ) 	ctx->level = 2;
			if (strcmp("notice",value) ) 	ctx->level = 3;
		}

		if(strcmp("logfile", name)==0) {			
			ctx->logfile = strdup(value);
		}
		
		if(strcmp("pidfile", name)==0) {			
			ctx->pidfile = strdup(value);
		}
		
		if(strcmp("listen", name)==0) {			
				ctx->listen = strdup(value);
		}

		if(strcmp("username", name)==0) {			
			if (strcmp("any", value))
				ctx->username = strdup(value);
		}

		if(strcmp("datadir", name)==0) {			
			ctx->datadir = strdup(value);
		}

	}
	
	return 0;
}

void parse(const char* fname, server_ctx_t *server_ctx) {
    bzero(server_ctx, sizeof(server_ctx_t));
    ini_parse(fname, dumper, (void*)server_ctx);
    
}
