#!/usr/bin/env python3
"""
Aria Language Specialist Model - Evaluation Script

Tests generated Aria code across 6 complexity levels to assess how well
the fine-tune overcame the base model's biases and learned Aria syntax.

Levels:
  1 - Basic syntax (variables, simple functions)
  2 - Structs and enums
  3 - Error handling (Result<T>, pass/fail, ? operator)
  4 - Loops with $ (the int64 capture test)
  5 - Pointers and cast operator
  6 - Complex multi-function programs

Usage:
    # Evaluate the newly trained model (all levels):
    python eval_aria_specialist.py --model tools/aria_specialist_model

    # Quick sanity check (levels 1-3 only):
    python eval_aria_specialist.py --model tools/aria_specialist_model --levels 1,2,3

    # Compare two checkpoints:
    python eval_aria_specialist.py --model tools/aria_specialist_model --output results_new.json
    python eval_aria_specialist.py --model tools/aria_specialist_model_prev --output results_prev.json

    # Run a single freeform prompt interactively:
    python eval_aria_specialist.py --model tools/aria_specialist_model --interactive
"""

import torch
import argparse
import json
import sys
from pathlib import Path
from dataclasses import dataclass, field
from typing import List, Optional
from transformers import AutoModelForCausalLM, AutoTokenizer, BitsAndBytesConfig
from peft import PeftModel


# ---------------------------------------------------------------------------
# Test cases
# ---------------------------------------------------------------------------

@dataclass
class EvalCase:
    level: int
    category: str
    instruction: str
    check_contains: List[str]          # must appear in correct output
    check_not_contains: List[str]      # common wrong patterns from base model bias
    note: str = ""
    max_new_tokens: int = 350


EVAL_CASES: List[EvalCase] = [

    # ── Level 1: Basic syntax ───────────────────────────────────────────────

    EvalCase(
        level=1,
        category="variable declaration",
        instruction="Declare an int32 variable named 'count' initialized to zero in Aria.",
        check_contains=["int32:count"],
        check_not_contains=["let ", "var ", "int count", "count: int", "count: i32"],
        note="Type-prefix style: int32:name = value"
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
        note="func:name = ReturnType(params) { pass(value); }"
    ),
    EvalCase(
        level=1,
        category="string output",
        instruction='Write an Aria program that prints "Hello, Aria!" to stdout.',
        check_contains=["stdout", "Hello, Aria"],
        check_not_contains=["print(", "println!(", "printf(", "console.log"],
        note="Aria uses stdout << value"
    ),

    # ── Level 2: Structs and enums ──────────────────────────────────────────

    EvalCase(
        level=2,
        category="struct definition",
        instruction=(
            "Define a struct in Aria called 'Point' with two int32 fields: x and y. "
            "Then write a function that creates and returns a Point."
        ),
        check_contains=["struct:Point", "int32:x", "int32:y", "func:"],
        check_not_contains=["struct Point {", "class Point", "struct Point;"],
        note="struct:Name = { type:field; } syntax"
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
        note="enum:Name = { VALUE = n, } syntax"
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
        note="Integration of struct + function"
    ),

    # ── Level 3: Error handling ─────────────────────────────────────────────

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
        note="Result<T> with explicit pass/fail — no exceptions"
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
        note="? is the unwrap-or-default operator"
    ),
    EvalCase(
        level=3,
        category="is_error check",
        instruction=(
            "Write Aria code that calls a function returning Result<string>, "
            "checks if it succeeded using .is_error, and prints an appropriate message."
        ),
        check_contains=["Result<string>", ".is_error", "stdout"],
        check_not_contains=["try {", "catch", ".is_ok()", "if err !="],
        note="Aria uses .is_error field, not .is_ok() method"
    ),

    # ── Level 4: Loops with $ ───────────────────────────────────────────────

    EvalCase(
        level=4,
        category="till loop in-place mutation",
        instruction=(
            "Write an Aria till loop that iterates over an int32 array called "
            "'values' and doubles each element in place."
        ),
        check_contains=["till(", "values[$]", "$"],
        check_not_contains=["for i in", "for (int", "for i =", "values[i]"],
        note="till(length-1, 1) { arr[$] = ... } — $ is the index"
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
        note="CRITICAL: $ must be captured as int64, not int32 — any int32 capture is wrong"
    ),
    EvalCase(
        level=4,
        category="loop with array indexing",
        instruction=(
            "Write an Aria program that creates an int64 array of 10 elements, "
            "fills it with values 0 through 9 using a loop, and then prints each value."
        ),
        check_contains=["int64", "loop(", "stdout"],
        check_not_contains=["int32:i = $", "int[10]", "new int"],
        note="Tests array declaration + loop + $ in combination"
    ),

    # ── Level 5: Pointers and cast ──────────────────────────────────────────

    EvalCase(
        level=5,
        category="pointer basics",
        instruction=(
            "In Aria, declare an int32 variable called 'value' set to 42. "
            "Take its address into a pointer, then dereference the pointer to read the value."
        ),
        check_contains=["int32->", "@", "<-"],
        check_not_contains=["&value", "*ptr", "int32*", "int32 *", "ref "],
        note="@ = address-of, <- = dereference, int32-> = pointer type"
    ),
    EvalCase(
        level=5,
        category="cast operator =>",
        instruction=(
            "Show how to cast an int64 value to int8 in Aria using the cast operator."
        ),
        check_contains=["=> int8", "int64"],
        check_not_contains=["as int8", "(int8)", "cast(", "=> {", "=> ("],
        note="CRITICAL: => is the CAST operator. Must NOT produce lambda syntax => { }"
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
        note="=> binds tighter than +: 'a + b => int8' casts b, not the sum"
    ),

    # ── Level 6: Complex programs ───────────────────────────────────────────

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
        note="Full integration: struct + functions + Result<T> error handling",
        max_new_tokens=500
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
        max_new_tokens=400
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


