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
#include <fstream>
#include <chrono>
#include <tuple>
#include <vector>

#include <unistd.h>
#include <pwd.h>
#define MAX_PATH FILENAME_MAX

#define NUM_BITS 1024
#define MPI_STR_SIZE NUM_BITS

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#include <mbedtls/bignum.h>

#define BASE_CONFIG {1024, 10, 100, 0, 14, 0, 0, 0}

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
void compute_puzzle_internal(char *y, char *x, char *N, uint64_t T)
{
    mbedtls_mpi x_mpi;
    mbedtls_mpi N_mpi;


    mbedtls_mpi_init(&x_mpi);
    mbedtls_mpi_init(&N_mpi);

    if (mbedtls_mpi_read_string(&x_mpi, 16, x) != 0)
        printf("x read failed\n");
    if (mbedtls_mpi_read_string(&N_mpi, 16, N) != 0)
        printf("N read failed\n");

    for (uint64_t i = 0; i < T; i++) {
        if (mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi) != 0)
            printf("x mult failed\n");
        if (mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi) != 0)
            printf("x mod failed\n");
    }

    size_t written;
    mbedtls_mpi_write_string(&x_mpi, 16, y, MPI_STR_SIZE, &written);

    mbedtls_mpi_free(&x_mpi);
    mbedtls_mpi_free(&N_mpi);
}

int64_t init_enclave()
{
    int ret_init;
    auto start = std::chrono::high_resolution_clock::now();
    
    ecall_init(global_eid, &ret_init);

    auto end = std::chrono::high_resolution_clock::now();

    if (ret_init != 0) {
        printf("initialization error: %d", ret_init);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}

int64_t teardown_enclave() {
    auto start = std::chrono::high_resolution_clock::now();
    
    ecall_teardown(global_eid);

    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}

int64_t set_config(struct config *c) 
{

    int ret_status;
    auto start = std::chrono::high_resolution_clock::now();
    
    ecall_set_config(global_eid, &ret_status, c);

    auto end = std::chrono::high_resolution_clock::now();

    if (ret_status < 0) {
        printf("config failed: %d\n", ret_status);
    }

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}
/*
 * Timing wrapper around computation
 */
int64_t compute_puzzle(char *y, char *x, char *N, uint64_t T)
{
    auto start = std::chrono::high_resolution_clock::now();
    
    compute_puzzle_internal(y, x, N, T);

    auto end = std::chrono::high_resolution_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}

int64_t request_puzzle(int global_eid, int *request_status, char*x,
                        char*N, uint64_t *T, int s, uint64_t *elements)
{
    auto start = std::chrono::steady_clock::now();
    
    ecall_request_puzzle(global_eid, request_status, x, N, T, s, elements);

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}

int64_t submit_solution(int global_eid, int *ret_val, uint64_t *intersection,
                        int s, uint64_t *elements, char *sol)
{
    auto start = std::chrono::steady_clock::now();
    
    ecall_submit_solution(global_eid, ret_val, intersection, s, elements, sol);

    auto end = std::chrono::steady_clock::now();

    return std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
        .count();
}


std::tuple<int64_t,int64_t,int64_t> make_request(std::vector<uint64_t> elements) {
    char x[MPI_STR_SIZE];
    char N[MPI_STR_SIZE];
    uint64_t T;

    int request_status;
    int64_t req_time = request_puzzle(global_eid, &request_status, x, N,
                            &T, elements.size() * sizeof(uint64_t), elements.data());
    switch (request_status) {
    case 0: { break; }
    case -1: { printf("mpi operations failed\n"); return {-1, -1, -1}; }
    case -2: { printf("mpi hex string write failed\n"); return {-1, -1, -1}; }
    default: { printf("unknown error in puzzle generation\n"); return {-1, -1, -1}; }
    };

    char sol[MPI_STR_SIZE];
    int64_t comp_time = compute_puzzle(sol, x, N, T);

    int ret_val;
    uint64_t intersection[elements.size()];

    int64_t submit_time = submit_solution(global_eid, &ret_val, intersection,
                            elements.size() * sizeof(uint64_t), elements.data(), sol);

    switch (ret_val) {
    case -1: { printf("VDF not accepted\n"); return {-1, -1, -1}; }
    case -2: { printf("mpi operations failed\n"); return {-1, -1, -1}; }
    case -3: { printf("mpi hex string write failed\n"); return {-1, -1, -1}; }
    default: {
        printf("ok\n");
        return {req_time, comp_time, submit_time};
    }
    };
}

void log_config(std::ofstream &f, struct config *c)
{
    f   << c->num_bits << ", "
        << c->private_set_digits << ", "
        << c->private_set_size << ", "
        << c->min_expected_cycles << ", "
        << c->T_exp << ", "
        << c->T_baseline_comp << ", "
        << c->T_input_size_comp << ", "
        << c->T_private_set_comp << ", "
        << set_config(c) << ", ";
}

void run_config(std::ofstream &f, struct config *c) {
    log_config(f, c);
    std::vector<uint64_t> elements = {1};
    auto [rt, ct, st] = make_request(elements);
    f << elements.size() << ", " << rt << ", " << ct << ", " << st << std::endl;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            elements.push_back(i*j);
        }
        auto [rt, ct, st] = make_request(elements);
        log_config(f, c);
        f << elements.size() << ", " << rt << ", " << ct << ", " << st << std::endl;
    }
}

void run_tests()
{
    std::ofstream config_data("data/config.csv");
    if (!config_data.is_open()) {
        std::cerr << "Failed to open file!\n";
        return;
    }
    config_data << "num_bits, private_set_digits, private_set_size, min_expected_cycles, T_exp, T_baseline_comp, T_input_size_comp, T_private_set_comp, ConfigTime, RequestSize, RequestTime, SolveTime, SubmitTime\n";
    /*
    *   struct config {
	*       uint64_t num_bits;
    *       uint64_t private_set_digits;
	*       uint64_t private_set_size;
    *       uint64_t min_expected_cycles;
	*       int T_exp;
	*       int T_baseline_comp;
	*       int T_input_size_comp;
	*       int T_private_set_comp;
    *   };
    */

    struct config c = BASE_CONFIG;
    for (uint64_t t_exp = 1; t_exp <= 12; t_exp++) {
        for (int bc = 0; bc < 1; bc++) {
            for (int isc = 0; isc < 2; isc++) {
                for (int psc = 0; psc < 1; psc++) {
                    c.T_exp = t_exp;
                    c.T_baseline_comp = bc;
                    c.T_input_size_comp = isc;
                    c.T_private_set_comp = psc;
                    run_config(config_data, &c);
                }
            }
        }
    }
    c = BASE_CONFIG;

    config_data.close();
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

    std::ofstream encl_data("data/encl_data.csv");
    if (!encl_data.is_open()) {
        std::cerr << "Failed to open file\n";
        return -1;
    }
    encl_data << "InitTime[ms], TeardownTime[ms]\n";

    int64_t init_time = init_enclave();
    encl_data << init_time << ", ";

    run_tests();

    int64_t tdwn_time = teardown_enclave();
    encl_data << tdwn_time << std::endl;

    sgx_destroy_enclave(global_eid);
    
    encl_data.close();
    printf("Enter a character before exit ...\n");
    getchar();
    return 0;
}

