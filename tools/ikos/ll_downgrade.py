#!/usr/bin/env python3
"""
ll_downgrade.py — Convert LLVM 20 opaque-pointer .ll to LLVM 14 typed-pointer .ll

Ariac emits LLVM IR with opaque 'ptr' (mandatory since LLVM 15).
IKOS 3.5 requires LLVM 14 with typed pointers (i64*, i8*, etc.).

Handles:
- ptr → typed pointers (i64*, i8*, etc.) in load/store/GEP
- ptr → i8* for remaining uses (params, structs, returns)
- memory(none) → readnone, memory(read) → readonly, memory(write) → writeonly
- Bitcast insertion for global reference type mismatches
- Bitcast insertion for local variable type mismatches (i8* → struct pointer)
- Preserves register names containing 'ptr' (e.g., %ptr, %env_ptr, %x.ptr)
- Strips GEP nuw flag (LLVM 15+ only)
"""

import re
import sys

# Match an LLVM type token (includes 'ptr' for opaque pointer matching before conversion)
TYPE_RE = r'(?:ptr|i\d+|float|double|half|fp128|void|%[\w.]+|\{[^}]*\}|\[[^\]]*\]|<\d+\s+x\s+(?:i\d+|float|double|half|fp128)>)'


def convert_load(line):
    """load <type>, ptr -> load <type>, <type>*"""
    m = re.search(r'(load\s+)(' + TYPE_RE + r')(,\s+)ptr(\s+)', line)
    if m:
        prefix, ty, comma, space = m.groups()
        replacement = f'{prefix}{ty}{comma}{ty}*{space}'
        line = line[:m.start()] + replacement + line[m.end():]
    return line


def convert_store(line):
    """store <type> <val>, ptr -> store <type> <val>, <type>*"""
    # Extract the store value type
    tm = re.search(r'store\s+(' + TYPE_RE + r')', line)
    if not tm:
        return line
    ty = tm.group(1)

    # Special case: store destination is a constant expression (getelementptr)
    # e.g., store i64 99, ptr getelementptr (i64, ptr @global_numbers, i64 1)
    # Here we need to convert the outer ', ptr getelementptr' first
    ce_m = re.search(r',(\s+)ptr(\s+)(getelementptr\b)', line)
    if ce_m:
        # Convert the destination ptr to ty* (matching stored value type)
        replacement = f',{ce_m.group(1)}{ty}*{ce_m.group(2)}{ce_m.group(3)}'
        line = line[:ce_m.start()] + replacement + line[ce_m.end():]
        # Now handle GEP's inner ptr separately
        line = convert_gep(line)
        return line

    # Find the LAST ', ptr ' — the destination pointer.
    # Using last match avoids confusing 'ptr' inside struct initializer values
    # (e.g., store { i32, ptr, i8 } { i32 42, ptr null, i8 0 }, ptr %dest)
    matches = list(re.finditer(r',(\s+)ptr(\s+)', line))
    if not matches:
        return line
    last = matches[-1]

    replacement = f',{last.group(1)}{ty}*{last.group(2)}'
    line = line[:last.start()] + replacement + line[last.end():]
    return line


def convert_gep(line):
    """getelementptr [inbounds] [nuw] <type>, ptr -> getelementptr [inbounds] <type>, <type>*
    Also handles constant expression form: getelementptr (<type>, ptr ...)
    """
    # Strip nuw/nsw flags from GEP (LLVM 15+ only)
    line = re.sub(r'(getelementptr\s+(?:inbounds\s+)?)nuw\s+', r'\1', line)
    m = re.search(r'(getelementptr\s+(?:inbounds\s+)?\(?\s*)(' + TYPE_RE + r')(,\s+)ptr(\s+)', line)
    if m:
        prefix, ty, comma, space = m.groups()
        replacement = f'{prefix}{ty}{comma}{ty}*{space}'
        line = line[:m.start()] + replacement + line[m.end():]
    return line


