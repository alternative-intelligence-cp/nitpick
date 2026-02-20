#!/usr/bin/env python3
"""
Aria Fuzzer V2 - Grammar Parser
Extracts type system and grammar rules from specs and programming guide
"""

import re
from dataclasses import dataclass, field
from typing import List, Dict, Set, Optional, Tuple
from pathlib import Path
from enum import Enum, auto


class TypeCategory(Enum):
    """Categories of types in Aria."""
    INTEGER = auto()
    UNSIGNED = auto()
    FLOAT = auto()
    TBB = auto()  # Restricted Bounds
    BOOL = auto()
    STRING = auto()
    VOID = auto()
    RESULT = auto()
    WILD = auto()
    CUSTOM = auto()


@dataclass
class PrimitiveType:
    """Represents a primitive type in Aria."""
    name: str
    category: TypeCategory
    size_bits: int
    min_value: Optional[int] = None
    max_value: Optional[int] = None
    
    def __str__(self) -> str:
        return self.name
    
    def __repr__(self) -> str:
        return f"PrimitiveType({self.name}, {self.size_bits}bit)"
    
    def full_name(self) -> str:
        """Get the full type name used in variable declarations."""
        # Map shorthand to full names
        type_map = {
            'i8': 'int8', 'i16': 'int16', 'i32': 'int32', 'i64': 'int64',
            'u8': 'uint8', 'u16': 'uint16', 'u32': 'uint32', 'u64': 'uint64',
            'f32': 'float32', 'f64': 'float64',
            'flt32': 'float32', 'flt64': 'float64', 'flt128': 'float128',
            'flt256': 'float256', 'flt512': 'float512',
            'tbb8': 'tbb8', 'tbb16': 'tbb16', 'tbb32': 'tbb32', 'tbb64': 'tbb64',
            'bool': 'bool', 'string': 'string', 'void': 'void'
        }
        return type_map.get(self.name, self.name)
    
    def literal_suffix(self) -> str:
        """Get the suffix for literals (e.g., 'i32' for 42i32)."""
        return self.name


@dataclass
class Operator:
    """Represents an operator and its properties."""
    symbol: str
    name: str
    precedence: int
    associativity: str  # 'left', 'right'
    operand_count: int  # 1=unary, 2=binary, 3=ternary
    valid_types: Set[TypeCategory] = field(default_factory=set)
    
    def __str__(self) -> str:
        return self.symbol


@dataclass
class GrammarRule:
    """Represents a grammar production rule."""
    name: str
    alternatives: List[List[str]]  # List of production alternatives
    
    def __repr__(self) -> str:
        return f"{self.name} := {' | '.join(' '.join(alt) for alt in self.alternatives)}"


