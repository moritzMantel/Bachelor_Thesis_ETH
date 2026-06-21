#include "Enclave.h"
#include "Enclave_t.h"

std::unordered_set<uint64_t> private_set;

/*
 * Interface with the internal private set, computes intersection.
 */
std::vector<uint64_t> compute_intersection(int n, uint64_t *elements)
{
    std::vector<uint64_t> intersection;
    
    for (int i = 0; i < n; i++) {
        if (private_set.find(elements[i]) != private_set.end())
            intersection.push_back(elements[i]);
    }
    return intersection;
}