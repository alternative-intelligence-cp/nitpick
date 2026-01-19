# Modulo Assignment Operator (%=)

**Category**: Operators → Compound Assignment  
**Operator**: `%=`  
**Purpose**: Modulo and assign

---

## Syntax

```aria
<variable> %= <expression>
```

---

## Description

The modulo assignment operator `%=` calculates the remainder and assigns it back to the variable.

---

## Basic Usage

```aria
x: i32 = 17;
x %= 5;  // x = x % 5
stdout << x;  // 2
```

---

## Equivalent Forms

```aria
// These are equivalent
x %= 10;
x = x % 10;
```

---

## Common Uses

### Wrap Around

```aria
index: i32 = 0;
for i in 0..100 {
    process(index);
    index += 1;
    index %= buffer_size;  // Wrap to 0
}
```

### Keep in Range

```aria
value: i32 = 255;
value += 10;
value %= 256;  // Keep 0-255
```

### Cycle Through States

```aria
state: i32 = 0;
state += 1;
state %= NUM_STATES;  // 0, 1, 2, 0, 1, 2, ...
```

---

## Best Practices

### ✅ DO: Use for Circular Buffers

```aria
write_pos: i32 = 0;
write_pos += 1;
write_pos %= BUFFER_SIZE;
```

### ✅ DO: Use for Cycling

```aria
color_index: i32 = 0;
color_index += 1;
color_index %= colors.length();
```

### ❌ DON'T: Modulo by Zero

```aria
// Wrong
x %= 0;  // Crash!

// Right
when divisor != 0 then
    x %= divisor;
end
```

---

## Related

- [Modulo (%)](modulo.md)
- [Div Assign (/=)](div_assign.md)
- [Add Assign (+=)](add_assign.md)

---

**Remember**: `%=` keeps remainder **in-place**!
