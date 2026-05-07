#!/usr/bin/env python3
"""
EMI Mutation Engine for Nitpick (npkc)

Equivalence Modulo Inputs (EMI) testing: given a seed program that compiles and
produces a known exit code, generate semantically equivalent variants by:

  1. Dead-code injection    — insert unreachable blocks that cannot affect output
  2. Identity expressions   — wrap values in arithmetic identities (x+0, x*1, x-0)
  3. Statement reordering   — swap adjacent independent variable declarations
  4. Redundant assignments  — insert "x = x;" for mutable bindings
  5. No-op block insertion  — insert empty or trivially-safe fixed-binding blocks

If any variant produces a different exit code or stdout → miscompile found.

Design constraints:
  - Works at the source-text level (no AST required)
  - Conservative: only mutations that are provably semantics-preserving
  - Nitpick-aware: respects func:main/func:failsafe structure, fixed vs mutable,
    exit vs pass, println() string-only constraint
"""

import re
import random
from typing import List, Tuple, Optional
from dataclasses import dataclass, field
from enum import Enum, auto


class MutationType(Enum):
    DEAD_CODE_INJECT   = auto()
    IDENTITY_EXPR      = auto()
    STMT_REORDER       = auto()
    REDUNDANT_ASSIGN   = auto()
    NOOP_BLOCK         = auto()
    # Phase 3: Synthesis mutations
    SYNTH_DEAD_FUNC    = auto()  # dead helper function at file scope
    SYNTH_DEAD_STRUCT  = auto()  # dead struct definition at file scope
    COMPLEX_DEAD_BLOCK = auto()  # complex multi-step arithmetic in dead block


@dataclass
class Mutation:
    kind: MutationType
    description: str


@dataclass
class Variant:
    source: str
    mutations: List[Mutation] = field(default_factory=list)
    seed_path: str = ""


# ---------------------------------------------------------------------------
# Source-level helpers
# ---------------------------------------------------------------------------

# Matches a mutable int32/int64/tbb32/tbb8/tbb16/tbb64 declaration:
#   int32:name = expr;
_MUTABLE_DECL_RE = re.compile(
    r'^(\s*)(int8|int16|int32|int64|uint8|uint16|uint32|uint64|tbb8|tbb16|tbb32|tbb64)'
    r'(:[\w]+\s*=\s*[^;]+;)',
    re.MULTILINE
)

# Matches a simple integer literal used as an rvalue (not in strings, not in exit)
_INT_LITERAL_RE = re.compile(r'\b(\d+)\b')

# Dead-code block templates — these are provably unreachable
_DEAD_BLOCKS = [
    # if (0 == 1) block (never executes) — parens required in Nitpick
    lambda indent, body: f"{indent}if (0 == 1) {{\n{indent}    {body}\n{indent}}}",
]

_DEAD_BLOCK_BODIES = [
    'fixed int32:_dead = 0;',
    'fixed int32:_dead = 42;',
    'fixed int32:_dead = 1 + 1;',
]

# Identity wrappers: given an integer expression string, wrap it
_IDENTITIES = [
    lambda e: f"({e} + 0)",
    lambda e: f"({e} - 0)",
    lambda e: f"({e} * 1)",
    lambda e: f"(0 + {e})",
    lambda e: f"(1 * {e})",
]


# ---------------------------------------------------------------------------
# Core mutation functions
# ---------------------------------------------------------------------------

