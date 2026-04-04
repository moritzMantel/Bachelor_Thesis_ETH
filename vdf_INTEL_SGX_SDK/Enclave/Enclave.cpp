/*
 * Copyright (C) 2011-2021 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */
#include "mbedtls/bignum.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"

#define mbedtls_printf       printf

#define MPI_STR_SIZE 1024
#define CYCLES_PER_SQUARING 5000

/*
 * struct config {
 *      uint64_t num_bits:              
 *      uint64_t private_set_digits;    
 *      uint64_t private_set_size;      
 *      uint64_t min_expected_cycles;   
 *      int T_exp;                      
 *      int T_baseline_comp;                               
 *      int T_input_size_comp;                            
 *      int T_private_set_comp;         
 * };
 */
struct config conf = {
    1024,   // number of bits of modulus (/2)
    11,     // how large is the domain of private set (eg. how many digits)
    100,    // how many items should be in private set
    0,      // expected number of cycles per request
    15,     // for default T = 2^T_exp
    0,      // 0 for T = 2^T_exp, 
            // 1 for T s.t. min expected cycles sat.
    1,      // 0 for constant T
            // 1 for linear scaling with input size
    0       // 0 for constant T
            // 1 to sat. E_cycles[hit]
};

uint64_t T_base;

mbedtls_mpi N;
mbedtls_mpi phi;
mbedtls_mpi exponent;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
const char *pers = "prime_gen_puzzle";

char N_str[MPI_STR_SIZE];
int n_len;

int initialized = 0;

std::unordered_set<uint64_t> private_set;

void initialize_private_set()
{
    uint64_t stepsize = (uint64_t) pow(10, conf.private_set_digits) / conf.private_set_size;

    for (int i = 0; i < conf.private_set_size; i++) {
        private_set.insert(i * stepsize);
    }
}

/*
 * If debug_build is enabled, this allows for overwriting the default conf.
 * This is used to evaluate parameters.
 * 
 * Returns:
 *      * 0         if successful
 *      * -1        if parameters not allowed
 *      * -2        if not permitted (eg. not a debug build)
 */
int ecall_set_config(struct config *c)
{
#ifdef debug_build

    if (c->num_bits <= 256 || c->num_bits > 4096)
        return -1;
    if (c->private_set_digits < 1 || c->private_set_digits > 20)
        return -1;
    if (c->private_set_size < 1 || c->private_set_size > 1000000000
                || c->private_set_size > pow(10, c->private_set_digits))
        return -1;
    if (c->T_exp < 1 || c->T_exp > 30)
        return -1;
    if (c->T_baseline_comp < 0 || c->T_baseline_comp > 2)
        return -1;
    if (c->T_input_size_comp < 0 || c->T_input_size_comp > 2)
        return -1;
    if (c->T_private_set_comp < 0 || c->T_private_set_comp > 2)
        return -1;
    
    conf = *c;
    return 0;
#else
    return -2;
#endif
}

int set_baseline()
{
    uint64_t square_it;
    switch(conf.T_baseline_comp) {
    case 0: {
        /* 
         * This simply sets T to the independently chosen constant value.
         */
        T_base = (uint64_t)1 << conf.T_exp;
        square_it = conf.T_exp;
        break;
    } 
    case 1: {
        /* 
         * This implements the idea that a request should take a minimum of
         * conf.min_expected_cycles many cycles per request.
         */
        square_it = log2(conf.min_expected_cycles / CYCLES_PER_SQUARING) + 1;
        T_base = (uint64_t)1 << square_it;
        break;
    }
    default: {
        printf("unsupported conf.T_baseline_comp\n");
        return -4;
    }
    }

    if (mbedtls_mpi_lset(&exponent, 2) != 0) {
        return -4;
    }

    for (uint64_t i = 0; i < square_it; i++) {
        if (mbedtls_mpi_mul_mpi(&exponent, &exponent, &exponent) != 0) {
            return -4;
        }
        if (mbedtls_mpi_mod_mpi(&exponent, &exponent, &phi) != 0) {
            return -4;
        }
    }
    return 0;
}

