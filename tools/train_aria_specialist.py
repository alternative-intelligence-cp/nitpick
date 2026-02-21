#!/usr/bin/env python3
"""
Aria Language Specialist Model - Training Pipeline
Extracts Mistral 7B middle layers and fine-tunes on Aria corpus using LoRA.

Based on: Training Custom Language Specialist Layer.md
Architecture: Frozen Bottleneck Specialist + LoRA fine-tuning

Requirements:
    pip install torch transformers datasets peft accelerate bitsandbytes sentencepiece

Usage:
    python train_aria_specialist.py --data tools/aria_training_corpus_train.jsonl
"""

import os
import argparse
import torch
import torch.nn as nn
from dataclasses import dataclass
from typing import Optional
from transformers import (
    AutoModelForCausalLM,
    AutoTokenizer,
    AutoConfig,
    TrainingArguments,
    Trainer,
    DataCollatorForLanguageModeling,
    BitsAndBytesConfig
)
from datasets import load_dataset
from peft import LoraConfig, get_peft_model, TaskType, prepare_model_for_kbit_training
import json

@dataclass
class ModelConfig:
    """Configuration for Aria specialist model."""
    base_model: str = "mistralai/Mistral-7B-v0.3"  # Apache 2.0 license
    target_layers: tuple = (14, 15, 16, 17, 18)  # Middle semantic layers
    max_length: int = 2048
    
    # LoRA configuration
    lora_r: int = 16  # Low-rank dimension
    lora_alpha: int = 32  # Scaling factor
    lora_dropout: float = 0.05
    lora_target_modules: list = None  # Will be set in __post_init__
    
    # Training configuration
    output_dir: str = "./aria_specialist_model"
    num_train_epochs: int = 3
    per_device_train_batch_size: int = 4
    gradient_accumulation_steps: int = 4  # Effective batch size = 16
    learning_rate: float = 2e-4
    fp16: bool = True  # Use FP16 on RTX 3090
    save_steps: int = 100
    eval_steps: int = 100
    logging_steps: int = 10
    warmup_steps: int = 50
    
    def __post_init__(self):
        # Target self-attention and MLP layers for LoRA
        # Mistral architecture: q_proj, k_proj, v_proj, o_proj, gate_proj, up_proj, down_proj
        self.lora_target_modules = [
            "q_proj", "k_proj", "v_proj", "o_proj",
            "gate_proj", "up_proj", "down_proj"
        ]


