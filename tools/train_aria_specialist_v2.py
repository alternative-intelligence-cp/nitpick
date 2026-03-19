#!/usr/bin/env python3
"""
Aria Language Specialist Model - Training Pipeline v2
Supports multiple base models with automatic VRAM-aware configuration.

Recommended models (all Apache 2.0 or MIT, no commercial restrictions):

  qwen/qwen2.5-coder-14b         Apache 2.0  ~14GB VRAM (4-bit)  RECOMMENDED
  qwen/qwen2.5-coder-32b         Apache 2.0  ~20GB VRAM (4-bit)  if 14B scores well
  microsoft/phi-4                MIT         ~14GB VRAM (4-bit)  alternative, MIT license
  mistralai/Mistral-Nemo-Base-2407  Apache 2.0  ~12GB VRAM (4-bit)
  mistralai/Mistral-7B-v0.3      Apache 2.0  ~5GB  VRAM (4-bit)  baseline (v1)

Usage:
    # Recommended first upgrade:
    python train_aria_specialist_v2.py \\
        --model Qwen/Qwen2.5-Coder-14B \\
        --data tools/aria_training_corpus_train.jsonl \\
        --eval-data tools/aria_training_corpus_val.jsonl \\
        --output tools/aria_specialist_qwen14b

    # Larger run (allow more VRAM, reduce batch if OOM):
    python train_aria_specialist_v2.py \\
        --model Qwen/Qwen2.5-Coder-32B \\
        --data tools/aria_training_corpus_train.jsonl \\
        --eval-data tools/aria_training_corpus_val.jsonl \\
        --output tools/aria_specialist_qwen32b \\
        --lora-rank 32 --batch-size 2

    # MIT-licensed alternative:
    python train_aria_specialist_v2.py \\
        --model microsoft/phi-4 \\
        --data tools/aria_training_corpus_train.jsonl \\
        --eval-data tools/aria_training_corpus_val.jsonl \\
        --output tools/aria_specialist_phi4
"""

import os
import argparse
import torch
import json
from dataclasses import dataclass
from typing import Optional, List
from pathlib import Path

from transformers import (
    AutoModelForCausalLM,
    AutoTokenizer,
    TrainingArguments,
    Trainer,
    DataCollatorForLanguageModeling,
    BitsAndBytesConfig,
)
from datasets import load_dataset
from peft import (
    LoraConfig,
    PeftModel,
    get_peft_model,
    TaskType,
    prepare_model_for_kbit_training,
)


# ---------------------------------------------------------------------------
# Known model profiles
# ---------------------------------------------------------------------------

MODEL_PROFILES = {
    # id (lowercased, partial match) → (lora_r, grad_accum, batch_size, max_len, target_modules)
    "qwen2.5-coder-32b": dict(lora_r=32, grad_accum=8, batch_size=1, max_len=2048),
    "qwen2.5-coder-14b": dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
    "qwen2.5-coder-7b":  dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
    "qwen2.5-14b":       dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
    "phi-4":             dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
    "phi-3":             dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
    "mistral-nemo":      dict(lora_r=16, grad_accum=4, batch_size=4, max_len=4096),
    "mistral-7b":        dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048),
}

# LoRA target modules by architecture family
LORA_TARGETS = {
    "qwen":    ["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"],
    "phi":     ["q_proj", "k_proj", "v_proj", "dense", "fc1", "fc2"],
    "mistral": ["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"],
    "default": ["q_proj", "k_proj", "v_proj", "o_proj", "gate_proj", "up_proj", "down_proj"],
}


def get_lora_targets(model_name: str) -> List[str]:
    name = model_name.lower()
    for family, targets in LORA_TARGETS.items():
        if family in name:
            return targets
    return LORA_TARGETS["default"]


def get_model_profile(model_name: str) -> dict:
    name = model_name.lower()
    for key, profile in MODEL_PROFILES.items():
        if key in name:
            return profile
    # Fallback defaults
    return dict(lora_r=16, grad_accum=4, batch_size=4, max_len=2048)


# ---------------------------------------------------------------------------
# Trainer
# ---------------------------------------------------------------------------

