#!/usr/bin/env python3
"""
Aria Random Program Generator
Phase 2.6: Property Testing Framework

Generates structurally valid Aria programs for property testing.
Goal: 100,000+ random programs to validate borrow checker soundness.
"""

import random
import string
from dataclasses import dataclass, field
from typing import List, Optional, Set, Tuple
from enum import Enum, auto

class TypeKind(Enum):
    """Aria type kinds for generation"""
    INT8 = auto()
    INT16 = auto()
    INT32 = auto()
    INT64 = auto()
    UINT8 = auto()
    UINT16 = auto()
    UINT32 = auto()
    UINT64 = auto()
    FLT32 = auto()
    FLT64 = auto()
    BOOL = auto()
    STRING = auto()
    TBB8 = auto()
    TBB16 = auto()
    TBB32 = auto()
    TBB64 = auto()
    WILD_PTR = auto()  # wild int8@

    def to_aria(self) -> str:
        mapping = {
            TypeKind.INT8: "int8",
            TypeKind.INT16: "int16",
            TypeKind.INT32: "int32",
            TypeKind.INT64: "int64",
            TypeKind.UINT8: "uint8",
            TypeKind.UINT16: "uint16",
            TypeKind.UINT32: "uint32",
            TypeKind.UINT64: "uint64",
            TypeKind.FLT32: "flt32",
            TypeKind.FLT64: "flt64",
            TypeKind.BOOL: "bool",
            TypeKind.STRING: "string",
            TypeKind.TBB8: "tbb8",
            TypeKind.TBB16: "tbb16",
            TypeKind.TBB32: "tbb32",
            TypeKind.TBB64: "tbb64",
            TypeKind.WILD_PTR: "wild int8@",
        }
        return mapping[self]

@dataclass
class Variable:
    """Represents a variable in the generated program"""
    name: str
    type_kind: TypeKind
    is_mutable: bool = True
    is_borrowed: bool = False
    is_borrowed_mut: bool = False
    is_freed: bool = False
    is_moved: bool = False

@dataclass
class Scope:
    """Represents a lexical scope"""
    variables: List[Variable] = field(default_factory=list)
    defers: List[str] = field(default_factory=list)
    parent: Optional['Scope'] = None

    def all_variables(self) -> List[Variable]:
        """Get all variables visible in this scope"""
        result = list(self.variables)
        if self.parent:
            result.extend(self.parent.all_variables())
        return result

    def find_variable(self, name: str) -> Optional[Variable]:
        """Find variable by name in this or parent scope"""
        for var in self.variables:
            if var.name == name:
                return var
        if self.parent:
            return self.parent.find_variable(name)
        return None

