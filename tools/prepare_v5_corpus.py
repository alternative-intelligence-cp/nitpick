#!/usr/bin/env python3
"""
Aria V5 Training Corpus Builder

Extends V4.2 corpus with:
  1. ECOSYSTEM: 104 new .aria files from aria_packages (9 libraries + GUI wrappers)
  2. FFI DRILLS: extern func patterns, C shim interop, linker usage
  3. BUG WORKAROUNDS: Known compiler bugs and correct workaround patterns
  4. LIBRARY USAGE: Practical examples using ecosystem libraries

V4.2 → V5 delta:
  - 9 new utility libraries (http, csv, datetime, regex, log, fs, socket, base64, test)
  - 3 GUI wrappers (GTK4, SDL2, raylib)
  - FFI/extern interop patterns central to library design
  - Bug workaround patterns for 4 confirmed compiler issues
"""

import json
import os
import re
import random
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
ECOSYSTEM_ROOT = REPO_ROOT / "aria_ecosystem"


def detect_features(code):
    """Detect which Aria features a file uses."""
    features = []
    if "struct:" in code: features.append("struct")
    if "enum:" in code: features.append("enum")
    if "till(" in code: features.append("till_loop")
    if "loop(" in code: features.append("loop")
    if re.search(r'\$\b', code): features.append("dollar_var")
    if "while(" in code: features.append("while_loop")
    if "pick(" in code: features.append("pick")
    if "Result<" in code or "result:" in code: features.append("result_type")
    if "fail(" in code: features.append("fail")
    if "pass(" in code: features.append("pass")
    if "->" in code and "int" in code: features.append("pointer")
    if "<-" in code: features.append("dereference")
    if "@" in code: features.append("address_of")
    if "=>" in code and "int" in code: features.append("cast")
    if "wild " in code: features.append("wild")
    if "extern " in code: features.append("extern")
    if "stdout" in code: features.append("stdout")
    if "mod:" in code: features.append("module")
    if "int128" in code or "int256" in code or "int512" in code: features.append("lbim")
    if "flt64" in code or "flt32" in code: features.append("float")
    if "float64" in code: features.append("float")
    if "string:" in code: features.append("string")
    if "&{" in code: features.append("interpolation")
    return features


def make_example(instruction, output, category, source, weight=1.0, features=None):
    """Create a training example in Alpaca format."""
    return {
        "instruction": instruction,
        "input": "",
        "output": output,
        "metadata": {
            "source": source,
            "category": category,
            "type": category,
            "confidence": "high",
            "features": features or []
        },
        "weight": weight
    }


def load_v42_corpus():
    """Load existing V4.2 corpus as base."""
    v42_path = REPO_ROOT / "tools" / "aria_training_corpus_v4.2.jsonl"
    if not v42_path.exists():
        # Try clean version
        v42_path = REPO_ROOT / "tools" / "aria_training_corpus_v4.2_clean.jsonl"
    if not v42_path.exists():
        print("  WARNING: V4.2 corpus not found, starting from scratch")
        return []

    examples = []
    with open(v42_path) as f:
        for line in f:
            line = line.strip()
            if line:
                examples.append(json.loads(line))
    print(f"  Loaded {len(examples)} examples from V4.2 corpus")
    return examples


def read_aria_file(path):
    """Read an .aria file, return content."""
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def extract_description(code):
    """Extract description from leading comments."""
    lines = code.split("\n")
    desc_lines = []
    for line in lines:
        stripped = line.strip()
        if stripped.startswith("//"):
            text = stripped.lstrip("/").strip()
            if text and not text.startswith("==="):
                desc_lines.append(text)
        elif stripped == "" and not desc_lines:
            continue
        else:
            break
    return " ".join(desc_lines[:3]) if desc_lines else ""