def convert_splat(line):
    """Convert LLVM 20 'splat (type val)' to LLVM 14 vector constant.

    splat (i32 42) in a <4 x i32> context → <i32 42, i32 42, i32 42, i32 42>
    Handles both:
      store <4 x i32> splat (i32 42), ...
      icmp sge <4 x i32> %v, splat (i32 10)
    """
    # Find splat(type val) and look backwards for <N x T> vector type
    m = re.search(r'splat\s+\((' + TYPE_RE + r')\s+([^)]+)\)', line)
    if not m:
        return line
    elem_type = m.group(1)
    elem_val = m.group(2)

    # Find the vector type <N x T> preceding the splat
    vec_m = re.search(r'<(\d+)\s+x\s+' + TYPE_RE + r'>', line[:m.start()])
    if not vec_m:
        return line
    count = int(vec_m.group(1))

    elems = ', '.join([f'{elem_type} {elem_val}'] * count)
    replacement = f'<{elems}>'
    line = line[:m.start()] + replacement + line[m.end():]
    return line


def convert_memory_attrs(line):
    """Convert LLVM 16+ memory() attributes to LLVM 14 equivalents."""
    # memory(none) → readnone
    line = re.sub(r'\bmemory\(none\)', 'readnone', line)
    # memory(read) → readonly
    line = re.sub(r'\bmemory\(read\)', 'readonly', line)
    # memory(write) → writeonly
    line = re.sub(r'\bmemory\(write\)', 'writeonly', line)
    # memory(argmem: ...) and other complex forms → strip entirely
    line = re.sub(r'\bmemory\([^)]*\)\s*', '', line)
    return line


def strip_new_attrs(line):
    """Strip LLVM 15+ attributes not recognized by LLVM 14."""
    # presplitcoroutine (LLVM 15+)
    line = re.sub(r'\bpresplitcoroutine\b\s*', '', line)
    # Remove empty attribute group definitions
    if re.match(r'attributes\s+#\d+\s*=\s*\{\s*\}\s*$', line.strip()):
        return ''
    return line


def downgrade_coro_intrinsics(line):
    """Downgrade LLVM 15+ coroutine intrinsics to LLVM 14 signatures.

    LLVM 15 added a 'token' parameter to llvm.coro.end.
    """
    # call i1 @llvm.coro.end(i8* %hdl, i1 false, token none) → drop token arg
    if '@llvm.coro.end' in line:
        line = re.sub(r',\s*token\s+\w+\s*\)', ')', line)
    # declare i1 @llvm.coro.end(i8*, i1, token) → drop token param
    if '@llvm.coro.end' in line and ('declare' in line or 'define' in line):
        line = re.sub(r',\s*token\s*\)', ')', line)
    return line


def fix_sret_byval(line):
    """Fix sret/byval: i8* sret(<type>) → <type>* sret(<type>)."""
    # Replace i8* sret(<type>) with <type>* sret(<type>)
    # and i8* byval(<type>) with <type>* byval(<type>)
    def replacer(m):
        attr = m.group(1)   # sret or byval
        inner_type = m.group(2)
        return f'{inner_type}* {attr}({inner_type})'

    line = re.sub(
        r'i8\*\s+(sret|byval)\((' + TYPE_RE + r')\)',
        replacer,
        line,
    )
    return line


def build_global_types(lines):
    """Build a map of global variable names to their LLVM types."""
    globals_map = {}
    for line in lines:
        m = re.match(
            r'(@[\w.]+)\s+=\s+(?:private\s+|internal\s+|external\s+|linkonce\s+|'
            r'linkonce_odr\s+|weak\s+|weak_odr\s+|common\s+|appending\s+|'
            r'available_externally\s+|dllimport\s+|dllexport\s+)*'
            r'(?:unnamed_addr\s+|local_unnamed_addr\s+)*'
            r'(?:constant|global)\s+(.+)',
            line
        )
        if m:
            name = m.group(1)
            type_and_init = m.group(2)
            ty = extract_type_from_def(type_and_init)
            if ty:
                globals_map[name] = ty
    return globals_map


def build_function_types(lines):
    """Build a map of function names to their full function pointer types (for LLVM 14)."""
    func_types = {}
    for line in lines:
        m = re.match(
            r'(?:define|declare)\s+'
            r'(?:private\s+|internal\s+|external\s+|linkonce\s+|linkonce_odr\s+|'
            r'dso_local\s+|hidden\s+|protected\s+|default\s+)*'
            r'(.+?)\s+(@[\w.]+)\s*\(([^)]*)\)',
            line,
        )
        if m:
            ret_type = m.group(1).strip()
            name = m.group(2)
            params_str = m.group(3)

            # Extract parameter types
            param_types = []
            if params_str.strip():
                for p in params_str.split(','):
                    p = p.strip()
                    # "type %name" or "type noundef %name" or just "type"
                    pm = re.match(r'(' + TYPE_RE + r'\**)', p)
                    if pm:
                        param_types.append(pm.group(1))

            fn_type = f'{ret_type} ({", ".join(param_types)})*'
            func_types[name] = fn_type
    return func_types


