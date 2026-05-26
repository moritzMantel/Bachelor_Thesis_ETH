# TLP Rate-Limiting - Intel SGX (SDK)

## Overview

This implementation targets enclave-level TEEs using the Intel SGX SDK. The focus here is not on a deployable server but on evaluating how different parameter choices affect the construction's security and performance.

The protected service is a Private Set Intersection (PSI): the enclave holds a private set, and clients can query which of their requested elements appear in it, but only after solving a Time-Lock Puzzle (TLP). The hardness of that puzzle is configurable in several dimensions and the app is built to sweep through combinations of those parameters and log results.

The TLP construction is the same repeated-squaring time-lock puzzle, now implemented using mbedTLS inside the enclave.

## Enclave (`Enclave/Enclave.cpp`)

All cryptographic state lives inside the enclave. The trusted code exposes the following ECALLs to the untrusted host:

- `ecall_init` generates the RSA modulus, computes `phi(N)`, precomputes the verification exponent `e = 2^T mod phi(N)`, and initializes the private set
- `ecall_set_config` overwrites the active configuration (debug builds only); triggers a re-initialization of `T` and the private set
- `ecall_request_puzzle` returns a puzzle `(x, N, T)` for a given array of requested elements; `x` is derived by hashing the element array with SHA-256
- `ecall_submit_solution` verifies a submitted solution and, if accepted, returns the intersection of the requested elements with the private set
- `ecall_teardown` frees all enclave-side state

`T` can be set as a fixed constant, scaled to meet a minimum expected cycle count, or scaled linearly with the number of requested elements, controlled by the `T_baseline_comp`, `T_input_size_comp`, and `T_private_set_comp` fields of `struct config`.

## Host App (`App/App.cpp`)

The untrusted host manages the enclave lifecycle, drives the TLP solve, and handles all benchmarking. The main flow is:

1. `initialize_enclave` loads and initializes the SGX enclave
2. `run_tests` sweeps through all parameter combinations, calling `run_config` for each
3. For each config, `make_request` does the full round trip: request puzzle, solve it untrusted-side, submit solution
4. Timing for each phase (request, solve, submit) is recorded and written to `data/<filename>.csv`

The puzzle is solved entirely on the untrusted side via `compute_puzzle_internal`, which runs the `T` squarings in a loop using mbedTLS.

## Configuration

The `struct config` struct controls all security parameters:

| Field | Description |
|---|---|
| `num_bits` | Bit-length of each prime factor of N |
| `private_set_size` / `private_set_digits` | Size and domain of the server's private set |
| `T_exp` | Base hardness: `T = 2^T_exp` squarings |
| `T_baseline_comp` | How T_base is set: fixed vs. minimum-cycles-driven |
| `T_input_size_comp` | Whether T scales linearly with request size |
| `T_private_set_comp` | Whether T accounts for private set density |
| `min_expected_cycles` | Target cycle budget per request (used by some T modes) |

## Dependencies

- [Intel SGX SDK](https://github.com/intel/linux-sgx)
- [mbedTLS](https://github.com/Mbed-TLS/mbedtls) -- big integer arithmetic, SHA-256, prime generation (inside and outside the enclave)