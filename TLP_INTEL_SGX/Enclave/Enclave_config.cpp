#include "Enclave.h"
#include "Enclave_t.h"

/*
 * struct config {
 *      uint64_t num_bits:              
 *      uint64_t private_set_digits;    
 *      uint64_t private_set_size;      
 *      uint64_t min_expected_cycles;   
 *      int T_exp;                      
 *      int T_baseline_comp;                               
 *      int T_input_size_comp;                            
 *      int T_private_set_comp;         
 * };
 */

struct config conf = {
    1024,   // number of bits of modulus (/2)
    4,     // how large is the domain of private set (eg. how many digits)
    1000,    // how many items should be in private set
    3000000,      // expected number of cycles per request
    15,     // for default T = 2^T_exp
    0,      // 0 for T = 2^T_exp, 
            // 1 for T s.t. min expected cycles sat.
    1,      // 0 for constant T
            // 1 for linear scaling with input size
    0       // 0 for constant T
            // 1 to sat. E_cycles[hit]
};

uint64_t T_base;

uint64_t config_get_T_exponent(void)
{
    switch(conf.T_baseline_comp) {
    case 0: {
        /* 
         * This simply sets T to the independently chosen constant value.
         */
        return conf.T_exp;
    }
    case 1: {
        /* 
         * This implements the idea that a request should take a minimum of
         * conf.min_expected_cycles many cycles per request.
         * At the same time, this can be seen as the full enumeration taking
         * at least conf.min_expected_cycles times the magnitude of the domain
         * many cycles.
         */
        return log2(conf.min_expected_cycles / CYCLES_PER_SQUARING) + 1;
    }
    default: {
        return 0;
    }
    }
}

/*
 * If debug_build is enabled, this allows for overwriting the default conf.
 * This is used to evaluate parameters.
 * 
 * Returns:
 *      * 0         if successful
 *      * -1        if parameters not allowed
 *      * -2        if not permitted (eg. not a debug build)
 */
int ecall_set_config(struct config *c)
{
#ifdef debug_build

    if (c->num_bits <= 256 || c->num_bits > 4096)
        return -1;
    if (c->private_set_digits < 1 || c->private_set_digits > 20)
        return -1;
    if (c->private_set_size < 1 || c->private_set_size > 1000000000
                || c->private_set_size > pow(10, c->private_set_digits))
        return -1;
    if (c->T_exp < 1 || c->T_exp > 30)
        return -1;
    if (c->T_baseline_comp < 0 || c->T_baseline_comp > 2)
        return -1;
    if (c->T_input_size_comp < 0 || c->T_input_size_comp > 2)
        return -1;
    if (c->T_private_set_comp < 0 || c->T_private_set_comp > 2)
        return -1;
    
    conf = *c;

    T_base = (uint64_t)1 << config_get_T_exponent();
    initialize_private_set();
    return 0;
#else
    return -2;
#endif
}

uint64_t compute_T_set()
{
    switch(conf.T_private_set_comp) {
    case 0: {
        /*
         * This does not modify T w.r.t the current state of the private set.
         */
        return T_base;
    }
    case 1: {
        /*
         * This corresponds to the idea that the expected number of cycles to
         * "find" an element in the private set should take 
         * conf.min_expected_cycles many cycles
         * (assuming random probing and uniform distribution of private set).
         * For that reason, T_base is scaled according to the probability to
         * randomly "find" an element.
         * 
         * This assumes that option 1 is set for baseline computation.
         */
        return (uint64_t) ((T_base * private_set.size()) / pow(10, conf.private_set_digits));
    }
    default: {
        return T_base;
    }
    }
}

uint64_t config_compute_T(int n)
{
    switch(conf.T_input_size_comp) {
    case 0: {
        /*
         * This does not scale T with number of elements per request.
         */
        return compute_T_set();
    }
    case 1: {
        /*
         * This linearly scales the necessary squarings with the number of 
         * requested elements.
         */
        return n * compute_T_set();
    }
    }
}

int adj_exponent_set(mbedtls_mpi *final_exp, uint64_t T_final)
{
    switch(conf.T_private_set_comp) {
    case 0: {
        /*
         * This does not modify T w.r.t the current state of the private set.
         */
        return mbedtls_mpi_copy(final_exp, &exponent);
    }
    case 1: {
        /*
         * Since scaling of the exponent mod phi only works for positive ints,
         * in this case, we have to completely recompute the exponent.
         */
        if (mbedtls_mpi_lset(final_exp, 1) != 0) {
            return -1;
        }

        for (uint64_t i = 0; i < T_final; i++) { 
            if (mbedtls_mpi_mul_int(final_exp, final_exp, 2) != 0) {
                return -1;
            }
            if (mbedtls_mpi_mod_mpi(final_exp, final_exp, &phi) != 0) {
                return -1;
            }
        }
        break;
    }
    default: {
        return 0;
    }
    }
}

int config_adj_exponent(mbedtls_mpi *final_exp, int n)
{
    uint64_t T_final = config_compute_T(n);
    if (adj_exponent_set(final_exp, T_final) != 0)
        return -1;

    switch(conf.T_input_size_comp) {
    case 0: {
        /*
         * This does not scale T with number of elements per request.
         */
        return 0;
    }
    case 1: {
        /*
         * This applies the linear scaling to T_base to the final_exp, which 
         * is equivalent to exponent^n
         */
        mbedtls_mpi temp;
        mbedtls_mpi_init(&temp);
        mbedtls_mpi_copy(&temp, final_exp);
        for (int i = 0; i < n-1; i++) {
            if (mbedtls_mpi_mul_mpi(final_exp, final_exp, &temp) != 0) {
                mbedtls_mpi_free(&temp);
                return -1;
            }
            if (mbedtls_mpi_mod_mpi(final_exp, final_exp, &phi) != 0) {
                mbedtls_mpi_free(&temp);
                return -1;
            }
        }
        mbedtls_mpi_free(&temp);
        return 0;
    }
    }
}

uint64_t config_get_num_bits()
{
    return conf.num_bits;
}

void initialize_private_set(void)
{
    private_set.clear();
    uint64_t stepsize = (uint64_t) pow(10, conf.private_set_digits) / conf.private_set_size;

    for (int i = 0; i < conf.private_set_size; i++) {
        private_set.insert(i * stepsize);
    }
}