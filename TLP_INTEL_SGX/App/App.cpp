#include <stdio.h>
#include <string.h>
#include <assert.h>

#include <string>
#include <iostream>
#include <fstream>
#include <chrono>
#include <tuple>
#include <vector>
#include <x86intrin.h>

#include <unistd.h>
#include <pwd.h>

#define NUM_BITS 1024
#define MPI_STR_SIZE NUM_BITS

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

#include <mbedtls/bignum.h>

/*
 * if no config is specified when running, this is used.
 */
#define BASE_CONFIG {1024, 4, 100, 3000000, 15, 0, 0, 0}

sgx_enclave_id_t global_eid = 0;

/*
 * Error handling, as per Intel SGX SDK template.
 */
typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug;
} sgx_errlist_t;

static sgx_errlist_t sgx_errlist[] = {
    { SGX_ERROR_UNEXPECTED,        "Unexpected error occurred.",                NULL },
    { SGX_ERROR_INVALID_PARAMETER, "Invalid parameter.",                        NULL },
    { SGX_ERROR_OUT_OF_MEMORY,     "Out of memory.",                            NULL },
    { SGX_ERROR_ENCLAVE_LOST,      "Power transition occurred.",                "Please refer to the sample \"PowerTransition\" for details." },
    { SGX_ERROR_INVALID_ENCLAVE,   "Invalid enclave image.",                    NULL },
    { SGX_ERROR_INVALID_ENCLAVE_ID,"Invalid enclave identification.",            NULL },
    { SGX_ERROR_INVALID_SIGNATURE, "Invalid enclave signature.",                NULL },
    { SGX_ERROR_OUT_OF_EPC,        "Out of EPC memory.",                        NULL },
    { SGX_ERROR_NO_DEVICE,         "Invalid SGX device.",                       "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards." },
    { SGX_ERROR_MEMORY_MAP_CONFLICT,"Memory map conflicted.",                   NULL },
    { SGX_ERROR_INVALID_METADATA,  "Invalid enclave metadata.",                 NULL },
    { SGX_ERROR_DEVICE_BUSY,       "SGX device was busy.",                      NULL },
    { SGX_ERROR_INVALID_VERSION,   "Enclave version was invalid.",              NULL },
    { SGX_ERROR_INVALID_ATTRIBUTE, "Enclave was not authorized.",               NULL },
    { SGX_ERROR_ENCLAVE_FILE_ACCESS,"Can't open enclave file.",                 NULL },
    { SGX_ERROR_NDEBUG_ENCLAVE,    "The enclave is signed as product enclave, and can not be created as debuggable enclave.", NULL },
    { SGX_ERROR_MEMORY_MAP_FAILURE,"Failed to reserve memory for the enclave.", NULL },
};

void print_error_message(sgx_status_t ret)
{
    size_t ttl = sizeof sgx_errlist / sizeof sgx_errlist[0];
    for (size_t idx = 0; idx < ttl; idx++) {
        if (ret == sgx_errlist[idx].err) {
            if (sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            return;
        }
    }
    printf("Error: Unexpected error occurred.\n");
}

/* ------------------------------------------------------------------ */
/*  SGX setup                                                           */
/* ------------------------------------------------------------------ */

int initialize_enclave(void)
{
    sgx_status_t ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG,
                                          NULL, NULL, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        return -1;
    }
    return 0;
}

void ocall_print_string(const char *str)
{
    printf("%s", str);
}

/*
 * Returns the current number of cycles
 */
uint64_t get_cycles()
{
    unsigned aux;
    return __rdtscp(&aux);
}

/*
 * Computes the TLP given by x, N and T.
 * Returns the solution y = x^2^T mod N.
 */
void compute_puzzle_internal(char *y, char *x, char *N, uint64_t T)
{
    mbedtls_mpi x_mpi, N_mpi;
    mbedtls_mpi_init(&x_mpi);
    mbedtls_mpi_init(&N_mpi);

    if (mbedtls_mpi_read_string(&x_mpi, 16, x) != 0) printf("x read failed\n");
    if (mbedtls_mpi_read_string(&N_mpi, 16, N) != 0) printf("N read failed\n");

    for (uint64_t i = 0; i < T; i++) {
        if (mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi) != 0) printf("x mult failed\n");
        if (mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi) != 0) printf("x mod failed\n");
    }

    size_t written;
    mbedtls_mpi_write_string(&x_mpi, 16, y, MPI_STR_SIZE, &written);

    mbedtls_mpi_free(&x_mpi);
    mbedtls_mpi_free(&N_mpi);
}

/*
 * Computes the TLP given by x, N and T.
 * Returns the solution y = x^2^T mod N.
 * 
 * Additionally records the cycles.
 */
