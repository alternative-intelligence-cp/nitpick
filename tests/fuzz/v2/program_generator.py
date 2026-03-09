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
            "    pass(0i32);\n"
            "};\n"
            "\n"
            "func:failsafe = NIL(int32:err_code) { };\n"
        )
    
    def _gen_type_test(self, typ: PrimitiveType) -> str:
        """Test arithmetic operations for a given type."""
        lines = []
        v1 = self._lit(typ, 10)
        v2 = self._lit(typ, 5)
        
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
        
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_control_flow(self) -> str:
        """if/while/for/till/loop in one function."""
        typ = self._random_arithmetic_type()
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
        lines.append(f"    for (i in 1..{n}) {{")
        lines.append(f"        sum = sum + 1i32;")
        lines.append("    }")
        lines.append("")
        
        # till + $ variable
        lines.append(f"    int64:last = 0;")
        lines.append(f"    till({n}, 1) {{")
        lines.append(f"        last = $;")
        lines.append("    }")
        lines.append("")
        
        # loop + break
        lines.append(f"    int32:lc = 0i32;")
        lines.append(f"    loop {{")
        lines.append(f"        lc = lc + 1i32;")
        lines.append(f"        if (lc >= 3i32) {{ break; }}")
        lines.append("    }")
        lines.append("")
        
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_functions(self) -> str:
        """Two functions: helper returns Result<int32>, main unwraps it."""
        fname = f"helper_{self._func_name()}"
        lines = []
        
        lines.append(f"func:{fname} = Result<int32>(int32:x) {{")
        lines.append(f"    if (x < 0i32) {{ fail(1i32); }}")
        lines.append(f"    pass(x * 2i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append(f"    Result<int32>:r = {fname}(5i32);")
        lines.append(f"    int32:v = r?;")
        lines.append(f"    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_edge_case(self) -> str:
        """Boundary values for a random integer type."""
        typ = self._random_integer_type()
        lines = []
        
        lines.append("func:main = int32() {")
        
        if typ.name in LARGE_TYPES:
            # Large types: just declare with small literal
            lines.append(f"    {typ.name}:a = 1000000;")
            lines.append(f"    {typ.name}:b = 2000000;")
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
        
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_pick(self) -> str:
        """pick statement with labeled arms and fall()."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int32:x = 2i32;")
        lines.append("    int32:result = 0i32;")
        lines.append("    pick (x) {")
        lines.append("        1 => { result = 10i32; }")
        lines.append("        2 => { result = 20i32; }")
        lines.append("        _ => { result = 99i32; }")
        lines.append("    }")
        lines.append("    if (result != 20i32) { pass(1i32); }")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
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
        lines.append("    if (result != 1i32) { pass(1i32); }")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_defer(self) -> str:
        """defer executes at scope exit in LIFO order."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int32:x = 0i32;")
        lines.append("    defer { x = x + 1i32; }")
        lines.append("    defer { x = x + 10i32; }")
        lines.append("    x = x + 100i32;")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_result(self) -> str:
        """Result<T> pass/fail/? unwrap chain."""
        lines = []
        lines.append("func:might_fail = Result<int32>(int32:x) {")
        lines.append("    if (x == 0i32) { fail(42i32); }")
        lines.append("    pass(x + 1i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append("    Result<int32>:ok = might_fail(5i32);")
        lines.append("    int32:val = ok?;")
        lines.append("    Result<int32>:bad = might_fail(0i32);")
        lines.append("    int32:fallback = bad ?? 0i32;")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_cast(self) -> str:
        """=> cast operator: widening and narrowing."""
        lines = []
        lines.append("func:main = int32() {")
        lines.append("    int8:small = 42i8;")
        lines.append("    int64:big = small => int64;")
        lines.append("    int32:mid = big => int32;")
        lines.append("    flt64:f = mid => flt64;")
        lines.append("    if (mid != 42i32) { pass(1i32); }")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
        return '\n'.join(lines) + '\n'
    
    def _gen_pipeline(self) -> str:
        """Forward |> and backward <| pipeline operators."""
        lines = []
        lines.append("func:double = int32(int32:x) { pass(x * 2i32); };")
        lines.append("func:inc    = int32(int32:x) { pass(x + 1i32); };")
        lines.append("")
        lines.append("func:main = int32() {")
        lines.append("    int32:r = 5i32 |> double |> inc;")
        lines.append("    if (r != 11i32) { pass(1i32); }")
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
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
            # Ensure unsigned types don't get negative literals
            if typ.category == TypeCategory.UNSIGNED and val < 0:
                val = abs(val)
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
        types = (self.type_system.get_types_by_category(TypeCategory.INTEGER) +
                 self.type_system.get_types_by_category(TypeCategory.UNSIGNED))
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
        
        lines.append("    pass(0i32);")
        lines.append("};")
        lines.append("")
        lines.append("func:failsafe = NIL(int32:err_code) { };")
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

