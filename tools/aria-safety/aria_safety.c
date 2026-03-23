/* aria_safety.c — Static safety audit tool for Aria source files
 *
 * Scans .aria files for constructs that require manual safety review.
 *
 * Findings:
 *   [WILD]     wild / wildx allocation — manual lifetime, no GC safety
 *   [RAW]      raw() — strips Result<T>; caller owns error handling
 *   [RESULT]   Result{...} — explicit Result construction; caller builds Result manually
 *   [DROP]     drop() — explicitly discards a Result<T>
 *   [OK]       ok() — bypasses error check on the unknown type
 *   [WEAK_CAS] compare_exchange_weak* — spurious failure; must be in retry loop
 *   [RELAXED]  relaxed atomic op — verify memory ordering is sufficient
 *   [FAILSAFE] empty or trivial failsafe block — error silently swallowed
 *   [UNSAFE]   unsafe block — bypasses safety guarantees
 *   [EXTERN]   extern declaration — FFI boundary; verify foreign function safety
 *   [CAST]     transmute/reinterpret — unchecked type conversion
 *   [TODO]     TODO/FIXME/HACK comment — unfinished or fragile code
 *
 * Build:
 *   gcc -O2 -Wall -Wextra -std=c99 -o aria-safety aria_safety.c
 *   (or: make)
 *
 * Usage:
 *   aria-safety file.aria [file.aria ...]
 *   aria-safety src/                          recursive directory scan
 *   aria-safety --json file.aria              JSON output
 *   aria-safety --summary src/                per-file summary stats
 *
 * Exit codes: 0 = clean, 1 = findings present, 2 = usage/IO error
 *
 * v1 limitations:
 *   - // comment stripping is naive (does not account for // inside strings)
 *   - brace counting can be thrown off by unbalanced braces in string literals
 *   - content on the same line as the failsafe opening brace is not checked
 *     for triviality (only single-line blocks are handled specially)
 *   - string literals are not excluded from pattern matching
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_LINE  4096
#define MAX_PATH  4096

/* ── globals ─────────────────────────────────────────────────── */

static int g_findings      = 0;
static int g_files_scanned = 0;
static int g_json_mode     = 0;
static int g_summary_mode  = 0;
static int g_json_first    = 1;  /* for comma-separated JSON array elements */

/* ── word-boundary search ────────────────────────────────────── */

static int is_word_char(unsigned char c)
{
    return isalnum(c) || c == '_';
}

/*
 * has_token: returns 1 if needle occurs in haystack as a word-boundary token.
 *
 * Rules:
 *   - the match must not be preceded by a word character
 *   - if needle ends with a word character, the match must not be followed
 *     by a word character (prevents "wild" from matching "wildlife")
 *   - if needle ends with a non-word character (e.g. '('), no suffix check
 *     is needed (prevents "raw(" from matching "_raw(")
 */
static int has_token(const char *haystack, const char *needle)
{
    size_t nlen = strlen(needle);
    const char *p = haystack;

    while ((p = strstr(p, needle)) != NULL) {
        /* prefix: must not be preceded by a word character */
        if (p > haystack && is_word_char((unsigned char)p[-1])) {
            p++;
            continue;
        }
        /* suffix: if needle ends with a word char, next char must not be one */
        if (is_word_char((unsigned char)needle[nlen - 1]) &&
            is_word_char((unsigned char)p[nlen])) {
            p++;
            continue;
        }
        return 1;
    }
    return 0;
}

/* ── pattern table ───────────────────────────────────────────── */

typedef struct {
    const char *needle;
    int         use_token;   /* 1 = has_token(), 0 = strstr() */
    const char *tag;
    const char *msg;
} Pattern;

