# Macros

**Category**: Advanced Features → Macros  
**Purpose**: Code generation and compile-time metaprogramming

---

## Overview

Macros generate code at **compile time**, enabling powerful abstractions without runtime cost.

---

## Basic Macro

```aria
macro square(x) {
    return x * x;
}

fn main() {
    Result: i32 = square(5);  // Expands to: 5 * 5
    stdout << result;  // 25
}
```

---

## Macro Syntax

```aria
// Simple macro
macro NAME(params) {
    // Code to generate
}

// Macro with type
macro<T> NAME(params) {
    // Generic macro
}
```

---

## Code Generation

```aria
macro repeat(n, code) {
    comptime {
        till(n - 1, 1) {
            @emit(code);
        }
    }
}

fn main() {
    repeat(3, {
        stdout << "Hello";
    });
    
    // Expands to:
    // stdout << "Hello";
    // stdout << "Hello";
    // stdout << "Hello";
}
```

---

## Token Manipulation

```aria
macro make_getter(field) {
    fn get_$field() -> auto {
        return self.$field;
    }
}

struct User {
    name: string,
    age: i32,
    
    make_getter(name);  // Generates get_name()
    make_getter(age);   // Generates get_age()
}

fn main() {
    user: User = User { name: "Alice", age: 30 };
    stdout << user.get_name();  // "Alice"
}
```

---

## Variadic Macros

```aria
macro log(level, ...args) {
    stdout << "[$level] ";
    comptime {
        till(args.length - 1, 1) {
            @emit(stdout << args[$] << " ");
        }
    }
    stdout << "\n";
}

fn main() {
    log("INFO", "User", "logged in", "from", "192.168.1.1");
    // Expands to:
    // stdout << "[INFO] " << "User" << " " << "logged in" << " " << "from" << " " << "192.168.1.1" << "\n";
}
```

---

## Common Patterns

### Debug Print Macro

```aria
macro debug_print(expr) {
    stdout << "$expr = " << expr << "\n";
}

fn main() {
    x: i32 = 42;
    debug_print(x);        // x = 42
    debug_print(x + 10);   // x + 10 = 52
}
```

---

### Assert Macro

```aria
macro assert(condition, message) {
    if !condition {
        stderr << "Assertion failed: $message\n";
        stderr << "  at $__FILE__:$__LINE__\n";
        exit(1);
    }
}

fn main() {
    x: i32 = 5;
    assert(x > 0, "x must be positive");
    assert(x < 10, "x must be less than 10");
}
```

---

### Min/Max Macros

```aria
macro min(a, b) {
    return if a < b { a } else { b };
}

macro max(a, b) {
    return if a > b { a } else { b };
}

fn main() {
    x: i32 = min(5, 10);  // 5
    y: i32 = max(5, 10);  // 10
}
```

---

### Defer Macro

```aria
macro defer(code) {
    @scope_exit {
        @emit(code);
    }
}

fn process_file(path: string) -> Result<void> {
    file: File = open(path)?;
    defer(file.close());
    
    // Work with file
    data: string = file.read_all()?;
    
    return Ok();
    // file.close() called automatically
}
```

---

### Generic Builder Macro

```aria
macro make_builder(TypeName, ...fields) {
    struct ${TypeName}Builder {
        comptime {
            till(fields.length - 1, 1) {
                ${fields[$].name}: ?${fields[$].type},
            }
        }
    }
    
    impl ${TypeName}Builder {
        pub fn new() -> ${TypeName}Builder {
            return ${TypeName}Builder {
                comptime {
                    till(fields.length - 1, 1) {
                        ${fields[$].name}: None,
                    }
                }
            };
        }
        
        comptime {
            till(fields.length - 1, 1) {
                pub fn ${fields[$].name}(value: ${fields[$].type}) -> ${TypeName}Builder {
                    self.${fields[$].name} = Some(value);
                    return self;
                }
            }
        }
        
        pub fn build() -> ${TypeName} {
            return ${TypeName} {
                comptime {
                    till(fields.length - 1, 1) {
                        ${fields[$].name}: self.${fields[$].name}?,
                    }
                }
            };
        }
    }
}

// Usage
struct User {
    name: string,
    age: i32,
    email: string,
}

make_builder(User, 
    { name: "string", type: string },
    { name: "age", type: i32 },
    { name: "email", type: string }
);

fn main() {
    user: User = UserBuilder.new()
        .name("Alice")
        .age(30)
        .email("alice@example.com")
        .build();
}
```

---

### Enum Variants Macro

```aria
macro enum_variants(EnumType) {
    pub fn variants() -> []string {
        comptime {
            variants: []string = [];
            till(EnumType.__variants.length - 1, 1) {
                variants.push(EnumType.__variants[$].name);
            }
            return variants;
        }
    }
}

enum Color {
    Red,
    Green,
    Blue,
    
    enum_variants(Color);
}

fn main() {
    variant_list = Color.variants();
    till(variant_list.length - 1, 1) {
        stdout << variant_list[$];  // "Red", "Green", "Blue"
    }
}
```

---

## Macro Hygiene

```aria
// Unhygienic - can capture variables
macro bad_swap(a, b) {
    temp = a;  // ⚠️ Might capture existing 'temp'
    a = b;
    b = temp;
}

// Hygienic - uses unique names
macro swap(a, b) {
    temp_$__COUNTER__ = a;  // Unique name
    a = b;
    b = temp_$__COUNTER__;
}
```

---

## Compile-Time Introspection

```aria
macro print_type_info(T) {
    stdout << "Type: $T\n";
    stdout << "Size: ${@sizeof(T)}\n";
    stdout << "Align: ${@alignof(T)}\n";
    
    comptime {
        if @has_field(T, "name") {
            stdout << "Has field 'name'\n";
        }
    }
}

struct User {
    name: string,
    age: i32,
}

fn main() {
    print_type_info(User);
}
```

---

## Best Practices

### ✅ DO: Use for Code Generation

```aria
macro make_accessors(struct_name, ...fields) {
    impl struct_name {
        comptime {
            till(fields.length - 1, 1) {
                field = fields[$];
                pub fn get_${field}() -> auto {
                    return self.${field};
                }
                
                pub fn set_${field}(value: auto) {
                    self.${field} = value;
                }
            }
        }
    }
}
```

### ✅ DO: Use for Compile-Time Assertions

```aria
macro static_assert(condition, message) {
    comptime {
        if !condition {
            @compile_error(message);
        }
    }
}

fn main() {
    static_assert(@sizeof(i32) == 4, "i32 must be 4 bytes");
}
```

### ⚠️ WARNING: Avoid Complex Macros

```aria
// ❌ Too complex - hard to debug
macro complex_logic(x, y, z, ...args) {
    // 100 lines of macro code
}

// ✅ Better - use functions or templates
fn complex_logic(x: auto, y: auto, z: auto) -> auto {
    // Clear, debuggable code
}
```

### ❌ DON'T: Overuse Macros

```aria
// ❌ Unnecessary macro
macro add(a, b) {
    return a + b;
}

// ✅ Just use a function
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Related

- [comptime](comptime.md) - Compile-time execution
- [metaprogramming](metaprogramming.md) - Metaprogramming
- [nasm_macros](nasm_macros.md) - NASM-style macros

---

**Remember**: Macros are powerful but should be used **judiciously** - prefer functions when possible!