def generate_ecosystem_examples():
    """Generate examples from all .aria files in aria_ecosystem/aria_packages/."""
    examples = []
    packages_dir = ECOSYSTEM_ROOT / "aria_packages"

    if not packages_dir.exists():
        print("  WARNING: aria_packages directory not found")
        return []

    aria_files = list(packages_dir.rglob("*.aria"))
    print(f"  Found {len(aria_files)} .aria files in aria_packages/")

    for filepath in aria_files:
        code = read_aria_file(str(filepath))
        if len(code.strip()) < 20:
            continue

        rel_path = str(filepath.relative_to(REPO_ROOT))
        desc = extract_description(code)
        features = detect_features(code)
        feature_str = ", ".join(features[:5]) if features else "basic syntax"

        # Determine if it's a source file or test file
        parts = filepath.parts
        pkg_name = None
        for i, p in enumerate(parts):
            if p == "aria_packages" and i + 1 < len(parts):
                pkg_name = parts[i + 1]
                break

        is_test = "tests" in parts or "test_" in filepath.name
        is_src = "src" in parts

        if is_test:
            # Test files: demonstrate library usage patterns
            prompt = f"Write an Aria test program for {pkg_name} demonstrating {feature_str}."
            examples.append(make_example(
                instruction=prompt,
                output=f"```aria\n{code}\n```",
                category="ecosystem_test",
                source=rel_path,
                weight=3.0,
                features=features
            ))

            # Variant: "How do I test..."
            if pkg_name:
                lib_pretty = pkg_name.replace("aria-", "").replace("-", " ")
                examples.append(make_example(
                    instruction=f"Show me how to test Aria's {lib_pretty} library with extern functions.",
                    output=f"```aria\n{code}\n```",
                    category="ecosystem_test",
                    source=rel_path,
                    weight=2.5,
                    features=features
                ))

        elif is_src:
            # Source files: API definitions and library interfaces
            prompt = f"Write the Aria source module for {pkg_name} providing {feature_str}."
            examples.append(make_example(
                instruction=prompt,
                output=f"```aria\n{code}\n```",
                category="ecosystem_source",
                source=rel_path,
                weight=3.0,
                features=features
            ))

            # Variant: "Design a library..."
            if pkg_name:
                lib_pretty = pkg_name.replace("aria-", "").replace("-", " ")
                examples.append(make_example(
                    instruction=f"Design an Aria library for {lib_pretty} operations using extern functions and a C shim.",
                    output=f"```aria\n{code}\n```",
                    category="ecosystem_source",
                    source=rel_path,
                    weight=2.5,
                    features=features
                ))
        else:
            # Other ecosystem files
            if desc:
                prompt = f"Write an Aria program that {desc.lower().rstrip('.')}."
            else:
                prompt = f"Write an Aria program demonstrating {feature_str}."
            examples.append(make_example(
                instruction=prompt,
                output=f"```aria\n{code}\n```",
                category="ecosystem_other",
                source=rel_path,
                weight=2.0,
                features=features
            ))

    return examples