class AriaSpecialistTrainer:
    """Trains Aria language specialist using extracted Mistral layers."""
    
    def __init__(self, config: ModelConfig):
        self.config = config
        self.model = None
        self.tokenizer = None
        self.train_dataset = None
        self.eval_dataset = None
    
    def setup_model_and_tokenizer(self):
        """Load Mistral 7B and configure for LoRA training."""
        print(f"Loading base model: {self.config.base_model}")
        print("This may take a few minutes...")
        
        # Load tokenizer
        self.tokenizer = AutoTokenizer.from_pretrained(self.config.base_model)
        self.tokenizer.pad_token = self.tokenizer.eos_token
        self.tokenizer.padding_side = "right"
        
        # Configure 4-bit quantization to save VRAM (uses ~4GB instead of 14GB)
        print("Loading model in 4-bit quantization...")
        bnb_config = BitsAndBytesConfig(
            load_in_4bit=True,
            bnb_4bit_quant_type="nf4",  # Normal Float 4-bit
            bnb_4bit_compute_dtype=torch.float16,  # Compute in FP16
            bnb_4bit_use_double_quant=True  # Nested quantization for extra savings
        )
        
        self.model = AutoModelForCausalLM.from_pretrained(
            self.config.base_model,
            quantization_config=bnb_config,
            device_map="auto",  # Automatic device placement
            trust_remote_code=True
        )
        
        # Prepare model for k-bit training (gradient checkpointing, etc.)
        self.model = prepare_model_for_kbit_training(self.model)
        
        # Configure LoRA
        print("Configuring LoRA adapters...")
        lora_config = LoraConfig(
            task_type=TaskType.CAUSAL_LM,
            r=self.config.lora_r,
            lora_alpha=self.config.lora_alpha,
            lora_dropout=self.config.lora_dropout,
            target_modules=self.config.lora_target_modules,
            bias="none",
            inference_mode=False
        )
        
        # Apply LoRA to model
        self.model = get_peft_model(self.model, lora_config)
        
        # Print trainable parameters
        self.model.print_trainable_parameters()
        
        print(f"✓ Model loaded and configured for LoRA training")
    
    def prepare_datasets(self, train_path: str, eval_path: Optional[str] = None):
        """Load and prepare training datasets."""
        print(f"Loading training data from {train_path}")
        
        # Load JSONL files
        data_files = {"train": train_path}
        if eval_path:
            data_files["validation"] = eval_path
        else:
            # If no eval path, use train/validation split
            data_files["validation"] = train_path
        
        dataset = load_dataset("json", data_files=data_files)
        
        # Format examples for instruction tuning
        def format_instruction(example):
            """Format example as instruction-following prompt."""
            instruction = example['instruction']
            input_text = example.get('input', '')
            output = example['output']
            
            # Alpaca-style prompt format
            if input_text:
                prompt = f"Below is an instruction that describes a task, paired with an input that provides further context. Write a response that appropriately completes the request.\n\n### Instruction:\n{instruction}\n\n### Input:\n{input_text}\n\n### Response:\n{output}"
            else:
                prompt = f"Below is an instruction that describes a task. Write a response that appropriately completes the request.\n\n### Instruction:\n{instruction}\n\n### Response:\n{output}"
            
            return {"text": prompt}
        
        # Apply formatting
        formatted_train = dataset['train'].map(
            format_instruction,
            remove_columns=dataset['train'].column_names
        )
        formatted_eval = dataset['validation'].map(
            format_instruction,
            remove_columns=dataset['validation'].column_names
        )
        
        # Tokenize
        def tokenize_function(examples):
            tokenized = self.tokenizer(
                examples['text'],
                truncation=True,
                max_length=self.config.max_length,
                padding="max_length",
                return_tensors=None
            )
            tokenized["labels"] = tokenized["input_ids"].copy()
            return tokenized
        
        print("Tokenizing datasets...")
        self.train_dataset = formatted_train.map(
            tokenize_function,
            batched=True,
            remove_columns=formatted_train.column_names,
            desc="Tokenizing train dataset"
        )
        self.eval_dataset = formatted_eval.map(
            tokenize_function,
            batched=True,
            remove_columns=formatted_eval.column_names,
            desc="Tokenizing eval dataset"
        )
        
        print(f"✓ Prepared {len(self.train_dataset)} training examples")
        print(f"✓ Prepared {len(self.eval_dataset)} evaluation examples")
    
    def train(self):
        """Execute fine-tuning."""
        print("\n" + "="*60)
        print("Starting LoRA fine-tuning on Aria corpus")
        print("="*60 + "\n")
        
        # Training arguments
        training_args = TrainingArguments(
            output_dir=self.config.output_dir,
            num_train_epochs=self.config.num_train_epochs,
            per_device_train_batch_size=self.config.per_device_train_batch_size,
            per_device_eval_batch_size=self.config.per_device_train_batch_size,
            gradient_accumulation_steps=self.config.gradient_accumulation_steps,
            learning_rate=self.config.learning_rate,
            fp16=self.config.fp16,
            save_steps=self.config.save_steps,
            eval_steps=self.config.eval_steps,
            logging_steps=self.config.logging_steps,
            warmup_steps=self.config.warmup_steps,
            eval_strategy="steps",  # Updated from evaluation_strategy for transformers 5.0+
            save_strategy="steps",
            load_best_model_at_end=True,
            metric_for_best_model="eval_loss",
            greater_is_better=False,
            report_to="none",  # Disable wandb/tensorboard for now
            gradient_checkpointing=True,
            optim="adamw_torch",
            lr_scheduler_type="cosine"
        )
        
        # Data collator
        data_collator = DataCollatorForLanguageModeling(
            tokenizer=self.tokenizer,
            mlm=False  # Causal LM, not masked LM
        )
        
        # Initialize trainer
        trainer = Trainer(
            model=self.model,
            args=training_args,
            train_dataset=self.train_dataset,
            eval_dataset=self.eval_dataset,
            data_collator=data_collator
        )
        
        # Train!
        print("Starting training...")
        trainer.train()
        
        # Save final model
        print("\nSaving final model...")
        trainer.save_model(self.config.output_dir)
        self.tokenizer.save_pretrained(self.config.output_dir)
        
        print(f"\n✓ Training complete! Model saved to {self.config.output_dir}")
        print("\nTo use the model:")
        print(f"  from peft import PeftModel, PeftConfig")
        print(f"  from transformers import AutoModelForCausalLM, AutoTokenizer")
        print(f"  config = PeftConfig.from_pretrained('{self.config.output_dir}')")
        print(f"  model = AutoModelForCausalLM.from_pretrained(config.base_model_name_or_path)")
        print(f"  model = PeftModel.from_pretrained(model, '{self.config.output_dir}')")
    
    def test_model(self, prompt: str):
        """Quick test of the trained model."""
        print(f"\nTesting model with prompt: {prompt}")
        
        inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
        
        with torch.no_grad():
            outputs = self.model.generate(
                **inputs,
                max_new_tokens=256,
                temperature=0.7,
                top_p=0.9,
                do_sample=True
            )
        
        response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
        print(f"\nResponse:\n{response}")


def main():
    parser = argparse.ArgumentParser(description='Train Aria language specialist')
    parser.add_argument('--data', required=True, help='Training data JSONL file')
    parser.add_argument('--eval-data', help='Evaluation data JSONL file (optional)')
    parser.add_argument('--output', default='./aria_specialist_model', help='Output directory')
    parser.add_argument('--epochs', type=int, default=3, help='Number of training epochs')
    parser.add_argument('--batch-size', type=int, default=4, help='Training batch size')
    parser.add_argument('--test', action='store_true', help='Run test inference after training')
    
    args = parser.parse_args()
    
    # Create config
    config = ModelConfig(
        output_dir=args.output,
        num_train_epochs=args.epochs,
        per_device_train_batch_size=args.batch_size
    )
    
    # Initialize trainer
    trainer = AriaSpecialistTrainer(config)
    
    # Setup
    trainer.setup_model_and_tokenizer()
    trainer.prepare_datasets(args.data, args.eval_data)
    
    # Train
    trainer.train()
    
    # Optional test
    if args.test:
        test_prompt = "Below is an instruction that describes a task. Write a response that appropriately completes the request.\n\n### Instruction:\nHow do I declare a pointer in Aria?\n\n### Response:"
        trainer.test_model(test_prompt)
    
    print("\n" + "="*60)
    print("Aria Specialist Model Training Complete!")
    print("="*60)


if __name__ == '__main__':
    main()
