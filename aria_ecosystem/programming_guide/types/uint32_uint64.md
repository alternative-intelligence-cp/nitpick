# Unsigned Medium/Large Integers (uint32, uint64)

**Category**: Types → Integers → Unsigned  
**Sizes**: 32-bit, 64-bit  
**Ranges**: uint32 (0 to 4.3 billion), uint64 (0 to 18.4 quintillion)  
**Purpose**: Production unsigned integers for sizes, counts, addresses  
**Status**: ✅ FULLY IMPLEMENTED

---

## Table of Contents

1. [Overview](#overview)
2. [uint32: 32-bit Unsigned Integer](#uint32-32-bit-unsigned-integer)
3. [uint64: 64-bit Unsigned Integer](#uint64-64-bit-unsigned-integer)
4. [vs Signed Integers (Critical Differences)](#vs-signed-integers-critical-differences)
5. [When to Use Unsigned vs Signed](#when-to-use-unsigned-vs-signed)
6. [Overflow Wrapping Behavior](#overflow-wrapping-behavior)
7. [Underflow Wrapping Behavior](#underflow-wrapping-behavior)
8. [Binary Representation](#binary-representation)
9. [Arithmetic Operations](#arithmetic-operations)
10. [Memory Addresses and Sizes](#memory-addresses-and-sizes)
11. [Hash Functions and Checksums](#hash-functions-and-checksums)
12. [C Interoperability](#c-interoperability)
13. [Performance Characteristics](#performance-characteristics)
14. [Common Patterns](#common-patterns)
15. [Anti-Patterns](#anti-patterns)
16. [Migration from C/C++](#migration-from-cc)
17. [Related Concepts](#related-concepts)

---

## Overview

Aria's **uint32** and **uint64** are production-scale unsigned integers representing **non-negative values only** with modular wrap-around arithmetic.

**Key Properties**:
- **Non-negative only**: Range starts at 0 (no negative values)
- **Massive positive range**: uint32 (4.3 billion), uint64 (18.4 quintillion)
- **Wrapping overflow**: Defined modular arithmetic
- **Dangerous underflow**: 0 - 1 = MAX (catches many by surprise!)
- **Memory/file sizes**: Natural for byte counts, array lengths, addresses
- **Fast**: Zero-overhead native machine instructions

**⚠️ CRITICAL**: Underflow wraps (0u32 - 1u32 = 4,294,967,295u32). Test carefully!

---

## uint32: 32-bit Unsigned Integer

### Range

**Minimum**: 0 (0x00000000)  
**Maximum**: 4,294,967,295 (0xFFFFFFFF, 2^32 - 1)  
**Total values**: 4,294,967,296 (2^32)

**Approximately**: 4.3 billion (exactly double int32's positive range)

### Declaration

```aria
uint32:count = 1000000u32;
uint32:file_size = 4294967295u32;
uint32:max_value = 4294967295u32;
uint32:min_value = 0u32;
```

### Literal Suffix

```aria
uint32:value = 3000000000u32;  // Explicit uint32 literal (u32 suffix)
```

### Common Use Cases

1. **File sizes**: Files up to 4GB (uint32 bytes)
2. **Array lengths**: Arrays up to 4.3 billion elements
3. **Memory sizes**: Buffer sizes, allocation sizes
4. **IPv4 addresses**: 32-bit IP addresses (0.0.0.0 to 255.255.255.255)
5. **Hash values**: 32-bit hash functions (CRC-32, FNV-1a)
6. **Counters**: Event counts, statistics (4.3B events)
7. **Unix timestamps**: Seconds since epoch (until Year 2106!)
8. **C interop**: Matching `uint32_t`, `unsigned int` on most platforms

### Example: File Size Handling

```aria
struct:FileInfo = {
    string:filename;
    uint32:size_bytes;    // File size up to 4GB
    uint32:crc32;         // Checksum
    uint32:creation_time; // Unix timestamp
};

FileInfo:document = {
    filename: "large_video.mp4",
    size_bytes: 3221225472u32,  // 3GB
    crc32: 0xDEADBEEFu32,
    creation_time: 1739318400u32  // Feb 12, 2026
};

// Check if file fits in memory
func:can_load_in_memory = (file_size: uint32, available_memory: uint64) -> bool {
    return uint64(file_size) <= available_memory;
}

if (can_load_in_memory(document.size_bytes, 8589934592u64)) {  // 8GB RAM
    log.write("File fits in memory\n");
}
```

### Production Scale Examples

```aria
// Database: 4.3 billion user IDs
uint32:user_id = 4000000000u32;

// Web server: Request counter
uint32:request_count = 0u32;

till server_running loop
    Request:req = accept_connection();
    request_count += 1u32;
    
    // Check for overflow (every billion requests)
    if (request_count % 1000000000u32 == 0u32) {
        log.write("Requests: "); request_count.write_to(log); log.write("\n");
    }
    
    if (request_count == 0u32) {
        // Wrapped! (4.3B + 1 = 0)
        stderr.write("Request counter overflow!\n");
        request_count = 1u32;  // Reset
    }
end

// IPv4 address (32 bits)
uint32:ip = 0xC0A80101u32;  // 192.168.1.1 in hex

// Extract octets
uint8:a = uint8((ip >> 24u32) & 0xFFu32);  // 192
uint8:b = uint8((ip >> 16u32) & 0xFFu32);  // 168
uint8:c = uint8((ip >> 8u32) & 0xFFu32);   // 1
uint8:d = uint8(ip & 0xFFu32);              // 1
```

---

## uint64: 64-bit Unsigned Integer

### Range

**Minimum**: 0 (0x0000000000000000)  
**Maximum**: 18,446,744,073,709,551,615 (0xFFFFFFFFFFFFFFFF, 2^64 - 1)  
**Total values**: 2^64

**Approximately**: 18.4 quintillion (18.4 × 10^18)

### Declaration

```aria
uint64:huge = 18000000000000000000u64;  // 18 quintillion
uint64:max_value = 18446744073709551615u64;
uint64:min_value = 0u64;
```

### Literal Suffix

```aria
uint64:value = 10000000000000u64;  // Explicit uint64 literal (10 trillion)
```

### Common Use Cases

1. **Large file sizes**: Files up to 18 exabytes (modern filesystems)
2. **Memory addresses**: 64-bit pointers, virtual memory
3. **Byte counts**: Total bytes processed, network traffic
4. **Large datasets**: Genomic data (billions of base pairs)
5. **High-precision timestamps**: Nanoseconds since epoch (until year 2554!)
6. **Cryptography**: Large integers for key generation
7. **Performance counters**: CPU cycles, instruction counts
8. **C interop**: Matching `uint64_t`, `unsigned long` on 64-bit platforms

### Example: Large File Operations

```aria
struct:LargeFileInfo = {
    string:path;
    uint64:size_bytes;     // Up to 18 exabytes
    uint64:blocks_used;    // Filesystem blocks
    uint64:inode_number;   // Filesystem inode
};

LargeFileInfo:backup = {
    path: "/backups/database_dump.sql",
    size_bytes: 549755813888u64,  // 512GB
    blocks_used: 1073741824u64,    // 1G blocks
    inode_number: 123456789u64
};

// Calculate blocks needed (4KB blocks)
func:calculate_blocks = (file_size: uint64) -> uint64 {
    const:uint64:BLOCK_SIZE = 4096u64;
    return (file_size + BLOCK_SIZE - 1u64) / BLOCK_SIZE;  // Round up
}

uint64:blocks = calculate_blocks(backup.size_bytes);

log.write("Blocks needed: "); blocks.write_to(log); log.write("\n");
```

### Extreme Scale Examples

```aria
// Network traffic: Petabytes processed
uint64:bytes_transferred = 5000000000000000u64;  // 5 petabytes

// Genomic database: 10 billion genomes × 3.2 billion base pairs
uint64:total_base_pairs = 32000000000000000000u64;  // 32 quintillion
// ⚠️ Wait, that's 3.2 × 10^19 (over uint64 max!)
// Actual: uint64 max is ~1.8 × 10^19

// Astronomical: Atoms in human body
uint64:atoms = 7000000000000000000000000000u64;  // 7 × 10^27
// ⚠️ DOESN'T FIT! uint64 max is ~10^19, not 10^27!
// Need int512 or larger for this scale!

// Realistic uint64 use: CPU cycle counter
uint64:cpu_cycles = read_performance_counter();

log.write("CPU cycles: "); cpu_cycles.write_to(log); log.write("\n");

// Nanosecond timestamps (good until year 2554)
uint64:timestamp_ns = 1739318400000000000u64;  // Feb 12, 2026 in nanoseconds
```

---

## vs Signed Integers (Critical Differences)

| Feature | Unsigned uint32/uint64 | Signed int32/int64 |
|---------|------------------------|-------------------|
| **Range** | uint32: 0 to 4.3B | int32: -2.1B to +2.1B |
| **Range** | uint64: 0 to 18.4 quintillion | int64: -9.2 to +9.2 quintillion |
| **Negative values** | NONE (0 is minimum) | Yes (negative numbers) |
| **Overflow** | Wraps to 0 | Wraps to negative MIN |
| **Underflow** | Wraps to MAX (DANGER!) | Wraps to positive MAX |
| **Bit pattern** | All bits are data | MSB is sign bit |
| **Use case** | Sizes, counts, addresses | Offsets, differences, signed data |

### Example: Range Comparison

```aria
// uint32 can represent larger positive values
uint32:u = 4000000000u32;  // ✅ Valid (4 billion)

int32:s = 4000000000i32;   // ❌ DOESN'T FIT! (max is 2.1B)
// Wraps to negative value!

// But signed can represent negatives
int32:offset = -1000000i32;  // ✅ Valid

uint32:unsigned_offset = -1000000u32;  // ❌ DOESN'T COMPILE (no negative literals)
```

---

## When to Use Unsigned vs Signed

### Use Unsigned (uint32/uint64) When:

1. **Sizes/counts**: File sizes, array lengths, byte counts (never negative)
2. **Memory addresses**: Pointers, offsets in address space
3. **Bit manipulation**: Flags, masks, bitfields
4. **Hash values**: CRC-32, hash function outputs
5. **Full positive range**: Need all 4.3B values (vs int32's 2.1B)
6. **C interop**: `size_t`, `uint32_t`, `uint64_t`
7. **Binary protocols**: Network packets, file formats

### Use Signed (int32/int64) When:

1. **Can be negative**: Differences, deltas, offsets
2. **Arithmetic**: Addition/subtraction where result might be negative
3. **Comparisons**: Need "less than zero" checks
4. **Default choice**: When unsure (safer for arithmetic!)
5. **Year 2038**: Use int64 for timestamps (TBB tbb32 for overflow detection)

### Quick Decision Matrix

```aria
// File size - USE UNSIGNED (never negative)
uint64:file_size = get_file_size("/data/bigfile.dat");

// File offset (can seek backwards) - USE SIGNED
int64:offset = -1000i64;  // Seek backwards from end

// Array length - USE UNSIGNED (never negative)
uint32:array_length = my_array.length;

// Array index difference - USE SIGNED (can be negative)
int32:index_diff = int32(end_index) - int32(start_index);

// Memory address - USE UNSIGNED
uint64:address = get_memory_address(ptr);

// Temperature - USE SIGNED
int32:temperature = -40i32;  // -40°C
```

---

## Overflow Wrapping Behavior

### Modular Arithmetic

```aria
// uint32 overflow (modulo 2^32)
uint32:max = 4294967295u32;
uint32:wrapped = max + 1u32;
// wrapped = 0 (wraps to minimum)

uint32:large = 4294967290u32;
large += 10u32;
// large = 4 (4,294,967,290 + 10 = 4,294,967,300 % 2^32 = 4)

// uint64 overflow (modulo 2^64)
uint64:max64 = 18446744073709551615u64;
uint64:wrapped64 = max64 + 1u64;
// wrapped64 = 0
```

### Example: Hash Function (Intentional Wrapping)

```aria
// FNV-1a hash function (uses uint32 wrapping)
func:fnv1a_hash = (data: string) -> uint32 {
    const:uint32:FNV_OFFSET = 2166136261u32;
    const:uint32:FNV_PRIME = 16777619u32;
    
    uint32:hash = FNV_OFFSET;
    
    till data.length loop:i
        uint8:byte = data[i];
        hash = hash ^ uint32(byte);
        hash = hash * FNV_PRIME;  // Intentional wrapping!
    end
    
    pass(hash);
}

uint32:hash = fnv1a_hash("Hello, World!");
log.write("Hash: 0x"); hash.write_hex_to(log); log.write("\n");
```

---

## Underflow Wrapping Behavior

### The Underflow Trap (Magnified!)

```aria
// DANGER: Underflow wraps to MAXIMUM!
uint32:count = 0u32;
count -= 1u32;
// count = 4,294,967,295 (NOT -1!) ⚠️

uint64:balance = 0u64;
balance -= 1u64;
// balance = 18,446,744,073,709,551,615 (NOT -1!) ⚠️
```

**Why extremely dangerous**: Looks like a huge positive value, breaks logic completely!

### Real-World Bug Example

```aria
// BUGGY: Bandwidth calculation
uint64:bytes_sent = 1000u64;
uint64:bytes_received = 5000u64;

uint64:net_upload = bytes_sent - bytes_received;  // ⚠️ UNDERFLOWS!
// net_upload = 18,446,744,073,709,547,616 (enormous!)

if (net_upload > 1000000u64) {
    stderr.write("Upload exceeds 1MB limit!\n");  // FALSE ALARM!
}

// CORRECT: Check before subtracting
int64:net_upload_signed;

if (bytes_sent > bytes_received) {
    net_upload_signed = int64(bytes_sent - bytes_received);  // Positive
} else {
    net_upload_signed = -int64(bytes_received - bytes_sent);  // Negative
}
```

### Safe Subtraction

```aria
func:safe_subtract_u32 = (a: uint32, b: uint32) -> ?uint32 {
    if (b > a) {
        return NIL;  // Would underflow
    }
    
    return a - b;  // Safe
}

// Usage
uint32:balance = 1000u32;
uint32:withdrawal = 5000u32;

?uint32:new_balance = safe_subtract_u32(balance, withdrawal);

if (new_balance == NIL) {
    stderr.write("Insufficient funds!\n");
} else {
    log.write("New balance: "); new_balance.write_to(log); log.write("\n");
}
```

---

## Binary Representation

### Bit Representation (uint32)

```
4,294,967,295 = 0xFFFFFFFF = all 32 bits set
2,147,483,648 = 0x80000000 = bit 31 set (NOT negative, unlike int32!)
1             = 0x00000001
0             = 0x00000000
```

**No sign bit**: Bit 31 is just data, not a sign!

### Bit Representation (uint64)

```
18,446,744,073,709,551,615 = 0xFFFFFFFFFFFFFFFF (all 64 bits set)
9,223,372,036,854,775,808  = 0x8000000000000000 (bit 63 set, NOT negative!)
1                           = 0x0000000000000001
0                           = 0x0000000000000000
```

---

## Arithmetic Operations

### Addition with Overflow Detection

```aria
func:checked_add_u32 = (a: uint32, b: uint32) -> ?uint32 {
    // Check if a + b would overflow
    if (b > (4294967295u32 - a)) {
        return NIL;  // Would overflow
    }
    
    return a + b;  // Safe
}

// Usage
uint32:file_size1 = 3000000000u32;  // 3GB
uint32:file_size2 = 2000000000u32;  // 2GB

?uint32:total = checked_add_u32(file_size1, file_size2);

if (total == NIL) {
    stderr.write("Combined file size too large for uint32!\n");
    
    // Use uint64 instead
    uint64:total64 = uint64(file_size1) + uint64(file_size2);
    log.write("Total (uint64): "); total64.write_to(log); log.write("\n");
}
```

### Multiplication Overflow

```aria
// Large multiplication can overflow
uint32:width = 65536u32;
uint32:height = 65536u32;
uint32:pixels = width * height;  // 4,294,967,296... overflows to 0!

// Safe: widen to uint64
uint64:pixels64 = uint64(width) * uint64(height);  // 4,294,967,296 (fits!)
```

---

## Memory Addresses and Sizes

### Address Arithmetic

```aria
// Memory address (64-bit pointer)
uint64:base_address = 0x7FFFF0000000u64;
uint64:offset = 1024u64;
uint64:target_address = base_address + offset;

log.write("Target: 0x");
target_address.write_hex_to(log);
log.write("\n");
```

### Size Calculations

```aria
// Array size in bytes
uint32:element_count = 1000000u32;
uint32:element_size = 64u32;  // 64 bytes per element

uint64:total_bytes = uint64(element_count) * uint64(element_size);
// 64,000,000 bytes (61MB)

log.write("Memory required: ");
total_bytes.write_to(log);
log.write(" bytes\n");
```

---

## Hash Functions and Checksums

### CRC-32 (Cyclic Redundancy Check)

```aria
func:crc32 = (data: uint8[], length: uint32) -> uint32 {
    const:uint32:CRC32_POLYNOMIAL = 0xEDB88320u32;
    uint32:crc = 0xFFFFFFFFu32;
    
    till length loop:i
        uint8:byte = data[i];
        crc = crc ^ uint32(byte);
        
        till 8u32 loop:bit
            if ((crc & 1u32) != 0u32) {
                crc = (crc >> 1u32) ^ CRC32_POLYNOMIAL;
            } else {
                crc = crc >> 1u32;
            }
        end
    end
    
    pass(~crc);  // Final XOR
}

uint8[]:data = [0x48u8, 0x65u8, 0x6Cu8, 0x6Cu8, 0x6Fu8];  // "Hello"
uint32:checksum = crc32(data, 5u32);

log.write("CRC-32: 0x");
checksum.write_hex_to(log);
log.write("\n");
```

---

## C Interoperability

### Direct ABI Compatibility

| Aria Type | C Type | Size | Range |
|-----------|--------|------|-------|
| uint32 | `uint32_t`, `unsigned int` | 4 bytes | 0 to 4.3B |
| uint64 | `uint64_t`, `unsigned long` (64-bit) | 8 bytes | 0 to 18.4 quintillion |

### FFI Example

```aria
// C library functions
extern "C" {
    func:c_file_size = uint64(pathname: int8@);
    func:c_malloc = uint8@(size: uint64);
    func:c_free = void(ptr: uint8@);
    func:c_crc32 = uint32(data: uint8@, length: uint32);
}

// Aria usage
uint64:size = c_file_size("/var/log/system.log");

log.write("File size: "); size.write_to(log); log.write(" bytes\n");

// Allocate memory
uint8@:buffer = c_malloc(4096u64);

if (buffer == NULL) {
    stderr.write("Allocation failed!\n");
} else {
    // Use buffer...
    
    // Free memory
    c_free(buffer);
}
```

---

## Performance Characteristics

### Zero Overhead

```aria
uint32:a = 1000000u32;
uint32:b = 2000000u32;
uint32:sum = a + b;
```

**Generated assembly** (x86-64):
```asm
mov eax, 1000000    ; Load into EAX (32-bit)
add eax, 2000000    ; Add
; Result in EAX
```

### uint32 vs uint64 Performance

**32-bit operations**: One instruction on all CPUs  
**64-bit operations**: One instruction on 64-bit CPUs, multiple instructions on 32-bit CPUs

**Modern 64-bit systems**: uint64 performance identical to uint32.

---

## Common Patterns

### Saturating Arithmetic

```aria
func:saturating_add_u32 = (a: uint32, b: uint32) -> uint32 {
    if (b > (4294967295u32 - a)) {
        return 4294967295u32;  // Saturate at max
    } else {
        return a + b;
    }
}

func:saturating_sub_u32 = (a: uint32, b: uint32) -> uint32 {
    if (b > a) {
        return 0u32;  // Saturate at min
    } else {
        return a - b;
    }
}
```

### Range-Checked Narrowing

```aria
func:narrow_to_u32 = (value: uint64) -> ?uint32 {
    if (value > 4294967295u64) {
        return NIL;  // Out of range
    }
    
    return uint32(value);  // Safe cast
}
```

### Byte Packing (uint32 from 4 bytes)

```aria
func:pack_u32 = (b3: uint8, b2: uint8, b1: uint8, b0: uint8) -> uint32 {
    return (uint32(b3) << 24u32) | (uint32(b2) << 16u32) | 
           (uint32(b1) << 8u32) | uint32(b0);
}

// Network byte order (big-endian)
uint32:ip = pack_u32(192u8, 168u8, 1u8, 1u8);  // 192.168.1.1
```

---

## Anti-Patterns

### ❌ File Size Accumulation Without Overflow Check

```aria
// WRONG: Can overflow silently
uint32:total_bytes = 0u32;

till files.length loop:i
    uint32:size = files[i].get_size();
    total_bytes += size;  // ❌ Can wrap at 4GB!
end

// RIGHT: Use uint64 for large totals
uint64:total_bytes = 0u64;

till files.length loop:i
    uint64:size = files[i].get_size();
    total_bytes += size;  // ✅ Good up to 18 exabytes
end
```

### ❌ Underflow in Difference Calculations

```aria
// WRONG: Underflow trap
uint32:start_time = get_timestamp();
uint32:end_time = start_time - 1000u32;  // ❌ If start_time < 1000, HUGE value!

// RIGHT: Check or use signed
int64:start = int64(get_timestamp());
int64:duration_ms = -1000i64;
int64:end = start + duration_ms;  // Can go negative safely
```

### ❌ Mixing Signed and Unsigned

```aria
// WRONG: Implicit conversion surprise
int32:signed_val = -1i32;
uint32:unsigned_val = 1u32;

if (signed_val < unsigned_val) {  // ⚠️ -1 converts to 4,294,967,295!
    // Comparison: 4,294,967,295 < 1 is FALSE (unexpected!)
}

// RIGHT: Explicit widening
int64:wide_signed = int64(signed_val);
int64:wide_unsigned = int64(unsigned_val);

if (wide_signed < wide_unsigned) {  // TRUE (correct)
}
```

---

## Migration from C/C++

### Underflow Behavior (Same as C)

**C**: Unsigned underflow wraps
```c
uint32_t a = 0;
a -= 1;  // a = 4,294,967,295 (wraps)
```

**Aria**: Same wrapping
```aria
uint32:a = 0u32;
a -= 1u32;  // a = 4,294,967,295 (wraps)
```

### Explicit Suffixes Required

**C**: Implicit
```c
uint32_t x = 1000000;  // int literal converted
```

**Aria**: Explicit
```aria
uint32:x = 1000000u32;  // u32 suffix required
```

---

## Related Concepts

### Other Integer Types

- **uint8, uint16**: Smaller unsigned integers
- **int32, int64**: Signed medium/large integers
- **tbb32, tbb64**: Symmetric-range error-propagating integers

### Special Values

- **NIL**: Optional types (not applicable to uint32/uint64)
- **ERR**: TBB sentinel (not applicable to unsigned)
- **NULL**: Pointer sentinel

---

## Summary

**uint32** and **uint64** are production-scale unsigned integers:

✅ **Massive positive range**: uint32 (4.3B), uint64 (18.4 quintillion)  
✅ **Wrapping overflow**: Defined modular arithmetic  
✅ **Zero overhead**: Native CPU instructions  
✅ **Ideal for sizes**: File sizes, array lengths, byte counts  
✅ **C compatible**: Direct FFI  

⚠️ **Underflow to MAX**: 0 - 1 = 4.3B (uint32) or 18.4 quintillion (uint64)!  
⚠️ **Cannot be negative**: Wrong choice if values can go below zero  
⚠️ **Signed/unsigned mixing**: Implicit conversions surprise  

**Use uint32/uint64 for**:
- File sizes, memory sizes, byte counts
- Array lengths, element counts
- Memory addresses, pointers
- Hash values, checksums
- IPv4 addresses, network ports
- Never-negative values

**Use int32/int64 for**:
- Can-be-negative values
- Default choice (safer)
- Timestamps (with overflow detection)

---

**Next**: [Balanced Numbers (trit, tryte, nit, nyte)](trit_tryte.md)  
**See Also**: [uint8/uint16](uint8_uint16.md), [int32/int64](int32_int64.md)

---

*Aria Language Project - Production Unsigned Integers*  
*February 12, 2026 - Timestamped prior art on unsigned semantics*
