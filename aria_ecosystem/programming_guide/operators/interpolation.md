# String Interpolation ($)

**Category**: Operators → String  
**Operator**: `$` in strings  
**Purpose**: Embed expressions in strings

---

## Syntax

```aria
"text $variable text"
"text ${expression} text"
```

---

## Description

String interpolation allows embedding variables and expressions directly in strings using `$`.

---

## Basic Usage

```aria
name: string = "Alice";
age: i32 = 25;

// Simple interpolation
msg: string = "Hello, $name!";
// "Hello, Alice!"

// With expressions
info: string = "Age: ${age + 1}";
// "Age: 26"
```

---

## Expression Interpolation

```aria
x: i32 = 10;
y: i32 = 20;

Result: string = "${x} + ${y} = ${x + y}";
// "10 + 20 = 30"
```

---

## Method Calls

```aria
user: User = get_user();

msg: string = "User: ${user.name()}, Status: ${user.status()}";
```

---

## Formatting

```aria
price: f64 = 19.99;

// With format specifiers (if supported)
text: string = "Price: $${price:.2f}";
// "Price: $19.99"
```

---

## Best Practices

### ✅ DO: Use for Readability

```aria
// Clear
msg: string = "User $name has $count items";

// Less clear
msg: string = "User " + name + " has " + to_string(count) + " items";
```

### ✅ DO: Use Braces for Complex Expressions

```aria
text: string = "Result: ${calculate(x, y)}";
```

### ❌ DON'T: Nest Too Deeply

```aria
// Confusing
msg: string = "${outer(${inner(${deep()})})}";

// Better
temp := inner(deep());
result := outer(temp);
msg: string = "$result";
```

---

## Escape Dollar Sign

```aria
// Literal $
price: string = "Cost: \$${amount}";
// "Cost: $50"
```

---

## Related

- [Strings](../types/string.md)
- [Backtick (`)](backtick.md) - Raw strings
- [String Concatenation](add.md)

---

**Remember**: `$` in strings **interpolates** expressions!
