/* Benchmark 2: Collatz Conjecture — C equivalent */
#include <stdio.h>
#include <stdint.h>

static int64_t collatz_steps(int64_t n) {
    int64_t steps = 0;
    int64_t val = n;
    while (val != 1) {
        if (val % 2 == 0) {
            val = val / 2;
        } else {
            val = val * 3 + 1;
        }
        steps++;
    }
    return steps;
}

int main(void) {
    int64_t total_steps = 0;
    int64_t max_steps = 0;
    for (int64_t i = 1; i <= 1000000; i++) {
        int64_t steps = collatz_steps(i);
        total_steps += steps;
        if (steps > max_steps) max_steps = steps;
    }

    if (total_steps != 131434424LL) return 1;
    if (max_steps != 524) return 2;

    printf("PASS\n");
    return 0;
}
