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

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <iostream>
#include <chrono>

#include <unistd.h>
#include <pwd.h>
#define MAX_PATH FILENAME_MAX

#define NUM_BITS 1024
#define MPI_STR_SIZE NUM_BITS

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#include <mbedtls/bignum.h>

#define EVALUATION

#ifdef EVALUATION
#define compute_puzzle compute_puzzle_timed
#define request_puzzle request_puzzle_timed
#define submit_solution submit_solution_timed
#else
#define compute_puzzle compute_puzzle_untimed
#define request_puzzle ecall_request_puzzle
#define submit_solution ecall_submit_solution
#endif

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
    {
        SGX_ERROR_NDEBUG_ENCLAVE,
        "The enclave is signed as product enclave, and can not be created as debuggable enclave.",
        NULL
    },
    {
        SGX_ERROR_MEMORY_MAP_FAILURE,
        "Failed to reserve memory for the enclave.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }
    
    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Call sgx_create_enclave to initialize an enclave instance
 */
int initialize_enclave(void)
{
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    
    /* Call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }

    int ret_init;
    ecall_init(global_eid, &ret_init);

    return ret_init;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate 
     * the input string to prevent buffer overflow. 
     */
    printf("%s", str);
}

/*
 * Computes the VDF given by x, N and T.
 * Returns the solution y = x^2^T mod N.
 */
void compute_puzzle_untimed(char *y, char *x, char *N, uint64_t T)
{
    mbedtls_mpi x_mpi;
    mbedtls_mpi N_mpi;


    mbedtls_mpi_init(&x_mpi);
    mbedtls_mpi_init(&N_mpi);

    if (mbedtls_mpi_read_string(&x_mpi, 16, x) != 0)
        printf("x read failed\n");
    if (mbedtls_mpi_read_string(&N_mpi, 16, N) != 0)
        printf("N read failed\n");

    printf("x: %s\n", x);
    printf("N: %s\n", N);
    printf("T: %lu\n", T);

    for (uint64_t i = 0; i < T; i++) {
        if (mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi) != 0)
            printf("x mult failed\n");
        if (mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi) != 0)
            printf("x mod failed\n");
    }

    size_t written;
    mbedtls_mpi_write_string(&x_mpi, 16, y, MPI_STR_SIZE, &written);

    printf("computed solution: %s\n", y);

    mbedtls_mpi_free(&x_mpi);
    mbedtls_mpi_free(&N_mpi);
}

/*
 * Timing wrapper around computation
 */
void compute_puzzle_timed(char *y, char *x, char *N, uint64_t T)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    compute_puzzle_untimed(y, x, N, T);

    auto end = std::chrono::high_resolution_clock::now();

    std::cout << "time to solve for T= " << T << ": " 
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
}

void request_puzzle_timed(int global_eid, int *request_status, char*x, char*N, uint64_t *T, int s, uint64_t *elements)
{
    auto start = std::chrono::steady_clock::now();
    
    ecall_request_puzzle(global_eid, request_status, x, N, T, s, elements);

    auto end = std::chrono::steady_clock::now();

    std::cout << "time to request:" 
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
}

void submit_solution_timed(int global_eid, int *ret_val, uint64_t *intersection, int s, uint64_t *elements, char *sol)
{
    auto start = std::chrono::steady_clock::now();
    
    ecall_submit_solution(global_eid, ret_val, intersection, s, elements, sol);

    auto end = std::chrono::steady_clock::now();

    std::cout << "time to verify and compute intersection:" 
        << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << " ms" << std::endl;
}


/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    /* Initialize the enclave */
    int init_ret = initialize_enclave();
    if(init_ret < 0){
        printf("Initialisation of crypto primitives failed: %d\n", init_ret);
        printf("Enter a character before exit ...\n");
        getchar();
        return -1; 
    }
    uint64_t elements[argc-1];

    for (int i = 1; i < argc; i++) {
        char *end;
        uint64_t element = std::strtoul(argv[i], &end, 10);
        if (*end != '\0')
            printf("argv[%d] invalid (skipped)\n", i);
        elements[i-1] = element;
    }
    char x[MPI_STR_SIZE];
    char N[MPI_STR_SIZE];
    uint64_t T;

    int request_status;
    request_puzzle(global_eid, &request_status, x, N, &T, (argc-1) * sizeof(uint64_t), elements);
    switch (request_status) {
    case 0: { printf("puzzle received\n"); break; }
    case -1: { printf("mpi operations failed\n"); break; }
    case -2: { printf("mpi hex string write failed\n"); break; }
    default: { printf("unknown error in puzzle generation\n"); break; }
    };

    char sol[MPI_STR_SIZE];
    compute_puzzle(sol, x, N, T);

    int ret_val;
    uint64_t intersection[argc-1];

    submit_solution(global_eid, &ret_val, intersection, (argc-1) * sizeof(uint64_t), elements, sol);

    switch (ret_val) {
    case -1: { printf("VDF not accepted\n"); break; }
    case -2: { printf("mpi operations failed\n"); break; }
    case -3: { printf("mpi hex string write failed\n"); break; }
    default: {
        printf("result received: intersection is\n{ ");
        for (int i = 0; i < ret_val; i++)
            printf("+%lu ", intersection[i]);
        printf(" }\n");
        break;
    }
    };

    ecall_teardown(global_eid);

    sgx_destroy_enclave(global_eid);
    
    printf("Enter a character before exit ...\n");
    getchar();
    return 0;
}

