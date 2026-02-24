#!/usr/bin/env python3
"""
Aria Specialist — CLI inference tool
Uses checkpoint-200 (best eval_loss 0.486, NOT the final overfit checkpoint-309)

Usage:
    python3 aria_specialist_infer.py "How do I write a function that returns a Result?"
    echo "How do I declare a struct?" | python3 aria_specialist_infer.py
    python3 aria_specialist_infer.py  # interactive REPL mode

The model is Mistral-7B-v0.3 + LoRA (r=16, middle layers 14-18), trained on
1,640 Aria language examples (spec sections, stdlib, tests, examples).
English is fully preserved from the base model.
"""

import sys
import os
import argparse

# Model paths
TOOLS_DIR  = os.path.dirname(os.path.abspath(__file__))
BEST_CKPT  = os.path.join(TOOLS_DIR, "aria_specialist_model", "checkpoint-200")
BASE_MODEL = "mistralai/Mistral-7B-v0.3"

# Alpaca-style prompt — must match training format exactly
def build_prompt(instruction: str, context: str = "") -> str:
    if context.strip():
        return (
            "Below is an instruction that describes a task, paired with an input "
            "that provides further context. Write a response that appropriately "
            "completes the request.\n\n"
            f"### Instruction:\n{instruction}\n\n"
            f"### Input:\n{context}\n\n"
            "### Response:"
        )
    return (
        "Below is an instruction that describes a task. Write a response that "
        "appropriately completes the request.\n\n"
        f"### Instruction:\n{instruction}\n\n"
        "### Response:"
    )


def load_model(checkpoint: str = BEST_CKPT, device: str = "auto"):
    """Load model once; returns (model, tokenizer)."""
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import PeftModel

    print(f"[aria-specialist] Loading base model: {BASE_MODEL}", file=sys.stderr)
    tokenizer = AutoTokenizer.from_pretrained(
        checkpoint,
        trust_remote_code=True,
    )
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    dtype = torch.float16 if torch.cuda.is_available() else torch.float32
    base = AutoModelForCausalLM.from_pretrained(
        BASE_MODEL,
        torch_dtype=dtype,
        device_map=device,
        trust_remote_code=True,
    )

    print(f"[aria-specialist] Loading LoRA adapter: {checkpoint}", file=sys.stderr)
    model = PeftModel.from_pretrained(base, checkpoint)
    model.eval()

    print("[aria-specialist] Ready.", file=sys.stderr)
    return model, tokenizer


def generate(
    model,
    tokenizer,
    instruction: str,
    context: str = "",
    max_new_tokens: int = 512,
    temperature: float = 0.2,
    top_p: float = 0.9,
    repetition_penalty: float = 1.1,
) -> str:
    import torch

    prompt = build_prompt(instruction, context)
    inputs = tokenizer(prompt, return_tensors="pt").to(model.device)

    with torch.no_grad():
        output_ids = model.generate(
            **inputs,
            max_new_tokens=max_new_tokens,
            temperature=temperature,
            top_p=top_p,
            repetition_penalty=repetition_penalty,
            do_sample=(temperature > 0),
            pad_token_id=tokenizer.pad_token_id,
            eos_token_id=tokenizer.eos_token_id,
        )

    # Decode only newly generated tokens (strip the prompt)
    new_ids = output_ids[0][inputs["input_ids"].shape[1]:]
    response = tokenizer.decode(new_ids, skip_special_tokens=True)

    # Trim at any secondary ### marker (model sometimes self-prompts)
    for stop in ["\n### Instruction:", "\n### Input:", "\n### Human:"]:
        if stop in response:
            response = response[: response.index(stop)]

    return response.strip()


def interactive_repl(model, tokenizer):
    print("Aria Specialist — interactive mode (Ctrl-C or 'exit' to quit)")
    print("Ask anything about the Aria programming language.\n")
    while True:
        try:
            instruction = input("aria> ").strip()
        except (EOFError, KeyboardInterrupt):
            print()
            break
        if not instruction or instruction.lower() in ("exit", "quit", "q"):
            break
        response = generate(model, tokenizer, instruction)
        print("\n" + response + "\n")


def main():
    parser = argparse.ArgumentParser(
        description="Aria language specialist — inference CLI"
    )
    parser.add_argument(
        "instruction",
        nargs="?",
        help="Question or instruction (omit for REPL or pipe from stdin)",
    )
    parser.add_argument(
        "--context", "-c",
        default="",
        help="Optional code snippet / context for the instruction",
    )
    parser.add_argument(
        "--checkpoint",
        default=BEST_CKPT,
        help=f"LoRA checkpoint directory (default: checkpoint-200)",
    )
    parser.add_argument(
        "--max-tokens", "-n",
        type=int,
        default=512,
    )
    parser.add_argument(
        "--temperature", "-t",
        type=float,
        default=0.2,
        help="Sampling temperature (0 = greedy)",
    )
    parser.add_argument(
        "--repl", "-i",
        action="store_true",
        help="Force interactive REPL mode",
    )
    args = parser.parse_args()

    model, tokenizer = load_model(args.checkpoint)

    # --- Determine mode ---
    if args.repl or (args.instruction is None and sys.stdin.isatty()):
        interactive_repl(model, tokenizer)
        return

    # Single query from arg or piped stdin
    if args.instruction:
        instruction = args.instruction
    else:
        instruction = sys.stdin.read().strip()

    if not instruction:
        parser.print_help()
        sys.exit(1)

    response = generate(
        model,
        tokenizer,
        instruction,
        context=args.context,
        max_new_tokens=args.max_tokens,
        temperature=args.temperature,
    )
    print(response)


if __name__ == "__main__":
    main()
