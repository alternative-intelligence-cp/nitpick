/* aria_rt_stubs.c — Minimal Aria runtime stubs for KLEE symbolic execution
 *
 * When KLEE runs an Aria program, the generated main() wrapper calls several
 * runtime init functions (GC, streams, args). These stubs replace them so
 * KLEE can symbolically execute the program logic without a full runtime.
 *
 * All stubs are no-ops or return success (0 / NULL). The only symbol that
 * matters for program correctness is klee_make_symbolic, which KLEE provides
 * from its own runtime library (linked automatically).
 */

#include <stdlib.h>
#include <string.h>

/* GC: no-op (KLEE manages memory symbolically) */
void npk_gc_init(void *a, void *b)      { (void)a; (void)b; }
void npk_gc_shutdown(void)              {}
void npk_gc_collect(void)               {}
void npk_gc_safepoint(void)             {}   /* called in loop bodies */
void *npk_gc_alloc(long size)           { return malloc((size_t)size); }
void npk_gc_free(void *p)               { free(p); }

/* Streams: no-op */
void npk_streams_init(void)             {}
void npk_streams_shutdown(void)         {}

/* Args: no-op */
void npk_args_init(int argc, char **argv) { (void)argc; (void)argv; }

/* Print: no-op (tests that print will silently discard output) */
typedef struct { const char *data; long length; } NpkString;
void npk_print_cstr(const char *s)      { (void)s; }
void npk_println_cstr(const char *s)    { (void)s; }
void npk_print_str(NpkString s)         { (void)s; }
void npk_println_str(NpkString s)       { (void)s; }
void npk_print_i64(long v)              { (void)v; }
void npk_println_i64(long v)            { (void)v; }
void npk_print_f64(double v)            { (void)v; }

/* Module registry: no-op */
void npk_module_register(const char *name) { (void)name; }
void npk_module_init_check(void)           {}

/* Error reporting: exit so KLEE sees the error path */
void npk_panic(const char *msg)         { (void)msg; exit(99); }
void npk_failsafe_dispatch(int code)    { exit(code); }
