# aria_alloc_buffer

**Category**: Memory Model → Allocation Functions  
**Function**: `aria_alloc_buffer(size: i32) -> []byte`  
**Purpose**: Allocate byte buffer

---

## Signature

```aria
fn aria_alloc_buffer(size: i32) -> []byte
```

---

## Description

Allocates a buffer of `size` bytes on the heap.

---

## Parameters

- `size`: Number of bytes to allocate

---

## Returns

Byte array of length `size`, or `nil` if allocation fails.

---

## Basic Usage

```aria
// Allocate 1KB buffer
buffer: []byte = aria_alloc_buffer(1024);

when buffer == nil then
    fail "Allocation failed";
end

// Use buffer
buffer[0] = 0xFF;
buffer[1] = 0xAB;

// Must free
aria_free(buffer);
```

---

## File I/O

```aria
// Read file into buffer
file: File = pass open("data.bin");
size: i32 = file.size();

buffer: []byte = aria_alloc_buffer(size);
defer aria_free(buffer);

pass file.read_into(buffer);
process(buffer);
```

---

## Network Buffer

```aria
// Receive data
buffer: []byte = aria_alloc_buffer(4096);
defer aria_free(buffer);

bytes_read: i32 = socket.recv(buffer);
process_data(buffer, bytes_read);
```

---

## Best Practices

### ✅ DO: Use defer

```aria
buf: []byte = aria_alloc_buffer(1024);
defer aria_free(buf);
```

### ✅ DO: Check Size

```aria
when size > 0 then
    buf: []byte = aria_alloc_buffer(size);
end
```

### ❌ DON'T: Leak Buffers

```aria
// Wrong
buf: []byte = aria_alloc_buffer(1024);
// Never freed!
```

---

## Related

- [aria_alloc](aria_alloc.md) - Raw allocation
- [aria_alloc_array](aria_alloc_array.md) - Typed arrays
- [aria_free](aria_free.md) - Free memory

---

**Remember**: Buffers must be **freed** with `aria_free`!
