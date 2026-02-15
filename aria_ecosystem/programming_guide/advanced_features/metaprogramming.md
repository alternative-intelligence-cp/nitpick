# Metaprogramming

**Category**: Advanced Features → Compile-Time  
**Purpose**: Programs that write programs

---

## Overview

Metaprogramming allows code to **generate**, **inspect**, and **transform** other code at compile time.

---

## Why Metaprogramming?

- ✅ Code generation
- ✅ Generic programming
- ✅ Zero-cost abstractions
- ✅ Compile-time reflection
- ✅ Type-safe code generation
- ✅ Eliminate boilerplate

---

## Type Reflection

```aria
comptime {
    type_info: TypeInfo = @type_info(User);
    
    stdout << "Struct: $type_info.name";
    stdout << "Fields: $type_info.fields.len";
    
    till(type_info.fields.length - 1, 1) {
        stdout << "  ${type_info.fields[$].name}: ${type_info.fields[$].type}";
    }
}
```

---

## Code Generation

### Generate Getters/Setters

```aria
comptime fn generate_accessors(T: type) {
    type_info: TypeInfo = @type_info(T);
    
    till(type_info.fields.length - 1, 1) {
        field = type_info.fields[$];
        // Generate getter
        @generate_function("get_" ++ field.name, {
            fn() -> field.type {
                return self.[field.name];
            }
        });
        
        // Generate setter
        @generate_function("set_" ++ field.name, {
            fn(value: field.type) {
                self.[field.name] = value;
            }
        });
    }
}

struct User {
    name: string,
    age: i32,
}

comptime generate_accessors(User);

// Now you can use:
user: User = User { name: "Alice", age: 30 };
name: string = user.get_name();
user.set_age(31);
```

---

## Generic Specialization

```aria
fn serialize<T>(value: T) -> string {
    comptime {
        if @type_is_integer(T) {
            return int_to_string(value);
        } else if @type_is_float(T) {
            return float_to_string(value);
        } else if @type_is_string(T) {
            return "\"" ++ value ++ "\"";
        } else if @type_is_struct(T) {
            return serialize_struct(value);
        } else {
            @compile_error("Cannot serialize type: $T");
        }
    }
}
```

---

## Compile-Time Introspection

```aria
comptime fn struct_to_json<T>(value: T) -> string {
    type_info: TypeInfo = @type_info(T);
    
    json: string = "{";
    first: bool = true;
    
    till(type_info.fields.length - 1, 1) {
        field = type_info.fields[$];
        if !first {
            json = json ++ ", ";
        }
        
        field_value = value.[field.name];
        json = json ++ "\"$field.name\": " ++ serialize(field_value);
        first = false;
    }
    
    json = json ++ "}";
    return json;
}

struct User {
    name: string,
    age: i32,
}

user: User = User { name: "Alice", age: 30 };
json: string = struct_to_json(user);
// {"name": "Alice", "age": 30}
```

---

## Type Manipulation

```aria
comptime fn make_optional(T: type) -> type {
    return ?T;
}

comptime fn make_array(T: type, N: i32) -> type {
    return [T; N];
}

type OptionalInt = comptime make_optional(i32);     // ?i32
type IntArray10 = comptime make_array(i32, 10);     // [i32; 10]
```

---

## Compile-Time Validation

```aria
comptime fn validate_struct(T: type) {
    type_info: TypeInfo = @type_info(T);
    
    // Must have at least one field
    if type_info.fields.len == 0 {
        @compile_error("Struct must have at least one field");
    }
    
    // Check for required fields
    has_id: bool = false;
    till(type_info.fields.length - 1, 1) {
        if type_info.fields[$].name == "id" {
            has_id = true;
        }
    }
    
    if !has_id {
        @compile_error("Struct must have 'id' field");
    }
}

struct User {
    id: i32,
    name: string,
}

comptime validate_struct(User);  // OK

struct Invalid {
    name: string,
}

comptime validate_struct(Invalid);  // Compile error: missing 'id' field
```

---

## Macro-like Code Generation

