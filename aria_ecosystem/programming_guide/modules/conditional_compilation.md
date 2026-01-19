# Conditional Compilation

**Category**: Modules → Compilation  
**Purpose**: Compile different code for different platforms and features

---

## Overview

Conditional compilation allows **one codebase** to support multiple platforms, architectures, and feature sets.

---

## Why Conditional Compilation?

- ✅ Cross-platform code
- ✅ Feature flags
- ✅ Debug vs release builds
- ✅ Architecture-specific optimizations
- ✅ Optional dependencies

---

## Platform-Specific Code

### Operating System

```aria
#[cfg(os = "linux")]
fn get_home_dir() -> string {
    return "/home/user";
}

#[cfg(os = "windows")]
fn get_home_dir() -> string {
    return "C:\\Users\\user";
}

#[cfg(os = "macos")]
fn get_home_dir() -> string {
    return "/Users/user";
}
```

---

### Architecture

```aria
#[cfg(arch = "x86_64")]
fn optimized_function() {
    // x86_64 SIMD instructions
}

#[cfg(arch = "aarch64")]
fn optimized_function() {
    // ARM NEON instructions
}

#[cfg(not(any(arch = "x86_64", arch = "aarch64")))]
fn optimized_function() {
    // Generic fallback
}
```

---

## Feature Flags

### Enabling Features

```toml
# In Cargo.toml or package config
[features]
default = ["logging"]
logging = []
database = []
cache = []
full = ["logging", "database", "cache"]
```

### Using Features

```aria
#[cfg(feature = "logging")]
pub fn log(msg: string) {
    stdout << "[LOG] $msg";
}

#[cfg(feature = "database")]
pub mod database {
    pub fn connect() -> Connection {
        // Database connection
    }
}

#[cfg(feature = "cache")]
pub mod cache {
    pub fn get(key: string) -> ?Data {
        // Cache lookup
    }
}
```

---

## Debug vs Release

```aria
#[cfg(debug)]
pub fn debug_info() {
    stdout << "Debug build";
    stdout << "Extra checks enabled";
}

#[cfg(release)]
pub fn debug_info() {
    // Optimized - no debug output
}

#[cfg(debug)]
const LOG_LEVEL: string = "DEBUG";

#[cfg(release)]
const LOG_LEVEL: string = "INFO";
```

---

## Complex Conditions

### AND Logic

```aria
#[cfg(all(os = "linux", arch = "x86_64"))]
fn linux_x86_64_specific() {
    // Only on 64-bit Linux
}
```

---

### OR Logic

```aria
#[cfg(any(os = "linux", os = "macos", os = "freebsd"))]
fn unix_like() {
    // Any Unix-like system
}
```

---

### NOT Logic

```aria
#[cfg(not(os = "windows"))]
fn non_windows() {
    // Everything except Windows
}
```

---

### Combined

```aria
#[cfg(all(
    any(os = "linux", os = "macos"),
    arch = "x86_64",
    not(feature = "minimal")
))]
fn advanced_unix_feature() {
    // 64-bit Unix with full features
}
```

---

## Common Patterns

### Platform Abstraction

```aria
// Platform-specific implementations
#[cfg(os = "linux")]
mod platform {
    pub fn sleep(ms: i32) {
        extern "C" {
            fn usleep(usec: i32);
        }
        extern.usleep(ms * 1000);
    }
}

#[cfg(os = "windows")]
mod platform {
    pub fn sleep(ms: i32) {
        extern "system" {
            fn Sleep(ms: i32);
        }
        extern.Sleep(ms);
    }
}

// Common interface
pub fn sleep(ms: i32) {
    platform.sleep(ms);
}
```

---

### Feature-Gated Modules

```aria
// Main module
pub mod core {
    pub fn basic_function() { }
}

// Optional modules
#[cfg(feature = "advanced")]
pub mod advanced {
    pub fn advanced_function() { }
}

#[cfg(feature = "experimental")]
pub mod experimental {
    pub fn experimental_function() { }
}
```

---

### Fallback Implementations

