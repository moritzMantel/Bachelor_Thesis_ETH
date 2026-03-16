#ifndef rate_limiting
#define rate_limiting
#include <libc.h>

int rate_limiting_init();
int rate_limiting_clean_up();

struct puzzle {
	char *N;
	char *x;
	char *T;
};
struct request_solve {
	unsigned long element;
	char *y;
};

struct puzzle *generate_puzzle(unsigned long requested_element);

int verify_puzzle(struct request_solve *p);
#endif