def extract_type_from_def(type_and_init):
    """Extract just the type from a global variable type+initializer string."""
    s = type_and_init.strip()

    m = re.match(r'(%[\w.]+)', s)
    if m:
        return m.group(1)

    m = re.match(r'(\[[^\]]+\])', s)
    if m:
        return m.group(1)

    if s.startswith('{'):
        depth = 0
        for i, c in enumerate(s):
            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    return s[:i + 1]

    m = re.match(r'(i\d+|float|double|half|fp128|void)\b', s)
    if m:
        return m.group(1)

    return None


def build_struct_defs(lines):
    """Parse struct type definitions: %Name = type { field1, field2, ... }

    Returns a map of struct name → list of field types.
    """
    struct_defs = {}
    for line in lines:
        m = re.match(r'(%[\w.]+)\s*=\s*type\s*\{([^}]*)\}', line.strip())
        if m:
            name = m.group(1)
            fields_str = m.group(2)
            fields = [f.strip() for f in fields_str.split(',') if f.strip()]
            struct_defs[name] = fields
    return struct_defs


def resolve_gep_result_type(base_type, indices, struct_defs):
    """Resolve the result type of a GEP operation.

    base_type: The aggregate type being indexed into (e.g., %Vec_int8)
    indices: List of constant index values (integers)
    struct_defs: Map of struct names → list of field types

    Returns the pointed-to type (not the pointer) or None if unresolvable.
    """
    current_type = base_type
    for i, idx in enumerate(indices):
        if i == 0:
            # First index steps through array of base_type; result is still base_type
            continue

        # Subsequent indices: index into aggregate type
        if current_type.startswith('%'):
            # Named struct — look up fields
            if current_type in struct_defs:
                fields = struct_defs[current_type]
                if idx < len(fields):
                    current_type = fields[idx]
                else:
                    return None
            else:
                return None
        elif current_type.startswith('{'):
            # Literal struct { field1, field2, ... }
            fields = [f.strip() for f in current_type.strip('{}').split(',') if f.strip()]
            if idx < len(fields):
                current_type = fields[idx]
            else:
                return None
        elif current_type.startswith('['):
            # Array [N x T] → T
            am = re.match(r'\[\d+\s+x\s+(.+)\]', current_type)
            if am:
                current_type = am.group(1).strip()
            else:
                return None
        else:
            return None  # Can't index into scalar type

    return current_type


def fix_phi_globals(line, globals_map):
    """Fix global references in phi nodes that need bitcast to match phi type.

    In typed-pointer IR, phi operands must match the phi type exactly.
    Global refs like @.str may have a different pointer type (e.g., [5 x i8]*)
    than the phi type (e.g., i8*). Insert bitcast constant expressions.
    """
    m = re.match(r'(\s*%[\w.]+\s*=\s*phi\s+)(' + TYPE_RE + r'\**)\s+', line)
    if not m:
        return line
    phi_type = m.group(2)

    def replacer(match):
        bracket_space = match.group(1)
        name = match.group(2)
        if name in globals_map:
            actual_type = globals_map[name]
            global_ptr_type = actual_type + '*'
            if global_ptr_type != phi_type:
                return f'{bracket_space}bitcast ({global_ptr_type} {name} to {phi_type})'
        return match.group(0)

    # Match @name inside phi operand brackets: [ @name, %block ]
    line = re.sub(r'(\[\s*)(@[\w.]+)(?=\s*,)', replacer, line)
    return line