/*
 * Initialises the cryptographic primitives necessary for the VDF.

 * Must be called before the first calls to 
 *      * 'ecall_request_puzzle'
 *      * 'ecall_submit_solution'
 * 
 * If any part of the initialisation fails, the return value will be negative.
 * In that case, the enclave is expected to be teared down.
 * 
 * Returns:
 *      * 0         if successful
 *      * -1        if random seed / context fails
 *      * -2        if prime generation fails
 *      * -3        if mbedtls mpi arithmetic operations fail
 *      * -4        if computation of exponent := 2^T fails
 *      * -5        if writing the hex strings fails
 */
int ecall_init(void)
{
    int ret_code;
    size_t written;

    mbedtls_mpi p;
    mbedtls_mpi q;
    mbedtls_mpi T_mpi;

    mbedtls_entropy_init( &entropy );
    mbedtls_ctr_drbg_init( &ctr_drbg );

    if (mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                            (const unsigned char *) pers,
                            strlen(pers)) != 0) {
        ret_code = -1;
        goto exit_1;
    }

    mbedtls_mpi_init(&p);
    mbedtls_mpi_init(&q);
    if (mbedtls_mpi_gen_prime(&p, conf.num_bits, 0, mbedtls_ctr_drbg_random,
                            &ctr_drbg) != 0) {
        ret_code = -2;
        goto exit_2;
    }
    if (mbedtls_mpi_gen_prime(&q, conf.num_bits, 0, mbedtls_ctr_drbg_random, 
                            &ctr_drbg) != 0) {
        ret_code = -2;
        goto exit_2;
    }

    mbedtls_mpi_init(&N);
    mbedtls_mpi_init(&phi);
    if (mbedtls_mpi_mul_mpi(&N, &p, &q) != 0) {
        ret_code = -3;
        goto exit_3;
    }
    if (mbedtls_mpi_sub_int(&p, &p, 1) != 0) {
        ret_code = -3;
        goto exit_3;
    }
    if (mbedtls_mpi_sub_int(&q, &q, 1) != 0) {
        ret_code = -3;
        goto exit_3;
    }
    if (mbedtls_mpi_mul_mpi(&phi, &p, &q) != 0) {
        ret_code = -3;
        goto exit_3;
    }

    mbedtls_mpi_init(&exponent);

    if (set_baseline() != 0) {
        ret_code = -4;
        goto exit_4;
    }

    if (mbedtls_mpi_write_string(&N, 16, N_str, MPI_STR_SIZE, &written) != 0) {
        ret_code = -5;
        goto exit_4;
    }
    n_len = written;

    initialize_private_set();

    /*
     * Initialisation successful, free temporary mpi variables
     */
    initialized = 1;
    ret_code = 0;
    mbedtls_mpi_free(&p);
    mbedtls_mpi_free(&q);
    goto exit_0;

    /*
     * Exit blocks, depending on initialisation progress before error, 
     * all already initialised mpi variables are freed.
     */
exit_4:
    mbedtls_mpi_free(&exponent);
exit_3:
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&phi);
exit_2:
    mbedtls_mpi_free(&q);
    mbedtls_mpi_free(&p);
exit_1:
    mbedtls_entropy_free(&entropy);
    mbedtls_ctr_drbg_free( &ctr_drbg );
exit_0:
    return ret_code;
}

/*
 * Frees all memory currently dynamically allocated.
 *
 * Is expected to be called before enclave teardown.
 */
void ecall_teardown(void)
{
    if (!initialized)
        return;
    mbedtls_ctr_drbg_free( &ctr_drbg );
    mbedtls_entropy_free( &entropy );
    mbedtls_mpi_free(&N);
    mbedtls_mpi_free(&phi);
    mbedtls_mpi_free(&exponent);
    initialized = 0;
}

