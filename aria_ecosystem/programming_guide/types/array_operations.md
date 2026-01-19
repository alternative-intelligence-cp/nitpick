# Array Operations

**Category**: Types → Arrays  
**Purpose**: Common array operations

---

## Length

```aria
arr: []i32 = [1, 2, 3, 4, 5];
len: i32 = arr.length();  // 5
```

---

## Append

```aria
arr: []i32 = [];
arr.append(10);
arr.append(20);
// arr = [10, 20]
```

---

## Slicing

```aria
arr: []i32 = [1, 2, 3, 4, 5];

sub: []i32 = arr[1..4];    // [2, 3, 4]
sub: []i32 = arr[..3];     // [1, 2, 3]
sub: []i32 = arr[2..];     // [3, 4, 5]
```

---

## Iteration

```aria
// For-each
for value in arr {
    stdout << value;
}

// With index
for i in 0..arr.length() {
    stdout << arr[i];
}
```

---

## Map

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

doubled: []i32 = numbers.map(fn(x: i32) -> i32 {
    return x * 2;
});
// [2, 4, 6, 8, 10]
```

---

## Filter

```aria
numbers: []i32 = [1, 2, 3, 4, 5, 6];

evens: []i32 = numbers.filter(fn(x: i32) -> bool {
    return x % 2 == 0;
});
// [2, 4, 6]
```

---

## Reduce/Fold

```aria
numbers: []i32 = [1, 2, 3, 4, 5];

sum: i32 = numbers.reduce(0, fn(acc: i32, x: i32) -> i32 {
    return acc + x;
});
// 15
```

---

## Sort

```aria
arr: []i32 = [5, 2, 8, 1, 9];
arr.sort();  // [1, 2, 5, 8, 9]

// Custom comparator
arr.sort(fn(a: i32, b: i32) -> i32 {
    return a <=> b;  // Ascending
});
```

---

## Reverse

```aria
arr: []i32 = [1, 2, 3, 4, 5];
arr.reverse();  // [5, 4, 3, 2, 1]
```

---

## Contains

```aria
arr: []i32 = [1, 2, 3, 4, 5];

has: bool = arr.contains(3);  // true
has: bool = arr.contains(10);  // false
```

---

## Find

```aria
arr: []i32 = [1, 2, 3, 4, 5];

// Find first matching
found: ?i32 = arr.find(fn(x: i32) -> bool {
    return x > 3;
});
// Some(4)
```

---

## Join (Strings)

```aria
words: []string = ["Hello", "World", "Aria"];
sentence: string = words.join(" ");
// "Hello World Aria"
```

---

## Related

- [Arrays](array.md)
- [Array Declaration](array_declaration.md)

---

**Remember**: Arrays have **rich operations** for transformation!