def fix_global_refs(line, globals_map, func_types=None):
    """Insert bitcast when a global ref's pointer type doesn't match its actual type."""
    if func_types is None:
        func_types = {}

    s = line.strip()
    # Don't apply function-as-value bitcasts to declare/define lines
    is_decl_line = s.startswith('declare ') or s.startswith('define ')

    # Find the callee @name position in call/invoke to avoid bitcasting it
    callee_spans = set()
    for cm in re.finditer(
        r'(?:call|invoke)\s+' + TYPE_RE + r'\**\s+(@[\w.]+)\s*\(', line
    ):
        callee_spans.add((cm.start(1), cm.end(1)))

    # Match <type>* @name or <type>** @name patterns
    def replacer(m):
        expected_type = m.group(1)
        stars = m.group(2)
        name = m.group(3)

        # Check global variables
        if name in globals_map:
            actual_type = globals_map[name]
            if actual_type != expected_type:
                return f'{expected_type}{stars} bitcast ({actual_type}* {name} to {expected_type}{stars})'

        # Check function references used as values (NOT as callees, NOT in declare/define)
        if not is_decl_line and name in func_types:
            # Skip if this @name is in a callee position
            name_span = (m.start(3), m.end(3))
            if name_span in callee_spans:
                return m.group(0)
            fn_ptr_type = func_types[name]
            expected_full = expected_type + stars
            if expected_full != fn_ptr_type:
                return f'{expected_type}{stars} bitcast ({fn_ptr_type} {name} to {expected_type}{stars})'

        return m.group(0)

    # Match typed pointer followed by global reference: <type>* @name or <type>** @name
    line = re.sub(
        r'(' + TYPE_RE + r')(\*+)\s+(@[\w.]+)',
        replacer,
        line
    )
    return line


def convert_line(line, globals_map, func_types=None):
    """Convert a single line of LLVM IR from opaque to typed pointers."""
    # Always handle memory() attributes
    if 'memory(' in line:
        line = convert_memory_attrs(line)

    # Strip LLVM 15+ attributes
    if 'presplitcoroutine' in line:
        line = strip_new_attrs(line)

    # Downgrade coroutine intrinsics
    if 'coro.end' in line:
        line = downgrade_coro_intrinsics(line)

    # Convert splat constants (can appear with or without ptr)
    if 'splat' in line:
        line = convert_splat(line)

    if 'ptr' not in line:
        line = fix_global_refs(line, globals_map, func_types)
        if 'phi' in line:
            line = fix_phi_globals(line, globals_map)
        return line

    # Handle specific instruction patterns first
    line = convert_load(line)
    line = convert_store(line)
    line = convert_gep(line)

    # Replace 'ptr' used as a TYPE only (not in register/global names).
    # Negative lookbehind: don't match if preceded by % or @ (register/global names)
    # Also exclude '.' to preserve names like %x.ptr, %r.ptr
    line = re.sub(r'(?<![%@\w.])ptr\b', 'i8*', line)

    # Fix sret/byval attribute type mismatches (after ptr→i8* conversion)
    if 'sret' in line or 'byval' in line:
        line = fix_sret_byval(line)

    # Fix global references with type mismatches
    line = fix_global_refs(line, globals_map, func_types)

    # Fix phi node operands that reference globals with type mismatch
    if 'phi' in line:
        line = fix_phi_globals(line, globals_map)

    return line


# ---------------------------------------------------------------------------
# Second pass: insert bitcasts for local variable type mismatches
# ---------------------------------------------------------------------------

_bc_counter = 0


def cleanup_empty_attr_groups(lines):
    """Remove empty attribute groups and strip their #N references."""
    # Find empty attribute groups
    empty_groups = set()
    for line in lines:
        m = re.match(r'attributes\s+(#\d+)\s*=\s*\{\s*\}\s*$', line.strip())
        if m:
            empty_groups.add(m.group(1))

    if not empty_groups:
        return lines

    result = []
    for line in lines:
        s = line.strip()
        # Remove empty attribute group definitions
        if re.match(r'attributes\s+#\d+\s*=\s*\{\s*\}\s*$', s):
            continue
        # Strip references to empty groups from function definitions
        for grp in empty_groups:
            line = re.sub(r'\s*' + re.escape(grp) + r'\b', '', line)
        result.append(line)
    return result


