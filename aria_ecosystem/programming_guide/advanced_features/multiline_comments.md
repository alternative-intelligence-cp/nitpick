# Multi-Line Comments

**Category**: Advanced Features → Syntax  
**Purpose**: Comments spanning multiple lines

---

## Overview

Multi-line comments provide **block commenting** for longer explanations and documentation.

---

## Basic Multi-Line Comment

```aria
/*
 This is a multi-line comment.
 It can span as many lines as needed.
 Useful for longer explanations.
*/

x: i32 = 42;
```

---

## Inline Multi-Line Comments

```aria
fn example() {
    x: i32 = /* comment */ 42;
    y: i32 = add(/* a */ 10, /* b */ 20);
}
```

---

## Nested Multi-Line Comments

```aria
/*
 Outer comment
 /* This is nested */
 Still in outer comment
 /* Another /* deeply nested */ comment */
*/

fn nested_example() {
    /* Level 1 /* Level 2 /* Level 3 */ Level 2 */ Level 1 */
    x: i32 = 42;
}
```

---

## Block Documentation

```aria
/*
 ┌────────────────────────────────────────┐
 │  USER AUTHENTICATION MODULE            │
 ├────────────────────────────────────────┤
 │  Handles user login, logout, and       │
 │  session management.                   │
 │                                        │
 │  Dependencies:                         │
 │  - Database for user storage           │
 │  - Session manager for tokens          │
 └────────────────────────────────────────┘
*/

mod auth;
```

---

## ASCII Art Comments

```aria
/*
     _    ____ ___    _    
    / \  |  _ \_ _|  / \   
   / _ \ | |_) | |  / _ \  
  / ___ \|  _ <| | / ___ \ 
 /_/   \_\_| \_\___/_/   \_\
 
 Aria Programming Language
 Version 1.0
*/

mod core;
```

---

## Commenting Out Code Blocks

```aria
fn refactored_function() {
    new_implementation();
    
    /*
    // Old implementation (kept for reference)
    fn old_way() {
        step1();
        step2();
        step3();
    }
    */
}
```

---

## Common Patterns

### File Headers

```aria
/*
 File: user_service.aria
 Author: Development Team
 Created: 2025-01-01
 
 Description:
   Provides user management functionality including
   CRUD operations, authentication, and authorization.
 
 Dependencies:
   - database.aria
   - auth.aria
   - validation.aria
*/

mod user_service;
```

---

### Function Documentation

```aria
/*
 Function: calculate_compound_interest
 
 Calculates compound interest using the formula:
   A = P(1 + r/n)^(nt)
 
 Where:
   P = Principal amount
   r = Annual interest rate (decimal)
   n = Number of times interest is compounded per year
   t = Number of years
 
 Parameters:
   principal - Initial amount
   rate - Annual interest rate (e.g., 0.05 for 5%)
   compounds_per_year - How often interest is compounded
   years - Investment duration
 
 Returns:
   Final amount after interest
 
 Example:
   // $1000 at 5% compounded monthly for 10 years
   result = calculate_compound_interest(1000.0, 0.05, 12, 10);
*/
fn calculate_compound_interest(
    principal: f64,
    rate: f64,
    compounds_per_year: i32,
    years: i32
) -> f64 {
    // Implementation
}
```

---

### Algorithm Explanation

```aria
fn quick_sort(arr: []i32) -> []i32 {
    /*
     Quick Sort Algorithm
     
     1. Choose pivot (last element)
     2. Partition array around pivot
     3. Recursively sort left partition (< pivot)
     4. Recursively sort right partition (> pivot)
     
     Time Complexity:
       Best:    O(n log n)
       Average: O(n log n)
       Worst:   O(n²)
     
     Space Complexity: O(log n) for recursion stack
    */
    
    if arr.len() <= 1 {
        return arr;
    }
    
    // Implementation
}
```

---

### License Headers

```aria
/*
 MIT License
 
 Copyright (c) 2025 Example Corporation
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in all
 copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 SOFTWARE.
*/

mod licensed_code;
```

---

### TODO Sections

```aria
/*
 TODO List:
 
 [ ] Implement user registration
 [x] Add input validation
 [ ] Add email verification
 [x] Password hashing
 [ ] Session management
 [ ] Rate limiting
 
 Priority: HIGH
 Assigned to: Backend Team
 Deadline: 2025-02-01
*/

fn user_registration() {
    // Implementation
}
```

---

### Warning Sections

```aria
/*
 ⚠️  WARNING ⚠️
 
 This function modifies global state and is NOT thread-safe.
 
 - DO NOT call from multiple threads
 - Ensure proper locking before use
 - Consider using thread-safe alternative: safe_update()
 
 This will be deprecated in version 2.0
*/
fn unsafe_global_update() {
    // Implementation
}
```

---

## Best Practices

### ✅ DO: Use for Complex Explanations

```aria
/*
 This sorting algorithm uses a hybrid approach:
 
 1. For small arrays (< 10 elements):
    - Uses insertion sort (faster for small data)
 
 2. For medium arrays (10-1000 elements):
    - Uses quicksort with median-of-three pivot
 
 3. For large arrays (> 1000 elements):
    - Uses parallel merge sort
    - Splits into 4 chunks processed concurrently
 
 Benchmarks show this approach is 30% faster than
 standard quicksort across varied workloads.
*/
fn hybrid_sort(arr: []i32) -> []i32 {
    // Implementation
}
```

### ✅ DO: Format for Readability

```aria
/*
 ╔═══════════════════════════════════════════╗
 ║   DATABASE CONNECTION POOL                ║
 ╠═══════════════════════════════════════════╣
 ║                                           ║
 ║   Max connections: 100                    ║
 ║   Min connections: 10                     ║
 ║   Timeout: 30 seconds                     ║
 ║   Retry attempts: 3                       ║
 ║                                           ║
 ╚═══════════════════════════════════════════╝
*/

struct ConnectionPool {
    // Fields
}
```

### ⚠️ WARNING: Keep Updated

```aria
// ❌ Bad - outdated comment
/*
 This function uses SHA-1 for hashing
 (updated to SHA-256 in v2.0)
*/
fn hash_password(password: string) -> string {
    return sha256(password);  // Comment says SHA-1!
}

// ✅ Good - accurate comment
/*
 This function uses SHA-256 for password hashing.
 
 Changed from SHA-1 in v2.0 for improved security.
 See: https://issues.example.com/SECURITY-42
*/
fn hash_password(password: string) -> string {
    return sha256(password);
}
```

### ❌ DON'T: Overuse

```aria
// ❌ Excessive multi-line comments for simple code
fn add(a: i32, b: i32) -> i32 {
    /*
     This function adds two integers.
     It takes parameter a and parameter b.
     Then it adds them together.
     Finally it returns the result.
    */
    return a + b;
}

// ✅ Simple code needs minimal or no comments
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

---

## Comparison with Single-Line

```aria
// Single-line comment
// Good for short explanations
// Can stack multiple lines

/*
 Multi-line comment
 Better for longer explanations
 Easier to read as a block
*/
```

---

## Related

- [comments](comments.md) - Comment basics

---

**Remember**: Multi-line comments are perfect for **detailed explanations**, **documentation blocks**, and **file headers**!
