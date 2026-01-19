# Till Direction

**Category**: Control Flow → Till Loops  
**Keywords**: `down`, `to`, `by`  
**Philosophy**: Natural counting direction

---

## What is Till Direction?

**Till direction** controls whether a till loop counts **up** or **down**.

---

## Up (Default)

```aria
till i: 10 {
    // Counts 0, 1, 2, ..., 9
}
```

---

## Down

```aria
till i: 10 down {
    // Counts 10, 9, 8, ..., 1
}
```

---

## Up with Range

```aria
till i: 5 to 15 {
    // Counts 5, 6, 7, ..., 14
}
```

---

## Down with Range

```aria
till i: 15 down to 5 {
    // Counts 15, 14, 13, ..., 6
}
```

---

## Complete syntax in [Till Syntax](till_syntax.md)

---

**Quick Reference**: Default = **up**, use `down` to reverse
