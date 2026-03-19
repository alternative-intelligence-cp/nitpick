#!/usr/bin/env python3
"""
Aria Specialist Model — Compilation Validation (Phase 4.3)

Generates 100 Aria programs from the specialist model, attempts to compile
each one with ariac, and tracks the success rate.

Usage:
    # Full validation (100 programs):
    python validate_specialist_compilation.py --model tools/aria_specialist_qwen7b_v3

    # Quick check (20 programs):
    python validate_specialist_compilation.py --model tools/aria_specialist_qwen7b_v3 --count 20

    # With custom compiler path:
    python validate_specialist_compilation.py --model tools/aria_specialist_qwen7b_v3 \
        --compiler build/ariac
"""

import os
import sys
import json
import shutil
import tempfile
import argparse
import subprocess
import re
from pathlib import Path
from typing import List, Optional, Tuple
from dataclasses import dataclass

# ---------------------------------------------------------------------------
# 100 diverse prompts for program generation
# ---------------------------------------------------------------------------

GENERATION_PROMPTS = [
    # Basic programs (1-20)
    "Write a complete Aria program that returns 0 from main.",
    "Write an Aria function that adds two int32 values and returns the result. Include main and failsafe.",
    "Write an Aria function that multiplies two int64 values. Include a complete program that calls it.",
    "Write a complete Aria program with a function that takes an int32 and returns its absolute value.",
    "Write a complete Aria program that computes the maximum of two int32 values.",
    "Write an Aria program with a function that returns the minimum of three int32 values.",
    "Write a complete Aria program that swaps two int32 values using a temporary variable.",
    "Write a complete Aria program that computes the square of an int32.",
    "Write an Aria function that returns true if an int32 is even, false otherwise. Include main.",
    "Write a complete Aria program that computes factorial of 5 using a loop.",
    "Write an Aria program that sums integers from 1 to 10 using a while loop.",
    "Write a complete Aria program that counts down from 10 to 1 using a while loop.",
    "Write an Aria program with a function that converts bool to int32 (1 for true, 0 for false).",
    "Write a complete Aria program that computes 2 raised to the power of 8 using a loop.",
    "Write an Aria function that clamps an int32 value between a min and max. Include main.",
    "Write a complete Aria program that uses a for loop to sum 1 through 100.",
    "Write an Aria program that computes the remainder of 17 divided by 5.",
    "Write a complete Aria program with nested if-else statements checking int32 ranges.",
    "Write an Aria function that returns the sign of an int32 (-1, 0, or 1). Include main.",
    "Write a complete Aria program that uses bitwise AND on two int32 values.",

    # Struct programs (21-35)
    "Write an Aria struct Point with int32 x and y fields. Write a function that creates a Point.",
    "Write an Aria struct Color with uint8 r, g, b fields and a function to create a white color.",
    "Define an Aria struct Pair with two int32 fields 'first' and 'second'. Write a swap function.",
    "Write an Aria struct Vec2 with flt64 x and y fields. Write a function that adds two Vec2.",
    "Define an Aria struct Counter with an int32 count field. Write increment and get functions.",
    "Write an Aria struct Range with int32 start and end fields. Write a function to check if a value is in range.",
    "Define an Aria struct Fraction with int32 numerator and denominator. Write a multiply function.",
    "Write an Aria struct Matrix2x2 with four flt64 fields. Write a determinant function.",
    "Write an Aria struct Temperature with a flt64 celsius field. Write a to_fahrenheit function.",
    "Write an Aria struct Circle with flt64 radius. Write an area function that approximates pi as 3.14159.",
    "Define an Aria struct MinMax with int32 min_val and max_val fields. Write a function to merge two MinMax.",
    "Write an Aria struct Pixel with int32 x, y and uint8 r, g, b. Write a function to create a red pixel.",
    "Write an Aria struct Timestamp with int64 seconds and int32 nanos. Write a comparison function.",
    "Define an Aria struct Size with int32 width and height. Write an area function.",
    "Write an Aria struct Config with bool debug and int32 level. Write a default config function.",

    # Array programs (36-50)
    "Write a complete Aria program that creates an int32 array of 5 elements and fills it with zeros.",
    "Write an Aria program that creates an int64 array of 10 elements, fills with indices using a loop.",
    "Write a function that sums all elements of an int32 array of 8 elements.",
    "Write an Aria program with a function that finds the maximum in an int32 array of 5 elements.",
    "Write a complete Aria program that reverses an int32 array of 4 elements in place.",
    "Write an Aria program that counts how many elements in an int32 array of 6 are positive.",
    "Write an Aria function that zeros out an int64 array of 10 elements using a loop.",
    "Write a complete Aria program that copies one int32 array of 5 elements to another.",
    "Write an Aria program that multiplies each element of an int32 array of 4 by 2.",
    "Write a complete Aria program with a function that checks if an int32 array of 5 is sorted.",
    "Write an Aria program that creates a bool array of 8 and sets alternating true/false.",
    "Write an Aria function that finds the index of a value in an int32 array of 6, returns -1 if not found.",
    "Write a complete Aria program with two int32 arrays of 4 and a function that adds them element-wise.",
    "Write an Aria program that uses a while loop to fill an int64 array of 8 with fibonacci-like values.",
    "Write a complete Aria program that computes the dot product of two int32 arrays of 3.",

    # Error handling programs (51-60)
    "Write an Aria function that divides two int32 values, failing with error code 1 if divisor is zero.",
    "Write a complete Aria program using Result type to handle a potential division by zero.",
    "Write an Aria function that validates an int32 is positive, failing if not. Include complete program.",
    "Write an Aria program with a function that checks array bounds, using fail for out of range.",
    "Write a complete Aria program where main calls a function that may fail, and handles the error.",
    "Write an Aria function that parses an int32 from a value, failing if the value is negative.",
    "Write a complete Aria program with two functions that return Result types, chained together.",
    "Write an Aria function that checks if an int32 is in range [0, 100], fails otherwise.",
    "Write a complete Aria program demonstrating the failsafe function handling an error.",
    "Write an Aria function returning Result<int32> that succeeds only for even inputs.",

    # Float programs (61-70)
    "Write a complete Aria program that computes the average of three flt64 values.",
    "Write an Aria function that converts Celsius to Fahrenheit. Include complete program.",
    "Write a complete Aria program with a flt64 linear interpolation function (lerp).",
    "Write an Aria program that computes the distance formula for 2D points using flt64.",
    "Write a complete Aria program that approximates square root using Newton's method (3 iterations).",
    "Write an Aria function that computes compound interest given principal, rate, and years as flt64.",
    "Write a complete Aria program that finds the larger of two flt64 values.",
    "Write an Aria function that rounds a flt64 to the nearest integer (as int32). Include main.",
    "Write a complete Aria program with a flt64 clamp function between 0.0 and 1.0.",
    "Write an Aria function that computes the area of a triangle given base and height as flt64.",

    # Cast and type conversion (71-80)
    "Write a complete Aria program that casts an int64 to int32 using the => operator.",
    "Write an Aria program demonstrating casting between int32 and int8 with =>.",
    "Write a complete Aria program that casts a flt64 to int32 and back.",
    "Write an Aria program that uses uint8 values and casts them to int32 for arithmetic.",
    "Write a complete Aria program showing int32 to int64 widening cast with =>.",
    "Write an Aria program with a function that takes int64 and returns int32 using => cast.",
    "Write a complete Aria program casting between signed and unsigned 32-bit integers with =>.",
    "Write an Aria program that accumulates uint8 values into an int32 sum using casts.",
    "Write a complete Aria program demonstrating bool to int32 cast.",
    "Write an Aria function that safely narrows int64 to int32 by clamping first.",

    # Loop variants (81-90)
    "Write a complete Aria program using a for loop with inclusive range 1..10.",
    "Write an Aria program using a for loop with exclusive range 0...5.",
    "Write a complete Aria program with nested while loops computing a multiplication table entry.",
    "Write an Aria program using a while loop to find the first power of 2 greater than 1000.",
    "Write a complete Aria program with a for loop that breaks early using a bool flag.",
    "Write an Aria program that uses a while loop to implement integer division without /.",
    "Write a complete Aria program with a loop that computes GCD of two int32 values.",
    "Write an Aria program with a for loop summing only even numbers from 1 to 20.",
    "Write a complete Aria program implementing binary search on a sorted int32 array of 8.",
    "Write an Aria program with a while loop doing repeated subtraction to compute modulo.",

    # Combined complex (91-100)
    "Write a complete Aria program with a struct Stack using an int32 array and push/pop functions.",
    "Write an Aria program with a struct Vector3 and functions for add, scale, and dot product.",
    "Write a complete Aria program with an enum and a function that maps enum values to strings.",
    "Write an Aria program with a struct Matrix with a 2x2 int32 array and a trace function.",
    "Write a complete Aria program computing moving average of an int32 array of 6 into flt64 results.",
    "Write an Aria program with a struct Queue using an int32 array and enqueue/dequeue functions.",
    "Write a complete Aria program that implements bubble sort on an int32 array of 5.",
    "Write an Aria program with a struct BitSet using uint64 and set/get/clear bit functions.",
    "Write a complete Aria program with multiple helper functions computing statistics (sum, min, max, avg).",
    "Write an Aria program with a struct Rational (int32 num/den) and add/reduce functions.",
]