void compute_puzzle_internal_cycles(char *y, char *x, char *N, uint64_t T, std::ostream &f)
{
    mbedtls_mpi x_mpi, N_mpi;
    mbedtls_mpi_init(&x_mpi);
    mbedtls_mpi_init(&N_mpi);

    if (mbedtls_mpi_read_string(&x_mpi, 16, x) != 0) printf("x read failed\n");
    if (mbedtls_mpi_read_string(&N_mpi, 16, N) != 0) printf("N read failed\n");

    for (uint64_t i = 0; i < T; i++) {
        uint64_t start = get_cycles();
        if (mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi) != 0) printf("x mult failed\n");
        if (mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi) != 0) printf("x mod failed\n");
        uint64_t end = get_cycles();
        f << T << "," << i << "," << (end - start) << "\n";
    }

    size_t written;
    mbedtls_mpi_write_string(&x_mpi, 16, y, MPI_STR_SIZE, &written);

    mbedtls_mpi_free(&x_mpi);
    mbedtls_mpi_free(&N_mpi);
}

/*
 * Runs the full TLP-based protocol once:
 *      1. request a puzzle
 *      2. solve the puzzle
 *      3. submit the solution
 *
 * log levels:
 *      0  silent
 *      1  print result to stdout
 *      2  append one CSV row to log_file
 *      3  append one CSV row + write per-squaring cycle counts
 *
 * Returns 
 *      * 0             on success, 
 *      * negative      on error (propagates Enclave error)
 */
int run_request(std::vector<uint64_t> &elements, int log,
                const std::string &log_file, struct config &c)
{
    char x[MPI_STR_SIZE] = {0};
    char N[MPI_STR_SIZE] = {0};
    char sol[MPI_STR_SIZE] = {0};
    uint64_t T = 0;
    int n = elements.size();

    /* 
     * 1. request puzzle
     */
    int req_status;
    auto t0 = std::chrono::steady_clock::now();
    ecall_request_puzzle(global_eid, &req_status, x, N, &T,
                         n * sizeof(uint64_t), elements.data());
    auto t1 = std::chrono::steady_clock::now();
    if (req_status != 0) {
        printf("ecall_request_puzzle failed: %d\n", req_status);
        return -1;
    }
    int64_t req_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    if (log == 4) {
        std::cout << "Puzzle:" << std::endl
                << "x: " << x << std::endl
                << "T: " << T << std::endl
                << "N: " << N << std::endl
                << std::endl;
    }

    /*
     * 2. solve the puzzle
     */
    auto t2 = std::chrono::steady_clock::now();
    if (log == 3) {
        std::string cycles_file = log_file + ".cycles.csv";
        bool write_header = !std::ifstream(cycles_file).good();
        std::ofstream cf(cycles_file, std::ios::app);
        if (!cf) { printf("failed to open cycles file\n"); return -2; }
        if (write_header)
            cf << "T,Iteration,Cycles\n";
        compute_puzzle_internal_cycles(sol, x, N, T, cf);
    } else {
        compute_puzzle_internal(sol, x, N, T);
    }
    auto t3 = std::chrono::steady_clock::now();
    int64_t solve_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count();

    /*
     * 3. submit the solution
     */
    int ret_val;
    uint64_t intersection[100] = {0};
    auto t4 = std::chrono::steady_clock::now();
    ecall_submit_solution(global_eid, &ret_val, intersection,
                          n * sizeof(uint64_t), elements.data(), sol);
    auto t5 = std::chrono::steady_clock::now();
    int64_t submit_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count();

    if (ret_val < 0) {
        printf("ecall_submit_solution failed: %d\n", ret_val);
        return -3;
    }
    int intersection_size = ret_val;

    /* 
     * logging
     */
    if (log == 1) {
        printf("success, intersection size: %d, elements: ", intersection_size);
        for (int i = 0; i < intersection_size; i++)
            printf("%lu ", intersection[i]);
        printf("\nrequest: %ldms, solve: %ldms, submit: %ldms\n",
               req_ms, solve_ms, submit_ms);
    }

    if (log >= 2 && log != 4) {
        bool write_header = !std::ifstream(log_file).good();
        std::ofstream f(log_file, std::ios::app);
        if (!f) { printf("failed to open log file\n"); return -4; }
        if (write_header)
            f << "num_bits,private_set_digits,private_set_size,min_expected_cycles,"
                 "T_exp,T_baseline_comp,T_input_size_comp,T_private_set_comp,"
                 "RequestSize,RequestTime,SolveTime,SubmitTime,IntersectionSize\n";
        f << c.num_bits << "," << c.private_set_digits << "," << c.private_set_size << ","
          << c.min_expected_cycles << "," << c.T_exp << "," << c.T_baseline_comp << ","
          << c.T_input_size_comp << "," << c.T_private_set_comp << ","
          << n << ","
          << req_ms << "," << solve_ms << "," << submit_ms << ","
          << intersection_size << "\n";
    }

    return 0;
}

