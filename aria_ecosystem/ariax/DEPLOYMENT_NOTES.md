# AriaX v1.1.0 - Six-Stream Edition Deployment

## What's New
- **14 fully functional six-stream CLI utilities** with FD 3 telemetry
- All utilities support --debug flag for NDJSON telemetry output
- Built from aria_utils libraries with proper six-stream architecture
- Ready for AriaX kernel (6.8.12-ariax) with FD 3/4/5 support

## Utilities with Six-Stream Support (NEW!)
1. acat - File concatenation with telemetry
2. als - Directory listing with rich metadata
3. acp - File copying with progress tracking
4. awc - Word count (built from scratch!)
5. amv - File moving with telemetry
6. agrep - Pattern searching with ANSI colors
7. aps - Process status viewer
8. aglob - Glob pattern matching
9. astat - File statistics
10. atar - TAR archive utility
11. ajq - JSON parser/formatter
12. acurl - HTTP client with progress
13. apic - Image processing (PNG/JPEG/BMP)
14. atest - Test framework runner

## Pre-Existing Utilities (9)
- abc, adap, aria_make, ariash, ascope, asql, depgraph
- test_six_streams, test_vte_streams

## VM Deployment Steps

### 1. Copy to VM
```bash
scp ariax-dist-1.1.0-six-stream.tar.gz randy@192.168.122.110:/tmp/
```

### 2. SSH to VM
```bash
ssh randy@192.168.122.110
```

### 3. Extract and Install
```bash
cd /tmp
tar -xzf ariax-dist-1.1.0-six-stream.tar.gz
cd dist_package
sudo ./install.sh
```

### 4. Verify Installation
```bash
# Check utilities are in PATH
which acat als acp awc

# Test six-stream telemetry
echo "test data" | awc --debug 3> /tmp/telemetry.log
cat /tmp/telemetry.log

# Should see NDJSON output like:
# {"level":"debug","component":"awc","message":"awc starting with 1 file(s)"}
# {"level":"telemetry","component":"awc","total_lines":1,...}
```

### 5. Test Six-Stream Architecture
```bash
# Verify FD 3/4/5 work in shell
echo "Testing FD 3" >&3  # Should work without error
echo "Testing FD 4" >&4  # Should work without error
echo "Testing FD 5" >&5  # Should work without error

# Test utility telemetry piping
acat --debug /etc/passwd 3>&1 | ajq '.'
```

### 6. Test Utility Combinations
```bash
# List and search with telemetry
als --debug /usr/bin 3> als.log | agrep --debug 'gcc' 3> agrep.log

# HTTP + JSON pipeline
acurl --debug -s https://httpbin.org/json 3> curl.log | ajq -c

# Image processing
apic --debug -w 800 input.png output.png 3> image.log
```

## Telemetry Format
All utilities output NDJSON on FD 3:
```json
{"level":"debug","component":"utility_name","message":"..."}
{"level":"telemetry","component":"utility_name","metric1":value,...}
{"level":"error","component":"utility_name","error":"..."}
```

## Next Steps
- Test all utilities on six-stream kernel
- Build remaining empty utilities (awatch, anetstat, etc.)
- Create AriaX ISO with pre-configured environment
- Match v0.1.0 specs to implementations
