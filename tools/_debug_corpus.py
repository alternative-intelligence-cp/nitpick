#!/usr/bin/env python3
"""Quick diagnostic: what format is stdout << in the corpus?"""
import json, sys

found = 0
with open('tools/aria_training_corpus_v4.jsonl') as f:
    for i, line in enumerate(f):
        obj = json.loads(line)
        output = obj.get('output', '')
        if 'stdout <<' in output:
            for cl in output.split('\n'):
                if 'stdout <<' in cl:
                    print(f"Ex {i}: {cl.strip()[:150]}")
                    found += 1
                    break
        if found >= 10:
            break

print(f"\nTotal found: {found}")
sys.stdout.flush()