void generate_base(mbedtls_mpi *x, int n, uint64_t *elements)
{
    unsigned char res[32];
    unsigned char buffer[n * sizeof(uint64_t)];

    mbedtls_mpi one;
    mbedtls_mpi_init(&one);
    mbedtls_mpi_lset(&one, 1);
    mbedtls_mpi gcd;
    mbedtls_mpi_init(&gcd);

    for (int i = 0; i < n; i++) {
        uint64_t v = elements[i];
        memcpy(&buffer[i * sizeof(uint64_t)], &v, sizeof(uint64_t));
    }

    if (mbedtls_sha256(buffer, sizeof(buffer), res, 0) != 0)
        printf("Error in hash generation\n");

    if (mbedtls_mpi_read_binary(x, res, 32) != 0)
        printf("Error in reading hash into mpi\n");

    mbedtls_mpi_gcd(&gcd, x, &N);

    if (mbedtls_mpi_cmp_mpi(&one, &gcd) != 0) 
        printf("GCD of x and N not 1!\n");
}

std::vector<uint64_t> compute_intersection(int n, uint64_t *elements)
{
    std::vector<uint64_t> intersection;
    
    for (int i = 0; i < n; i++) {
        if (private_set.find(elements[i]) != private_set.end())
            intersection.push_back(elements[i]);
    }
    return intersection;
}

uint64_t compute_T_set()
{
    switch(conf.T_private_set_comp) {
    case 0: {
        /*
         * This does not modify T w.r.t the current state of the private set.
         */
        return T_base;
    }
    case 1: {
        /*
         * This corresponds to the idea that the expected number of cycles to
         * "find" an element in the private set should take 
         * conf.min_expected_cycles many cycles
         * (assuming random probing and uniform distribution of private set).
         * For that reason, T_base is scaled according to the probability to
         * randomly "find" an element.
         * 
         * This assumes that option 1 is set for baseline computation.
         */
        return T_base * (private_set.size() / (uint64_t) pow(10, conf.private_set_digits));
    }
    default: {
        return T_base;
    }
    }
}

uint64_t compute_T(int n)
{
    switch(conf.T_input_size_comp) {
    case 0: {
        /*
         * This does not scale T with number of elements per request.
         */
        return compute_T_set();
    }
    case 1: {
        /*
         * This linearly scales the necessary squarings with the number of 
         * requested elements.
         */
        return n * compute_T_set();
    }
    }
}

/*
 * Returns a VDF puzzle.
 * The puzzle consists of three (large) integers x, N and T.
 * The solution of the puzzle is y = (x^2^T) mod N.
 *
 * Params:
 *      * x_buf:    the buffer where x will be written into (size == 1024)
 *      * N_buf:    the buffer where N will be written into (size == 1024)
 *      * T_buf:    the buffer where T will be written into (size == 1024)
 *      * size:     size of the elems buffer
 *      * elems:    array of the requested elements
 * 
 * Returns:
 *      *  0        if the puzzle was successfully generated
 *      * -1        if any of the mbedtls mpi operations fail
 *      * -2        if the writing of the hex strings fail
 *      * -3        if mbedtls state is not initialized correctly
 */
int ecall_request_puzzle(char *x_buf, char *N_buf, uint64_t *T, int s, uint64_t *elems)
{
    if (!initialized)
        return -3;
    
    mbedtls_mpi x;
    size_t written;
    int ret_code = -3;
    int n = s / sizeof(uint64_t);

    mbedtls_mpi_init(&x);

    generate_base(&x, n, elems);

    if (mbedtls_mpi_mod_mpi(&x, &x, &N) != 0) {
        ret_code = -1;
        goto exit;
    }

    if (mbedtls_mpi_write_string(&x, 16, x_buf, MPI_STR_SIZE, &written) != 0) {
        ret_code = -2;
        goto exit;
    }

    *T = compute_T(n);

    memcpy(N_buf, N_str, n_len);
    ret_code = 0;

exit:
    mbedtls_mpi_free(&x);
    return ret_code;
}

