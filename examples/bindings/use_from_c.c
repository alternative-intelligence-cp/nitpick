/* Call Nitpick shared library from C
 *
 * Build:
 *   npkc mathlib.npk --shared -o libmathlib.so
 *   gcc use_from_c.c -L. -lmathlib -Wl,-rpath,. -o use_from_c
 *   ./use_from_c
 */
#include <stdio.h>

extern int add(int a, int b);
extern int subtract(int a, int b);
extern int multiply(int x, int y);
extern int square(int n);
extern int is_even(int n);  // Aria bool maps to C int (0/1)

int main() {
    printf("=== Calling Aria from C ===\n");
    printf("add(3, 4) = %d\n", add(3, 4));
    printf("subtract(10, 3) = %d\n", subtract(10, 3));
    printf("multiply(5, 6) = %d\n", multiply(5, 6));
    printf("square(9) = %d\n", square(9));
    printf("is_even(4) = %d\n", is_even(4));
    printf("is_even(7) = %d\n", is_even(7));
    return 0;
}
