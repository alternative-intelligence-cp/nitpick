# The `nyte` Type (Byte)

**Category**: Types → Specialized  
**Syntax**: `nyte`  
**Purpose**: 8-bit byte value

---

## Overview

`nyte` represents an **8-bit byte**. Alternative name for byte-oriented operations.

---

## Range

| Aspect | Value |
|--------|-------|
| **Values** | 0 to 255 or -128 to 127 |
| **Storage** | 8 bits (1 byte) |

---

## Use Cases

### Binary Data

```aria
// Read bytes
buffer: []nyte = read_bytes(file, 1024);

// Process byte by byte
for byte in buffer {
    process_nyte(byte);
}
```

### Protocol Encoding

```aria
// Network packet bytes
packet: []nyte = create_packet();
```

---

## Comparison

```aria
// nyte vs int8/uint8
nyte_val: nyte = 0xFF;
int_val: int8 = 0xFF as int8;    // -1
uint_val: uint8 = 0xFF;  // 255
```

---

## Related

- [int8](int8.md) - Signed 8-bit
- [uint8](uint8.md) - Unsigned 8-bit
- [nit](nit.md) - 4-bit nibble

---

**Remember**: `nyte` is just another name for **byte**!
