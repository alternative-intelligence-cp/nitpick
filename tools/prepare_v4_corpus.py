#!/usr/bin/env python3
"""
Aria V4 Training Corpus Builder

V3 Analysis showed:
  - Only 2% compilable-looking examples (main + }; + failsafe)
  - 46% had stray closing braces teaching model to emit } instead of };
  - till() in only 4%, loop() in 0%
  - 33% pure explanation with no code

V4 Strategy:
  1. CORE: 427 verified-compilable .aria files → complete program examples (3x weight)
  2. AUGMENTED: Generate variations of compilable programs (different prompts)
  3. GRAMMAR DRILLS: Heavy };  terminator reinforcement
  4. LOOP DRILLS: till(), loop(), $ capture patterns
  5. CLEAN V3: Fix stray braces in existing V3 corpus
  6. SYNTHETIC: Task-oriented prompts matching validation test format
"""

import json
import os
import re
import subprocess
import random
from pathlib import Path

REPO_ROOT = Path(__file__).parent.parent
COMPILER = REPO_ROOT / "build" / "ariac"
COMPILABLE_LIST = Path("/tmp/aria_compiles.txt")


def load_compilable_files():
    """Load list of verified-compilable .aria files."""
    if not COMPILABLE_LIST.exists():
        print("Building compilable file list...")
        all_aria = []
        for d in ["tests", "examples"]:
            for f in (REPO_ROOT / d).rglob("*.aria"):
                all_aria.append(f)

        compilable = []
        for f in all_aria:
            try:
                r = subprocess.run(
                    [str(COMPILER), str(f), "-o", "/tmp/aria_v4_test"],
                    capture_output=True, timeout=10
                )
                if r.returncode == 0:
                    compilable.append(str(f))
            except (subprocess.TimeoutExpired, Exception):
                pass

        with open(COMPILABLE_LIST, "w") as out:
            out.write("\n".join(compilable) + "\n")
        print(f"  Found {len(compilable)} compilable files")
    else:
        with open(COMPILABLE_LIST) as f:
            compilable = [l.strip() for l in f if l.strip()]
        print(f"  Loaded {len(compilable)} compilable files from cache")

    return compilable


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
    if "int1024" in code or "int2048" in code or "int4096" in code: features.append("lbim_large")
    if "flt64" in code or "flt32" in code: features.append("float")
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
            "type": "compilable_program" if category == "compilable" else category,
            "confidence": "high",
            "features": features or []
        },
        "weight": weight
    }


def generate_compilable_examples(compilable_files):
    """Generate examples from verified-compilable .aria files."""
    examples = []

    for filepath in compilable_files:
        fpath = Path(filepath)
        if not fpath.is_absolute():
            fpath = REPO_ROOT / fpath
        code = read_aria_file(str(fpath))
        try:
            rel_path = str(fpath.relative_to(REPO_ROOT))
        except ValueError:
            rel_path = str(fpath)
        desc = extract_description(code)
        features = detect_features(code)
        feature_str = ", ".join(features[:5]) if features else "basic syntax"
        name = Path(filepath).stem.replace("_", " ").replace("-", " ")

        # Variant 1: "Write a complete program that..." (matches validation prompts)
        if desc:
            prompt = f"Write a complete Aria program that {desc.lower().rstrip('.')}."
        else:
            prompt = f"Write a complete Aria program demonstrating {feature_str}."

        examples.append(make_example(
            instruction=prompt,
            output=f"```aria\n{code}\n```",
            category="compilable",
            source=rel_path,
            weight=3.0,
            features=features
        ))

        # Variant 2: "Show me..." style
        examples.append(make_example(
            instruction=f"Show me a working Aria program called {name} that uses {feature_str}.",
            output=f"```aria\n{code}\n```",
            category="compilable",
            source=rel_path,
            weight=2.5,
            features=features
        ))

        # Variant 3: Feature-focused (for underrepresented features)
        for feat in features:
            if feat in ("till_loop", "loop", "dollar_var", "cast", "stdout",
                        "pointer", "dereference", "lbim", "lbim_large", "pick"):
                examples.append(make_example(
                    instruction=f"Write an Aria program that uses {feat.replace('_', ' ')}.",
                    output=f"```aria\n{code}\n```",
                    category="compilable_featured",
                    source=rel_path,
                    weight=3.5,
                    features=features
                ))
                break  # One extra per underrepresented feature

    return examples