class AriaGenerator:
    """
    Generates random valid Aria programs.

    Key properties ensured:
    1. All borrows respect XOR rule (1 mutable OR N immutable)
    2. No use-after-free (freed memory not accessed)
    3. No use-after-move (moved variables not accessed)
    4. All wild memory is properly freed (via defer or explicit)
    5. Control flow is valid (all paths return)
    """

    def __init__(self, seed: Optional[int] = None,
                 complexity: int = 3,
                 test_borrow_checker: bool = True,
                 test_wild_memory: bool = True,
                 test_tbb: bool = True,
                 test_control_flow: bool = True):
        """
        Initialize generator with configuration.

        Args:
            seed: Random seed for reproducibility
            complexity: 1-5, controls program size and nesting
            test_borrow_checker: Generate borrow-heavy programs
            test_wild_memory: Generate wild allocation programs
            test_tbb: Generate TBB error propagation programs
            test_control_flow: Generate complex control flow
        """
        if seed is not None:
            random.seed(seed)

        self.complexity = complexity
        self.test_borrow_checker = test_borrow_checker
        self.test_wild_memory = test_wild_memory
        self.test_tbb = test_tbb
        self.test_control_flow = test_control_flow

        # State tracking
        self.var_counter = 0
        self.current_scope: Optional[Scope] = None
        self.indent_level = 0
        self.generated_functions: List[str] = []
        self.helper_functions: List[str] = []

    def _fresh_var(self, prefix: str = "v") -> str:
        """Generate a fresh variable name that doesn't conflict with keywords"""
        self.var_counter += 1
        # Use underscore prefix to avoid conflicts with type names like tbb8, int32, etc.
        return f"_{prefix}{self.var_counter}"

    def _indent(self) -> str:
        """Get current indentation"""
        return "    " * self.indent_level

    def _random_type(self, include_wild: bool = False,
                     include_tbb: bool = False) -> TypeKind:
        """Choose a random type"""
        types = [TypeKind.INT32, TypeKind.INT64, TypeKind.BOOL]

        # NOTE: Wild pointers are NOT included in random type selection
        # They should only be created through _gen_wild_memory_pattern which handles defer properly
        # if include_wild and self.test_wild_memory:
        #     types.append(TypeKind.WILD_PTR)

        if include_tbb and self.test_tbb:
            types.extend([TypeKind.TBB8, TypeKind.TBB32])

        return random.choice(types)

    def _random_int_literal(self, type_kind: TypeKind) -> str:
        """Generate a random integer literal for a type.

        Aria uses small integers (fits in int8 range) to avoid type width issues.
        """
        if type_kind in (TypeKind.INT8, TypeKind.TBB8):
            return str(random.randint(0, 100))  # Positive only for simplicity
        elif type_kind in (TypeKind.INT16, TypeKind.TBB16):
            return str(random.randint(0, 100))
        elif type_kind in (TypeKind.INT32, TypeKind.TBB32):
            return str(random.randint(0, 100))  # Keep small to avoid int64 inference
        elif type_kind in (TypeKind.INT64, TypeKind.TBB64):
            return str(random.randint(0, 100))
        elif type_kind == TypeKind.BOOL:
            return random.choice(["true", "false"])
        elif type_kind in (TypeKind.FLT32, TypeKind.FLT64):
            return f"{random.uniform(0.0, 100.0):.2f}"
        else:
            return "0"

    def _gen_expression(self, type_kind: TypeKind, depth: int = 0) -> str:
        """Generate an expression of the given type"""
        # Wild pointers always need alloc()
        if type_kind == TypeKind.WILD_PTR:
            return f"alloc({random.randint(1, 100)})"

        if depth > 2 or random.random() < 0.5:
            # Base case: literal or variable
            if type_kind.name.startswith("TBB"):
                size = type_kind.name[3:]  # "8", "16", "32", "64"
                val = self._random_int_literal(type_kind)
                return f"tbb{size}_from_int({val})"
            else:
                # For simplicity, just use literals to avoid scope issues
                # (variables defined in if branches aren't visible outside)
                return self._random_int_literal(type_kind)

        # Recursive case: binary operation
        # NOTE: Arithmetic on integer literals produces int64, so only use for int64
        if type_kind == TypeKind.INT64:
            op = random.choice(["+", "-", "*"])
            left = self._gen_expression(type_kind, depth + 1)
            right = self._gen_expression(type_kind, depth + 1)
            return f"({left} {op} {right})"
        elif type_kind in (TypeKind.INT8, TypeKind.INT16, TypeKind.INT32):
            # For smaller int types, just return literals to avoid type promotion
            return self._random_int_literal(type_kind)
        elif type_kind.name.startswith("TBB"):
            size = type_kind.name[3:]
            op = random.choice(["add", "sub", "mul"])
            left = self._gen_expression(type_kind, depth + 1)
            right = self._gen_expression(type_kind, depth + 1)
            return f"tbb{size}_{op}({left}, {right})"
        elif type_kind == TypeKind.BOOL:
            if random.random() < 0.5:
                return random.choice(["true", "false"])
            else:
                int_type = random.choice([TypeKind.INT32, TypeKind.INT64])
                left = self._gen_expression(int_type, depth + 1)
                right = self._gen_expression(int_type, depth + 1)
                op = random.choice(["<", ">", "<=", ">=", "==", "!="])
                return f"({left} {op} {right})"

        return self._random_int_literal(type_kind)

    def _gen_var_decl(self, force_type: Optional[TypeKind] = None,
                      force_wild: bool = False) -> Tuple[str, Variable]:
        """Generate a variable declaration"""
        var_name = self._fresh_var()

        if force_wild:
            type_kind = TypeKind.WILD_PTR
        elif force_type:
            type_kind = force_type
        else:
            type_kind = self._random_type(include_wild=self.test_wild_memory,
                                         include_tbb=self.test_tbb)

        type_str = type_kind.to_aria()
        expr = self._gen_expression(type_kind)

        var = Variable(name=var_name, type_kind=type_kind)
        if self.current_scope:
            self.current_scope.variables.append(var)

        return f"{self._indent()}{type_str}:{var_name} = {expr};\n", var

    def _gen_borrow(self, var: Variable, mutable: bool = False) -> str:
        """Generate a borrow of a variable.

        Aria borrow syntax:
        - Mutable borrow: type$:name = $var;
        - Immutable borrow: type$:name = !$var;
        """
        ref_name = self._fresh_var("ref")

        if mutable:
            if var.is_borrowed or var.is_borrowed_mut:
                # Can't borrow mutably if already borrowed
                return ""
            var.is_borrowed_mut = True
            borrow_expr = f"${var.name}"  # Mutable: $var
            ref_type = f"{var.type_kind.to_aria()}$"  # type$
        else:
            if var.is_borrowed_mut:
                # Can't borrow immutably if mutably borrowed
                return ""
            var.is_borrowed = True
            borrow_expr = f"!${var.name}"  # Immutable: !$var
            ref_type = f"{var.type_kind.to_aria()}$"  # type$

        return f"{self._indent()}{ref_type}:{ref_name} = {borrow_expr};\n"

    def _gen_if_stmt(self, depth: int = 0) -> str:
        """Generate an if statement"""
        if depth > self.complexity:
            return ""

        cond = self._gen_expression(TypeKind.BOOL)

        result = f"{self._indent()}if ({cond}) {{\n"
        self.indent_level += 1

        # Generate body statements
        num_stmts = random.randint(1, 3)
        for _ in range(num_stmts):
            result += self._gen_statement(depth + 1)

        self.indent_level -= 1

        # Maybe add else branch
        if random.random() < 0.5:
            result += f"{self._indent()}}} else {{\n"
            self.indent_level += 1

            num_stmts = random.randint(1, 3)
            for _ in range(num_stmts):
                result += self._gen_statement(depth + 1)

            self.indent_level -= 1

        result += f"{self._indent()}}}\n"  # No semicolon after if block
        return result

    def _gen_while_loop(self, depth: int = 0) -> str:
        """Generate a while loop"""
        if depth > self.complexity:
            return ""

        # Create a loop counter
        counter_name = self._fresh_var("i")
        limit = random.randint(1, 10)

        result = f"{self._indent()}int32:{counter_name} = 0;\n"
        result += f"{self._indent()}while ({counter_name} < {limit}) {{\n"
        self.indent_level += 1

        # Generate body statements
        num_stmts = random.randint(1, 2)
        for _ in range(num_stmts):
            result += self._gen_statement(depth + 1)

        # Increment counter
        result += f"{self._indent()}{counter_name} = {counter_name} + 1;\n"

        self.indent_level -= 1
        result += f"{self._indent()}}}\n"  # No semicolon after while block
        return result

    def _gen_defer(self) -> str:
        """Generate a defer statement"""
        # Find wild pointers that need freeing
        if not self.current_scope:
            return ""

        wild_vars = [v for v in self.current_scope.variables
                    if v.type_kind == TypeKind.WILD_PTR and not v.is_freed]

        if not wild_vars:
            return ""

        var = random.choice(wild_vars)
        var.is_freed = True  # Mark as will-be-freed

        return f"{self._indent()}defer {{ free({var.name}); }}\n"  # No semicolon after defer

    def _gen_wild_memory_pattern(self, in_nested_scope: bool = False) -> str:
        """Generate a wild memory allocation/free pattern"""
        result = ""

        # Allocate
        decl, var = self._gen_var_decl(force_wild=True)
        result += decl

        # Always use defer in nested scopes to avoid scope issues
        # At top level, sometimes use explicit free
        if in_nested_scope or random.random() < 0.8:
            # Use defer (safer pattern) - no semicolon after defer block
            result += f"{self._indent()}defer {{ free({var.name}); }}\n"
            var.is_freed = True
        else:
            # Will need explicit free later (only at top level)
            pass

        return result

    def _gen_tbb_pattern(self) -> str:
        """Generate a TBB error propagation pattern"""
        result = ""

        tbb_type = random.choice([TypeKind.TBB8, TypeKind.TBB32])
        size = tbb_type.name[3:]

        # Create initial TBB value
        var1_name = self._fresh_var("tbb")
        val = random.randint(-100, 100)
        result += f"{self._indent()}tbb{size}:{var1_name} = tbb{size}_from_int({val});\n"

        # Possibly create an error
        if random.random() < 0.3:
            var2_name = self._fresh_var("tbb_err")
            result += f"{self._indent()}tbb{size}:{var2_name} = tbb{size}_div({var1_name}, tbb{size}_from_int(0));\n"

            # Check error
            err_check = self._fresh_var("is_err")
            result += f"{self._indent()}bool:{err_check} = tbb{size}_is_err({var2_name});\n"
        else:
            # Normal operation
            var2_name = self._fresh_var("tbb_op")
            op = random.choice(["add", "sub", "mul"])
            result += f"{self._indent()}tbb{size}:{var2_name} = tbb{size}_{op}({var1_name}, tbb{size}_from_int({random.randint(1, 10)}));\n"

        return result

    def _gen_borrow_pattern(self) -> str:
        """Generate a borrow checker test pattern"""
        result = ""

        # Create a variable
        decl, var = self._gen_var_decl(force_type=TypeKind.INT32)
        result += decl

        # For now, only do simple borrow patterns that we know work
        pattern = random.choice(["mutable_borrow", "simple_use"])

        if pattern == "mutable_borrow":
            # Single mutable borrow (valid)
            borrow = self._gen_borrow(var, mutable=True)
            if borrow:
                result += borrow

        elif pattern == "simple_use":
            # Just use the variable
            result += f"{self._indent()}{var.name} = {var.name} + 1;\n"

        return result

    def _gen_statement(self, depth: int = 0) -> str:
        """Generate a random statement"""
        if depth > self.complexity:
            # At max depth, just generate simple statements
            decl, _ = self._gen_var_decl()
            return decl

        # Weight statement types based on configuration
        choices = ["var_decl"]
        weights = [1.0]

        if self.test_control_flow:
            choices.extend(["if_stmt", "while_loop"])
            weights.extend([0.3, 0.2])

        if self.test_borrow_checker:
            choices.append("borrow_pattern")
            weights.append(0.4)

        if self.test_wild_memory:
            choices.append("wild_memory")
            weights.append(0.3)

        if self.test_tbb:
            choices.append("tbb_pattern")
            weights.append(0.3)

        # Normalize weights
        total = sum(weights)
        weights = [w / total for w in weights]

        choice = random.choices(choices, weights=weights)[0]

        # depth > 0 means we're inside a nested scope (if/while body)
        in_nested = depth > 0

        if choice == "var_decl":
            decl, _ = self._gen_var_decl()
            return decl
        elif choice == "if_stmt":
            return self._gen_if_stmt(depth)
        elif choice == "while_loop":
            return self._gen_while_loop(depth)
        elif choice == "borrow_pattern":
            return self._gen_borrow_pattern()
        elif choice == "wild_memory":
            return self._gen_wild_memory_pattern(in_nested_scope=in_nested)
        elif choice == "tbb_pattern":
            return self._gen_tbb_pattern()

        return ""

    def _gen_helper_functions(self) -> str:
        """Generate helper functions used by tests"""
        # Aria doesn't require helper functions for borrow tests
        # The borrow operations are done inline
        return ""

    def generate_program(self, num_statements: int = 10) -> str:
        """Generate a complete Aria program"""
        self.var_counter = 0
        self.current_scope = Scope()
        self.indent_level = 0

        # Start with file header
        result = """/**
 * Auto-generated Aria program for property testing
 * Seed: {seed}
 */

""".format(seed=random.getstate()[1][0] if hasattr(random.getstate()[1], '__getitem__') else 0)

        # Add helper functions
        result += self._gen_helper_functions()

        # Main function
        result += "func:main = int32() {\n"
        self.indent_level = 1

        # Generate statements
        for _ in range(num_statements):
            stmt = self._gen_statement()
            if stmt:
                result += stmt

        # Free any remaining wild pointers
        wild_vars = [v for v in self.current_scope.variables
                    if v.type_kind == TypeKind.WILD_PTR and not v.is_freed]
        for var in wild_vars:
            result += f"{self._indent()}free({var.name});\n"

        # Return
        result += f"{self._indent()}return 0;\n"
        self.indent_level = 0
        result += "};\n"

        return result

    def generate_borrow_checker_test(self) -> str:
        """Generate a program specifically testing borrow checker"""
        self.test_borrow_checker = True
        self.test_wild_memory = True
        self.test_tbb = False
        self.test_control_flow = True
        return self.generate_program(num_statements=15)

    def generate_tbb_test(self) -> str:
        """Generate a program specifically testing TBB propagation"""
        self.test_borrow_checker = False
        self.test_wild_memory = False
        self.test_tbb = True
        self.test_control_flow = True
        return self.generate_program(num_statements=10)

    def generate_wild_memory_test(self) -> str:
        """Generate a program specifically testing wild memory"""
        self.test_borrow_checker = False
        self.test_wild_memory = True
        self.test_tbb = False
        self.test_control_flow = True
        return self.generate_program(num_statements=12)


