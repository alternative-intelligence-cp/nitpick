# Not Equal Operator (!=)

**Category**: Operators → Comparison  
**Operator**: `!=`  
**Purpose**: Test inequality

---

## Syntax

```aria
<expression> != <expression>
```

---

## Description

The not equal operator `!=` tests if two values are **not** equal, returning `true` if different, `false` if same.

---

## Basic Usage

```aria
// Different values
when 5 != 3 then
    stdout << "Not equal";  // Prints
end

// Same values
when 5 != 5 then
    stdout << "Won't print";
end

// Variables
status: string = "pending";
when status != "complete" then
    stdout << "Still processing";
end
```

---

## Null Checking

```aria
value: i32? = get_optional();

when value != nil then
    stdout << "Has value: " << value;
else
    stdout << "No value";
end
```

---

## String Comparison

```aria
// Content comparison
input: string = "hello";

when input != "" then
    process(input);
end

when input != "quit" then
    continue_loop();
end
```

---

## Type Requirements

```aria
// Must be same type
x: i32 = 42;
y: i32 = 100;

when x != y then  // OK
    stdout << "Different";
end

// Must cast different types
a: i32 = 5;
b: f64 = 5.0;

when a != (b as i32) then  // Cast needed
    ...
end
```

---

## Best Practices

### ✅ DO: Use for Validation

```aria
when password != "" then
    authenticate(password);
else
    fail "Password required";
end
```

### ✅ DO: Use for Loop Conditions

```aria
while status != "done" {
    status = process_next();
}
```

### ✅ DO: Check for Null

```aria
when ptr != nil then
    use(ptr);
end
```

### ❌ DON'T: Use for Floating Point

```aria
// Wrong: Precision issues
when float1 != float2 then
    ...
end

// Right: Use epsilon
when abs(float1 - float2) > epsilon then
    ...
end
```

---

## Common Patterns

### Until Loop

```aria
response: string = "";
while response != "yes" {
    response = ask("Continue?");
}
```

### Filter

```aria
for item in items {
    when item != excluded_value then
        results.push(item);
    end
}
```

### Validation

```aria
when username != "" and password != "" then
    login(username, password);
else
    fail "Credentials required";
end
```

---

## Opposite of Equal

```aria
// These are equivalent
when x != y then ...
when !(x == y) then ...

// Prefer != for clarity
```

---

## Related

- [Equal (==)](equal.md)
- [Logical NOT (!)](logical_not.md)
- [Null Checking](../types/optional_types.md)

---

**Remember**: `!=` tests **inequality** - opposite of `==`!