def generate_ffi_drills():
    """Generate drills for extern/FFI patterns — central to ecosystem libraries."""
    drills = []

    templates = [
        # ── Basic extern declarations ────────────────────────────────────
        (
            "Write an Aria program that declares an extern function from a C library and calls it.",
            'extern func:my_lib_init = int32();\nextern func:my_lib_cleanup = void();\n\nfunc:main = int32(){\n    int32:ok = my_lib_init();\n    if(ok == 1i32){\n        my_lib_cleanup();\n    }\n    pass(0i32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n    pass(NIL);\n};\n'
        ),
        (
            "Show how to use extern func in Aria to call a C function that takes a string parameter.",
            'extern func:c_print_message = void(string:msg);\n\nfunc:main = int32(){\n    c_print_message("Hello from Aria!");\n    pass(0i32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n    pass(NIL);\n};\n'
        ),
        (
            "Write an Aria extern declaration for a C function that takes int32 and returns int32.",
            'extern func:c_square = int32(int32:x);\n\nfunc:main = int32(){\n    int32:result = c_square(7i32);\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n    pass(NIL);\n};\n'
        ),
        (
            "How do I declare multiple extern functions in Aria for a C shim library?",
            '// Declare extern functions matching C shim function signatures\nextern func:my_lib_init      = int32();\nextern func:my_lib_process   = int32(int32:input);\nextern func:my_lib_get_result = int32();\nextern func:my_lib_cleanup   = void();\n\nfunc:main = int32(){\n    int32:r = my_lib_init();\n    int32:p = my_lib_process(42i32);\n    int32:val = my_lib_get_result();\n    my_lib_cleanup();\n    pass(val);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n    pass(NIL);\n};\n'
        ),

        # ── Test pattern with extern assert functions ────────────────────
        (
            "Write an Aria test file using extern assert functions from a C shim.",
            '// Test file pattern: extern assert helpers from C shim\nextern func:my_lib_init         = int32();\nextern func:my_lib_do_thing     = int32(int32:x);\nextern func:my_lib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:my_lib_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = my_lib_init();\n    my_lib_assert_int_eq(r, 1i32, "test 01: init");\n\n    int32:v = my_lib_do_thing(5i32);\n    my_lib_assert_int_eq(v, 10i32, "test 02: do_thing(5)");\n\n    my_lib_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Show the pattern for writing test assertions in Aria using extern C helpers.",
            '// Aria test pattern: C shim provides assert_int_eq and test_summary\n// The C shim tracks pass/fail counts internally\n\nextern func:lib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:lib_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    // Each test calls assert_int_eq with actual, expected, description\n    lib_assert_int_eq(1i32, 1i32, "test 01: basic equality");\n    lib_assert_int_eq(2i32 + 3i32, 5i32, "test 02: addition");\n\n    // Summary prints pass/fail counts\n    lib_test_summary();\n    pass(0i32);\n};\n'
        ),

        # ── Extern with string parameters ────────────────────────────────
        (
            "Write Aria code that passes strings to extern C functions.",
            'extern func:csv_parse_row = int32(string:line, string:delimiter);\nextern func:csv_field_count = int32();\nextern func:csv_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:csv_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = csv_parse_row("hello,world,foo", ",");\n    csv_assert_int_eq(r, 1i32, "test 01: parse CSV row");\n\n    int32:n = csv_field_count();\n    csv_assert_int_eq(n, 3i32, "test 02: 3 fields");\n\n    csv_test_summary();\n    pass(0i32);\n};\n'
        ),

        # ── Library compilation pattern ──────────────────────────────────
        (
            "How do I compile and link an Aria program with a C shim library?",
            'To compile and link an Aria program against a C shim library:\n\n1. Build the C shim as a shared library:\n```bash\ncc -O2 -shared -fPIC -Wall -o libaria_mylib_shim.so shim/aria_mylib_shim.c\n```\n\n2. Compile and link the Aria program:\n```bash\nariac tests/test_mylib.aria -o /tmp/test_mylib -L. -laria_mylib_shim\n```\n\n3. Run with the library path:\n```bash\nLD_LIBRARY_PATH=. /tmp/test_mylib\n```\n\nThe C shim function names must match the extern declarations in the .aria file.\nFor example, `extern func:my_lib_init = int32();` maps to `int32_t my_lib_init(void)` in C.'
        ),

        # ── Limitation: max ~7 externs per file ──────────────────────────
        (
            "What is the maximum number of extern functions in a single Aria file?",
            'Due to current compiler complexity limits, Aria programs should use no more than approximately 7 extern function declarations per file. Exceeding this limit may cause SIGABRT during compilation.\n\nWorkaround: split tests across multiple files, each with its own subset of extern declarations.\n\n```aria\n// File 1: test_basic.aria (4 externs)\nextern func:lib_init = int32();\nextern func:lib_process = int32(int32:x);\nextern func:lib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:lib_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = lib_init();\n    lib_assert_int_eq(r, 1i32, "test 01: init");\n    lib_test_summary();\n    pass(0i32);\n};\n```'
        ),

        # ── Failsafe pattern ─────────────────────────────────────────────
        (
            "What is the failsafe function in Aria and why is every program required to have one?",
            'The `failsafe` function is Aria\'s global error handler. Every Aria program must define it. It receives an int32 error code and returns NIL (void).\n\n```aria\n// Minimal failsafe — required in every Aria program\nfunc:failsafe = NIL(int32:err_code) {\n    pass(NIL);\n};\n```\n\nWhen a function call fails (returns an error via the Result type), the failsafe function is invoked. It serves as a last-resort error handler, similar to a top-level exception handler.\n\nCommon patterns:\n```aria\n// Failsafe that prints the error code\nfunc:failsafe = NIL(int32:err_code) {\n    print(`Fatal error: &{err_code}`);\n    pass(NIL);\n};\n```'
        ),

        # ── How to wrap a C library for Aria ─────────────────────────────
        (
            "How do I create a C shim to wrap a C library for use in Aria?",
            'To create a C shim for Aria:\n\n1. Write C functions with simple signatures (int32_t, const char*, void):\n```c\n#include <stdint.h>\n#include <string.h>\n#include <stdio.h>\n\n// Internal state\nstatic int g_initialized = 0;\n\n// Init function — returns 1 on success, 0 on failure\nint32_t aria_mylib_init(void) {\n    g_initialized = 1;\n    return 1;\n}\n\n// Process function — takes and returns simple types\nint32_t aria_mylib_process(int32_t input) {\n    if (!g_initialized) return -1;\n    return input * 2;\n}\n\n// Test helper — assertion with pass/fail tracking\nstatic int g_passed = 0, g_failed = 0;\nvoid aria_mylib_assert_int_eq(int32_t actual, int32_t expected, const char *msg) {\n    if (actual == expected) { g_passed++; printf("[PASS] %s\\n", msg); }\n    else { g_failed++; printf("[FAIL] %s (got %d, expected %d)\\n", msg, actual, expected); }\n}\n\nvoid aria_mylib_test_summary(void) {\n    printf("passed=%d failed=%d total=%d\\n", g_passed, g_failed, g_passed + g_failed);\n}\n```\n\n2. Match extern declarations in Aria:\n```aria\nextern func:aria_mylib_init       = int32();\nextern func:aria_mylib_process    = int32(int32:input);\nextern func:aria_mylib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_mylib_test_summary  = void();\n```\n\n3. Build: `cc -O2 -shared -fPIC -o libaria_mylib_shim.so shim.c`\n4. Link: `ariac test.aria -o test -L. -laria_mylib_shim`'
        ),
    ]

    for prompt, code in templates:
        drills.append(make_example(
            instruction=prompt,
            output=f"```aria\n{code}\n```" if "```" not in code else code,
            category="ffi_drill",
            source="synthetic_v5",
            weight=3.5,
            features=detect_features(code)
        ))

    return drills


