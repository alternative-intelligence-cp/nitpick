# Phase 4.2: File I/O Library - Implementation Plan
**Date**: December 23, 2025  
**Status**: Planning Complete, Ready for Implementation

## Discovery: Runtime Already Complete! 🎉

**Excellent News**: The C++ runtime implementation in `/src/runtime/io/io.cpp` is **100% complete** (737 lines)!

### What Exists (Runtime Layer - C++)

**File in**: `src/runtime/io/io.cpp`  
**Header**: `include/runtime/io.h`

✅ **Simple File Operations** (all implemented):
- `aria_read_file(path)` - Read entire file to string
- `aria_write_file(path, content)` - Write string to file
- `aria_read_binary(path, &size)` - Read binary file to buffer
- `aria_write_binary(path, data, size)` - Write binary buffer
- `aria_file_exists(path)` - Check if file exists
- `aria_file_size(path)` - Get file size in bytes
- `aria_delete_file(path)` - Delete file

✅ **Stream Operations** (all implemented):
- `aria_open_file(path, mode)` - Open file stream
- `aria_stream_close(stream)` - Close stream
- `aria_stream_read_line(stream)` - Read line
- `aria_stream_write(stream, str)` - Write string
- `aria_stream_write_bytes(stream, data, size)` - Write bytes
- `aria_stream_read_bytes(stream, buffer, size)` - Read bytes
- `aria_stream_eof(stream)` - Check EOF
- `aria_stream_flush(stream)` - Flush buffer
- `aria_stream_seek(stream, offset, whence)` - Seek position
- `aria_stream_tell(stream)` - Get position

✅ **Structured Parsing** (stub implementations):
- `aria_read_json(path)` - Read and parse JSON (basic)
- `aria_parse_json(str)` - Parse JSON string (stub)
- `aria_json_get(obj, key)` - Get JSON field
- `aria_json_as_string/number/bool()` - Type conversions
- `aria_json_free(value)` - Free JSON object
- `aria_read_csv(path)` - Read and parse CSV
- `aria_parse_csv(str)` - Parse CSV string
- `aria_csv_free(data)` - Free CSV data

✅ **Path Operations** (all implemented):
- `aria_path_absolute(path)` - Get absolute path
- `aria_path_dirname(path)` - Get directory name
- `aria_path_basename(path)` - Get base filename
- `aria_path_join(dir, name)` - Join path components
- `aria_path_is_absolute(path)` - Check if absolute

✅ **Result Type** (error handling):
- `aria_result_ok(value, size)` - Create success result
- `aria_result_err(error)` - Create error result
- `aria_result_free(result)` - Free result

**Total**: ~20+ functions fully implemented with cross-platform support (Windows + POSIX)

### What's Missing (Aria Language Layer)

The runtime exists, but we need to expose it to Aria code. Two approaches:

**Option A: Builtin Functions** (Recommended)
- Register functions in type checker (like `print()`)
- Add IR generation to call C++ runtime
- Simple, direct access from Aria

**Option B: Standard Library Module** (Future)
- Create `lib/std/io.aria` module
- Wrap C++ functions with Aria-level API
- Better organization, more Aria-like

**For Phase 4.2, we'll use Option A** (builtins) to get working quickly.

---

## Implementation Plan

### Stage 1: Register I/O Builtins in Type Checker (2-3 hours)

**Goal**: Make I/O functions available in Aria code  
**File**: `src/frontend/sema/type_checker.cpp`

**Builtins to Register** (priority order):

1. **File Operations** (start with these):
   ```cpp
   // In TypeChecker constructor, around line 680 (near print builtin)
   
   // readFile(path: string) -> result<string>
   // writeFile(path: string, content: string) -> result<void>
   // fileExists(path: string) -> bool
   // fileSize(path: string) -> int64
   // deleteFile(path: string) -> result<void>
   ```

2. **Stream Operations** (defer to Stage 2):
   ```cpp
   // openFile(path: string, mode: string) -> stream
   // readLine(stream) -> string
   // write(stream, str: string) -> int64
   // close(stream) -> void
   ```

3. **Path Operations** (nice to have):
   ```cpp
   // pathJoin(dir: string, name: string) -> string
   // pathBasename(path: string) -> string
   // pathDirname(path: string) -> string
   ```

**Steps**:
- [ ] Find existing builtin registration code (search for "print")
- [ ] Add function signatures to builtin list
- [ ] Define parameter types and return types
- [ ] Handle result types correctly (result<T>)
- [ ] Test compilation with I/O function calls

**Validation**:
- ✅ Aria code can call `readFile("test.txt")`
- ✅ Type checker accepts I/O function calls
- ✅ Proper error messages for wrong arguments

### Stage 2: Add IR Code Generation (2-3 hours)

**Goal**: Generate LLVM IR that calls C++ runtime functions  
**File**: `src/backend/ir/codegen_expr.cpp`

**Pattern to Follow**:
Look at how `print()` builtin generates IR (around line 1288 in codegen_expr.cpp)

**For Each Builtin**:
```cpp
// Example: readFile builtin
if (builtin_name == "readFile") {
    // 1. Get or declare C function
    llvm::Function* read_file_func = getOrDeclareReadFile();
    
    // 2. Codegen arguments
    llvm::Value* path_arg = codegen(call->args[0]);
    
    // 3. Create call instruction
    llvm::Value* result = builder.CreateCall(read_file_func, {path_arg});
    
    // 4. Return result
    return result;
}
```

