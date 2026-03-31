#!/usr/bin/env python3
"""
Aria Fuzzer V2 - Program Generator
Grammar-based generation of valid Aria programs
"""

import random
from typing import List, Optional, Tuple
from pathlib import Path

from grammar_parser import (
    load_type_system, load_operators, load_grammar,
    PrimitiveType, TypeCategory, Operator
)

# Types safe to use in generated arithmetic (avoids frac/tfp/vec struct complexity)
ARITHMETIC_CATEGORIES = {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB}

# Types where we skip boundary-value edge cases (no known int64-range bounds)
LARGE_TYPES = {'int128','int256','int512','int1024','int2048','int4096',
               'uint128','uint256','uint512','uint1024','uint2048','uint4096'}


class AriaGenerator:
    """Generates syntactically valid Aria programs."""
    
    def __init__(self, aria_root: Path = None, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)
        
        self.type_system = load_type_system()
        self.operators = load_operators()
        self.grammar = load_grammar()
        
        self.var_counter = 0
        self.func_counter = 0
    
    # ------------------------------------------------------------------
    # Public interface
    # ------------------------------------------------------------------
    
    def generate_program(self, strategy: str = 'minimal') -> str:
        """
        Generate a complete Aria program.

        Strategies:
          minimal       Single function returning 0
          type_test     Arithmetic for a random numeric type
          control_flow  if / while / for / till / loop
          functions     Multiple functions with pass/fail
          edge_case     Boundary values
          pick          pick statement with labeled arms
          when          when/then/end construct
          defer         defer statement LIFO ordering
          result        Result<T> pass/fail error propagation
          cast          => cast operator
          pipeline      |> and <| pipeline operators
        """
        dispatch = {
            'minimal':      self._gen_minimal,
            'type_test':    lambda: self._gen_type_test(self._random_arithmetic_type()),
            'control_flow': self._gen_control_flow,
            'functions':    self._gen_functions,
            'edge_case':    self._gen_edge_case,
            'pick':         self._gen_pick,
            'when':         self._gen_when,
            'defer':        self._gen_defer,
            'result':       self._gen_result,
            'cast':         self._gen_cast,
            'pipeline':     self._gen_pipeline,
            'struct':       self._gen_struct,
            'enum':         self._gen_enum,
            'array':        self._gen_array,
            'pointer':      self._gen_pointer,
            'loop_range':   self._gen_loop_range,
            'optional_type': self._gen_optional_type,
            'combo': self._gen_combo,
            'negative': self._gen_negative,
            'type_coercion': self._gen_type_coercion,
            'large_type': self._gen_large_type,
            'recursive': self._gen_recursive,
            'tbb_sticky': self._gen_tbb_sticky,
            'string_interp': self._gen_string_interp,
            'multi_struct': self._gen_multi_struct,
            'runtime_check': self._gen_runtime_check,
            'syscall': self._gen_syscall,
        }
        fn = dispatch.get(strategy)
        if fn is None:
            fn = dispatch[random.choice(list(dispatch))]
        return fn()
    
    # ------------------------------------------------------------------
    # Strategy implementations
    # ------------------------------------------------------------------
    
    def _gen_minimal(self) -> str:
        return (
            "func:main = int32() {\n"
            "    exit 0;\n"
            "};\n"
            "\n"
            "func:failsafe = int32(tbb32:err) { exit 1; };\n"
        )
    
    def _gen_type_test(self, typ: PrimitiveType) -> str:
        """Test arithmetic operations for a given type."""
        lines = []
        v1 = self._lit(typ)
        v2 = self._lit(typ)
        
        lines.append("func:main = int32() {")
        lines.append(f"    {typ.name}:a = {v1};")
        lines.append(f"    {typ.name}:b = {v2};")
        
        bin_ops = self.operators.get_binary_operators(typ.category)
        # Omit / and % when b could be zero
        safe_ops = [op for op in bin_ops if op.symbol not in ('/', '%')][:4]
        for op in safe_ops:
            v = self._var()
            if op.symbol in ('==', '!=', '<', '>', '<=', '>='):
                lines.append(f"    bool:{v} = (a {op.symbol} b);")
            else:
                lines.append(f"    {typ.name}:{v} = (a {op.symbol} b);")
        
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_control_flow(self) -> str:
        """if/while/for/till/loop in one function."""
        # Use standard int/float types to avoid TBB literal suffix ambiguity (0tbb → ternary)
        typ = random.choice([t for t in self.type_system.get_all_types()
                            if t.name in ('int8', 'int16', 'int32', 'int64',
                                          'uint8', 'uint16', 'uint32', 'uint64')])
        v1 = self._lit(typ, random.randint(3, 10))
        lines = []
        
        lines.append("func:main = int32() {")
        lines.append(f"    {typ.name}:x = {v1};")
        lines.append("")
        
        # if / else
        lines.append(f"    if (x > {self._lit(typ, 0)}) {{")
        lines.append(f"        x = x + {self._lit(typ, 1)};")
        lines.append("    } else {")
        lines.append(f"        x = x - {self._lit(typ, 1)};")
        lines.append("    }")
        lines.append("")
        
        # while
        lines.append(f"    int32:count = 0i32;")
        lines.append(f"    while (count < 3i32) {{")
        lines.append(f"        count = count + 1i32;")
        lines.append("    }")
        lines.append("")
        
        # for range
        n = random.randint(3, 8)
        lines.append(f"    int32:sum = 0i32;")
        lines.append(f"    for (int32:i in 1..{n}) {{")
        lines.append(f"        sum = sum + 1i32;")
        lines.append("    }")
        lines.append("")
        
        # till + $ variable
        lines.append(f"    int64:last = 0i64;")
        lines.append(f"    till({n}, 1) {{")
        lines.append(f"        last = $;")
        lines.append("    }")
        lines.append("")
        
        # while loop with counter (replaces old loop{break} which isn't supported)
        lines.append(f"    int32:lc = 0i32;")
        lines.append(f"    while (lc < 3i32) {{")
        lines.append(f"        lc = lc + 1i32;")
        lines.append("    }")
        lines.append("")
        
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_functions(self) -> str:
        """Two functions: helper with pass/fail, main unwraps with ? default."""
        fname = f"helper_{self._func_name()}"
        lines = []
        
        lines.append(f"func:{fname} = int32(int32:x) {{")
        lines.append(f"    if (x < 0i32) {{ fail 1i32; }}")
        lines.append(f"    pass x * 2i32;")
        lines.append("};")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append(f"    int32:v = {fname}(5i32) ? -1i32;")
        lines.append(f"    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_edge_case(self) -> str:
        """Boundary values for a random integer type."""
        typ = self._random_integer_type()
        lines = []
        
        lines.append("func:main = int32() {")
        
        if typ.name in LARGE_TYPES:
            # Large types: use properly suffixed literals
            suf = typ.literal_suffix()
            lines.append(f"    {typ.name}:a = 1000000{suf};")
            lines.append(f"    {typ.name}:b = 2000000{suf};")
            lines.append(f"    {typ.name}:c = a + b;")
        else:
            suf = typ.literal_suffix()
            if typ.min_value is not None:
                # Avoid emitting the ERR sentinel for TBB types
                safe_min = typ.min_value + (1 if typ.category == TypeCategory.TBB else 0)
                lines.append(f"    {typ.name}:min_val = {safe_min}{suf};")
            if typ.max_value is not None:
                lines.append(f"    {typ.name}:max_val = {typ.max_value}{suf};")
            lines.append(f"    {typ.name}:zero = 0{suf};")
            if typ.category == TypeCategory.INTEGER and typ.min_value is not None and typ.min_value < 0:
                lines.append(f"    {typ.name}:neg = -1{suf};")
        
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_pick(self) -> str:
        """pick statement with parenthesized arms."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int32:x = 2i32;")
        lines.append("    int32:result = 0i32;")
        lines.append("    pick (x) {")
        lines.append("        (1i32) { result = 10i32; },")
        lines.append("        (2i32) { result = 20i32; },")
        lines.append("        (3i32) { result = 30i32; },")
        lines.append("        (*) {}")
        lines.append("    }")
        lines.append("    if (result != 20i32) { exit 1; }")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_when(self) -> str:
        """when/then/end — runs body while condition holds, then executes then/end."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int32:i = 0i32;")
        lines.append("    int32:result = 0i32;")
        lines.append("    when (i < 3i32) {")
        lines.append("        i = i + 1i32;")
        lines.append("    } then {")
        lines.append("        result = 1i32;  // ran at least once")
        lines.append("    } end {")
        lines.append("        result = 2i32;  // condition was false from start")
        lines.append("    }")
        lines.append("    if (result != 1i32) { exit 1; }")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_defer(self) -> str:
        """defer executes at scope exit in LIFO order."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int32:x = 0i32;")
        lines.append("    defer { x = x + 1i32; }")
        lines.append("    defer { x = x + 10i32; }")
        lines.append("    x = x + 100i32;")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_result(self) -> str:
        """Result unwrap with ? default and raw()."""
        lines = []
        lines.append("func:might_fail = int32(int32:x) {")
        lines.append("    if (x == 0i32) { fail 42i32; }")
        lines.append("    pass x + 1i32;")
        lines.append("};")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append("    int32:val = might_fail(5i32) ? -1i32;")
        lines.append("    int32:safe = raw might_fail(3i32);")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_cast(self) -> str:
        """=> cast operator: widening and narrowing."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int8:small = 42i8;")
        lines.append("    int64:big = small => int64;")
        lines.append("    int32:mid = big => int32;")
        lines.append("    flt64:f = mid => flt64;")
        lines.append("    if (mid != 42i32) { exit 1; }")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _gen_pipeline(self) -> str:
        """Forward |> and backward <| pipeline operators with raw()."""
        lines = []
        lines.append("func:double = int32(int32:x) { pass x * 2i32; };")
        lines.append("func:inc    = int32(int32:x) { pass x + 1i32; };")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append("    int32:r = raw 5i32 |> double |> inc;")
        lines.append("    if (r != 11i32) { exit 1; }")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_struct(self) -> str:
        """Struct declaration, instantiation, and field access."""
        fields = random.choice([
            [('int32', 'x'), ('int32', 'y')],
            [('float64', 'width'), ('float64', 'height')],
            [('int64', 'id'), ('int32', 'count'), ('bool', 'active')],
        ])
        sname = f"TestStruct_{self._func_name()}"
        lines = []
        lines.append(f"struct:{sname} = {{")
        for ftype, fname in fields:
            lines.append(f"    {ftype}:{fname};")
        lines.append("}; ")
        lines.append("")
        lines.append("func:main = int32() {")
        # Construct an instance with literal values
        vals = []
        for ftype, fname in fields:
            if ftype == 'int32':
                vals.append(f"{random.randint(1,100)}i32")
            elif ftype == 'int64':
                vals.append(f"{random.randint(1,100)}i64")
            elif ftype in ('float64', 'flt64'):
                vals.append(f"{random.uniform(1,100):.2f}")
            elif ftype == 'bool':
                vals.append(random.choice(['true', 'false']))
            else:
                vals.append("0i32")
        lines.append(f"    {sname}:s = {{ {', '.join(vals)} }};")
        # Access each field
        for ftype, fname in fields:
            v = self._var()
            lines.append(f"    {ftype}:{v} = s.{fname};")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_enum(self) -> str:
        """Enum declaration and pick-based dispatch."""
        n_values = random.randint(3, 6)
        ename = f"Color_{self._func_name()}"
        names = ['RED', 'GREEN', 'BLUE', 'YELLOW', 'CYAN', 'MAGENTA'][:n_values]
        lines = []
        lines.append(f"enum:{ename} = {{")
        for i, name in enumerate(names):
            sep = ',' if i < len(names) - 1 else ''
            lines.append(f"    {name} = {i}{sep}")
        lines.append("}; ")
        lines.append("")
        lines.append("func:main = int32() {")
        pick_val = random.randint(0, n_values - 1)
        lines.append(f"    int32:val = {pick_val}i32;")
        lines.append(f"    int32:result = 0i32;")
        lines.append(f"    pick (val) {{")
        for i, name in enumerate(names):
            lines.append(f"        ({i}i32) {{ result = {(i+1)*10}i32; }},")
        lines.append(f"        (*) {{}}")
        lines.append(f"    }}")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_array(self) -> str:
        """Array declaration, indexing with literals and $ in till."""
        size = random.choice([3, 4, 5, 8])
        lines = []
        lines.append("func:main = int32() {")
        vals = [f"{random.randint(1,50)}i32" for _ in range(size)]
        lines.append(f"    int32[{size}]:arr = [{', '.join(vals)}];")
        lines.append(f"    int32:sum = 0i32;")
        lines.append(f"    till({size}, 1) {{")
        lines.append(f"        sum = sum + arr[$];")
        lines.append(f"    }}")
        # Also test direct literal index
        lines.append(f"    int32:first = arr[0];")
        lines.append(f"    int32:last = arr[{size-1}];")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_pointer(self) -> str:
        """Pointer declaration with alloc/free (alloc returns int8@)."""
        size = random.choice([8, 16, 32, 64])
        lines = []
        lines.append("func:main = int32() {")
        lines.append(f"    int64:size = {size}i64;")
        lines.append("    wild int8->:ptr = alloc(size);")
        lines.append("    defer { free(ptr); }")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_loop_range(self) -> str:
        """loop(start, end, step) with $ variable and direct array indexing."""
        start = random.randint(0, 2)
        end = random.randint(start + 3, start + 8)
        step = random.choice([1, 2])
        lines = []
        lines.append("func:main = int32() {")
        lines.append(f"    int64:total = 0i64;")
        lines.append(f"    loop({start}, {end}, {step}) {{")
        lines.append(f"        int64:i = $;")
        lines.append(f"        total = total + i;")
        lines.append(f"    }}")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_runtime_check(self) -> str:
        """Generate a program with a deterministic, verifiable exit code.

        The exit code is embedded in a comment on line 1: // EXPECT_EXIT: N
        This lets an external harness compile, run, and verify correctness.
        """
        variant = random.choice([
            'arithmetic',
            'pipeline_chain',
            'cast_chain',
            'loop_sum',
            'pick_value',
            'conditional',
        ])

        if variant == 'arithmetic':
            a = random.randint(1, 30)
            b = random.randint(1, 30)
            expected = a + b
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:main = int32() {{\n"
                f"    int32:a = {a}i32;\n"
                f"    int32:b = {b}i32;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'pipeline_chain':
            # 5 |> double |> inc = 11
            start = random.randint(1, 10)
            expected = start * 2 + 1
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:double = int32(int32:x) {{ pass x * 2i32; }};\n"
                f"func:inc = int32(int32:x) {{ pass x + 1i32; }};\n\n"
                f"func:main = int32() {{\n"
                f"    int32:r = raw {start}i32 |> double |> inc;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'cast_chain':
            val = random.randint(1, 100)
            expected = val
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:main = int32() {{\n"
                f"    int8:a = {val}i8;\n"
                f"    int64:b = a => int64;\n"
                f"    int32:c = b => int32;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'loop_sum':
            n = random.randint(3, 10)
            expected = sum(range(1, n + 1))  # for 1..n is inclusive [1, n]
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:main = int32() {{\n"
                f"    int32:sum = 0i32;\n"
                f"    for (int32:i in 1..{n}) {{\n"
                f"        sum = sum + i;\n"
                f"    }}\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'pick_value':
            target = random.randint(1, 3)
            values = {1: 10, 2: 20, 3: 30}
            expected = values[target]
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:main = int32() {{\n"
                f"    int32:x = {target}i32;\n"
                f"    int32:result = 0i32;\n"
                f"    pick (x) {{\n"
                f"        (1i32) {{ result = 10i32; }},\n"
                f"        (2i32) {{ result = 20i32; }},\n"
                f"        (3i32) {{ result = 30i32; }},\n"
                f"        (*) {{}}\n"
                f"    }}\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'conditional':
            a = random.randint(1, 50)
            b = random.randint(1, 50)
            expected = a if a > b else b  # max(a,b)
            prog = (
                f"// EXPECT_EXIT: {expected}\n"
                f"func:main = int32() {{\n"
                f"    int32:a = {a}i32;\n"
                f"    int32:b = {b}i32;\n"
                f"    int32:result = 0i32;\n"
                f"    if (a > b) {{\n"
                f"        result = a;\n"
                f"    }} else {{\n"
                f"        result = b;\n"
                f"    }}\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        return prog

    def _gen_syscall(self) -> str:
        """Test sys() tiered syscall system — safe, full, and raw tiers."""
        # Only use syscalls that are safe to execute in a fuzzer environment
        safe_noarg = ['GETPID', 'GETPPID', 'GETTID', 'GETUID', 'GETGID', 'GETEUID', 'GETEGID']
        full_noarg = ['GETPID', 'GETPPID', 'GETTID', 'GETUID', 'GETGID']

        variant = random.choice([
            'safe_noarg',
            'safe_write',
            'safe_multi',
            'full_noarg',
            'raw_noarg',
            'result_unwrap',
            'wrapper_func',
        ])

        if variant == 'safe_noarg':
            sc = random.choice(safe_noarg)
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f"    Result<int64>:r = sys({sc});\n"
                f"    int64:val = r ? -1i64;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'safe_write':
            msg = f"fuzz_{random.randint(0, 9999)}"
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f'    Result<int64>:w = sys(WRITE, 1i64, "{msg}\\n", {len(msg) + 1}i64);\n'
                f"    int64:bytes = w ? 0i64;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'safe_multi':
            calls = random.sample(safe_noarg, min(3, len(safe_noarg)))
            body = ""
            for i, sc in enumerate(calls):
                body += f"    Result<int64>:r{i} = sys({sc});\n"
                body += f"    int64:v{i} = r{i} ? -1i64;\n"
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f"{body}"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'full_noarg':
            sc = random.choice(full_noarg)
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f"    Result<int64>:r = sys!!({sc});\n"
                f"    int64:val = r ? -1i64;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'raw_noarg':
            sc = random.choice(full_noarg)
            # For raw tier, use the syscall number directly
            syscall_numbers = {
                'GETPID': 39, 'GETPPID': 110, 'GETTID': 186,
                'GETUID': 102, 'GETGID': 104,
            }
            nr = syscall_numbers[sc]
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f"    int64:val = sys!!!({nr}i64);\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'result_unwrap':
            sc = random.choice(safe_noarg)
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:main = int32() {{\n"
                f"    Result<int64>:r = sys({sc});\n"
                f"    int64:val = r ? -1i64;\n"
                f"    if (val > 0i64) {{\n"
                f'        drop println("ok");\n'
                f"    }}\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        elif variant == 'wrapper_func':
            sc = random.choice(safe_noarg)
            prog = (
                f'use "sys.aria".*;\n\n'
                f"func:get_val = int64() {{\n"
                f"    Result<int64>:r = sys({sc});\n"
                f"    int64:val = r ? -1i64;\n"
                f"    pass val;\n"
                f"}};\n\n"
                f"func:main = int32() {{\n"
                f"    Result<int64>:v = get_val();\n"
                f"    int64:result = v ? 0i64;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:failsafe = int32(tbb32:err) {{ exit 1; }};\n"
            )

        return prog

    def _gen_tbb_sticky(self) -> str:
        """Test TBB sticky error semantics: ERR propagates through arithmetic."""
        # ERR sentinels are one below usable min; suffixes needed for literals
        # 0tbb conflicts with ternary prefix 0t, so use bare 0 for zero
        tbb_types = [
            ('tbb8',  '-128',      '127',        'tbb8'),
            ('tbb16', '-32768',    '32767',       'tbb16'),
            ('tbb32', '-2147483648','2147483647', 'tbb32'),
        ]
        tname, err_val, max_val, suf = random.choice(tbb_types)
        variant = random.choice([
            'err_propagation',   # ERR + x = ERR
            'overflow_to_err',   # max + 1 = ERR
            'err_comparison',    # ERR == ERR
            'err_chain',         # chain of ops on ERR
            'mixed_ops',         # mix of normal and ERR values
        ])
        lines = []
        lines.append("func:main = int32() {")

        if variant == 'err_propagation':
            lines.append(f"    {tname}:err = {err_val};")
            lines.append(f"    {tname}:normal = 1{suf};")
            lines.append(f"    {tname}:r1 = err + normal;")
            lines.append(f"    {tname}:r2 = err - normal;")
            lines.append(f"    {tname}:r3 = err * normal;")
            lines.append(f"    bool:still_err1 = (r1 == err);")
            lines.append(f"    bool:still_err2 = (r2 == err);")
            lines.append(f"    bool:still_err3 = (r3 == err);")

        elif variant == 'overflow_to_err':
            lines.append(f"    {tname}:err_sentinel = {err_val};")
            lines.append(f"    {tname}:m = {max_val}{suf};")
            lines.append(f"    {tname}:result = m + 1{suf};")
            lines.append(f"    bool:is_err = (result == err_sentinel);")

        elif variant == 'err_comparison':
            lines.append(f"    {tname}:e1 = {err_val};")
            lines.append(f"    {tname}:e2 = {err_val};")
            lines.append(f"    bool:same = (e1 == e2);")
            lines.append(f"    bool:ne = (e1 != e2);")

        elif variant == 'err_chain':
            lines.append(f"    {tname}:val = {err_val};")
            n = random.randint(3, 6)
            for i in range(n):
                lines.append(f"    val = val + 1{suf};")
            lines.append(f"    {tname}:err_check = {err_val};")
            lines.append(f"    bool:still_err = (val == err_check);")

        elif variant == 'mixed_ops':
            a = random.randint(1, 50)
            b = random.randint(1, 50)
            lines.append(f"    {tname}:a = {a}{suf};")
            lines.append(f"    {tname}:b = {b}{suf};")
            lines.append(f"    {tname}:s = a + b;")
            lines.append(f"    {tname}:d = a - b;")
            lines.append(f"    {tname}:p = a * b;")
            lines.append(f"    bool:cmp = (s > d);")

        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_string_interp(self) -> str:
        """String interpolation with backtick templates and &{expr}."""
        variant = random.choice([
            'simple_var',        # `x = &{x}`
            'expression',        # `result = &{sum}`
            'multi_interp',      # Multiple &{} in one string
            'nested_call',       # Interpolation inside loop ($ via temp var)
            'no_interp',         # Plain backtick string (no &{})
        ])
        lines = []
        lines.append("func:main = int32() {")

        if variant == 'simple_var':
            lines.append("    int32:x = 42i32;")
            lines.append("    println(`x = &{x}`);")

        elif variant == 'expression':
            lines.append("    int32:a = 10i32;")
            lines.append("    int32:b = 20i32;")
            lines.append("    int32:sum = a + b;")
            lines.append("    println(`sum = &{sum}`);")

        elif variant == 'multi_interp':
            lines.append("    int32:x = 1i32;")
            lines.append("    int32:y = 2i32;")
            lines.append("    println(`x=&{x} y=&{y}`);")

        elif variant == 'nested_call':
            # Can't use &{$} directly — the } closes the interp block.
            # Assign $ to a temp variable first.
            n = random.randint(3, 5)
            lines.append(f"    till({n}, 1) {{")
            lines.append("        int64:idx = $;")
            lines.append("        println(`i = &{idx}`);")
            lines.append("    }")

        elif variant == 'no_interp':
            lines.append("    println(`hello world`);")

        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_multi_struct(self) -> str:
        """Multiple structs in one program, including cross-references."""
        variant = random.choice([
            'two_independent',    # Two unrelated structs
            'struct_in_func',     # Pass struct to function
            'struct_return',      # Return struct from function (via fields)
            'struct_array_field', # Struct with array field
        ])
        lines = []

        if variant == 'two_independent':
            s1 = f"Point_{self._func_name()}"
            s2 = f"Size_{self._func_name()}"
            lines.append(f"struct:{s1} = {{")
            lines.append(f"    int32:x;")
            lines.append(f"    int32:y;")
            lines.append(f"}}; ")
            lines.append("")
            lines.append(f"struct:{s2} = {{")
            lines.append(f"    int32:w;")
            lines.append(f"    int32:h;")
            lines.append(f"}}; ")
            lines.append("")
            lines.append("func:main = int32() {")
            lines.append(f"    {s1}:p = {{ {random.randint(1,100)}i32, {random.randint(1,100)}i32 }};")
            lines.append(f"    {s2}:s = {{ {random.randint(1,100)}i32, {random.randint(1,100)}i32 }};")
            lines.append(f"    int32:area = s.w * s.h;")
            lines.append(f"    int32:dist = p.x + p.y;")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif variant == 'struct_in_func':
            sname = f"Pair_{self._func_name()}"
            fname = f"sum_pair_{self._func_name()}"
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:a;")
            lines.append(f"    int32:b;")
            lines.append(f"}}; ")
            lines.append("")
            lines.append(f"func:{fname} = int32({sname}:p) {{")
            lines.append(f"    pass p.a + p.b;")
            lines.append(f"}};")
            lines.append("")
            a, b = random.randint(1, 50), random.randint(1, 50)
            lines.append("func:main = int32() {")
            lines.append(f"    {sname}:pair = {{ {a}i32, {b}i32 }};")
            lines.append(f"    int32:result = {fname}(pair) ? -1i32;")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif variant == 'struct_return':
            # Can't return structs directly yet, so compute from fields
            sname = f"Rect_{self._func_name()}"
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:x;")
            lines.append(f"    int32:y;")
            lines.append(f"    int32:w;")
            lines.append(f"    int32:h;")
            lines.append(f"}}; ")
            lines.append("")
            x, y = random.randint(0, 50), random.randint(0, 50)
            w, h = random.randint(1, 100), random.randint(1, 100)
            lines.append("func:main = int32() {")
            lines.append(f"    {sname}:r = {{ {x}i32, {y}i32, {w}i32, {h}i32 }};")
            lines.append(f"    int32:area = r.w * r.h;")
            lines.append(f"    int32:right = r.x + r.w;")
            lines.append(f"    int32:bottom = r.y + r.h;")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif variant == 'struct_array_field':
            sname = f"Buffer_{self._func_name()}"
            size = random.choice([3, 4, 5])
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:size;")
            lines.append(f"    int32[{size}]:data;")
            lines.append(f"}}; ")
            lines.append("")
            vals = [f"{random.randint(1, 50)}i32" for _ in range(size)]
            lines.append("func:main = int32() {")
            lines.append(f"    {sname}:buf = {{ {size}i32, [{', '.join(vals)}] }};")
            lines.append(f"    int32:sum = 0i32;")
            lines.append(f"    till({size}, 1) {{")
            lines.append(f"        sum = sum + buf.data[$];")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_combo(self) -> str:
        """Combine 2-3 features in one program to test interactions.

        Each combo variant uses building blocks from other strategies,
        assembled into a single program with a helper function and main.
        Only uses patterns known to compile individually.
        """
        combos = [
            'struct_pick',        # Struct fields dispatched via pick
            'array_loop_result',  # Array + loop + pass/fail
            'defer_cast_when',    # Defer + cast + when
            'pipeline_struct',    # Pipeline through functions using struct
            'array_pick_defer',   # Array indexing + pick + defer
            'func_cast_loop',     # Function calls + cast + for-range loop
            'struct_array',       # Struct containing logic + array iteration
            'when_result_cast',   # When + result unwrap + cast
            'optional_coalesce_pick',  # Optional ?? + pick dispatch
            'loop_pipeline_defer',     # Loop + pipeline + defer
        ]
        combo = random.choice(combos)

        lines = []

        if combo == 'struct_pick':
            sname = f"Item_{self._func_name()}"
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:kind;")
            lines.append(f"    int32:value;")
            lines.append(f"}}; ")
            lines.append("")
            lines.append("func:main = int32() {")
            kind = random.randint(1, 3)
            val = random.randint(10, 99)
            lines.append(f"    {sname}:item = {{ {kind}i32, {val}i32 }};")
            lines.append(f"    int32:result = 0i32;")
            lines.append(f"    pick (item.kind) {{")
            lines.append(f"        (1i32) {{ result = item.value + 100i32; }},")
            lines.append(f"        (2i32) {{ result = item.value + 200i32; }},")
            lines.append(f"        (3i32) {{ result = item.value + 300i32; }},")
            lines.append(f"        (*) {{ result = -1i32; }}")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'array_loop_result':
            fname = f"check_{self._func_name()}"
            lines.append(f"func:{fname} = int32(int32:x) {{")
            lines.append(f"    if (x < 0i32) {{ fail x; }}")
            lines.append(f"    pass x * 2i32;")
            lines.append(f"}};")
            lines.append("")
            size = random.choice([3, 4, 5])
            vals = [f"{random.randint(1, 50)}i32" for _ in range(size)]
            lines.append("func:main = int32() {")
            lines.append(f"    int32[{size}]:arr = [{', '.join(vals)}];")
            lines.append(f"    int32:total = 0i32;")
            lines.append(f"    till({size}, 1) {{")
            lines.append(f"        int32:doubled = {fname}(arr[$]) ? 0i32;")
            lines.append(f"        total = total + doubled;")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'defer_cast_when':
            lines.append("func:main = int32() {")
            lines.append("    int32:cleanup = 0i32;")
            lines.append("    defer { cleanup = cleanup + 1i32; }")
            lines.append("    int8:small = 10i8;")
            lines.append("    int32:big = small => int32;")
            lines.append("    int32:counter = 0i32;")
            lines.append("    when (counter < big) {")
            lines.append("        counter = counter + 1i32;")
            lines.append("    } then {")
            lines.append("        big = counter;")
            lines.append("    } end {")
            lines.append("        big = -1i32;")
            lines.append("    }")
            lines.append("    exit 0;")
            lines.append("};")

        elif combo == 'pipeline_struct':
            sname = f"Pair_{self._func_name()}"
            fname1 = f"get_val_{self._func_name()}"
            fname2 = f"double_{self._func_name()}"
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:x;")
            lines.append(f"    int32:y;")
            lines.append(f"}}; ")
            lines.append("")
            lines.append(f"func:{fname1} = int32(int32:n) {{")
            lines.append(f"    pass n + 1i32;")
            lines.append(f"}};")
            lines.append("")
            lines.append(f"func:{fname2} = int32(int32:n) {{")
            lines.append(f"    pass n * 2i32;")
            lines.append(f"}};")
            lines.append("")
            val = random.randint(1, 20)
            lines.append("func:main = int32() {")
            lines.append(f"    int32:a = raw {val}i32 |> {fname1} |> {fname2};")
            lines.append(f"    {sname}:p = {{ a, a + 1i32 }};")
            lines.append(f"    int32:sum = p.x + p.y;")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'array_pick_defer':
            size = random.choice([3, 4, 5])
            vals = [f"{random.randint(1, 50)}i32" for _ in range(size)]
            lines.append("func:main = int32() {")
            lines.append(f"    int32[{size}]:data = [{', '.join(vals)}];")
            lines.append(f"    int32:freed = 0i32;")
            lines.append(f"    defer {{ freed = 1i32; }}")
            lines.append(f"    int32:first = data[0];")
            lines.append(f"    int32:result = 0i32;")
            lines.append(f"    pick (first) {{")
            for v in range(1, 51, 10):
                lines.append(f"        ({v}i32) {{ result = {v * 10}i32; }},")
            lines.append(f"        (*) {{ result = first; }}")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'func_cast_loop':
            fname = f"widen_{self._func_name()}"
            lines.append(f"func:{fname} = int64(int32:x) {{")
            lines.append(f"    pass x => int64;")
            lines.append(f"}};")
            lines.append("")
            n = random.randint(3, 6)
            lines.append("func:main = int32() {")
            lines.append(f"    int64:total = 0i64;")
            lines.append(f"    for (int32:i in 1..{n}) {{")
            lines.append(f"        int64:w = {fname}(i) ? 0i64;")
            lines.append(f"        total = total + w;")
            lines.append(f"    }}")
            lines.append(f"    int32:result = total => int32;")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'struct_array':
            sname = f"Counter_{self._func_name()}"
            lines.append(f"struct:{sname} = {{")
            lines.append(f"    int32:count;")
            lines.append(f"    int32:max;")
            lines.append(f"}}; ")
            lines.append("")
            size = random.choice([3, 4, 5])
            vals = [f"{random.randint(1, 20)}i32" for _ in range(size)]
            lines.append("func:main = int32() {")
            lines.append(f"    {sname}:c = {{ 0i32, {size}i32 }};")
            lines.append(f"    int32[{size}]:items = [{', '.join(vals)}];")
            lines.append(f"    int32:sum = 0i32;")
            lines.append(f"    till({size}, 1) {{")
            lines.append(f"        sum = sum + items[$];")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'when_result_cast':
            fname = f"safe_div_{self._func_name()}"
            lines.append(f"func:{fname} = int32(int32:a, int32:b) {{")
            lines.append(f"    if (b == 0i32) {{ fail -1i32; }}")
            lines.append(f"    pass a / b;")
            lines.append(f"}};")
            lines.append("")
            lines.append("func:main = int32() {")
            lines.append(f"    int32:val = {fname}(10i32, 2i32) ? -1i32;")
            lines.append(f"    int64:wide = val => int64;")
            lines.append(f"    int32:counter = 0i32;")
            lines.append(f"    when (counter < val) {{")
            lines.append(f"        counter = counter + 1i32;")
            lines.append(f"    }} then {{")
            lines.append(f"        wide = counter => int64;")
            lines.append(f"    }} end {{")
            lines.append(f"        wide = -1i64;")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'optional_coalesce_pick':
            lines.append("func:main = int32() {")
            base_val = random.randint(1, 3)
            lines.append(f"    int32?:maybe = {base_val}i32;")
            lines.append(f"    int32:val = maybe ?? 0i32;")
            lines.append(f"    int32:result = 0i32;")
            lines.append(f"    pick (val) {{")
            lines.append(f"        (1i32) {{ result = 10i32; }},")
            lines.append(f"        (2i32) {{ result = 20i32; }},")
            lines.append(f"        (3i32) {{ result = 30i32; }},")
            lines.append(f"        (*) {{ result = -1i32; }}")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        elif combo == 'loop_pipeline_defer':
            fname1 = f"inc_{self._func_name()}"
            fname2 = f"sq_{self._func_name()}"
            lines.append(f"func:{fname1} = int32(int32:x) {{ pass x + 1i32; }};")
            lines.append(f"func:{fname2} = int32(int32:x) {{ pass x * x; }};")
            lines.append("")
            n = random.randint(3, 5)
            lines.append("func:main = int32() {")
            lines.append(f"    int32:sum = 0i32;")
            lines.append(f"    defer {{ sum = sum + 0i32; }}")
            lines.append(f"    for (int32:i in 1..{n}) {{")
            lines.append(f"        int32:r = raw i |> {fname1} |> {fname2};")
            lines.append(f"        sum = sum + r;")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append(f"}};")

        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_type_coercion(self) -> str:
        """Test cast (=>) between random type pairs to probe the coercion matrix."""
        # Types known to support => casts between each other
        castable = [
            ('int8', '42i8'), ('int16', '42i16'), ('int32', '42i32'), ('int64', '42i64'),
            ('uint8', '42u8'), ('uint16', '42u16'), ('uint32', '42u32'), ('uint64', '42u64'),
            ('flt32', '42.0f32'), ('flt64', '42.0'),
        ]
        # Pick 3-5 random types and chain casts through them
        n = random.randint(3, 5)
        chain = random.sample(castable, n)
        lines = []
        lines.append("func:main = int32() {")
        src_type, src_lit = chain[0]
        lines.append(f"    {src_type}:v0 = {src_lit};")
        for i in range(1, len(chain)):
            dst_type, _ = chain[i]
            lines.append(f"    {dst_type}:v{i} = v{i-1} => {dst_type};")
        # Cast final back to int32 for return
        last = len(chain) - 1
        last_type = chain[last][0]
        if last_type != 'int32':
            lines.append(f"    int32:result = v{last} => int32;")
        else:
            lines.append(f"    int32:result = v{last};")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_large_type(self) -> str:
        """Arithmetic chains on 128-4096 bit LBIM types to stress the backend."""
        large_ints = [
            ('int128', 'i128'), ('int256', 'i256'), ('int512', 'i512'),
            ('int1024', 'i1024'), ('int2048', 'i2048'), ('int4096', 'i4096'),
            ('uint128', 'u128'), ('uint256', 'u256'), ('uint512', 'u512'),
            ('uint1024', 'u1024'), ('uint2048', 'u2048'), ('uint4096', 'u4096'),
        ]
        tname, suf = random.choice(large_ints)
        val_a = random.randint(1000, 999999)
        val_b = random.randint(1, 999)
        lines = []
        lines.append("func:main = int32() {")
        lines.append(f"    {tname}:a = {val_a}{suf};")
        lines.append(f"    {tname}:b = {val_b}{suf};")
        lines.append(f"    {tname}:sum = a + b;")
        lines.append(f"    {tname}:diff = a - b;")
        lines.append(f"    {tname}:prod = a * b;")
        lines.append(f"    bool:eq = (a == b);")
        lines.append(f"    bool:lt = (a < b);")
        lines.append(f"    bool:gt = (a > b);")
        # Chain: result = (a + b) - (a - b) = 2*b
        lines.append(f"    {tname}:chain = sum - diff;")
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_recursive(self) -> str:
        """Recursive functions: factorial or fibonacci."""
        variant = random.choice(['factorial', 'fibonacci', 'countdown'])
        lines = []

        if variant == 'factorial':
            fname = f"fact_{self._func_name()}"
            lines.append(f"func:{fname} = int64(int64:n) {{")
            lines.append(f"    if (n <= 1i64) {{ pass 1i64; }}")
            lines.append(f"    int64:sub = {fname}(n - 1i64) ? 1i64;")
            lines.append(f"    pass n * sub;")
            lines.append(f"}};")
            lines.append("")
            n = random.randint(3, 10)
            lines.append("func:main = int32() {")
            lines.append(f"    int64:r = {fname}({n}i64) ? -1i64;")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'fibonacci':
            fname = f"fib_{self._func_name()}"
            lines.append(f"func:{fname} = int64(int64:n) {{")
            lines.append(f"    if (n <= 0i64) {{ pass 0i64; }}")
            lines.append(f"    if (n == 1i64) {{ pass 1i64; }}")
            lines.append(f"    int64:a = {fname}(n - 1i64) ? 0i64;")
            lines.append(f"    int64:b = {fname}(n - 2i64) ? 0i64;")
            lines.append(f"    pass a + b;")
            lines.append(f"}};")
            lines.append("")
            n = random.randint(3, 12)
            lines.append("func:main = int32() {")
            lines.append(f"    int64:r = {fname}({n}i64) ? -1i64;")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'countdown':
            fname = f"count_{self._func_name()}"
            lines.append(f"func:{fname} = int32(int32:n) {{")
            lines.append(f"    if (n <= 0i32) {{ exit 0; }}")
            lines.append(f"    int32:sub = {fname}(n - 1i32) ? 0i32;")
            lines.append(f"    pass sub + 1i32;")
            lines.append(f"}};")
            lines.append("")
            n = random.randint(5, 20)
            lines.append("func:main = int32() {")
            lines.append(f"    int32:r = {fname}({n}i32) ? -1i32;")
            lines.append("    exit 0;")
            lines.append("};")

        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'

    def _gen_negative(self) -> str:
        """Generate intentionally INVALID programs to test compiler error handling.

        The compiler should reject these with an error, NOT crash or hang.
        A crash (signal) or timeout on any of these is a bug.
        """
        variants = [
            'type_mismatch',     # Assign wrong type to variable
            'undeclared_var',    # Use a variable never declared
            'missing_failsafe',  # No failsafe function
            'wrong_arg_count',   # Call function with wrong number of args
            'wrong_arg_type',    # Call function with wrong argument type
            'bad_return_type',   # Pass wrong type from function
            'double_decl',       # Declare same variable twice
            'bad_pick_type',     # Pick on mismatched arm types
            'bad_array_index',   # Array with wrong index type
            'missing_semicolon', # Omit semicolons
        ]
        variant = random.choice(variants)

        if variant == 'type_mismatch':
            return (
                "func:main = int32() {\n"
                '    int32:x = "hello";\n'
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'undeclared_var':
            return (
                "func:main = int32() {\n"
                "    int32:x = never_declared + 1i32;\n"
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'missing_failsafe':
            return (
                "func:main = int32() {\n"
                "    exit 0;\n"
                "};\n"
            )

        elif variant == 'wrong_arg_count':
            fname = f"takes_one_{self._func_name()}"
            return (
                f"func:{fname} = int32(int32:x) {{\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:main = int32() {{\n"
                f"    int32:r = {fname}(1i32, 2i32) ? -1i32;\n"
                f"    exit 0;\n"
                f"}};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'wrong_arg_type':
            fname = f"wants_int_{self._func_name()}"
            return (
                f"func:{fname} = int32(int32:x) {{\n"
                f"    exit 0;\n"
                f"}};\n\n"
                f"func:main = int32() {{\n"
                f'    int32:r = {fname}("oops") ? -1i32;\n'
                f"    exit 0;\n"
                f"}};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'bad_return_type':
            return (
                "func:main = int32() {\n"
                '    exit 0;\n'
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'double_decl':
            return (
                "func:main = int32() {\n"
                "    int32:x = 1i32;\n"
                "    int32:x = 2i32;\n"
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'bad_pick_type':
            return (
                "func:main = int32() {\n"
                '    string:s = "hello";\n'
                "    pick (s) {\n"
                "        (1i32) { exit 1; },\n"
                "        (*) {}\n"
                "    }\n"
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'bad_array_index':
            return (
                "func:main = int32() {\n"
                "    int32[3]:arr = [1i32, 2i32, 3i32];\n"
                '    int32:x = arr["zero"];\n'
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        elif variant == 'missing_semicolon':
            return (
                "func:main = int32() {\n"
                "    int32:x = 5i32\n"
                "    exit 0;\n"
                "};\n\n"
                "func:failsafe = int32(tbb32:err) { exit 1; };\n"
            )

        return self._gen_minimal()  # fallback

    def _gen_optional_type(self) -> str:
        """Probe optional type (T?) patterns to find compiler crashes.

        The compiler is known to segfault on `int64?:x = NIL;` during codegen.
        This strategy generates various optional patterns to map the full crash
        surface: which types crash, which operations crash, and which are safe.
        """
        # Pick a sub-variant to test
        variant = random.choice([
            'nil_decl',         # T?:x = NIL  (known crash)
            'value_decl',       # T?:x = <value>
            'coalesce',         # x ?? default
            'nil_compare',      # x == NIL / x != NIL
            'multi_optional',   # Multiple optional vars
            'optional_param',   # Optional function parameter
            'optional_return',  # Function returning optional
            'optional_control', # Optional in if/pick
            'reassign_nil',     # Assign value then reassign NIL
            'coalesce_chain',   # Chained ?? operators
        ])

        # Pick a random base type for the optional
        base_types = [
            ('int32', '42i32', '0i32'),
            ('int64', '100i64', '0i64'),
            ('int8', '7i8', '0i8'),
            ('uint32', '55u32', '0u32'),
            ('uint64', '99u64', '0u64'),
            ('flt64', '3.14', '0.0'),
            ('bool', 'true', 'false'),
        ]
        base_type, sample_val, zero_val = random.choice(base_types)

        lines = []

        if variant == 'nil_decl':
            # Simplest crash trigger: declare optional with NIL
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:x = NIL;")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'value_decl':
            # Optional with an actual value (may or may not crash)
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:x = {sample_val};")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'coalesce':
            # Null coalescing — ?? is reported to work
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:maybe = NIL;")
            lines.append(f"    {base_type}:val = maybe ?? {sample_val};")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'nil_compare':
            # NIL comparison — reportedly works
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:opt = {sample_val};")
            lines.append(f"    if (opt == NIL) {{")
            lines.append(f"        exit 1;")
            lines.append(f"    }}")
            lines.append(f"    if (opt != NIL) {{")
            lines.append(f"        exit 0;")
            lines.append(f"    }}")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'multi_optional':
            # Multiple optional declarations in one function
            lines.append("func:main = int32() {")
            for i, (bt, sv, zv) in enumerate(random.sample(base_types, min(4, len(base_types)))):
                if i % 2 == 0:
                    lines.append(f"    {bt}?:opt_{i} = NIL;")
                else:
                    lines.append(f"    {bt}?:opt_{i} = {sv};")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'optional_param':
            # Function taking an optional parameter
            fname = f"takes_opt_{self._func_name()}"
            lines.append(f"func:{fname} = int32({base_type}?:x) {{")
            lines.append(f"    if (x == NIL) {{ pass -1i32; }}")
            lines.append(f"    exit 0;")
            lines.append("};")
            lines.append("")
            lines.append("func:main = int32() {")
            lines.append(f"    int32:r = {fname}(NIL) ? -1i32;")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'optional_return':
            # Function returning an optional type
            fname = f"maybe_val_{self._func_name()}"
            lines.append(f"func:{fname} = {base_type}?(int32:flag) {{")
            lines.append(f"    if (flag == 0i32) {{ pass NIL; }}")
            lines.append(f"    pass {sample_val};")
            lines.append("};")
            lines.append("")
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:result = raw {fname}(1i32);")
            lines.append(f"    {base_type}:val = result ?? {zero_val};")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'optional_control':
            # Optional in control flow
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:opt = {sample_val};")
            lines.append(f"    {base_type}:result = {zero_val};")
            lines.append(f"    if (opt != NIL) {{")
            lines.append(f"        result = opt ?? {zero_val};")
            lines.append(f"    }}")
            lines.append(f"    exit 0;")
            lines.append("};")

        elif variant == 'reassign_nil':
            # Assign a value, then reassign to NIL
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:x = {sample_val};")
            lines.append(f"    x = NIL;")
            lines.append(f"    {base_type}:val = x ?? {zero_val};")
            lines.append("    exit 0;")
            lines.append("};")

        elif variant == 'coalesce_chain':
            # Chained null coalescing
            lines.append("func:main = int32() {")
            lines.append(f"    {base_type}?:a = NIL;")
            lines.append(f"    {base_type}?:b = NIL;")
            lines.append(f"    {base_type}?:c = {sample_val};")
            lines.append(f"    {base_type}:val = a ?? b ?? c ?? {zero_val};")
            lines.append("    exit 0;")
            lines.append("};")

        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    # ------------------------------------------------------------------
    # Expression helpers
    # ------------------------------------------------------------------
    
    def _gen_expression(self, typ: PrimitiveType, depth: int = 0, max_depth: int = 3) -> str:
        if depth >= max_depth or random.random() < 0.3:
            return self._lit(typ)
        bin_ops = [op for op in self.operators.get_binary_operators(typ.category)
                   if op.symbol not in ('/', '%', '==', '!=', '<', '>', '<=', '>=')]
        if not bin_ops:
            return self._lit(typ)
        op = random.choice(bin_ops)
        left = self._gen_expression(typ, depth + 1, max_depth)
        right = self._gen_expression(typ, depth + 1, max_depth)
        return f"({left} {op.symbol} {right})"
    
    def _lit(self, typ: PrimitiveType, val: Optional[int] = None) -> str:
        """Generate a literal for the type, optionally with a specific value."""
        suf = typ.literal_suffix()
        
        if typ.category in (TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB):
            if val is None:
                if typ.min_value is not None and typ.max_value is not None:
                    safe_min = max(typ.min_value, -1000)
                    safe_max = min(typ.max_value, 1000)
                    # Bias toward interesting boundaries
                    r = random.random()
                    if r < 0.15 and typ.min_value >= safe_min:
                        val = max(typ.min_value + (1 if typ.category == TypeCategory.TBB else 0), 0 if typ.category == TypeCategory.UNSIGNED else typ.min_value)
                    elif r < 0.3 and typ.max_value <= safe_max:
                        val = typ.max_value
                    elif r < 0.4:
                        val = 0
                    elif r < 0.5:
                        val = 1
                    else:
                        val = random.randint(max(safe_min, 0 if typ.category == TypeCategory.UNSIGNED else safe_min), safe_max)
                else:
                    val = random.randint(1, 100)
            # Clamp to type range if known
            if typ.min_value is not None and val < typ.min_value:
                val = typ.min_value + (1 if typ.category == TypeCategory.TBB else 0)
            if typ.max_value is not None and val > typ.max_value:
                val = typ.max_value
            # Ensure unsigned types don't get negative literals
            if typ.category == TypeCategory.UNSIGNED and val < 0:
                val = abs(val) if abs(val) <= (typ.max_value or 1000) else 0
            # TBB zero-value workaround: 0tbb8 is lexed as 0t (ternary prefix)
            if typ.category == TypeCategory.TBB and val == 0:
                return "0"
            return f"{val}{suf}" if suf else str(val)
        
        elif typ.category == TypeCategory.FLOAT:
            if val is None:
                val_f = random.uniform(-100.0, 100.0)
            else:
                val_f = float(val)
            return f"{val_f:.4f}{suf}"
        
        elif typ.category == TypeCategory.BOOL:
            return random.choice(['true', 'false'])
        
        elif typ.category == TypeCategory.STRING:
            return random.choice(['"hello"', '"test"', '"aria"', '""', '"x"'])
        
        else:
            # Unknown/complex type — use a small integer
            return f"{val or 1}i32"
    
    def _var(self) -> str:
        name = f"v{self.var_counter}"
        self.var_counter += 1
        return name
    
    def _func_name(self) -> str:
        name = f"fn{self.func_counter}"
        self.func_counter += 1
        return name
    
    def _random_arithmetic_type(self) -> PrimitiveType:
        """Pick a random type suitable for arithmetic in generated code."""
        candidates = [t for t in self.type_system.get_all_types()
                      if t.category in ARITHMETIC_CATEGORIES
                      and t.name not in LARGE_TYPES]
        return random.choice(candidates)
    
    def _random_integer_type(self) -> PrimitiveType:
        # Only include types with proper integer literal suffixes
        types = [t for t in
                 self.type_system.get_types_by_category(TypeCategory.INTEGER) +
                 self.type_system.get_types_by_category(TypeCategory.UNSIGNED)
                 if t.literal_suffix()]  # Exclude types with no suffix
        return random.choice(types)


class TypeExhaustiveGenerator:
    """Generates tests that exhaustively cover type × operator combinations."""
    
    def __init__(self, aria_root: Path = None):
        self.type_system = load_type_system()
        self.operators = load_operators()
    
    def generate_all_type_tests(self) -> List[Tuple[str, str]]:
        """
        Generate one test per (type, operator) pair for all numeric types.
        Returns list of (filename, content).
        """
        tests = []
        
        numeric_types = [t for t in self.type_system.get_all_types()
                         if t.category in ARITHMETIC_CATEGORIES]
        
        for typ in numeric_types:
            # Skip operators that require unsigned or can produce UB on boundaries
            bin_ops = [op for op in self.operators.get_binary_operators(typ.category)
                       if op.symbol not in ('/', '%')]
            for op in bin_ops:
                name = f"type_exhaustive_{typ.name}_{op.name}.aria"
                code = self._gen_operator_test(typ, op)
                tests.append((name, code))
        
        return tests
    
    def _gen_operator_test(self, typ: PrimitiveType, op: Operator) -> str:
        lines = []
        lines.append(f"// Auto-generated: {typ.name} {op.symbol}")
        lines.append("func:main = int32() {")
        
        a_lit = self._safe_lit(typ, 10)
        b_lit = self._safe_lit(typ, 5)
        lines.append(f"    {typ.name}:a = {a_lit};")
        lines.append(f"    {typ.name}:b = {b_lit};")
        
        if op.symbol in ('==', '!=', '<', '>', '<=', '>='):
            lines.append(f"    bool:result = (a {op.symbol} b);")
        else:
            lines.append(f"    {typ.name}:result = (a {op.symbol} b);")
        
        lines.append("    exit 0;")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = int32(tbb32:err) { exit 1; };")
        return '\n'.join(lines) + '\n'
    
    def _safe_lit(self, typ: PrimitiveType, val: int) -> str:
        suf = typ.literal_suffix()
        if typ.category == TypeCategory.FLOAT:
            return f"{float(val):.1f}{suf}"
        # Clamp to type range
        if typ.max_value is not None:
            val = min(val, typ.max_value)
        if typ.min_value is not None:
            val = max(val, typ.min_value)
        # TBB zero-value workaround: 0tbb8 is lexed as 0t (ternary prefix)
        if typ.category == TypeCategory.TBB and val == 0:
            return "0"
        return f"{val}{suf}" if suf else str(val)


if __name__ == '__main__':
    gen = AriaGenerator(seed=42)
    
    strategies = ['minimal', 'type_test', 'control_flow', 'functions',
                  'edge_case', 'pick', 'when', 'defer', 'result', 'cast', 'pipeline']
    
    for s in strategies:
        print(f"\n{'='*60}")
        print(f"Strategy: {s}")
        print('='*60)
        print(gen.generate_program(s))
    
    exhaustive = TypeExhaustiveGenerator()
    tests = exhaustive.generate_all_type_tests()
    print(f"\nExhaustive tests: {len(tests)}")

