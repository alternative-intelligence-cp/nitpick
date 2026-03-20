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
    FIXED = auto()    # fix256
    TBB = auto()      # Twisted Balanced Binary
    TRIT = auto()     # Balanced ternary (trit/tryte/nit/nyte)
    FRAC = auto()     # Exact rational (frac8-64)
    TFP = auto()      # Twisted FP (tfp32/64)
    VEC = auto()      # Vector types (vec2/3/9)
    BOOL = auto()
    STRING = auto()
    VOID = auto()
    RESULT = auto()
    WILD = auto()
    ATOMIC = auto()
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
        # Aria uses its own type names directly — no remapping needed.
        # Short aliases (i8, u32 etc.) are not used in Aria source.
        type_map = {
            'i8': 'int8', 'i16': 'int16', 'i32': 'int32', 'i64': 'int64',
            'u8': 'uint8', 'u16': 'uint16', 'u32': 'uint32', 'u64': 'uint64',
        }
        return type_map.get(self.name, self.name)
    
    def literal_suffix(self) -> str:
        """Get the suffix for literals (e.g., 'i32' for 42i32)."""
        # Suffix map for types where suffix != type name
        suffix_map = {
            'flt32': 'f32', 'flt64': 'f64', 'flt128': 'f128',
            'flt256': 'f256', 'flt512': 'f512',
            'uint1': 'u8', 'uint2': 'u8', 'uint4': 'u8',
            'uint8': 'u8', 'uint16': 'u16', 'uint32': 'u32', 'uint64': 'u64',
            'uint128': 'u128', 'uint256': 'u256', 'uint512': 'u512',
            'uint1024': 'u1024', 'uint2048': 'u2048', 'uint4096': 'u4096',
            'int1': 'i8', 'int2': 'i8', 'int4': 'i8',
            'int8': 'i8', 'int16': 'i16', 'int32': 'i32', 'int64': 'i64',
            'int128': 'i128', 'int256': 'i256', 'int512': 'i512',
            'int1024': 'i1024', 'int2048': 'i2048', 'int4096': 'i4096',
            'fix256': 'fix256',
            'tbb8': 'tbb8', 'tbb16': 'tbb16', 'tbb32': 'tbb32', 'tbb64': 'tbb64',
            'bool': '',
        }
        return suffix_map.get(self.name, self.name)


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
    """Defines the complete Aria type system from the spec."""
    
    def __init__(self, aria_root: Path = None):
        self.types: Dict[str, PrimitiveType] = {}
        self._define_all_types()
    
    def _define_all_types(self):
        """Hardcode all types from aria_specs.txt — no file parsing needed."""
        T = PrimitiveType
        C = TypeCategory
        
        # --- Signed integers ---
        self.types['int1']   = T('int1',   C.INTEGER, 1,   min_value=-1,                    max_value=0)
        self.types['int2']   = T('int2',   C.INTEGER, 2,   min_value=-2,                    max_value=1)
        self.types['int4']   = T('int4',   C.INTEGER, 4,   min_value=-8,                    max_value=7)
        self.types['int8']   = T('int8',   C.INTEGER, 8,   min_value=-128,                  max_value=127)
        self.types['int16']  = T('int16',  C.INTEGER, 16,  min_value=-32768,                max_value=32767)
        self.types['int32']  = T('int32',  C.INTEGER, 32,  min_value=-2147483648,           max_value=2147483647)
        self.types['int64']  = T('int64',  C.INTEGER, 64,  min_value=-(2**63),              max_value=2**63-1)
        self.types['int128'] = T('int128', C.INTEGER, 128, min_value=None,                  max_value=None)
        self.types['int256'] = T('int256', C.INTEGER, 256, min_value=None,                  max_value=None)
        self.types['int512'] = T('int512', C.INTEGER, 512, min_value=None,                  max_value=None)
        # Large LBIM types: use small safe literals
        for name, bits in [('int1024',1024),('int2048',2048),('int4096',4096)]:
            self.types[name] = T(name, C.INTEGER, bits, min_value=None, max_value=None)
        
        # --- Unsigned integers ---
        self.types['uint1']   = T('uint1',   C.UNSIGNED, 1,   min_value=0, max_value=1)
        self.types['uint2']   = T('uint2',   C.UNSIGNED, 2,   min_value=0, max_value=3)
        self.types['uint4']   = T('uint4',   C.UNSIGNED, 4,   min_value=0, max_value=15)
        self.types['uint8']   = T('uint8',   C.UNSIGNED, 8,   min_value=0, max_value=255)
        self.types['uint16']  = T('uint16',  C.UNSIGNED, 16,  min_value=0, max_value=65535)
        self.types['uint32']  = T('uint32',  C.UNSIGNED, 32,  min_value=0, max_value=4294967295)
        self.types['uint64']  = T('uint64',  C.UNSIGNED, 64,  min_value=0, max_value=None)
        self.types['uint128'] = T('uint128', C.UNSIGNED, 128, min_value=0, max_value=None)
        self.types['uint256'] = T('uint256', C.UNSIGNED, 256, min_value=0, max_value=None)
        self.types['uint512'] = T('uint512', C.UNSIGNED, 512, min_value=0, max_value=None)
        for name, bits in [('uint1024',1024),('uint2048',2048),('uint4096',4096)]:
            self.types[name] = T(name, C.UNSIGNED, bits, min_value=0, max_value=None)
        
        # --- Floating point ---
        for name, bits in [('flt32',32),('flt64',64),('flt128',128),('flt256',256),('flt512',512)]:
            self.types[name] = T(name, C.FLOAT, bits)
        
        # --- Fixed point ---
        self.types['fix256'] = T('fix256', C.FIXED, 256)
        
        # --- Twisted Balanced Binary ---
        self.types['tbb8']  = T('tbb8',  C.TBB, 8,  min_value=-127,       max_value=127)
        self.types['tbb16'] = T('tbb16', C.TBB, 16, min_value=-32767,     max_value=32767)
        self.types['tbb32'] = T('tbb32', C.TBB, 32, min_value=-2147483647,max_value=2147483647)
        self.types['tbb64'] = T('tbb64', C.TBB, 64, min_value=None,       max_value=None)
        
        # --- Balanced ternary / nonary ---
        self.types['trit']  = T('trit',  C.TRIT, 8,  min_value=-1, max_value=1)
        self.types['tryte'] = T('tryte', C.TRIT, 16, min_value=None, max_value=None)
        self.types['nit']   = T('nit',   C.TRIT, 8,  min_value=-4, max_value=4)
        self.types['nyte']  = T('nyte',  C.TRIT, 16, min_value=None, max_value=None)
        
        # --- Fractions ---
        for name, bits in [('frac8',8),('frac16',16),('frac32',32),('frac64',64)]:
            self.types[name] = T(name, C.FRAC, bits)
        
        # --- Twisted floating point ---
        self.types['tfp32'] = T('tfp32', C.TFP, 32)
        self.types['tfp64'] = T('tfp64', C.TFP, 64)
        
        # --- Primitives ---
        self.types['bool']   = T('bool',   C.BOOL,   8)
        self.types['string'] = T('string', C.STRING, 0)
        self.types['NIL']    = T('NIL',    C.VOID,   0)
    
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
        
        # Ternary (Aria: is COND : TRUE_EXPR : FALSE_EXPR)
        self.operators['is::'] = Operator('is::', 'ternary', 10, 'right', 3, set())
        
        # Null coalescing / unwrap
        self.operators['??'] = Operator('??', 'null_coalesce', 11, 'right', 2, set())
        self.operators['?']  = Operator('?',  'unwrap',        11, 'right', 1, set())
        self.operators['?!'] = Operator('?!', 'emphatic_unwrap',11,'right', 1, set())
        
        # Spaceship
        all_ord = {TypeCategory.INTEGER, TypeCategory.UNSIGNED, TypeCategory.FLOAT, TypeCategory.TBB}
        self.operators['<=>'] = Operator('<=>', 'spaceship', 4, 'left', 2, all_ord)
        
        # Cast (infix)
        self.operators['=>'] = Operator('=>', 'cast', 12, 'right', 2, set())
        
        # Pipeline
        self.operators['|>'] = Operator('|>', 'pipe_fwd', 13, 'left', 2, set())
        self.operators['<|'] = Operator('<|', 'pipe_bwd', 13, 'right', 2, set())
    
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
        
        # Struct declaration
        self.rules['StructDecl'] = GrammarRule('StructDecl', [
            ['struct', ':', 'Identifier', '=', '{', 'FieldList+', '}', ';']
        ])
        
        # Enum declaration
        self.rules['EnumDecl'] = GrammarRule('EnumDecl', [
            ['enum', ':', 'Identifier', '=', '{', 'EnumValueList+', '}', ';']
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
        
        # for range: for (i in 1..N) { }
        self.rules['ForStmt'] = GrammarRule('ForStmt', [
            ['for', '(', 'Identifier', 'in', 'Expr', '..', 'Expr', ')', 'Block']
        ])
        
        # till loop: till(N, step) { ... $ ... }
        self.rules['TillStmt'] = GrammarRule('TillStmt', [
            ['till', '(', 'Expr', ',', 'Expr', ')', 'Block']
        ])
        
        # loop + break  OR  loop(start, end, step) { $ }
        self.rules['LoopStmt'] = GrammarRule('LoopStmt', [
            ['loop', 'Block'],
            ['loop', '(', 'Expr', ',', 'Expr', ',', 'Expr', ')', 'Block']
        ])
        
        # pass/fail (not return)
        self.rules['PassStmt'] = GrammarRule('PassStmt', [
            ['pass', '(', 'Expr', ')', ';']
        ])
        
        # pick statement
        # pick statement: pick (expr) { (val) { ... }, (*) { ... } }
        self.rules['PickStmt'] = GrammarRule('PickStmt', [
            ['pick', '(', 'Expr', ')', '{', 'PickCase+', '}']
        ])
        
        # when/then/end
        self.rules['WhenStmt'] = GrammarRule('WhenStmt', [
            ['when', '(', 'Expr', ')', 'Block', 'then', 'Block', 'end', 'Block']
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
def load_type_system(aria_root: Path = None) -> TypeSystemParser:
    """Load the complete Aria type system."""
    return TypeSystemParser()


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