def _collect_var_types(func_lines, struct_defs=None):
    """Collect SSA variable types from their definitions (post-conversion)."""
    if struct_defs is None:
        struct_defs = {}
    var_types = {}

    for line in func_lines:
        s = line.strip()

        # Function parameters
        if s.startswith('define '):
            paren_m = re.search(r'\(([^)]*)\)', s)
            if paren_m:
                for param in paren_m.group(1).split(','):
                    param = param.strip()
                    pm = re.match(r'(.+?)\s+(%[\w.]+)\s*$', param)
                    if pm:
                        var_types[pm.group(2)] = pm.group(1).strip()
            continue

        # %name = alloca <type> [, <numtype> <num>] [, align N]
        m = re.match(
            r'\s*(%[\w.]+)\s*=\s*alloca\s+(' + TYPE_RE + r'\**)\s*(?:,|$)', s
        )
        if m:
            var_types[m.group(1)] = m.group(2).strip() + '*'
            continue

        # %name = load <type>, ...
        m = re.match(r'\s*(%[\w.]+)\s*=\s*load\s+(.+?)\s*,', s)
        if m:
            var_types[m.group(1)] = m.group(2).strip()
            continue

        # %name = [tail] call <rettype> [@%]...
        m = re.match(
            r'\s*(%[\w.]+)\s*=\s*(?:tail\s+|musttail\s+|notail\s+)?call\s+(.+?)\s+[@%]',
            s,
        )
        if m:
            var_types[m.group(1)] = m.group(2).strip()
            continue

        # %name = phi <type> [...]
        m = re.match(r'\s*(%[\w.]+)\s*=\s*phi\s+(.+?)\s+\[', s)
        if m:
            var_types[m.group(1)] = m.group(2).strip()
            continue

        # %name = bitcast ... to <type>
        m = re.match(r'\s*(%[\w.]+)\s*=\s*bitcast\s+.+\s+to\s+(.+)', s)
        if m:
            var_types[m.group(1)] = m.group(2).strip()
            continue

        # %name = extractvalue <type> %val, <idx>
        # Result type depends on struct member — for now just track surrounding type
        # e.g., extractvalue { i8*, i8* } %x, 0 → i8*
        m = re.match(
            r'\s*(%[\w.]+)\s*=\s*extractvalue\s+(\{[^}]*\})\s+[^,]+,\s*(\d+)',
            s,
        )
        if m:
            struct_type = m.group(2)
            idx = int(m.group(3))
            members = [x.strip() for x in struct_type.strip('{}').split(',')]
            if idx < len(members):
                var_types[m.group(1)] = members[idx]
            continue

        # %name = getelementptr [inbounds] <type>, <type>* %base, <indices>
        # Resolve result type from struct definitions and constant indices.
        gep_m = re.match(
            r'\s*(%[\w.]+)\s*=\s*getelementptr\s+(?:inbounds\s+)?(' + TYPE_RE + r')\s*,',
            s,
        )
        if gep_m:
            gep_name = gep_m.group(1)
            base_type = gep_m.group(2)
            # Extract constant integer indices from the GEP
            idx_part = s[gep_m.end():]
            indices = []
            for idx_m in re.finditer(r'i(?:32|64)\s+(\d+)', idx_part):
                indices.append(int(idx_m.group(1)))
            if indices:
                result_type = resolve_gep_result_type(base_type, indices, struct_defs)
                if result_type:
                    var_types[gep_name] = result_type + '*'
            continue

        # %name = inttoptr i64 %val to <type>
        m = re.match(r'\s*(%[\w.]+)\s*=\s*inttoptr\s+.+\s+to\s+(.+)', s)
        if m:
            var_types[m.group(1)] = m.group(2).strip()
            continue

    return var_types


def _fix_indirect_calls(line, var_types):
    """Fix indirect calls: call <rettype> %var(...) where %var is i8*.

    LLVM 14 requires the callee to be a proper function pointer type.
    Returns (modified_line, list_of_bitcast_lines) or (line, None).
    """
    global _bc_counter

    # Match: [%name =] [tail] call <rettype> %varname(<args>)
    m = re.search(
        r'((?:tail\s+|musttail\s+|notail\s+)?call\s+)'
        r'(' + TYPE_RE + r')\s+'
        r'(%[\w.]+)'
        r'\(([^)]*)\)',
        line,
    )
    if not m:
        return line, None

    call_prefix = m.group(1)  # "call " or "tail call "
    ret_type = m.group(2)
    callee = m.group(3)
    args_str = m.group(4)

    # Only fix if callee is i8* (from opaque ptr conversion)
    if callee not in var_types:
        return line, None
    if var_types[callee] != 'i8*':
        return line, None

    # Build argument type list from the call arguments
    arg_types = []
    if args_str.strip():
        for arg in args_str.split(','):
            arg = arg.strip()
            # Extract just the type from "type value" pairs
            am = re.match(r'(' + TYPE_RE + r'\**)\s+', arg)
            if am:
                arg_types.append(am.group(1))
            else:
                return line, None  # Can't parse arg types, bail

    fn_type = f'{ret_type} ({", ".join(arg_types)})*'

    indent = len(line) - len(line.lstrip())
    indent_str = ' ' * indent if indent > 0 else '  '

    bc_name = f'%_bc.{_bc_counter}'
    _bc_counter += 1
    bc_line = f'{indent_str}{bc_name} = bitcast i8* {callee} to {fn_type}'

    # Replace the callee in the call instruction
    line = line[:m.start(3)] + bc_name + line[m.end(3):]

    return line, [bc_line]


