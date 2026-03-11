# Result Extraction in Aria

**Category**: Types → Result  
**Purpose**: Extracting values from `result<T>`

---

## Overview

Aria provides four ways to handle a `result<T>` value:

| Operator | Behaviour |
|----------|-----------|
| `? default` | Use `default` if error |
| `?!` | Trigger failsafe if error |
| `?` (inside result-returning func) | Propagate error upward |
| `raw()` | Extract value — TOS-acknowledged, no check performed |

---

## 1. Safe extraction with default (`? default`)

```aria
// Returns value on success, default on error
string:config = readFile("config.txt") ? "default config";

int64:count = getCount() ? 0;

string:theme = readFile("theme.css") ? "/* default theme */";
```

---

## 2. Must-succeed (`?!`)

```aria
// Returns value on success, triggers failsafe on error
Config:cfg = loadConfig()?!;     // failsafe fires if load fails

Key:key = readKeyFile()?!;       // cryptographic key MUST load

string:data = readFile("required.txt")?!;
```

---

## 3. Error propagation (`?`)

```aria
// In a result-returning function, ? propagates error upward
func:loadAndProcess = result<Config>(string:path) {
    string:content = readFile(path)?;        // propagate if read fails
    Config:cfg = parseConfig(content)?;      // propagate if parse fails
    pass(cfg);
};
```

---

## 4. Explicit check then extract (`raw()`)

```aria
result<string>:data = readFile("input.txt");

if (data.is_error) {
    fail(`Cannot read: &{data.err}`);
}

string:content = raw(data);  // safe — we checked above
processContent(content);
```

---

## Checking before extracting

```aria
result<string>:result = readFile("config.txt");

if (!result.is_error) {
    string:content = raw(result);
    print(content);
} else {
    print(`Failed to read file: &{result.err}`);
}
```

---

## Related

- [result<T>](result.md)
- [Result Err/Val](result_err_val.md)

---

**Remember**: `?!` — failsafe on error. `? default` — safe fallback. `raw()` — I checked, extract it. `?` — propagate error.
