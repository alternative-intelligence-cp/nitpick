# Aria Standard Library

## Current Status

### Implemented Modules ✅
- **math.aria** - Mathematical functions (sqrt, pow, sin, cos, etc.)
- **string.aria** - String manipulation (concat, find, split, trim, etc.)
- **io.aria** - Six-stream I/O (stdin, stdout, stderr, stddbg, stddati, stddato)

### Specification Complete (Awaiting Generics) 📋
- **collections.aria** - Vec<T>, HashMap<K,V>, HashSet<T>, LinkedList<T>
  - Full API designed and documented
  - Implementation ready for when generic types are parsed
  - Uses proper Aria syntax with wild memory management

## Generics Support

The collections module is written using generic syntax (`func<T>`, `Vec<int32>`, etc.) 
which is specified in aria_specs.txt but not yet implemented in the parser.

Once generics are working, collections.aria will provide:
- Type-safe data structures
- Zero-cost abstractions  
- Memory-safe operations
- Full aria-doc documentation

## Next Modules

After generics are implemented:
1. **time.aria** - Timestamps, durations, timers
2. **error.aria** - Error types, result helpers
3. **fs.aria** - Filesystem operations
4. **process.aria** - Process spawning, environment
5. **crypto.aria** - Hashing, random, encoding
6. **mem.aria** - Allocators, smart pointers
7. **encoding.aria** - JSON, CSV, UTF-8
8. **net.aria** - TCP/UDP sockets
9. **sync.aria** - Threads, locks, channels