```aria
// Optimized version for x86_64
#[cfg(arch = "x86_64")]
fn fast_checksum(data: []u8) -> u32 {
    // SIMD implementation
}

// Generic fallback
#[cfg(not(arch = "x86_64"))]
fn fast_checksum(data: []u8) -> u32 {
    // Portable implementation
}
```

---

## Conditional Imports

```aria
#[cfg(feature = "json")]
import serde_json;

#[cfg(feature = "yaml")]
import serde_yaml;

#[cfg(feature = "database")]
import database.{connect, query};

#[cfg(all(os = "linux", feature = "epoll"))]
import linux.epoll;
```

---

## Conditional Structs

```aria
pub struct Config {
    // Common fields
    pub name: string,
    pub version: string,
    
    // Platform-specific
    #[cfg(os = "windows")]
    pub windows_registry_key: string,
    
    #[cfg(os = "linux")]
    pub linux_systemd_unit: string,
    
    // Feature-gated
    #[cfg(feature = "database")]
    pub db_connection_string: string,
    
    #[cfg(feature = "cache")]
    pub cache_size: usize,
}
```

---

## Conditional Tests

```aria
#[cfg(test)]
mod tests {
    #[cfg(os = "linux")]
    #[test]
    fn test_linux_specific() {
        // Only runs on Linux
    }
    
    #[cfg(feature = "database")]
    #[test]
    fn test_database() {
        // Only runs when database feature enabled
    }
}
```

---

## Development Helpers

```aria
#[cfg(debug)]
pub fn assert_valid(data: Data) {
    if !data.is_valid() {
        panic("Invalid data in debug mode");
    }
}

#[cfg(release)]
pub fn assert_valid(data: Data) {
    // No-op in release
}
```

---

## Target Detection

```aria
// Check at compile time
#[cfg(target_pointer_width = "64")]
const IS_64_BIT: bool = true;

#[cfg(target_pointer_width = "32")]
const IS_64_BIT: bool = false;

#[cfg(target_endian = "little")]
const IS_LITTLE_ENDIAN: bool = true;

#[cfg(target_endian = "big")]
const IS_LITTLE_ENDIAN: bool = false;
```

---

## Best Practices

### ✅ DO: Provide Fallbacks

```aria
#[cfg(feature = "optimized")]
fn process() {
    // Optimized version
}

#[cfg(not(feature = "optimized"))]
fn process() {
    // Standard version
}
```

### ✅ DO: Group Platform Code

```aria
// Good organization
#[cfg(os = "linux")]
mod linux_impl;

#[cfg(os = "windows")]
mod windows_impl;

#[cfg(os = "macos")]
mod macos_impl;

// Common interface
pub use platform_impl::*;
```

### ✅ DO: Document Conditions

```aria
/// Available on Unix-like systems only
#[cfg(any(os = "linux", os = "macos", os = "freebsd"))]
pub fn unix_socket_connect(path: string) -> Socket {
    // Implementation
}
```

### ❌ DON'T: Duplicate Code

```aria
// Bad
#[cfg(os = "linux")]
fn process() {
    common_logic();  // Duplicated
    linux_specific();
}

#[cfg(os = "windows")]
fn process() {
    common_logic();  // Duplicated
    windows_specific();
}

// Better
fn process() {
    common_logic();
    
    #[cfg(os = "linux")]
    linux_specific();
    
    #[cfg(os = "windows")]
    windows_specific();
}
```

---

## Common Configurations

### Cross-Platform File Paths

```aria
#[cfg(os = "windows")]
const PATH_SEPARATOR: u8 = '\\';

#[cfg(not(os = "windows"))]
const PATH_SEPARATOR: u8 = '/';
```

### Networking

```aria
#[cfg(os = "windows")]
#[link(name = "ws2_32")]
extern "system" {
    fn WSAStartup(version: i32, data: *void) -> i32;
}

#[cfg(any(os = "linux", os = "macos"))]
extern "C" {
    fn socket(domain: i32, type: i32, protocol: i32) -> i32;
}
```

---

## Related

- [cfg](cfg.md) - cfg attribute

---

**Remember**: One codebase, **many targets** - use conditional compilation wisely!
