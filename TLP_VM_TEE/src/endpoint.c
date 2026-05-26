#include "../include/rate_limiting.h"
#include "../include/service.h"
#include <microhttpd.h>
#include <stdio.h>

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

	/*
	 * This handles a request to "http:<ip>/request_TLP?element=<elem>"
	 * The element argument is extracted and a puzzle is generated and
	 * returned in the response text.
	 */
	if (strncmp(url, "/request_TLP", strlen("/request_TLP")) == 0) {
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

	/*
	 * This handles a request to "http:<ip>/solve?element=<elem>&y=<y>"
	 * The element and y argument are extracted and verified.
	 * 
	 * If accepted, the result of the requested element is returned.
	 */
	} else if (strncmp(url, "/solve", strlen("/solve")) == 0) {
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
		const char *y_arg = MHD_lookup_connection_value(
			connection, MHD_GET_ARGUMENT_KIND, "y");
		if (!y_arg) {
    			snprintf(response_text, 2048, "missing y parameter\n");
			ret_status = MHD_HTTP_BAD_REQUEST;
			goto respond;
		}
		s = malloc(sizeof(struct request_solve));
		s->element = elem;
		s->y = strndup(y_arg, get_max_hexlen());

		if (verify_puzzle(s)) {
			snprintf(response_text, 2048, "Request:%s;Result:%s;", elem_str, service(elem));
			ret_status = MHD_HTTP_OK;
		} else {
			snprintf(response_text, 2048, "TLP verification failed\n");
			ret_status = MHD_HTTP_FORBIDDEN;
		}
		free(s->y);
		free(s);
		goto respond;
	/* 
	 * Any other endpoints are rejected
	 */
	} else {
		snprintf(response_text, 2048, "unknown endpoint\n");
		ret_status = MHD_HTTP_BAD_REQUEST;
		goto respond;
	}
respond:
    	response = MHD_create_response_from_buffer(strlen(response_text),
                                        		(void*)response_text,
                                               		MHD_RESPMEM_MUST_COPY);
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