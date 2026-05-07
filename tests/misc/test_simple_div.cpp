#include <stdio.h>
#include <stdint.h>

// Minimal test to debug division

typedef struct {
    uint64_t limbs[4];
} npk_fix256_t;

// Forward declarations - CPU version only
extern "C" {
    npk_fix256_t npk_fix256_div(npk_fix256_t a, npk_fix256_t b);
}

npk_fix256_t make_int(int64_t val) {
    npk_fix256_t result;
    result.limbs[0] = 0;
    result.limbs[1] = 0;
    result.limbs[2] = (uint64_t)val;
    result.limbs[3] = (val < 0) ? ~0ULL : 0;
    return result;
}

void print_fix256(const char* label, npk_fix256_t val) {
    printf("%s: [%016lx, %016lx, %016lx, %016lx]\n", 
           label, val.limbs[0], val.limbs[1], val.limbs[2], val.limbs[3]);
}

int main() {
    printf("Testing CPU division implementation\n\n");
    
    npk_fix256_t a = make_int(10);
    npk_fix256_t b = make_int(2);
    
    print_fix256("Input A (10)", a);
    print_fix256("Input B (2)", b);
    
    npk_fix256_t result = npk_fix256_div(a, b);
    
    print_fix256("Result", result);
    printf("Result as int: %ld\n", (int64_t)result.limbs[2]);
    
    return 0;
}