/*
 * Run one request, as specified in the following CLI:
 *
 * ./app 
 *      <do_config> 
 *      if <do_config>:
 *          [config fields] 
 *      <log> 
 *      if <log>:
 *          [filename] 
 *      <n> 
 *      <e_1,e_2, ... ,e_n>
 */
int SGX_CDECL main(int argc, char *argv[])
{
    int idx = 1;

    if (argc < 2) {
        printf("usage: ./app <do_config> [config fields] <log> [filename] <n> <e_1,e_2, ... ,e_n>\n");
        return -1;
    }
    
    /*
     * If custom config provided, extract fields.
     */
    bool do_config = std::stoi(argv[idx++]);
    struct config c = BASE_CONFIG;
    if (do_config) {
        if (argc < idx + 8) { printf("expected 8 config fields\n"); return -1; }
        c.num_bits            = std::stoull(argv[idx++]);
        c.private_set_digits  = std::stoull(argv[idx++]);
        c.private_set_size    = std::stoull(argv[idx++]);
        c.min_expected_cycles = std::stoull(argv[idx++]);
        c.T_exp               = std::stoi(argv[idx++]);
        c.T_baseline_comp     = std::stoi(argv[idx++]);
        c.T_input_size_comp   = std::stoi(argv[idx++]);
        c.T_private_set_comp  = std::stoi(argv[idx++]);
    }

    /*
     * Extract log level
     */
    int log = std::stoi(argv[idx++]);
    std::string log_file = "";
    if (log >= 2 && log != 4) {
        if (argc < idx + 1) { printf("expected log filename\n"); return -1; }
        log_file = "evaluation/data/" + std::string(argv[idx++]) + ".csv";
    }

    /*
     * Extract requested elements
     */
    int n = std::stoi(argv[idx++]);
    if (n < 1 || n > 100) { printf("n must be 1..100\n"); return -1; }
    if (argc < idx + n)   { printf("expected %d elements\n", n); return -1; }

    std::vector<uint64_t> elements;
    for (int i = 0; i < n; i++)
        elements.push_back(std::stoull(argv[idx++]));

    
    /* 
     * Enclave gets set up
     */
    if (initialize_enclave() < 0) {
        printf("enclave initialisation failed\n");
        return -1;
    }

    /*
     * Config gets set.
     */
    int64_t config_ms = 0;
    int ret_status;
    auto tc0 = std::chrono::steady_clock::now();
    ecall_set_config(global_eid, &ret_status, &c);
    auto tc1 = std::chrono::steady_clock::now();
    config_ms = std::chrono::duration_cast<std::chrono::milliseconds>(tc1 - tc0).count();
    if (ret_status != 0) {
        printf("ecall_set_config failed: %d\n", ret_status);
        sgx_destroy_enclave(global_eid);
        return -1;
    }

    /*
     * Enclave initialises crypto state
     */
    int ret_init;
    auto ti0 = std::chrono::steady_clock::now();
    ecall_init(global_eid, &ret_init);
    auto ti1 = std::chrono::steady_clock::now();
    int64_t init_ms = std::chrono::duration_cast<std::chrono::milliseconds>(ti1 - ti0).count();
    if (ret_init != 0) {
        printf("ecall_init failed: %d\n", ret_init);
        sgx_destroy_enclave(global_eid);
        return -1;
    }

    /*
     * The request (as per CLI) gets run
     */
    int ret = run_request(elements, log, log_file, c);

    /*
     * Free all enclave state
     */
    auto td0 = std::chrono::steady_clock::now();
    ecall_teardown(global_eid);
    auto td1 = std::chrono::steady_clock::now();
    int64_t teardown_ms = std::chrono::duration_cast<std::chrono::milliseconds>(td1 - td0).count();

    /*
     * Log setup / teardown stats
     */
    if (log >= 2 && log != 4) {
        bool write_encl_header = !std::ifstream("evaluation/data/encl_data.csv").good();
        std::ofstream encl_data("evaluation/data/encl_data.csv", std::ios::app);
        if (write_encl_header)
            encl_data << "SetConfigTime,InitTime,TeardownTime\n";
        encl_data << config_ms << "," << init_ms << "," << teardown_ms << "\n";
    }

    /*
     * Shut down enclave.
     */
    sgx_destroy_enclave(global_eid);
    return ret;
}