class AdversarialGenerator(AriaGenerator):
    """
    Generates adversarial programs that are DESIGNED to trigger bugs.
    These should be REJECTED by the compiler.
    """

    def generate_use_after_free(self) -> str:
        """Generate a use-after-free attempt (should be rejected)"""
        return """/**
 * Adversarial: Use-After-Free
 * Expected: COMPILER ERROR
 */

func:main = int32() {
    wild int8@:ptr = alloc(100);
    free(ptr);
    // BUG: accessing freed memory
    wild int8@:alias = ptr;  // Should error: ptr is FREED
    return 0;
};
"""

    def generate_double_free(self) -> str:
        """Generate a double-free attempt (should be rejected)"""
        return """/**
 * Adversarial: Double Free
 * Expected: COMPILER ERROR
 */

func:main = int32() {
    wild int8@:ptr = alloc(100);
    free(ptr);
    free(ptr);  // Should error: already FREED
    return 0;
};
"""

    def generate_aliasing_violation(self) -> str:
        """Generate an aliasing violation (should be rejected)"""
        return """/**
 * Adversarial: Aliasing Violation
 * Expected: COMPILER ERROR
 */

func:modify_both = void(int32$:a, int32$:b) {
    return;
};

func:main = int32() {
    int32:x = 10;
    modify_both($x, $x);  // Should error: same variable borrowed mutably twice
    return 0;
};
"""

    def generate_borrow_mutation(self) -> str:
        """Generate mutation while borrowed (should be rejected)"""
        return """/**
 * Adversarial: Mutation While Borrowed
 * Expected: COMPILER ERROR
 */

func:main = int32() {
    int32:x = 10;
    int32$:ref = !$x;  // Immutable borrow (type$:name = !$var)
    x = 20;            // Should error: x is borrowed
    return 0;
};
"""

    def generate_loop_carried_uaf(self) -> str:
        """Generate loop-carried use-after-free (should be rejected)"""
        return """/**
 * Adversarial: Loop-Carried Use-After-Free
 * Expected: COMPILER ERROR
 */

func:main = int32() {
    wild int8@:ptr = alloc(100);
    int32:i = 0;
    while (i < 3) {
        // Second iteration: ptr is MAY_FREED
        wild int8@:alias = ptr;  // Should error on iteration 2+
        free(ptr);
        i = i + 1;
    }
    return 0;
};
"""

    def generate_conditional_leak(self) -> str:
        """Generate a conditional memory leak (should be rejected or warned)"""
        return """/**
 * Adversarial: Conditional Memory Leak
 * Expected: COMPILER WARNING or ERROR
 */

func:main = int32() {
    wild int8@:ptr = alloc(100);
    bool:cond = true;
    if (cond) {
        free(ptr);
    } else {
        // Path doesn't free - memory leak
    }
    // ptr is MAY_FREED here
    return 0;
};
"""


if __name__ == "__main__":
    # Test the generator
    gen = AriaGenerator(seed=42, complexity=3)
    print("=== Generated Aria Program ===")
    print(gen.generate_program(num_statements=8))

    print("\n=== Adversarial Test ===")
    adv = AdversarialGenerator()
    print(adv.generate_use_after_free())
