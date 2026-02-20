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


class AriaGenerator:
    """Generates syntactically valid Aria programs."""
    
    def __init__(self, aria_root: Path, seed: Optional[int] = None):
        if seed is not None:
            random.seed(seed)
        
        self.type_system = load_type_system(aria_root)
        self.operators = load_operators()
        self.grammar = load_grammar()
        
        # Counter for unique identifiers
        self.var_counter = 0
        self.func_counter = 0
    
    def generate_program(self, strategy: str = 'minimal') -> str:
        """
        Generate a complete Aria program.
        
        Strategies:
        - 'minimal': Single function returning 0
        - 'type_test': Test arithmetic for a specific type
        - 'control_flow': Test if/while/for  
        - 'functions': Multiple functions
        - 'edge_case': Boundary values
        """
        if strategy == 'minimal':
            return self._gen_minimal()
        elif strategy == 'type_test':
            return self._gen_type_test(self._random_numeric_type())
        elif strategy == 'control_flow':
            return self._gen_control_flow()
        elif strategy == 'functions':
            return self._gen_functions()
        elif strategy == 'edge_case':
            return self._gen_edge_case()
        else:
            # Random strategy
            return self.generate_program(random.choice([
                'minimal', 'type_test', 'control_flow', 'functions', 'edge_case'
            ]))
    
    def _gen_minimal(self) -> str:
        """Generate minimal valid program.""" 
        return """func:main = int32() { return 0i32; };
func:failsafe = void(int32:err_code) {};
"""
    
    def _gen_type_test(self, typ: PrimitiveType) -> str:
        """Generate a program that tests arithmetic operations for a type."""
        code = []
        
        # Generate main function
        code.append("func:main = int32() {")
        
        # Declare some variables
        val1 = self._gen_literal_for_type(typ)
        val2 = self._gen_literal_for_type(typ)
        
        code.append(f"    {typ.full_name()}:a = {val1};")
        code.append(f"    {typ.full_name()}:b = {val2};")
        code.append("")
        
        # Test operations
        bin_ops = self.operators.get_binary_operators(typ.category)
        for op in bin_ops[:5]:  # Limit to avoid huge programs
            var_name = self._gen_var_name()
            code.append(f"    {typ.full_name()}:{var_name} = (a {op.symbol} b);")
        
        code.append("")
        code.append("    return 0i32;")
        code.append("};")
        code.append("")
        code.append("func:failsafe = void(int32:err_code) {};")
        
        return '\n'.join(code) + '\n'
    
    def _gen_control_flow(self) -> str:
        """Generate program with control flow."""
        typ = self._random_numeric_type()
        code = []
        
        code.append("func:main = int32() {")
        code.append(f"    {typ.full_name()}:x = {self._gen_literal_for_type(typ)};")
        code.append("")
        
        # If statement
        code.append("    if (x > 0{}) {{".format(typ.literal_suffix()))
        code.append(f"        x = (x - 1{typ.literal_suffix()});")
        code.append("    } else {")
        code.append(f"        x = (x + 1{typ.literal_suffix()});")
        code.append("    };")
        code.append("")
        
        # While loop
        code.append(f"    while (x > 0{typ.literal_suffix()}) {{")
        code.append(f"        x = (x - 1{typ.literal_suffix()});")
        code.append("    };")
        code.append("")
        
        # For loop
        count = random.randint(5, 20)
        code.append(f"    for (i till {count}) {{")
        code.append(f"        x = (x + 1{typ.literal_suffix()});")
        code.append("    };")
        code.append("")
        
        code.append("    return 0i32;")
        code.append("};")
        code.append("")
        code.append("func:failsafe = void(int32:err_code) {};")
        
        return '\n'.join(code) + '\n'
    
    def _gen_functions(self) -> str:
        """Generate program with multiple functions."""
        code = []
        
        # Helper function
        typ = self._random_numeric_type()
        code.append(f"func:add_{typ.name} = {typ.name}({typ.name}:a, {typ.name}:b) {{")
        code.append("    return (a + b);")
        code.append("};")
        code.append("")
        
        # Main function that calls helper
        val1 = self._gen_literal_for_type(typ)
        val2 = self._gen_literal_for_type(typ)
        
        code.append("func:main = i32() {")
        code.append(f"    {typ.name}:result = add_{typ.name}({val1}, {val2});")
        code.append("    return 0;")
        code.append("};")
        
        return '\n'.join(code) + '\n'
    
    def _gen_edge_case(self) -> str:
        """Generate program testing boundary values."""
        typ = self._random_integer_type()
        code = []
        
        code.append("func:main = int32() {")
        
        # Test MIN value
        if typ.min_value is not None:
            code.append(f"    {typ.full_name()}:min_val = {typ.min_value}{typ.literal_suffix()};")
        
        # Test MAX value  
        if typ.max_value is not None:
            code.append(f"    {typ.full_name()}:max_val = {typ.max_value}{typ.literal_suffix()};")
        
        # Test zero
        code.append(f"    {typ.full_name()}:zero = 0{typ.literal_suffix()};")
        
        # Test small values
        if typ.category == TypeCategory.INTEGER:
            code.append(f"    {typ.full_name()}:neg_one = -1{typ.literal_suffix()};")
            code.append(f"    {typ.full_name()}:one = 1{typ.literal_suffix()};")
        else:
            code.append(f"    {typ.full_name()}:one = 1{typ.literal_suffix()};")
        
        # Test operations on boundaries
        if typ.max_value is not None:
            code.append(f"    {typ.full_name()}:near_max = {typ.max_value - 1}{typ.literal_suffix()};")
            code.append(f"    {typ.full_name()}:sum = (near_max + 1{typ.literal_suffix()});  // Should equal MAX")
        
        code.append("")
        code.append("    return 0i32;")
        code.append("};")
        code.append("")
        code.append("func:failsafe = void(int32:err_code) {};")
        
        return '\n'.join(code) + '\n'
    
    def _gen_expression(self, typ: PrimitiveType, depth: int = 0, max_depth: int = 3) -> str:
        """Generate a random expression of the given type."""
        if depth >= max_depth or random.random() < 0.3:
            # Base case: literal or variable
            return self._gen_literal_for_type(typ)
        
        # Recursive case: binary operation
        bin_ops = self.operators.get_binary_operators(typ.category)
        if not bin_ops:
            return self._gen_literal_for_type(typ)
        
        op = random.choice(bin_ops)
        left = self._gen_expression(typ, depth + 1, max_depth)
        right = self._gen_expression(typ, depth + 1, max_depth)
        
        return f"({left} {op.symbol} {right})"
    
    def _gen_literal_for_type(self, typ: PrimitiveType) -> str:
        """Generate a random literal value for a type."""
        if typ.category in {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB}:
            # Integer literal
            if typ.min_value is not None and typ.max_value is not None:
                # Generate value in valid range
                # Use smaller range for practicality
                safe_min = max(typ.min_value, -1000000)
                safe_max = min(typ.max_value, 1000000)
                
                # Bias toward interesting values
                choice = random.random()
                if choice < 0.1 and typ.min_value > -1000000:
                    val = typ.min_value
                elif choice < 0.2 and typ.max_value < 1000000:
                    val = typ.max_value
                elif choice < 0.3:
                    val = 0
                elif choice < 0.4:
                    val = 1
                elif choice < 0.5 and typ.category == TypeCategory.INTEGER:
                    val = -1
                else:
                    val = random.randint(safe_min, safe_max)
                
                return f"{val}{typ.literal_suffix()}"
            else:
                # No bounds known, use small value
                val = random.randint(-100, 100)
                return f"{val}{typ.literal_suffix()}"
        
        elif typ.category == TypeCategory.FLOAT:
            # Float literal
            val = random.uniform(-1000.0, 1000.0)
            # Float literals use f32/f64 suffix
            return f"{val:.6f}{typ.literal_suffix()}"
        
        elif typ.category == TypeCategory.BOOL:
            return random.choice(['true', 'false'])
        
        elif typ.category == TypeCategory.STRING:
            # Simple string literal
            choices = [
                '"hello"',
                '"test"',
                '"fuzzer"',
                '""',
                '"a"'
            ]
            return random.choice(choices)
        
        else:
            return "0i32"
    
    def _random_numeric_type(self) -> PrimitiveType:
        """Get a random numeric type."""
        categories = [TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT]
        types = []
        for cat in categories:
            types.extend(self.type_system.get_types_by_category(cat))
        return random.choice(types)
    
    def _random_integer_type(self) -> PrimitiveType:
        """Get a random integer type (signed or unsigned)."""
        types = (self.type_system.get_types_by_category(TypeCategory.INTEGER) +
                self.type_system.get_types_by_category(TypeCategory.UNSIGNED))
        return random.choice(types)
    
    def _gen_var_name(self) -> str:
        """Generate a unique variable name."""
        name = f"var_{self.var_counter}"
        self.var_counter += 1
        return name
    
    def _gen_func_name(self) -> str:
        """Generate a unique function name."""
        name = f"func_{self.func_counter}"
        self.func_counter += 1
        return name


