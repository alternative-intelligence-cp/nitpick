# aria-safety

Static safety audit tool for Aria source files. Pattern-match scanner that
surfaces constructs requiring manual review — not a replacement for careful
code reading, but a fast first pass to find the places that need it.

## Build

```
make
```

Or directly:

```
gcc -O2 -Wall -Wextra -std=c99 -o aria-safety aria_safety.c
```

## Usage

```
aria-safety file.aria             scan a single file
aria-safety src/                  scan directory recursively
aria-safety lib/std/ stdlib/      scan multiple paths
```

## Output

```
lib/std/mem.aria:42:  [WILD]     wild allocation — no GC safety; manual lifetime required
lib/std/sync.aria:57: [WEAK_CAS] compare_exchange_weak — spurious failure possible; verify inside retry loop
src/parser.aria:91:   [RAW]      raw() strips Result<T> — caller must handle failure explicitly
```

## Finding Tags

| Tag          | What it flags                                          |
|--------------|--------------------------------------------------------|
| `[WILD]`     | `wild` / `wildx` allocation — manual lifetime, no GC  |
| `[RAW]`      | `raw()` — strips `Result<T>` wrapper                  |
| `[DROP]`     | `drop()` — explicitly discards a `Result<T>`           |
| `[OK]`       | `ok()` — bypasses error check on the `unknown` type   |
| `[WEAK_CAS]` | `compare_exchange_weak*` — must be inside retry loop  |
| `[RELAXED]`  | relaxed atomic op — verify ordering is sufficient     |
| `[FAILSAFE]` | empty or trivial `failsafe` block                     |

## Exit Codes

| Code | Meaning                        |
|------|-------------------------------|
| `0`  | No findings — clean           |
| `1`  | One or more findings present  |
| `2`  | Usage error or no files found |

## v1 Limitations

- `//` comment stripping is naive and does not account for `//` inside string literals
- Brace counting for `failsafe` block triviality can be thrown off by unbalanced
  `{` / `}` inside string literals within the block
- Content placed on the same line as the `failsafe` opening brace (after the `{`)
  is not checked for triviality in multi-line blocks
- String literals are not excluded from pattern matching; a string containing
  `raw(` or `wild` would be flagged

These are all acceptable for a v1 review-aid tool. False positives require a
quick human glance; false negatives for the edge cases above are rare in practice.
