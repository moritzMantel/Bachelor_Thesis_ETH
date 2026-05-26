#include "../include/endpoint.h"
#include "../include/rate_limiting.h"
#include <stdio.h>
#include <openssl/bn.h>



#ifdef test_simple
#define T_BASE 5
#define NUM_BITS 2
#else
#define T_BASE 50000 	/* this gives roughly 1 second of computation time on
 			   my machine, enough for simple testing */
#define NUM_BITS 1024
#endif

BIGNUM *N;
BIGNUM *phi;
BIGNUM *e;

/*
 * Initialises the state needed to generate puzzles according to RSA TLP:
 * 	N = p * q, for p and q large primes
 *	phi = (p-1) * (q-1)
 *	e = 2^T mod phi, s.t. x^2^T mod N = x^e mod N
 *
 * This function must be called before the first request.
 */
int rate_limiting_init()
{
	N = BN_new();
	phi = BN_new();
	e = BN_new();
	BN_CTX *ctx = BN_CTX_new();

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

    	BN_free(p);
    	BN_free(q);
	BN_free(two);
	BN_free(bn_T);
	BN_CTX_free(ctx);

    	return 0;
}

/*
 * Frees the global state.
 *
 * Should be called after last request.
 */
int rate_limiting_clean_up()
{
    	BN_free(N);
    	BN_free(phi);
	BN_free(e);

	return 0;
}

/*
 * Quick utility that returns the number of hex characters needed to
 * represent any number in our RSA group (eg. mod N).
 */
size_t get_max_hexlen()
{
	return BN_num_bytes(N) * 2;
}

/*
 * Generates a RSA TLP puzzle.
 * The puzzle consists of x, T and N. The solution is y = x^2^T mod N.
 * The base x is chosen to be completely deterministic from the requested
 * element, namely x = requested_element % N. This avoids having to store any
 * "per-request" state.
 * 
 * Params:
 * 	* requested_element:	elem for which the service is requested
 *
 * Returns:
 *	* res (struct puzzle):
 *	 * res->N		the modulos
 *	 * res->x		the base
 *	 * res->T		the number of squarings
 */
struct puzzle *generate_puzzle(unsigned long requested_element)
{
	BN_CTX *ctx = BN_CTX_new();
	BIGNUM *X = BN_new();
	BN_set_word(X, requested_element);
	BN_mod(X, X, N, ctx);


	struct puzzle *res = malloc(sizeof(struct puzzle));
	res->N = BN_bn2hex(N);
	res->x = BN_bn2hex(X);
	res->T = malloc(33);
	sprintf(res->T, "%032x", (unsigned int)T_BASE);
	res->T[32] = '\0';

	BN_free(X);
	BN_CTX_free(ctx);

	return res;
}

/*
 * Verifies whether a submitted solution is correct.
 * Namely, it verifies whether p->y mod N = p->element^2^T mod N.
 *
 * Params:
 *	* p (struct request_solve):
 *	 * p->y				solution element^2^T
 *	 * p->element			the requested element
 *			
 * Returns:
 *	* True				if the solution is correct
 *	* False				if the solution is incorrect
 */
int verify_puzzle(struct request_solve *p)
{
	/*
	 * Reject any solution that is longer than the maximum hex
	 * representation of a number mod N.
	 */
	size_t max = get_max_hexlen();
	if (!p->y || strnlen(p->y, max + 1) > max)
		return -1;

	BN_CTX *ctx = BN_CTX_new();
	BIGNUM *X = BN_new();
	BIGNUM *Y = NULL;
	BIGNUM *Y_true = BN_new();

	BN_set_word(X, p->element);
	BN_mod(X, X, N, ctx);

	BN_hex2bn(&Y, p->y);

	BN_mod_exp(Y_true, X, e, N, ctx);

	int res = BN_cmp(Y, Y_true) == 0;

	BN_free(X);
	BN_free(Y);
	BN_free(Y_true);
	BN_CTX_free(ctx);

	return res;
}