# Pipe Backward Operator (<|)

**Category**: Operators → Functional  
**Operator**: `<|`  
**Purpose**: Apply function to value

---

## Syntax

```aria
<function> <| <value>
```

---

## Description

The pipe backward operator `<|` applies the function on the left to the value on the right.

---

## Basic Usage

```aria
// Without pipe
Result: i32 = calculate(complex_expression());

// With pipe
Result: i32 = calculate <| complex_expression();
```

---

## Reduce Parentheses

```aria
// Many parentheses
print(format(calculate(x + y)));

// Cleaner
print <| format <| calculate <| x + y;
```

---

## Best Practices

### ✅ DO: Use for Clarity

```aria
save <| validate <| load(file);
```

### ❌ DON'T: Mix with Forward Pipe

```aria
// Confusing
result := value |> func1 <| func2;

// Stick to one direction
result := value |> func1 |> func2;
```

---

## Related

- [Pipe Forward (|>)](pipe_forward.md)
- [Function Application](../functions/functions.md)

---

**Remember**: `<|` applies function **backward**!