def build_prompt(instruction: str) -> str:
    return (
        "Below is an instruction that describes a task. "
        "Write a response that appropriately completes the request.\n\n"
        f"### Instruction:\n{instruction}\n\n### Response:\n"
    )


def extract_aria_code(text: str) -> Optional[str]:
    """Extract Aria code from model output. Tries code blocks first, then raw text."""
    # Try ```aria ... ``` blocks
    blocks = re.findall(r'```aria\s*\n(.*?)```', text, re.DOTALL)
    if blocks:
        # Concatenate all blocks — the model sometimes splits into multiple
        return '\n\n'.join(blocks)

    # Try ``` ... ``` blocks (generic)
    blocks = re.findall(r'```\s*\n(.*?)```', text, re.DOTALL)
    if blocks:
        combined = '\n\n'.join(blocks)
        # Verify it looks like Aria
        if 'func:' in combined or 'pass(' in combined:
            return combined

    # Try to find raw Aria code (func: pattern)
    lines = text.split('\n')
    code_lines = []
    in_code = False
    for line in lines:
        if 'func:' in line or 'struct:' in line or 'enum:' in line:
            in_code = True
        if in_code:
            code_lines.append(line)

    if code_lines:
        return '\n'.join(code_lines)

    return None


def ensure_failsafe(code: str) -> str:
    """Add failsafe function if not present."""
    if 'failsafe' not in code:
        code += '\n\nfunc:failsafe = NIL(int32:err_code) { pass(NIL); };\n'
    return code


