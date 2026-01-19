# awc - Aria Word Count

Word count utility with six-stream architecture support.

## Features

- Count lines, words, characters (UTF-8 aware), and bytes
- Multiple file support with totals
- Six-stream telemetry via `--debug`
- Compatible with standard `wc` command

## Usage

```bash
awc [OPTIONS] [FILE]...
```

## Options

- `-l` - Count lines
- `-w` - Count words
- `-m` - Count characters (UTF-8 aware)
- `-c` - Count bytes
- `-L` - Print maximum line length
- `--debug` - Output telemetry to FD 3
- `--help` - Display help

If no options specified, `-l -w -c` are assumed (matching standard `wc`).

## Examples

```bash
# Count lines, words, and bytes (default)
awc file.txt

# Count only lines
awc -l file.txt

# Count words in multiple files
awc -w file1.txt file2.txt file3.txt

# Count with telemetry
awc --debug file.txt 3> telemetry.log
```

## Six-Stream Architecture

- FD 0 (stdin): Read input when no files specified
- FD 1 (stdout): Count results
- FD 2 (stderr): Error messages
- FD 3 (stddbg): Debug telemetry (NDJSON format)
