#include "../include/rate_limiting.h"
#include <microhttpd.h>
#include <stdio.h>
#include <libc.h>

#define NUM_THREADS 4
#define PORT 8080

static enum MHD_Result request_handler(void *cls,
				struct MHD_Connection *connection,
                           	const char *url, const char *method,
                           	const char *version, const char *upload_data,
                           	size_t *upload_data_size, void **con_cls)
{
    	struct MHD_Response *response;
	char response_text[2048];
    	int ret;
	struct puzzle *p = NULL;

	if (strncmp(url, "/request_vdf", strlen("/request_vdf")) == 0) {
		const char *elem_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "element");
		unsigned long elem = 0;
		if (!elem_str) {
    			snprintf(response_text, 2048, "missing element parameter\n");
			goto respond;
		}
		char *end;
		elem = strtoul(elem_str, &end, 10);
		if (*end != '\0') {
    			snprintf(response_text, 2048, "invalid argument\n");
			goto respond;
		}
		p = generate_puzzle(elem);
		snprintf(response_text, 2048, "x=%s;T=%s;N=%s;", p->x, p->T, p->N);
		printf("PUZZLE:\n%s\n\n", response_text);
	} else {
		snprintf(response_text, 2048, "unkown endpoint\n");
	}
respond:
    	response = MHD_create_response_from_buffer(strlen(response_text),
                                        		(void*)response_text,
                                               		MHD_RESPMEM_PERSISTENT);
    	ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    	MHD_destroy_response(response);
	if (p) {
		free(p->N);
		free(p->T);
		free(p->x);
		free(p);
	}
    	return ret;
}

int main() {
    	struct MHD_Daemon *daemon;
	rate_limiting_init();

    	daemon = MHD_start_daemon(MHD_USE_INTERNAL_POLLING_THREAD,
				PORT,
                              	NULL, NULL,
                              	&request_handler, NULL,
                              	MHD_OPTION_THREAD_POOL_SIZE, NUM_THREADS,
                             	MHD_OPTION_END);
    	if (!daemon) return 1;

   	getchar();
    	MHD_stop_daemon(daemon);
	rate_limiting_clean_up();
    	return 0;
}