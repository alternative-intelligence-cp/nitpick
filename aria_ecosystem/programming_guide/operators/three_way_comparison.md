# Three-Way Comparison (<=>)

**Category**: Operators → Comparison  
**Operator**: `<=>`  
**Purpose**: Spaceship operator for ordering

---

## Syntax

```aria
<expression> <=> <expression>
```

---

## Description

The three-way comparison operator `<=>` (spaceship operator) compares two values and returns their ordering relationship.

---

## Return Values

```aria
// Returns:
// -1 if left < right
//  0 if left == right
//  1 if left > right

Result: i32 = 5 <=> 10;  // -1
Result: i32 = 10 <=> 10; //  0
Result: i32 = 15 <=> 10; //  1
```

---

## Sorting

```aria
// Perfect for sort comparisons
fn compare_users(a: User, b: User) -> i32 {
    return a.name <=> b.name;
}

users.sort(compare_users);
```

---

## Chained Comparison

```aria
// Try multiple criteria
fn compare(a: Item, b: Item) -> i32 {
    // First by priority
    Result: i32 = a.priority <=> b.priority;
    when result != 0 then
        return result;
    end
    
    // Then by name
    return a.name <=> b.name;
}
```

---

## Best Practices

### ✅ DO: Use for Sorting

```aria
fn compare(a: i32, b: i32) -> i32 {
    return a <=> b;
}
```

### ✅ DO: Use for Multi-Field Comparison

```aria
fn compare_records(a: Record, b: Record) -> i32 {
    result := a.date <=> b.date;
    when result != 0 then return result end;
    
    return a.id <=> b.id;
}
```

### ❌ DON'T: Use for Simple Equality

```aria
// Overkill
when (a <=> b) == 0 then ...

// Better
when a == b then ...
```

---

## Related

- [Less Than (<)](less_than.md)
- [Greater Than (>)](greater_than.md)
- [Equal (==)](equal.md)
- [Sorting](../concepts/sorting.md)

---

**Remember**: `<=>` returns **-1, 0, or 1** for ordering!