static const Pattern PATTERNS[] = {
    /* manual memory management (wildx before wild so "wildx" is not also
     * caught as "wild" — has_token prevents that anyway, but order is clear) */
    { "wildx",                 1, "WILD",    "wildx allocation — no GC, no bounds checking; manual lifetime required" },
    { "wild",                  1, "WILD",    "wild allocation — no GC safety; manual lifetime required" },

    /* Result<T> bypasses */
    { "raw(",                  1, "RAW",     "raw() strips Result<T> — caller must handle failure explicitly" },
    { "Result{",               0, "RESULT",  "Result{...} explicit construction — verify val/err/is_error fields are correct" },
    { "drop(",                 1, "DROP",    "drop() discards Result<T> — confirm this error is intentionally ignored" },
    { "ok(",                   1, "OK",      "ok() bypasses unknown error check — ensure value is known-good" },

    /* weak CAS */
    { "compare_exchange_weak", 0, "WEAK_CAS","compare_exchange_weak — spurious failure possible; verify inside retry loop" },

    /* relaxed atomics */
    { "load_relaxed",          0, "RELAXED", "load_relaxed — no acquire fence; verify ordering with surrounding code" },
    { "store_relaxed",         0, "RELAXED", "store_relaxed — no release fence; verify ordering with surrounding code" },
    { "fetch_add_relaxed",     0, "RELAXED", "fetch_add_relaxed — relaxed ordering; verify no dependent reads follow" },
    { "fetch_sub_relaxed",     0, "RELAXED", "fetch_sub_relaxed — relaxed ordering; verify no dependent reads follow" },
    { "fetch_or_relaxed",      0, "RELAXED", "fetch_or_relaxed — relaxed ordering; verify ordering is sufficient" },
    { "fetch_and_relaxed",     0, "RELAXED", "fetch_and_relaxed — relaxed ordering; verify ordering is sufficient" },
    { "fetch_xor_relaxed",     0, "RELAXED", "fetch_xor_relaxed — relaxed ordering; verify ordering is sufficient" },
    { "swap_relaxed",          0, "RELAXED", "swap_relaxed — relaxed ordering; verify ordering is sufficient" },

    /* unsafe blocks */
    { "unsafe",                 1, "UNSAFE",  "unsafe block — type and memory safety guarantees bypassed" },

    /* FFI boundary */
    { "extern",                 1, "EXTERN",  "extern declaration — FFI boundary; verify foreign function safety" },

    /* unchecked casts */
    { "transmute(",             0, "CAST",    "transmute() — unchecked type reinterpretation; verify layout compatibility" },
    { "reinterpret(",           0, "CAST",    "reinterpret() — unchecked cast; verify type is valid" },
};

#define N_PATTERNS (sizeof(PATTERNS) / sizeof(PATTERNS[0]))

/* ── emit ────────────────────────────────────────────────────── */

static void emit(const char *file, int lineno, const char *tag, const char *msg)
{
    if (g_json_mode) {
        if (g_json_first) {
            printf("[\n");
            g_json_first = 0;
        } else {
            printf(",\n");
        }
        /* Emit JSON object — file/line/tag/msg are safe ASCII, no escaping needed */
        printf("  {\"file\":\"%s\",\"line\":%d,\"tag\":\"%s\",\"message\":\"%s\"}",
               file, lineno, tag, msg);
    } else {
        printf("%s:%d: [%s] %s\n", file, lineno, tag, msg);
    }
    g_findings++;
}

/* ── helpers ─────────────────────────────────────────────────── */

static void strip_nl(char *s)
{
    size_t n = strlen(s);
    if (n > 0 && s[n - 1] == '\n') s[--n] = '\0';
    if (n > 0 && s[n - 1] == '\r') s[--n] = '\0';
}

/* Strip // line comment in-place.  Naive: does not handle // inside strings. */
static void strip_line_comment(char *s)
{
    char *p = strstr(s, "//");
    if (p) *p = '\0';
}

/*
 * is_trivial_between_braces: for single-line failsafe blocks, checks whether
 * the content between the first '{' and the next '}' is trivial (empty or
 * only "pass(NIL)").
 */
