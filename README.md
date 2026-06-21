# Rate-Limiting in Trusted Execution Environments

> **tl;dr** Bachelor's thesis repo with two TLP-based rate-limiting prototypes: one for VM-level TEEs (AMD SEV-SNP), one for enclave-level TEEs (Intel SGX SDK).

---

This repository contains the code produced as part of my bachelor's thesis, exploring **rate-limiting** mechanisms enforced inside Trusted Execution Environments (TEEs) using **Time-Lock Puzzles (TLPs)**.

## Structure

### `TLP_VM_TEE/`

A rate-limited **Private Set Membership** interface designed to run inside a VM-level TEE, targeting platforms like AMD SEV-SNP or Intel TDX. The focus here is on a **proof of concept** for TLP-based rate-limiting: the service enforces rate-limits in a way that is integrity-protected by the confidential VM boundary, preventing a malicious host from bypassing or manipulating the limiting logic.

### `TLP_INTEL_SGX/`

An Intel SGX implementation built using the Intel SGX SDK. The protected interface is a **Private Set Intersection** service. This implementation is focused on experimentation with security parameters in a realistic TEE setting: varying the difficulty of the puzzle, trying different configurations and evaluating the resulting latencies.

### Time-Lock Puzzle Construction

The TLP construction used is repeated squaring in an RSA group, the classic Rivest-Shamir-Wagner time-lock puzzle. 
Verification is cheap (a single modular exponentiation using the precomputed exponent `e = 2^T mod phi(N)`), while solving requires `T` sequential squarings that cannot be meaningfully parallelized.

The TEE does not store any state for a client, the basis for the puzzle is recomputed deterministically from the requested element.

## About
 
| | |
|---|---|
| **Author** | Moritz Mantel |
| **Institution** | ETH Zurich |
| **Degree** | BSc Computer Science |
| **Year** | 2026 |
| **Supervisor** | Nicolas Dutly |