class AriaSpecialistTrainerV2:
    def __init__(
        self,
        model_name: str,
        output_dir: str,
        lora_r: Optional[int] = None,
        lora_alpha: Optional[int] = None,
        lora_dropout: float = 0.05,
        num_epochs: int = 3,
        batch_size: Optional[int] = None,
        grad_accum: Optional[int] = None,
        max_length: Optional[int] = None,
        learning_rate: float = 2e-4,
        resume_from: Optional[str] = None,
    ):
        self.model_name = model_name
        self.output_dir = output_dir
        self.lora_dropout = lora_dropout
        self.num_epochs = num_epochs
        self.learning_rate = learning_rate

        profile = get_model_profile(model_name)
        self.lora_r = lora_r or profile["lora_r"]
        self.lora_alpha = lora_alpha or (self.lora_r * 2)
        self.batch_size = batch_size or profile["batch_size"]
        self.grad_accum = grad_accum or profile["grad_accum"]
        self.max_length = max_length or profile["max_len"]
        self.lora_targets = get_lora_targets(model_name)

        self.resume_from = resume_from
        self.model = None
        self.tokenizer = None
        self.train_dataset = None
        self.eval_dataset = None

        print(f"\n{'='*60}")
        print(f"  Aria Specialist Trainer v2")
        print(f"{'='*60}")
        print(f"  Base model  : {model_name}")
        print(f"  Output      : {output_dir}")
        print(f"  LoRA rank   : {self.lora_r}  alpha: {self.lora_alpha}")
        print(f"  Batch size  : {self.batch_size}  grad_accum: {self.grad_accum}")
        print(f"  Effective batch: {self.batch_size * self.grad_accum}")
        print(f"  Max length  : {self.max_length}")
        print(f"  Epochs      : {num_epochs}")
        print(f"  LR          : {learning_rate}")
        print(f"  LoRA targets: {self.lora_targets}")
        print(f"{'='*60}\n")

    def setup_model(self):
        print(f"Loading tokenizer...")
        self.tokenizer = AutoTokenizer.from_pretrained(
            self.model_name, trust_remote_code=True
        )
        if self.tokenizer.pad_token is None:
            self.tokenizer.pad_token = self.tokenizer.eos_token
        self.tokenizer.padding_side = "right"

        print(f"Loading model in 4-bit NF4 quantization...")
        bnb = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",
            bnb_4bit_compute_dtype=torch.float16,
            bnb_4bit_use_double_quant=True,
        )

        self.model = AutoModelForCausalLM.from_pretrained(
            self.model_name,
            quantization_config=bnb,
            device_map="auto",
            trust_remote_code=True,
        )

        self.model = prepare_model_for_kbit_training(self.model)

        if self.resume_from:
            print(f"Loading existing LoRA adapter from {self.resume_from} for continued training...")
            self.model = PeftModel.from_pretrained(
                self.model, self.resume_from, is_trainable=True
            )
        else:
            print(f"Configuring LoRA (r={self.lora_r}, alpha={self.lora_alpha})...")
            lora_cfg = LoraConfig(
                task_type=TaskType.CAUSAL_LM,
                r=self.lora_r,
                lora_alpha=self.lora_alpha,
                lora_dropout=self.lora_dropout,
                target_modules=self.lora_targets,
                bias="none",
                inference_mode=False,
            )
            self.model = get_peft_model(self.model, lora_cfg)
        self.model.print_trainable_parameters()
        print(f"✓ Model ready\n")

    def prepare_datasets(self, train_path: str, eval_path: Optional[str] = None):
        print(f"Loading datasets...")
        data_files = {"train": train_path}
        data_files["validation"] = eval_path if eval_path else train_path

        dataset = load_dataset("json", data_files=data_files)

        def format_example(ex):
            instruction = ex["instruction"]
            inp = ex.get("input", "")
            output = ex["output"]
            if inp:
                text = (
                    "Below is an instruction that describes a task, paired with an input "
                    "that provides further context. Write a response that appropriately "
                    "completes the request.\n\n"
                    f"### Instruction:\n{instruction}\n\n"
                    f"### Input:\n{inp}\n\n"
                    f"### Response:\n{output}"
                )
            else:
                text = (
                    "Below is an instruction that describes a task. Write a response "
                    "that appropriately completes the request.\n\n"
                    f"### Instruction:\n{instruction}\n\n"
                    f"### Response:\n{output}"
                )
            return {"text": text}

        fmt_train = dataset["train"].map(format_example, remove_columns=dataset["train"].column_names)
        fmt_eval  = dataset["validation"].map(format_example, remove_columns=dataset["validation"].column_names)

        def tokenize(examples):
            tok = self.tokenizer(
                examples["text"],
                truncation=True,
                max_length=self.max_length,
                padding="max_length",
                return_tensors=None,
            )
            tok["labels"] = tok["input_ids"].copy()
            return tok

        print("Tokenizing...")
        self.train_dataset = fmt_train.map(
            tokenize, batched=True, remove_columns=fmt_train.column_names,
            desc="Train"
        )
        self.eval_dataset = fmt_eval.map(
            tokenize, batched=True, remove_columns=fmt_eval.column_names,
            desc="Eval"
        )
        print(f"✓ Train: {len(self.train_dataset)}  Eval: {len(self.eval_dataset)}\n")

    def train(self):
        os.makedirs(self.output_dir, exist_ok=True)

        # Detect bf16 support (A100/H100 etc) vs fp16 (3090)
        use_bf16 = torch.cuda.is_bf16_supported()
        use_fp16 = not use_bf16
        print(f"Mixed precision: {'bf16' if use_bf16 else 'fp16'}")

        training_args = TrainingArguments(
            output_dir=self.output_dir,
            num_train_epochs=self.num_epochs,
            per_device_train_batch_size=self.batch_size,
            per_device_eval_batch_size=self.batch_size,
            gradient_accumulation_steps=self.grad_accum,
            learning_rate=self.learning_rate,
            fp16=use_fp16,
            bf16=use_bf16,
            save_steps=100,
            eval_steps=100,
            logging_steps=10,
            warmup_steps=50,
            eval_strategy="steps",
            save_strategy="steps",
            load_best_model_at_end=True,
            metric_for_best_model="eval_loss",
            greater_is_better=False,
            report_to="none",
            gradient_checkpointing=True,
            optim="adamw_torch",
            lr_scheduler_type="cosine",
            dataloader_num_workers=2,
        )

        collator = DataCollatorForLanguageModeling(
            tokenizer=self.tokenizer, mlm=False
        )

        trainer = Trainer(
            model=self.model,
            args=training_args,
            train_dataset=self.train_dataset,
            eval_dataset=self.eval_dataset,
            data_collator=collator,
        )

        print("\n" + "="*60)
        print("  Starting training")
        print("="*60 + "\n")

        trainer.train()

        print("\nSaving final model...")
        trainer.save_model(self.output_dir)
        self.tokenizer.save_pretrained(self.output_dir)

        # Save a human-readable training summary
        summary = {
            "base_model": self.model_name,
            "lora_r": self.lora_r,
            "lora_alpha": self.lora_alpha,
            "num_epochs": self.num_epochs,
            "max_length": self.max_length,
            "effective_batch_size": self.batch_size * self.grad_accum,
            "learning_rate": self.learning_rate,
        }
        with open(Path(self.output_dir) / "training_summary.json", "w") as f:
            json.dump(summary, f, indent=2)

        print(f"\n✓ Training complete! Model saved to {self.output_dir}")
        print(f"\nTo evaluate:")
        print(f"  python tools/eval_aria_specialist.py --model {self.output_dir}")


