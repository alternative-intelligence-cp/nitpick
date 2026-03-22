/* Benchmark 3: GCD Chain — C equivalent */
#include <stdio.h>
#include <stdint.h>

static int64_t gcd(int64_t a, int64_t b) {
    while (b != 0) {
        int64_t t = b;
        b = a % b;
        a = t;
    }
    return a;
}

int main(void) {
    int64_t total = 0;
    for (int64_t i = 1; i <= 2000; i++) {
        for (int64_t j = i + 1; j <= 2000; j++) {
            total += gcd(i, j);
        }
    }

    if (total != 8734164LL) return 1;

    printf("PASS\n");
    return 0;
}
