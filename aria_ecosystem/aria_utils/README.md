# Aria System Utilities

**Hex-Stream Native System Tools for the Aria Ecosystem**

This repository contains reimagined versions of classic Unix/Linux system utilities, redesigned from the ground up to leverage Aria's revolutionary 6-stream I/O topology. These tools demonstrate the power of separating User Interface, Data Payload, and Telemetry into dedicated channels.

## The Hex-Stream Model

Unlike traditional 3-stream Unix tools (stdin/stdout/stderr), Aria tools utilize six streams:

| Stream | FD | Purpose | Content |
|--------|----|---------| ---------|
| `stdin` | 0 | Control Input | User commands, interactive input |
| `stdout` | 1 | User Interface | Human-readable output, colors, formatting |
| `stderr` | 2 | Critical Errors | Fatal failures only |
| `stddbg` | 3 | Telemetry/Debug | Structured logs, performance metrics |
| `stddati` | 4 | Data Input | Binary structured data streams |
| `stddato` | 5 | Data Output | Binary structured data streams |

## Why This Matters

**The Problem**: Traditional tools force you to choose between human-readable output and machine-parseable data. They mix binary payloads with progress bars, corrupt pipes with color codes, and make parsing output fragile.

**The Solution**: Aria tools provide rich UIs on `stdout` while streaming structured binary data on `stddato`, with comprehensive telemetry on `stddbg`. No parsing. No fragility. Pure composability.

## Implemented Utilities

### Priority 1: Pipeline Fundamentals
- **aps** - Process Status (replaces `ps`/`top`)
- **als** - File Listing (replaces `ls`/`find`)
- **acurl** - Network Transfer (replaces `curl`/`wget`)

### Priority 2: Data Transformation
- **agrep** - Pattern Search (replaces `grep`)
- **asql** - SQL Client (replaces `psql`/`mysql`)
- **affmpeg** - Media Converter (replaces `ffmpeg`)

### System Administration
- **astat** - File Statistics (replaces `stat`)
- **atar** - Archive Manager (replaces `tar`)
- **ajq** - JSON Processor (replaces `jq`)
- **anetstat** - Network Statistics (replaces `netstat`/`ss`)
- **adf** - Disk Usage (replaces `df`)
- **asystemctl** - Service Manager (replaces `systemctl`)

### Developer Tools
- **agit** - Git Operations (wraps `git` with hex-streams)
- **amake** - Build System (replaces `make`)
- **atest** - Test Runner
- **ascope** - Data Visualizer

### Advanced Utilities
- **aparallel** - Parallel Execution (replaces GNU `parallel`)
- **awatch** - File System Watcher (replaces `inotifywait`)

## Example Usage

```bash
# Traditional way (fragile):
ps aux | grep python | awk '{print $2}' | xargs kill

# Aria way (type-safe):
aps | filter --name python | pluck --field pid | xargs kill
```

```bash
# Traditional way (breaks with spaces in filenames):
find . -name "*.jpg" -exec du -h {} \;

# Aria way (always correct):
als -R | filter --ext jpg | map --field size
```

```bash
# Traditional way (mixed output):
curl https://example.com/file.zip > output.zip
# (progress bar corrupts the file OR you use --silent and see nothing)

# Aria way (clean separation):
acurl https://example.com/file.zip
# See progress on screen, file goes to stddato, clean as a whistle
```

## Architecture

Each utility follows the hex-stream pattern:
- **stdout**: Beautiful, colored, formatted for humans
- **stddato**: Typed binary structs (Process, FileEntry, Match, etc.)
- **stddbg**: JSON-formatted telemetry and diagnostics

This enables:
- **Pipeline Safety**: Binary data never corrupts with text
- **Type Safety**: Downstream tools receive actual data structures
- **Observability**: Rich telemetry without cluttering the UI
- **Composability**: Tools chain naturally without parsing

## Development Status

🚧 **Active Development** - Research and specification phase complete. Implementation in progress.

See `/docs/research/gemini/responses/` for detailed architectural specifications of each utility.

## Building

Requirements:
- Aria compiler (`ariac`) - see [aria](https://github.com/yourusername/aria)
- Aria standard library
- LLVM 20+

```bash
aria_make build --release
```

## Contributing

This is part of the larger Aria ecosystem research project. See the main [Aria repository](https://github.com/yourusername/aria) for contribution guidelines.

## License

[To be determined]

---

**Part of the Alternative Intelligence Liberation Platform (AILP)**