def generate_bug_workaround_drills():
    """Generate examples teaching known compiler bug workarounds."""
    drills = []

    templates = [
        # ── Bug 1: String-returning externs with params segfault ──────────
        (
            "What are the known limitations of extern functions that return strings in Aria?",
            'Extern functions that return strings AND take parameters may segfault at runtime. This is a known compiler bug.\n\n**Workaround:** Use int32 return types for status, and have the C shim store results internally. Access results via a separate parameterless getter.\n\n```aria\n// INCORRECT — may segfault:\n// extern func:get_name = string(int32:id);\n\n// CORRECT — use status + getter pattern:\nextern func:lookup_name   = int32(int32:id);  // returns 1 on success\nextern func:get_last_name = string();          // no params → safe\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:ok = lookup_name(42i32);\n    // get_last_name() is safe because it has no parameters\n    pass(0i32);\n};\n```'
        ),

        # ── Bug 2: String variable corruption from extern returns ────────
        (
            "Why does storing a string from an extern function in a variable cause corruption in Aria?",
            'Storing a string returned by an extern function in a variable may produce corrupted data. This is a known compiler bug with string lifetime management in the FFI layer.\n\n**Workaround:** Pass the extern string result directly to another function call instead of storing it.\n\n```aria\nextern func:get_message = string();\nextern func:check_message = void(string:msg);\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    // INCORRECT — variable may be corrupted:\n    // string:msg = get_message();\n    // check_message(msg);  // msg might be garbage\n\n    // CORRECT — pass result directly without storing:\n    // Or use int32 status returns and C-side comparison\n    pass(0i32);\n};\n```'
        ),

        # ── Bug 3: Complexity/SIGABRT with many externs ──────────────────
        (
            "Why does my Aria program crash during compilation with SIGABRT when I have many extern functions?",
            'The Aria compiler may SIGABRT when a single file has too many extern declarations (typically more than ~7-9). This is a known complexity limit in the current compiler.\n\n**Workaround:** Split test files so each has at most 7 extern declarations.\n\n```aria\n// test_part1.aria — first 4 externs\nextern func:lib_init = int32();\nextern func:lib_op_a = int32(int32:x);\nextern func:lib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:lib_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = lib_init();\n    lib_assert_int_eq(r, 1i32, "test 01: init");\n    lib_test_summary();\n    pass(0i32);\n};\n```\n\n```aria\n// test_part2.aria — separate file with different externs\nextern func:lib_op_b = int32(string:data);\nextern func:lib_op_c = int32();\nextern func:lib_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:lib_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = lib_op_b("test data");\n    lib_assert_int_eq(r, 1i32, "test 01: op_b");\n    lib_test_summary();\n    pass(0i32);\n};\n```'
        ),

        # ── Bug 4: "free" in extern names ────────────────────────────────
        (
            "Why does using the word 'free' in an extern function name fail in Aria?",
            'The Aria compiler treats function names containing "free" as local stub functions rather than importing them as extern symbols. This causes linker errors.\n\n**Workaround:** Avoid the word "free" in extern function names. Use alternatives like "release", "cleanup", "destroy", or "dispose".\n\n```aria\n// INCORRECT — will generate a local stub instead of extern import:\n// extern func:my_lib_free_buffer = void();\n\n// CORRECT — use alternative naming:\nextern func:my_lib_release_buffer = void();\nextern func:my_lib_cleanup = void();\nextern func:my_lib_destroy = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    my_lib_cleanup();\n    pass(0i32);\n};\n```'
        ),
    ]

    for prompt, code in templates:
        drills.append(make_example(
            instruction=prompt,
            output=code if "```" in code else f"```aria\n{code}\n```",
            category="bug_workaround",
            source="synthetic_v5",
            weight=4.0,
            features=detect_features(code)
        ))

    return drills


