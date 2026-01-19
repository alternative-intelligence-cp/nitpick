# Add Assignment Operator (+=)

**Category**: Operators → Compound Assignment  
**Operator**: `+=`  
**Purpose**: Add and assign

---

## Syntax

```aria
<variable> += <expression>
```

---

## Description

The add assignment operator `+=` adds the right operand to the variable and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 10;
x += 5;  // x = x + 5
stdout << x;  // 15
```

---

## Equivalent Forms

```aria
// These are equivalent
x += 3;
x = x + 3;
```

---

## Common Uses

### Accumulation

```aria
sum: i32 = 0;
for value in values {
    sum += value;
}
```

### Counter Increment

```aria
count: i32 = 0;
count += 1;  // Increment

// Or use ++
count++;
```

### String Building

```aria
message: string = "";
message += "Hello";
message += " ";
message += "World";
stdout << message;  // "Hello World"
```

---

## Type Compatibility

```aria
// Integer
count: i32 = 10;
count += 5;  // OK

// Float
price: f64 = 99.99;
price += 10.50;  // 110.49

// Types must match
x: i32 = 10;
x += 5.5;  // ❌ Type error!
```

---

## Best Practices

### ✅ DO: Use for Accumulation

```aria
total: i32 = 0;
for item in items {
    total += item.price;
}
```

### ✅ DO: Use for Counters

```aria
attempts: i32 = 0;
attempts += 1;
```

### ❌ DON'T: Use for Single Increment

```aria
// Unclear
x += 1;

// Better
x++;
```

---

## Related

- [Addition (+)](add.md)
- [Sub Assign (-=)](sub_assign.md)
- [Increment (++)](increment.md)

---

**Remember**: `+=` adds **in-place**!