def _process_function(func_lines, struct_defs=None):
    """Process one function: insert bitcasts for local variable type mismatches."""
    global _bc_counter

    var_types = _collect_var_types(func_lines, struct_defs)
    new_lines = []

    for line in func_lines:
        s = line.strip()

        # Don't modify define / closing brace / labels / comments
        if s.startswith('define ') or s == '}' or s.endswith(':') or s.startswith(';'):
            new_lines.append(line)
            continue

        # --- Handle indirect function calls: call <rettype> %var(<args>) ---
        line, bc_prefix = _fix_indirect_calls(line, var_types)
        if bc_prefix:
            new_lines.extend(bc_prefix)

        # Find TYPE* %varname patterns where there's a mismatch
        mismatches = []
        for m in re.finditer(
            r'(' + TYPE_RE + r')(\*+)\s+(%[\w.]+)', line
        ):
            expected_type = m.group(1) + m.group(2)
            var_name = m.group(3)

            if var_name not in var_types:
                continue

            actual_type = var_types[var_name]
            if actual_type == expected_type:
                continue

            # Both must be pointer types for bitcast
            if not (actual_type.endswith('*') and expected_type.endswith('*')):
                continue

            mismatches.append({
                'start': m.start(3),
                'end': m.end(3),
                'var': var_name,
                'expected': expected_type,
                'actual': actual_type,
            })

        if not mismatches:
            new_lines.append(line)
            continue

        # Generate bitcasts and substitute (right-to-left to preserve positions)
        indent = len(line) - len(line.lstrip())
        indent_str = ' ' * indent if indent > 0 else '  '

        bc_lines = []
        for mm in sorted(mismatches, key=lambda x: x['start'], reverse=True):
            bc_name = f'%_bc.{_bc_counter}'
            _bc_counter += 1
            bc_lines.append(
                f'{indent_str}{bc_name} = bitcast {mm["actual"]} {mm["var"]} to {mm["expected"]}'
            )
            line = line[: mm['start']] + bc_name + line[mm['end'] :]

        # Insert bitcast lines BEFORE the instruction that uses them
        for bc_line in reversed(bc_lines):
            new_lines.append(bc_line)
        new_lines.append(line)

    return new_lines


def insert_local_bitcasts(lines, struct_defs=None):
    """Split into functions and insert bitcasts for type mismatches."""
    global _bc_counter
    _bc_counter = 0

    result = []
    func_buf = []
    in_func = False

    for line in lines:
        s = line.strip()

        if s.startswith('define '):
            in_func = True
            func_buf = [line]
            continue

        if in_func:
            func_buf.append(line)
            if s == '}':
                result.extend(_process_function(func_buf, struct_defs))
                in_func = False
                func_buf = []
            continue

        result.append(line)

    # Flush any unterminated function (shouldn't happen in valid IR)
    if func_buf:
        result.extend(func_buf)

    return result


