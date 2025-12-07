# CMakeLists.txt
# Root build script for Aria Compiler and Runtime
cmake_minimum_required(VERSION 3.14)
project(AriaCompiler VERSION 1.0 LANGUAGES C CXX)

# ==============================================================================
# 1. Dependency Resolution
# ==============================================================================

# Find LLVM 18+
# We require LLVM 18+ for the AVX-512 'VPTERNLOGD' intrinsic support
# LLVM 19.x is also compatible
find_package(LLVM REQUIRED CONFIG)
if(LLVM_VERSION_MAJOR LESS 18)
    message(FATAL_ERROR "LLVM version must be 18 or higher, found ${LLVM_PACKAGE_VERSION}")
endif()
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

llvm_map_components_to_libnames(llvm_libs 
    core support native irreader
    x86asmparser x86codegen x86desc x86info
    aarch64asmparser aarch64codegen aarch64desc aarch64info
    armasmparser armcodegen armdesc arminfo
    riscvasmparser riscvcodegen riscvdesc riscvinfo
)

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
 src/frontend/preprocessor.cpp    # Preprocessor (Macros, Includes, Conditionals)
 src/frontend/lexer.cpp
 src/frontend/parser.cpp           # Core Parser Implementation (all methods consolidated)
 src/frontend/parser_struct.cpp    # Struct declarations
 # src/frontend/parser_decl.cpp      # Variable declarations (merged into parser.cpp)
 # src/frontend/parser_expr.cpp      # Expression parsing (merged into parser.cpp)
 # src/frontend/parser_func.cpp      # Function declarations (merged into parser.cpp)
 # src/frontend/parser_stmt.cpp      # Statement parsing (merged into parser.cpp)
 src/frontend/sema/borrow_checker.cpp  # Borrow Checker Implementation
 src/frontend/sema/escape_analysis.cpp # Escape Analysis Implementation
 src/frontend/sema/type_checker.cpp    # Type Checking Implementation
 # src/frontend/ast/control_flow.cpp  # TODO: Create when implementing control flow AST nodes
 src/backend/codegen.cpp
 src/backend/codegen_tbb.cpp           # TBB Sticky Error Propagation (v0.0.7 Critical Fix)
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
 src/runtime/memory/wildx_allocator.c  # Wildx allocator (executable memory for JIT)
 src/runtime/gc/gc_impl.cpp            # Major/Minor GC Logic
 src/runtime/gc/nursery.cpp            # Fragmented Nursery Implementation
 src/runtime/io_windows.cpp            # lpReserved2 implementation
 src/runtime/io_linux.cpp              # Linux fork/dup2 implementation
 src/runtime/io/print.cpp              # Print functions (print, println)
 src/runtime/io/file.cpp               # File I/O (readFile, writeFile)
 src/runtime/concurrency/scheduler.cpp # Work-stealing logic
 src/runtime/concurrency/ramp.cpp      # [New] RAMP Logic
 src/stdlib/io/fast_read.cpp
 src/runtime/core/string_impl.cpp      # SSO String Implementation
)

# Critical: The runtime MUST contain the static mimalloc symbols.
target_link_libraries(aria_runtime mimalloc-static)

# ==============================================================================
# 5. Testing Infrastructure
# ==============================================================================
# Enable testing support
enable_testing()

# Add the tests subdirectory
add_subdirectory(tests)

# ==============================================================================
# 6. Build Artifacts
# ==============================================================================
message(STATUS "Aria Compiler Build Configured.")
message(STATUS "Run 'cmake --build .' to compile.")
message(STATUS "Run 'make test' or 'ninja test' to run unit tests.")
