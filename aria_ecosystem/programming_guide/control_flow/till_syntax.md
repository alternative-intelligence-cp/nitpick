# Till Syntax Reference

**Category**: Control Flow → Syntax  
**Topic**: Complete till loop syntax

---

## Basic Till (Count to N)

```aria
till i: 10 {
    // i goes from 0 to 9
}
```

---

## With Start and End

```aria
till i: start to end {
    // i from start to end-1
}
```

---

## Count Down

```aria
till i: 10 down {
    // i from 10 to 1
}

till i: 10 down to 0 {
    // i from 10 to 0
}
```

---

## With Step

```aria
till i: 0 to 10 by 2 {
    // 0, 2, 4, 6, 8
}

till i: 10 down to 0 by 2 {
    // 10, 8, 6, 4, 2, 0
}
```

---

## Loop Control

```aria
till i: 100 {
    if condition {
        break;     // Exit loop
    }
    
    if other_condition {
        continue;  // Next iteration
    }
}
```

---

## Examples

```aria
// Count 0-9
till i: 10 {
    stdout << i;
}

// Count 1-10
till i: 1 to 11 {
    stdout << i;
}

// Countdown
till i: 10 down {
    stdout << i << "...\n";
}

// Even numbers
till i: 0 to 20 by 2 {
    stdout << i;
}

// Array indexing
till i: array.length() {
    process(array[i]);
}
```

---

## Related Topics

- [Till](till.md) - Till loop guide
- [Loop Direction](loop_direction.md) - Up/down direction
- [Till Direction](till_direction.md) - Direction details
- [For](for.md) - For loops

---

**Quick Reference**: `till i: N { }` or `till i: start to end by step { }`