def reconcile_declarations(lines):
    """Fix extern declarations whose param types don't match call sites.

    After sret/byval conversion, call sites may use typed pointers
    (e.g., %struct.fix256* sret(...)) while the declare line still has i8*.
    Collect call-site signatures and rewrite mismatched declarations.
    """
    def extract_balanced_args(line, start):
        """Extract args from position of opening '(' handling nested parens."""
        depth = 0
        for i in range(start, len(line)):
            if line[i] == '(':
                depth += 1
            elif line[i] == ')':
                depth -= 1
                if depth == 0:
                    return line[start + 1:i]
        return None

    # Collect call-site argument signatures per function name
    call_sigs = {}  # name → args string
    for line in lines:
        s = line.strip()
        # Find call/invoke @funcname(
        m = re.search(r'(?:call|invoke)\s+' + TYPE_RE + r'\**\s+(@[\w.]+)\s*\(', s)
        if not m:
            continue
        name = m.group(1)
        args = extract_balanced_args(s, m.end() - 1)
        if args is not None and name not in call_sigs:
            call_sigs[name] = args

    if not call_sigs:
        return lines

    result = []
    for line in lines:
        s = line.strip()
        if not s.startswith('declare '):
            result.append(line)
            continue

        # Find the function name and opening paren in declare line
        m = re.search(r'(@[\w.]+)\s*\(', s)
        if not m:
            result.append(line)
            continue

        name = m.group(1)
        if name not in call_sigs:
            result.append(line)
            continue

        call_args = call_sigs[name]
        # Strip value names/constants from call-site args to get just types + attrs
        decl_params = []
        for arg in _split_args(call_args):
            arg = arg.strip()
            if not arg:
                continue
            # Handle constant expressions like "i8* bitcast (...)"
            # or "i8* getelementptr (...)" — extract just the type
            ce_m = re.match(r'(' + TYPE_RE + r'\**)\s+(?:bitcast|getelementptr|inttoptr|ptrtoint)\s*\(', arg)
            if ce_m:
                decl_params.append(ce_m.group(1))
                continue
            # Remove the trailing %name or literal value, keep type + attrs
            parts = arg.rsplit(None, 1)
            if len(parts) == 2 and (parts[1].startswith('%') or parts[1].startswith('@')
                                     or re.match(r'^-?\d', parts[1])
                                     or parts[1] in ('null', 'undef', 'zeroinitializer', 'true', 'false', 'none')):
                decl_params.append(parts[0])
            else:
                decl_params.append(arg)

        new_params = ', '.join(decl_params)

        # Rebuild: everything before '(' + new params + ')' + trailing attrs
        paren_start = s.index(name) + len(name)
        paren_start = s.index('(', paren_start)
        old_args = extract_balanced_args(s, paren_start)
        if old_args is None:
            result.append(line)
            continue
        paren_end = paren_start + 1 + len(old_args) + 1  # skip '(' + args + ')'
        new_line = s[:paren_start] + '(' + new_params + ')' + s[paren_end:]
        result.append(new_line)

    return result


def _split_args(args_str):
    """Split function arguments respecting nested parentheses."""
    parts = []
    depth = 0
    current = []
    for ch in args_str:
        if ch == '(' :
            depth += 1
            current.append(ch)
        elif ch == ')':
            depth -= 1
            current.append(ch)
        elif ch == ',' and depth == 0:
            parts.append(''.join(current))
            current = []
        else:
            current.append(ch)
    if current:
        parts.append(''.join(current))
    return parts


def convert_file(input_path, output_path=None):
    with open(input_path, 'r') as f:
        text = f.read()

    lines = text.split('\n')
    globals_map = build_global_types(lines)

    # First pass: convert ptr → typed pointers (without function type fixups)
    converted = [convert_line(line, globals_map) for line in lines]

    # Build struct type definitions from converted lines (typed pointers)
    struct_defs = build_struct_defs(converted)

    # Build function type map from converted lines (typed pointers)
    func_types = build_function_types(converted)

    # Re-run global ref fixup with function types for function-as-value references
    converted = [fix_global_refs(line, globals_map, func_types) for line in converted]

    # Second pass: insert bitcasts for local variable type mismatches
    converted = insert_local_bitcasts(converted, struct_defs)

    # Third pass: remove empty attribute groups and their references
    converted = cleanup_empty_attr_groups(converted)

    # Fourth pass: reconcile extern declarations with call-site signatures
    converted = reconcile_declarations(converted)

    result = '\n'.join(converted)

    if output_path:
        with open(output_path, 'w') as f:
            f.write(result)
    else:
        sys.stdout.write(result)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print(f'Usage: {sys.argv[0]} <input.ll> [output.ll]', file=sys.stderr)
        sys.exit(1)

    input_path = sys.argv[1]
    output_path = sys.argv[2] if len(sys.argv) > 2 else None
    convert_file(input_path, output_path)
