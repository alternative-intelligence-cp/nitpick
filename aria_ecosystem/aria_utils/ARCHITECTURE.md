# aria_utils Architecture

**Purpose**: Collection of command-line utilities written in Aria.

---

## Philosophy

### Core Principles

1. **Dogfooding** - Demonstrate Aria's capabilities by using it
2. **Cross-Platform** - Works on Linux, macOS, Windows
3. **Performance** - Fast as or faster than system utilities
4. **Correctness** - Robust error handling, edge cases covered
5. **Usability** - Consistent interface, helpful errors

### Non-Negotiables

- ✅ **Pure Aria** - No C/C++ unless absolutely necessary
- ✅ **Cross-Platform** - Must work on all major platforms
- ✅ **Test Coverage** - Every utility has comprehensive tests
- ✅ **Documentation** - Man pages and examples for each tool
- ✅ **No Dependencies** - Only Aria standard library

---

## Component Overview

```
aria_utils/
├── src/
│   ├── cat/           # File concatenation
│   ├── grep/          # Pattern search
│   ├── find/          # File search
│   ├── wc/            # Word count
│   ├── diff/          # File comparison
│   ├── fmt/           # Code formatter
│   ├── lint/          # Code linter
│   └── ...            # Other utilities
├── tests/             # Test suite
├── examples/          # Example usage
└── docs/              # Documentation
```

---

## Utility Breakdown

### File Utilities

#### aria_cat
**Purpose**: Concatenate and display files

**Features**:
- Display multiple files
- Number lines
- Show non-printing characters
- Handle binary files gracefully

**Usage**:
```bash
aria_cat file1.txt file2.txt
aria_cat -n file.txt  # Number lines
```

#### aria_grep
**Purpose**: Search for patterns in files

**Features**:
- Regular expression support
- Recursive directory search
- Line numbers and context
- Case-insensitive search
- Invert match

**Usage**:
```bash
aria_grep "pattern" file.txt
aria_grep -r "pattern" directory/
aria_grep -i "pattern" file.txt  # Case-insensitive
```

#### aria_find
**Purpose**: Find files by criteria

**Features**:
- Name pattern matching
- Type filtering (file, directory, link)
- Size filtering
- Date filtering
- Execute commands on results

**Usage**:
```bash
aria_find . -name "*.aria"
aria_find /path -type f -size +1M
```

#### aria_wc
**Purpose**: Count lines, words, bytes

**Features**:
- Line count
- Word count
- Byte/character count
- Multiple file support

**Usage**:
```bash
aria_wc file.txt
aria_wc -l file.txt  # Line count only
```

#### aria_diff
**Purpose**: Compare files

**Features**:
- Unified diff format
- Context diff format
- Side-by-side display
- Recursive directory comparison

**Usage**:
```bash
aria_diff file1.txt file2.txt
aria_diff -u old/ new/  # Unified format
```

---

### Text Processing Utilities

#### aria_sed
**Purpose**: Stream editor

**Features**:
- Search and replace
- Line deletion
- Line insertion
- Regular expressions
- In-place editing

**Usage**:
```bash
aria_sed 's/old/new/g' file.txt
aria_sed -i 's/old/new/g' file.txt  # In-place
```

#### aria_awk
**Purpose**: Pattern scanning and processing

**Features**:
- Field extraction
- Pattern matching
- Arithmetic operations
- Custom separators

**Usage**:
```bash
aria_awk '{print $1}' file.txt
aria_awk -F: '{print $1, $3}' /etc/passwd
```

#### aria_cut
**Purpose**: Extract sections from lines

**Features**:
- Field extraction
- Character ranges
- Byte ranges
- Custom delimiters

**Usage**:
```bash
aria_cut -f1,3 -d: file.txt
aria_cut -c1-10 file.txt
```

#### aria_sort
**Purpose**: Sort lines

**Features**:
- Alphabetic sorting
- Numeric sorting
- Reverse sorting
- Unique lines only
- Custom sort keys

**Usage**:
```bash
aria_sort file.txt
aria_sort -n numbers.txt  # Numeric sort
aria_sort -u file.txt     # Unique only
```

