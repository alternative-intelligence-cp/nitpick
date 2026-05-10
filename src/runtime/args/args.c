/**
 * Aria Runtime - Command-Line Argument Access
 * 
 * Stores argc/argv from main() and provides accessor functions
 * that Aria programs can call via extern declarations.
 *
 * The "npk_arg" function returns an AriaString* (heap-allocated)
 * matching how the compiler represents extern string returns.
 */

#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* Aria string representation: { ptr, i64 } */
typedef struct {
    char* data;
    int64_t length;
} AriaString;

static int32_t g_argc = 0;
static char** g_argv = NULL;

void npk_args_init(int32_t argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

int32_t npk_get_argc(void) {
    return g_argc;
}

/* Returns argv[index] as an AriaString*, or empty string if OOB */
/* DEPRECATED: ABI-broken (returns pointer, not value). Use get_argv() builtin. */
AriaString* npk_arg(int32_t index) {
    AriaString* result = (AriaString*)malloc(sizeof(AriaString));
    if (!result) {
        return NULL;
    }
    const char* src;
    if (index < 0 || index >= g_argc || !g_argv || !g_argv[index]) {
        src = "";
    } else {
        src = g_argv[index];
    }
    int64_t len = (int64_t)strlen(src);
    result->data = (char*)malloc((size_t)len + 1);
    if (result->data) {
        memcpy(result->data, src, (size_t)len + 1);
    }
    result->length = len;
    return result;
}

/* Returns the number of user-supplied arguments (argc - 1, excludes program name).
 * Used by the get_argc() builtin. */
int32_t npk_get_argc_builtin(void) {
    int32_t n = g_argc - 1;
    return n > 0 ? n : 0;
}

/* Returns argv[index + 1] as a null-terminated C string (excludes program name).
 * index 0 = first user argument. Returns "" if out of bounds.
 * Used by the get_argv() builtin via getOrDeclareStringFromCstr(). */
const char* npk_get_argv_cstr(int32_t index) {
    int32_t real = index + 1;
    if (real < 1 || real >= g_argc || !g_argv || !g_argv[real]) {
        return "";
    }
    return g_argv[real];
}
