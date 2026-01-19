# aria_shell Architecture

**Purpose**: Interactive REPL and scripting shell for Aria language.

---

## Philosophy

### Core Principles

1. **Interactive Development** - Immediate feedback for exploration
2. **Scripting Power** - Full language features in scripts
3. **Ergonomic Design** - Comfortable for extended use
4. **Integration** - Works seamlessly with Aria ecosystem
5. **Reliability** - Errors don't crash the shell

### Non-Negotiables

- ✅ **Full Language Support** - All Aria features available in REPL
- ✅ **Proper Error Handling** - Errors display but don't crash shell
- ✅ **Performance** - REPL responses <100ms for typical input
- ✅ **Spec Compliance** - Same semantics as compiled Aria
- ✅ **No Surprises** - REPL behavior matches compiled behavior

---

## Component Overview

```
aria_shell/
├── src/
│   ├── repl/          # REPL implementation
│   ├── script/        # Script execution engine
│   ├── completion/    # Tab completion
│   ├── history/       # Command history
│   └── utils/         # Utilities
├── inc/               # Header files
├── tests/             # Test suite
├── examples/          # Example scripts
└── tools/             # Development tools
```

---

## Component Breakdown

### 1. REPL Core (src/repl/)

**Purpose**: Interactive read-eval-print loop.

**Key Files**:
- `repl.aria` - Main REPL loop
- `eval.aria` - Expression evaluation
- `context.aria` - REPL context management

**Responsibilities**:
- Read user input
- Parse and evaluate expressions
- Display results
- Maintain state between commands
- Handle errors gracefully

**Execution Flow**:
```
User Input → Parse → Evaluate → Print Result → Repeat
```

### 2. Script Engine (src/script/)

**Purpose**: Execute Aria scripts from files.

**Key Files**:
- `script.aria` - Script execution
- `args.aria` - Command-line argument parsing
- `env.aria` - Environment variable access

**Responsibilities**:
- Load script from file
- Parse command-line arguments
- Execute script in isolated context
- Report errors with stack traces
- Exit with appropriate status code

### 3. Tab Completion (src/completion/)

**Purpose**: Provide intelligent tab completion.

**Key Files**:
- `complete.aria` - Completion engine
- `symbol.aria` - Symbol table for completion

**Responsibilities**:
- Complete variable names
- Complete function names
- Complete module paths
- Complete file paths
- Context-aware suggestions

**Algorithm**:
```
User types partial input + TAB
→ Parse partial input
→ Determine completion context
→ Find matching symbols
→ Display suggestions
```

### 4. Command History (src/history/)

**Purpose**: Track and recall previous commands.

**Key Files**:
- `history.aria` - History management
- `search.aria` - History search

**Responsibilities**:
- Store command history
- Persist history to disk
- Search history
- Recall previous commands (↑/↓)
- Prevent duplicate consecutive entries

**Storage Format**:
```
~/.aria_history
timestamp|command
timestamp|command
...
```

### 5. Utilities (src/utils/)

**Purpose**: Common utilities for shell operations.

**Key Files**:
- `io.aria` - I/O helpers
- `format.aria` - Output formatting
- `error.aria` - Error display

**Responsibilities**:
- Pretty-print values
- Format error messages
- Handle terminal control
- Colorize output (optional)

---

## Data Flow

### REPL Interaction Flow

```
User types: let x = 1 + 2

1. Read input from terminal
   ↓
2. Add to history
   ↓
3. Parse input into AST
   ↓
4. Type check expression
   ↓
5. Evaluate in REPL context
   ↓
6. Store result (x = 3)
   ↓
7. Format output ("3")
   ↓
8. Display to user
   ↓
9. Wait for next input
```

### Script Execution Flow

```
User runs: aria_shell script.aria arg1 arg2

1. Parse command-line arguments
   ↓
2. Load script file
   ↓
3. Parse entire script into AST
   ↓
4. Type check script
   ↓
5. Create execution context
   ↓
6. Inject arguments as variables
   ↓
7. Execute script top-to-bottom
   ↓
8. Catch and report errors
   ↓
9. Exit with status code
```

---

## Key Algorithms

### REPL Context Management

**Challenge**: Maintain state across multiple inputs

```
Algorithm: Context Update
1. Parse input
2. If input is declaration:
   - Add to symbol table
   - Store value
3. If input is expression:
   - Evaluate in current context
   - Display result
4. If input is statement:
   - Execute statement
   - Update context if needed
```

### Error Recovery

**Challenge**: Errors shouldn't crash the shell

```
Algorithm: Error Handling
1. Wrap evaluation in error handler
2. On error:
   - Display error message with context
   - Show stack trace (if available)
   - Reset parser state
   - Continue REPL loop
3. On success:
   - Display result
   - Continue REPL loop
```

### Tab Completion

**Challenge**: Provide relevant completions fast

```
Algorithm: Completion
1. Get cursor position and input
2. Extract partial symbol before cursor
3. Determine context (variable, function, module, etc.)
4. Query symbol table for matches
5. Filter by prefix
6. Return top N matches (<100ms)
```

---

## REPL Special Commands

### Inspection Commands
- `:type <expr>` - Show type of expression
- `:info <symbol>` - Show information about symbol
- `:doc <symbol>` - Show documentation
- `:list` - List all variables in scope

### File Commands
- `:load <file>` - Load file into REPL context
- `:save <file>` - Save REPL session to file
- `:edit <symbol>` - Edit definition (opens $EDITOR)

### Control Commands
- `:quit` - Exit shell
- `:help` - Show help
- `:clear` - Clear screen
- `:reset` - Reset REPL context

---

## Testing Strategy

### Unit Tests
- REPL context management
- Expression evaluation
- Error handling and recovery
- Command history
- Tab completion logic

### Integration Tests
- Full REPL sessions
- Script execution with various inputs
- Error scenarios
- History persistence
- Completion accuracy

### Performance Tests
- REPL response time
- Script execution overhead
- Completion latency
- Memory usage over long sessions

---

## Performance Goals

### REPL Performance
- **Response time**: <100ms for typical expressions
- **Startup time**: <50ms
- **Memory**: <50MB for typical session

### Script Performance
- **Overhead**: <10% compared to compiled Aria
- **Startup**: <100ms for small scripts

---

## Future Considerations

### Planned Features
- Debugger integration (breakpoints, step execution)
- Profiler integration
- Hot-reload for imported modules
- Remote REPL (connect to running process)

### Potential Optimizations
- JIT compilation for frequently-executed expressions
- Cached parsing for repeated commands
- Incremental type checking

---

## Integration Points

### With ariac
- Uses compiler for parsing and type checking
- Shares AST representation
- Uses same semantic analysis

### With aria_make
- Can execute build scripts
- Provides scripting for build automation

### With aria_utils
- Shares common utilities
- Uses same I/O libraries

### With Aria Standard Library
- Provides shell-specific utilities
- File I/O, process management, etc.

---

*This architecture prioritizes user experience and reliability - the shell should never crash.*
