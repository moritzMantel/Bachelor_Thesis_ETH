#include <chrono>
#include <mbedtls/bignum.h>
#include <openssl/bn.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <x86intrin.h>

#define X_HEX "A111F275CC2E7588000001D300A31E76336D15B9D314CD1A1D8F3D3556975EED"
#define N_HEX "A708C278C8067461B50CD7F104FE4A612A8C53049AE543802914515155A753CFF168C03A64B793FE325C3FC7BEBFB4BFB405AF92EA6993689CA93C71378B9FFD2EFEC803BB61C3B997F7D7D170BEE8D8DF5F943E2D77D936E8946BFC10C59E577F16E90BE67C3185637C9B8AFFD58F7B11F21A6659160C38D6B0E7F7245680860DAF92C12B64E13FFAF10B6A7FC717E905CD19D728FE1F4BA94FFFA6AD9CDDDB58B62631B4FD3E816BF99311132DFB3D0F99596AD1E115EDED8A315E051B12C5BF1A1BA40A2E090CAA4D6B45CF22BD820FF52CA22231A36F63EA5E57623CF8E61A66C611C57BF08902894E75ACEE14C16CC44BBFA86A398708A523DC92F46781"

/*
 * requires an x86_64 architecture
 */
uint64_t get_cycles()
{
	unsigned aux;
	return __rdtscp(&aux);
}

uint64_t measure_mbedtls_avg_cycles(uint64_t T)
{
	mbedtls_mpi x_mpi, N_mpi;
    	mbedtls_mpi_init(&x_mpi);
    	mbedtls_mpi_init(&N_mpi);

    	mbedtls_mpi_read_string(&x_mpi, 16, X_HEX);
    	mbedtls_mpi_read_string(&N_mpi, 16, N_HEX);

	uint64_t total_cycles = 0;

	for (uint64_t i = 0; i < T; i++) {
		uint64_t c0 = get_cycles();
        	mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi);
        	mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi);
		uint64_t c1 = get_cycles();

		total_cycles += (c1 - c0);
    	}


    	mbedtls_mpi_free(&x_mpi);
    	mbedtls_mpi_free(&N_mpi);

	return total_cycles / T;
}

uint64_t measure_openssl_avg_cycles(uint64_t T)
{
	BIGNUM *x_BN = BN_new();
	BIGNUM *N_BN = BN_new();
	BN_CTX *ctx = BN_CTX_new();

	BN_hex2bn(&x_BN, X_HEX);
	BN_hex2bn(&N_BN, N_HEX);

	uint64_t total_cycles = 0;

	for (uint64_t i = 0; i < T; i++) {

		uint64_t c0 = get_cycles();
		BN_mul(x_BN, x_BN, x_BN, ctx);
		BN_mod(x_BN, x_BN, N_BN, ctx);
		uint64_t c1 = get_cycles();

		total_cycles += (c1 - c0);
    	}

	BN_free(x_BN);
	BN_free(N_BN);
	BN_CTX_free(ctx);

	return total_cycles / T;
}

uint64_t measure_mbedtls_total_cycles(uint64_t T)
{
	mbedtls_mpi x_mpi, N_mpi;
    	mbedtls_mpi_init(&x_mpi);
    	mbedtls_mpi_init(&N_mpi);

    	mbedtls_mpi_read_string(&x_mpi, 16, X_HEX);
    	mbedtls_mpi_read_string(&N_mpi, 16, N_HEX);

	uint64_t c0 = get_cycles();

	for (uint64_t i = 0; i < T; i++) {
        	mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi);
        	mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi);
    	}
	uint64_t c1 = get_cycles();

    	mbedtls_mpi_free(&x_mpi);
    	mbedtls_mpi_free(&N_mpi);

	return c1 - c0;
}

uint64_t measure_openssl_total_cycles(uint64_t T)
{
	BIGNUM *x_BN = BN_new();
	BIGNUM *N_BN = BN_new();
	BN_CTX *ctx = BN_CTX_new();

	BN_hex2bn(&x_BN, X_HEX);
	BN_hex2bn(&N_BN, N_HEX);

	uint64_t c0 = get_cycles();

	for (uint64_t i = 0; i < T; i++) {
		BN_mul(x_BN, x_BN, x_BN, ctx);
		BN_mod(x_BN, x_BN, N_BN, ctx);
	}
	uint64_t c1 = get_cycles();

	BN_free(x_BN);
	BN_free(N_BN);
	BN_CTX_free(ctx);

	return c1 - c0;
}

