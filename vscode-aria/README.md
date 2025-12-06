# Aria Language Support for VS Code

Syntax highlighting and language support for the [Aria programming language](https://github.com/alternative-intelligence-cp/aria).

## Features

- **Syntax Highlighting**: Full syntax highlighting for Aria source files (`.aria`)
- **Auto-closing**: Automatic closing of brackets, quotes, and parentheses
- **Comment Support**: Line (`//`) and block (`/* */`) comments
- **Bracket Matching**: Matching and folding for code blocks

## Supported Syntax

- Keywords: `if`, `else`, `while`, `for`, `return`, `break`, `continue`, `defer`, `fall`, `pick`, `case`, `default`, `label`, `goto`, `func`, `wild`, `const`, `auto`, `mod`, `pub`, `use`, `as`, `result`
- Types: `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64`, `float32`, `float64`, `bool`, `void`, `string`
- Built-in functions: `print`, `aria.alloc`, `aria.free`, `aria.realloc`, `aria.strlen`, `aria.strcmp`, `aria.strcpy`, `aria.strcat`
- String interpolation with backticks: `` `Hello ${name}` ``
- Numeric literals: decimal, hex (`0x`), binary (`0b`), octal (`0o`), floats
- Comments: `//` line comments and `/* */` block comments

## Installation

### From Source

1. Clone the Aria repository
2. Navigate to `vscode-aria/`
3. Run `code --install-extension .` (or copy to `~/.vscode/extensions/`)

### Manual Installation

Copy the `vscode-aria` folder to:
- **Linux/macOS**: `~/.vscode/extensions/aria-language-0.0.1/`
- **Windows**: `%USERPROFILE%\.vscode\extensions\aria-language-0.0.1\`

Then reload VS Code.

## About Aria

Aria is a systems programming language designed for clarity, safety, and performance. It features:

- Clean syntax with type annotations
- Pattern matching with `pick`/`fall`
- Automatic `Result` type wrapping for error handling
- Defer statements for resource cleanup
- String interpolation with backticks
- Recursive function support

Learn more at [github.com/alternative-intelligence-cp/aria](https://github.com/alternative-intelligence-cp/aria)

## License

AGPL-3.0 (see [LICENSE.md](../LICENSE.md))

## Contributing

See [CONTRIBUTING.md](../CONTRIBUTING.md) for contribution guidelines.
