# VDF Rate-Limiting -- AMD SEV-SNP (VM-level TEE)

> **tl;dr** An HTTP server that enforces VDF-based rate limiting. Clients must solve a time-lock puzzle before accessing the service endpoint. Intended to run inside a confidential VM (e.g. AMD SEV-SNP) so the host cannot tamper with the puzzle parameters or skip verification.

---

## Overview

The server exposes two endpoints and uses a Verifiable Delay Function (VDF) as a rate-limiting primitive: before a client can call the actual service, it must solve a puzzle that takes a bounded amount of sequential computation. Because the puzzle parameters are generated and verified server-side inside the TEE, a malicious host cannot hand out easy puzzles or bypass verification.

The VDF construction used is repeated squaring in an RSA group, the classic Rivest-Shamir-Wagner time-lock puzzle. 
Verification is cheap (a single modular exponentiation using the precomputed exponent `e = 2^T mod phi(N)`), while solving requires `T` sequential squarings that cannot be meaningfully parallelized.

The server does not store any state for a client, the basis for the puzzle is recomputed deterministically from the requested element.

## Endpoints

### `GET /request_vdf?element=<n>`

Returns a puzzle `(x, T, N)` for the requested element. The client must compute `y = x^(2^T) mod N` before it can proceed.

- `x` is derived from the requested element, keeping it deterministic for verification.
- `T` is the hardness parameter (number of squarings)
- `N` is the RSA modulus, generated fresh at server startup and fixed for the lifetime of the process

### `GET /solve?element=<n>&vdf_solve=<y>`

Verifies the submitted solution `y` against the expected answer. Verification is done by checking `y == x^e mod N`, where `e = 2^T mod phi(N)` is precomputed during initialization. If the solution is correct, the actual service result is returned.

## Code Overview

### `src/server.c`

Entry point and HTTP request handling.

- `main()` initializes rate limiting, starts the libmicrohttpd daemon with a thread pool, and blocks until input
- `request_handler()` dispatches incoming requests to `/request_vdf` or `/solve`, parses query parameters, and builds the response

### `src/rate_limiting.c`

Core VDF logic: RSA group setup, puzzle generation, and verification.

- `rate_limiting_init()` generates the RSA modulus `N = p*q`, computes `phi(N)`, and precomputes the verification exponent `e = 2^T mod phi(N)`
- `rate_limiting_clean_up()` frees all OpenSSL BIGNUM state
- `generate_puzzle()` constructs a puzzle `(x, T, N)` for a given element; `x` is clamped to `element % MAX_BASE`
- `verify_puzzle()` checks a submitted solution `y` by comparing it to `x^e mod N`

### `src/service.c`

Stub for the actual protected service. `service()` returns the result for a given element. 
Here it simply implements the "private set" of all even numbers.

### `test/client.py`

Reference client in Python. Requests a puzzle, solves it with `pow(x, 2**T, N)`, and submits the solution.

## Client

`client.py` is a minimal Python client demonstrating the full flow: request a puzzle, solve it locally, submit the solution.

```
python3 client.py
```

The solve step uses Python's built-in `pow(x, 2**T, N)`, which is not competitive with a native C implementation but is sufficient for testing correctness.

## Dependencies

- [libmicrohttpd](https://www.gnu.org/software/libmicrohttpd/) - HTTP server
- [OpenSSL](https://www.openssl.org/) (`libcrypto`) - big integer arithmetic via `BIGNUM`