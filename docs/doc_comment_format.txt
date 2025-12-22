# Aria Documentation Comment Format

## Overview

Aria documentation comments follow a syntax similar to Rust's doc comments and JavaDoc, using `///` for single-line comments and `/** ... */` for multi-line blocks. The `aria-doc` tool extracts these comments and generates HTML documentation.

## Doc Comment Syntax

### Single-Line Comments

```aria
/// This is a documentation comment for the following item.
/// Multiple lines can be used by repeating ///.
pub fn example() -> i32 {
    return 42;
}
```

### Multi-Line Comments

```aria
/**
 * This is a multi-line documentation comment.
 * 
 * It can span multiple lines and include formatting.
 * 
 * @param x The input value
 * @return The computed result
 */
pub fn compute(x: i32) -> i32 {
    return x * 2;
}
```

### Inline Documentation

Inner doc comments (for module/file documentation):

```aria
//! This module provides mathematical utilities.
//! 
//! It includes functions for common operations.

/// Compute absolute value
pub fn abs(x: i32) -> i32 {
    if x < 0 { -x } else { x }
}
```

## Markdown Support

Doc comments support GitHub Flavored Markdown:

### Headers

```aria
/// # Primary Header
/// ## Secondary Header
/// ### Tertiary Header
```

### Code Blocks

```aria
/// Example usage:
/// ```aria
/// let result = compute(42);
/// assert(result == 84);
/// ```
```

### Lists

```aria
/// Features:
/// - Fast computation
/// - Memory efficient
/// - Thread safe
```

### Links

```aria
/// See also: [max](max) and [min](min)
/// External link: [Aria Documentation](https://aria-lang.org/docs)
```

### Tables

```aria
/// | Input | Output |
/// |-------|--------|
/// | 0     | 0      |
/// | 1     | 2      |
/// | 2     | 4      |
```

### Emphasis

```aria
/// This is *italic* and this is **bold**.
/// This is `inline code`.
```

## Special Annotations

### @param / @parameter

Document function parameters:

```aria
/**
 * Calculate the sum of two numbers.
 * 
 * @param a The first number
 * @param b The second number
 * @return The sum of a and b
 */
pub fn add(a: i32, b: i32) -> i32 {
    return a + b;
}
```

### @return / @returns

Document return values:

```aria
/// Compute factorial
/// @param n The input number (must be >= 0)
/// @return The factorial of n
/// @panics If n is negative
pub fn factorial(n: i32) -> i32 {
    // ...
}
```

### @throws / @panics

Document error conditions:

```aria
/// Open a file for reading
/// @param path The file path
/// @return A file handle
/// @throws IOError if file doesn't exist
/// @panics If path contains null bytes
pub fn open_file(path: string) -> Result<File, IOError> {
    // ...
}
```

### @example / @examples

Provide usage examples:

```aria
/**
 * Binary search in a sorted array
 * 
 * @param arr Sorted array to search
 * @param target Value to find
 * @return Index if found, -1 otherwise
 * 
 * @example
 * ```aria
 * let arr = [1, 3, 5, 7, 9];
 * let idx = binary_search(arr, 5);
 * assert(idx == 2);
 * ```
 */
pub fn binary_search(arr: [i32], target: i32) -> i32 {
    // ...
}
```

### @deprecated

Mark items as deprecated:

```aria
/**
 * Old implementation - use compute_v2 instead
 * 
 * @deprecated Use compute_v2() for better performance
 */
pub fn compute_v1(x: i32) -> i32 {
    // ...
}
```

### @see / @seealso

Cross-reference related items:

```aria
/// Calculate minimum of two values
/// @see max
/// @seealso clamp
pub fn min(a: i32, b: i32) -> i32 {
    // ...
}
```

### @since

Document when an item was added:

```aria
/// New async file operations
/// @since 1.2.0
pub async fn read_file_async(path: string) -> Result<string, IOError> {
    // ...
}
```

### @note / @warning

Special notices:

```aria
/**
 * High-performance sort (unstable)
 * 
 * @param arr Array to sort
 * @note This is an unstable sort - equal elements may be reordered
 * @warning Not thread-safe - use sync primitives if needed
 */
pub fn quick_sort(arr: &mut [i32]) {
    // ...
}
```

## Documentation Sections

### Module Documentation

```aria
//! # Math Utilities
//!
//! This module provides common mathematical functions.
//!
//! ## Features
//! - Integer arithmetic
//! - Floating point operations
//! - Statistical functions
//!
//! ## Example
//! ```aria
//! import math;
//! 
//! let result = math.abs(-42);
//! ```

pub fn abs(x: i32) -> i32 { /* ... */ }
```

### Struct Documentation

```aria
/**
 * A 2D point with x and y coordinates.
 * 
 * Supports basic vector operations.
 * 
 * @example
 * ```aria
 * let p1 = Point { x: 3, y: 4 };
 * let p2 = Point { x: 1, y: 2 };
 * let distance = p1.distance_to(p2);
 * ```
 */
pub struct Point {
    /// X coordinate
    pub x: f64,
    
    /// Y coordinate
    pub y: f64,
}

impl Point {
    /// Calculate distance to another point
    /// @param other The other point
    /// @return Euclidean distance
    pub fn distance_to(self, other: Point) -> f64 {
        // ...
    }
}
```

### Enum Documentation

```aria
/**
 * HTTP request methods
 * 
 * Represents standard HTTP verbs.
 */
pub enum HttpMethod {
    /// GET request - retrieve resource
    GET,
    
    /// POST request - create resource
    POST,
    
    /// PUT request - update resource
    PUT,
    
