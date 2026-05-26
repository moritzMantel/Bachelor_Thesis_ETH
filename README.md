# Rate-Limiting in Trusted Execution Environments

> **tl;dr** Bachelor's thesis repo with two TLP-based rate-limiting implementations: one for VM-level TEEs (AMD SEV-SNP), one for enclave-level TEEs (Intel SGX SDK).

---

This repository contains the code produced as part of my bachelor's thesis, exploring rate-limiting mechanisms enforced inside Trusted Execution Environments (TEEs) using Time-Lock Puzzles (TLPs).

## Structure

### `TLP_VM_TEE/`

A rate-limited server endpoint designed to run inside a VM-level TEE, targeting platforms like AMD SEV-SNP or Intel TDX. The focus here is on a realistic deployment scenario: the server enforces rate limits in a way that is integrity-protected by the confidential VM boundary, preventing a malicious host from bypassing or manipulating the limiting logic.

### `TLP_INTEL_SGX/`

An Intel SGX implementation built using the official SGX SDK. This one is less focused on production deployment and more on experimentation with security parameters -- tweaking TLP difficulty, measuring overhead inside an enclave, and stress-testing the construction under different configurations.

## About
 
| | |
|---|---|
| **Author** | Moritz Mantel |
| **Institution** | ETH Zurich |
| **Degree** | BSc Computer Science |
| **Year** | 2026 |
| **Supervisor** | Nicolas Dutly |