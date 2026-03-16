#include "../include/rate_limiting.h"
#include "../include/service.h"
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
	int ret_status;
	struct puzzle *p = NULL;
	struct request_solve *s = NULL;

	/*
	 * unused, silent compiler warnings:
	 */
	(void) cls;
  	(void) method;
  	(void) version;
  	(void) upload_data;
  	(void) upload_data_size;
  	(void) con_cls;  

	if (strncmp(url, "/request_vdf", strlen("/request_vdf")) == 0) {
		const char *elem_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "element");
		unsigned long elem = 0;
		if (!elem_str) {
    			snprintf(response_text, 2048, "missing element parameter\n");
			ret_status = MHD_HTTP_BAD_REQUEST;
			goto respond;
		}
		char *end;
		elem = strtoul(elem_str, &end, 10);
		if (*end != '\0') {
    			snprintf(response_text, 2048, "invalid argument\n");
			ret_status = MHD_HTTP_BAD_REQUEST;
			goto respond;
		}
		p = generate_puzzle(elem);
		snprintf(response_text, 2048, "x=%s;T=%s;N=%s;", 
						p->x, p->T, p->N);
		ret_status = MHD_HTTP_OK;
		goto respond;
	} else if (strncmp(url, "/solve", strlen("/solve")) == 0) {
		s = malloc(sizeof(struct request_solve));
		const char *elem_str = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "element");
		unsigned long elem = 0;
		if (!elem_str) {
    			snprintf(response_text, 2048, "missing element parameter\n");
			ret_status = MHD_HTTP_BAD_REQUEST;
			goto respond;
		}
		const char *y_arg = MHD_lookup_connection_value(connection, MHD_GET_ARGUMENT_KIND, "vdf_solve");
		char *end;
		elem = strtoul(elem_str, &end, 10);
		if (*end != '\0') {
    			snprintf(response_text, 2048, "invalid argument\n");
			ret_status = MHD_HTTP_BAD_REQUEST;
			goto respond;
		}
		s->element = elem;
		s->y = y_arg;

		if (verify_puzzle(s)) {
			sprintf(response_text, "Request:%s;Result:%s;", elem_str, service(elem));
			ret_status = MHD_HTTP_OK;
			goto respond;
		} else {
			sprintf(response_text, "VDF verification failed\n");
			ret_status = MHD_HTTP_FORBIDDEN;
			goto respond;
		}
	} else {
		snprintf(response_text, 2048, "unkown endpoint\n");
		ret_status = MHD_HTTP_BAD_REQUEST;
		goto respond;
	}
respond:
    	response = MHD_create_response_from_buffer(strlen(response_text),
                                        		(void*)response_text,
                                               		MHD_RESPMEM_PERSISTENT);
    	ret = MHD_queue_response(connection, ret_status, response);
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