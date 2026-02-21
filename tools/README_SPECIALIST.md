# Aria Language Specialist Model - Training Guide

## Overview

This directory contains tools for training an Aria language specialist model using modular deep learning techniques. The approach extracts middle layers from Mistral 7B (Apache 2.0) and fine-tunes them on Aria language corpus using LoRA (Low-Rank Adaptation).

**Purpose:** Create an AI "teacher" specialized in Aria language that can answer syntax questions, provide code examples, and assist with development without requiring the full language specification in context.

**Based on:** Research document at `/home/randy/Workspace/META/RESEARCH/aria-lang/Training Custom Language Specialist Layer.md`

## Architecture

- **Base Model:** Mistral 7B v0.3 (Apache 2.0 license)
- **Target Layers:** 14-18 (middle semantic reasoning layers)
- **Fine-tuning Method:** LoRA (Parameter-Efficient Fine-Tuning)
- **Training Data:** ~1,800 examples from:
  - Aria language specification (7,089 lines)
  - Standard library implementations (9,712 lines)
  - Example programs (2,282 lines)
  - Test cases (14,807 lines sampled)
  - Synthetic Q&A pairs

## Hardware Requirements

- **GPU:** NVIDIA RTX 3090 (24GB VRAM) or equivalent
  - A100 (40GB) recommended for larger batch sizes
  - Can use CPU with much slower training
- **RAM:** 32GB+ recommended
- **Storage:** ~50GB for model, checkpoints, and data

## Installation

### 1. Install Dependencies

```bash
cd /home/randy/Workspace/REPOS/aria/tools
pip install -r requirements_specialist.txt
```

### 2. Verify GPU Availability

```bash
nvidia-smi
python3 -c "import torch; print(f'CUDA available: {torch.cuda.is_available()}')"
```

## Training Pipeline

### Phase 1: Prepare Training Data (Already Complete!)

The training corpus has been generated with **1,823 examples**:

```bash
# Already executed - data is at:
# - tools/aria_training_corpus_train.jsonl (1,640 examples)
# - tools/aria_training_corpus_val.jsonl (183 examples)

# To regenerate if needed:
python3 prepare_aria_training_data.py --output aria_training_corpus.jsonl
```

**Data Distribution:**
- Specification: 1,087 examples (weight: 3.0x)
- Stdlib: 375 examples (weight: 2.0x)
- Tests: 211 examples (weight: 0.8x)
- Examples: 141 examples (weight: 1.5x)
- Synthetic Q&A: 9 examples (weight: 2.0x)

### Phase 2: Train the Specialist Model

```bash
# Basic training (3 epochs, batch size 4)
python3 train_aria_specialist.py \
    --data aria_training_corpus_train.jsonl \
    --eval-data aria_training_corpus_val.jsonl \
    --output ./aria_specialist_model \
    --epochs 3 \
    --batch-size 4

# With test inference after training
python3 train_aria_specialist.py \
    --data aria_training_corpus_train.jsonl \
    --eval-data aria_training_corpus_val.jsonl \
    --output ./aria_specialist_model \
    --epochs 3 \
    --test
```

**Training Time Estimate:**
- RTX 3090: ~12-18 hours for 3 epochs
- A100: ~8-12 hours for 3 epochs

**What happens during training:**
1. Downloads Mistral 7B v0.3 (~14GB)
2. Loads model in FP16 to save VRAM
3. Applies LoRA adapters to attention and MLP layers
4. Fine-tunes on Aria corpus with:<br>   - Learning rate: 2e-4
   - Effective batch size: 16 (4 per device × 4 accumulation steps)
   - Cosine learning rate schedule
   - Gradient checkpointing for memory efficiency

**Training Progress:**
- Monitor training loss in terminal output
- Evaluation runs every 100 steps
- Checkpoints saved every 100 steps
- Best model saved based on validation loss

### Phase 3: Use the Trained Model

#### Option A: Python API

```python
from peft import PeftModel, PeftConfig
from transformers import AutoModelForCausalLM, AutoTokenizer

# Load model
config = PeftConfig.from_pretrained('./aria_specialist_model')
base_model = AutoModelForCausalLM.from_pretrained(
    config.base_model_name_or_path,
    torch_dtype=torch.float16,
    device_map="auto"
)
model = PeftModel.from_pretrained(base_model, './aria_specialist_model')
tokenizer = AutoTokenizer.from_pretrained('./aria_specialist_model')

# Ask a question
prompt = """Below is an instruction that describes a task. Write a response that appropriately completes the request.

### Instruction:
How do I use Result<T> for error handling in Aria?

### Response:"""

inputs = tokenizer(prompt, return_tensors="pt").to(model.device)
outputs = model.generate(**inputs, max_new_tokens=256, temperature=0.7)
response = tokenizer.decode(outputs[0], skip_special_tokens=True)
print(response)
```

#### Option B: Create Agent Wrapper

