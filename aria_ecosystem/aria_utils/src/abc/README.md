# abc - Aria Build Configuration Parser

High-performance, deterministic parser for Aria Build Configuration (ABC) files.

## Purpose

Provides configuration file parsing for:
- **aria_make** - Build system configuration (build.aria files)
- **ariac** - Compiler project files
- **Future IDE integration** - LSP support for build files

## Features

- **Aria-style Syntax**: Unquoted keys, trailing commas, `//` comments
- **Variable Interpolation**: `&{variable}` patterns with cycle detection
- **Environment Variables**: `&{ENV.HOME}` access
- **Glob Integration**: Source pattern expansion via aglob
- **Error Recovery**: Multiple errors reported in single pass
- **IDE Support**: Precise line/column tracking for diagnostics

## Quick Start

```cpp
#include "abc/parser.hpp"
#include "abc/interpolator.hpp"

using namespace aria::abc;

int main() {
    auto result = parse_abc(R"({
        variables: {
            src: "src",
            main: "&{src}/main.aria"
        },
        project: { name: "myapp" },
        targets: [
            {
                name: "app",
                type: "binary",
                sources: ["&{main}"]
            }
        ]
    })");

    if (result.success()) {
        Interpolator interp;
        interp.interpolate_ast(*result.ast);

        std::cout << "Project: " << result.ast->project_name() << "\n";
    }

    return 0;
}
```

## ABC Format Syntax

```
{
    // Project metadata
    project: {
        name: "myapp",
        version: "1.0.0",
    },

    // Variables for interpolation
    variables: {
        src: "src",
        build: "build",
        flags: "-O2 -Wall",
    },

    // Build targets
    targets: [
        {
            name: "main",
            type: "binary",
            sources: ["&{src}/**/*.aria"],
            output: "&{build}/myapp",
            flags: ["&{flags}"],
            depends_on: ["core"],
        },
    ],
}
```

### Key Features

| Feature | Example | Description |
|---------|---------|-------------|
| Unquoted keys | `name: "value"` | Keys don't need quotes |
| Trailing commas | `{ a: 1, b: 2, }` | Commas after last element OK |
| Comments | `// comment` | Line comments |
| Interpolation | `"&{var}"` | Variable substitution |
| Environment | `"&{ENV.HOME}"` | Environment variable access |
| Nested vars | `"&{a}/&{b}"` | Multiple patterns in one string |

## Building

```bash
mkdir build && cd build
cmake ..
make

# Run tests
make test
./test_abc
```

## Architecture

```
aria::abc::Lexer         - Tokenizes ABC source
aria::abc::Parser        - Recursive descent parser
aria::abc::Interpolator  - Variable resolution with cycle detection
aria::abc::SourceResolver - Glob pattern expansion (integrates with aglob)
```

## API Reference

### Parsing

```cpp
// Parse from string
ParseResult parse_abc(std::string_view source);

// Parse from file
ParseResult parse_abc_file(const std::string& filename);
```

### Variable Interpolation

```cpp
Interpolator interp;

// Set global variables
interp.set_variable("src", "source");

// Or from parsed AST
interp.set_global_variables(*ast.variables);

// Interpolate a single string
auto result = interp.interpolate("&{src}/main.aria");

// Interpolate entire AST in-place
auto errors = interp.interpolate_ast(*ast);
```

### Source Resolution

```cpp
SourceResolver resolver("/project/root");

// Single pattern
auto result = resolver.resolve("src/**/*.aria");

// Multiple patterns with exclusions
auto result = resolver.resolve_all({
    "src/**/*.aria",
    "!src/test/**"
});
```

## Error Handling

All operations return structured results with errors:

```cpp
auto result = parse_abc(source);

if (result.has_errors()) {
    for (const auto& err : result.errors) {
        std::cerr << err.to_string() << "\n";
    }
}
```

Error messages include source location context:

```
Error at 10:15: Expected ':' after key
10 | sources ["main.aria"]
   |         ^
```

## License

Part of the Aria Language Project. See LICENSE.md in repository root.