int adj_exponent_set(mbedtls_mpi *final_exp, uint64_t T_final)
{
    switch(conf.T_private_set_comp) {
    case 0: {
        /*
         * This does not modify T w.r.t the current state of the private set.
         */
        return mbedtls_mpi_copy(final_exp, &exponent);
    }
    case 1: {
        /*
         * Since scaling of the exponent mod phi only works for positive ints,
         * in this case, we have to completely recompute the exponent.
         */
        if (mbedtls_mpi_lset(final_exp, 2) != 0) {
            return -1;
        }

        for (uint64_t i = 0; i < T_final; i++) { 
            if (mbedtls_mpi_mul_mpi(final_exp, final_exp, final_exp) != 0) {
                return -1;
            }
            if (mbedtls_mpi_mod_mpi(final_exp, final_exp, &phi) != 0) {
                return -1;
            }
        }

    }
    default: {
        return 0;
    }
    }
}

int adj_exponent(mbedtls_mpi *final_exp, int n, uint64_t T_final)
{
    if (adj_exponent_set(final_exp, T_final) != 0)
        return -1;

    switch(conf.T_input_size_comp) {
    case 0: {
        /*
         * This does not scale T with number of elements per request.
         */
        return 0;
    }
    case 1: {
        /*
         * This applies the linear scaling to T_base to the final_exp, which 
         * is equivalent to exponent^n
         */
        mbedtls_mpi temp;
        mbedtls_mpi_init(&temp);
        mbedtls_mpi_copy(&temp, final_exp);
        for (int i = 0; i < n-1; i++) {
            if (mbedtls_mpi_mul_mpi(final_exp, final_exp, &temp) != 0) {
                mbedtls_mpi_free(&temp);
                return -1;
            }
            if (mbedtls_mpi_mod_mpi(final_exp, final_exp, &phi) != 0) {
                mbedtls_mpi_free(&temp);
                return -1;
            }
        }
        mbedtls_mpi_free(&temp);
        return 0;
    }
    }
}

/*
 * Verifies the VDF solution and checks membership of the requested element.
 *
 * Params:
 *      * result:   intersection between requested elements and private set
 *      * size:     size of the input buffer elems
 *      * elems:    array of requested elements
 *      * y_str:    the hex string containing the submitted solution y
 * 
 * Returns:
 *      * 0         if the intersection was computed and successfully returned
 *      * -1        if the solution was not accepted
 *      * -2        if any of the mbedtls mpi operations fail
 *      * -3        if reading the hex string fails
 *      * -4        if mbedtls state is not initialized correctly
 */
int ecall_submit_solution(uint64_t *result, int s, uint64_t *elems, char *y_str)
{
    if (!initialized)
        return -4;

    mbedtls_mpi x;
    mbedtls_mpi y;
    mbedtls_mpi final_exp;
    char expected[1024];
    int ret;
    int n = s / sizeof(uint64_t);

    mbedtls_mpi_init(&x);
    mbedtls_mpi_init(&y);
    mbedtls_mpi_init(&final_exp);

    generate_base(&x, n, elems);

    adj_exponent(&final_exp, n, compute_T(n));

    if (mbedtls_mpi_mod_mpi(&x, &x, &N) != 0) {
        ret = -2;
        goto exit;
    }
    if (mbedtls_mpi_exp_mod(&x, &x, &final_exp, &N, NULL) != 0) {
        ret = -2;
        goto exit;
    }
    if (mbedtls_mpi_read_string(&y, 16, y_str) != 0) {
        ret = -3;
        goto exit;
    }

    size_t written;
    mbedtls_mpi_write_string(&x, 16, expected, 1025, &written);

    if (mbedtls_mpi_cmp_mpi(&x, &y) != 0) {
        ret = -1;
    } else {
        // cost for brute force
        // how to choose T w.r.t. private set properties

        std::vector<uint64_t> intersection = compute_intersection(n, elems);
        for (int i = 0; i < intersection.size(); i++) {
            result[i] = intersection[i];
        }
        for (int i = intersection.size(); i < n; i++) {
            result[i] = 0;
        }
        ret = intersection.size();
    }

exit:
    mbedtls_mpi_free(&x);
    mbedtls_mpi_free(&y);
    return ret;
}

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 *   'printf' function is required for sgx protobuf logging module.
 */
int printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
    return 0;
}