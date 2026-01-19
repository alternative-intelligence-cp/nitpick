# TBB Sticky Errors

**Category**: Types → TBB System  
**Purpose**: Understanding error propagation

---

## Sticky Error Propagation

Once a TBB value becomes ERR, it **stays ERR** through all operations - errors "stick"!

---

## How It Works

```aria
x: tbb32 = ERR;
y: tbb32 = 100;

// All operations return ERR
sum: tbb32 = x + y;      // ERR
diff: tbb32 = x - y;     // ERR
prod: tbb32 = x * y;     // ERR
quot: tbb32 = x / y;     // ERR
mod: tbb32 = x % y;      // ERR
```

---

## Propagation Rules

### Arithmetic

```aria
// If either operand is ERR, result is ERR
ERR + 5 = ERR
10 + ERR = ERR
ERR * ERR = ERR
```

### Comparison

```aria
// ERR compares as less than all valid values
ERR < -127  // true (for tbb8)
ERR == ERR  // true
```

---

## Benefits

### No Manual Checks Needed

```aria
// Without TBB - manual checks
result1: i32 = step1();
when result1 == ERROR_CODE then return ERROR_CODE end;

result2: i32 = step2(result1);
when result2 == ERROR_CODE then return ERROR_CODE end;

result3: i32 = step3(result2);
when result3 == ERROR_CODE then return ERROR_CODE end;

// With TBB - automatic propagation
result1: tbb32 = step1();
result2: tbb32 = step2(result1);  // ERR if result1 is ERR
result3: tbb32 = step3(result2);  // ERR if result2 is ERR

// Check once at end
when result3 == ERR then
    handle_error();
end
```

### Clear Error Flow

```aria
// Errors flow through calculation
balance: tbb32 = get_balance();      // Might be ERR
fee: tbb32 = calculate_fee(balance);  // ERR if balance is ERR
total: tbb32 = balance + fee;        // ERR if either is ERR

when total == ERR then
    stdout << "Transaction failed";
else
    commit_transaction(total);
end
```

---

## Example: Safe Calculator

```aria
fn safe_calculate(expr: string) -> tbb32 {
    a: tbb32 = parse_number(expr.get_operand(0));  // ERR if invalid
    op: string = expr.get_operator();
    b: tbb32 = parse_number(expr.get_operand(1));  // ERR if invalid
    
    // If either parse failed, this propagates ERR
    Result: tbb32 = when op == "+" then
        a + b
    elsif op == "-" then
        a - b
    elsif op == "*" then
        a * b
    elsif op == "/" then
        when b == 0 then ERR else a / b end
    else
        ERR
    end;
    
    return result;  // ERR if any step failed
}

// Use
Result: tbb32 = safe_calculate("10 + abc");
when result == ERR then
    stdout << "Invalid expression";
else
    stdout << "Result: $result";
end
```

---

## Overflow Detection

```aria
// Overflow becomes ERR
x: tbb32 = 2147483647;  // Max value
y: tbb32 = 1;

sum: tbb32 = x + y;  // ERR (overflow!)

when sum == ERR then
    stdout << "Calculation overflowed";
end
```

---

## Best Practices

### ✅ DO: Check at End

```aria
// Let errors propagate
Result: tbb32 = step1() + step2() + step3();

// Check once
when result != ERR then
    use_result(result);
end
```

### ✅ DO: Document ERR Conditions

```aria
// Returns ERR if file not found or invalid format
fn load_config(path: string) -> tbb32 {
    ...
}
```

### ❌ DON'T: Ignore ERR

```aria
value: tbb32 = might_fail();
process(value);  // ❌ Could be ERR!

// Better
when value != ERR then
    process(value);
end
```

---

## Related

- [TBB Overview](tbb_overview.md)
- [ERR Sentinel](tbb_err_sentinel.md)
- [Result Type](result.md) - Alternative error handling

---

**Remember**: ERR **sticks** - propagates through all operations!
