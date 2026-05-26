#ifndef RATE_LIMITING_H
#define RATE_LIMITING_H
#include <libc.h>

struct puzzle {
	char *N;
	char *x;
	char *T;
};
struct request_solve {
	unsigned long element;
	char *y;
};

int rate_limiting_init();
int rate_limiting_clean_up();
size_t get_max_hexlen();
struct puzzle *generate_puzzle(unsigned long requested_element);
int verify_puzzle(struct request_solve *p);

#endif