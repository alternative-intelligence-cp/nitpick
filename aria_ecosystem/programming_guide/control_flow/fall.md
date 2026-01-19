# Fall Keyword

**Category**: Control Flow → Keywords  
**Keyword**: `fall`  
**Purpose**: Fallthrough to next case

---

## What is Fall?

**fall** continues execution into the next case in a `pick` statement.

---

## Syntax

```aria
case value:
    // code
    fall;  // Continue to next case
```

---

## Basic Usage

```aria
pick x {
    case 1:
        stdout << "One\n";
        fall;  // Falls to case 2
    case 2:
        stdout << "Two\n";
}
```

---

## Must Be Last Statement

```aria
// ✅ Correct: fall is last
case 1:
    stdout << "One\n";
    fall;

// ❌ Wrong: Code after fall
case 1:
    fall;
    stdout << "Never runs\n";  // Unreachable
```

---

## Falls to Next Case

```aria
pick value {
    case "A":
        score = 90;
        fall;  // Goes to case "B"
    case "B":
        passed = true;  // Runs for both "A" and "B"
}
```

---

## Can Fall Multiple Times

```aria
case 1:
    a();
    fall;  // to case 2
case 2:
    b();
    fall;  // to case 3
case 3:
    c();
    fall;  // to else
else:
    d();
```

---

## Examples

See [Fallthrough](fallthrough.md) for complete examples and patterns.

---

## Related Topics

- [Fallthrough](fallthrough.md) - Fallthrough concept
- [Pick](pick.md) - Pattern matching
- [Pick Syntax](pick_syntax.md) - Complete syntax

---

**Remember**: `fall` = **continue to next case** - must be last statement in case block!
