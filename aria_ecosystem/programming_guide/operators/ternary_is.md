# Ternary Is Operator

**Category**: Operators → Conditional  
**Purpose**: Conditional type checking

---

## Description

Combines type checking with conditional assignment.

---

## Basic Pattern

```aria
// Check type and assign
Result: string = value is string ? value : "default";

// Equivalent to
Result: string = when value is string then
    value
else
    "default"
end
```

---

## With Pattern Matching

```aria
// Extract and use
output: i32 = result is Ok(n) ? n : 0;

// Longer form
output: i32 = when result is Ok(n) then
    n
else
    0
end
```

---

## Best Practices

### ✅ DO: Use for Simple Cases

```aria
text: string = input is string ? input : "";
```

### ❌ DON'T: Overcomplicate

```aria
// Too complex
result := x is Type1 ? process1(x) :
          x is Type2 ? process2(x) :
          x is Type3 ? process3(x) :
          default();

// Better: Use when-then-end
result := when x is Type1 then
    process1(x)
elsif x is Type2 then
    process2(x)
elsif x is Type3 then
    process3(x)
else
    default()
end
```

---

## Related

- [Is Operator (is)](is_operator.md)
- [Question (?)](question_operator.md)
- [When-Then-End](../control_flow/when_then_end.md)

---

**Remember**: Combines **type check** with **conditional**!
