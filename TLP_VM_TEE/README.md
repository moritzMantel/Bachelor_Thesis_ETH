# TLP Rate-Limiting: VM-level TEE
> **tl;dr** A Private Set Membership service that enforces TLP-based rate-limiting. Clients must solve a Time-Lock Puzzle before accessing the service. Intended to run inside a confidential VM (e.g. AMD SEV-SNP, Intel TDX) so the host cannot tamper with the puzzle parameters or skip verification.

---

## Overview

The service uses a Time-Lock Puzzle (TLP) as a rate-limiting primitive: before a client can call the actual service, it must solve a puzzle that takes a minimum amount of sequential computation. Because the puzzle parameters are generated and verified inside the TEE, a malicious host cannot hand out easy puzzles or bypass verification.

The implementation is exposed both as a **shared library** and as an **HTTP server**.

---

## Interface (Library)

The library interface is declared in `include/rate_limiting.h`. Callers must allocate a buffer of at least `buf_size()` bytes for the return value.

### `void request(char *ret_buf, unsigned long e)`

Generates a puzzle `(x, T, N)` for the requested element `e` and writes it as JSON into `ret_buf`:
```json
{"x":"<hex>","T":"<hex>","N":"<hex>"}
```
The client must compute `y = x^(2^T) mod N` before calling `request_sol`.

### `void request_sol(char *ret_buf, unsigned long e, char *Y)`

Verifies the submitted solution `Y` for element `e`. Writes into `ret_buf`:
- `"1"` if `e` is in the private set and `Y` is correct
- `"0"` if `e` is not in the private set and `Y` is correct
- `"NOT ACCEPTED: <code>"` if `Y` is not a valid solution
- `"NO MEMORY"` if allocation fails

### Lifecycle

```c
init();         // must be called before any request() or request_sol()
// ... use the library ...
teardown();     // must be called after last use
```

---

## Server

The HTTP server listens on port `8080` with a thread pool of 4 threads.

### `GET /request_TLP?element=<n>`

Returns a puzzle for element `n` in the format `x=<hex>;T=<hex>;N=<hex>;`.

### `GET /solve?element=<n>&y=<y>`

Verifies the submitted solution `y` for element `n`. On success, returns `Request:<n>;Result:<result>;`. On failure, returns HTTP 403.

---

## Source Files

| File | Description |
|---|---|
| `src/rate_limiting.c` | Core TLP logic: RSA group setup (`N = p*q`, `e = 2^T mod phi(N)`), puzzle generation, and constant-time verification via `BN_mod_exp_mont_consttime` and `CRYPTO_memcmp` |
| `src/library.c` | Library interface: wraps `rate_limiting.c` into `init`, `teardown`, `request`, `request_sol` |
| `src/server.c` | HTTP server: libmicrohttpd request handler dispatching to `/request_TLP` and `/solve` |
| `src/service.c` | Stub for the protected service; implements a dummy private set of all even numbers |

---

## Build

Requires `pkg-config`, `libmicrohttpd`, and `openssl` to be installed.

```
make        # builds both library and server
make lib    # shared library only (build/rate_limiter.dylib or .so)
make server # HTTP server only   (build/server)
make clean  # remove build/
```

Pass `MACROS='-Dtest_simple'` to build with small hardcoded primes and a low T for quick testing:

```
make MACROS='-Dtest_simple'
```

---

## Example Clients

Both clients are in `test/`.

### `test/client_lib.py` — library via ctypes

Loads `rate_limiter.dylib`, wraps the C interface in Python helpers, and runs the full protocol interactively:

```
python3 test/client_lib.py
Choose element...
> 42
Puzzle: {'x': '...', 'T': '...', 'N': '...'}
Solution: 0X...
42 is in S!
```

The script exposes `request(e)`, `solve(puzzle)`, and `submit(e, Y)` as standalone functions and can also be imported as a module.

### `test/client_server.py` — HTTP server

Runs the full protocol against the HTTP server (must be running on `localhost:8080`):

```
python3 test/client_server.py
```

Requests a puzzle from `/request_TLP`, solves it locally with `pow(x, 2**T, N)`, and submits to `/solve`. The script exposes `send_request`, `solve_TLP`, and `send_solve` as standalone functions.

---

## Dependencies

- [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) — HTTP server (server interface only)
- [OpenSSL](https://www.openssl.org/) (`libcrypto`) — big integer arithmetic via `BIGNUM`

Tested with: libmicrohttpd 1.0.2, OpenSSL 3.6.2, Apple Clang 21.0.0 on macOS 26 (Apple M2).