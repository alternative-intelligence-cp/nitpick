# Quick Reference - AriaSH

## Keyboard Shortcuts

| Key | Mode | Action |
|-----|------|--------|
| **ESC** | Any | Toggle RUN ↔ EDIT mode |
| **Enter** | RUN | Submit immediately |
| **Enter** | EDIT | Add newline (continue editing) |
| **;; + Enter** | EDIT | Submit multi-line block |
| **↑ / ↓** | Any | Navigate history (planned) |
| **Backspace** | Any | Delete character |
| **Ctrl+D** | Any | Exit shell |

## Built-in Commands

```bash
help        # Display help information
clear       # Clear screen and show banner
exit        # Exit shell (works with or without ;)
quit        # Same as exit
```

## Common Patterns

### Variable Declaration
```aria
int8 x = 5;
int16 altitude = 5280;
string name = "Aria";
```

### Expression Evaluation
```aria
[RUN] aria> x + 10
=> 15
```

### Multi-line EDIT Mode
```aria
[EDIT] aria> int8 a = 10;
[EDIT] ... int8 b = 20;
[EDIT] ... a + b;;

=> 30
```

### Quick Calculation (RUN mode)
```aria
[RUN] aria> 15 + 27
=> 42
```

## Type Reference

| Type | Size | Range | Description |
|------|------|-------|-------------|
| `int8` | 8-bit | -128 to 127 | Signed byte |
| `int16` | 16-bit | -32,768 to 32,767 | Signed short |
| `int32` | 32-bit | -2^31 to 2^31-1 | Signed int |
| `int64` | 64-bit | -2^63 to 2^63-1 | Signed long |
| `string` | Variable | - | Text string |
| `bool` | 1-bit | true/false | Boolean |

## Operator Precedence

1. Parentheses `( )`
2. Unary `-`, `!`
3. Multiplicative `*`, `/`
4. Additive `+`, `-`
5. Comparison `<`, `>`, `<=`, `>=`
6. Equality `==`, `!=`
7. Logical AND `&&`
8. Logical OR `||`

## Error Messages

| Error | Meaning | Fix |
|-------|---------|-----|
| `Expected variable name` | Missing identifier after type | Add variable name |
| `Expected command name` | Invalid syntax | Check for typos |
| `Undefined variable: x` | Variable not declared | Declare before use |
| `Parse error at line X` | Syntax error | Check syntax |

## Tips & Tricks

**Fast Exit**: Just type `exit` (no semicolon needed)

**Quick Math**: RUN mode is perfect for calculator use
```aria
[RUN] aria> 42 * 1.5
=> 63
```

**Avoid Semicolon Spam**: Don't type `;;;` - parser will complain

**Mode Indicator**: Watch the prompt:
- `[RUN] aria>` = Enter submits
- `[EDIT] aria>` = Need ;; to submit

**Clean Syntax**: Extra spaces are okay:
```aria
int8  x  =  5  ;    # Works fine
```

**Comments**: Not yet implemented (planned)

## Common Workflows

### Interactive Calculator
```aria
[RUN] aria> 100 + 23
=> 123
[RUN] aria> 42 * 2
=> 84
```

### Variable Workspace
```aria
[EDIT] aria> int8 price = 50;
[EDIT] ... int8 quantity = 3;
[EDIT] ... int8 total = price * quantity;;

[RUN] aria> total
=> 150
```

### Function Testing (when implemented)
```aria
[EDIT] aria> func double(int8 x) {
[EDIT] ...     return x * 2;
[EDIT] ... };;

[RUN] aria> double(21)
=> 42
```

## Troubleshooting

**Shell hangs**: Press Ctrl+C, report bug  
**Can't exit EDIT mode**: Type `exit;;` and press Enter  
**Segfault**: Update to v0.1.0+ (fixed)  
**Weird characters**: Check terminal encoding (UTF-8)

## Development

**Build**: `cmake . && make ariash`  
**Test**: `./tests/test_*`  
**Debug**: Add `std::cerr` statements, rebuild  
**Clean**: `rm -rf build && mkdir build`

---

**Version**: 0.1.0  
**Updated**: January 15, 2026