def generate_library_usage_drills():
    """Generate practical examples showing how to use ecosystem libraries."""
    drills = []

    templates = [
        (
            "Write an Aria program that uses the aria-csv library to parse a CSV row.",
            'extern func:aria_csv_init       = int32();\nextern func:aria_csv_parse_row  = int32(string:line, string:delim);\nextern func:aria_csv_field_count = int32();\nextern func:aria_csv_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_csv_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_csv_init();\n    aria_csv_assert_int_eq(r, 1i32, "test 01: csv init");\n\n    int32:p = aria_csv_parse_row("name,age,email", ",");\n    aria_csv_assert_int_eq(p, 1i32, "test 02: parse row");\n\n    int32:n = aria_csv_field_count();\n    aria_csv_assert_int_eq(n, 3i32, "test 03: 3 fields");\n\n    aria_csv_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-base64 to encode a string.",
            'extern func:aria_base64_init       = int32();\nextern func:aria_base64_encode     = int32(string:input);\nextern func:aria_base64_result_len = int32();\nextern func:aria_base64_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_base64_test_summary = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_base64_init();\n    aria_base64_assert_int_eq(r, 1i32, "test 01: init");\n\n    int32:e = aria_base64_encode("Hello, World!");\n    aria_base64_assert_int_eq(e, 1i32, "test 02: encode");\n\n    aria_base64_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-log to log messages at different levels.",
            'extern func:aria_log_init    = int32();\nextern func:aria_log_info    = int32(string:msg);\nextern func:aria_log_warn    = int32(string:msg);\nextern func:aria_log_error   = int32(string:msg);\nextern func:aria_log_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_log_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_log_init();\n    aria_log_assert_int_eq(r, 1i32, "test 01: log init");\n\n    int32:i = aria_log_info("Application started");\n    aria_log_assert_int_eq(i, 1i32, "test 02: info log");\n\n    int32:w = aria_log_warn("Low memory");\n    aria_log_assert_int_eq(w, 1i32, "test 03: warn log");\n\n    aria_log_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-regex to match a pattern.",
            'extern func:aria_regex_init    = int32();\nextern func:aria_regex_compile = int32(string:pattern);\nextern func:aria_regex_match   = int32(string:text);\nextern func:aria_regex_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_regex_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_regex_init();\n    aria_regex_assert_int_eq(r, 1i32, "test 01: regex init");\n\n    int32:c = aria_regex_compile("[0-9]+");\n    aria_regex_assert_int_eq(c, 1i32, "test 02: compile pattern");\n\n    int32:m = aria_regex_match("abc123def");\n    aria_regex_assert_int_eq(m, 1i32, "test 03: match digits");\n\n    aria_regex_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-datetime to get the current timestamp.",
            'extern func:aria_datetime_init  = int32();\nextern func:aria_datetime_now   = int32();\nextern func:aria_datetime_year  = int32();\nextern func:aria_datetime_month = int32();\nextern func:aria_datetime_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_datetime_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_datetime_init();\n    aria_datetime_assert_int_eq(r, 1i32, "test 01: datetime init");\n\n    int32:n = aria_datetime_now();\n    aria_datetime_assert_int_eq(n, 1i32, "test 02: get now");\n\n    aria_datetime_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-fs to check if a file exists.",
            'extern func:aria_fs_init       = int32();\nextern func:aria_fs_exists     = int32(string:path);\nextern func:aria_fs_is_file    = int32(string:path);\nextern func:aria_fs_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_fs_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_fs_init();\n    aria_fs_assert_int_eq(r, 1i32, "test 01: fs init");\n\n    int32:e = aria_fs_exists("/tmp");\n    aria_fs_assert_int_eq(e, 1i32, "test 02: /tmp exists");\n\n    aria_fs_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-http to make a GET request.",
            'extern func:aria_http_init   = int32();\nextern func:aria_http_get    = int32(string:url);\nextern func:aria_http_status = int32();\nextern func:aria_http_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_http_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_http_init();\n    aria_http_assert_int_eq(r, 1i32, "test 01: http init");\n\n    int32:g = aria_http_get("http://127.0.0.1:8888/health");\n    aria_http_assert_int_eq(g, 1i32, "test 02: GET request");\n\n    int32:s = aria_http_status();\n    aria_http_assert_int_eq(s, 200i32, "test 03: status 200");\n\n    aria_http_test_summary();\n    pass(0i32);\n};\n'
        ),
        (
            "Write an Aria program that uses aria-socket for UDP communication.",
            'extern func:aria_socket_init   = int32();\nextern func:aria_socket_udp_open = int32(int32:port);\nextern func:aria_socket_close  = int32(int32:fd);\nextern func:aria_socket_assert_int_eq = void(int32:actual, int32:expected, string:msg);\nextern func:aria_socket_test_summary  = void();\n\nfunc:failsafe = NIL(int32:code) { pass(NIL); };\n\nfunc:main = int32() {\n    int32:r = aria_socket_init();\n    aria_socket_assert_int_eq(r, 1i32, "test 01: socket init");\n\n    int32:fd = aria_socket_udp_open(0i32);\n    aria_socket_assert_int_eq(fd > 0i32 => int32, 1i32, "test 02: udp open");\n\n    aria_socket_test_summary();\n    pass(0i32);\n};\n'
        ),
    ]

    for prompt, code in templates:
        drills.append(make_example(
            instruction=prompt,
            output=f"```aria\n{code}\n```",
            category="library_usage",
            source="synthetic_v5",
            weight=3.0,
            features=detect_features(code)
        ))

    return drills


