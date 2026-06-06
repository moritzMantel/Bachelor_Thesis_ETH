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
#include <stdio.h>              /* vsnprintf */
#include <string.h>

#include <string>
#include <unordered_set>
#include <vector>

#include "Enclave.h"
#include "Enclave_t.h"
#include "mbedtls/bignum.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/sha256.h"

#define mbedtls_printf          printf

mbedtls_mpi N;
mbedtls_mpi phi;
mbedtls_mpi exponent;
mbedtls_entropy_context entropy;
mbedtls_ctr_drbg_context ctr_drbg;
const char *pers = "prime_gen_puzzle";

char N_str[MPI_STR_SIZE];
int n_len;

int initialized = 0;

/*
 * Initialises the cryptographic primitives necessary for the TLP.

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
    size_t written, squarings;
    size_t written, squarings;

    mbedtls_mpi p;
    mbedtls_mpi q;
    mbedtls_mpi T_mpi;

    initialize_private_set();

    initialize_private_set();

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
    if (mbedtls_mpi_gen_prime(&p, config_get_num_bits(), 0, mbedtls_ctr_drbg_random,
    if (mbedtls_mpi_gen_prime(&p, config_get_num_bits(), 0, mbedtls_ctr_drbg_random,
                            &ctr_drbg) != 0) {
        ret_code = -2;
        goto exit_2;
    }
    if (mbedtls_mpi_gen_prime(&q, config_get_num_bits(), 0, mbedtls_ctr_drbg_random, 
    if (mbedtls_mpi_gen_prime(&q, config_get_num_bits(), 0, mbedtls_ctr_drbg_random, 
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

    squarings = config_get_T_exponent();

    if (!squarings) {
        ret_code = -3;
        goto exit_3;
    }

    squarings = config_get_T_exponent();

    if (!squarings) {
        ret_code = -3;
        goto exit_3;
    }

    mbedtls_mpi_init(&exponent);

    if (mbedtls_mpi_lset(&exponent, 2) != 0) {
    if (mbedtls_mpi_lset(&exponent, 2) != 0) {
        ret_code = -4;
        goto exit_4;
    }

    for (uint64_t i = 0; i < squarings; i++) {
        if (mbedtls_mpi_mul_mpi(&exponent, &exponent, &exponent) != 0) {
            ret_code = -4;
            goto exit_4;
        }
        if (mbedtls_mpi_mod_mpi(&exponent, &exponent, &phi) != 0) {
            ret_code = -4;
            goto exit_4;
        }
    }

    for (uint64_t i = 0; i < squarings; i++) {
        if (mbedtls_mpi_mul_mpi(&exponent, &exponent, &exponent) != 0) {
            ret_code = -4;
            goto exit_4;
        }
        if (mbedtls_mpi_mod_mpi(&exponent, &exponent, &phi) != 0) {
            ret_code = -4;
            goto exit_4;
        }
    }

    if (mbedtls_mpi_write_string(&N, 16, N_str, MPI_STR_SIZE, &written) != 0) {
        ret_code = -5;
        goto exit_4;
    }
    n_len = written;

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
    mbedtls_ctr_drbg_free(&ctr_drbg);
    mbedtls_entropy_free(&entropy);
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

/*
 * Returns a TLP puzzle.
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

    *T = config_compute_T(n);
    *T = config_compute_T(n);

    memcpy(N_buf, N_str, n_len);
    ret_code = 0;

exit:
    mbedtls_mpi_free(&x);
    return ret_code;
}

/*
 * Verifies the TLP solution and checks membership of the requested element.
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

    config_adj_exponent(&final_exp, n);
    config_adj_exponent(&final_exp, n);

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

    // printf("Solution: %s\n", expected);

    // printf("Solution: %s\n", expected);

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