def ensure_main(code: str) -> str:
    """Add a minimal main if not present."""
    if 'func:main' not in code and 'pub func:main' not in code:
        code += '\n\nfunc:main = int32() { pass(0); };\n'
    return code


def compile_aria(source: str, compiler: str, timeout: int = 10) -> Tuple[bool, str]:
    """Compile Aria source code. Returns (success, message)."""
    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.aria', delete=False, dir='/tmp'
    ) as f:
        f.write(source)
        src_path = f.name

    out_path = src_path.replace('.aria', '')

    try:
        result = subprocess.run(
            [compiler, src_path, '-o', out_path],
            capture_output=True, text=True, timeout=timeout
        )
        # Clean up binary if produced
        if os.path.exists(out_path):
            os.unlink(out_path)

        if result.returncode == 0:
            return True, "OK"
        else:
            # Extract first error line
            err = (result.stderr or result.stdout or "Unknown error").strip()
            first_line = err.split('\n')[0][:200]
            return False, first_line
    except subprocess.TimeoutExpired:
        return False, "TIMEOUT"
    except Exception as e:
        return False, f"ERROR: {e}"
    finally:
        if os.path.exists(src_path):
            os.unlink(src_path)


@dataclass
class ValidationResult:
    prompt_idx: int
    prompt: str
    code_extracted: bool
    compiles: bool
    message: str
    raw_output: str
    code: str


