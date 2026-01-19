# acurl - Aria HTTP Client

Hex-Stream HTTP client library implementing the Aria transfer protocol.

## Purpose

`acurl` provides a high-fidelity HTTP client that utilizes the Aria Hex-Stream I/O topology. Unlike legacy tools (curl, wget) that conflate UI, data, and errors on shared streams, acurl strictly segregates:

- **stdout (FD 1)**: User interface (progress bars, status)
- **stderr (FD 2)**: Error reporting
- **stddbg (FD 3)**: Telemetry and protocol traces (NDJSON)
- **stddato (FD 5)**: Binary data output

This separation enables:
- Bit-perfect binary transfers
- Rich UI without corrupting pipelines
- Structured observability for debugging
- Safe shell composition

## Features

- **Hex-Stream Topology**: Full 6-stream I/O support
- **TBB Safety**: Twisted Balanced Binary arithmetic for overflow protection
- **Zero-Copy I/O**: splice() optimization on Linux
- **Async Ready**: libcurl multi interface support
- **NDJSON Telemetry**: Structured logging to stddbg
- **FFI Support**: C-compatible API for Aria runtime

## Quick Start

```cpp
#include "acurl/http_client.hpp"
using namespace aria::acurl;

// Simple GET with Hex-Stream output
auto result = hex_get("https://example.com/file.bin");
// Binary data -> FD 5 (stddato)
// Progress -> FD 1 (stdout)
// Errors -> FD 2 (stderr)

// Custom request
HttpRequest req;
req.method = HttpMethod::POST;
req.url = "https://api.example.com/upload";
req.body = "data to upload";
req.headers.push_back({"Content-Type", "application/octet-stream"});

HttpClient client;
client.set_stddato_fd(FD_STDDATO);
auto [response, error] = client.perform(req);

if (error == ErrorCode::OK) {
    std::cout << "Status: " << response.status_code << "\n";
}
```

## Building

```bash
mkdir build && cd build
cmake ..
make
make test
```

Requires: libcurl development headers

## Shell Usage

```bash
# Download to file (stddato -> file)
./acurl https://example.com/large.tar.gz 5>output.tar.gz

# Pipe to tar (stddato -> tar)
./acurl https://example.com/archive.tar.gz 5>| tar xzf -

# Capture telemetry
./acurl https://example.com/data.json 3>debug.log 5>data.json

# Full Hex-Stream
./acurl https://example.com/file.bin \
    1>ui.txt \     # Progress
    2>errors.txt \ # Errors
    3>debug.log \  # Telemetry
    5>output.bin   # Data
```

## FFI API (C)

```c
#include "acurl/http_client.hpp"

// Ensure Hex-Stream FDs are available
aria_acurl_ensure_streams();

// Simple GET to stddato
AriaHttpResponse resp = aria_acurl_get("https://example.com/file.bin");
if (resp.error == 0) {
    printf("Downloaded %ld bytes in %ld us\n",
           resp.bytes_transferred, resp.time_us);
}

// Custom request
AriaHttpClientHandle client = aria_acurl_new();
AriaHttpRequest req = {
    .url = "https://api.example.com/data",
    .method = "POST",
    .body = "{\"key\":\"value\"}",
    .body_len = 15,
    .stddato_fd = 5
};
AriaHttpResponse resp = aria_acurl_perform(client, &req);
aria_acurl_free(client);
```

## Telemetry Format

Telemetry is emitted as NDJSON to stddbg (FD 3):

```json
{"type":"request_start","ts":1703644800000000,"method":"GET","url":"https://..."}
{"type":"header_out","ts":1703644800001000,"data":"GET / HTTP/1.1"}
{"type":"header_in","ts":1703644800100000,"data":"HTTP/1.1 200 OK"}
{"type":"progress","ts":1703644800200000,"bytes":65536,"total":1048576,"speed":524288.0}
{"type":"complete","ts":1703644801000000,"status":200,"bytes":1048576,"time_us":1000000}
```

## TBB Safety

All size calculations use TBB (Twisted Balanced Binary) types with sticky error propagation:

```cpp
// If Content-Length overflows, bytes_total becomes TBB64_ERR
int64_t bytes_total = parse_content_length(header);

// Any operation on ERR propagates ERR
bytes_total = tbb_add(bytes_total, chunk_size);

// Check before use
if (is_tbb_err(bytes_total)) {
    // Handle overflow - prevents buffer allocation attacks
    return ErrorCode::OVERFLOW_ERROR;
}
```

## Platform Notes

### Linux
- Uses splice() for zero-copy when possible
- io_uring support planned for future versions

### macOS / Windows
- Standard buffered I/O fallback
- All features work, zero-copy not available

## License

Part of the Aria Language Project.