def generate_grammar_drills():
    """Generate drill examples reinforcing }; terminators and core grammar."""
    drills = []

    # Pattern: simple function with };
    templates = [
        # Minimal programs
        (
            "Write the simplest possible Aria program that returns 0.",
            'func:main = int32(){\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write a minimal Aria hello world program.",
            'func:main = int32(){\n    print("Hello, World!");\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that returns 42 from main.",
            'func:main = int32(){\n    pass(42);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Variables + return
        (
            "Write an Aria program that declares an int32 variable x set to 10 and returns it.",
            'func:main = int32(){\n    int32:x = 10;\n    pass(x);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that adds two int32 variables and returns the sum.",
            'func:main = int32(){\n    int32:a = 5;\n    int32:b = 7;\n    int32:sum = a + b;\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Functions
        (
            "Write an Aria function called 'double' that takes an int32 and returns it multiplied by 2. Include a main that calls it.",
            'func:double = int32(int32:x){\n    pass(x * 2);\n};\n\nfunc:main = int32(){\n    Result<int32>:r = double(5);\n    int32:result = r ? 0i32;\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria function 'maximum' that returns the larger of two int32 values.",
            'func:maximum = int32(int32:a, int32:b){\n    if(a > b){\n        pass(a);\n    }\n    pass(b);\n};\n\nfunc:main = int32(){\n    Result<int32>:r = maximum(10, 20);\n    int32:result = r ? 0i32;\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program with a function that computes the square of an int32.",
            'func:square = int32(int32:n){\n    pass(n * n);\n};\n\nfunc:main = int32(){\n    Result<int32>:r = square(7);\n    int32:result = r ? 0i32;\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # If/else
        (
            "Write an Aria program with if/else that checks if a number is positive.",
            'func:main = int32(){\n    int32:x = 5;\n    if(x > 0){\n        pass(1);\n    }else{\n        pass(0);\n    }\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that returns 1 if x is even, 0 if odd.",
            'func:is_even = int32(int32:x){\n    if(x % 2 == 0){\n        pass(1);\n    }\n    pass(0);\n};\n\nfunc:main = int32(){\n    Result<int32>:r = is_even(4);\n    int32:val = r ? 0i32;\n    pass(val);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # While loop
        (
            "Write an Aria program that sums integers from 1 to 10 using a while loop.",
            'func:main = int32(){\n    int32:sum = 0;\n    int32:i = 1;\n    while(i <= 10){\n        sum = sum + i;\n        i = i + 1;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that computes factorial of 5 using a while loop.",
            'func:main = int32(){\n    int32:result = 1;\n    int32:n = 5;\n    while(n > 1){\n        result = result * n;\n        n = n - 1;\n    }\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that counts down from 10 to 1 using while loop.",
            'func:main = int32(){\n    int32:count = 10;\n    while(count > 0){\n        count = count - 1;\n    }\n    pass(count);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Till loop
        (
            "Write an Aria program that uses a till loop to sum 0 through 9.",
            'func:main = int32(){\n    int32:sum = 0;\n    till(10, 1){\n        int64:i = $;\n        sum = sum + i => int32;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria till loop that prints numbers 0 to 4.",
            'func:main = int32(){\n    till(5, 1){\n        int64:idx = $;\n        print(`&{idx}`);\n    }\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Show me how to use a till loop in Aria with the $ variable.",
            'func:main = int32(){\n    int32:total = 0;\n    till(5, 1){\n        int64:i = $;\n        total = total + i => int32;\n    }\n    pass(total);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Loop
        (
            "Write an Aria program that uses loop(start, limit, step) to iterate from 0 to 9.",
            'func:main = int32(){\n    int32:sum = 0;\n    loop(0, 10, 1){\n        int64:i = $;\n        sum = sum + i => int32;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program using a loop to process even numbers from 0 to 18.",
            'func:main = int32(){\n    int32:sum = 0;\n    loop(0, 20, 2){\n        int64:i = $;\n        sum = sum + i => int32;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # More till loop drills (boost coverage)
        (
            "Write an Aria program that uses till to count from 0 to 99.",
            'func:main = int32(){\n    int32:count = 0;\n    till(100, 1){\n        int64:n = $;\n        count = count + 1;\n    }\n    pass(count);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria till loop that computes the product of 1 through 5 (factorial).",
            'func:main = int32(){\n    int32:product = 1;\n    till(5, 1){\n        int64:i = $;\n        int32:val = i => int32 + 1;\n        product = product * val;\n    }\n    pass(product);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program with till(3, 1) and print the $ value each iteration.",
            'func:main = int32(){\n    till(3, 1){\n        int64:iter = $;\n        print(`Iteration &{iter}`);\n    }\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Use till in Aria to sum even numbers: capture $ and check if divisible by 2.",
            'func:main = int32(){\n    int32:sum = 0;\n    till(20, 1){\n        int64:i = $;\n        int32:val = i => int32;\n        if(val % 2 == 0){\n            sum = sum + val;\n        }\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria till loop that finds the first number > 7.",
            'func:main = int32(){\n    int32:found = -1;\n    till(20, 1){\n        int64:i = $;\n        int32:val = i => int32;\n        if(val > 7){\n            if(found == -1){\n                found = val;\n            }\n        }\n    }\n    pass(found);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write a till loop in Aria that steps by 2 using till(limit, step).",
            'func:main = int32(){\n    int32:sum = 0;\n    till(10, 2){\n        int64:i = $;\n        sum = sum + i => int32;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Explain the $ variable in Aria's till loop with a code example.",
            'In Aria, `$` is the automatic loop counter variable in `till()` and `loop()` constructs. It is always `int64` type.\n\n```aria\nfunc:main = int32(){\n    int32:total = 0;\n    till(5, 1){\n        int64:current = $;  // $ holds 0, 1, 2, 3, 4\n        total = total + current => int32;\n    }\n    pass(total);  // returns 10\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n```\n\nKey points:\n- `$` is type `int64`\n- `till(limit, step)` iterates from 0 to limit-1\n- Use `=> int32` to cast `$` to other integer types'
        ),
        # More loop() drills
        (
            "Write an Aria program using loop(5, 15, 3) to sum values.",
            'func:main = int32(){\n    int32:sum = 0;\n    loop(5, 15, 3){\n        int64:i = $;\n        sum = sum + i => int32;\n    }\n    pass(sum);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria loop that counts backwards from 10 to 1.",
            'func:main = int32(){\n    int32:count = 0;\n    loop(10, 0, -1){\n        int64:i = $;\n        count = count + 1;\n    }\n    pass(count);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write Aria code using loop() to iterate over indices and build a running total.",
            'func:main = int32(){\n    int32:total = 0;\n    loop(1, 11, 1){\n        int64:i = $;\n        total = total + i => int32;\n    }\n    pass(total);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "What is the difference between till() and loop() in Aria?",
            '`till(limit, step)` always starts from 0 and iterates up to (but not including) limit.\n`loop(start, limit, step)` lets you specify the starting value.\n\nBoth use `$` as the loop counter (type `int64`).\n\n```aria\n// till: iterates 0, 1, 2, 3, 4\nfunc:main = int32(){\n    till(5, 1){\n        int64:i = $;\n    }\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n```\n\n```aria\n// loop: iterates 10, 12, 14, 16, 18\nfunc:main = int32(){\n    loop(10, 20, 2){\n        int64:i = $;\n    }\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n```'
        ),
        (
            "Write an Aria program that uses loop() inside a function to compute a sum, then calls it from main.",
            'func:sum_range = int32(int32:lo, int32:hi){\n    int32:total = 0;\n    loop(lo => int64, hi => int64, 1){\n        int64:i = $;\n        total = total + i => int32;\n    }\n    pass(total);\n};\n\nfunc:main = int32(){\n    Result<int32>:r = sum_range(1i32, 100i32);\n    int32:result = r ? 0i32;\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Show a till loop with nested if-else in Aria.",
            'func:main = int32(){\n    int32:evens = 0;\n    int32:odds = 0;\n    till(10, 1){\n        int64:i = $;\n        int32:val = i => int32;\n        if(val % 2 == 0){\n            evens = evens + 1;\n        }else{\n            odds = odds + 1;\n        }\n    }\n    pass(evens + odds);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program with a while loop and a till loop working together.",
            'func:main = int32(){\n    int32:outer = 0;\n    int32:total = 0;\n    while(outer < 3){\n        till(5, 1){\n            int64:i = $;\n            total = total + i => int32;\n        }\n        outer = outer + 1;\n    }\n    pass(total);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Struct
        (
            "Write an Aria struct Point with int32 x and y fields. Write a function that creates a Point and returns it.",
            'struct:Point = {\n    int32:x;\n    int32:y;\n};\n\nfunc:make_point = Point(int32:px, int32:py){\n    Point:p = {x: px, y: py};\n    pass(p);\n};\n\nfunc:main = int32(){\n    Result<Point>:r = make_point(3, 4);\n    Point:pt = r ? Point{x: 0, y: 0};\n    pass(pt.x + pt.y);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Define an Aria struct Color with uint8 r, g, b fields.",
            'struct:Color = {\n    uint8:r;\n    uint8:g;\n    uint8:b;\n};\n\nfunc:make_red = Color(){\n    Color:c = {r: 255, g: 0, b: 0};\n    pass(c);\n};\n\nfunc:main = int32(){\n    Result<Color>:r = make_red();\n    Color:red = r ? Color{r: 0, g: 0, b: 0};\n    pass(red.r => int32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Cast operator =>
        (
            "Show how to cast an int64 value to int8 in Aria using the cast operator.",
            'func:main = int32(){\n    int64:big = 100;\n    int8:small = big => int8;\n    pass(small => int32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        (
            "Write an Aria program that casts an int32 to int64.",
            'func:main = int32(){\n    int32:x = 42;\n    int64:y = x => int64;\n    pass(y => int32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Pointers
        (
            "Write an Aria program that takes the address of a variable and dereferences it.",
            'func:main = int32(){\n    int32:value = 99;\n    int32->:ptr = @value;\n    int32:copy = <-ptr;\n    pass(copy);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Multiple functions
        (
            "Write an Aria program with three functions: add, subtract, and main.",
            'func:add = int32(int32:a, int32:b){\n    pass(a + b);\n};\n\nfunc:subtract = int32(int32:a, int32:b){\n    pass(a - b);\n};\n\nfunc:main = int32(){\n    Result<int32>:r1 = add(10, 5);\n    int32:sum = r1 ? 0i32;\n    Result<int32>:r2 = subtract(10, 5);\n    int32:diff = r2 ? 0i32;\n    pass(sum + diff);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Bitwise
        (
            "Write an Aria program that uses bitwise AND on two int32 values.",
            'func:main = int32(){\n    uint32:a = 0xFF;\n    uint32:b = 0x0F;\n    uint32:result = a & b;\n    pass(result => int32);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Boolean
        (
            "Write an Aria function that returns true if an int32 is even, false otherwise.",
            'func:is_even = bool(int32:x){\n    pass((x % 2) == 0);\n};\n\nfunc:main = int32(){\n    Result<bool>:r = is_even(4);\n    bool:result = r ? false;\n    if(result){\n        pass(1);\n    }\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Nesting
        (
            "Write an Aria program with nested if-else statements.",
            'func:classify = int32(int32:x){\n    if(x > 100){\n        pass(3);\n    }else if(x > 50){\n        pass(2);\n    }else if(x > 0){\n        pass(1);\n    }else{\n        pass(0);\n    }\n};\n\nfunc:main = int32(){\n    Result<int32>:r = classify(75);\n    int32:val = r ? 0i32;\n    pass(val);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Pick (switch)
        (
            "Write an Aria program using pick (switch statement).",
            'func:main = int32(){\n    int32:day = 3;\n    int32:result = 0;\n    pick(day){\n        (1) { result = 10; },\n        (2) { result = 20; },\n        (3) { result = 30; },\n        (*) { result = 0; }\n    }\n    pass(result);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Float
        (
            "Write an Aria program that adds two flt64 values.",
            'func:main = int32(){\n    flt64:a = 3.14;\n    flt64:b = 2.72;\n    flt64:sum = a + b;\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # Enum
        (
            "Define an Aria enum Direction with values NORTH, SOUTH, EAST, WEST.",
            'enum:Direction = {\n    NORTH = 0,\n    SOUTH = 1,\n    EAST = 2,\n    WEST = 3\n};\n\nfunc:main = int32(){\n    // Enum values used as constants\n    int32:north = 0;\n    int32:south = 1;\n    pass(north);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
        # String interpolation
        (
            "Write an Aria program that uses string interpolation with backticks.",
            'func:main = int32(){\n    int32:age = 25;\n    string:name = "Aria";\n    print(`Hello from &{name}, age &{age}`);\n    pass(0);\n};\n\nfunc:failsafe = NIL(int32:err_code){\n};\n'
        ),
    ]

    # Each drill gets HEAVY weight
    for prompt, code in templates:
        drills.append(make_example(
            instruction=prompt,
            output=f"```aria\n{code}```",
            category="grammar_drill",
            source="synthetic_v4",
            weight=4.0,
            features=detect_features(code)
        ))

        # Also add with slight variation in prompt
        alt_prompt = prompt.replace("Write", "Create").replace("Show", "Demonstrate")
        if alt_prompt != prompt:
            drills.append(make_example(
                instruction=alt_prompt,
                output=f"```aria\n{code}```",
                category="grammar_drill",
                source="synthetic_v4",
                weight=3.5,
                features=detect_features(code)
            ))

    return drills


def generate_terminator_rules():
    """Generate explicit grammar rule examples for }; awareness."""
    rules = []

    rule_pairs = [
        (
            "What is required at the end of every Aria function definition?",
            "Every function in Aria MUST end with `};` — a closing brace followed by a semicolon. "
            "This is different from C/C++/Rust where functions end with just `}`.\n\n"
            "```aria\n// CORRECT:\nfunc:hello = int32(){\n    pass(0);\n};\n\n"
            "// WRONG — missing semicolon:\nfunc:hello = int32(){\n    pass(0);\n}\n```\n\n"
            "This applies to ALL top-level definitions: functions, structs, enums.\n\n"
            "```aria\nstruct:Point = {\n    int32:x;\n    int32:y;\n};  // <-- semicolon required\n\n"
            "enum:Color = {\n    RED = 0,\n    GREEN = 1\n};  // <-- semicolon required\n```"
        ),
        (
            "Why do Aria functions end with }; instead of just }?",
            "In Aria, functions, structs, and enums are all top-level declarations that end with `};`. "
            "The semicolon after the closing brace marks the end of the declaration. This is similar "
            "to how C struct definitions end with `};` — Aria extends this pattern to all definitions.\n\n"
            "```aria\n// Functions end with };\nfunc:add = int32(int32:a, int32:b){\n    pass(a + b);\n};\n\n"
            "// Structs end with };\nstruct:Vec2 = {\n    flt64:x;\n    flt64:y;\n};\n\n"
            "// Enums end with };\nenum:Status = {\n    OK = 0,\n    ERR = 1\n};\n```"
        ),
        (
            "What is the failsafe function in Aria?",
            "Every Aria program that has a `main` function MUST also have a `failsafe` function. "
            "It is the global error handler — called when an unhandled `fail()` occurs.\n\n"
            "Minimal failsafe:\n```aria\nfunc:failsafe = NIL(int32:err_code){\n};\n```\n\n"
            "Complete program pattern:\n```aria\nfunc:main = int32(){\n    // your code here\n    pass(0);\n};\n\n"
            "func:failsafe = NIL(int32:err_code){\n    // handle errors\n};\n```"
        ),
        (
            "What is the correct structure of a complete Aria program?",
            "A complete Aria program requires:\n1. A `func:main` that returns `int32`\n"
            "2. A `func:failsafe` error handler\n3. All functions/structs/enums end with `};`\n"
            "4. `pass(value)` to return values\n\n"
            "```aria\n// Optional: struct/enum definitions\nstruct:Data = {\n    int32:value;\n};\n\n"
            "// Optional: helper functions\nfunc:process = int32(int32:x){\n    pass(x * 2);\n};\n\n"
            "// Required: main function\nfunc:main = int32(){\n    Result<int32>:r = process(21);\n    int32:result = r ? 0i32;\n    pass(result);\n};\n\n"
            "// Required: failsafe error handler\nfunc:failsafe = NIL(int32:err_code){\n};\n```"
        ),
    ]

    for q, a in rule_pairs:
        rules.append(make_example(
            instruction=q,
            output=a,
            category="grammar_rule",
            source="synthetic_v4",
            weight=4.0
        ))
        # Triple up on these critical rules
        rules.append(make_example(
            instruction=q.replace("?", "").strip() + " — explain with examples.",
            output=a,
            category="grammar_rule",
            source="synthetic_v4",
            weight=3.5
        ))

    return rules


def clean_v3_corpus():
    """Load V3 corpus, fix stray closing braces, filter low-quality."""
    v3_path = REPO_ROOT / "tools" / "aria_training_corpus_v3.jsonl"
    if not v3_path.exists():
        print("  V3 corpus not found, skipping")
        return []

    cleaned = []
    fixed = 0
    dropped = 0

    with open(v3_path) as f:
        for line in f:
            ex = json.loads(line)
            out = ex.get("output", "")

            # Fix stray closing brace at end of code blocks
            # Multiple patterns: }\n```, }\r\n```, }  \n```
            if "```" in out:
                out_fixed = out
                # Pattern 1: lone } right before closing ```
                out_fixed = re.sub(r'\n\}\s*\n```', '\n```', out_fixed)
                # Pattern 2: } as last line inside code block
                out_fixed = re.sub(r'\n\}\s*```', '\n```', out_fixed)
                # Pattern 3: code block ending with };\n}\n``` (extra stray after valid };)
                out_fixed = re.sub(r'(};\s*\n)\}\s*\n```', r'\1```', out_fixed)
                if out_fixed != out:
                    fixed += 1
                    ex["output"] = out_fixed
                    out = out_fixed

            # Fix void -> NIL in failsafe functions (void only valid in extern blocks)
            if "func:failsafe = void" in out:
                out = out.replace("func:failsafe = void", "func:failsafe = NIL")
                ex["output"] = out

            # Drop examples with no useful content
            if len(out.strip()) < 20:
                dropped += 1
                continue

            # Reduce weight of pure-explanation examples
            cat = ex.get("metadata", {}).get("category", "")
            if "```" not in out and "func:" not in out:
                ex["weight"] = max(ex.get("weight", 1.0) * 0.5, 0.3)

            # Boost examples that have compilable patterns
            if "func:main" in out and "};" in out and "failsafe" in out:
                ex["weight"] = max(ex.get("weight", 1.0), 2.5)

            cleaned.append(ex)

    print(f"  V3: {len(cleaned)} kept, {fixed} stray braces fixed, {dropped} dropped")
    return cleaned


def build_v4_corpus():
    """Build the complete V4 corpus."""
    print("=" * 60)
    print("  ARIA V4 CORPUS BUILDER")
    print("=" * 60)

    all_examples = []

    # Phase 1: Compilable programs (THE CORE)
    print("\n[1/4] Loading compilable .aria files...")
    compilable_files = load_compilable_files()
    compilable_examples = generate_compilable_examples(compilable_files)
    all_examples.extend(compilable_examples)
    print(f"  Generated {len(compilable_examples)} examples from {len(compilable_files)} compilable files")

    # Phase 2: Grammar drills
    print("\n[2/4] Generating grammar drills...")
    drills = generate_grammar_drills()
    terminator_rules = generate_terminator_rules()
    all_examples.extend(drills)
    all_examples.extend(terminator_rules)
    print(f"  Generated {len(drills)} drill examples + {len(terminator_rules)} rule examples")

    # Phase 3: Clean V3 corpus
    print("\n[3/4] Cleaning V3 corpus...")
    v3_cleaned = clean_v3_corpus()
    all_examples.extend(v3_cleaned)

    # Phase 4: Deduplicate by instruction text
    print("\n[4/4] Deduplicating...")
    seen = set()
    deduped = []
    for ex in all_examples:
        key = ex["instruction"].strip().lower()[:200]
        if key not in seen:
            seen.add(key)
            deduped.append(ex)

    # Statistics
    print(f"\n{'=' * 60}")
    print(f"  V4 CORPUS STATISTICS")
    print(f"{'=' * 60}")
    print(f"  Total examples: {len(deduped)}")

    cats = {}
    feat_counts = {}
    total_weight = 0
    has_code = 0
    has_main = 0
    has_sb = 0
    has_failsafe = 0
    has_till = 0
    has_loop = 0

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
        if "till(" in out:
            has_till += 1
        if "loop(" in out:
            has_loop += 1

        for feat in ex.get("metadata", {}).get("features", []):
            feat_counts[feat] = feat_counts.get(feat, 0) + 1

    print(f"  Total weight: {total_weight:.0f}")
    print(f"  Has code: {has_code} ({100*has_code//len(deduped)}%)")
    print(f"  Has func:main: {has_main} ({100*has_main//len(deduped)}%)")
    print(f"  Has }};: {has_sb} ({100*has_sb//len(deduped)}%)")
    print(f"  Has failsafe: {has_failsafe} ({100*has_failsafe//len(deduped)}%)")
    print(f"  Has till(): {has_till} ({100*has_till//len(deduped)}%)")
    print(f"  Has loop(): {has_loop} ({100*has_loop//len(deduped)}%)")
    print(f"\n  Categories:")
    for k, v in sorted(cats.items(), key=lambda x: -x[1]):
        print(f"    {k}: {v}")
    print(f"\n  Top features:")
    for k, v in sorted(feat_counts.items(), key=lambda x: -x[1])[:15]:
        print(f"    {k}: {v}")

    # Save
    output_path = REPO_ROOT / "tools" / "aria_training_corpus_v4.jsonl"
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

    train_path = REPO_ROOT / "tools" / "aria_training_corpus_v4_train.jsonl"
    val_path = REPO_ROOT / "tools" / "aria_training_corpus_v4_val.jsonl"
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
    build_v4_corpus()
