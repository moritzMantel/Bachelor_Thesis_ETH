#include "../include/endpoint.h"
#include "../include/rate_limiting.h"
#include <stdio.h>
#include <openssl/bn.h>



#ifdef test_simple
#define T_BASE 5
#define NUM_BITS 2
#define MAX_BASE 13
#else
#define T_BASE 10000
#define NUM_BITS 1024
#define MAX_BASE 16384
#endif

BIGNUM *N;
BIGNUM *phi;
BN_CTX *ctx;
BIGNUM *e;

char *N_str;

int rate_limiting_init()
{
	N = BN_new();
	phi = BN_new();
	e = BN_new();
	ctx = BN_CTX_new();

	BIGNUM *p = BN_new();
	BIGNUM *q = BN_new();
	BIGNUM *bn_T = BN_new();
	BIGNUM *two = BN_new();

	BN_set_word(two, 2);
	BN_set_word(bn_T, T_BASE);

#ifdef test_simple
	BN_set_word(p, 7);
	BN_set_word(q, 13);
#else
    	BN_generate_prime_ex(p, NUM_BITS, 0, NULL, NULL, NULL);
    	BN_generate_prime_ex(q, NUM_BITS, 0, NULL, NULL, NULL);
#endif
    	BN_mul(N, p, q, ctx);

    	BN_sub_word(p, 1);
    	BN_sub_word(q, 1);
    	BN_mul(phi, p, q, ctx);
	BN_mod_exp(e, two, bn_T, phi, ctx);

	N_str = BN_bn2hex(N);

    	BN_free(p);
    	BN_free(q);
	BN_free(two);
	BN_free(bn_T);
    	return 0;
}

int rate_limiting_clean_up()
{
    	BN_free(N);
    	BN_free(phi);
	BN_free(e);
    	BN_CTX_free(ctx);
	OPENSSL_free(N_str);
	return 0;
}

struct puzzle *generate_puzzle(unsigned long requested_element)
{
	unsigned int x = requested_element % MAX_BASE;

	struct puzzle *res = malloc(sizeof(struct puzzle));
	res->N = malloc(513);
	res->N_len = strnlen(N_str, 512);
	res->x = malloc(33);
	res->x_len = 32;
	res->T = malloc(33);
	res->T_len = 32;
	strncpy(res->N, N_str, res->N_len);
	res->N[512] = '\0';
	sprintf(res->x, "%032x", x);
	res->x[32] = '\0';
	sprintf(res->T, "%032x", T_BASE);
	res->T[32] = '\0';

	return res;
}

int verify_puzzle(struct request_solve *p)
{
	if (!p->y || strnlen(p->y, 513) == 513)
		return -1;

	BIGNUM *x_given = BN_new();
	BIGNUM *Y = NULL;
	BIGNUM *Y_true = BN_new();

	BN_set_word(x_given, p->element % MAX_BASE);
	BN_hex2bn(&Y, p->y);

	BN_mod_exp(Y_true, x_given, e, N, ctx);

	int res = BN_cmp(Y, Y_true) == 0;

	BN_free(x_given);
	BN_free(Y);
	BN_free(Y_true);

	return res;
}