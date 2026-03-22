/* Benchmark 1: Fibonacci + Prime Counting — C equivalent */
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

static int64_t fib(int64_t n) {
    int64_t a = 0, b = 1;
    for (int64_t i = 0; i < n; i++) {
        int64_t next = a + b;
        a = b;
        b = next;
    }
    return a;
}

static bool is_prime(int64_t n) {
    if (n < 2) return false;
    if (n == 2) return true;
    if (n % 2 == 0) return false;
    for (int64_t i = 3; i * i <= n; i += 2) {
        if (n % i == 0) return false;
    }
    return true;
}

static int64_t count_primes(int64_t limit) {
    int64_t count = 0;
    for (int64_t i = 2; i <= limit; i++) {
        if (is_prime(i)) count++;
    }
    return count;
}

int main(void) {
    int64_t fib_val = fib(78);
    int64_t prime_count = count_primes(100000);

    if (fib_val != 8944394323791464LL) return 1;
    if (prime_count != 9592) return 2;

    printf("PASS\n");
    return 0;
}