uint64_t measure_mbedtls_loop(uint64_t T)
{
	mbedtls_mpi x_mpi, N_mpi;
    	mbedtls_mpi_init(&x_mpi);
    	mbedtls_mpi_init(&N_mpi);

    	mbedtls_mpi_read_string(&x_mpi, 16, X_HEX);
    	mbedtls_mpi_read_string(&N_mpi, 16, N_HEX);

	auto t0 = std::chrono::steady_clock::now();

	for (uint64_t i = 0; i < T; i++) {
        	mbedtls_mpi_mul_mpi(&x_mpi, &x_mpi, &x_mpi);
        	mbedtls_mpi_mod_mpi(&x_mpi, &x_mpi, &N_mpi);
    	}

	auto t1 = std::chrono::steady_clock::now();

    	mbedtls_mpi_free(&x_mpi);
    	mbedtls_mpi_free(&N_mpi);

	return (std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

uint64_t measure_openssl_loop(uint64_t T)
{
	BIGNUM *x_BN = BN_new();
	BIGNUM *N_BN = BN_new();
	BN_CTX *ctx = BN_CTX_new();

	BN_hex2bn(&x_BN, X_HEX);
	BN_hex2bn(&N_BN, N_HEX);

	auto t0 = std::chrono::steady_clock::now();

	for (uint64_t i = 0; i < T; i++) {
		BN_mul(x_BN, x_BN, x_BN, ctx);
		BN_mod(x_BN, x_BN, N_BN, ctx);
    	}

	auto t1 = std::chrono::steady_clock::now();

	BN_free(x_BN);
	BN_free(N_BN);
	BN_CTX_free(ctx);

	return (std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

uint64_t measure_openssl_exp(uint64_t T)
{
	BIGNUM *x_BN = BN_new();
	BIGNUM *N_BN = BN_new();
	BIGNUM *two_BN = BN_new();
	BIGNUM *T_BN = BN_new();
	BN_CTX *ctx = BN_CTX_new();

	BN_set_word(two_BN, 2);
	BN_set_word(T_BN, T);

	BN_hex2bn(&x_BN, X_HEX);
	BN_hex2bn(&N_BN, N_HEX);

	BN_exp(T_BN, two_BN, T_BN, ctx);

	auto t0 = std::chrono::steady_clock::now();

	BN_mod_exp(x_BN, x_BN, T_BN, N_BN, ctx);

	auto t1 = std::chrono::steady_clock::now();

	BN_free(x_BN);
	BN_free(N_BN);
	BN_free(two_BN);
	BN_free(T_BN);
	BN_CTX_free(ctx);

	return (std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

int main()
{
	{
	bool write_header = !std::ifstream("evaluation/data/impl_solve_vm.csv").good();
        std::ofstream file("evaluation/data/impl_solve_vm.csv", std::ios::app);
        if (write_header)
            file << "Impl,T,Time\n";

	for (int i = 5; i < 25; i++) {
		std::cout << "Iteration " << i << "\n";
		uint64_t T = (uint64_t) std::pow(2, i);
		uint64_t mbedtls = measure_mbedtls_loop(T);
		uint64_t openssl_loop = measure_openssl_loop(T);
		uint64_t openssl_exp = measure_openssl_exp(T);
		file << "mbedtls," << T << "," << mbedtls << "\n";
		file << "openssl_loop," << T << "," << openssl_loop << "\n";
		file << "openssl_exp," << T << "," << openssl_exp << "\n";
	}
	}
	{
	bool write_header = !std::ifstream("evaluation/data/impl_cycles.csv").good();
        std::ofstream file("evaluation/data/impl_cycles.csv", std::ios::app);
        if (write_header)
            file << "Impl,T,AvgCycles\n";
	for (int i = 5; i < 25; i++) {
		std::cout << "Iteration " << i << "\n";

		uint64_t T = (uint64_t) pow(2, i);
		file << "mbedtls," << T << "," << measure_mbedtls_avg_cycles(T) << "\n";
		file << "openssl," << T << "," << measure_openssl_avg_cycles(T) << "\n";
	}
	}

	{
	bool write_header = !std::ifstream("evaluation/data/impl_cycles_total.csv").good();
        std::ofstream file("evaluation/data/impl_cycles_total.csv", std::ios::app);
        if (write_header)
            file << "Impl,T,TotalCycles\n";
	for (int i = 5; i < 25; i++) {
		std::cout << "Iteration " << i << "\n";

		uint64_t T = (uint64_t) pow(2, i);
		file << "mbedtls," << T << "," << measure_mbedtls_total_cycles(T) << "\n";
		file << "openssl," << T << "," << measure_openssl_total_cycles(T) << "\n";
	}
	}
}
