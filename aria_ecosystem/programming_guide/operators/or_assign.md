# Bitwise OR Assignment Operator (|=)

**Category**: Operators → Compound Assignment  
**Operator**: `|=`  
**Purpose**: Bitwise OR and assign

---

## Syntax

```aria
<variable> |= <expression>
```

---

## Description

The bitwise OR assignment operator `|=` performs a bitwise OR and assigns the result back.

---

## Basic Usage

```aria
x: i32 = 0b0011;
x |= 0b0100;  // x = x | 0b0100
stdout << x;  // 0b0111 = 7
```

---

## Equivalent Forms

```aria
// These are equivalent
x |= mask;
x = x | mask;
```

---

## Common Uses

### Set Bits

```aria
// Set bit at position
value: i32 = 0b0000;
value |= (1 << 3);  // Set bit 3
// value is now 0b1000
```

### Add Flags

```aria
// Add permission
permissions: i32 = READ;
permissions |= WRITE;  // Add write
permissions |= EXEC;   // Add execute
```

### Combine Options

```aria
options: i32 = 0;
options |= OPTION_VERBOSE;
options |= OPTION_FORCE;
options |= OPTION_QUIET;
```

---

## Best Practices

### ✅ DO: Use for Setting Flags

```aria
state |= FLAG_ACTIVE;
state |= FLAG_READY;
```

### ✅ DO: Use for Combining

```aria
result |= ERROR_FLAG;
result |= WARNING_FLAG;
```

### ❌ DON'T: Mix Bitwise and Logical

```aria
// Wrong
flags |= (x > 5);  // Type error!

// Right
flags |= NEW_FLAG;
```

---

## Related

- [Bitwise OR (|)](bitwise_or.md)
- [Bitwise AND Assignment (&=)](and_assign.md)
- [Bitwise XOR Assignment (^=)](xor_assign.md)

---

**Remember**: `|=` sets bits **in-place**!
