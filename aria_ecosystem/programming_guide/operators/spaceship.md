# The `<=>` Operator (Three-Way Comparison)

**Category**: Operators → Comparison  
**Syntax**: `a <=> b`  
**Purpose**: Three-way comparison returning ordering relationship

---

## Overview

The **spaceship operator** (`<=>`) performs a three-way comparison between two values, returning:

- **Negative** if `a < b`
- **Zero** if `a == b`  
- **Positive** if `a > b`

**Philosophy**: Single comparison for sorting and ordering, efficient and expressive.

---

## Basic Syntax

```aria
int64:result = a <=> b;

if (result < 0) {
    print("a is less than b");
} else if (result == 0) {
    print("a equals b");
} else {
    print("a is greater than b");
}
```

### Return Values

| Condition | Return Value | Example |
|-----------|--------------|---------|
| `a < b` | Negative (typically -1) | `5 <=> 10` → `-1` |
| `a == b` | Zero (0) | `5 <=> 5` → `0` |
| `a > b` | Positive (typically +1) | `10 <=> 5` → `+1` |

---

## Example 1: Basic Comparison

```aria
int64:x = 10;
int64:y = 20;

int64:cmp = x <=> y;
// cmp = -1 (x < y)

cmp = x <=> x;
// cmp = 0 (x == x)

cmp = y <=> x;
// cmp = 1 (y > x)
```

---

## Example 2: Sorting Comparison Function

### Using Spaceship Operator

```aria
func:compareInts = int64(int64:a, int64:b) {
    pass(a <=> b);  // Single expression!
};

// For ascending order
int64[]:array = [5, 2, 8, 1, 9];
sort(array, compareInts);
// array = [1, 2, 5, 8, 9]
```

### Without Spaceship Operator

```aria
func:compareInts = int64(int64:a, int64:b) {
    if (a < b) { pass(-1); }
    if (a == b) { pass(0); }
    pass(1);
};
```

**Benefit**: Spaceship is more concise and efficient.

---

## Use Case 1: Sorting Arrays

### Ascending Order

```aria
func:ascending = int64(int64:a, int64:b) {
    pass(a <=> b);
};

int64[]:data = [42, 17, 99, 3, 56];
sort(data, ascending);
// data = [3, 17, 42, 56, 99]
```

### Descending Order

```aria
func:descending = int64(int64:a, int64:b) {
    pass(b <=> a);  // Reverse comparison
};

int64[]:data = [42, 17, 99, 3, 56];
sort(data, descending);
// data = [99, 56, 42, 17, 3]
```

---

## Use Case 2: Custom Object Comparison

### Struct Comparison

```aria
%STRUCT Person {
    string:name,
    int64:age
}

func:comparePersonsByAge = int64(Person:a, Person:b) {
    pass(a.age <=> b.age);
};

Person[]:people = [
    { "Alice", 30 },
    { "Bob", 25 },
    { "Charlie", 35 }
];

sort(people, comparePersonsByAge);
// Sorted by age: Bob(25), Alice(30), Charlie(35)
```

### Multi-Field Comparison

```aria
func:comparePersons = int64(Person:a, Person:b) {
    // First compare by age
    int64:ageCmp = a.age <=> b.age;
    if (ageCmp != 0) {
        pass(ageCmp);
    }
    
    // Ages equal, compare by name
    pass(a.name <=> b.name);
};
```

---

## Use Case 3: Binary Search

### Finding Element

```aria
func:binarySearch = int64(int64[]:array, int64:target) {
    int64:left = 0;
    int64:right = array.length - 1;
    
    while (left <= right) {
        int64:mid = (left + right) / 2;
        int64:cmp = target <=> array[mid];
        
        if (cmp == 0) {
            pass(mid);  // Found!
        } else if (cmp < 0) {
            right = mid - 1;  // Search left half
        } else {
            left = mid + 1;   // Search right half
        }
    }
    
    pass(-1);  // Not found
};

int64[]:sorted = [1, 3, 5, 7, 9, 11, 13];
int64:index = binarySearch(sorted, 7);
// index = 3
```

---

