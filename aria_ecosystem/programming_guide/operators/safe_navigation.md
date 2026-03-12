# Safe Navigation Operator (`?.`)

> **⚠️ PLANNED FEATURE - NOT YET IMPLEMENTED**  
> This operator is recognized by the lexer but codegen is pending.  
> See [ARIA_SPECS_AUDIT_FEB11_2026.md](../../../META/ARIA/ARIA_SPECS_AUDIT_FEB11_2026.md) line 6521.  
> Do not use this operator in production code yet.

**Category**: Operators → Safety  
**Operator**: `?.`  
**Purpose**: Access nested properties safely, returning NIL if chain breaks

---

## Overview

The safe navigation operator (`?.`) allows you to access nested object properties without explicitly checking for NIL at each level. If any part of the chain is NIL, the entire expression returns NIL instead of crashing.

**Philosophy**: Gracefully handle missing data without defensive null checks.

---

## Basic Syntax

```aria
object?.property        // Returns property or NIL
object?.method()        // Calls method or returns NIL
object?.nested?.deep    // Chain safely through multiple levels
```

---

## Simple Examples

### Example 1: Accessing Optional Fields

```aria
obj?:user = getUser();  // May return NIL

// Without safe navigation (verbose)
string:name;
if (user != NIL) {
    if (user.profile != NIL) {
        name = user.profile.name;
    } else {
        name = "Unknown";
    }
} else {
    name = "Unknown";
}

// With safe navigation (concise)
string:name = user?.profile?.name ?? "Unknown";
```

### Example 2: Method Calls

```aria
obj?:config = loadConfig();

// Safely call method if config exists
int64:port = config?.getPort() ?? 8080;

// If config is NIL, entire expression is NIL
// Then ?? provides default value 8080
```

---

## How It Works

### Chain Evaluation

```aria
user?.profile?.address?.city
```

Evaluates left-to-right, stopping at first NIL:

1. Check if `user` is NIL → if yes, return NIL and **stop**
2. If not, access `user.profile`
3. Check if `profile` is NIL → if yes, return NIL and **stop**
4. If not, access `profile.address`
5. Check if `address` is NIL → if yes, return NIL and **stop**
6. If not, access `address.city` and return it

---

## Combined with Null Coalescing (`??`)

Safe navigation and null coalescing are perfect partners:

```aria
// Get user's email, or default
string:email = user?.profile?.email ?? "no-email@example.com";

// Get nested config value, or default
int64:timeout = config?.server?.timeout ?? 30;

// Complex chain with fallback
string:display = person?.name?.full ?? person?.name?.first ?? "Anonymous";
```

---

## With Arrays and Indexing

```aria
obj?:data = fetchData();

// Safe array access
obj?:first = data?.items?[0];
string:name = first?.name ?? "No data";

// Safe nested array access
int64:value = data?.matrix?[0]?[1] ?? 0;
```

---

## With Function Calls

### Safe Method Invocation

```aria
obj?:service = getService();

// Call method only if service exists
result<Connection>:r = service?.connect("localhost", 8080);

if (!r?.is_error) {
    print("Connected!");
}
```

### Chained Method Calls

```aria
string:result = user
    ?.getProfile()
    ?.getSettings()
    ?.getTheme()
    ?.getName()
    ?? "default";
```

---

## Optional Types (`?`)

Safe navigation pairs with optional type declarations:

```aria
// Declare optional types with ? suffix
obj?:user = NIL;
string?:name = NIL;
int64?:age = NIL;

// Safe navigation on optional types
string:displayName = user?.name ?? "Guest";
int64:displayAge = user?.age ?? 0;
```

---

## Common Patterns

### Pattern 1: Deeply Nested Data

```aria
// API response with optional nesting
obj?:response = apiCall();

string:city = response
    ?.data
    ?.user
    ?.profile
    ?.location
    ?.address
    ?.city
    ?? "Unknown";
```

### Pattern 2: Configuration Fallbacks

```aria
obj?:config = loadConfig();

// Try specific config, fall back to default
int64:maxConns = config
    ?.database
    ?.connection_pool
    ?.max_connections
    ?? 10;
```

### Pattern 3: Optional Method Chaining

```aria
obj?:user = getCurrentUser();

// Chain operations, any can return NIL
obj?:updated = user
    ?.getAccount()
    ?.updatePreferences(prefs)
    ?.save();

if (updated == NIL) {
    stderr.write("Update failed");
}
```

### Pattern 4: Safe Collection Access

```aria
obj?:data = getData();

// Get first item's name safely
string:first = data?.items?[0]?.name ?? "No items";

// Get specific nested element
int64:value = data?.matrix?[2]?[3]?.value ?? -1;
```

