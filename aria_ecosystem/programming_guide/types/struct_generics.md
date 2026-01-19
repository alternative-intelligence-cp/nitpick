# Struct Generics

**Category**: Types → Structs  
**Purpose**: Generic struct types

---

## Basic Generic Struct

```aria
// Generic container
struct Box<T> {
    value: T
}

// Use with specific types
int_box: Box<i32> = { value = 42 };
string_box: Box<string> = { value = "hello" };
```

---

## Multiple Type Parameters

```aria
struct Pair<T, U> {
    first: T,
    second: U
}

// Use
pair: Pair<i32, string> = {
    first = 42,
    second = "answer"
};
```

---

## Generic Methods

```aria
struct Container<T> {
    items: []T
}

fn add<T>(self: &Container<T>, item: T) {
    self.items.append(item);
}

fn get<T>(self: &Container<T>, index: i32) -> ?T {
    when index >= 0 and index < self.items.length() then
        return self.items[index];
    end
    return nil;
}
```

---

## Common Patterns

### Option/Maybe

```aria
struct Option<T> {
    has_value: bool,
    value: T
}
```

### Result

```aria
struct Result<T, E> {
    is_ok: bool,
    value: T,
    error: E
}
```

---

## Related

- [Structs](struct.md)
- [Generics](../advanced_features/generics.md)

---

**Remember**: Generics make structs **reusable** across types!