## Use Case 4: Priority Queue

### Heap Comparison

```aria
func:heapCompare = int64(int64:a, int64:b) {
    pass(b <=> a);  // Max heap (reverse order)
};

func:insert = void(int64[]:heap, int64:value) {
    heap.append(value);
    heapifyUp(heap, heap.length - 1, heapCompare);
};

func:extractMax = int64(int64[]:heap) {
    int64:max = heap[0];
    heap[0] = heap[heap.length - 1];
    heap.removeLast();
    heapifyDown(heap, 0, heapCompare);
    pass(max);
};
```

---

## String Comparison

### Lexicographic Ordering

```aria
string:a = "apple";
string:b = "banana";

int64:cmp = a <=> b;
// cmp < 0 (apple comes before banana)

cmp = "zebra" <=> "aardvark";
// cmp > 0 (zebra comes after aardvark)

cmp = "hello" <=> "hello";
// cmp == 0 (equal strings)
```

### Case-Sensitive vs Case-Insensitive

```aria
// Case-sensitive (default)
int64:cmp1 = "Apple" <=> "apple";
// cmp1 < 0 ('A' < 'a' in ASCII)

// Case-insensitive (using lowercase)
int64:cmp2 = "Apple".toLowerCase() <=> "apple".toLowerCase();
// cmp2 == 0 (equal when case ignored)
```

---

## Numeric Comparison

### Integer Types

```aria
int64:a = 100;
int64:b = 200;

int64:cmp = a <=> b;
// cmp = -1
```

### Floating Point

```aria
float64:x = 3.14;
float64:y = 2.71;

int64:cmp = x <=> y;
// cmp = 1 (3.14 > 2.71)
```

### TBB (Twisted Balanced Binary)

```aria
tbb8:a = 50;
tbb8:b = ERR;  // -128 sentinel

int64:cmp = a <=> b;
// cmp > 0 (50 > ERR)
```

---

## Common Patterns

### Pattern 1: Min/Max Functions

```aria
func:min = int64(int64:a, int64:b) {
    pass(is (a <=> b) < 0 : a : b);
};

func:max = int64(int64:a, int64:b) {
    pass(is (a <=> b) > 0 : a : b);
};

int64:smaller = min(10, 20);  // 10
int64:larger = max(10, 20);   // 20
```

### Pattern 2: Clamp Function

```aria
func:clamp = int64(int64:value, int64:min, int64:max) {
    if ((value <=> min) < 0) { pass(min); }
    if ((value <=> max) > 0) { pass(max); }
    pass(value);
};

int64:x = clamp(150, 0, 100);  // 100
int64:y = clamp(-50, 0, 100);  // 0
int64:z = clamp(50, 0, 100);   // 50
```

### Pattern 3: Range Check

```aria
func:inRange = bool(int64:value, int64:min, int64:max) {
    pass((value <=> min) >= 0 && (value <=> max) <= 0);
};

bool:check = inRange(50, 0, 100);  // true
check = inRange(150, 0, 100);      // false
```

### Pattern 4: Version Comparison

```aria
%STRUCT Version {
    int64:major,
    int64:minor,
    int64:patch
}

func:compareVersions = int64(Version:a, Version:b) {
    int64:majorCmp = a.major <=> b.major;
    if (majorCmp != 0) { pass(majorCmp); }
    
    int64:minorCmp = a.minor <=> b.minor;
    if (minorCmp != 0) { pass(minorCmp); }
    
    pass(a.patch <=> b.patch);
};

Version:v1 = { 1, 2, 3 };
Version:v2 = { 1, 3, 0 };

int64:cmp = compareVersions(v1, v2);
// cmp < 0 (v1.2.3 < v1.3.0)
```

---

## Performance Benefits

### Single Comparison

```aria
// Efficient: Single comparison operation
int64:cmp = a <=> b;

if (cmp < 0) { handleLess(); }
else if (cmp == 0) { handleEqual(); }
else { handleGreater(); }
```

### vs Multiple Comparisons

```aria
// Less efficient: Multiple comparison operations
if (a < b) {
    handleLess();
} else if (a == b) {
    handleEqual();
} else {
    handleGreater();
}
```

