#!/usr/bin/env python3
"""
Aria Specialist Model — Compile-Graded Evaluation

Augments the keyword-check eval (eval_aria_specialist.py) with a real
ariac compile pass.  A test case passes only when BOTH criteria hold:

  1. Keyword checks (same logic as eval_aria_specialist.py)
  2. ariac compiles the generated code without errors

The compile pass rate is the only metric that proves the model actually
learned Aria syntax rather than just learning which keywords to echo.

Usage:
    # Evaluate the final adapter (all levels):
    python tools/eval_aria_compile.py

    # Compare checkpoints:
    python tools/eval_aria_compile.py --checkpoint checkpoint-200 --output results_200.json
    python tools/eval_aria_compile.py --checkpoint checkpoint-378 --output results_378.json

    # Quick smoke-test (levels 1-2 only):
    python tools/eval_aria_compile.py --levels 1,2

    # Use a different base model path:
    python tools/eval_aria_compile.py --model ./tools/aria_specialist_model_qwen
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional

import torch
from peft import PeftModel
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig

# ---------------------------------------------------------------------------
# ariac binary discovery
# ---------------------------------------------------------------------------

REPO_ROOT = Path(__file__).resolve().parent.parent
ARIAC_BIN = (
    (Path(os.environ["ARIAC_BIN"]) if "ARIAC_BIN" in os.environ else None)
    or (REPO_ROOT / "build" / "ariac" if (REPO_ROOT / "build" / "ariac").exists() else None)
    or shutil.which("ariac")
)


def aria_compile(source: str) -> dict:
    """Compile Aria source; return {success, errors, warnings}."""
    if not ARIAC_BIN:
        return {"success": False, "errors": ["ariac not found"], "warnings": []}

    with tempfile.NamedTemporaryFile(suffix=".aria", mode="w",
                                     delete=False, encoding="utf-8") as f:
        f.write(source)
        src_path = f.name
    out_path = src_path + ".out"

    try:
        proc = subprocess.run(
            [str(ARIAC_BIN), src_path, "-o", out_path],
            capture_output=True, text=True, timeout=30,
        )
        errors, warnings = [], []
        for line in (proc.stderr + proc.stdout).splitlines():
            lo = line.lower()
            if "error" in lo:
                errors.append(line)
            elif "warning" in lo or "warn" in lo:
                warnings.append(line)
            elif line.strip():
                (errors if proc.returncode != 0 else warnings).append(line)
        return {"success": proc.returncode == 0, "errors": errors, "warnings": warnings}
    except subprocess.TimeoutExpired:
        return {"success": False, "errors": ["compile timeout"], "warnings": []}
    finally:
        for p in (src_path, out_path):
            try:
                os.unlink(p)
            except OSError:
                pass


# ---------------------------------------------------------------------------
# Eval cases (same EVAL_CASES as eval_aria_specialist.py, verbatim)
# ---------------------------------------------------------------------------

@dataclass
class EvalCase:
    level: int
    category: str
    instruction: str
    check_contains: List[str]
    check_not_contains: List[str]
    note: str = ""
    max_new_tokens: int = 350


EVAL_CASES: List[EvalCase] = [

    # ── Level 1 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=1,
        category="variable declaration",
        instruction="Declare an int32 variable named 'count' initialized to zero in Aria.",
        check_contains=["int32:count"],
        check_not_contains=["let ", "var ", "int count", "count: int", "count: i32"],
        note="Type-prefix style: int32:name = value",
    ),
    EvalCase(
        level=1,
        category="simple function",
        instruction=(
            "Write an Aria function called 'add' that takes two int32 parameters "
            "named 'a' and 'b' and returns their sum as int32."
        ),
        check_contains=["func:add", "int32", "pass("],
        check_not_contains=["return ", "def ", "fn ", "->"],
        note="func:name = ReturnType(params) { pass(value); }",
    ),
    EvalCase(
        level=1,
        category="string output",
        instruction='Write an Aria program that prints "Hello, Aria!" to stdout.',
        check_contains=["Hello, Aria"],
        check_not_contains=["println!(", "printf(", "console.log"],
        note="Aria uses stdout << value or print()",
    ),

    # ── Level 2 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=2,
        category="struct definition",
        instruction=(
            "Define a struct in Aria called 'Point' with two int32 fields: x and y. "
            "Then write a function that creates and returns a Point."
        ),
        check_contains=["struct:Point", "int32:x", "int32:y", "func:"],
        check_not_contains=["struct Point {", "class Point", "struct Point;"],
        note="struct:Name = { type:field; } syntax",
    ),
    EvalCase(
        level=2,
        category="enum definition",
        instruction=(
            "Define an enum in Aria called 'Direction' with four values: "
            "NORTH=0, SOUTH=1, EAST=2, WEST=3."
        ),
        check_contains=["enum:Direction", "NORTH", "SOUTH", "EAST", "WEST"],
        check_not_contains=["enum Direction {", "enum Direction:", "Direction::NORTH"],
        note="enum:Name = { VALUE = n, } syntax",
    ),
    EvalCase(
        level=2,
        category="struct with function",
        instruction=(
            "Write an Aria struct called 'Rectangle' with float64 fields 'width' and 'height', "
            "and a function 'area' that takes a Rectangle and returns its area as float64."
        ),
        check_contains=["struct:Rectangle", "float64:width", "float64:height", "func:area"],
        check_not_contains=["return ", "impl ", "class "],
        note="Integration of struct + function",
    ),

    # ── Level 3 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=3,
        category="Result<T> with fail",
        instruction=(
            "Write an Aria function called 'divide' that takes two int32 parameters "
            "and returns Result<int32>. Return fail(-1) if the divisor is zero, "
            "otherwise return pass(ok(a / b))."
        ),
        check_contains=["Result<int32>", "fail(", "pass("],
        check_not_contains=["throw ", "raise ", "try {", "catch", "return "],
        note="Result<T> with explicit pass/fail — no exceptions",
    ),
    EvalCase(
        level=3,
        category="? unwrap operator",
        instruction=(
            "Show how to call an Aria function that returns Result<int32> and "
            "unwrap the result using the ? operator, providing -1 as the default "
            "value if an error occurs."
        ),
        check_contains=["?", "-1"],
        check_not_contains=[".unwrap()", ".expect(", "try {", "catch"],
        note="? is the unwrap-or-default operator",
    ),
    EvalCase(
        level=3,
        category="is_error check",
        instruction=(
            "Write Aria code that calls a function returning Result<string>, "
            "checks if it succeeded using .is_error, and prints an appropriate message."
        ),
        check_contains=["Result<string>", ".is_error"],
        check_not_contains=["try {", "catch", ".is_ok()", "if err !="],
        note="Aria uses .is_error field, not .is_ok() method",
    ),

    # ── Level 4 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=4,
        category="till loop in-place mutation",
        instruction=(
            "Write an Aria till loop that iterates over an int32 array called "
            "'values' and doubles each element in place."
        ),
        check_contains=["till(", "values[$]", "$"],
        check_not_contains=["for i in", "for (int", "for i =", "values[i]"],
        note="till(length-1, 1) { arr[$] = ... } — $ is the index",
    ),
    EvalCase(
        level=4,
        category="$ captured as int64",
        instruction=(
            "Write an Aria loop from 0 to 9 (inclusive). Inside the loop, "
            "capture the current index $ into a named variable and use it."
        ),
        check_contains=["loop(", "$"],
        check_not_contains=["int32:i = $", "int:i = $", "i32:i = $"],
        note="CRITICAL: $ must be captured as int64, not int32",
    ),
    EvalCase(
        level=4,
        category="loop with array indexing",
        instruction=(
            "Write an Aria program that creates an int64 array of 10 elements, "
            "fills it with values 0 through 9 using a loop, and then prints each value."
        ),
        check_contains=["int64", "loop("],
        check_not_contains=["int32:i = $", "int[10]", "new int"],
        note="Tests array declaration + loop + $ in combination",
    ),

    # ── Level 5 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=5,
        category="pointer basics",
        instruction=(
            "In Aria, declare an int32 variable called 'value' set to 42. "
            "Take its address into a pointer, then dereference the pointer to read the value."
        ),
        check_contains=["int32->", "@"],
        check_not_contains=["&value", "int32*", "int32 *", "ref "],
        note="@ = address-of, int32-> = pointer type",
    ),
    EvalCase(
        level=5,
        category="cast operator =>",
        instruction="Show how to cast an int64 value to int8 in Aria using the cast operator.",
        check_contains=["=> int8", "int64"],
        check_not_contains=["as int8", "(int8)", "cast(", "=> {", "=> ("],
        note="CRITICAL: => is the CAST operator",
    ),
    EvalCase(
        level=5,
        category="cast precedence",
        instruction=(
            "In Aria, add two int64 values and cast only the second operand to int8 "
            "before the addition. Show the correct expression."
        ),
        check_contains=["=> int8"],
        check_not_contains=["(int8)", "as int8", "int8("],
        note="=> binds tighter than +: 'a + b => int8' casts b, not the sum",
    ),

    # ── Level 6 ─────────────────────────────────────────────────────────────

    EvalCase(
        level=6,
        category="inventory system",
        instruction=(
            "Write a complete Aria program with: "
            "(1) a struct 'Item' with string field 'name' and float64 field 'price', "
            "(2) a function 'create_item' that takes a name and price and returns an Item, "
            "(3) a function 'apply_discount' that takes an Item and a float64 discount "
            "percentage, returns Result<Item>, and fails if discount is not between 0 and 100."
        ),
        check_contains=["struct:Item", "float64:price", "Result<Item>", "fail(", "pass("],
        check_not_contains=["return ", "class Item", "throw ", "raise "],
        note="Full integration: struct + functions + Result<T>",
        max_new_tokens=500,
    ),
    EvalCase(
        level=6,
        category="linked list node",
        instruction=(
            "Define an Aria struct 'Node' that contains an int32 value and a pointer "
            "to the next Node (which may be null). Write a function to create a new Node."
        ),
        check_contains=["struct:Node", "int32:value", "int32->", "func:"],
        check_not_contains=["class Node", "struct Node {", "nullptr", "NULL"],
        note="Self-referential struct with pointer field",
        max_new_tokens=400,
    ),
]


# ---------------------------------------------------------------------------
# Prompt / scoring helpers
# ---------------------------------------------------------------------------

def build_prompt(instruction: str) -> str:
    return (
        "Below is an instruction that describes a task. "
        "Write a response that appropriately completes the request.\n\n"
        f"### Instruction:\n{instruction}\n\n### Response:\n"
    )


def extract_code_blocks(text: str) -> List[str]:
    """Pull out fenced ```...``` blocks; if none, return the whole response."""
    blocks = re.findall(r"```(?:aria|)?\s*\n(.*?)```", text, re.DOTALL)
    return blocks if blocks else [text]


def keyword_score(response: str, case: EvalCase) -> dict:
    hits = [s for s in case.check_contains if s in response]
    misses = [s for s in case.check_contains if s not in response]
    bad = [s for s in case.check_not_contains if s in response]
    precision = len(hits) / len(case.check_contains) if case.check_contains else 1.0
    penalty = 0.5 * (len(bad) / max(len(case.check_not_contains), 1))
    return {
        "score": round(max(0.0, precision - penalty), 2),
        "hits": hits,
        "misses": misses,
        "bad_patterns": bad,
    }


def try_compile(response: str) -> dict:
    """Try each code block from response; return best (first success, else last)."""
    blocks = extract_code_blocks(response)
    last = None
    for block in blocks:
        result = aria_compile(block)
        if result["success"]:
            return result
        last = result
    return last or {"success": False, "errors": ["no code found"], "warnings": []}


def render_bar(score: float, width: int = 10) -> str:
    filled = int(round(score * width))
    return "█" * filled + "░" * (width - filled)


LEVEL_LABELS = {
    1: "Basic syntax   ",
    2: "Structs/enums  ",
    3: "Error handling ",
    4: "Loops / $      ",
    5: "Pointers / cast",
    6: "Complex program",
}


# ---------------------------------------------------------------------------
# Model wrapper
# ---------------------------------------------------------------------------

class AriaEvaluator:
    def __init__(self, model_path: str, base_model: Optional[str] = None):
        self.model_path = model_path
        self.base_model = base_model
        self.model = None
        self.tokenizer = None

    def load(self):
        print(f"Loading adapter : {self.model_path}")
        config_path = Path(self.model_path) / "adapter_config.json"
        if config_path.exists() and not self.base_model:
            with open(config_path) as f:
                cfg = json.load(f)
            self.base_model = cfg.get("base_model_name_or_path",
                                      "mistralai/Mistral-7B-v0.3")
        elif not self.base_model:
            self.base_model = "mistralai/Mistral-7B-v0.3"
        print(f"Base model      : {self.base_model}")
        print("Loading in 4-bit NF4 …")

        bnb = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.float16,
            bnb_4bit_use_double_quant=True,
        )
        base = AutoModelForCausalLM.from_pretrained(
            self.base_model, quantization_config=bnb, device_map="auto"
        )
        self.tokenizer = AutoTokenizer.from_pretrained(self.model_path)
        self.tokenizer.pad_token = self.tokenizer.eos_token
        self.model = PeftModel.from_pretrained(base, self.model_path)
        self.model.eval()
        print("✓ Model ready\n")

    def generate(self, prompt: str, max_new_tokens: int = 350) -> str:
        inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
        with torch.no_grad():
            out = self.model.generate(
                **inputs,
                max_new_tokens=max_new_tokens,
                temperature=0.2,
                top_p=0.9,
                do_sample=True,
                pad_token_id=self.tokenizer.eos_token_id,
                repetition_penalty=1.1,
            )
        return self.tokenizer.decode(out[0], skip_special_tokens=True)

    def run(self, cases: List[EvalCase], output_file: Optional[str] = None) -> list:
        results = []
        level_kw: dict = {}   # keyword scores per level
        level_cp: dict = {}   # compile-pass bool per level

        if not ARIAC_BIN:
            print("⚠  WARNING: ariac not found — compile scores will all be 0.")
            print(f"   Set ARIAC_BIN or ensure build/ariac exists in {REPO_ROOT}\n")

        print("=" * 72)
        print("  ARIA SPECIALIST — COMPILE-GRADED EVALUATION")
        print("=" * 72)

        for idx, case in enumerate(cases, 1):
            print(f"\n[{idx}/{len(cases)}] L{case.level} – {case.category.upper()}")
            print(f"  {case.instruction[:100]}{'…' if len(case.instruction) > 100 else ''}")

            prompt = build_prompt(case.instruction)
            raw = self.generate(prompt, max_new_tokens=case.max_new_tokens)

            # Extract response
            response = (
                raw.split("### Response:", 1)[-1].strip()
                if "### Response:" in raw else raw.strip()
            )

            # Keyword score
            ks = keyword_score(response, case)

            # Compile score
            cr = try_compile(response)
            compile_ok = cr["success"]

            # Combined pass: keywords ≥ 1.0 AND compiles
            # (partial keyword credit still shown, but "compile pass" is binary)
            combined = round(ks["score"] * (1.0 if compile_ok else 0.0), 2)

            print(f"\n  Keywords : [{render_bar(ks['score'])}] {ks['score']:.0%}", end="")
            print(f"  |  Compile: {'✅ PASS' if compile_ok else '❌ FAIL'}", end="")
            print(f"  |  Combined: {combined:.0%}")
            if ks["hits"]:
                print(f"  ✅ Found   : {ks['hits']}")
            if ks["misses"]:
                print(f"  ❌ Missing : {ks['misses']}")
            if ks["bad_patterns"]:
                print(f"  ⚠️  Wrong   : {ks['bad_patterns']}")
            if not compile_ok and cr["errors"]:
                # Show only first 3 error lines to keep output clean
                for e in cr["errors"][:3]:
                    print(f"  ✗  {e.strip()}")
                if len(cr["errors"]) > 3:
                    print(f"  ✗  … ({len(cr['errors']) - 3} more errors)")

            print("\n  ┌─ Generated response " + "─" * 49)
            for line in response.splitlines()[:25]:
                print(f"  │ {line}")
            if len(response.splitlines()) > 25:
                print(f"  │ … ({len(response.splitlines()) - 25} more lines)")
            print("  └" + "─" * 71)

            level_kw.setdefault(case.level, []).append(ks["score"])
            level_cp.setdefault(case.level, []).append(1.0 if compile_ok else 0.0)

            results.append({
                "level": case.level,
                "category": case.category,
                "instruction": case.instruction,
                "note": case.note,
                "keyword_score": ks["score"],
                "compile_pass": compile_ok,
                "combined_score": combined,
                "hits": ks["hits"],
                "misses": ks["misses"],
                "bad_patterns": ks["bad_patterns"],
                "compile_errors": cr["errors"][:5],
                "response": response,
            })

        # ── Summary table ─────────────────────────────────────────────────────
        print("\n" + "=" * 72)
        print("  SUMMARY")
        print("=" * 72)
        print(f"  {'Level':<26s} {'Keywords':>10s}  {'Compile':>10s}  {'Combined':>10s}")
        print("  " + "-" * 60)

        all_kw, all_cp, all_comb = [], [], []
        for level in sorted(level_kw):
            kw_avg = sum(level_kw[level]) / len(level_kw[level])
            cp_avg = sum(level_cp[level]) / len(level_cp[level])
            co_avg = kw_avg * cp_avg   # both required
            label = f"L{level} {LEVEL_LABELS.get(level, '')}"
            print(f"  {label:<26s} {kw_avg:>9.0%}   {cp_avg:>9.0%}   {co_avg:>9.0%}")
            all_kw.extend(level_kw[level])
            all_cp.extend(level_cp[level])
            all_comb.append(co_avg)

        print("  " + "-" * 60)
        overall_kw = sum(all_kw) / len(all_kw) if all_kw else 0.0
        overall_cp = sum(all_cp) / len(all_cp) if all_cp else 0.0
        overall_co = sum(all_comb) / len(all_comb) if all_comb else 0.0
        print(f"  {'OVERALL':<26s} {overall_kw:>9.0%}   {overall_cp:>9.0%}   {overall_co:>9.0%}")
        print("=" * 72)

        # Verdict based on compile pass rate (the honest metric)
        if overall_cp >= 0.80:
            verdict = "Strong — model generates compilable Aria code reliably"
        elif overall_cp >= 0.60:
            verdict = "Good — most output compiles; niche patterns still need work"
        elif overall_cp >= 0.40:
            verdict = "Moderate — compiles ~half the time; significant syntax errors remain"
        elif overall_cp >= 0.20:
            verdict = "Weak — rarely produces compilable code; more training data needed"
        else:
            verdict = "Poor — model generates syntactically invalid Aria consistently"

        print(f"\n  Compile verdict: {verdict}")

        # Regression flags
        if level_cp.get(4) and (sum(level_cp[4]) / len(level_cp[4])) < 0.5:
            print("\n  ⚠  FLAG L4: loop/$ compile rate < 50% — int64 $ capture likely wrong")
        if level_cp.get(5) and (sum(level_cp[5]) / len(level_cp[5])) < 0.5:
            print("  ⚠  FLAG L5: pointer/cast compile rate < 50% — => lambda bias detectable")
        print()

        if output_file:
            with open(output_file, "w") as fout:
                json.dump({
                    "model": self.model_path,
                    "base_model": self.base_model,
                    "ariac": str(ARIAC_BIN) if ARIAC_BIN else None,
                    "overall_keyword": round(overall_kw, 4),
                    "overall_compile": round(overall_cp, 4),
                    "overall_combined": round(overall_co, 4),
                    "verdict": verdict,
                    "by_level": {
                        str(lv): {
                            "keyword": round(sum(level_kw[lv]) / len(level_kw[lv]), 4),
                            "compile": round(sum(level_cp[lv]) / len(level_cp[lv]), 4),
                        }
                        for lv in sorted(level_kw)
                    },
                    "results": results,
                }, fout, indent=2)
            print(f"  Results → {output_file}")

        return results


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Compile-graded evaluation of the Aria specialist model.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--model", default="./tools/aria_specialist_model",
        metavar="PATH",
        help="Path to the LoRA adapter directory (default: ./tools/aria_specialist_model)",
    )
    parser.add_argument(
        "--checkpoint", default=None,
        metavar="NAME",
        help=(
            "Checkpoint subdirectory name, e.g. 'checkpoint-378'. "
            "Resolved relative to --model. Overrides --model when given."
        ),
    )
    parser.add_argument(
        "--base-model", default=None,
        metavar="HF_ID",
        help="Override base model HF hub ID (reads adapter_config.json by default)",
    )
    parser.add_argument(
        "--levels", default="1,2,3,4,5,6",
        metavar="LEVELS",
        help="Comma-separated levels to run, e.g. '1,2,3' (default: all)",
    )
    parser.add_argument(
        "--output", default=None,
        metavar="FILE",
        help="Write JSON results to FILE (default: tools/eval_compile_<checkpoint>.json)",
    )
    args = parser.parse_args()

    # Resolve model path
    model_path = args.model
    if args.checkpoint:
        model_path = str(Path(args.model) / args.checkpoint)

    if not Path(model_path).exists():
        print(f"Error: model path not found: {model_path}", file=sys.stderr)
        sys.exit(1)

    # Default output path
    if args.output is None:
        tag = args.checkpoint or Path(model_path).name
        args.output = str(REPO_ROOT / "tools" / f"eval_compile_{tag}.json")

    levels = set(int(x.strip()) for x in args.levels.split(",") if x.strip())
    cases = [c for c in EVAL_CASES if c.level in levels]
    if not cases:
        print(f"No eval cases for levels {levels}", file=sys.stderr)
        sys.exit(1)

    print(f"ariac binary    : {ARIAC_BIN or '(not found — compile scores will be 0)'}")
    print(f"eval cases      : {len(cases)} (levels {sorted(levels)})\n")

    evaluator = AriaEvaluator(model_path, args.base_model)
    evaluator.load()
    evaluator.run(cases, output_file=args.output)


if __name__ == "__main__":
    main()
