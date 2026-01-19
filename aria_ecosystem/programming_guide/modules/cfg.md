# `cfg` Attribute

**Category**: Modules → Conditional Compilation  
**Syntax**: `#[cfg(...)]`  
**Purpose**: Conditionally compile code based on configuration

---

## Overview

`cfg` enables **platform-specific** and **feature-specific** compilation.

---

## Basic Syntax

```aria
#[cfg(condition)]
item
```

---

## Operating System

```aria
#[cfg(os = "linux")]
fn linux_specific() {
    stdout << "Running on Linux";
}

#[cfg(os = "windows")]
fn windows_specific() {
    stdout << "Running on Windows";
}

#[cfg(os = "macos")]
fn macos_specific() {
    stdout << "Running on macOS";
}
```

---

## Architecture

```aria
#[cfg(arch = "x86_64")]
fn x86_64_code() {
    stdout << "64-bit x86";
}

#[cfg(arch = "aarch64")]
fn arm_code() {
    stdout << "ARM 64-bit";
}

#[cfg(arch = "wasm32")]
fn wasm_code() {
    stdout << "WebAssembly";
}
```

---

## Features

```aria
#[cfg(feature = "logging")]
fn log_message(msg: string) {
    stdout << "[LOG] $msg";
}

#[cfg(feature = "database")]
use database.*;

#[cfg(feature = "cache")]
mod cache {
    // Cache implementation
}
```

---

## Build Configuration

```aria
#[cfg(debug)]
fn debug_only() {
    stdout << "Debug build";
}

#[cfg(release)]
fn release_only() {
    stdout << "Release build";
}
```

---

## Boolean Logic

### `not`

```aria
#[cfg(not(os = "windows"))]
fn unix_like() {
    // Runs on non-Windows platforms
}
```

### `all` (AND)

```aria
#[cfg(all(os = "linux", arch = "x86_64"))]
fn linux_x86_64() {
    // Only on 64-bit Linux
}
```

### `any` (OR)

```aria
#[cfg(any(os = "linux", os = "macos"))]
fn unix_platforms() {
    // Runs on Linux or macOS
}
```

---

## Complex Conditions

```aria
#[cfg(all(
    any(os = "linux", os = "macos"),
    not(arch = "wasm32")
))]
fn native_unix() {
    // Linux or macOS, but not WebAssembly
}
```

---

## Module-Level

```aria
#[cfg(os = "linux")]
mod linux {
    pub fn platform_init() {
        // Linux initialization
    }
}

#[cfg(os = "windows")]
mod windows {
    pub fn platform_init() {
        // Windows initialization
    }
}
```

---

## Import Conditional

```aria
#[cfg(feature = "http")]
import std.http;

#[cfg(feature = "database")]
import database.sqlite;
```

---

## Struct Fields

```aria
struct Config {
    common_field: string,
    
    #[cfg(os = "windows")]
    windows_setting: i32,
    
    #[cfg(os = "linux")]
    linux_setting: i32,
}
```

---

## Function Body

```aria
fn platform_specific_code() {
    #[cfg(os = "linux")]
    {
        stdout << "Linux code";
    }
    
    #[cfg(os = "windows")]
    {
        stdout << "Windows code";
    }
}
```

---

## Common Conditions

### Operating Systems

```aria
os = "linux"
os = "windows"
os = "macos"
os = "ios"
os = "android"
os = "freebsd"
```

### Architectures

```aria
arch = "x86"
arch = "x86_64"
arch = "arm"
arch = "aarch64"
arch = "wasm32"
arch = "wasm64"
```

### Pointer Width

```aria
target_pointer_width = "32"
target_pointer_width = "64"
```

### Endianness

```aria
target_endian = "little"
target_endian = "big"
```

---

## Best Practices

### ✅ DO: Provide Defaults

```aria
#[cfg(os = "windows")]
const PATH_SEP: u8 = '\\';

#[cfg(not(os = "windows"))]
const PATH_SEP: u8 = '/';
```

### ✅ DO: Group Platform Code

```aria
#[cfg(os = "linux")]
mod platform {
    pub fn init() { }
    pub fn cleanup() { }
}

#[cfg(os = "windows")]
mod platform {
    pub fn init() { }
    pub fn cleanup() { }
}
```

### ✅ DO: Document Conditions

```aria
// Only available on Unix-like systems
#[cfg(any(os = "linux", os = "macos"))]
pub fn unix_socket() -> Socket {
    // Implementation
}
```

### ❌ DON'T: Over-complicate

```aria
#[cfg(all(
    any(os = "linux", os = "macos"),
    not(any(arch = "wasm32", arch = "wasm64")),
    feature = "advanced"
))]  // ❌ Too complex!
```

---

## cfg_attr

Apply attributes conditionally:

```aria
#[cfg_attr(os = "windows", link(name = "ws2_32"))]
extern "C" {
    fn network_function();
}
```

---

## Related

- [conditional_compilation](conditional_compilation.md) - Conditional compilation patterns

---

**Remember**: Use `cfg` for **platform** and **feature** differences!
