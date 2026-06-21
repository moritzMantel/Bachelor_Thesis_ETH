# TLP Rate-Limiting: Intel SGX
> **tl;dr** A Private Set Intersection service that enforces TLP-based rate-limiting inside an Intel SGX enclave. Clients must solve a Time-Lock Puzzle before the enclave reveals the intersection result. The enclave generates and verifies puzzle parameters, so a malicious host cannot hand out easy puzzles or bypass verification.

---

## Overview

The service uses a Time-Lock Puzzle (TLP) as a rate-limiting primitive: before a client can query the private set, it must solve a puzzle that takes a minimum amount of sequential computation. Puzzle generation and verification happen entirely inside the SGX enclave, so the untrusted host cannot observe or tamper with the private set, the puzzle parameters, or the verification exponent.

The enclave exposes three ECALLs: `ecall_request_puzzle` to obtain a puzzle, and `ecall_submit_solution` to submit the solution and receive the intersection result. `ecall_set_config` and `ecall_init` handle setup.

---

## Enclave Interface (ECALLs)

### `int ecall_set_config(struct config *c)`

Overwrites the default configuration. Must be called before `ecall_init`. The config controls the modulus size, private set parameters, and T computation mode. Returns 0 on success, negative on invalid parameters.

### `int ecall_init()`

Generates the RSA modulus `N = p*q`, computes `phi(N)`, and precomputes the verification exponent `e = 2^T mod phi(N)`. Must be called before any puzzle requests. Returns 0 on success, negative on failure.

### `int ecall_request_puzzle(char *x_buf, char *N_buf, uint64_t *T, int s, uint64_t *elems)`

Generates a puzzle `(x, T, N)` for the requested element set. The base `x` is derived deterministically from the (sorted) requested elements via SHA-256, ensuring stateless verification. The client must compute `y = x^(2^T) mod N`.

### `int ecall_submit_solution(uint64_t *result, int s, uint64_t *elems, char *y_str)`

Verifies the submitted solution `y` and, if correct, returns the intersection of the requested elements with the private set. Verification recomputes `x^e mod N` inside the enclave and compares to `y` using a constant-time comparison to avoid timing side channels.

### Lifecycle

```c
ecall_set_config(eid, &ret, &c);   // optional, overwrites defaults
ecall_init(eid, &ret);             // generates RSA group, must be called first
// ... ecall_request_puzzle / ecall_submit_solution ...
ecall_teardown(eid);               // frees enclave crypto state
```

---

## Build

Requires the [Intel SGX SDK](https://github.com/intel/linux-sgx) installed at `/opt/intel/sgxsdk`.

```
make                        # HW debug mode (default)
make SGX_MODE=SIM           # simulation mode (no SGX hardware required)
make SGX_DEBUG=0            # release mode (requires manual enclave signing)
make clean
```

The build produces `app` (untrusted host binary) and `enclave.signed.so` (signed enclave image).

---

## Source Files

| File | Description |
|---|---|
| `App/App.cpp` | Untrusted host application: enclave lifecycle, CLI parsing, puzzle solving, CSV logging |
| `Enclave/Enclave.cpp` | Core enclave logic: `ecall_init`, `ecall_teardown`, `ecall_request_puzzle`, `ecall_submit_solution`, `generate_base`, `const_time_memcmp` |
| `Enclave/Enclave_config.cpp` | Configuration management: T computation modes (constant, min-cycles, input-size scaling, private-set scaling) |
| `Enclave/Enclave_set.cpp` | Private set implementation: dummy set of evenly spaced integers, intersection computation |
| `Enclave/Enclave.edl` | EDL interface definition: ECALL/OCALL declarations and buffer annotations |

---

## App CLI

The `app` binary runs one full protocol request (puzzle generation, solving, submission) per invocation. It is primarily used by `experiments.py` but can also be called directly.

```
./app <do_config> [config fields] <log> [filename] <n> <e_1> ... <e_n>
```

| Argument | Description |
|---|---|
| `do_config` | 1 to supply custom config fields, 0 to use defaults |
| `[config fields]` | 8 fields in order: `num_bits private_set_digits private_set_size min_expected_cycles T_exp T_baseline_comp T_input_size_comp T_private_set_comp` |
| `log` | 0: silent, 1: print result, 2: append CSV row, 3: CSV + per-squaring cycles, 4: print puzzle details and result |
| `[filename]` | Base name for the output CSV (required when log >= 2, written to `evaluation/data/<filename>.csv`) |
| `n` | Number of requested elements (1..100) |
| `e_1 ... e_n` | The requested elements |

Examples:

```bash
# request element 42 with default config, print result
./app 0 1 1 42

# request elements 1 2 3 with custom T_exp=10, log to evaluation/data/test.csv
./app 1 1024 4 100 0 10 0 0 0 2 test 3 1 2 3
```

---

## Evaluation

All evaluation scripts are in the root of `TLP_INTEL_SGX/`.

### `experiments.py`

Drives the `app` binary to collect timing data across six experiments: varying T, input size, cycle counts per squaring, expected cycles, domain size, and private set size. Outputs CSVs to `evaluation/data/`.

```
python3 experiments.py
```

### `solver_experiments.cpp`

Benchmarks mbedTLS and OpenSSL puzzle solver implementations (loop-based and `mod_exp`-based) in wall-clock time and raw cycle counts, on x86-64. Outputs to `evaluation/data/`.

```
g++ solver_experiments.cpp -lmbedcrypto -lssl -lcrypto -o solver_experiments
./solver_experiments
```

### `sidechannel_experiment.cpp`

Measures timing of `const_time_memcmp` vs `mbedtls_mpi_cmp_mpi` as a function of matching prefix length, and timing of `mbedtls_mpi_exp_mod` as a function of exponent size. Outputs to `evaluation/data/`.

```
g++ sidechannel_experiment.cpp -lmbedcrypto -o sidechannel_experiment
./sidechannel_experiment
```

### `python_experiment.py`

Measures Python's built-in `pow(x, 2**T, N)` solver in milliseconds and appends results to `evaluation/data/impl_solve_vm.csv` for comparison with native implementations.

```
python3 python_experiment.py
```

### `plot_experiments.py`

Reads all CSVs from `evaluation/data/` and produces figures in `evaluation/figures/`.

```
python3 plot_experiments.py
```

---

## Dependencies

- [Intel SGX SDK](https://github.com/intel/linux-sgx) — enclave toolchain (`sgx_edger8r`, `sgx_sign`, trusted/untrusted runtime)
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) — big integer arithmetic inside and outside the enclave (`libmbedcrypto`)
- [OpenSSL](https://www.openssl.org/) (`libcrypto`) — alternative solver implementation in `solver_experiments.cpp`

Tested with: Intel SGX SDK 2.23.100.2, mbedTLS 3.5.0 (Intel SGX build), OpenSSL 3.0.16, gcc 11.4.0 on Ubuntu 22.04.4 (Intel Core i9-9900K).