def _safe_insertion_points(lines: list, main_start: int, main_end: int) -> list:
    """
    Return line indices inside func:main where it is safe to insert a new statement.
    A line is safe if:
      - paren depth is 0 (not inside a multi-line call or initializer)
      - bracket depth is 0 (not inside a multi-line array literal)
      - brace depth is exactly 1 (direct statement in func:main body, not inside
        a nested struct literal, if-body, or inner scope block)
      - the line ends with ';' (complete statement)
      - it isn't an exit/pass/fail line
      - it isn't blank or a comment
    """
    paren_depth   = 0
    bracket_depth = 0
    brace_depth   = 1   # we start already inside func:main's opening brace
    safe = []
    for i in range(main_start, main_end - 1):
        line = lines[i]
        # Check depths BEFORE processing current line (state entering this line).
        # This correctly excludes closing-delimiter lines like `15.0];` or `};`.
        if (paren_depth == 0
                and bracket_depth == 0
                and brace_depth == 1
                and not re.search(r'\bexit\b|\bpass\b|\bfail\b', line)
                and line.strip() != ''
                and not line.strip().startswith('//')
                and line.strip().endswith(';')):
            safe.append(i)
        # Update depths after the check
        paren_depth   += line.count('(') - line.count(')')
        bracket_depth += line.count('[') - line.count(']')
        brace_depth   += line.count('{') - line.count('}')
    return safe


