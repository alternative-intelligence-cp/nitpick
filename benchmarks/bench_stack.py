#!/usr/bin/env python3
"""bench_stack.py — Python equivalent of the Aria stack benchmarks."""
import time

N = 10_000_000

# Warm up
stack = []
s = 0
for w in range(1000):
    stack.append(w * 3 + 1)
    stack.append(w * 7 + 2)
    wb = stack.pop()
    wa = stack.pop()
    s += wa + wb

stack.clear()
s = 0

t_start = time.monotonic_ns()

for i in range(N):
    stack.append(i * 3 + 1)
    stack.append(i * 7 + 2)
    b = stack.pop()
    a = stack.pop()
    s += b + a

t_end = time.monotonic_ns()

elapsed_ns = t_end - t_start
elapsed_ms = elapsed_ns // 1_000_000

print("=== PYTHON LIST-AS-STACK ===")
print(f"  iterations: {N}")
print(f"  sum: {s}")
print(f"  elapsed_ms: {elapsed_ms}")
print(f"  elapsed_ns: {elapsed_ns}")