---

## Comparison with Explicit Checks

### Without Safe Navigation (Verbose)

```aria
string:getCity = string(obj?:response) {
    if (response == NIL) { pass("Unknown"); }
    
    obj?:data = response.data;
    if (data == NIL) { pass("Unknown"); }
    
    obj?:location = data.location;
    if (location == NIL) { pass("Unknown"); }
    
    string?:city = location.city;
    if (city == NIL) { pass("Unknown"); }
    
    pass(city);
};
```

### With Safe Navigation (Concise)

```aria
func:getCity = string(obj?:response) {
    pass(response?.data?.location?.city ?? "Unknown");
};
```

---

## Edge Cases

### NIL in the Middle

```aria
obj:a = { b: NIL };

// Chain breaks at 'b'
obj?:result = a?.b?.c?.d;  // NIL (b is NIL)
```

### All Valid Chain

```aria
obj:a = { b: { c: { d: 42 } } };

// No NIL in chain
int64:result = a?.b?.c?.d ?? 0;  // 42
```

### Empty Chain

```aria
obj?:value = user?;  // Syntax error! Must access property
obj?:value = user;   // Correct: direct assignment
```

---

## Performance

### Zero-Cost When Not NIL

If the chain never hits NIL, safe navigation compiles to direct property access:

```aria
// This code...
int64:x = obj?.field;

// ...when obj is not NIL, compiles to:
int64:x = obj.field;
```

### Short-Circuit Evaluation

Evaluation stops at first NIL, avoiding unnecessary checks:

```aria
// If 'a' is NIL, b and c are never evaluated
result = a?.b()?.c();
```

---

## Type Safety

### Return Type is Always Optional

```aria
obj:user = { name: "Alice", age: 30 };

// Even if user is not optional, safe navigation makes result optional
string?:name = user?.name;  // string? (optional)
int64?:age = user?.age;     // int64? (optional)

// Must handle NIL possibility
string:displayName = user?.name ?? "Unknown";
```

---

## Best Practices

### ✅ Use for Deeply Nested Optional Data

```aria
// GOOD: Handles complex API responses elegantly
string:value = response?.data?.nested?.field ?? "default";
```

### ✅ Combine with ?? for Defaults

```aria
// GOOD: Safe navigation + sensible default
int64:timeout = config?.timeout ?? 30;
```

### ✅ Use for Optional Method Calls

```aria
// GOOD: Call method only if object exists
result = service?.connect() ?? defaultResult;
```

### ❌ Don't Overuse for Simple Cases

```aria
// BAD: Unnecessary when you know it's not NIL
int64:x = definitelyExists?.field;

// GOOD: Direct access when guaranteed
int64:x = definitelyExists.field;
```

### ❌ Don't Chain Too Deeply Without Context

```aria
// BAD: Hard to debug which level failed
value = a?.b?.c?.d?.e?.f?.g?.h;

// GOOD: Break into logical steps
step1 = a?.b?.c;
step2 = step1?.d?.e;
value = step2?.f?.g ?? default;
```

---

## Error Handling

### Distinguishing NIL from Error

```aria
Result<obj>:r = fetchUser();

// Check for error first
if (r.is_error) {
    stderr.write(`Error: &{r.err}`);
    fail(1);
}

// Then use safe navigation on success value
string:name = raw(r)?.profile?.name ?? "Unknown";
```

### Logging Failed Chains

```aria
obj?:user = getCurrentUser();
string?:email = user?.profile?.email;

if (email == NIL) {
    // Log which part of chain failed
    if (user == NIL) {
        stderr.write("No user");
    } else if (user.profile == NIL) {
        stderr.write("No profile");
    }
}
```

---

## Comparison with Other Languages

### Aria

```aria
string:city = user?.address?.city ?? "Unknown";
```

### TypeScript / JavaScript

```typescript
const city = user?.address?.city ?? "Unknown";
```

### C# (Null-Conditional)

```csharp
string city = user?.address?.city ?? "Unknown";
```

### Swift

```swift
let city = user?.address?.city ?? "Unknown"
```

### Kotlin

```kotlin
val city = user?.address?.city ?: "Unknown"
```

---

## Related Topics

- [Null Coalescing Operator](null_coalesce.md) - `??` for default values
- [NIL vs NULL](../types/nil_null_void.md) - Understanding NIL semantics
- [Optional Types](../types/optional.md) - Using `?` suffix
- [result Type](../types/result.md) - Error handling
- [Unwrap Operator](unwrap.md) - `?` for result unwrapping

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 186  
**Critical for**: Safe access to optional nested data, API responses, config files
