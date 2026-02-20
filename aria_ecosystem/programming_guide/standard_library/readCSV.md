# readCSV()

**Category**: Standard Library → I/O  
**Syntax**: `readCSV(path: string) -> Result<[][]string>`  
**Purpose**: Read CSV file as 2D array

---

## Overview

`readCSV()` reads a CSV (Comma-Separated Values) file and returns rows as arrays of strings.

---

## Syntax

```aria
import std.csv;

data: Result<[][]string> = readCSV("data.csv");
```

---

## Parameters

- **path** (`string`) - Path to CSV file

---

## Returns

- `Result<[][]string>` - 2D array of strings (rows and columns)

---

## Examples

### Basic Usage

```aria
import std.csv;

// Read CSV
Result: Result<[][]string> = readCSV("users.csv");

when result is Ok(rows) then
    till(rows.length - 1, 1) {
        stdout << "Row: $(rows[$].join(", "))";
    end
elsif result is Err(msg) then
    stderr << "Failed to read CSV: $msg";
end
```

### With Headers

```aria
// users.csv:
// name,age,city
// Alice,30,NYC
// Bob,25,LA

rows: [][]string = readCSV("users.csv")?;

// First row is headers
headers: []string = rows[0];  // ["name", "age", "city"]

// Data rows
till(rows.length() - 1, 1) {
    when $ >= 1 then  // Skip header at index 0
        row: []string = rows[$];
        name: string = row[0];
        age: string = row[1];  // Note: still string, need to parse
        city: string = row[2];
        
        stdout << "$name, age $(age), from $city";
    end
end
```

### Parse to Structs

```aria
struct User {
    name: string,
    age: i32,
    city: string
}

rows: [][]string = readCSV("users.csv")?;

users: []User = [];

// Skip header (row 0)
till(rows.length() - 1, 1) {
    when $ >= 1 then
        row: []string = rows[$];
        
        user: User = {
            name = row[0],
            age = parse_int(row[1])?,
            city = row[2]
        };
        
        users.append(user);
    end
end
```

### Process Large CSV

```aria
// For huge files, use streaming
file: File = openFile("huge.csv", "r")?;
defer file.close();

loop
    line: ?string = file.read_line();
    when line == nil then break; end
    
    fields: []string = line.split(",");
    process_row(fields);
end
```

---

## Error Cases

- File doesn't exist
- Invalid CSV format
- No read permission
- I/O error

---

## Best Practices

### ✅ DO: Parse Data Types

```aria
rows: [][]string = readCSV("numbers.csv")?;

till(rows.length - 1, 1) {
    // CSV values are strings - parse them
    value: i32 = parse_int(rows[$][0])?;
    price: flt64 = parse_float(rows[$][1])?;
end
```

### ✅ DO: Handle Missing Fields

```aria
till(rows.length - 1, 1) {
    row = rows[$];
    when row.length() < 3 then
        stderr << "Incomplete row: $(row.join(","))";
        continue;
    end
    
    // Safe to access row[0], row[1], row[2]
end
```

### ❌ DON'T: Assume All Rows Same Length

```aria
rows: [][]string = readCSV("messy.csv")?;

till(rows.length - 1, 1) {
    // Dangerous - row might not have 3 columns!
    value: string = rows[$][2];  // ❌ May crash
end

// Better
till(rows.length - 1, 1) {
    row = rows[$];
    when row.length() > 2 then
        value: string = row[2];  // ✅ Safe
    end
end
```

---

## Custom Delimiters

```aria
// For tab-separated values (TSV)
rows: [][]string = readCSV("data.tsv", delimiter="\t")?;

// For semicolon-separated
rows: [][]string = readCSV("data.csv", delimiter=";")?;
```

---

## Related

- [readFile()](readFile.md) - Read raw file
- [readJSON()](readJSON.md) - Read JSON files
- [writeCSV()](writeCSV.md) - Write CSV files
- [split()](../string/split.md) - String splitting

---

**Remember**: CSV returns **strings** - parse types manually!