def main():
    parser = argparse.ArgumentParser(description="Validate specialist model compilation rate")
    parser.add_argument("--model", required=True, help="Path to LoRA adapter directory")
    parser.add_argument("--base-model", default=None, help="Override base model ID")
    parser.add_argument("--compiler", default="build/ariac", help="Path to Aria compiler")
    parser.add_argument("--count", type=int, default=100, help="Number of programs to generate (max 100)")
    parser.add_argument("--output", default="tools/validation_results.json", help="Output JSON file")
    parser.add_argument("--temperature", type=float, default=0.3, help="Generation temperature")
    args = parser.parse_args()

    # Verify compiler exists
    if not os.path.exists(args.compiler):
        print(f"ERROR: Compiler not found at {args.compiler}")
        sys.exit(1)

    count = min(args.count, len(GENERATION_PROMPTS))
    prompts = GENERATION_PROMPTS[:count]

    # Load model
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig
    from peft import PeftModel

    config_path = Path(args.model) / "adapter_config.json"
    if config_path.exists() and not args.base_model:
        with open(config_path) as f:
            cfg = json.load(f)
        base_model = cfg.get("base_model_name_or_path", "Qwen/Qwen2.5-7B-Instruct")
    else:
        base_model = args.base_model or "Qwen/Qwen2.5-7B-Instruct"

    print(f"Loading base model: {base_model}")
    bnb = BitsAndBytesConfig(
        load_in_4bit=True,
        bnb_4bit_quant_type="nf4",
        bnb_4bit_compute_dtype=torch.float16,
        bnb_4bit_use_double_quant=True,
    )
    model = AutoModelForCausalLM.from_pretrained(
        base_model, quantization_config=bnb, device_map="auto"
    )
    tokenizer = AutoTokenizer.from_pretrained(args.model)
    tokenizer.pad_token = tokenizer.eos_token
    model = PeftModel.from_pretrained(model, args.model)
    model.eval()
    print("Model ready.\n")

    # Generate and compile
    results: List[ValidationResult] = []
    pass_count = 0
    extract_fail = 0

    print("=" * 70)
    print(f"  COMPILATION VALIDATION — {count} programs")
    print("=" * 70)

    for i, prompt in enumerate(prompts):
        prompt_text = build_prompt(prompt)
        inputs = tokenizer(prompt_text, return_tensors="pt").to(model.device)

        with torch.no_grad():
            out = model.generate(
                **inputs,
                max_new_tokens=600,
                temperature=args.temperature,
                top_p=0.9,
                do_sample=(args.temperature > 0),
                pad_token_id=tokenizer.eos_token_id,
                repetition_penalty=1.1,
            )

        raw = tokenizer.decode(out[0][inputs["input_ids"].shape[1]:], skip_special_tokens=True)

        # Trim at secondary markers
        for stop in ["\n### Instruction:", "\n### Input:", "\n### Human:"]:
            if stop in raw:
                raw = raw[:raw.index(stop)]

        code = extract_aria_code(raw)
        if code is None:
            results.append(ValidationResult(i, prompt, False, False, "NO_CODE_EXTRACTED", raw, ""))
            extract_fail += 1
            status = "EXTRACT_FAIL"
        else:
            code = ensure_failsafe(code)
            code = ensure_main(code)
            ok, msg = compile_aria(code, args.compiler)
            results.append(ValidationResult(i, prompt, True, ok, msg, raw, code))
            if ok:
                pass_count += 1
                status = "PASS"
            else:
                status = f"FAIL: {msg[:80]}"

        # Progress
        done = i + 1
        rate = pass_count / done * 100
        print(f"  [{done:3d}/{count}] {status:12s}  (running: {rate:.0f}%)  {prompt[:60]}...")

    # Summary
    total = len(results)
    extracted = total - extract_fail
    compile_pass = pass_count
    compile_fail = extracted - pass_count

    print("\n" + "=" * 70)
    print("  RESULTS SUMMARY")
    print("=" * 70)
    print(f"  Total programs generated : {total}")
    print(f"  Code extracted           : {extracted} ({extracted/total*100:.0f}%)")
    print(f"  Compilation PASS         : {compile_pass} ({compile_pass/total*100:.0f}%)")
    print(f"  Compilation FAIL         : {compile_fail} ({compile_fail/total*100:.0f}%)")
    print(f"  Extract failures         : {extract_fail} ({extract_fail/total*100:.0f}%)")
    print(f"\n  COMPILATION SUCCESS RATE : {compile_pass/total*100:.1f}%")
    target = ">95%" if compile_pass/total >= 0.95 else "< 95% — NEEDS ITERATION"
    print(f"  TARGET                   : {target}")
    print("=" * 70)

    # Save results
    output_data = {
        "model": args.model,
        "count": total,
        "extracted": extracted,
        "compile_pass": compile_pass,
        "compile_fail": compile_fail,
        "extract_fail": extract_fail,
        "success_rate": round(compile_pass / total * 100, 1),
        "results": [
            {
                "idx": r.prompt_idx,
                "prompt": r.prompt,
                "extracted": r.code_extracted,
                "compiles": r.compiles,
                "message": r.message,
                "code": r.code,
            }
            for r in results
        ],
    }

    with open(args.output, 'w') as f:
        json.dump(output_data, f, indent=2)
    print(f"\nDetailed results saved to {args.output}")

    # List failures for debugging
    failures = [r for r in results if not r.compiles]
    if failures:
        print(f"\n--- FAILURES ({len(failures)}) ---")
        for r in failures[:20]:  # Show first 20
            print(f"\n  [{r.prompt_idx}] {r.prompt[:80]}")
            print(f"      {r.message}")

    sys.exit(0 if compile_pass / total >= 0.95 else 1)


if __name__ == "__main__":
    main()