```aria
comptime fn implement_trait(T: type, trait_name: string) {
    if trait_name == "Debug" {
        @generate_impl(T, {
            fn debug() -> string {
                return "@type_name(T) { ... }";
            }
        });
    } else if trait_name == "Clone" {
        @generate_impl(T, {
            fn clone() -> T {
                return self;
            }
        });
    }
}

struct Point {
    x: i32,
    y: i32,
}

comptime implement_trait(Point, "Debug");
comptime implement_trait(Point, "Clone");

point: Point = Point { x: 10, y: 20 };
debug_str: string = point.debug();
cloned: Point = point.clone();
```

---

## Generic Algorithms

```aria
comptime fn generate_comparison<T>() {
    if @type_has_method(T, "compare") {
        // Use custom compare
        fn compare(a: T, b: T) -> i32 {
            return a.compare(b);
        }
    } else if @type_is_numeric(T) {
        // Numeric comparison
        fn compare(a: T, b: T) -> i32 {
            if a < b { return -1; }
            if a > b { return 1; }
            return 0;
        }
    } else {
        @compile_error("Type $T cannot be compared");
    }
}
```

---

## Common Patterns

### Builder Pattern Generation

```aria
comptime fn generate_builder(T: type) {
    type_info: TypeInfo = @type_info(T);
    
    // Generate Builder struct
    builder_fields = [];
    till(type_info.fields.length - 1, 1) {
        builder_fields.push({
            name: type_info.fields[$].name,
            type: ?type_info.fields[$].type,
        });
    }
    
    @generate_struct(T.name ++ "Builder", builder_fields);
    
    // Generate with_* methods
    till(type_info.fields.length - 1, 1) {
        field = type_info.fields[$];
        @generate_method(T.name ++ "Builder", "with_" ++ field.name, {
            fn(value: field.type) -> Self {
                self.[field.name] = value;
                return self;
            }
        });
    }
}
```

---

### Enum String Conversion

```aria
comptime fn generate_enum_to_string(E: type) {
    type_info: TypeInfo = @type_info(E);
    
    @generate_impl(E, {
        fn to_string(self) -> string {
            comptime {
                till(type_info.variants.length - 1, 1) {
                    if self == E.[type_info.variants[$].name] {
                        return type_info.variants[$].name;
                    }
                }
            }
            return "Unknown";
        }
    });
}

enum Status {
    Active,
    Inactive,
    Pending,
}

comptime generate_enum_to_string(Status);

status: Status = Status.Active;
name: string = status.to_string();  // "Active"
```

---

## Best Practices

### ✅ DO: Generate Boilerplate

```aria
comptime {
    // Generate repetitive code
    types = [i8, i16, i32, i64];
    till(types.length - 1, 1) {
        generate_parser(types[$]);
    }
}
```

### ✅ DO: Validate at Compile Time

```aria
comptime {
    validate_configuration();
    check_type_constraints();
}
```

### ✅ DO: Type-Safe Code Generation

```aria
comptime fn generate_safe<T>(generator: string) {
    validate_type(T);
    return generate_code(T, generator);
}
```

### ❌ DON'T: Overcomplicate

```aria
// Simple case - just write it
fn get_name() -> string { return self.name; }

// Don't metaprogram when not needed
comptime generate_simple_getter("name");  // ❌ Overkill
```

---

## Advanced Examples

### Serialization Framework

```aria
comptime fn derive_serialize(T: type) {
    type_info: TypeInfo = @type_info(T);
    
    @generate_impl(T, {
        fn serialize() -> []u8 {
            buffer: []u8 = [];
            
            comptime till(type_info.fields.length - 1, 1) {
                field_data = serialize(self.[type_info.fields[$].name]);
                buffer.append(field_data);
            }
            
            return buffer;
        }
        
        fn deserialize(data: []u8) -> Result<T> {
            offset: usize = 0;
            Result: T;
            
            comptime till(type_info.fields.length - 1, 1) {
                field = type_info.fields[$];
                field_value = deserialize<field.type>(data[offset..])?;
                result.[field.name] = field_value;
                offset += @size_of(field.type);
            }
            
            return Ok(result);
        }
    });
}
```

---

## Related

- [const](const.md) - Const keyword
- [compile_time](compile_time.md) - Compile-time computation
- [comptime](comptime.md) - Comptime expressions
- [macros](macros.md) - Macros

---

**Remember**: Metaprogramming is **powerful** - write code that writes **optimal code**!