class TypeSystemParser:
    """Parses TYPE_SYSTEM.md to extract type definitions."""
    
    def __init__(self, spec_path: Path):
        self.spec_path = spec_path
        self.types: Dict[str, PrimitiveType] = {}
        self._parse()
    
    def _parse(self):
        """Parse the type system specification."""
        content = self.spec_path.read_text()
        
        # Parse integer types
        self._parse_integers(content)
        # Parse unsigned types
        self._parse_unsigned(content)
        # Parse floating-point types
        self._parse_floats(content)
        # Parse TBB types
        self._parse_tbb(content)
        # Parse other primitives
        self._parse_primitives(content)
    
    def _parse_integers(self, content: str):
        """Extract signed integer types."""
        # Find the integer table in the spec
        int_pattern = r'\|\s*`(i\d+)`\s*\|\s*(\d+)\s*bytes?\s*\|\s*(-?[\d^]+)\s*to\s*(-?[\d^]+)'
        
        for match in re.finditer(int_pattern, content):
            name = match.group(1)
            size_bytes = int(match.group(2))
            min_val_str = match.group(3)
            max_val_str = match.group(4)
            
            # Parse min/max (handle 2^31-1 notation)
            min_val = self._parse_bound(min_val_str, size_bytes, signed=True, is_min=True)
            max_val = self._parse_bound(max_val_str, size_bytes, signed=True, is_min=False)
            
            self.types[name] = PrimitiveType(
                name=name,
                category=TypeCategory.INTEGER,
                size_bits=size_bytes * 8,
                min_value=min_val,
                max_value=max_val
            )
    
    def _parse_unsigned(self, content: str):
        """Extract unsigned integer types."""
        uint_pattern = r'\|\s*`(u\d+)`\s*\|\s*(\d+)\s*bytes?\s*\|\s*(\d+)\s*to\s*([\d^]+)'
        
        for match in re.finditer(uint_pattern, content):
            name = match.group(1)
            size_bytes = int(match.group(2))
            max_val_str = match.group(4)
            
            max_val = self._parse_bound(max_val_str, size_bytes, signed=False, is_min=False)
            
            self.types[name] = PrimitiveType(
                name=name,
                category=TypeCategory.UNSIGNED,
                size_bits=size_bytes * 8,
                min_value=0,
                max_value=max_val
            )
    
    def _parse_floats(self, content: str):
        """Extract floating-point types."""
        float_types = {
            'f32': 32,
            'f64': 64,
            'flt32': 32,
            'flt64': 64,
            'flt128': 128,
            'flt256': 256,
            'flt512': 512
        }
        
        for name, size_bits in float_types.items():
            if name in content or name.replace('flt', 'f') in content:
                self.types[name] = PrimitiveType(
                    name=name,
                    category=TypeCategory.FLOAT,
                    size_bits=size_bits
                )
    
    def _parse_tbb(self, content: str):
        """Extract TBB (Restricted Bounds) types."""
        tbb_types = ['tbb8', 'tbb16', 'tbb32', 'tbb64']
        
        for name in tbb_types:
            if name in content:
                size_bits = int(name[3:])
                self.types[name] = PrimitiveType(
                    name=name,
                    category=TypeCategory.TBB,
                    size_bits=size_bits,
                    min_value=0,
                    max_value=2**size_bits - 1
                )
    
    def _parse_primitives(self, content: str):
        """Add other primitive types."""
        # Boolean
        self.types['bool'] = PrimitiveType(
            name='bool',
            category=TypeCategory.BOOL,
            size_bits=8
        )
        
        # String
        self.types['string'] = PrimitiveType(
            name='string',
            category=TypeCategory.STRING,
            size_bits=0  # Variable size
        )
        
        # Void
        self.types['void'] = PrimitiveType(
            name='void',
            category=TypeCategory.VOID,
            size_bits=0
        )
    
    def _parse_bound(self, bound_str: str, size_bytes: int, signed: bool, is_min: bool) -> int:
        """Parse a numeric bound, handling scientific notation."""
        bound_str = bound_str.strip()
        
        # Handle 2^N notation
        if '^' in bound_str:
            match = re.search(r'2\^(\d+)', bound_str)
            if match:
                exp = int(match.group(1))
                val = 2 ** exp
                if '-1' in bound_str or is_min:
                    val -= 1
                if bound_str.startswith('-'):
                    val = -val
                return val
        
        # Handle direct numbers
        try:
            return int(bound_str.replace(',', ''))
        except ValueError:
            # Fallback: calculate from size
            if signed:
                if is_min:
                    return -(2 ** (size_bytes * 8 - 1))
                else:
                    return 2 ** (size_bytes * 8 - 1) - 1
            else:
                return 2 ** (size_bytes * 8) - 1
    
    def get_type(self, name: str) -> Optional[PrimitiveType]:
        """Get a type by name."""
        return self.types.get(name)
    
    def get_all_types(self) -> List[PrimitiveType]:
        """Get all defined types."""
        return list(self.types.values())
    
    def get_types_by_category(self, category: TypeCategory) -> List[PrimitiveType]:
        """Get all types of a specific category."""
        return [t for t in self.types.values() if t.category == category]


