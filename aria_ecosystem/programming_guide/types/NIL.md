# The `NIL` Constant (Aria's "No Value")

**Category**: Types → Special Values → Optional  
**Syntax**: `NIL`  
**Purpose**: Represents "no value present" in Aria's optional type system  
**Type**: Unit type (type-safe absence indicator)  
**Status**: ✅ FULLY IMPLEMENTED, IDIOMATIC STANDARD

---

## Table of Contents

1. [Overview](#overview)
2. [NIL vs NULL vs ERR](#nil-vs-null-vs-err)
3. [Optional Types (?T Syntax)](#optional-types-t-syntax)
4. [Safe Navigation (?.  Operator)](#safe-navigation--operator)
5. [Null Coalescing (?? Operator)](#null-coalescing--operator)
6. [Pattern Matching with NIL](#pattern-matching-with-nil)
7. [NIL in Function Returns](#nil-in-function-returns)
8. [Nikola Consciousness Applications](#nikola-consciousness-applications)
9. [Performance Characteristics](#performance-characteristics)
10. [Common Patterns](#common-patterns)
11. [Anti-Patterns](#anti-patterns)
12. [Migration from Null-Based Languages](#migration-from-null-based-languages)
13. [Implementation Details](#implementation-details)
14. [Related Concepts](#related-concepts)

---

## Overview

`NIL` represents the **absence of a value** in Aria's type system. It is **not an error** - just the indication that no value is present.

**Key Properties**:
- **Type-safe**: Only exists in optional types (`?T`)
- **Explicit**: Cannot be created accidentally (must use `NIL` keyword)
- **Distinct**: Different from NULL (pointers), ERR (TBB errors), and void (FFI marker)
- **Composable**: Works seamlessly with `?.` (safe navigation) and `??` (null coalescing)
- **Zero-cost**: Optimizes to efficient null checks in generated code

**⚠️ CRITICAL**: NIL is for **optional types** only. Use NULL for pointers, ERR for TBB errors, and Result<T,E> for errors with context.

---

## NIL vs NULL vs ERR

| Concept | Purpose | Type System | Example | When to Use |
|---------|---------|-------------|---------|-------------|
| **NIL** | No value present (absence) | Optional types (`?T`) | `user?:data = NIL;` | Value may or may not exist (not an error) |
| **NULL** | Null pointer (address 0x0) | Pointers (`@` types) | `int32@:ptr = NULL;` | Pointer initialization, C FFI |
| **ERR** | TBB error sentinel | TBB types only | `tbb32:err = ERR;` | Arithmetic errors (overflow, div-by-zero) |
| **void** | No return value (FFI marker) | `extern` blocks only | `extern { func:exit = void(int32); }` | C function signatures |

### Quick Decision Guide

```aria
// Situation: Database lookup might not find user
?User:user = find_user(username);  // ✅ NIL if not found (expected)

// Situation: Pointer to allocated memory
int32@:buffer = aria.alloc<int32>(size);  // ✅ NULL if allocation failed

// Situation: Sensor reading could fail
tbb16:temperature = read_sensor();  // ✅ ERR if sensor malfunction

// Situation: C library function returns void
extern { func:c_exit = void(int32:code); }  // ✅ void for C FFI
```

---

## Optional Types (?T Syntax)

An **optional type** represents a value that might or might not be present.

### Declaration

```aria
// Optional integer - might be a value, might be NIL
?int32:user_id = NIL;

// Optional string - starts with no value
?string:cached_name = NIL;

// Optional custom type
struct:User = { string:name, int32:age };
?User:current_user = NIL;

// Optional TBB type (NIL is different from ERR!)
?tbb32:sensor_reading = NIL;  // No reading YET (different from ERR = sensor failed)
```

### Assignment

```aria
?int32:value = NIL;  // Start with no value

value = 42;  // Assign a value

value = NIL;  // Back to no value
```

### Checking for NIL

```aria
?string:name = get_cached_name();

if (name == NIL) {
    stderr.write("No cached name\n");
} else {
    log.write("Name: ");
    name.write_to(log);  // Safe - not NIL
    log.write("\n");
}

// Inequality check
if (name != NIL) {
    // Have a name, use it
    process_name(name);
}
```

---

## Safe Navigation (?. Operator)

The **safe navigation operator** (`?.`) short-circuits if any value in chain is NIL.

### Basic Safe Navigation

```aria
struct:Address = { string:city, string:zip };
struct:User = { string:name, ?Address:address };

?User:user = find_user(username);

// Safe navigation - returns NIL if user is NIL OR address is NIL
?string:city = user?.address?.city;

if (city == NIL) {
    stderr.write("User or address not found\n");
} else {
    log.write("City: ");
    city.write_to(log);
    log.write("\n");
}
```

**How it works**: Each `?.` in the chain checks for NIL. If any step is NIL, the entire expression becomes NIL immediately (no further evaluation).

### Safe Method Calls

```aria
struct:Logger = {
    func:log = (message: string) -> void { ... }
};

?Logger:logger = get_logger();

// Safe method call - only calls if logger is not NIL
logger?.log("Application started");  // No-op if logger is NIL
```

### Deep Chains

```aria
struct:Config = { ?Database:db };
struct:Database = { ?Connection:conn };
struct:Connection = { ?string:uri };

?Config:config = load_config();

// Safe navigation through deep nesting
?string:db_uri = config?.db?.conn?.uri;

if (db_uri == NIL) {
    stderr.write("Database URI not configured\n");
    use_default_database();
}
```

---

## Null Coalescing (?? Operator)

The **null coalescing operator** (`??`) provides a default value when NIL is encountered.

### Basic Null Coalescing

```aria
?string:cached_name = get_cache("user_name");

// Use cached name if present, otherwise "Unknown"
string:name = cached_name ?? "Unknown";

log.write("Name: ");
name.write_to(log);  // Guaranteed to have a value
log.write("\n");
```

### Chained Coalescing

```aria
?string:primary = get_primary_name();
?string:secondary = get_secondary_name();
?string:tertiary = get_tertiary_name();

// Try primary, then secondary, then tertiary, finally use default
string:name = primary ?? secondary ?? tertiary ?? "Default Name";
```

### With Safe Navigation

```aria
struct:User = { ?string:nickname, string:legal_name };
?User:user = find_user(id);

// Get nickname if present, otherwise legal name, otherwise "Anonymous"
string:display_name = user?.nickname ?? user?.legal_name ?? "Anonymous";
```

### Coalescing to NIL-Producing Expression

```aria
?string:cache = get_cache("key");
?string:fallback = get_fallback_cache("key");

// Both might be NIL - result could still be NIL!
?string:result = cache ?? fallback;

if (result == NIL) {
    stderr.write("No cache available\n");
}
```

---

## Pattern Matching with NIL

### Using `when` Conditionals

```aria
?int32:value = get_optional_value();

when value == NIL then
    stderr.write("No value present\n");
else
    log.write("Value: ");
    value.write_to(log);
    log.write("\n");
end
```

### Pattern Matching (pick statement - future syntax)

```aria
?string:result = fetch_data();

pick result {
    NIL: {
        stderr.write("No data available\n");
    }
    some_value: {
        log.write("Data: ");
        some_value.write_to(log);
        log.write("\n");
    }
}
```

### Unwrapping with Assertion

```aria
?User:user = find_user(id);

// Assert that user is NOT NIL (panic if it is!)
User:valid_user = user!;  // Unwrap operator (! suffix)

// Only do this if you're CERTAIN user is not NIL
// Otherwise, program panics: "Unwrapped NIL value"
```

**⚠️ WARNING**: Unwrapping NIL causes a **panic** (program termination). Only use when you can guarantee the value exists!

---

## NIL in Function Returns

### Optional Return Values

```aria
// Function that may or may not find a user
func:find_user = (username: string) -> ?User {
    User?:user = database_lookup(username);
    
    if (user == NIL) {
        pass(NIL);  // Not found - return NIL
    }
    
    pass(user);  // Found - return user
}

// Usage
?User:user = find_user("alice");

if (user == NIL) {
    stderr.write("User not found\n");
} else {
    log.write("Found user: ");
    user.name.write_to(log);
    log.write("\n");
}
```

### NIL vs Result<T, E>

```aria
// NIL: When "not found" is the only failure mode
func:get_cache = (key: string) -> ?string {
    if (!cache_contains(key)) return NIL;
    return cache_fetch(key);
}

// Result: When multiple failure modes exist
enum:CacheError = { NotFound, Expired, Corrupted };

func:get_cache_result = (key: string) -> Result<string, CacheError> {
    if (!cache_contains(key)) fail(NotFound);
    
    ?CacheEntry:entry = cache_fetch_entry(key);
    if (entry.timestamp < current_time()) fail(Expired);
    if (!entry.validate_checksum()) fail(Corrupted);
    
    pass(entry.value);
}
```

**Rule of thumb**: Use `?T` when absence is the only "error". Use `Result<T, E>` when you need to distinguish between multiple error types.

### Multiple Optional Parameters

```aria
func:create_user = (
    name: string,
    email: ?string,          // Optional email
    phone: ?string,          // Optional phone
    address: ?Address        // Optional address
) -> User {
    User:user = {
        name: name,
        email: email ?? "no-email@example.com",
        phone: phone ?? "000-000-0000",
        address: address ?? default_address
    };
    
    return user;
}

// Usage - any optional parameter can be NIL
User:alice = create_user("Alice", "alice@example.com", NIL, NIL);
User:bob = create_user("Bob", NIL, "555-1234", NIL);
```

---

## Nikola Consciousness Applications

### Missing Patient Records (Therapy Data)

```aria
struct:TherapySession = {
    int32:session_id,
    string:patient_name,
    ?string:therapist_notes,  // Optional (therapist might not have written notes yet)
    ?tbb16:emotional_score    // Optional (might not be scored yet)
};

?TherapySession:session = fetch_session(session_id);

if (session == NIL) {
    stderr.write("Session not found\n");
    return HTTP_404_NOT_FOUND;
}

// Session exists - check optional fields
string:notes = session.therapist_notes ?? "[No notes yet]";
tbb16:emotion = session.emotional_score ?? 0;  // Default to neutral (0) if not scored

log.write("Session ");
session.session_id.write_to(log);
log.write(": ");
session.patient_name.write_to(log);
log.write("\n");
log.write("Notes: ");
notes.write_to(log);
log.write("\n");
log.write("Emotional score: ");
emotion.write_to(log);
log.write("\n");
```

### Sensor Fusion with Missing Data

```aria
// Nikola emotional state from multiple sensors (some might be offline)
struct:EmotionalSensors = {
    ?tbb16:audio_emotion,    // Voice tone analysis (microphone might fail)
    ?tbb16:facial_emotion,   // Facial expression (camera might fail)
    ?tbb16:posture_emotion   // Body language (depth sensor might fail)
};

EmotionalSensors:sensors = read_all_emotion_sensors();

// Compute emotional state using only available sensors
tbb16:combined_emotion = 0;
int32:sensor_count = 0;

// Audio sensor
if (sensors.audio_emotion != NIL) {
    // Check that sensor didn't ERROR (ERR is different from NIL!)
    if (sensors.audio_emotion != ERR) {
        combined_emotion = combined_emotion + sensors.audio_emotion;
        sensor_count = sensor_count + 1;
    }
}

// Facial sensor
if (sensors.facial_emotion != NIL) {
    if (sensors.facial_emotion != ERR) {
        combined_emotion = combined_emotion + sensors.facial_emotion;
        sensor_count = sensor_count + 1;
    }
}

// Posture sensor
if (sensors.posture_emotion != NIL) {
    if (sensors.posture_emotion != ERR) {
        combined_emotion = combined_emotion + sensors.posture_emotion;
        sensor_count = sensor_count + 1;
    }
}

if (sensor_count == 0) {
    stderr.write("No emotional sensors available\n");
    return ERR;  // Can't compute emotion
}

tbb16:average_emotion = combined_emotion / sensor_count;

log.write("Emotional state (");
sensor_count.write_to(log);
log.write(" sensors): ");
average_emotion.write_to(log);
log.write("\n");
```

**Key distinction**: NIL = sensor not present/configured. ERR = sensor malfunction. Both need handling!

### Configuration with Defaults

```aria
struct:NikolaConfig = {
    ?int32:sample_rate_hz,        // Optional (default: 1000 Hz)
    ?int32:max_sessions,          // Optional (default: 100)
    ?string:log_path,             // Optional (default: "/var/log/nikola")
    ?tbb64:simulation_timesteps   // Optional (default: 10,000)
};

?NikolaConfig:config = load_config("nikola.conf");

// Apply defaults for missing config values
int32:sample_rate = config?.sample_rate_hz ?? 1000;
int32:max_sessions = config?.max_sessions ?? 100;
string:log_path = config?.log_path ?? "/var/log/nikola";
tbb64:timesteps = config?.simulation_timesteps ?? 10000;

log.write("Nikola configuration:\n");
log.write("  Sample rate: "); sample_rate.write_to(log); log.write(" Hz\n");
log.write("  Max sessions: "); max_sessions.write_to(log); log.write("\n");
log.write("  Log path: "); log_path.write_to(log); log.write("\n");
log.write("  Simulation timesteps: "); timesteps.write_to(log); log.write("\n");
```

---

## Performance Characteristics

### Zero-Cost Abstraction

Optional types compile to efficient null checks with no runtime overhead:

```aria
?int32:value = get_value();

if (value == NIL) {
    handle_no_value();
} else {
    process_value(value);
}
```

**Generated code** (pseudocode):
```c
// Optimized to single comparison
int32_t* value_ptr = get_value();
if (value_ptr == NULL) {
    handle_no_value();
} else {
    process_value(*value_ptr);
}
```

**Performance**: Identical to manual null pointer checks in C/C++.

### Safe Navigation Optimization

```aria
?User:user = find_user(id);
?string:city = user?.address?.city;
```

**Generated code** (pseudocode):
```c
User* user = find_user(id);
const char* city = NULL;

if (user != NULL) {
    Address* address = user->address;
    if (address != NULL) {
        city = address->city;
    }
}
```

**Performance**: Short-circuit evaluation means only necessary checks are performed. Branch predictors optimize common cases (likely not NIL).

### Null Coalescing Optimization

```aria
string:name = cached_name ?? fallback_name ?? "Default";
```

**Generated code** (pseudocode):
```c
const char* name;
if (cached_name != NULL) {
    name = cached_name;
} else if (fallback_name != NULL) {
    name = fallback_name;
} else {
    name = "Default";
}
```

**Performance**: Lazy evaluation - only evaluates expressions until non-NIL found.

### Memory Layout

Optional types typically use **pointer representation** or **tag byte**:

| Implementation | Size Overhead | Use Case |
|----------------|---------------|----------|
| **Nullable pointer** | 0 bytes (pointer itself is null) | Heap-allocated types, strings |
| **Tagged union** | 1 byte (bool flag) | Small stack values (int32, tbb32) |
| **Niche optimization** | 0 bytes (use invalid value as NIL sentinel) | Types with unused bit patterns |

**Example**: `?string` is just a nullable pointer (8 bytes on 64-bit). `?int32` might be a 5-byte tagged union (4 bytes + 1 byte tag).

---

## Common Patterns

### Optional Chaining with Fallback

```aria
?Config:config = load_config();
?Database:db = config?.database;
?string:connection_string = db?.connection_string;

string:conn = connection_string ?? "localhost:5432";  // Default connection

log.write("Database connection: ");
conn.write_to(log);
log.write("\n");
```

### Early Return on NIL

```aria
func:process_user = (username: string) -> result {
    ?User:user = find_user(username);
    
    if (user == NIL) {
        stderr.write("User not found\n");
        fail(1);  // Early return on NIL
    }
    
    // Safe to use user here (guaranteed not NIL)
    log.write("Processing user: ");
    user.name.write_to(log);
    log.write("\n");
    
    pass(0);
}
```

### Transform Optional Value

```aria
?string:cached_data = get_cache("key");

// Map operation: transform value if present, NIL otherwise
?int32:cached_length = cached_data?.length;  // NIL if cached_data is NIL

if (cached_length == NIL) {
    stderr.write("No cached data\n");
} else {
    log.write("Cached data length: ");
    cached_length.write_to(log);
    log.write("\n");
}
```

### Filter Pattern

```aria
?User[]:all_users = fetch_all_users();

// Filter out users with no email
User[]:users_with_email = [];

if (all_users != NIL) {
    till all_users.length loop:i
        User:user = all_users[i];
        
        if (user.email != NIL) {
            users_with_email.append(user);
        }
    end
}
```

---

## Anti-Patterns

### ❌ Using NIL for Errors

```aria
// WRONG: NIL should not represent errors
func:divide = (a: int32, b: int32) -> ?int32 {
    if (b == 0) return NIL;  // ❌ Division by zero is ERROR, not absence!
    return a / b;
}

// RIGHT: Use Result or ERR
func:divide_tbb = tbb32(tbb32:a, tbb32:b) {
    if b == 0 then pass(ERR); end  // ✅ Clear error signal
    pass(a / b);
};

func:divide_result = Result<int32, string>(int32:a, int32:b) {
    if b == 0 then fail("Division by zero"); end  // ✅ Error with context
    pass(a / b);
};
```

### ❌ Unwrapping Without Checking

```aria
?User:user = find_user(id);

// WRONG: Unwrapping without checking (can panic!)
User:valid_user = user!;  // ❌ Panics if user is NIL!

// RIGHT: Check before unwrapping
if (user == NIL) {
    stderr.write("User not found\n");
    return;
}

User:valid_user = user!;  // ✅ Safe (already checked)

// BETTER: Use if-let or pattern matching (future syntax)
if let User:valid_user = user {
    // valid_user is guaranteed non-NIL in this scope
    process_user(valid_user);
}
```

### ❌ Confusing NIL with ERR

```aria
// WRONG: Mixing NIL and ERR semantics
?tbb32:sensor = read_sensor();  // ❌ Sensor failure returns ERR, not NIL!

// RIGHT: Use tbb32 directly (ERR handles failure)
tbb32:sensor = read_sensor();  // ✅ ERR if sensor malfunction

if (sensor == ERR) {
    stderr.write("Sensor malfunction\n");
}

// OR use optional for "sensor not present" vs ERR for "sensor malfunction"
?tbb32:sensor = get_optional_sensor();

if (sensor == NIL) {
    stderr.write("Sensor not configured\n");
} else if (sensor == ERR) {
    stderr.write("Sensor malfunction\n");
} else {
    log.write("Sensor reading: ");
    sensor.write_to(log);
    log.write("\n");
}
```

### ❌ Excessive Unwrapping

```aria
// WRONG: Manually unwrapping nested optionals
?Config:config = load_config();

if (config != NIL) {
    if (config.database != NIL) {
        if (config.database.connection != NIL) {
            string:conn = config.database.connection.uri;
        }
    }
}

// RIGHT: Use safe navigation
?string:conn = config?.database?.connection?.uri;

if (conn == NIL) {
    stderr.write("Database connection not configured\n");
}
```

---

## Migration from Null-Based Languages

### JavaScript/TypeScript

```javascript
// JavaScript - null and undefined both exist
const user = findUser(username);

if (user === null || user === undefined) {
    console.error("User not found");
}

// Nullable chaining (TypeScript)
const city = user?.address?.city ?? "Unknown";
```

```aria
// Aria - only NIL, no separate "undefined"
?User:user = find_user(username);

if (user == NIL) {
    stderr.write("User not found\n");
}

// Safe navigation + null coalescing
string:city = user?.address?.city ?? "Unknown";
```

**Key difference**: Aria has **one** absence value (NIL), not two (null + undefined).

### Python

```python
# Python - None is the absence value
user = find_user(username)

if user is None:
    print("User not found", file=sys.stderr)

# No built-in safe navigation or null coalescing (until Python 3.10)
city = user.address.city if user and user.address else "Unknown"
```

```aria
// Aria - NIL with built-in operators
?User:user = find_user(username);

if (user == NIL) {
    stderr.write("User not found\n");
}

// Safe navigation + null coalescing (built-in)
string:city = user?.address?.city ?? "Unknown";
```

### Java/Kotlin

```java
// Java - null is the absence value (NullPointerException danger!)
User user = findUser(username);

if (user == null) {
    System.err.println("User not found");
    return;
}

// No safe navigation (use Optional<T>)
String city = user.getAddress() != null ? user.getAddress().getCity() : "Unknown";
```

```kotlin
// Kotlin - nullable types (similar to Aria!)
val user: User? = findUser(username)

if (user == null) {
    System.err.println("User not found")
    return
}

// Safe navigation + Elvis operator
val city = user?.address?.city ?: "Unknown"
```

```aria
// Aria - very similar to Kotlin!
?User:user = find_user(username);

if (user == NIL) {
    stderr.write("User not found\n");
    return;
}

// Safe navigation + null coalescing
string:city = user?.address?.city ?? "Unknown";
```

**Aria is closest to Kotlin** in null safety design!

---

## Implementation Details

### Nullable Pointer Representation

For heap-allocated types (strings, objects):

```c
// ?string is just a nullable pointer
typedef struct {
    const char* data;  // NULL = NIL
    size_t length;
} aria_string;

aria_string* optional_string;  // NULL pointer = NIL
```

**Size**: Same as pointer (8 bytes on 64-bit), no overhead.

### Tagged Union Representation

For small stack types:

```c
// ?int32 is a tagged union
typedef struct {
    bool has_value;  // false = NIL
    int32_t value;
} optional_int32;
```

**Size**: `sizeof(int32) + sizeof(bool)` = 5 bytes (padding may increase to 8).

### Niche Optimization

For types with unused bit patterns:

```c
// ?tbb32 can use ERR (-2,147,483,648) as NIL representation!
// No tag byte needed - ERR value is never valid data, so repurpose as NIL
typedef int32_t optional_tbb32;  // INT32_MIN = NIL

// Check for NIL
if (value == INT32_MIN) {
    // NIL
}
```

**Size**: Same as tbb32 (4 bytes), **zero overhead!**

⚠️ **NOTE**: This is implementation detail! In code, always use `NIL`, never `INT32_MIN`.

---

## Related Concepts

### Other Special Values

- **NULL**: C-style null pointer (address 0x0) - pointer types only
- **ERR**: TBB error sentinel - arithmetic/data errors
- **void**: C FFI marker (no return value) - extern blocks only

### Error Handling Alternatives

- **Result<T, E>**: Errors with context (parsing, validation, I/O failures)
- **Panics (!!!)**: Unrecoverable errors (logic bugs, invariant violations)
- **Exceptions (try/catch)**: Exceptional conditions (network timeouts, file errors)

### Related Operators

- **Safe navigation (`?.`)**: Short-circuit on NIL
- **Null coalescing (`??`)**: Default value on NIL
- **Unwrap (`!`)**: Assert not NIL (panics if NIL)

---

## Summary

`NIL` is Aria's **type-safe absence value** for optional types:

✅ **Type-safe**: Only exists in optional types (`?T`)  
✅ **Explicit**: Must use `NIL` keyword (cannot be created accidentally)  
✅ **Composable**: Works with `?.` (safe navigation) and `??` (null coalescing)  
✅ **Zero-cost**: Compiles to efficient null checks (same as C pointers)  
✅ **Distinct**: Different from NULL (pointers), ERR (TBB errors), void (FFI)  
✅ **Clear semantics**: "No value present" (not an error, just absence)  

**Use NIL for**:
- Optional values ("may or may not exist" without being an error)
- Database queries that might not find records
- Cache lookups (cache miss is not an error)
- Configuration fields with defaults
- Sensor data that might be unavailable

**Don't use NIL for**:
- Errors (use ERR for TBB data errors, Result for errors with context)
- Pointers (use NULL instead)
- C FFI (use NULL for pointers, void for no-return)
- Indicating failure (NIL = absence, not failure)

---

**Next**: [NULL (C Pointer Sentinel)](NULL.md) - Legacy null pointers  
**See Also**: [ERR (TBB Error Sentinel)](ERR.md), [Result Type](result.md), [Optional Types](optional_types.md)

---

*Aria Language Project - Type-Safe Absence by Design*  
*February 12, 2026 - Establishing timestamped prior art on NIL semantics*
