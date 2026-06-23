#include <chrono>
#include <mbedtls/bignum.h>
#include <cmath>
#include <fstream>
#include <iostream>
#include <cstring>

#define X_HEX "A111F275CC2E7588000001D300A31E76336D15B9D314CD1A1D8F3D3556975EED"
#define N_HEX "A708C278C8067461B50CD7F104FE4A612A8C53049AE543802914515155A753CFF168C03A64B793FE325C3FC7BEBFB4BFB405AF92EA6993689CA93C71378B9FFD2EFEC803BB61C3B997F7D7D170BEE8D8DF5F943E2D77D936E8946BFC10C59E577F16E90BE67C3185637C9B8AFFD58F7B11F21A6659160C38D6B0E7F7245680860DAF92C12B64E13FFAF10B6A7FC717E905CD19D728FE1F4BA94FFFA6AD9CDDDB58B62631B4FD3E816BF99311132DFB3D0F99596AD1E115EDED8A315E051B12C5BF1A1BA40A2E090CAA4D6B45CF22BD820FF52CA22231A36F63EA5E57623CF8E61A66C611C57BF08902894E75ACEE14C16CC44BBFA86A398708A523DC92F46781"

#define MPI_BIN_SIZE 128 // 1024 bits = 128 bytes

#define ITERS 1000

int const_time_memcmp(unsigned char *a, unsigned char *b, size_t len)
{
    unsigned char diff = 0;
    for (size_t i = 0; i < len; i++) {
        diff |= a[i] ^ b[i];
    }
    return diff;
}

uint64_t measure_comparison_const(unsigned char *x_buf, unsigned char *y_buf)
{
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < ITERS; i++) {
        const_time_memcmp(x_buf, y_buf, MPI_BIN_SIZE);
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
}

uint64_t measure_comparison_mpi(mbedtls_mpi *x_mpi, mbedtls_mpi *y_mpi)
{
    auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < ITERS; i++) {
        mbedtls_mpi_cmp_mpi(x_mpi, y_mpi);
    }
    auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
}

int main()
{
    /*
     * Comparison side-channel measurement
     */
    {
        bool write_header = !std::ifstream("evaluation/data/sidechannel_comp.csv").good();
        std::ofstream file("evaluation/data/sidechannel_comp.csv", std::ios::app);
        if (write_header)
            file << "Impl,MatchingBytes,Time\n";

        mbedtls_mpi x_mpi;
        mbedtls_mpi_init(&x_mpi);
        mbedtls_mpi_read_string(&x_mpi, 16, X_HEX);

        unsigned char x_buf[MPI_BIN_SIZE];
        mbedtls_mpi_write_binary(&x_mpi, x_buf, MPI_BIN_SIZE);

        for (int i = 0; i <= MPI_BIN_SIZE; i++) {
            unsigned char y_buf[MPI_BIN_SIZE];
            memcpy(y_buf, x_buf, MPI_BIN_SIZE);
            if (i < MPI_BIN_SIZE)
                y_buf[i] ^= 0xFF;

            mbedtls_mpi y_mpi;
            mbedtls_mpi_init(&y_mpi);
            mbedtls_mpi_read_binary(&y_mpi, y_buf, MPI_BIN_SIZE);

            file << "comp_const," << i << "," << measure_comparison_const(x_buf, y_buf) << "\n";
            file << "comp_mpi," << i << "," << measure_comparison_mpi(&x_mpi, &y_mpi) << "\n";

            mbedtls_mpi_free(&y_mpi);
        }

        mbedtls_mpi_free(&x_mpi);
    }
    return 0;
}