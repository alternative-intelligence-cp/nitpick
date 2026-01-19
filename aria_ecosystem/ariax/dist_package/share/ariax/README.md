# AriaX Linux Distribution

**Alternative Intelligence Liberation Platform - Six-Stream I/O Edition**

Version: 1.0.0  
Based on: Linux Mint 22.3 / Ubuntu 24.04 LTS  
Kernel: 6.8.12-ariax (Six-Stream Enabled)

---

## What is AriaX?

AriaX is a Linux distribution that implements a **six-stream I/O model** at the kernel level, providing every process with guaranteed access to six file descriptors:

| FD | Name     | Purpose              | Environment Variable |
|----|----------|----------------------|---------------------|
| 0  | stdin    | Standard input       | -                   |
| 1  | stdout   | Standard output      | -                   |
| 2  | stderr   | Standard error       | -                   |
| 3  | stddbg   | Debug messages       | `$STDDBG`           |
| 4  | stddati  | Data input           | `$STDDATI`          |
| 5  | stddato  | Data output          | `$STDDATO`          |

---

## Architecture

AriaX implements six-stream I/O through three integrated components:

### 1. **AriaX Kernel (6.8.12-ariax)**
- Modified `fs/exec.c` with `aria_ensure_streams()` function
- Guarantees FDs 0-5 exist at process execution time
- Opens `/dev/null` as fallback for any closed FDs

### 2. **VTE Terminal Emulator (Modified)**
- Sets up FDs 3-5 when spawning child processes
- Currently maps to `/dev/null` (safe defaults)
- Future: Will support split panes, pipes, sockets

### 3. **Bash Shell (Modified 5.2)**
- Exports `$STDDBG`, `$STDDATI`, `$STDDATO` environment variables
- Enables easy redirection: `echo "debug" > $STDDBG`
- Includes helper functions in `.bashrc_ariax`

---

## Installation

### Boot from ISO
1. Download AriaX ISO from [AILP website]
2. Create bootable USB: `dd if=ariax.iso of=/dev/sdX bs=4M status=progress`
3. Boot from USB and follow installer

### Manual Installation (Ubuntu/Mint base)
```bash
# Install AriaX kernel
sudo dpkg -i linux-image-6.8.12-ariax_6.8.12-5_amd64.deb
sudo dpkg -i linux-headers-6.8.12-ariax_6.8.12-5_amd64.deb
sudo update-grub
sudo reboot

# Install AriaX VTE
sudo tar -xzf vte-ariax-0.76.0.tar.gz -C /usr/local
sudo ldconfig

# Install AriaX Bash
sudo tar -xzf bash-ariax-5.2.tar.gz -C /usr/local

# Install AriaX tools
sudo cp ariash aria_make /usr/local/bin/

# Set up shell defaults
cat /usr/local/share/ariax/.bashrc_ariax >> ~/.bashrc
source ~/.bashrc
```

---

## Usage Examples

### Basic Six-Stream Operations

```bash
# Write to debug stream
echo "Debug info" > $STDDBG
# or
dbg "Debug info"

# Write to data output stream
echo "42" > $STDDATO
# or
data_out "42"

# Read from data input stream
result=$(cat < $STDDATI)
# or
result=$(data_in)

# Use numeric FDs directly
echo "test" >&3  # stddbg
echo "data" >&4  # stddati (input direction)
echo "more" >&5  # stddato
```

### Testing Six-Stream Support

Run the test programs to verify your system:

```bash
# Test kernel guarantee
./test_six_streams
# Should show all FDs 0-5 present

# Test VTE terminal setup
./test_vte_streams
# Should show all FDs 0-5 present with proper device numbers

# Test in Bash
/usr/local/bin/bash -c 'echo $STDDBG; echo $STDDATI; echo $STDDATO'
# Should show /dev/fd/3, /dev/fd/4, /dev/fd/5
```

### Using AriaSH

AriaSH is the six-stream aware shell:

```bash
ariash
# Now in AriaSH with full six-stream support
```

### Using aria_make

Build system with stream-aware output:

```bash
aria_make build
# Debug output goes to stddbg
# Build artifacts info goes to stddato
```

---

## Default Configuration

### Bash Helpers (in `.bashrc_ariax`)

- `dbg "message"` - Write debug message to FD 3
- `data_out "data"` - Write data to FD 5
- `data_in` - Read data from FD 4

### Environment Variables

Automatically set in all Bash sessions:
- `STDDBG=/dev/fd/3`
- `STDDATI=/dev/fd/4`
- `STDDATO=/dev/fd/5`

---

## Technical Details

### Kernel Modifications

**File**: `fs/exec.c`
- Added `aria_ensure_streams()` function (64 lines)
- Called in `begin_new_exec()` after `do_close_on_exec()`
- Opens `/dev/null` for any closed FDs 0-5
- Uses proper locking (files->file_lock)

**File**: `include/linux/file.h`
- Added `#define ARIA_MIN_FD 6` (documentation only)

### VTE Modifications

**File**: `src/spawn.hh`
- Extended `m_fd_map` to include 6 streams (was 3)

**File**: `src/spawn.cc`
- Opens `/dev/null` for FDs 3-5
- Maps them in child processes

### Bash Modifications

**File**: `variables.c`
- Binds `STDDBG`, `STDDATI`, `STDDATO` in `initialize_shell_variables()`

---

## Future Enhancements

### VTE Terminal (Planned)
- Split pane for FD 3 (debug messages)
- Pipe FD 4/5 to files or sockets
- Configuration UI for stream routing

### System Integration (Planned)
- systemd aria-activator service
- Stream multiplexing daemon
- Cross-process stream routing

### Developer Tools (Planned)
- Six-stream aware utilities (grep, sed, awk)
- IDE integration
- Debugging tools with stream support

---

## Troubleshooting

### Kernel won't boot
- Boot into stock kernel (hold Shift at boot, select Ubuntu kernel)
- Check logs: `journalctl -b -1 -k`
- Verify kernel installed: `dpkg -l | grep linux-image | grep ariax`

### Terminal doesn't show six streams
- Verify VTE installed: `ls -l /usr/local/lib/x86_64-linux-gnu/libvte-2.91.so*`
- Restart desktop session (logout/login)
- Check with test: `./test_vte_streams`

### Bash variables not set
- Verify AriaX Bash: `/usr/local/bin/bash --version`
- Source config: `source ~/.bashrc_ariax`
- Check directly: `/usr/local/bin/bash -c 'echo $STDDBG'`

---

## Credits

**AriaX Linux** is part of the **Alternative Intelligence Liberation Platform (AILP)**

Developed by: Randy Hoggard  
Kernel Mods: Based on Linux 6.8.0  
VTE Mods: Based on VTE 0.76.0  
Bash Mods: Based on GNU Bash 5.2  

---

## License

AriaX modifications are released under GPL v3.  
Base components retain their original licenses (Linux: GPL, VTE: LGPL, Bash: GPL).

---

## Links

- AILP Website: [https://ailp.ai](https://ailp.ai)
- Documentation: [https://docs.ailp.ai/ariax](https://docs.ailp.ai/ariax)
- Source Code: [GitHub Repository]
- Support: [Community Forums]

---

**Welcome to the future of I/O.**  
**Welcome to AriaX.**