def score_output(raw: str, case: EvalCase) -> dict:
    # Extract just the response part
    if "### Response:" in raw:
        response = raw.split("### Response:", 1)[-1].strip()
    else:
        response = raw.strip()

    hits = [s for s in case.check_contains if s in response]
    misses = [s for s in case.check_contains if s not in response]
    bad_hits = [s for s in case.check_not_contains if s in response]

    precision = len(hits) / len(case.check_contains) if case.check_contains else 1.0
    penalty = 0.5 * (len(bad_hits) / max(len(case.check_not_contains), 1))
    final = round(max(0.0, precision - penalty), 2)

    return {
        "score": final,
        "hits": hits,
        "misses": misses,
        "bad_patterns": bad_hits,
        "response": response,
    }


def render_bar(score: float, width: int = 10) -> str:
    filled = int(score * width)
    return "█" * filled + "░" * (width - filled)


LEVEL_LABELS = {
    1: "Basic syntax",
    2: "Structs/enums",
    3: "Error handling",
    4: "Loops / $",
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
        print(f"Loading adapter: {self.model_path}")

        # Read base model from adapter_config.json unless overridden
        config_path = Path(self.model_path) / "adapter_config.json"
        if config_path.exists() and not self.base_model:
            with open(config_path) as f:
                cfg = json.load(f)
            self.base_model = cfg.get("base_model_name_or_path", "mistralai/Mistral-7B-v0.3")
        elif not self.base_model:
            self.base_model = "mistralai/Mistral-7B-v0.3"

        print(f"Base model: {self.base_model}")
        print("Loading in 4-bit NF4...")

        bnb = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.float16,
            bnb_4bit_use_double_quant=True,
        )

        base = AutoModelForCausalLM.from_pretrained(
            self.base_model,
            quantization_config=bnb,
            device_map="auto",
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
                temperature=0.2,       # low for deterministic, syntactic output
                top_p=0.9,
                do_sample=True,
                pad_token_id=self.tokenizer.eos_token_id,
                repetition_penalty=1.1,
            )
        return self.tokenizer.decode(out[0], skip_special_tokens=True)

    def interactive(self):
        """REPL for freeform prompts."""
        print("Interactive mode. Type 'quit' to exit.\n")
        while True:
            try:
                instruction = input("Instruction> ").strip()
            except (EOFError, KeyboardInterrupt):
                break
            if instruction.lower() in ("quit", "exit", "q"):
                break
            if not instruction:
                continue
            raw = self.generate(build_prompt(instruction), max_new_tokens=500)
            response = raw.split("### Response:", 1)[-1].strip() if "### Response:" in raw else raw
            print("\n--- Response ---")
            print(response)
            print("--- End ---\n")

    def run(self, cases: List[EvalCase], output_file: Optional[str] = None) -> list:
        results = []
        level_scores: dict = {}

        print("=" * 70)
        print("  ARIA SPECIALIST MODEL EVALUATION")
        print("=" * 70)

        for i, case in enumerate(cases, 1):
            print(f"\n[{i}/{len(cases)}] Level {case.level} — {case.category.upper()}")
            print(f"  {case.instruction[:100]}{'...' if len(case.instruction) > 100 else ''}")
            if case.note:
                print(f"  ⚑  {case.note}")

            prompt = build_prompt(case.instruction)
            raw = self.generate(prompt, max_new_tokens=case.max_new_tokens)
            scored = score_output(raw, case)

            bar = render_bar(scored["score"])
            pct = f"{scored['score']:.0%}"
            print(f"\n  Score: [{bar}] {pct}")
            if scored["hits"]:
                print(f"  ✅ Found    : {scored['hits']}")
            if scored["misses"]:
                print(f"  ❌ Missing  : {scored['misses']}")
            if scored["bad_patterns"]:
                print(f"  ⚠️  Wrong    : {scored['bad_patterns']}")

            print("\n  ┌─ Generated output " + "─" * 49)
            for line in scored["response"].splitlines()[:30]:  # cap display at 30 lines
                print(f"  │ {line}")
            if scored["response"].count("\n") > 30:
                print("  │ ... (truncated)")
            print("  └" + "─" * 69)

            level_scores.setdefault(case.level, []).append(scored["score"])
            results.append({
                "level": case.level,
                "category": case.category,
                "instruction": case.instruction,
                "note": case.note,
                "score": scored["score"],
                "hits": scored["hits"],
                "misses": scored["misses"],
                "bad_patterns": scored["bad_patterns"],
                "response": scored["response"],
            })

        # ── Summary ──────────────────────────────────────────────────────────
        print("\n" + "=" * 70)
        print("  SUMMARY")
        print("=" * 70)

        all_scores = []
        for level in sorted(level_scores):
            scores = level_scores[level]
            avg = sum(scores) / len(scores)
            bar = render_bar(avg)
            label = LEVEL_LABELS.get(level, f"Level {level}")
            print(f"  L{level} {label:<18s}: [{bar}] {avg:.0%}  ({len(scores)} test{'s' if len(scores) > 1 else ''})")
            all_scores.extend(scores)

        overall = sum(all_scores) / len(all_scores) if all_scores else 0.0
        print(f"\n  Overall: [{render_bar(overall)}] {overall:.0%}")

        if overall >= 0.80:
            verdict = "Strong — base bias well overcome; good candidate for Nikola-scale training"
        elif overall >= 0.65:
            verdict = "Good — most syntax learned; pointer/cast area needs more examples or higher LoRA rank"
        elif overall >= 0.50:
            verdict = "Moderate — basic patterns ok; complex syntax needs larger base model or more data"
        elif overall >= 0.35:
            verdict = "Weak — syntax partially learned; significant base model bias remaining"
        else:
            verdict = "Poor — fine-tune did not overcome base model; try larger model or dataset quality improvements"

        print(f"  Verdict: {verdict}")
        print("=" * 70)

        # Critical failure flags
        l4_scores = level_scores.get(4, [])
        l5_scores = level_scores.get(5, [])
        if l4_scores and (sum(l4_scores) / len(l4_scores)) < 0.5:
            print("\n  ⚠️  FLAG: Level 4 (loops/$) is below 50%.")
            print("     The int64/int32 $ capture issue is likely still present.")
            print("     Add more loop examples to training data with explicit int64 captures.")
        if l5_scores and (sum(l5_scores) / len(l5_scores)) < 0.5:
            print("\n  ⚠️  FLAG: Level 5 (pointers/cast) is below 50%.")
            print("     Base model's => lambda bias likely not overcome.")
            print("     Consider: higher lora_r, more cast examples, or larger base model.")

        if output_file:
            with open(output_file, "w") as f:
                json.dump({
                    "model": self.model_path,
                    "base_model": self.base_model,
                    "overall": round(overall, 4),
                    "verdict": verdict,
                    "by_level": {
                        str(lv): round(sum(sc) / len(sc), 4)
                        for lv, sc in level_scores.items()
                    },
                    "results": results,
                }, f, indent=2)
            print(f"\n  Detailed results → {output_file}")

        return results


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Evaluate the Aria language specialist LoRA model.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--model", default="./tools/aria_specialist_model",
        help="Path to the LoRA adapter directory (default: ./tools/aria_specialist_model)"
    )
    parser.add_argument(
        "--base-model", default=None,
        help="Override base model HF hub ID (reads from adapter_config.json by default)"
    )
    parser.add_argument(
        "--levels", default="1,2,3,4,5,6",
        help="Comma-separated list of levels to run, e.g. '1,2,3' (default: all)"
    )
    parser.add_argument(
        "--output", default="tools/eval_results.json",
        help="Path to write JSON results (default: tools/eval_results.json)"
    )
    parser.add_argument(
        "--interactive", action="store_true",
        help="After eval (or instead if --levels is empty), open an interactive prompt REPL"
    )
    parser.add_argument(
        "--no-eval", action="store_true",
        help="Skip the eval suite; only useful with --interactive"
    )
    args = parser.parse_args()

    levels = set(int(x.strip()) for x in args.levels.split(",")) if args.levels else set()
    cases = [c for c in EVAL_CASES if c.level in levels]

    evaluator = AriaEvaluator(args.model, args.base_model)
    evaluator.load()

    if not args.no_eval and cases:
        evaluator.run(cases, output_file=args.output)

    if args.interactive:
        evaluator.interactive()


if __name__ == "__main__":
    main()
