#!/usr/bin/env python3
"""
Aria Specialist — JSON-over-stdio daemon

Protocol: newline-delimited JSON (JSON Lines) on stdin/stdout.

REQUEST  → {"id": 1, "instruction": "...", "context": "..."}
RESPONSE ← {"id": 1, "response": "...", "ok": true}
ERROR    ← {"id": 1, "error": "...", "ok": false}

The daemon loads the model once on startup and then serves queries in a loop.
This fits naturally into Aria's 6-stream I/O model:
  - stddati (FD 4) → requests in
  - stddato (FD 5) → responses out
  (or just use regular stdin/stdout for subprocess integration)

Usage:
    # Start the daemon (stays alive):
    python3 aria_specialist_server.py

    # Pipe a single query:
    echo '{"id":1,"instruction":"How do I declare a mutable variable?"}' \
        | python3 aria_specialist_server.py

    # As a subprocess from Aria shell or any tool:
    proc = subprocess.Popen(["python3", "aria_specialist_server.py"],
                             stdin=PIPE, stdout=PIPE)

PID file: /tmp/aria_specialist.pid  (written on startup)
"""

import sys
import os
import json
import signal

TOOLS_DIR  = os.path.dirname(os.path.abspath(__file__))
BEST_CKPT  = os.path.join(TOOLS_DIR, "aria_specialist_model", "checkpoint-200")
PID_FILE   = "/tmp/aria_specialist.pid"
READY_MSG  = '{"ready":true,"checkpoint":"checkpoint-200"}'


def _log(msg: str):
    """Write diagnostic to stderr (goes to stddbg / FD 3 in 6-stream mode)."""
    print(f"[aria-specialist] {msg}", file=sys.stderr, flush=True)


def load_model(checkpoint: str = BEST_CKPT):
    import torch
    from transformers import AutoModelForCausalLM, AutoTokenizer
    from peft import PeftModel

    _log(f"Loading base model mistralai/Mistral-7B-v0.3 ...")
    tokenizer = AutoTokenizer.from_pretrained(checkpoint, trust_remote_code=True)
    if tokenizer.pad_token is None:
        tokenizer.pad_token = tokenizer.eos_token

    dtype = torch.float16 if torch.cuda.is_available() else torch.float32
    base = AutoModelForCausalLM.from_pretrained(
        "mistralai/Mistral-7B-v0.3",
        torch_dtype=dtype,
        device_map="auto",
        trust_remote_code=True,
    )
    _log(f"Loading LoRA adapter from {checkpoint} ...")
    model = PeftModel.from_pretrained(base, checkpoint)
    model.eval()
    _log("Model ready.")
    return model, tokenizer


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


def generate(model, tokenizer, instruction: str, context: str = "",
             max_new_tokens: int = 512, temperature: float = 0.2) -> str:
    import torch

    prompt = build_prompt(instruction, context)
    inputs = tokenizer(prompt, return_tensors="pt").to(model.device)

    with torch.no_grad():
        out_ids = model.generate(
            **inputs,
            max_new_tokens=max_new_tokens,
            temperature=temperature,
            top_p=0.9,
            repetition_penalty=1.1,
            do_sample=(temperature > 0),
            pad_token_id=tokenizer.pad_token_id,
            eos_token_id=tokenizer.eos_token_id,
        )

    new_ids = out_ids[0][inputs["input_ids"].shape[1]:]
    response = tokenizer.decode(new_ids, skip_special_tokens=True)
    for stop in ["\n### Instruction:", "\n### Input:", "\n### Human:"]:
        if stop in response:
            response = response[: response.index(stop)]
    return response.strip()


def handle_request(model, tokenizer, req: dict) -> dict:
    req_id = req.get("id", None)
    try:
        instruction = req.get("instruction", "").strip()
        if not instruction:
            return {"id": req_id, "ok": False, "error": "empty instruction"}
        context      = req.get("context", "")
        max_tokens   = int(req.get("max_tokens", 512))
        temperature  = float(req.get("temperature", 0.2))
        response = generate(model, tokenizer, instruction, context,
                             max_tokens, temperature)
        return {"id": req_id, "ok": True, "response": response}
    except Exception as exc:
        return {"id": req_id, "ok": False, "error": str(exc)}


def write_pid():
    with open(PID_FILE, "w") as f:
        f.write(str(os.getpid()))


def cleanup(*_):
    try:
        os.remove(PID_FILE)
    except FileNotFoundError:
        pass
    sys.exit(0)


def serve():
    write_pid()
    signal.signal(signal.SIGTERM, cleanup)
    signal.signal(signal.SIGINT,  cleanup)

    model, tokenizer = load_model()

    # Signal readiness to the parent process (JSON on stdout)
    print(READY_MSG, flush=True)

    for raw_line in sys.stdin:
        raw_line = raw_line.strip()
        if not raw_line:
            continue
        try:
            req = json.loads(raw_line)
        except json.JSONDecodeError as exc:
            print(json.dumps({"ok": False, "error": f"JSON parse error: {exc}"}),
                  flush=True)
            continue

        resp = handle_request(model, tokenizer, req)
        print(json.dumps(resp), flush=True)

    cleanup()


if __name__ == "__main__":
    serve()
