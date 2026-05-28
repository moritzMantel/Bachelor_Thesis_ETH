#include <stdlib.h>
#include "../include/rate_limiting.h"
#include "../include/service.h"

size_t buf_size(void);

/*
 * Must be called before use of the library.
 */
void init()
{
	rate_limiting_init();
}

/*
 * Must be called after use of the library.
 */
void teardown()
{
	rate_limiting_clean_up();
}

/*
 * Requests the puzzle for an element.
 *
 * Params:
 *	* ret_buf:	result will be written into here
 *			format is json, consisting of x, T, N
 *	* e:		requested element
 */
void request(char *ret_buf, unsigned long e)
{
	struct puzzle *p = generate_puzzle(e);
	size_t buf_s = buf_size();
	snprintf(ret_buf, buf_s, "{\"x\":\"%s\",\"T\":\"%s\",\"N\":\"%s\"}", p->x, p->T, p->N);}

/*
 * Submit a request for an element including the solution.
 *
 * Params:
 *	* ret_buf:	result will be written into here:
 *			- "1" if e is in S
 *			- "0" if e is not in S
 *			- "NO MEMORY" if malloc fails
 *			- "NOT ACCEPTED" if Y is not a valid solution
 *	* e:		requested element
 *	* Y:		solution y = e^2^T in hex
 */
void request_sol(char *ret_buf, unsigned long e, char *Y)
{
	struct request_solve *s = malloc(sizeof(struct request_solve));
	if (!s) {
		sprintf(ret_buf, "NO MEMORY\n");
		return;
	}
	s->element = e;
	s->y = Y;

	int accepted = verify_puzzle(s);
	free(s);

	if (accepted) {
		snprintf(ret_buf, 2, "%s", service(e));
	} else {
		sprintf(ret_buf, "NOT ACCEPTED\n");
	}
}

/*
 * Returns the size a caller must allocate at a minimum for the return buffers.
 * 
 * get_max_hexlen() returns the maximum number of hex chars needed to encode
 * numbers in the puzzle domain. There are 3 numbers + 22 characters for the
 * json encoding.
 *
 * Returns:
 *	size of buffer needed
 */
size_t buf_size(void)
{
	return get_max_hexlen()*3 + 23;
}