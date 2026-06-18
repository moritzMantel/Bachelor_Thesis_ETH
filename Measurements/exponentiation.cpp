#include <chrono>
#include <mbedtls/bignum.h>
#include <openssl/bn.h>
#include <cmath>
#include <fstream>
#include <iostream>


#define x "A111F275CC2E7588000001D300A31E76336D15B9D314CD1A1D8F3D3556975EED"
#define N "A708C278C8067461B50CD7F104FE4A612A8C53049AE543802914515155A753CFF168C03A64B793FE325C3FC7BEBFB4BFB405AF92EA6993689CA93C71378B9FFD2EFEC803BB61C3B997F7D7D170BEE8D8DF5F943E2D77D936E8946BFC10C59E577F16E90BE67C3185637C9B8AFFD58F7B11F21A6659160C38D6B0E7F7245680860DAF92C12B64E13FFAF10B6A7FC717E905CD19D728FE1F4BA94FFFA6AD9CDDDB58B62631B4FD3E816BF99311132DFB3D0F99596AD1E115EDED8A315E051B12C5BF1A1BA40A2E090CAA4D6B45CF22BD820FF52CA22231A36F63EA5E57623CF8E61A66C611C57BF08902894E75ACEE14C16CC44BBFA86A398708A523DC92F46781"

uint64_t measure_mbedtls_loop(uint64_t T)
{
	mbedtls_mpi x_mpi, N_mpi;
    	mbedtls_mpi_init(&x_mpi);
    	mbedtls_mpi_init(&N_mpi);

    	mbedtls_mpi_read_string(&x_mpi, 16, x);
    	mbedtls_mpi_read_string(&N_mpi, 16, N);

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

	BN_hex2bn(&x_BN, x);
	BN_hex2bn(&N_BN, N);

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

	BN_hex2bn(&x_BN, x);
	BN_hex2bn(&N_BN, N);

	BN_exp(T_BN, two_BN, T_BN, ctx);

	auto t0 = std::chrono::steady_clock::now();

	BN_mod_exp(x_BN, x_BN, T_BN, N_BN, ctx);

	auto t1 = std::chrono::steady_clock::now();

	BN_free(x_BN);
	BN_free(N_BN);
	BN_CTX_free(ctx);

	return (std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
}

int main()
{
	bool write_header = !std::ifstream("data/impl_solve.csv").good();
        std::ofstream file("data/impl_solve.csv", std::ios::app);
        if (write_header)
            file << "Impl,T,Time\n";

	for (int i = 5; i < 25; i++) {
		std::cout << "Iteration " << i << std::endl;
		uint64_t T = (uint64_t) std::pow(2, i);
		uint64_t mbedtls = measure_mbedtls_loop(T);
		uint64_t openssl_loop = measure_openssl_loop(T);
		uint64_t openssl_exp = measure_openssl_exp(T);
		file << "mbedtls," << T << "," << mbedtls << std::endl;
		file << "openssl_loop," << T << "," << openssl_loop << std::endl;
		file << "openssl_exp," << T << "," << openssl_exp << std::endl;
	}
	
}