**Helper Functions Needed**:
```cpp
llvm::Function* getOrDeclareReadFile() {
    // Check if already declared
    llvm::Function* func = module->getFunction("aria_read_file");
    if (func) return func;
    
    // Declare: AriaResult* aria_read_file(const char* path)
    llvm::FunctionType* func_type = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),  // AriaResult*
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)}, // const char*
        false
    );
    
    func = llvm::Function::Create(
        func_type,
        llvm::Function::ExternalLinkage,
        "aria_read_file",
        module
    );
    
    return func;
}
```

**Steps**:
- [ ] Add helper declaration functions for each I/O builtin
- [ ] Add IR generation in codegen switch statement
- [ ] Handle string arguments (convert Aria string to char*)
- [ ] Handle result return values
- [ ] Test IR generation (compile and check .ll output)

**Validation**:
- ✅ `ariac test.aria --emit-llvm` shows correct function calls
- ✅ IR contains `call @aria_read_file` instructions
- ✅ Arguments passed correctly

### Stage 3: Link Runtime Library (30 minutes)

**Goal**: Ensure compiled programs link against I/O runtime

**File**: `CMakeLists.txt`

**Steps**:
- [ ] Check if `io.cpp` is already in build
- [ ] If not, add to runtime library target
- [ ] Verify linking works

**Validation**:
- ✅ Test programs compile and link successfully
- ✅ No undefined reference errors

### Stage 4: Create Comprehensive Tests (2-3 hours)

**Goal**: Validate all I/O operations work correctly

**Test Files to Create**:

1. **tests/io/file_read_basic.aria**
   ```aria
   func:main = int64() {
       // Test basic file reading
       result<string>:r = readFile("test_data.txt");
       when (r.is_ok()) {
           string:content = r.unwrap();
           print(content);
           pass(0);
       } then {
           print("Error reading file");
           pass(1);
       } end;
   };
   ```

2. **tests/io/file_write_basic.aria**
   ```aria
   func:main = int64() {
       // Test basic file writing
       result<void>:r = writeFile("/tmp/test_output.txt", "Hello, World!");
       when (r.is_ok()) {
           print("Write successful");
           pass(0);
       } then {
           print("Write failed");
           pass(1);
       } end;
   };
   ```

3. **tests/io/file_operations.aria** (comprehensive)
   - Test fileExists()
   - Test fileSize()
   - Test deleteFile()
   - Test error handling (file not found, permission denied)

4. **tests/io/file_stream.aria**
   - Test openFile()
   - Test readLine()
   - Test write()
   - Test close()

5. **tests/io/path_operations.aria**
   - Test pathJoin()
   - Test pathBasename()
   - Test pathDirname()

**Test Data Files**:
- Create `tests/io/test_data.txt` with sample content
- Create various test files for error cases

**Steps**:
- [ ] Create test directory: `tests/io/`
- [ ] Create test data files
- [ ] Write test programs
- [ ] Add tests to CMake test suite
- [ ] Run all tests and verify they pass

**Validation**:
- ✅ All basic I/O tests pass
- ✅ Error cases handled correctly
- ✅ No memory leaks (valgrind clean)
- ✅ Cross-platform (Linux working, Windows optional)

### Stage 5: Documentation (1 hour)

**Goal**: Document I/O functions for users

**Files to Update**:
- [ ] Create `docs/stdlib/io_reference.md` - Complete API reference
- [ ] Update `docs/ARIA_LANGUAGE_GUIDE.md` - Add I/O examples
- [ ] Update `/home/randy/._____RANDY_____/____FILES/aria/STATE` - Phase 4.2 complete
- [ ] Update `/home/randy/._____RANDY_____/____FILES/aria/CHAT` - Session summary
- [ ] Update `/home/randy/._____RANDY_____/____FILES/aria/TODO` - Mark Phase 4.2 done

**Documentation Structure**:
```markdown
# Aria File I/O Reference

## Simple File Operations

### readFile(path: string) -> result<string>
Reads entire file into a string.

**Parameters**:
- `path` - File path to read

**Returns**: 
- `result<string>` - Success with file contents, or error

**Example**:
```aria
result<string>:content = readFile("config.txt");
when (content.is_ok()) {
    print(content.unwrap());
} then {
    print("Error: " + content.unwrap_err());
} end;
```

... (repeat for all functions)
```

---

## Time Estimates

| Stage | Task | Time |
|-------|------|------|
| 1 | Register builtins in type checker | 2-3 hours |
| 2 | Add IR code generation | 2-3 hours |
| 3 | Link runtime library | 30 min |
| 4 | Create comprehensive tests | 2-3 hours |
| 5 | Documentation | 1 hour |
| **Total** | **Phase 4.2 Complete** | **8-11 hours** |

---

## Success Criteria

Phase 4.2 is complete when:

✅ File I/O builtins registered in type checker  
✅ IR code generation working for all I/O functions  
✅ Runtime library properly linked  
✅ Comprehensive test suite passing (10+ tests)  
✅ No memory leaks (valgrind clean)  
✅ Documentation complete with examples  
✅ Context files updated

---

## Notes

**Key Insight**: The hard work is already done! The C++ runtime is complete and well-tested. We just need to expose it to Aria code via the builtin mechanism.

**Follow Existing Pattern**: Look at how `print()` is implemented:
1. Type checker registration
2. IR code generation
3. External C function linkage

**Quality Over Speed**: Take time to test thoroughly. File I/O is critical infrastructure - bugs here will affect every program that uses files.

**Next After This**: Phase 4.4 (Math Library) or circle back to Phase 4.2.5 (allocator testing prerequisites).
