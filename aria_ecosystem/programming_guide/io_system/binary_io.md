# Binary I/O

**Category**: I/O System → Formats  
**Streams**: Primarily stddati (4), stddato (5)  
**Philosophy**: Efficient data transfer on the data plane

---

## Overview

**Binary I/O** refers to reading and writing raw bytes or binary-encoded structured data. In Aria, binary I/O typically uses the **data plane** streams (stddati/stddato).

---

## Binary Formats

Common binary formats on the data plane:

### MessagePack
```aria
use std::msgpack;

// Read binary MessagePack
data: msgpack::Value = stddati.read_msgpack()?;

// Write binary MessagePack
stddato.write_msgpack(large_dataset)?;
```

### Protocol Buffers
```aria
use std::protobuf;

message: MyProto = stddati.read_protobuf()?;
stddato.write_protobuf(response)?;
```

### CBOR
```aria
use std::cbor;

data: cbor::Value = stddati.read_cbor()?;
stddato.write_cbor(result)?;
```

### Raw Bytes
```aria
// Read raw bytes
bytes: []u8 = stddati.read_all()?;

// Write raw bytes
stddato.write(bytes)?;
```

---

## Why Binary I/O?

**Advantages over text**:
- **Smaller size**: More compact than JSON
- **Faster parsing**: No text-to-number conversion
- **Type preservation**: Binary formats preserve types natively
- **Efficiency**: Lower CPU and bandwidth usage

**Use binary for**:
- Large datasets
- High-throughput pipelines
- Network protocols
- File formats (images, audio, video)

---

## Binary vs Text vs Structured Text

| Aspect | Binary | Structured Text | Plain Text |
|--------|--------|-----------------|------------|
| **Format** | MessagePack, Protobuf | JSON, XML | UTF-8 strings |
| **Size** | Smallest | Medium | Largest |
| **Speed** | Fastest | Medium | Slowest |
| **Human-readable** | No | Yes | Yes |
| **Use case** | Data plane | Data plane | Control plane |

---

## Example: Binary Data Pipeline

```aria
use std::msgpack;

fn main() {
    // Read binary data from stddati
    input_data: msgpack::Value = stddati.read_msgpack()?;
    
    stddbg << "Read " << input_data.size() << " bytes (binary)";
    
    // Process
    output_data: msgpack::Value = transform(input_data);
    
    // Write binary data to stddato
    stddato.write_msgpack(output_data)?;
    
    stdout << "Binary processing complete\n";
}
```

**Usage**:
```bash
# Binary pipeline (smaller, faster than JSON)
./producer 5>data.msgpack
./consumer 4<data.msgpack 5>result.msgpack
```

---

## When to Use Binary I/O

✅ **Use binary I/O for**:
- Large datasets (GB+)
- Performance-critical pipelines
- Network protocols
- File formats (images, video, etc.)
- Embedded systems (size constraints)

❌ **Don't use binary I/O for**:
- User-facing messages (use text on control plane)
- Debug output (use stddbg)
- Small config files (JSON is fine)
- When human inspection is important

---

## See Also

- [Text I/O](text_io.md) - Human-readable text formats
- [stddati](stddati.md), [stddato](stddato.md) - Data plane streams
- [Data Plane](data_plane.md) - Where binary I/O happens
- [Six-Stream Topology](six_stream_topology.md) - Full I/O architecture
