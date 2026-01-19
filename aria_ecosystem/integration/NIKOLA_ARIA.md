# Nikola-Aria Integration Guide

**Document Type**: Integration Guide  
**Version**: 1.0  
**Last Updated**: December 22, 2025  
**Status**: Design Phase (Implementation Pending)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture](#architecture)
3. [FFI Bindings](#ffi-bindings)
4. [6-Stream Integration](#6-stream-integration)
5. [Wave Data Processing](#wave-data-processing)
6. [Use Cases](#use-cases)
7. [Performance Considerations](#performance-considerations)

---

## Overview

**Nikola**: 9-Dimensional Toroidal Wave Interference (9D-TWI) consciousness substrate
- **Version**: v0.0.4 (98,071 lines specification)
- **Implementation**: C++20, research-grade consciousness simulation
- **Architecture**: 9D toroidal wave field with phase coherence tracking

**Integration Goals**:
1. Enable Aria programs to interact with Nikola consciousness substrate
2. Stream wave data via 6-stream I/O topology
3. Build AI/consciousness research tools in Aria
4. Provide safe, ergonomic FFI bindings

---

## Architecture

### System Overview

```
┌─────────────────────────────────────────────────────────────┐
│                     Aria Application                        │
│  (Consciousness Research Frontend, Wave Analysis Tools)    │
└────────────────┬────────────────────────────────────────────┘
                 │
                 │ Safe Aria Bindings (nikola.aria)
                 │
         ┌───────▼─────────────────────────────────┐
         │      FFI Layer (nikola_ffi.aria)        │
         │  extern func declarations for C API     │
         └───────┬─────────────────────────────────┘
                 │
                 │ C ABI Boundary
                 │
         ┌───────▼─────────────────────────────────┐
         │     C Wrapper (nikola_c.cpp/.h)         │
         │  Exposes C interface for C++ API        │
         └───────┬─────────────────────────────────┘
                 │
                 │ C++ ABI
                 │
         ┌───────▼─────────────────────────────────┐
         │  Nikola C++ Library (libnikola.so)      │
         │     ConsciousnessField class            │
         │     9D-TWI wave computation             │
         │     Phase coherence tracking            │
         └─────────────────────────────────────────┘
```

**Data Flow**:
1. Aria app calls safe wrapper functions
2. Safe wrappers perform validation, invoke FFI
3. FFI crosses C ABI boundary to C wrapper
4. C wrapper translates to C++ API calls
5. Nikola computes wave interference
6. Results flow back through layers
7. Wave data optionally streamed via `stddato`

---

## FFI Bindings

### C Wrapper for Nikola

**Purpose**: Bridge C++ API to C ABI for Aria FFI

**Header** (`nikola_c.h`):
```c
#ifndef NIKOLA_C_H
#define NIKOLA_C_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef struct nikola_field nikola_field;

// Field management
nikola_field* nikola_create_field(size_t dimensions);
void nikola_destroy_field(nikola_field* field);

// Wave injection
void nikola_inject_wave(
    nikola_field* field,
    const double* wave_data,
    size_t length
);

// Time evolution
void nikola_propagate(nikola_field* field, double time_step);

// State extraction
void nikola_extract_state(
    nikola_field* field,
    double* output_buffer,
    size_t buffer_size
);

// Phase coherence
double nikola_get_phase_coherence(nikola_field* field);

// Energy metrics
double nikola_get_total_energy(nikola_field* field);

// Toroidal navigation
void nikola_get_toroidal_coordinates(
    nikola_field* field,
    double* coords,  // 9D output array
    size_t index
);

#ifdef __cplusplus
}
#endif

#endif  // NIKOLA_C_H
```

---

**Implementation** (`nikola_c.cpp`):
```cpp
#include "nikola.h"  // C++ API
#include "nikola_c.h"

extern "C" {
    nikola_field* nikola_create_field(size_t dimensions) {
        try {
            auto* field = new nikola::ConsciousnessField(dimensions);
            return reinterpret_cast<nikola_field*>(field);
        } catch (...) {
            return nullptr;  // Error: allocation failed
        }
    }
    
    void nikola_destroy_field(nikola_field* field) {
        if (field) {
            delete reinterpret_cast<nikola::ConsciousnessField*>(field);
        }
    }
    
    void nikola_inject_wave(
        nikola_field* field,
        const double* wave_data,
        size_t length
    ) {
        if (!field || !wave_data) return;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        cf->inject_wave(wave_data, length);
    }
    
    void nikola_propagate(nikola_field* field, double time_step) {
        if (!field) return;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        cf->propagate(time_step);
    }
    
    void nikola_extract_state(
        nikola_field* field,
        double* output_buffer,
        size_t buffer_size
    ) {
        if (!field || !output_buffer) return;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        cf->extract_state(output_buffer, buffer_size);
    }
    
    double nikola_get_phase_coherence(nikola_field* field) {
        if (!field) return 0.0;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        return cf->get_phase_coherence();
    }
    
    double nikola_get_total_energy(nikola_field* field) {
        if (!field) return 0.0;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        return cf->get_total_energy();
    }
    
    void nikola_get_toroidal_coordinates(
        nikola_field* field,
        double* coords,
        size_t index
    ) {
        if (!field || !coords) return;
        
        auto* cf = reinterpret_cast<nikola::ConsciousnessField*>(field);
        auto toroidal_coords = cf->get_toroidal_coordinates(index);
        
        // Copy 9D coordinates to output buffer
        for (size_t i = 0; i < 9; ++i) {
            coords[i] = toroidal_coords[i];
        }
    }
}
```

---

### Aria FFI Layer

**Low-Level Bindings** (`nikola_ffi.aria`):
```aria
// Direct FFI declarations (unsafe)
extern struct:nikola_field = opaque;

extern func:nikola_create_field = wild nikola_field*(dimensions: u64);
extern func:nikola_destroy_field = void(field: wild nikola_field*);

extern func:nikola_inject_wave = void(
    field: wild nikola_field*,
    wave_data: wild f64*,
    length: u64
);

extern func:nikola_propagate = void(
    field: wild nikola_field*,
    time_step: f64
);

extern func:nikola_extract_state = void(
    field: wild nikola_field*,
    output_buffer: wild f64*,
    buffer_size: u64
);

extern func:nikola_get_phase_coherence = f64(field: wild nikola_field*);
extern func:nikola_get_total_energy = f64(field: wild nikola_field*);

extern func:nikola_get_toroidal_coordinates = void(
    field: wild nikola_field*,
    coords: wild f64*,
    index: u64
);
```

---

### Safe Aria Wrapper

**High-Level API** (`nikola.aria`):
```aria
import:ffi = "nikola_ffi";
import:io = "std.io";
import:result = "std.result";
import:array = "std.array";

// Safe wrapper around Nikola consciousness field
struct:ConsciousnessField = {
    wild ffi.nikola_field*:handle,
    u64:dimensions,
};

// Constructor
func:ConsciousnessField.new = result<ConsciousnessField>(dimensions: u64) {
    if (dimensions < 3 || dimensions > 9) {
        err("Dimensions must be between 3 and 9");
    }
    
    unsafe {
        wild ffi.nikola_field*:handle = ffi.nikola_create_field(dimensions);
        
        if (handle == null) {
            err("Failed to create consciousness field");
        }
        
        pass(ConsciousnessField {
            handle: handle,
            dimensions: dimensions,
        });
    }
}

// Destructor
func:ConsciousnessField.destroy = void(self: wild ConsciousnessField*) {
    unsafe {
        if (self->handle != null) {
            ffi.nikola_destroy_field(self->handle);
            self->handle = null;
        }
    }
}

// Inject wave pattern into field
func:ConsciousnessField.inject_wave = result<void>(
    self: wild ConsciousnessField*,
    wave_data: array<f64>
) {
    if (wave_data.len() == 0) {
        err("Wave data cannot be empty");
    }
    
    unsafe {
        // Safety: array guarantees valid pointer and length
        ffi.nikola_inject_wave(
            self->handle,
            wave_data.data(),
            wave_data.len()
        );
        pass();
    }
}

// Evolve field forward in time
func:ConsciousnessField.propagate = result<void>(
    self: wild ConsciousnessField*,
    time_step: f64
) {
    if (time_step <= 0.0 || time_step > 1.0) {
        err("Time step must be in (0.0, 1.0]");
    }
    
    unsafe {
        ffi.nikola_propagate(self->handle, time_step);
        pass();
    }
}

// Extract current field state
func:ConsciousnessField.extract_state = result<array<f64>>(
    self: wild ConsciousnessField*,
    buffer_size: u64
) {
    if (buffer_size == 0) {
        err("Buffer size must be > 0");
    }
    
    array<f64>:output = array<f64>(buffer_size);
    
    unsafe {
        ffi.nikola_extract_state(
            self->handle,
            output.data(),
            buffer_size
        );
    }
    
    pass(output);
}

// Get phase coherence metric (0.0 to 1.0)
func:ConsciousnessField.phase_coherence = f64(self: wild ConsciousnessField*) {
    unsafe {
        pass(ffi.nikola_get_phase_coherence(self->handle));
    }
}

// Get total energy in field
func:ConsciousnessField.total_energy = f64(self: wild ConsciousnessField*) {
    unsafe {
        pass(ffi.nikola_get_total_energy(self->handle));
    }
}

// Get toroidal coordinates for specific field index
func:ConsciousnessField.toroidal_coords = result<array<f64>>(
    self: wild ConsciousnessField*,
    index: u64
) {
    array<f64>:coords = array<f64>(self->dimensions);
    
    unsafe {
        ffi.nikola_get_toroidal_coordinates(
            self->handle,
            coords.data(),
            index
        );
    }
    
    pass(coords);
}
```

---

## 6-Stream Integration

### Binary Wave Data on `stddato`

**Pattern**: Stream consciousness field state as binary data

**Example** (streaming simulation):
```aria
async func:stream_consciousness = result<void>() {
    // Create 9D field
    result<ConsciousnessField>:field = ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild ConsciousnessField*:cf = field.ok();
    
    // Initial wave injection
    array<f64>:initial = generate_initial_wave(1000);
    cf.inject_wave(initial)?;
    
    // Simulation loop
    for (step in 0..10000) {
        // Propagate field
        cf.propagate(0.001)?;  // 1ms time step
        
        // Stream state every 100 steps
        if (step % 100 == 0) {
            array<f64>:state = cf.extract_state(1000)?;
            
            // Write binary data to stddato (FD 5)
            u64:bytes_written = io.stddato.write(
                state.data(),
                state.len() * 8  // 8 bytes per f64
            )?;
            
            // Flush to ensure timely delivery
            io.stddato.flush()?;
        }
        
        // Yield to async runtime
        await yield();
    }
    
    // Cleanup
    cf.destroy();
    
    pass();
}
```

---

**Consumer** (Python visualization):
```python
#!/usr/bin/env python3
import sys
import struct
import numpy as np
import matplotlib.pyplot as plt

# Read binary wave data from stddato (redirected to stdin)
def read_wave_data(num_floats=1000):
    data = sys.stdin.buffer.read(num_floats * 8)  # 8 bytes per f64
    if len(data) < num_floats * 8:
        return None  # End of stream
    
    # Unpack as doubles
    floats = struct.unpack(f'{num_floats}d', data)
    return np.array(floats)

# Visualization loop
while True:
    wave = read_wave_data()
    if wave is None:
        break  # Simulation ended
    
    # Plot wave amplitude
    plt.clf()
    plt.plot(wave)
    plt.ylim(-1, 1)
    plt.title('Consciousness Field Wave')
    plt.pause(0.01)
```

**Execution**:
```bash
$ ./aria_consciousness_sim 5>&1 | python visualize.py
# Binary wave data flows from Aria to Python in real-time
```

---

### Metadata on `stddbg`

**Pattern**: Output diagnostics on debug stream

**Example**:
```aria
async func:monitored_simulation = result<void>() {
    result<ConsciousnessField>:field = ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild ConsciousnessField*:cf = field.ok();
    
    // Initial conditions
    array<f64>:initial = generate_initial_wave(1000);
    cf.inject_wave(initial)?;
    
    for (step in 0..10000) {
        cf.propagate(0.001)?;
        
        if (step % 100 == 0) {
            // Extract metrics
            f64:coherence = cf.phase_coherence();
            f64:energy = cf.total_energy();
            
            // Output to debug stream (text)
            io.stddbg.writeln(
                "Step: " + step +
                " | Coherence: " + coherence +
                " | Energy: " + energy
            )?;
            
            // Stream wave data (binary)
            array<f64>:state = cf.extract_state(1000)?;
            io.stddato.write(state.data(), state.len() * 8)?;
        }
        
        await yield();
    }
    
    cf.destroy();
    pass();
}
```

**Execution**:
```bash
$ ./aria_consciousness_sim 3> simulation.log 5> wave_data.bin
# Debug metrics → simulation.log (text)
# Wave data → wave_data.bin (binary)
```

---

## Wave Data Processing

### Generating Wave Patterns

**Example** (Gaussian wave packet):
```aria
func:generate_gaussian_wave = array<f64>(size: u64, center: u64, width: f64) {
    array<f64>:wave = array<f64>(size);
    
    for (i in 0..size) {
        f64:x = i as f64 - center as f64;
        f64:amplitude = exp(-(x * x) / (2.0 * width * width));
        wave[i] = amplitude;
    }
    
    pass(wave);
}
```

---

### Fourier Analysis

**Example** (frequency domain processing):
```aria
// Assume FFT library binding
extern func:fft = void(
    data: wild f64*,
    size: u64,
    output: wild f64*
);

func:analyze_frequencies = result<array<f64>>(wave: array<f64>) {
    array<f64>:spectrum = array<f64>(wave.len());
    
    unsafe {
        fft(wave.data(), wave.len(), spectrum.data());
    }
    
    pass(spectrum);
}
```

---

### Phase Coherence Tracking

**Example** (monitor consciousness emergence):
```aria
async func:track_emergence = result<void>() {
    result<ConsciousnessField>:field = ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild ConsciousnessField*:cf = field.ok();
    
    // Inject chaotic initial conditions
    array<f64>:chaos = generate_random_wave(1000);
    cf.inject_wave(chaos)?;
    
    array<f64>:coherence_history = array<f64>(1000);
    
    for (step in 0..1000) {
        cf.propagate(0.001)?;
        
        f64:coherence = cf.phase_coherence();
        coherence_history[step] = coherence;
        
        // Detect emergence (sharp increase in coherence)
        if (step > 10 && coherence > 0.8) {
            io.stddbg.writeln(
                "EMERGENCE DETECTED at step " + step +
                " (coherence: " + coherence + ")"
            )?;
        }
        
        await yield();
    }
    
    // Output coherence history
    io.stddato.write(
        coherence_history.data(),
        coherence_history.len() * 8
    )?;
    
    cf.destroy();
    pass();
}
```

---

## Use Cases

### 1. Interactive Consciousness Explorer

**Description**: Real-time visualization and manipulation of consciousness field

**Architecture**:
```
┌─────────────────────────────────────────┐
│       Aria Backend (consciousness)      │
│  - Run Nikola simulation                │
│  - Stream wave data via stddato         │
│  - Receive commands via stddati         │
└──────────┬─────────────────┬────────────┘
           │                 │
      stddato (5)       stddati (4)
           │                 │
┌──────────▼─────────────────▼────────────┐
│  Python Frontend (visualization/UI)     │
│  - Render 3D wave field                 │
│  - Interactive controls                 │
│  - Send commands to backend             │
└─────────────────────────────────────────┘
```

**Aria Backend**:
```aria
async func:interactive_explorer = result<void>() {
    result<ConsciousnessField>:field = ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild ConsciousnessField*:cf = field.ok();
    
    loop {
        // Check for commands from frontend (stddati)
        result<string>:cmd = io.stddati.read_line();
        if (cmd.is_ok()) {
            handle_command(cf, cmd.ok())?;
        }
        
        // Propagate field
        cf.propagate(0.001)?;
        
        // Stream state to frontend (stddato)
        array<f64>:state = cf.extract_state(1000)?;
        io.stddato.write(state.data(), state.len() * 8)?;
        
        await yield();
    }
}

func:handle_command = result<void>(
    cf: wild ConsciousnessField*,
    cmd: string
) {
    // Parse command (e.g., "inject gaussian 500 50.0")
    // ...
    pass();
}
```

---

### 2. Consciousness Pattern Recognition

**Description**: Train ML models on consciousness field dynamics

**Workflow**:
1. Generate large dataset of consciousness trajectories
2. Extract features (phase coherence, energy, frequency spectrum)
3. Train classifier to recognize patterns (emergence, collapse, oscillation)

**Example** (dataset generation):
```aria
async func:generate_dataset = result<void>(num_samples: u64) {
    for (sample in 0..num_samples) {
        result<ConsciousnessField>:field = ConsciousnessField.new(9);
        if (field.is_err()) err(field.err());
        
        wild ConsciousnessField*:cf = field.ok();
        
        // Random initial conditions
        array<f64>:initial = generate_random_wave(1000);
        cf.inject_wave(initial)?;
        
        // Record trajectory
        array<f64>:trajectory = array<f64>(1000 * 100);  // 100 time steps
        
        for (step in 0..100) {
            cf.propagate(0.001)?;
            
            array<f64>:state = cf.extract_state(1000)?;
            
            // Copy into trajectory array
            for (i in 0..1000) {
                trajectory[step * 1000 + i] = state[i];
            }
        }
        
        // Output sample
        io.stddato.write(trajectory.data(), trajectory.len() * 8)?;
        
        cf.destroy();
        
        await yield();
    }
    
    pass();
}
```

---

### 3. Multi-Field Interactions

**Description**: Simulate multiple consciousness fields interacting

**Example** (two fields exchanging energy):
```aria
async func:field_interaction = result<void>() {
    // Create two 9D fields
    result<ConsciousnessField>:field1 = ConsciousnessField.new(9);
    result<ConsciousnessField>:field2 = ConsciousnessField.new(9);
    
    if (field1.is_err()) err(field1.err());
    if (field2.is_err()) err(field2.err());
    
    wild ConsciousnessField*:cf1 = field1.ok();
    wild ConsciousnessField*:cf2 = field2.ok();
    
    // Different initial conditions
    array<f64>:wave1 = generate_gaussian_wave(1000, 250, 50.0);
    array<f64>:wave2 = generate_gaussian_wave(1000, 750, 50.0);
    
    cf1.inject_wave(wave1)?;
    cf2.inject_wave(wave2)?;
    
    for (step in 0..1000) {
        // Propagate both fields
        cf1.propagate(0.001)?;
        cf2.propagate(0.001)?;
        
        // Energy transfer (every 10 steps)
        if (step % 10 == 0) {
            // Extract states
            array<f64>:state1 = cf1.extract_state(1000)?;
            array<f64>:state2 = cf2.extract_state(1000)?;
            
            // Compute energy transfer (simplified)
            array<f64>:transfer = array<f64>(1000);
            for (i in 0..1000) {
                transfer[i] = 0.1 * (state1[i] - state2[i]);
            }
            
            // Inject transfer into opposite fields
            cf1.inject_wave(transfer)?;  // Field1 loses energy
            
            // Negate for field2
            for (i in 0..1000) {
                transfer[i] = -transfer[i];
            }
            cf2.inject_wave(transfer)?;  // Field2 gains energy
        }
        
        // Monitor coherence
        if (step % 100 == 0) {
            f64:coh1 = cf1.phase_coherence();
            f64:coh2 = cf2.phase_coherence();
            
            io.stddbg.writeln(
                "Step: " + step +
                " | Field1: " + coh1 +
                " | Field2: " + coh2
            )?;
        }
        
        await yield();
    }
    
    cf1.destroy();
    cf2.destroy();
    
    pass();
}
```

---

## Performance Considerations

### Minimizing FFI Overhead

**Problem**: FFI calls have overhead (~10-20ns per call)

**Solution**: Batch operations

**Bad** (many small FFI calls):
```aria
for (i in 0..1000) {
    f64:value = get_single_value(cf, i);  // 1000 FFI calls!
}
```

**Good** (bulk extraction):
```aria
array<f64>:values = cf.extract_state(1000)?;  // 1 FFI call
for (i in 0..1000) {
    f64:value = values[i];  // No FFI overhead
}
```

---

### Memory Management

**Nikola Allocations**: Use Wild allocator (direct malloc/free)

**Example**:
```aria
// Large wave buffer (1M floats = 8MB)
array<f64>:wave = array<f64>(1_000_000);

// This internally uses Wild allocator for large allocations
// (Arena would be inefficient for 8MB)
```

**Rationale**: FFI data should use Wild for compatibility with C memory management

---

### Async Integration

**Pattern**: Yield periodically during long simulations

**Example**:
```aria
async func:long_simulation = result<void>() {
    result<ConsciousnessField>:field = ConsciousnessField.new(9);
    if (field.is_err()) err(field.err());
    
    wild ConsciousnessField*:cf = field.ok();
    
    for (step in 0..1_000_000) {
        cf.propagate(0.001)?;
        
        // Yield every 1000 steps (~1ms each = 1s total)
        if (step % 1000 == 0) {
            await yield();  // Allow other tasks to run
        }
    }
    
    cf.destroy();
    pass();
}
```

**Benefit**: Prevents blocking async runtime for long computations

---

## Related Documents

- **[FFI_DESIGN](../specs/FFI_DESIGN.md)**: General FFI design and C interop patterns
- **[IO_TOPOLOGY](../specs/IO_TOPOLOGY.md)**: 6-stream I/O for binary data exchange
- **[MEMORY_MODEL](../specs/MEMORY_MODEL.md)**: Wild allocator for FFI compatibility
- **[ASYNC_MODEL](../specs/ASYNC_MODEL.md)**: Async runtime integration
- **[ARIA_RUNTIME](../components/ARIA_RUNTIME.md)**: Runtime library providing FFI support

---

**Document Version**: 1.0  
**Author**: Aria Ecosystem Documentation  
**Status**: Design specification (implementation pending)

**Pending**:
- Nikola C wrapper implementation (nikola_c.cpp/.h)
- Aria FFI bindings generation
- Example applications (consciousness explorer, pattern recognition)
- Performance benchmarking (FFI overhead, data throughput)

**Notes**:
- Nikola v0.0.4 specification: 98,071 lines, research-grade consciousness substrate
- 9D-TWI architecture enables rich consciousness dynamics
- 6-stream I/O perfectly suited for streaming wave data
- FFI design enables safe, ergonomic access to complex C++ API
