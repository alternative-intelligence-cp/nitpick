#!/usr/bin/env node
// bench_stack.js — Node.js equivalent of the Aria stack benchmarks.
"use strict";

const N = 10_000_000;

// Warm up
const stack = [];
let s = 0;
for (let w = 0; w < 1000; w++) {
    stack.push(w * 3 + 1);
    stack.push(w * 7 + 2);
    const wb = stack.pop();
    const wa = stack.pop();
    s += wa + wb;
}
stack.length = 0;
s = 0;

const t_start = process.hrtime.bigint();

for (let i = 0; i < N; i++) {
    stack.push(i * 3 + 1);
    stack.push(i * 7 + 2);
    const b = stack.pop();
    const a = stack.pop();
    s += b + a;
}

const t_end = process.hrtime.bigint();
const elapsed_ns = Number(t_end - t_start);
const elapsed_ms = Math.floor(elapsed_ns / 1_000_000);

console.log("=== NODE.JS ARRAY-AS-STACK ===");
console.log(`  iterations: ${N}`);
console.log(`  sum: ${s}`);
console.log(`  elapsed_ms: ${elapsed_ms}`);
console.log(`  elapsed_ns: ${elapsed_ns}`);
