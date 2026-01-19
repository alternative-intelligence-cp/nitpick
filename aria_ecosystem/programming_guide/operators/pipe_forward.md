# Pipe Forward Operator (|>)

**Category**: Operators → Functional  
**Operator**: `|>`  
**Purpose**: Chain function calls

---

## Syntax

```aria
<value> |> <function>
```

---

## Description

The pipe forward operator `|>` passes the left value as the first argument to the function on the right.

---

## Basic Usage

```aria
// Without pipe
Result: i32 = multiply(add(5, 3), 2);

// With pipe
Result: i32 = 5
    |> add(3)
    |> multiply(2);
// More readable!
```

---

## Chaining

```aria
text: string = "  Hello World  "
    |> trim()
    |> to_lowercase()
    |> replace("world", "aria");

stdout << text;  // "hello aria"
```

---

## With Data Processing

```aria
Result: []i32 = numbers
    |> filter(is_even)
    |> map(square)
    |> take(5);
```

---

## Best Practices

### ✅ DO: Use for Readability

```aria
// Clear data flow
processed := data
    |> validate
    |> transform
    |> save;
```

### ✅ DO: Chain Transformations

```aria
output := input
    |> parse
    |> normalize
    |> format;
```

### ❌ DON'T: Overuse

```aria
// Too many steps
result := value
    |> step1
    |> step2
    |> step3
    |> step4
    |> step5
    |> step6
    |> step7;

// Better: Group logical steps
temp := value |> step1 |> step2 |> step3;
result := temp |> step4 |> step5;
```

---

## Related

- [Pipe Backward (<|)](pipe_backward.md)
- [Method Chaining](dot.md)
- [Functional Programming](../concepts/functional.md)

---

**Remember**: `|>` makes data **flow** more **readable**!
