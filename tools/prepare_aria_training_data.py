#!/usr/bin/env python3
"""
Aria Language Specialist Training Data Preparation
Extracts and structures .aria source files and specifications for LLM fine-tuning.

Usage:
    python prepare_aria_training_data.py --output aria_training_corpus.jsonl
"""

import os
import json
import re
import argparse
from pathlib import Path
from typing import Dict, List, Tuple
from dataclasses import dataclass, asdict

@dataclass
class TrainingExample:
    """Single training example for instruction tuning."""
    instruction: str
    input: str
    output: str
    metadata: Dict
    weight: float = 1.0

class AriaCorpusBuilder:
    """Builds training corpus from Aria language sources."""
    
    def __init__(self, repo_root: str):
        self.repo_root = Path(repo_root)
        self.examples: List[TrainingExample] = []
        
    def extract_function_docs(self, code: str) -> List[Tuple[str, str, str]]:
        """
        Extract function signatures, documentation, and implementations.
        Returns: [(signature, docstring, implementation)]
        """
        results = []
        # Match: optional docstring + function definition
        pattern = r'(?:///(.*?)\n)?func:(\w+)\s*=\s*([^(]+)\((.*?)\)\s*\{(.*?)(?=\n(?:func:|pub func:|//|$))'
        
        for match in re.finditer(pattern, code, re.DOTALL | re.MULTILINE):
            docstring = (match.group(1) or "").strip()
            func_name = match.group(2)
            return_type = match.group(3).strip()
            params = match.group(4).strip()
            body = match.group(5).strip()
            
            signature = f"func:{func_name} = {return_type}({params})"
            implementation = f"{signature} {{\n{body}\n}}"
            
            results.append((signature, docstring, implementation))
            
        return results
    
    def extract_type_definitions(self, code: str) -> List[Tuple[str, str]]:
        """
        Extract struct and enum definitions.
        Returns: [(definition, context)]
        """
        results = []
        
        # Structs: struct:Name = { ... };
        struct_pattern = r'(?:///(.*?)\n)?(?:pub\s+)?struct:(\w+)\s*=\s*\{([^}]+)\};'
        for match in re.finditer(struct_pattern, code, re.DOTALL):
            docstring = (match.group(1) or "").strip()
            struct_name = match.group(2)
            fields = match.group(3).strip()
            
            definition = f"struct:{struct_name} = {{\n{fields}\n}};"
            results.append((definition, docstring))
        
        # Enums: enum:Name = { ... };
        enum_pattern = r'(?:///(.*?)\n)?(?:pub\s+)?enum:(\w+)\s*=\s*\{([^}]+)\};'
        for match in re.finditer(enum_pattern, code, re.DOTALL):
            docstring = (match.group(1) or "").strip()
            enum_name = match.group(2)
            values = match.group(3).strip()
            
            definition = f"enum:{enum_name} = {{\n{values}\n}};"
            results.append((definition, docstring))
            
        return results
    
    def process_aria_file(self, filepath: Path, weight: float = 1.0):
        """Process a single .aria source file."""
        try:
            with open(filepath, 'r', encoding='utf-8') as f:
                code = f.read()
        except Exception as e:
            print(f"Error reading {filepath}: {e}")
            return
        
        # Determine category for metadata
        path_str = str(filepath.relative_to(self.repo_root))
        if 'lib/std' in path_str:
            category = 'stdlib'
            weight = 2.0  # Higher weight for stdlib
        elif 'examples' in path_str:
            category = 'example'
            weight = 1.5
        elif 'test_' in filepath.name:
            category = 'test'
            weight = 0.8  # Lower weight for tests
        else:
            category = 'source'
        
        # Extract functions
        functions = self.extract_function_docs(code)
        for sig, doc, impl in functions:
            # Create Q&A pair for function
            if doc:
                instruction = f"Explain this Aria function: {sig}"
                output = f"{doc}\n\nImplementation:\n```aria\n{impl}\n```"
            else:
                instruction = f"Show me the implementation of: {sig}"
                output = f"```aria\n{impl}\n```"
            
            self.examples.append(TrainingExample(
                instruction=instruction,
                input="",
                output=output,
                metadata={
                    "source": path_str,
                    "category": category,
                    "type": "function",
                    "confidence": "high"
                },
                weight=weight
            ))
        
        # Extract type definitions
        types = self.extract_type_definitions(code)
        for definition, doc in types:
            instruction = "Show me an Aria type definition"
            if doc:
                output = f"{doc}\n\n```aria\n{definition}\n```"
            else:
                output = f"```aria\n{definition}\n```"
            
            self.examples.append(TrainingExample(
                instruction=instruction,
                input="",
                output=output,
                metadata={
                    "source": path_str,
                    "category": category,
                    "type": "type_definition",
                    "confidence": "high"
                },
                weight=weight
            ))
        
        # Also add complete file as context example
        if len(code.strip()) > 100 and category in ['stdlib', 'example']:
            module_name = filepath.stem
            self.examples.append(TrainingExample(
                instruction=f"Show me the complete {module_name} module in Aria",
                input="",
                output=f"```aria\n{code}\n```",
                metadata={
                    "source": path_str,
                    "category": category,
                    "type": "complete_module",
                    "confidence": "high"
                },
                weight=weight * 1.5  # Boost complete modules
            ))
    
    def process_spec_file(self, spec_path: Path):
        """Process the language specification document."""
        try:
            with open(spec_path, 'r', encoding='utf-8') as f:
                content = f.read()
        except Exception as e:
            print(f"Error reading spec: {e}")
            return
        
        # Split spec into sections (simple approach: by blank lines and headers)
        sections = re.split(r'\n\s*\n', content)
        
        for section in sections:
            section = section.strip()
            if len(section) < 50:  # Skip tiny fragments
                continue
            
            # Try to extract section title
            lines = section.split('\n')
            title = lines[0] if lines[0].startswith('//') or lines[0].isupper() else "Language Specification"
            
            # Create Q&A for spec sections
            if 'pointer' in section.lower() or 'arrow' in section.lower():
                instruction = "Explain pointer syntax in Aria"
            elif 'result' in section.lower() and '<' in section:
                instruction = "How does Result<T> work in Aria?"
            elif 'struct' in section.lower():
                instruction = "What is the syntax for defining structs in Aria?"
            elif 'enum' in section.lower():
                instruction = "What is the syntax for defining enums in Aria?"
            else:
                instruction = f"Explain this Aria language feature: {title}"
            
            self.examples.append(TrainingExample(
                instruction=instruction,
                input="",
                output=section,
                metadata={
                    "source": "aria_specs.txt",
                    "category": "specification",
                    "type": "spec_section",
                    "confidence": "canonical"
                },
                weight=3.0  # Highest weight for specs
            ))
    
    def generate_synthetic_qa(self):
        """Generate synthetic Q&A pairs for common patterns."""
        
        # Common syntax questions
        syntax_qa = [
            {
                "q": "How do I declare a pointer type in Aria?",
                "a": "Use the arrow syntax: `int8->` means 'pointer to int8'. For example:\n```aria\nint8->:ptr  // pointer variable\nint32->:array  // pointer to int32\n```"
            },
            {
                "q": "How do I get the address of a variable in Aria?",
                "a": "Use the @ operator (address-of):\n```aria\nint32:value = 42;\nint32->:ptr = @value;  // ptr now points to value\n```"
            },
            {
                "q": "How do I dereference a pointer in Aria?",
                "a": "Use the <- operator:\n```aria\nint32->:ptr = @value;\nint32:copy = <-ptr;  // dereference to get value\n```"
            },
            {
                "q": "What's the difference between @ and -> in Aria?",
                "a": "- `->` in types means 'pointer to' (e.g., `int8->:ptr`)\n- `@` gets the address of a variable (e.g., `@value`)\n- `<-` dereferences a pointer (e.g., `<-ptr`)\n- `->` for member access on pointers (e.g., `ptr->field`)"
            },
            {
                "q": "How do I define a struct in Aria?",
                "a": "Use `struct:Name = { field:type; ... };` syntax:\n```aria\nstruct:Point = {\n    int32:x;\n    int32:y;\n};\n```"
            },
            {
                "q": "How do I define an enum in Aria?",
                "a": "Use `enum:Name = { VALUE = number, ... };` syntax:\n```aria\nenum:Status = {\n    OK = 0,\n    ERROR = 1,\n    PENDING = 2\n};\n```"
            },
            {
                "q": "How do I handle errors in Aria?",
                "a": "Use Result<T> type with .value, .is_error, and .error fields:\n```aria\nResult<int32>:result = some_function();\nif (result.is_error) {\n    // handle error: result.error\n} else {\n    // use value: result.value\n}\n```\n\nOr use the unwrap operator ?:\n```aria\nint32:val = some_function() ? 0;  // 0 if error\n```"
            },
            {
                "q": "What does the ? operator do in Aria?",
                "a": "The ? operator unwraps a Result<T>, returning the default value on error:\n```aria\nResult<int32>:result = read_file(path);\nint32:value = result ? 0;  // Returns result.value if ok, else 0\n```"
            },
            {
                "q": "How do I return from a function in Aria?",
                "a": "Use `pass()` for success or `fail()` for errors:\n```aria\nfunc:get_value = int32() {\n    pass(42);  // return success with value 42\n}\n\nfunc:checked_value = Result<int32>(int32:x) {\n    if (x < 0) {\n        fail(-1);  // return error\n    } else {\n        pass(ok(x));  // return success\n    }\n}\n```"
            },
        ]
        
        for qa in syntax_qa:
            self.examples.append(TrainingExample(
                instruction=qa["q"],
                input="",
                output=qa["a"],
                metadata={
                    "source": "synthetic",
                    "category": "qa",
                    "type": "syntax_explanation",
                    "confidence": "high"
                },
                weight=2.0
            ))
    
    def build_corpus(self):
        """Build complete training corpus."""
        print("Building Aria training corpus...")
        
        # Process stdlib
        print("Processing stdlib...")
        stdlib_path = self.repo_root / "lib" / "std"
        if stdlib_path.exists():
            for aria_file in stdlib_path.rglob("*.aria"):
                self.process_aria_file(aria_file, weight=2.0)
        
        # Process examples
        print("Processing examples...")
        examples_path = self.repo_root / "examples"
        if examples_path.exists():
            for aria_file in examples_path.rglob("*.aria"):
                self.process_aria_file(aria_file, weight=1.5)
        
        # Process tests (sample to avoid overwhelming with test code)
        print("Processing tests (sampling)...")
        test_files = list(self.repo_root.rglob("test_*.aria"))
        # Sample ~20% of tests
        import random
        random.seed(42)
        sampled_tests = random.sample(test_files, min(len(test_files), 70))
        for aria_file in sampled_tests:
            self.process_aria_file(aria_file, weight=0.5)
        
        # Process specification
        print("Processing language specification...")
        spec_path = self.repo_root / ".internal" / "aria_specs.txt"
        if spec_path.exists():
            self.process_spec_file(spec_path)
        
        # Generate synthetic Q&A
        print("Generating synthetic Q&A pairs...")
        self.generate_synthetic_qa()
        
        print(f"\nTotal examples generated: {len(self.examples)}")
        
        # Calculate weighted distribution
        categories = {}
        for ex in self.examples:
            cat = ex.metadata['category']
            categories[cat] = categories.get(cat, 0) + 1
        
        print("\nDistribution by category:")
        for cat, count in sorted(categories.items()):
            print(f"  {cat}: {count}")
    
    def save_corpus(self, output_path: str):
        """Save corpus in JSONL format for training."""
        print(f"\nSaving to {output_path}...")
        
        with open(output_path, 'w', encoding='utf-8') as f:
            for example in self.examples:
                # Convert to dict and write as JSON line
                record = {
                    'instruction': example.instruction,
                    'input': example.input,
                    'output': example.output,
                    'metadata': example.metadata,
                    'weight': example.weight
                }
                f.write(json.dumps(record, ensure_ascii=False) + '\n')
        
        print(f"Saved {len(self.examples)} examples")
        
        # Also save a train/val split
        import random
        random.seed(42)
        shuffled = self.examples.copy()
        random.shuffle(shuffled)
        
        split_idx = int(len(shuffled) * 0.9)  # 90/10 split
        train = shuffled[:split_idx]
        val = shuffled[split_idx:]
        
        train_path = output_path.replace('.jsonl', '_train.jsonl')
        val_path = output_path.replace('.jsonl', '_val.jsonl')
        
        with open(train_path, 'w', encoding='utf-8') as f:
            for ex in train:
                record = {
                    'instruction': ex.instruction,
                    'input': ex.input,
                    'output': ex.output,
                    'metadata': ex.metadata,
                    'weight': ex.weight
                }
                f.write(json.dumps(record, ensure_ascii=False) + '\n')
        
        with open(val_path, 'w', encoding='utf-8') as f:
            for ex in val:
                record = {
                    'instruction': ex.instruction,
                    'input': ex.input,
                    'output': ex.output,
                    'metadata': ex.metadata,
                    'weight': ex.weight
                }
                f.write(json.dumps(record, ensure_ascii=False) + '\n')
        
        print(f"Train: {len(train)} examples -> {train_path}")
        print(f"Val: {len(val)} examples -> {val_path}")


def main():
    parser = argparse.ArgumentParser(description='Prepare Aria training data')
    parser.add_argument('--repo', default='.', help='Aria repository root')
    parser.add_argument('--output', default='aria_corpus.jsonl', help='Output file')
    args = parser.parse_args()
    
    builder = AriaCorpusBuilder(args.repo)
    builder.build_corpus()
    builder.save_corpus(args.output)
    
    print("\n✓ Data preparation complete!")
    print("\nNext steps:")
    print("1. Review generated examples in the output file")
    print("2. Set up Mistral 7B model extraction")
    print("3. Configure LoRA fine-tuning")


if __name__ == '__main__':
    main()