#### aria_uniq
**Purpose**: Filter duplicate lines

**Features**:
- Remove duplicates
- Count occurrences
- Show only duplicates
- Case-insensitive comparison

**Usage**:
```bash
aria_uniq file.txt
aria_uniq -c file.txt  # Count occurrences
```

---

### Development Tools

#### aria_fmt
**Purpose**: Format Aria code

**Features**:
- Consistent indentation
- Line wrapping
- Comment preservation
- Configurable style

**Usage**:
```bash
aria_fmt file.aria
aria_fmt -i file.aria  # In-place
aria_fmt -check file.aria  # Check only
```

**Algorithm**:
```
1. Parse Aria source into AST
2. Apply formatting rules
3. Pretty-print AST to source
4. Preserve comments and whitespace intent
```

#### aria_lint
**Purpose**: Lint Aria code

**Features**:
- Style violations
- Potential bugs
- Code smells
- Customizable rules

**Usage**:
```bash
aria_lint file.aria
aria_lint --strict file.aria
```

**Checks**:
- Unused variables
- Dead code
- Inconsistent naming
- Missing error handling
- Type issues

#### aria_doc
**Purpose**: Generate documentation

**Features**:
- Extract doc comments
- Generate HTML/Markdown
- API reference
- Example extraction

**Usage**:
```bash
aria_doc src/ -o docs/
aria_doc --format markdown src/
```

#### aria_test
**Purpose**: Run tests

**Features**:
- Discover and run tests
- Parallel execution
- Coverage reporting
- Filter by name/tag

**Usage**:
```bash
aria_test
aria_test --filter "test_name"
aria_test --coverage
```

#### aria_bench
**Purpose**: Benchmark code

**Features**:
- Time measurement
- Memory profiling
- Statistical analysis
- Comparison reports

**Usage**:
```bash
aria_bench benchmark.aria
aria_bench --compare baseline.json
```

---

## Common Design Patterns

### Command-Line Parsing

All utilities use consistent CLI pattern:

```aria
import std.cli

fn main(args: []String) -> Result<(), Error> {
    let opts = parse_options(args)?;
    
    if opts.help {
        print_help();
        return Ok(());
    }
    
    let result = execute(opts)?;
    Ok(())
}
```

### Error Handling

Consistent error reporting:

```aria
fn process_file(path: String) -> Result<(), Error> {
    let file = open(path).or_else(|e| {
        eprintln!("Error: Cannot open {}: {}", path, e);
        return Err(e);
    })?;
    
    // Process file...
    
    Ok(())
}
```

### Stream Processing

For large files, use streaming:

```aria
func:process_large_file = void(string:path) {
    File?:file = openFile(path)?;
    string[]?:lines = file.readLines()?;
    
    till(lines.length - 1, 1) {
        string:line = lines[$];
        process_line(line)?;
    }
}
```

---

## Testing Strategy

### Unit Tests
- Individual function testing
- Edge case handling
- Error conditions

### Integration Tests
- Full utility execution
- Various input formats
- Cross-platform behavior

### Performance Tests
- Benchmark against system utilities
- Memory usage
- Scalability

---

## Performance Goals

Each utility should:
- **Startup**: <50ms
- **Small files (<1MB)**: <100ms
- **Large files (>1GB)**: Competitive with system utilities
- **Memory**: O(1) for streaming operations

---

## Future Considerations

### Additional Utilities
- `aria_tar` - Archive utility
- `aria_zip` - Compression
- `aria_http` - HTTP client
- `aria_json` - JSON processor
- `aria_yaml` - YAML processor

### Optimizations
- Parallel processing for large files
- SIMD for pattern matching
- Memory-mapped files for large inputs

---

## Integration Points

### With Aria Ecosystem
- Uses Aria standard library
- Demonstrates language features
- Provides useful tools for Aria development

### With System
- Standard input/output
- Exit codes
- Environment variables
- Signal handling

---

*These utilities showcase Aria's capabilities while providing genuinely useful tools.*
