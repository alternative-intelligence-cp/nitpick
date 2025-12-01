# CMakeLists.txt
# Root build script for Aria Compiler and Runtime
cmake_minimum_required(VERSION 3.14)
project(AriaCompiler VERSION 1.0 LANGUAGES C CXX)

# ==============================================================================
# 1. Dependency Resolution
# ==============================================================================

# Find LLVM 18+
# We require LLVM 18 specifically for the AVX-512 'VPTERNLOGD' intrinsic support
find_package(LLVM 18 REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(llvm_libs core support native irreader)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

# Integrate Mimalloc (Static Link for Wild Heap)
# We embed mimalloc from the 'vendor' directory to ensure version stability.
# By setting MI_BUILD_STATIC to ON, we tell mimalloc's own CMake to generate a.a/.lib
set(MI_BUILD_STATIC ON CACHE BOOL "Build static library")
set(MI_BUILD_SHARED OFF CACHE BOOL "Build shared library")
set(MI_BUILD_TESTS OFF CACHE BOOL "Build tests")
set(MI_OVERRIDE OFF CACHE BOOL "Do not override standard malloc globally yet")

add_subdirectory(vendor/mimalloc)

# ==============================================================================
# 2. Source Configuration
# ==============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Enable strict warnings to catch potential pointer aliasing issues early
add_compile_options(-Wall -Wextra -Wno-unused-parameter)

include_directories(src)

# ==============================================================================
# 3. Target Definition: The Compiler (ariac)
# ==============================================================================

add_executable(ariac
 src/driver/main.cpp               # Compiler Driver Entry Point
 src/frontend/lexer.cpp
 src/frontend/parser_decl.cpp      # Implementation of Variable Declaration Parsing
 src/frontend/parser_pick.cpp      # Pick Parser
 src/frontend/parser_stmt.cpp      # [New] Defer and Stmt Parser
 src/frontend/ast/control_flow.cpp
 src/backend/codegen.cpp
 src/backend/lowering_ternary.cpp  # AVX-512 Lowering Logic
)

# Link against LLVM and Mimalloc
target_link_libraries(ariac 
 PRIVATE 
 ${llvm_libs}
 mimalloc-static
)

# ==============================================================================
# 4. Target Definition: The Runtime Library (libaria)
# ==============================================================================

# This library is linked into every compiled Aria program.
# It contains the GC, the Scheduler, and the bridge to Mimalloc.
add_library(aria_runtime STATIC
 src/runtime/memory/allocator.c        # The bridge (aria.alloc -> mi_malloc)
 src/runtime/gc/gc_impl.cpp            # Major/Minor GC Logic
 src/runtime/gc/nursery.cpp            # Fragmented Nursery Implementation
 src/runtime/io_windows.cpp            # lpReserved2 implementation
 src/runtime/io_linux.cpp              # Linux fork/dup2 implementation
 src/runtime/concurrency/scheduler.cpp # Work-stealing logic
 src/runtime/concurrency/ramp.cpp      # [New] RAMP Logic
 src/stdlib/io/fast_read.cpp
 src/runtime/core/string_impl.cpp      # SSO String Implementation
)

# Critical: The runtime MUST contain the static mimalloc symbols.
target_link_libraries(aria_runtime mimalloc-static)

# ==============================================================================
# 5. Build Artifacts
# ==============================================================================
message(STATUS "Aria Compiler Build Configured.")
message(STATUS "Run 'cmake --build.' to compile.")

/* ==========================================================================
   MODULE: DISTRIBUTION / INSTALLER
   FILE: dist/install_aria.sh
   ========================================================================== */
#!/bin/bash
# dist/install_aria.sh
# Official Aria Language AppImage Installer
# Best Practices Enforced:
# 1. No absolute paths in binary (handled by build system configuration)
# 2. Integration with ~/.local/share/applications for desktop menus
# 3. Symlinking to ~/.local/bin for CLI access without altering global /usr

set -e

# Resolve the absolute path of the installer script
APPIMAGE_PATH=$(realpath "$0")
INSTALL_DIR="$HOME/.local/bin"
DESKTOP_DIR="$HOME/.local/share/applications"
ICON_DIR="$HOME/.local/share/icons"

echo "=========================================="
echo "   Aria Language Environment Installer    "
echo "=========================================="

# 1. Create Directories
echo "[*] Creating user-local directories..."
mkdir -p "$INSTALL_DIR"
mkdir -p "$DESKTOP_DIR"
mkdir -p "$ICON_DIR"

# 2. Install Executable
# We recommend keeping the AppImage in a stable location like ~/Applications
TARGET_DIR="$HOME/Applications"
TARGET_APPIMAGE="$TARGET_DIR/aria-lang.AppImage"

mkdir -p "$TARGET_DIR"

if; then
   echo "[*] Copying AppImage to $TARGET_APPIMAGE..."
   cp "$APPIMAGE_PATH" "$TARGET_APPIMAGE"
fi

chmod +x "$TARGET_APPIMAGE"

# 3. Create Desktop Entry
# This allows the Aria IDE or Compiler GUI to appear in system menus.
echo "[*] Generating Desktop Entry..."
cat > "$DESKTOP_DIR/aria-lang.desktop" <<EOF

Type=Application
Name=Aria Language
Comment=Aria High-Performance Systems Language
Exec="$TARGET_APPIMAGE"
Icon=aria-lang
Categories=Development;IDE;
Terminal=true
StartupNotify=true
EOF

# Make the desktop entry executable (required by some DEs)
chmod +x "$DESKTOP_DIR/aria-lang.desktop"

# 4. CLI Integration
# Add a symlink to PATH if it doesn't exist
if; then
   rm "$INSTALL_DIR/aria"
fi

echo "[*] Linking 'aria' command to $INSTALL_DIR/aria..."
ln -s "$TARGET_APPIMAGE" "$INSTALL_DIR/aria"

# 5. Environment Check
if]; then
   echo "WARNING: $INSTALL_DIR is not in your PATH."
   echo "Add the following to your shell config (.bashrc /.zshrc):"
   echo "export PATH=\"\$HOME/.local/bin:\$PATH\""
fi

echo "=========================================="
echo "   Installation Complete. Run 'aria'      "
echo "=========================================="

