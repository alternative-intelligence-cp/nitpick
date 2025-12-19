# Tree-sitter Grammar for Aria

Tree-sitter parser for the Aria programming language, providing structural syntax trees for modern editors.

## Features

- **Full Aria syntax support**: TBB types, hybrid memory model, string interpolation, pattern matching
- **Incremental parsing**: Efficient re-parsing on edits
- **Error recovery**: Partial parse trees even with syntax errors
- **Query system**: Syntax highlighting, code navigation, textobjects
- **Editor integration**: Neovim, Helix, Emacs, and more

## Installation

### Neovim (nvim-treesitter)

1. Install the parser:

```bash
cd ~/.local/share/nvim/site/pack/tree-sitter/start/
git clone https://github.com/ailp/tree-sitter-aria
cd tree-sitter-aria
npm install
npm run generate
```

2. Add to your Neovim config:

```lua
-- ~/.config/nvim/init.lua or ~/.config/nvim/lua/treesitter.lua
require'nvim-treesitter.configs'.setup {
  ensure_installed = { "aria" },
  highlight = {
    enable = true,
  },
  incremental_selection = {
    enable = true,
  },
  textobjects = {
    select = {
      enable = true,
      lookahead = true,
      keymaps = {
        ["af"] = "@function.outer",
        ["if"] = "@function.inner",
        ["ac"] = "@class.outer",
        ["ic"] = "@class.inner",
      },
    },
  },
}
```

3. Register the parser:

```lua
local parser_config = require("nvim-treesitter.parsers").get_parser_configs()
parser_config.aria = {
  install_info = {
    url = "~/.local/share/nvim/site/pack/tree-sitter/start/tree-sitter-aria",
    files = {"src/parser.c"},
  },
  filetype = "aria",
}
```

### Helix Editor

1. Build the grammar:

```bash
cd editors/tree-sitter-aria
tree-sitter generate
tree-sitter build -o libtree-sitter-aria.so
```

2. Copy files to Helix runtime:

```bash
# Linux/macOS
mkdir -p ~/.config/helix/runtime/grammars/sources/aria
cp -r . ~/.config/helix/runtime/grammars/sources/aria/
cp libtree-sitter-aria.so ~/.config/helix/runtime/grammars/
cp -r queries ~/.config/helix/runtime/queries/aria/
```

3. Add to `~/.config/helix/languages.toml`:

```toml
[[language]]
name = "aria"
scope = "source.aria"
file-types = ["aria"]
roots = ["aria.toml"]
comment-token = "//"
indent = { tab-width = 2, unit = "  " }

[language-server]
command = "aria-ls"

[[grammar]]
name = "aria"
source = { path = "~/.config/helix/runtime/grammars/sources/aria" }
```

## Development

### Building

```bash
npm install
npm run generate
npm test
```

### Testing

Tree-sitter uses corpus tests in `test/corpus/`:

```bash
tree-sitter test
```

### Parsing Examples

```bash
tree-sitter parse examples/sample.aria
```

## Queries

- **highlights.scm**: Syntax highlighting
- **locals.scm**: Scope-aware features (LSP rename)
- **textobjects.scm**: Smart text selection
- **injections.scm**: Embedded language syntax

## Architecture

The grammar is defined in `grammar.js` following Tree-sitter's DSL:

- **Tokens**: Comments, identifiers, literals
- **Expressions**: Binary ops, calls, field access, etc.
- **Statements**: Functions, structs, variables, control flow
- **Types**: TBB types, memory qualifiers, generics

Precedence levels match Aria's language spec for correct operator binding.

## Contributing

To add new language features:

1. Update `grammar.js` with new rules
2. Add tests to `test/corpus/`
3. Update `queries/*.scm` for highlighting
4. Run `npm run generate && npm test`

## License

MIT

## Links

- [Aria Compiler](https://github.com/ailp/aria)
- [Tree-sitter Documentation](https://tree-sitter.github.io/tree-sitter/)
- [nvim-treesitter](https://github.com/nvim-treesitter/nvim-treesitter)
- [Helix Editor](https://helix-editor.com/)