class TypeExhaustiveGenerator:
    """Generates tests that exhaustively cover type operations."""
    
    def __init__(self, aria_root: Path):
        self.type_system = load_type_system(aria_root)
        self.operators = load_operators()
    
    def generate_all_type_tests(self) -> List[Tuple[str, str]]:
        """
        Generate test for every type + operation combination.
        Returns list of (filename, content) tuples.
        """
        tests = []
        
        # For each numeric type
        numeric_types = (
            self.type_system.get_types_by_category(TypeCategory.INTEGER) +
            self.type_system.get_types_by_category(TypeCategory.UNSIGNED) +
            self.type_system.get_types_by_category(TypeCategory.FLOAT)
        )
        
        for typ in numeric_types:
            # For each binary operator
            bin_ops = self.operators.get_binary_operators(typ.category)
            
            for op in bin_ops:
                test_name = f"type_exhaustive_{typ.name}_{op.name}.aria"
                test_code = self._gen_operator_test(typ, op)
                tests.append((test_name, test_code))
        
        return tests
    
    def _gen_operator_test(self, typ: PrimitiveType, op: Operator) -> str:
        """Generate test for specific type + operator."""
        code = []
        
        code.append(f"// Auto-generated: Test {typ.name} {op.symbol} operator")
        code.append("func:main = int32() {")
        
        # Test with normal values
        code.append(f"    {typ.full_name()}:a = 10{typ.literal_suffix()};")
        code.append(f"    {typ.full_name()}:b = 5{typ.literal_suffix()};")
        
        if op.symbol in {'==', '!=', '<', '>', '<=', '>='}:
            # Comparison operators return bool
            code.append(f"    bool:result = (a {op.symbol} b);")
        else:
            # Arithmetic/bitwise operators return same type
            code.append(f"    {typ.full_name()}:result = (a {op.symbol} b);")
        
        # Test with boundary values if applicable
        if typ.min_value is not None:
            code.append(f"    {typ.full_name()}:min_val = {typ.min_value}{typ.literal_suffix()};")
            if op.symbol not in {'/', '%'}:  # Avoid division by extreme values
                code.append(f"    {typ.full_name()}:test_min = (min_val {op.symbol} b);")
        
        if typ.max_value is not None:
            code.append(f"    {typ.full_name()}:max_val = {typ.max_value}{typ.literal_suffix()};")
            if op.symbol not in {'/', '%'}:
                code.append(f"    {typ.full_name()}:test_max = (max_val {op.symbol} b);")
        
        code.append("")
        code.append("    return 0i32;")
        code.append("};")
        code.append("")
        code.append("func:failsafe = void(int32:err_code) {};")
        
        return '\n'.join(code) + '\n'


if __name__ == '__main__':
    from pathlib import Path
    
    aria_root = Path(__file__).parent.parent.parent.parent
    
    print("=== Aria Program Generator Test ===\n")
    
    gen = AriaGenerator(aria_root, seed=42)
    
    print("1. Minimal Program:")
    print(gen.generate_program('minimal'))
    
    print("\n2. Type Test (i32):")
    i32_type = gen.type_system.get_type('i32')
    print(gen._gen_type_test(i32_type))
    
    print("\n3. Control Flow:")
    print(gen.generate_program('control_flow'))
    
    print("\n4. Edge Case Test:")
    print(gen.generate_program('edge_case'))
    
    print("\n5. Type-Exhaustive Generator:")
    exhaustive_gen = TypeExhaustiveGenerator(aria_root)
    tests = exhaustive_gen.generate_all_type_tests()
    print(f"Generated {len(tests)} exhaustive tests")
    print(f"Sample: {tests[0][0]}")
    print(tests[0][1])
