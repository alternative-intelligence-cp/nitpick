# Dot Operator (.)

**Category**: Operators → Access  
**Operator**: `.`  
**Purpose**: Access members and call methods

---

## Syntax

```aria
<expression>.<member>
<expression>.<method>(<args>)
```

---

## Description

The dot operator `.` accesses struct fields, calls methods, and accesses module members.

---

## Field Access

```aria
struct Point {
    x: i32,
    y: i32
}

point: Point = Point { x: 10, y: 20 };

// Access fields
stdout << point.x;  // 10
stdout << point.y;  // 20

// Modify fields
point.x = 30;
```

---

## Method Calls

```aria
struct Rectangle {
    width: i32,
    height: i32
    
    fn area(self: &Rectangle) -> i32 {
        return self.width * self.height;
    }
}

rect: Rectangle = Rectangle { width: 5, height: 10 };

// Call method
area: i32 = rect.area();
stdout << area;  // 50
```

---

## Chaining

```aria
// Chain method calls
Result: string = text
    .trim()
    .to_lowercase()
    .replace("old", "new");

// Chain field access
nested_value: i32 = obj.field1.field2.field3;
```

---

## Module Access

```aria
// Access module members
value: i32 = Math.abs(-42);
pi: f64 = Math.PI;

// Nested modules
Result: string = Network.HTTP.get("url");
```

---

## Auto-Dereferencing

```aria
// Works with references
point_ref: &Point = get_point();
x: i32 = point_ref.x;  // Automatically dereferences

// Also works with pointers
point_ptr: *Point = get_pointer();
y: i32 = point_ptr.y;  // Auto-dereference
```

---

## Best Practices

### ✅ DO: Chain for Readability

```aria
Result: string = input
    .trim()
    .to_lowercase()
    .split(" ")
    .join("-");
```

### ✅ DO: Use for Encapsulation

```aria
// Good: Use methods
user.get_email()

// Instead of direct access
user.email  // If private
```

### ❌ DON'T: Over-Chain

```aria
// Too complex
result := obj.a.b.c.d.e.f.g.h.i.j.method();

// Better: Break it up
intermediate := obj.a.b.c;
result := intermediate.get_result();
```

---

## Related

- [Arrow (->)](arrow.md) - Type annotations
- [Colon (:)](colon.md) - Type specification
- [Method Calls](../functions/methods.md)

---

**Remember**: `.` accesses members and **auto-dereferences**!