**Benefit**: Spaceship operator may perform comparison **once** internally, avoiding redundant checks.

---

## Best Practices

### ✅ Use for Sorting Comparisons

```aria
// GOOD: Concise sort comparator
func:compare = int64(int64:a, int64:b) {
    pass(a <=> b);
};
```

### ✅ Use for Three-Way Logic

```aria
// GOOD: Clean three-way branching
pick(value <=> threshold) {
    case -1:
        handleBelow();
    case 0:
        handleEqual();
    case 1:
        handleAbove();
}
```

### ✅ Chain Comparisons for Complex Sorting

```aria
// GOOD: Multi-level comparison
func:compare = int64(Record:a, Record:b) {
    int64:cmp = a.priority <=> b.priority;
    if (cmp != 0) { pass(cmp); }
    pass(a.timestamp <=> b.timestamp);
};
```

### ❌ Don't Use for Simple Boolean Checks

```aria
// BAD: Overkill for simple check
if ((a <=> b) < 0) { doSomething(); }

// GOOD: Use direct comparison
if (a < b) { doSomething(); }
```

### ❌ Don't Assume Specific Values

```aria
// BAD: Assuming -1, 0, +1
if (a <=> b == -1) { /* ... */ }

// GOOD: Use relational checks
if ((a <=> b) < 0) { /* ... */ }
```

---

## Comparison with Other Languages

### Aria `<=>`

```aria
int64:cmp = a <=> b;
if (cmp < 0) { /* a < b */ }
```

### C++ `<=>`

```cpp
auto cmp = a <=> b;
if (cmp < 0) { /* a < b */ }
```

### Perl `<=>`

```perl
my $cmp = $a <=> $b;
if ($cmp < 0) { # a < b }
```

### Ruby `<=>`

```ruby
cmp = a <=> b
if cmp < 0 then # a < b end
```

### PHP `<=>`

```php
$cmp = $a <=> $b;
if ($cmp < 0) { /* a < b */ }
```

---

## Advanced Examples

### Example 1: Merge Sort

```aria
func:merge = int64[](int64[]:left, int64[]:right) {
    int64[]:result = [];
    int64:i = 0, j = 0;
    
    while (i < left.length && j < right.length) {
        int64:cmp = left[i] <=> right[j];
        
        if (cmp <= 0) {
            result.append(left[i]);
            i++;
        } else {
            result.append(right[j]);
            j++;
        }
    }
    
    // Append remaining
    while (i < left.length) { result.append(left[i++]); }
    while (j < right.length) { result.append(right[j++]); }
    
    pass(result);
};
```

### Example 2: Custom Comparator Chain

```aria
%STRUCT Employee {
    string:department,
    int64:level,
    string:name
}

func:compareEmployees = int64(Employee:a, Employee:b) {
    // 1. Department
    int64:deptCmp = a.department <=> b.department;
    if (deptCmp != 0) { pass(deptCmp); }
    
    // 2. Level (descending)
    int64:levelCmp = b.level <=> a.level;
    if (levelCmp != 0) { pass(levelCmp); }
    
    // 3. Name
    pass(a.name <=> b.name);
};
```

### Example 3: Balanced Ternary Comparison

```aria
trit:a = +1;  // Positive trit
trit:b = -1;  // Negative trit

int64:cmp = a <=> b;
// cmp > 0 (+1 > -1)

trit:c = 0;   // Zero trit
cmp = c <=> b;
// cmp > 0 (0 > -1)
```

---

## Related Topics

- [Comparison Operators](comparison.md) - <, >, ==, !=, <=, >=
- [Ternary Operator](is_ternary.md) - Inline conditionals
- [pick Construct](../control_flow/pick.md) - Pattern matching with spaceship
- [Sorting](../stdlib/sort.md) - Array sorting functions
- [TBB Types](../types/tbb8.md) - Twisted Balanced Binary comparison

---

**Status**: Comprehensive  
**Specification**: aria_specs.txt Line 184  
**Unique Feature**: Single operator for three-way comparison, efficient sorting
