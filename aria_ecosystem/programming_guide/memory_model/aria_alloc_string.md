# aria_alloc_string

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_alloc_string(text: string) -> string`  
**Purpose**: Allocate heap string

---

## Signature

```aria
fn aria_alloc_string(text: string) -> string
```

---

## Description

Allocates a new string on the heap with a copy of `text`.

---

## Parameters

- `text`: String to copy

---

## Returns

Newly allocated string, or `nil` if allocation fails.

---

## Basic Usage

```aria
// Allocate string
str: string = aria_alloc_string("Hello, World!");

when str == nil then
    fail "Allocation failed";
end

stdout << str;

// Must free
aria_free(str);
```

---

## Building Strings

```aria
// Allocate and modify
str: string = aria_alloc_string("Hello");
defer aria_free(str);

str = str + " World";
stdout << str;  // "Hello World"
```

---

## Best Practices

### ✅ DO: Use defer

```aria
str: string = aria_alloc_string("text");
defer aria_free(str);
```

### ❌ DON'T: Forget to Free

```aria
// Wrong: Memory leak
fn bad() {
    str: string = aria_alloc_string("leak");
    // Never freed!
}
```

---

## Related

- [aria_alloc](aria_alloc.md) - Raw allocation
- [aria_free](aria_free.md) - Free memory
- [Allocators](allocators.md) - Allocation overview

---

**Remember**: Allocated strings must be **freed**!
