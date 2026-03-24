/**
 * Aria Runtime - Command-Line Argument Access
 * 
 * Stores argc/argv from main() and provides accessor functions
 * that Aria programs can call via extern declarations.
 *
 * The "aria_arg" function returns an AriaString* (heap-allocated)
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

void aria_args_init(int32_t argc, char** argv) {
    g_argc = argc;
    g_argv = argv;
}

int32_t aria_get_argc(void) {
    return g_argc;
}

/* Returns argv[index] as an AriaString*, or empty string if OOB */
AriaString* aria_arg(int32_t index) {
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
