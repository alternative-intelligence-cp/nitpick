# Legacy Standard Library

This directory contains old-syntax Aria standard library modules from the
pre-`stdlib/` era. These files use the old import conventions (`use std.*`)
and C-style pointer notation (`int8*`, `void*`) that predates the current
`string` type and intrinsic model.

## Do Not Use

These files are **not compiled** and **not on any active search path**.
They are kept for historical reference only.

## Current stdlib

The active standard library lives in `stdlib/` at the repo root.
The `std/` directory is a symlink to `stdlib/`, enabling `use std.io` to
resolve to `stdlib/io.aria`.