def main():
    parser = argparse.ArgumentParser(
        description="Train Aria language specialist (v2 — multi-model support)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument(
        "--model",
        default="Qwen/Qwen2.5-Coder-14B",
        help="HuggingFace model ID or local path (default: Qwen/Qwen2.5-Coder-14B)"
    )
    parser.add_argument("--data",    required=True, help="Training JSONL file")
    parser.add_argument("--eval-data", help="Eval JSONL file (optional)")
    parser.add_argument("--output",  default="./aria_specialist_model", help="Output directory")
    parser.add_argument("--epochs",  type=int,   default=3)
    parser.add_argument("--batch-size", type=int, default=None, help="Override batch size")
    parser.add_argument("--lora-rank",  type=int, default=None, help="Override LoRA rank")
    parser.add_argument("--lr",      type=float, default=2e-4)
    parser.add_argument(
        "--resume-from",
        default=None,
        help="Path to existing LoRA adapter dir to continue training from (e.g. tools/aria_specialist_qwen7b)"
    )
    parser.add_argument(
        "--dry-run", action="store_true",
        help="Print config and exit without training (for sanity-checking VRAM estimates)"
    )
    args = parser.parse_args()

    trainer = AriaSpecialistTrainerV2(
        model_name=args.model,
        output_dir=args.output,
        lora_r=args.lora_rank,
        num_epochs=args.epochs,
        batch_size=args.batch_size,
        learning_rate=args.lr,
        resume_from=args.resume_from,
    )

    if args.dry_run:
        print("Dry run — exiting before model load.")
        return

    trainer.setup_model()
    trainer.prepare_datasets(args.data, args.eval_data)
    trainer.train()


if __name__ == "__main__":
    main()