    /// DELETE request - remove resource
    DELETE,
}
```

### Trait Documentation

```aria
/**
 * Trait for types that can be displayed as strings.
 * 
 * Implement this trait to provide custom string formatting.
 * 
 * @example
 * ```aria
 * impl Display for Point {
 *     fn display(self) -> string {
 *         return "&{self.x}, &{self.y}";
 *     }
 * }
 * ```
 */
pub trait Display {
    /// Convert to string representation
    /// @return String representation of the value
    fn display(self) -> string;
}
```

### Generic Type Documentation

```aria
/**
 * A generic container holding a single value.
 * 
 * @type T The type of the contained value
 * 
 * @example
 * ```aria
 * let container = Box<i32>::new(42);
 * let value = container.get();
 * ```
 */
pub struct Box<T> {
    value: T,
}

impl<T> Box<T> {
    /// Create a new box containing a value
    /// @param value The value to store
    /// @return A new Box<T>
    pub fn new(value: T) -> Box<T> {
        // ...
    }
}
```

## HTML Output Format

### Generated Structure

```
docs/
├── index.html              # Module index
├── module_name/
│   ├── index.html          # Module overview
│   ├── struct.StructName.html
│   ├── enum.EnumName.html
│   ├── trait.TraitName.html
│   ├── fn.function_name.html
│   └── ...
├── search.html             # Search interface
├── static/
│   ├── style.css           # Styling
│   ├── script.js           # Interactive features
│   └── search-index.js     # Search data
└── src/
    └── module_name.aria.html  # Syntax-highlighted source
```

### HTML Features

- **Responsive design** - Works on desktop, tablet, mobile
- **Search** - Full-text search across all documentation
- **Syntax highlighting** - Code examples with proper highlighting
- **Cross-references** - Clickable links between items
- **Source links** - Jump to source code
- **Collapsible sections** - Toggle visibility of long sections
- **Dark mode** - Toggle light/dark themes
- **Type information** - Hoverable type tooltips

## Command-Line Interface

### Basic Usage

```bash
# Generate docs for current package
aria-doc

# Generate docs for specific files
aria-doc src/math.aria src/stats.aria

# Specify output directory
aria-doc --output docs/

# Include private items
aria-doc --document-private-items

# Generate docs for dependencies
aria-doc --include-dependencies
```

### Options

```
aria-doc [OPTIONS] [FILES...]

OPTIONS:
    -o, --output <DIR>               Output directory (default: ./docs)
    --document-private-items         Include private items in documentation
    --include-dependencies           Generate docs for dependencies
    --no-deps                        Don't document dependencies
    --theme <light|dark|both>        Theme to generate (default: both)
    --markdown-css <FILE>            Custom CSS for markdown rendering
    --html-in-header <FILE>          HTML to inject in <head>
    --html-before-content <FILE>     HTML to inject before content
    --html-after-content <FILE>      HTML to inject after content
    --search-index                   Generate search index (default: true)
    --source-linking                 Link to source code (default: true)
    -v, --verbose                    Verbose output
    -h, --help                       Show help message
```

### Configuration File

`aria-doc.toml`:

```toml
[documentation]
output_dir = "docs"
document_private_items = false
include_dependencies = true
theme = "both"

[html]
favicon = "assets/favicon.ico"
logo = "assets/logo.png"
custom_css = "assets/custom.css"

[markdown]
syntax_highlighting = true
auto_heading_links = true
```

## Best Practices

### Documentation Guidelines

1. **Every public item should have docs**
   - Functions, structs, enums, traits, modules
   - Document all parameters and return values
   - Include at least one example

2. **Start with a summary**
   - First line should be a brief summary (one sentence)
   - Followed by detailed explanation
   - Then examples and special notes

3. **Use concrete examples**
   - Show real usage, not just syntax
   - Include expected output
   - Cover common use cases

4. **Document edge cases**
   - Explain error conditions
   - Document panics/exceptions
   - Mention performance characteristics

5. **Keep it up-to-date**
   - Update docs when code changes
   - Remove deprecated examples
   - Fix broken links

### Style Conventions

- Use present tense ("Returns the value", not "Will return")
- Start with a verb ("Calculate", "Create", "Parse")
- Be concise but complete
- Use code formatting for identifiers: `variable_name`
- Link to related items when relevant

### Example: Well-Documented Function

```aria
/**
 * Performs binary search on a sorted array.
 * 
 * This function uses the binary search algorithm to find the index
 * of a target value in a sorted array. The array must be sorted in
 * ascending order for this function to work correctly.
 * 
 * Time complexity: O(log n)
 * Space complexity: O(1)
 * 
 * @param arr The sorted array to search in
 * @param target The value to search for
 * @return The index of the target if found, -1 otherwise
 * 
 * @example
 * ```aria
 * let numbers = [1, 3, 5, 7, 9, 11];
 * let index = binary_search(numbers, 7);
 * assert(index == 3);
 * 
 * let not_found = binary_search(numbers, 4);
 * assert(not_found == -1);
 * ```
 * 
 * @note The array must be sorted in ascending order
 * @see linear_search for unsorted arrays
 * @since 1.0.0
 */
pub fn binary_search(arr: [i32], target: i32) -> i32 {
    let mut left = 0;
    let mut right = arr.len() - 1;
    
    while left <= right {
        let mid = left + (right - left) / 2;
        
        if arr[mid] == target {
            return mid;
        } else if arr[mid] < target {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }
    
    return -1;
}
```

## Testing Documentation

### Doc Tests

Code in `@example` blocks can be automatically tested:

```bash
aria-doc --test
```

This extracts and runs all code examples as tests.

### Coverage

Check documentation coverage:

```bash
aria-doc --check-coverage
```

Reports percentage of public items with documentation.

---

This specification provides a complete foundation for the Aria documentation system, following industry best practices while maintaining the language's unique style.
