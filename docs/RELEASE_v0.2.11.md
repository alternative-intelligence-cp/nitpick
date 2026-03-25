# Aria v0.2.11 — OS Components, Kernel & AI Filesystem

## What's New

### Threading & Concurrency (Task 1 — CRITICAL)

Full threading and concurrency support for Aria programs:

#### Thread Pool (`stdlib/thread_pool.aria`)
```aria
use "thread_pool.aria".*;
use "atomic.aria".*;

func:worker = void(int64:arg) {
    // process task
};

int64:pool = raw(ThreadPool.create(4i32));  // 4 worker threads
raw(ThreadPool.submit(pool, worker, 42i64));
raw(ThreadPool.wait_idle(pool));
raw(ThreadPool.shutdown(pool));
```

#### Atomics (`stdlib/atomic.aria`)
Thread-safe atomic types with all memory orderings:
- `AtomicInt32` — 32-bit atomic integer
- `AtomicInt64` — 64-bit atomic integer
- `AtomicBool` — atomic boolean

```aria
use "atomic.aria".*;

AtomicInt32:counter = raw(AtomicInt32.create(0i32));
AtomicInt32.fetch_add(counter, 1i32, MEMORY_ORDER_SEQ_CST);
int32:val = raw(AtomicInt32.load(counter, MEMORY_ORDER_SEQ_CST));
```

#### Lock-Free Data Structures (`stdlib/lockfree.aria`)
- `LFQueue` — MPMC bounded queue (Vyukov-style)
- `LFStack` — Lock-free stack (Treiber algorithm)
- `RingBuf` — SPSC ring buffer (Lamport-style)

```aria
use "lockfree.aria".*;

int64:q = raw(LFQueue.create(256i64));
raw(LFQueue.push(q, 42i64));
int64:val = raw(LFQueue.pop_val(q));
```

### OS Components (Task 2)

#### Arena Allocator (`stdlib/arena.aria`)
Fast bump allocator with bulk free — ideal for frame-based allocation:
```aria
use "arena.aria".*;

int64:a = raw(Arena.create(1048576i64));  // 1MB
int64:off = raw(Arena.alloc(a, 64i64));   // alloc 64 bytes
raw(Arena.write_int64(a, off, 42i64));
raw(Arena.reset(a));                       // bulk free
raw(Arena.destroy(a));
```

#### Pool Allocator (`stdlib/pool_alloc.aria`)
Fixed-size block allocator with O(1) alloc/free:
```aria
use "pool_alloc.aria".*;

int64:pool = raw(PoolAlloc.create(64i64, 256i64));  // 64-byte blocks, 256 of them
int64:blk = raw(PoolAlloc.get(pool));
raw(PoolAlloc.write_int64(pool, blk, 0i64, 999i64));
raw(PoolAlloc.put(pool, blk));
```

#### IPC — Shared Memory (`stdlib/shm.aria`)
Anonymous shared memory regions:
```aria
use "shm.aria".*;

int64:shm = raw(SharedMemory.create(4096i64));
raw(SharedMemory.write_int64(shm, 0i64, 42i64));
int64:val = raw(SharedMemory.read_int64(shm, 0i64));
```

#### IPC — Pipes (`stdlib/pipe.aria`)
```aria
use "pipe.aria".*;

int64:p = raw(Pipe.create());
raw(Pipe.write_int64(p, 777i64));
int64:val = raw(Pipe.read_int64(p));
```

#### Signal Handling (`stdlib/signal.aria`)
```aria
use "signal.aria".*;

raw(Signal.register(10i32));         // SIGUSR1
int32:pending = raw(Signal.pending(10i32));
```

#### Process Management (`stdlib/process.aria`)
```aria
use "process.aria".*;

int64:pid = raw(Process.getpid());
string:home = raw(Process.getenv("HOME"));
```

### AI-Native Filesystem (Task 3)

AriaFS — a FUSE-based filesystem optimized for AI workloads:

- **Tensor metadata** via extended attributes (dtype, shape, checkpoint flags)
- **64KB block size** for large sequential I/O
- **Snapshot support** for model checkpointing
- **Content-addressable** design (future: SHA-256 block dedup)