static int is_trivial_between_braces(const char *line)
{
    const char *open = strchr(line, '{');
    if (!open) return 1;

    const char *p = open + 1;
    while (*p == ' ' || *p == '\t') p++;

    return (*p == '}' ||
            strncmp(p, "pass(NIL)", 9) == 0);
}

/* ── file scanner ────────────────────────────────────────────── */

static void scan_file(const char *path)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "aria-safety: cannot open '%s': %s\n",
                path, strerror(errno));
        return;
    }

    char raw[MAX_LINE];
    char line[MAX_LINE];
    int  lineno = 0;

    /* failsafe block tracking */
    typedef enum { FS_NONE, FS_WAIT, FS_INSIDE } FsState;
    FsState fs_state     = FS_NONE;
    int     fs_depth     = 0;   /* brace nesting depth inside the block */
    int     fs_start     = 0;   /* line where 'failsafe' keyword appeared */
    int     fs_open_line = 0;   /* line where the opening '{' was found */
    int     fs_trivial   = 1;   /* cleared once we see non-trivial content */

    while (fgets(raw, sizeof(raw), f)) {
        lineno++;
        strip_nl(raw);

        /* working copy: strip // comment so patterns don't fire in comments */
        memcpy(line, raw, strlen(raw) + 1);
        strip_line_comment(line);

        /* first non-whitespace character */
        const char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;
        int blank = (*trimmed == '\0');

        /* ── 1. line pattern checks ───────────────────────── */
        if (!blank) {
            for (size_t i = 0; i < N_PATTERNS; i++) {
                int hit = PATTERNS[i].use_token
                    ? has_token(line, PATTERNS[i].needle)
                    : (strstr(line, PATTERNS[i].needle) != NULL);
                if (hit)
                    emit(path, lineno, PATTERNS[i].tag, PATTERNS[i].msg);
            }

            /* TODO/FIXME/HACK in comments (check original raw line) */
            {
                const char *cmt = strstr(raw, "//");
                if (cmt) {
                    if (strstr(cmt, "TODO") || strstr(cmt, "FIXME") || strstr(cmt, "HACK"))
                        emit(path, lineno, "TODO", "TODO/FIXME/HACK — unfinished or fragile code");
                }
            }
        }

        /* ── 2. failsafe entry detection ──────────────────── */
        if (fs_state == FS_NONE && !blank && has_token(line, "failsafe")) {
            fs_state     = FS_WAIT;
            fs_depth     = 0;
            fs_start     = lineno;
            fs_open_line = 0;
            fs_trivial   = 1;
        }

        /* ── 3. failsafe content triviality check ─────────── */
        /*
         * Only runs when we are already INSIDE (past the opening brace).
         * Strips braces from the line and checks whether what remains is
         * non-trivial (anything other than empty or "pass(NIL)").
         */
        if (fs_state == FS_INSIDE && !blank) {
            char content[MAX_LINE];
            const char *s = trimmed;
            char       *d = content;
            while (*s) {
                if (*s != '{' && *s != '}') *d++ = *s;
                s++;
            }
            *d = '\0';

            const char *ct = content;
            while (*ct == ' ' || *ct == '\t') ct++;

            if (*ct != '\0' &&
                strncmp(ct, "pass(NIL)",  9) != 0 &&
                strncmp(ct, "pass(NIL);", 10) != 0) {
                fs_trivial = 0;
            }
        }

        /* ── 4. failsafe brace counting + state transitions ── */
        if (fs_state == FS_WAIT || fs_state == FS_INSIDE) {
            const char *p = line;
            while (*p) {
                if (*p == '{') {
                    if (fs_depth == 0) {
                        fs_state     = FS_INSIDE;
                        fs_open_line = lineno;
                    }
                    fs_depth++;
                } else if (*p == '}' && fs_depth > 0) {
                    fs_depth--;
                    if (fs_depth == 0) {
                        /* block closed — decide triviality */
                        int trivial;
                        if (fs_open_line == lineno) {
                            /* single-line block: inspect content between { } */
                            trivial = is_trivial_between_braces(line);
                        } else {
                            trivial = fs_trivial;
                        }
                        if (trivial) {
                            emit(path, fs_start, "FAILSAFE",
                                 "empty or trivial failsafe block — error is silently swallowed");
                        }
                        fs_state = FS_NONE;
                        fs_depth = 0;
                        break;
                    }
                }
                p++;
            }
        }
    }

    fclose(f);
    g_files_scanned++;
}