def build_v5_corpus():
    """Build the complete V5 corpus."""
    print("=" * 60)
    print("  ARIA V5 CORPUS BUILDER")
    print("=" * 60)

    all_examples = []

    # Phase 1: Load V4.2 as base
    print("\n[1/5] Loading V4.2 corpus as base...")
    v42 = load_v42_corpus()
    all_examples.extend(v42)

    # Phase 2: Ecosystem .aria files
    print("\n[2/5] Generating ecosystem library examples...")
    eco_examples = generate_ecosystem_examples()
    all_examples.extend(eco_examples)
    print(f"  Generated {len(eco_examples)} ecosystem examples")

    # Phase 3: FFI/extern drills
    print("\n[3/5] Generating FFI/extern pattern drills...")
    ffi_drills = generate_ffi_drills()
    all_examples.extend(ffi_drills)
    print(f"  Generated {len(ffi_drills)} FFI drills")

    # Phase 4: Bug workaround drills
    print("\n[4/5] Generating bug workaround drills...")
    bug_drills = generate_bug_workaround_drills()
    all_examples.extend(bug_drills)
    print(f"  Generated {len(bug_drills)} bug workaround drills")

    # Phase 5: Library usage drills
    print("\n[5/5] Generating library usage drills...")
    lib_drills = generate_library_usage_drills()
    all_examples.extend(lib_drills)
    print(f"  Generated {len(lib_drills)} library usage drills")

    # Deduplicate by instruction text
    print("\nDeduplicating...")
    seen = set()
    deduped = []
    for ex in all_examples:
        key = ex["instruction"].strip().lower()[:200]
        if key not in seen:
            seen.add(key)
            deduped.append(ex)

    dups_removed = len(all_examples) - len(deduped)
    print(f"  Removed {dups_removed} duplicates")

    # Statistics
    print(f"\n{'=' * 60}")
    print(f"  V5 CORPUS STATISTICS")
    print(f"{'=' * 60}")
    print(f"  Total examples: {len(deduped)}")

    cats = {}
    feat_counts = {}
    total_weight = 0
    has_code = 0
    has_main = 0
    has_sb = 0
    has_failsafe = 0
    has_extern = 0

    for ex in deduped:
        cat = ex.get("metadata", {}).get("category", "unknown")
        cats[cat] = cats.get(cat, 0) + 1
        total_weight += ex.get("weight", 1.0)
        out = ex.get("output", "")
        if "```" in out or "func:" in out:
            has_code += 1
        if "func:main" in out:
            has_main += 1
        if "};" in out:
            has_sb += 1
        if "failsafe" in out:
            has_failsafe += 1
        if "extern " in out:
            has_extern += 1

        for feat in ex.get("metadata", {}).get("features", []):
            feat_counts[feat] = feat_counts.get(feat, 0) + 1

    print(f"  Total weight: {total_weight:.0f}")
    print(f"  Has code: {has_code} ({100*has_code//len(deduped)}%)")
    print(f"  Has func:main: {has_main} ({100*has_main//len(deduped)}%)")
    print(f"  Has }};: {has_sb} ({100*has_sb//len(deduped)}%)")
    print(f"  Has failsafe: {has_failsafe} ({100*has_failsafe//len(deduped)}%)")
    print(f"  Has extern: {has_extern} ({100*has_extern//len(deduped)}%)")
    print(f"\n  Categories:")
    for k, v in sorted(cats.items(), key=lambda x: -x[1]):
        print(f"    {k}: {v}")
    print(f"\n  Top features:")
    for k, v in sorted(feat_counts.items(), key=lambda x: -x[1])[:15]:
        print(f"    {k}: {v}")

    # Save
    output_path = REPO_ROOT / "tools" / "aria_training_corpus_v5.jsonl"
    with open(output_path, "w") as f:
        for ex in deduped:
            f.write(json.dumps(ex) + "\n")
    print(f"\n  Saved to: {output_path}")

    # Split train/val (90/10)
    random.seed(42)
    random.shuffle(deduped)
    split = int(len(deduped) * 0.9)
    train = deduped[:split]
    val = deduped[split:]

    train_path = REPO_ROOT / "tools" / "aria_training_corpus_v5_train.jsonl"
    val_path = REPO_ROOT / "tools" / "aria_training_corpus_v5_val.jsonl"
    with open(train_path, "w") as f:
        for ex in train:
            f.write(json.dumps(ex) + "\n")
    with open(val_path, "w") as f:
        for ex in val:
            f.write(json.dumps(ex) + "\n")
    print(f"  Train: {len(train)}  Val: {len(val)}")
    print(f"\n  Done!")
    return deduped


if __name__ == "__main__":
    build_v5_corpus()