class OperatorParser:
    """Parses operators from programming guide."""
    
    def __init__(self):
        self.operators: Dict[str, Operator] = {}
        self._define_operators()
    
    def _define_operators(self):
        """Define all Aria operators with precedence."""
        # Based on typical C-like precedence
        
        # Arithmetic: precedence 1-3
        self.operators['+'] = Operator('+', 'add', 1, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB})
        self.operators['-'] = Operator('-', 'subtract', 1, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB})
        self.operators['*'] = Operator('*', 'multiply', 2, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB})
        self.operators['/'] = Operator('/', 'divide', 2, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB})
        self.operators['%'] = Operator('%', 'modulo', 2, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        
        # Comparison: precedence 4
        for op in ['==', '!=', '<', '>', '<=', '>=']:
            self.operators[op] = Operator(op, f'compare_{op}', 4, 'left', 2,
                                          {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB, TypeCategory.BOOL})
        
        # Logical: precedence 5-6
        self.operators['!'] = Operator('!', 'not', 6, 'right', 1, {TypeCategory.BOOL})
        self.operators['&&'] = Operator('&&', 'and', 5, 'left', 2, {TypeCategory.BOOL})
        self.operators['||'] = Operator('||', 'or', 5, 'left', 2, {TypeCategory.BOOL})
        
        # Bitwise: precedence 7-9
        self.operators['&'] = Operator('&', 'bitand', 7, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        self.operators['|'] = Operator('|', 'bitor', 7, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        self.operators['^'] = Operator('^', 'xor', 7, 'left', 2,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        self.operators['~'] = Operator('~', 'bitnot', 8, 'right', 1,
                                       {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        self.operators['<<'] = Operator('<<', 'shl', 9, 'left', 2,
                                        {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        self.operators['>>'] = Operator('>>', 'shr', 9, 'left', 2,
                                        {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.TBB})
        
        # Ternary
        self.operators['?:'] = Operator('?:', 'ternary', 10, 'right', 3, set())
        
        # Null coalescing
        self.operators['??'] = Operator('??', 'null_coalesce', 11, 'right', 2, set())
    
    def get_operator(self, symbol: str) -> Optional[Operator]:
        """Get an operator by symbol."""
        return self.operators.get(symbol)
    
    def get_binary_operators(self, type_category: TypeCategory) -> List[Operator]:
        """Get all binary operators valid for a type category."""
        return [op for op in self.operators.values()
                if op.operand_count == 2 and type_category in op.valid_types]
    
    def get_unary_operators(self, type_category: TypeCategory) -> List[Operator]:
        """Get all unary operators valid for a type category."""
        return [op for op in self.operators.values()
                if op.operand_count == 1 and type_category in op.valid_types]


class GrammarParser:
    """Defines Aria grammar rules."""
    
    def __init__(self):
        self.rules: Dict[str, GrammarRule] = {}
        self._define_grammar()
    
    def _define_grammar(self):
        """Define core Aria grammar rules."""
        
        # Top-level
        self.rules['Program'] = GrammarRule('Program', [
            ['Function+']
        ])
        
        # Functions
        self.rules['Function'] = GrammarRule('Function', [
            ['func', ':', 'Identifier', '=', 'Type', '(', 'ParamList?', ')', 'Block', ';']
        ])
        
        self.rules['ParamList'] = GrammarRule('ParamList', [
            ['Param'],
            ['Param', ',', 'ParamList']
        ])
        
        self.rules['Param'] = GrammarRule('Param', [
            ['Type', ':', 'Identifier']
        ])
        
        # Blocks and statements
        self.rules['Block'] = GrammarRule('Block', [
            ['{', 'Statement*', '}']
        ])
        
        self.rules['Statement'] = GrammarRule('Statement', [
            ['VarDecl'],
            ['Assignment'],
            ['IfStmt'],
            ['WhileStmt'],
            ['ForStmt'],
            ['ReturnStmt'],
            ['CallStmt'],
            ['DeferStmt']
        ])
        
        self.rules['VarDecl'] = GrammarRule('VarDecl', [
            ['Type', ':', 'Identifier', '=', 'Expr', ';']
        ])
        
        self.rules['Assignment'] = GrammarRule('Assignment', [
            ['Identifier', '=', 'Expr', ';']
        ])
        
        self.rules['IfStmt'] = GrammarRule('IfStmt', [
            ['if', '(', 'Expr', ')', 'Block'],
            ['if', '(', 'Expr', ')', 'Block', 'else', 'Block']
        ])
        
        self.rules['WhileStmt'] = GrammarRule('WhileStmt', [
            ['while', '(', 'Expr', ')', 'Block']
        ])
        
        self.rules['ForStmt'] = GrammarRule('ForStmt', [
            ['for', '(', 'Identifier', 'till', 'Expr', ')', 'Block']
        ])
        
        self.rules['ReturnStmt'] = GrammarRule('ReturnStmt', [
            ['return', 'Expr', ';']
        ])
        
        self.rules['DeferStmt'] = GrammarRule('DeferStmt', [
            ['defer', 'Block']
        ])
        
        # Expressions
        self.rules['Expr'] = GrammarRule('Expr', [
            ['Literal'],
            ['Identifier'],
            ['BinOp'],
            ['UnaryOp'],
            ['Call'],
            ['(', 'Expr', ')']
        ])
        
        self.rules['BinOp'] = GrammarRule('BinOp', [
            ['Expr', 'Operator', 'Expr']
        ])
        
        self.rules['UnaryOp'] = GrammarRule('UnaryOp', [
            ['Operator', 'Expr']
        ])
        
        self.rules['Call'] = GrammarRule('Call', [
            ['Identifier', '(', 'ArgList?', ')']
        ])
        
        self.rules['ArgList'] = GrammarRule('ArgList', [
            ['Expr'],
            ['Expr', ',', 'ArgList']
        ])
        
        # Literals
        self.rules['Literal'] = GrammarRule('Literal', [
            ['IntLiteral'],
            ['FloatLiteral'],
            ['BoolLiteral'],
            ['StringLiteral']
        ])
    
    def get_rule(self, name: str) -> Optional[GrammarRule]:
        """Get a grammar rule by name."""
        return self.rules.get(name)


# Module interface
def load_type_system(aria_root: Path) -> TypeSystemParser:
    """Load the type system from specs."""
    spec_path = aria_root / 'aria_ecosystem' / 'specs' / 'TYPE_SYSTEM.md'
    return TypeSystemParser(spec_path)


def load_operators() -> OperatorParser:
    """Load operator definitions."""
    return OperatorParser()


def load_grammar() -> GrammarParser:
    """Load grammar rules."""
    return GrammarParser()


if __name__ == '__main__':
    # Test the parser
    aria_root = Path(__file__).parent.parent.parent.parent
    
    print("Loading type system...")
    type_sys = load_type_system(aria_root)
    
    print(f"\nFound {len(type_sys.types)} types:")
    for cat in TypeCategory:
        types = type_sys.get_types_by_category(cat)
        if types:
            print(f"  {cat.name}: {', '.join(t.name for t in types)}")
    
    print("\nInteger types with bounds:")
    for t in type_sys.get_types_by_category(TypeCategory.INTEGER):
        print(f"  {t.name}: [{t.min_value}, {t.max_value}]")
    
    print("\nLoading operators...")
    ops = load_operators()
    print(f"Found {len(ops.operators)} operators")
    
    print("\nBinary operators for integers:")
    int_ops = ops.get_binary_operators(TypeCategory.INTEGER)
    print(f"  {', '.join(op.symbol for op in int_ops)}")
    
    print("\nLoading grammar...")
    grammar = load_grammar()
    print(f"Found {len(grammar.rules)} grammar rules")
    
    print("\nSample rules:")
    for rule_name in ['Function', 'IfStmt', 'ForStmt']:
        rule = grammar.get_rule(rule_name)
        print(f"  {rule}")