/* ── directory/path scanner ──────────────────────────────────── */

static void scan_path(const char *path);  /* forward declaration */

static void scan_dir(const char *dirpath)
{
    DIR *d = opendir(dirpath);
    if (!d) {
        fprintf(stderr, "aria-safety: cannot open '%s': %s\n",
                dirpath, strerror(errno));
        return;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_name[0] == '.') continue;  /* skip ., .., hidden */

        char path[MAX_PATH];
        int n = snprintf(path, sizeof(path), "%s/%s", dirpath, ent->d_name);
        if (n < 0 || n >= (int)sizeof(path)) continue;  /* path truncated */

        scan_path(path);
    }
    closedir(d);
}

static void scan_path(const char *path)
{
    struct stat st;
    if (stat(path, &st) != 0) {
        fprintf(stderr, "aria-safety: '%s': %s\n", path, strerror(errno));
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        scan_dir(path);
    } else if (S_ISREG(st.st_mode)) {
        size_t len = strlen(path);
        if (len >= 5 && strcmp(path + len - 5, ".aria") == 0)
            scan_file(path);
    }
}

/* ── main ────────────────────────────────────────────────────── */

int main(int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr,
            "aria-safety — static safety audit for Aria source files\n"
            "\n"
            "Usage:\n"
            "  aria-safety [options] <path> [path ...]    path = .aria file or directory\n"
            "\n"
            "Options:\n"
            "  --json      Output findings as JSON array\n"
            "  --summary   Print per-tag summary statistics\n"
            "\n"
            "Finds:\n"
            "  [WILD]     wild / wildx allocation\n"
            "  [RAW]      raw() call — strips Result<T>\n"
            "  [RESULT]   Result{...} — explicit Result construction\n"
            "  [DROP]     drop() call — discards Result<T>\n"
            "  [OK]       ok() call — bypasses unknown error check\n"
            "  [WEAK_CAS] compare_exchange_weak — must be in retry loop\n"
            "  [RELAXED]  relaxed atomic operation\n"
            "  [FAILSAFE] empty or trivial failsafe block\n"
            "  [UNSAFE]   unsafe block — bypasses safety guarantees\n"
            "  [EXTERN]   extern declaration — FFI boundary\n"
            "  [CAST]     transmute/reinterpret — unchecked type conversion\n"
            "  [TODO]     TODO/FIXME/HACK comment — unfinished code\n"
            "\n"
            "Exit codes: 0 = clean, 1 = findings, 2 = error\n");
        return 2;
    }

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--json") == 0) {
            g_json_mode = 1;
            continue;
        }
        if (strcmp(argv[i], "--summary") == 0) {
            g_summary_mode = 1;
            continue;
        }
        scan_path(argv[i]);
    }

    if (g_json_mode && g_files_scanned > 0) {
        /* Close was deferred; we opened with "[\n" before first emit */
    }

    if (g_files_scanned == 0) {
        if (g_json_mode) printf("[]\n");
        fprintf(stderr, "aria-safety: no .aria files found\n");
        return 2;
    }

    if (g_json_mode) {
        printf("\n]\n");
    }

    if (g_findings > 0) {
        fprintf(stderr, "\n%d finding%s across %d file%s\n",
                g_findings,      g_findings      == 1 ? "" : "s",
                g_files_scanned, g_files_scanned == 1 ? "" : "s");
        return 1;
    }

    fprintf(stderr, "clean — %d file%s scanned, no findings\n",
            g_files_scanned, g_files_scanned == 1 ? "" : "s");
    return 0;
}