#### CLI Tools
```bash
# Mount
aria-fs-create /mnt/aria-fs

# Inspect files + tensor metadata
aria-fs-inspect /mnt/aria-fs

# Stats
aria-fs-compact /mnt/aria-fs
```

#### Aria API (`stdlib/aifs.aria`)
```aria
use "aifs.aria".*;

raw(AIFs.set_dtype("/mnt/aria-fs/weights.bin", "float32"));
raw(AIFs.set_shape("/mnt/aria-fs/weights.bin", "3,224,224"));
string:dtype = raw(AIFs.get_dtype("/mnt/aria-fs/weights.bin"));
```

### AriaX Kernel Modifications (Task 4)

#### Hexstream Kernel Patches
Three kernel patches for the AriaX six-stream I/O topology:

1. **01-hexstream-preserve-fds.patch** — Preserve FDs 3-5 across exec()
2. **02-hexstream-auto-open.patch** — Auto-open FDs 3-5 at process init
3. **03-hexstream-procfs-names.patch** — Show stream names in /proc/pid/fdinfo

#### Hexstream Aria API (`stdlib/hexstream.aria`)
```aria
use "hexstream.aria".*;

raw(Hexstream.init());                         // Ensure FDs 3-5 open
raw(Hexstream.redirect_to_file(3i32, "debug.log"));
raw(Hexstream.dbg("starting computation"));
raw(Hexstream.write_int64(5i32, 42i64));       // FD 5 = stddato
```

## New Stdlib Modules (this release)

| Module | Type | Description |
|--------|------|-------------|
| `thread_pool.aria` | Type:ThreadPool | Fixed-size thread pool |
| `atomic.aria` | Type:AtomicInt32/Int64/Bool | Thread-safe atomics |
| `lockfree.aria` | Type:LFQueue/LFStack/RingBuf | Lock-free data structures |
| `arena.aria` | Type:Arena | Bump allocator |
| `pool_alloc.aria` | Type:PoolAlloc | Fixed-block allocator |
| `shm.aria` | Type:SharedMemory | Shared memory IPC |
| `pipe.aria` | Type:Pipe | Unix pipe IPC |
| `signal.aria` | Type:Signal | Signal handling |
| `process.aria` | Type:Process | Process management |
| `aifs.aria` | Type:AIFs | AI filesystem metadata |
| `hexstream.aria` | Type:Hexstream | Six-stream I/O |

## Test Results

| Test File | Tests | Status |
|-----------|-------|--------|
| test_thread_basic.aria | 7 | ALL PASS |
| test_thread_full.aria | 10 | ALL PASS |
| test_thread_pool.aria | 4 | ALL PASS |
| test_lockfree.aria | 7 | ALL PASS |
| test_os.aria | 19 | ALL PASS |
| test_pipe.aria | 2 | ALL PASS |
| test_hexstream.aria | 7 | ALL PASS |
| **Total** | **56** | **ALL PASS** |

## Runtime Architecture

All complex C types are exposed to Aria via the **handle-based shim pattern**:
- Static pools of C structs indexed by `int64` handles
- Avoids the `{i1, ptr}` optional wrapper corruption from `wild ?*` returns
- Zero overhead for the hot path (direct array indexing)

New runtime shims added:
- `src/runtime/thread/thread_pool_shim.cpp` — Thread pool with task queue
- `src/runtime/thread/lockfree_shim.c` — Lock-free MPMC queue, stack, ring buffer
- `src/runtime/atomic/atomic_shim.cpp` — Atomic type handle wrappers
- `src/runtime/os/os_shim.c` — Arena, pool, SHM, pipe, signal, process
- `src/runtime/os/aifs_shim.c` — AI filesystem metadata (xattr-based)
- `src/runtime/os/hexstream_shim.c` — Hexstream I/O (FDs 3-5)

## Build

```bash
cd REPOS/aria/build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

AriaFS (standalone):
```bash
cd tools/aria-fs
gcc -o aria-fs aria_fs.c $(pkg-config --cflags --libs fuse3) -lpthread -O2
```