```python
# Create aria_agent.py
from peft import PeftModel, PeftConfig
from transformers import AutoModelForCausalLM, AutoTokenizer
import torch

class AriaLanguageExpert:
    """AI assistant specialized in Aria programming language."""
    
    def __init__(self, model_path='./aria_specialist_model'):
        config = PeftConfig.from_pretrained(model_path)
        base_model = AutoModelForCausalLM.from_pretrained(
            config.base_model_name_or_path,
            torch_dtype=torch.float16,
            device_map="auto"
        )
        self.model = PeftModel.from_pretrained(base_model, model_path)
        self.tokenizer = AutoTokenizer.from_pretrained(model_path)
        self.model.eval()
    
    def ask(self, question: str, context: str = "") -> str:
        """Ask the Aria expert a question."""
        if context:
            prompt = f"""Below is an instruction that describes a task, paired with an input that provides further context. Write a response that appropriately completes the request.

### Instruction:
{question}

### Input:
{context}

### Response:"""
        else:
            prompt = f"""Below is an instruction that describes a task. Write a response that appropriately completes the request.

### Instruction:
{question}

### Response:"""
        
        inputs = self.tokenizer(prompt, return_tensors="pt").to(self.model.device)
        
        with torch.no_grad():
            outputs = self.model.generate(
                **inputs,
                max_new_tokens=512,
                temperature=0.7,
                top_p=0.9,
                do_sample=True,
                pad_token_id=self.tokenizer.eos_token_id
            )
        
        response = self.tokenizer.decode(outputs[0], skip_special_tokens=True)
        # Extract just the response part
        if "### Response:" in response:
            response = response.split("### Response:")[-1].strip()
        return response

# Usage
expert = AriaLanguageExpert()
answer = expert.ask("How do I declare a pointer in Aria?")
print(answer)
```

## Evaluation Metrics

### Quantitative Metrics (After Training)

1. **Perplexity:** Target <2.5 on held-out Aria code
2. **Validation Loss:** Monitor convergence
3. **Code Completion:** Test on completing partial Aria functions

### Qualitative Testing

Test the model on these questions:

```python
test_questions = [
    "How do I declare a pointer in Aria?",
    "What's the syntax for Result<T>?",
    "Show me how to define a struct in Aria",
    "Explain the difference between @ and -> in Aria",
    "How do I handle errors in Aria functions?",
    "What does the unwrap operator ? do?",
]

for q in test_questions:
    print(f"\nQ: {q}")
    print(f"A: {expert.ask(q)}\n")
```

**Success Criteria:**
- ✓ Correct Aria syntax (not Rust/C++ contamination)
- ✓ Accurate pointer syntax (-> for types, @ for address-of)
- ✓ Result<T> fields: .value, .is_error, .error
- ✓ Explains pass() and fail() correctly
- ✓ No references to outdated @ syntax for references

## Troubleshooting

### Out of Memory (OOM)

```bash
# Reduce batch size
python3 train_aria_specialist.py --batch-size 2

# Or use gradient accumulation (in code, increase gradient_accumulation_steps)
```

### Slow Training

- Ensure FP16 is enabled (default: yes)
- Check GPU utilization: `nvidia-smi -l 1`
- Consider using `gradient_checkpointing=True` (default: yes)

### Model Not Learning

- Check validation loss - should decrease over time
- Verify data quality: `head -5 aria_training_corpus_train.jsonl`
- Increase learning rate cautiously (default: 2e-4)

## Next Steps After Training

1. **Evaluate on held-out Aria code snippets**
2. **Test on real development questions**
3. **Deploy as local API server** (FastAPI or similar)
4. **Integrate with IDE** (VS Code extension, GitHub Copilot alternative)
5. **Continuous improvement:** Re-train as Aria language evolves

## Files Generated

```
tools/
├── aria_training_corpus.jsonl          # Full corpus (1,823 examples)
├── aria_training_corpus_train.jsonl    # Training split (1,640 examples)
├── aria_training_corpus_val.jsonl      # Validation split (183 examples)
└── aria_specialist_model/              # Trained model output
    ├── adapter_config.json             # LoRA configuration
    ├── adapter_model.bin               # LoRA weights (~50-100MB)
    ├── tokenizer_config.json
    ├── tokenizer.model
    └── README.md                       # Model card
```

## Cost/Time Summary

- **Data Preparation:** 5 minutes (automated)
- **Model Download:** 15-30 minutes (one-time, 14GB)
- **Training:** 12-18 hours (RTX 3090, 3 epochs)
- **Disk Space:** ~50GB total
- **VRAM:** ~18-22GB during training

## Citation

This approach is based on:
- **Mistral 7B** (Apache 2.0): https://mistral.ai
- **LoRA** (Low-Rank Adaptation): https://arxiv.org/abs/2106.09685
- **PEFT Library**: https://github.com/huggingface/peft

---

**Status:** Ready to train! All data prepared, RTX 3090 detected, scripts ready.

**Recommendation:** Start overnight training run, monitor first few hours to ensure convergence starts.