def _inject_dead_code(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """Insert an unreachable if-block at a random safe insertion point inside func:main."""
    lines = source.split('\n')
    main_start = None
    main_end = None
    brace_depth = 0
    in_main = False

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth > 0 and main_start is None:
                main_start = i + 1
            if brace_depth == 0 and main_start is not None:
                main_end = i
                break

    if main_start is None or main_end is None or main_end - main_start < 2:
        return None

    insert_candidates = _safe_insertion_points(lines, main_start, main_end)
    if not insert_candidates:
        return None

    insert_at = rng.choice(insert_candidates)
    indent = re.match(r'^(\s*)', lines[insert_at]).group(1)
    body = rng.choice(_DEAD_BLOCK_BODIES)
    template = rng.choice(_DEAD_BLOCKS)
    dead_block = template(indent, body)

    new_lines = lines[:insert_at] + [dead_block] + lines[insert_at:]
    return (
        '\n'.join(new_lines),
        Mutation(MutationType.DEAD_CODE_INJECT,
                 f"inserted dead block at line {insert_at}")
    )


def _apply_identity_expr(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    Wrap a simple integer literal in an arithmetic identity inside a mutable
    variable declaration. Avoids println() arguments (string-only constraint).
    """
    # Find all mutable int declarations in main
    lines = source.split('\n')
    candidates = []
    in_main = False
    brace_depth = 0

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth == 0 and in_main:
                in_main = False
            # Only touch mutable int32/int64 declarations (not smaller types — identity
            # expressions produce int32 and would cause a type error on int8/int16/uint*)
            if (in_main and brace_depth > 0
                    and re.match(r'^\s*(int32|int64):', line)
                    and 'println' not in line
                    and 'exit' not in line
                    and 'fixed' not in line):
                candidates.append(i)

    if not candidates:
        return None

    target_line_idx = rng.choice(candidates)
    line = lines[target_line_idx]

    # Find the rvalue portion (after '=')
    eq_pos = line.find('=')
    if eq_pos == -1:
        return None
    rvalue = line[eq_pos + 1:].rstrip().rstrip(';').strip()

    # Only wrap simple integer literals to be safe
    if not re.fullmatch(r'\d+', rvalue):
        return None

    wrapper = rng.choice(_IDENTITIES)
    new_rvalue = wrapper(rvalue)
    new_line = line[:eq_pos + 1] + ' ' + new_rvalue + ';'
    lines[target_line_idx] = new_line

    return (
        '\n'.join(lines),
        Mutation(MutationType.IDENTITY_EXPR,
                 f"wrapped literal '{rvalue}' with identity on line {target_line_idx}")
    )


def _reorder_adjacent_decls(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    Swap two adjacent independent mutable variable declarations inside func:main.
    They are independent if neither references the other's variable name.
    """
    lines = source.split('\n')
    in_main = False
    brace_depth = 0
    decl_lines = []

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth == 0:
                in_main = False
            if in_main and brace_depth > 0 and _MUTABLE_DECL_RE.match(line):
                decl_lines.append(i)

    # A rvalue is pure if it contains only literals, arithmetic operators, parens,
    # and no identifiers that could be variables/calls with side effects.
    # We conservatively allow only: digits, spaces, +/-/*/parens, and nothing else.
    def _is_pure_literal_expr(rvalue: str) -> bool:
        stripped = rvalue.strip().rstrip(';').strip()
        # Allow only: digits, spaces, basic arithmetic, parens
        return bool(re.fullmatch(r'[\d\s+\-*/()]+', stripped))

    # Find adjacent pairs that are truly independent with pure rvalues
    adjacent_pairs = []
    for j in range(len(decl_lines) - 1):
        a, b = decl_lines[j], decl_lines[j + 1]
        if b != a + 1:
            continue  # not adjacent
        # Extract variable names
        ma = re.search(r':(\w+)\s*=', lines[a])
        mb = re.search(r':(\w+)\s*=', lines[b])
        if not ma or not mb:
            continue
        name_a = ma.group(1)
        name_b = mb.group(1)
        rvalue_a = lines[a][lines[a].find('=') + 1:]
        rvalue_b = lines[b][lines[b].find('=') + 1:]
        # Both rvalues must be pure literal expressions (no calls, no side effects)
        if not _is_pure_literal_expr(rvalue_a) or not _is_pure_literal_expr(rvalue_b):
            continue
        # Independent: b's rvalue doesn't use a's name, a's rvalue doesn't use b's name
        if re.search(r'\b' + re.escape(name_a) + r'\b', rvalue_b):
            continue
        if re.search(r'\b' + re.escape(name_b) + r'\b', rvalue_a):
            continue
        adjacent_pairs.append((a, b))

    if not adjacent_pairs:
        return None

    a, b = rng.choice(adjacent_pairs)
    lines[a], lines[b] = lines[b], lines[a]

    return (
        '\n'.join(lines),
        Mutation(MutationType.STMT_REORDER,
                 f"swapped independent declarations at lines {a} and {b}")
    )


def _insert_redundant_assign(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    After a mutable variable declaration, insert a redundant self-assignment: x = x;
    This is semantically a no-op for all types.
    """
    lines = source.split('\n')
    in_main = False
    brace_depth = 0
    candidates = []

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth == 0:
                in_main = False
            if in_main and brace_depth > 0 and _MUTABLE_DECL_RE.match(line):
                m = re.search(r':(\w+)\s*=', line)
                if m:
                    candidates.append((i, m.group(1)))

    if not candidates:
        return None

    insert_after, varname = rng.choice(candidates)
    indent = re.match(r'^(\s*)', lines[insert_after]).group(1)
    redund = f"{indent}{varname} = {varname};"

    new_lines = lines[:insert_after + 1] + [redund] + lines[insert_after + 1:]
    return (
        '\n'.join(new_lines),
        Mutation(MutationType.REDUNDANT_ASSIGN,
                 f"inserted redundant '{varname} = {varname};' after line {insert_after}")
    )


def _insert_noop_block(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """Insert a no-op fixed-binding block inside func:main. Scoped, no side effects."""
    lines = source.split('\n')
    main_start = None
    main_end = None
    brace_depth = 0
    in_main = False

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth > 0 and main_start is None:
                main_start = i + 1
            if brace_depth == 0 and main_start is not None:
                main_end = i
                break

    if main_start is None or main_end is None or main_end - main_start < 2:
        return None

    insert_candidates = _safe_insertion_points(lines, main_start, main_end)
    if not insert_candidates:
        return None

    insert_at = rng.choice(insert_candidates)
    indent = re.match(r'^(\s*)', lines[insert_at]).group(1)
    val = rng.randint(0, 127)
    noop = f"{indent}{{\n{indent}    fixed int32:_noop = {val};\n{indent}}}"

    new_lines = lines[:insert_at] + [noop] + lines[insert_at:]
    return (
        '\n'.join(new_lines),
        Mutation(MutationType.NOOP_BLOCK,
                 f"inserted no-op block at line {insert_at}")
    )


# ---------------------------------------------------------------------------
# Phase 3: Synthesis mutations
# ---------------------------------------------------------------------------

def _find_toplevel_insert_pos(source: str) -> Optional[int]:
    """
    Return the character offset of the start of 'func:main = ' in source.
    We insert synthesized top-level declarations (structs, functions) here so
    they appear at file scope but before func:main.
    """
    m = re.search(r'^func:main\s*=', source, re.MULTILINE)
    return m.start() if m else None


def _synth_dead_func(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    Inject a synthesized dead helper function at file scope (never called from
    func:main).  Exercises: function-definition parsing, type checking, register
    allocation, and function-table building — all with a fresh, compiler-unseen
    name.

    Shape:
        func:_emi_deadN = int32() {
            int32:_da = K1;
            int32:_db = K2;
            int32:_dc = _da + _db;
            int32:_dd = _dc - K3;
            pass _dd;
        };
    """
    pos = _find_toplevel_insert_pos(source)
    if pos is None:
        return None

    n = rng.randint(10000, 99999)
    k1 = rng.randint(1, 200)
    k2 = rng.randint(1, 200)
    k3 = rng.randint(1, 200)

    # Synthesize 3-5 arithmetic steps
    ops = ['+', '-', '+', '-', '+']
    rng.shuffle(ops)
    num_steps = rng.randint(3, 5)

    lines = [
        f"func:_emi_dead{n} = int32() {{",
        f"    int32:_da = {k1};",
        f"    int32:_db = {k2};",
        f"    int32:_dc = _da {ops[0]} _db;",
    ]
    if num_steps >= 4:
        lines.append(f"    int32:_dd = _dc {ops[1]} {k3};")
        last = "_dd"
    else:
        last = "_dc"
    if num_steps >= 5:
        k4 = rng.randint(1, 200)
        lines.append(f"    int32:_de = {last} {ops[2]} {k4};")
        last = "_de"
    lines.append(f"    pass {last};")
    lines.append("};")
    lines.append("")  # blank line after

    dead_func = "\n".join(lines) + "\n"
    new_source = source[:pos] + dead_func + source[pos:]
    return (
        new_source,
        Mutation(MutationType.SYNTH_DEAD_FUNC,
                 f"injected dead function _emi_dead{n} ({num_steps} steps)")
    )


def _synth_dead_struct(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    Inject a synthesized dead struct definition at file scope (never instantiated
    in live code).  Exercises: struct-layout code, field-index calculation, and
    name-table building.

    Generates 2-4 fields with randomly shuffled int32/int64 types — the shuffle
    constitutes the 'field reordering' called for by the Phase 3 spec.

    Shape:
        struct:_EmiDeadN = {
            int32:_fa;
            int64:_fb;
            int32:_fc;
        };
    """
    pos = _find_toplevel_insert_pos(source)
    if pos is None:
        return None

    n = rng.randint(10000, 99999)
    num_fields = rng.randint(2, 4)

    # Field type pool — shuffle gives random layout (the 'reordering')
    type_pool = ['int32', 'int64', 'int32', 'int64']
    rng.shuffle(type_pool)
    field_names = ['_fa', '_fb', '_fc', '_fd']

    fields = "\n".join(
        f"    {type_pool[i]}:{field_names[i]};"
        for i in range(num_fields)
    )

    dead_struct = f"struct:_EmiDead{n} = {{\n{fields}\n}};\n\n"
    new_source = source[:pos] + dead_struct + source[pos:]
    return (
        new_source,
        Mutation(MutationType.SYNTH_DEAD_STRUCT,
                 f"injected dead struct _EmiDead{n} ({num_fields} fields)")
    )


def _complex_dead_block(source: str, rng: random.Random) -> Optional[Tuple[str, Mutation]]:
    """
    Inject a multi-statement dead arithmetic block inside func:main.

    Unlike the simpler DEAD_CODE_INJECT (single-line body), this generates a
    chain of 3-5 int32 operations — stressing constant-folding, dead-code
    elimination, and the compiler's handling of sequential dependent assignments
    inside unreachable branches.

    Shape:
        if (0 == 1) {
            int32:_cbN_1 = K1;
            int32:_cbN_2 = K2;
            int32:_cbN_3 = _cbN_1 + _cbN_2;
            int32:_cbN_4 = _cbN_3 - K3;
            int32:_cbN_5 = _cbN_4 + K4;
        }

    Variable names include a random N to avoid clashes when multiple mutations
    are applied to the same source.
    """
    lines = source.split('\n')
    main_start = None
    main_end = None
    brace_depth = 0
    in_main = False

    for i, line in enumerate(lines):
        if re.search(r'func:main\s*=', line):
            in_main = True
        if in_main:
            brace_depth += line.count('{') - line.count('}')
            if brace_depth > 0 and main_start is None:
                main_start = i + 1
            if brace_depth == 0 and main_start is not None:
                main_end = i
                break

    if main_start is None or main_end is None or main_end - main_start < 2:
        return None

    insert_candidates = _safe_insertion_points(lines, main_start, main_end)
    if not insert_candidates:
        return None

    insert_at = rng.choice(insert_candidates)
    indent = re.match(r'^(\s*)', lines[insert_at]).group(1)
    inner = indent + "    "

    n  = rng.randint(10000, 99999)
    k1 = rng.randint(1, 100)
    k2 = rng.randint(1, 100)
    k3 = rng.randint(1, 100)
    k4 = rng.randint(1, 100)
    ops = ['+', '-', '+', '-']
    rng.shuffle(ops)
    num_steps = rng.randint(3, 5)

    body_lines = [
        f"{inner}int32:_cb{n}_1 = {k1};",
        f"{inner}int32:_cb{n}_2 = {k2};",
        f"{inner}int32:_cb{n}_3 = _cb{n}_1 {ops[0]} _cb{n}_2;",
    ]
    if num_steps >= 4:
        body_lines.append(f"{inner}int32:_cb{n}_4 = _cb{n}_3 {ops[1]} {k3};")
    if num_steps >= 5:
        body_lines.append(f"{inner}int32:_cb{n}_5 = _cb{n}_4 {ops[2]} {k4};")

    body = "\n".join(body_lines)
    dead_block = f"{indent}if (0 == 1) {{\n{body}\n{indent}}}"

    new_lines = lines[:insert_at] + [dead_block] + lines[insert_at:]
    return (
        '\n'.join(new_lines),
        Mutation(MutationType.COMPLEX_DEAD_BLOCK,
                 f"inserted complex dead block at line {insert_at} ({num_steps} steps, id={n})")
    )


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

_MUTATORS = [
    _inject_dead_code,
    _apply_identity_expr,
    _reorder_adjacent_decls,
    _insert_redundant_assign,
    _insert_noop_block,
    # Phase 3: synthesis mutations
    _synth_dead_func,
    _synth_dead_struct,
    _complex_dead_block,
]


def generate_variants(source: str, seed_path: str, count: int,
                      rng: random.Random, max_mutations_per_variant: int = 3) -> List[Variant]:
    """
    Generate up to `count` distinct EMI variants from `source`.
    Each variant applies 1..max_mutations_per_variant mutations.
    Returns only variants whose source differs from the original.
    """
    variants: List[Variant] = []
    seen: set = set()
    seen.add(source)
    attempts = 0
    max_attempts = count * 10

    while len(variants) < count and attempts < max_attempts:
        attempts += 1
        current = source
        applied: List[Mutation] = []
        n_mutations = rng.randint(1, max_mutations_per_variant)

        mutator_pool = list(_MUTATORS)
        rng.shuffle(mutator_pool)

        for mutator in mutator_pool[:n_mutations]:
            result = mutator(current, rng)
            if result is not None:
                current, mut = result
                applied.append(mut)

        if current not in seen and applied:
            seen.add(current)
            v = Variant(source=current, mutations=applied, seed_path=seed_path)
            variants.append(v)

    return variants
