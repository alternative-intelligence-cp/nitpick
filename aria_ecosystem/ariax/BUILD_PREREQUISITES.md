# AriaX Build Prerequisites Checklist

**Last Checked**: December 27, 2025  
**System**: Ubuntu 24.04 LTS (Noble Numbat)  
**Target**: Custom Kernel 6.8.0 + ISO Build

---

## ✅ INSTALLED - Kernel Build Tools

### Core Build Environment
- ✅ **gcc** - `/usr/bin/gcc` - C compiler
- ✅ **make** - `/usr/bin/make` - Build automation
- ✅ **build-essential** - Meta-package for essential build tools
- ✅ **bison** - Parser generator for kernel build
- ✅ **flex** - Lexical analyzer for kernel build
- ✅ **bc** - Basic calculator (required for kernel scripts)

### Kernel-Specific Dependencies
- ✅ **libelf-dev** - ELF library development files (for BPF, BTF)
- ✅ **libelf1t64** - ELF runtime library
- ✅ **libssl-dev** - SSL development files (for module signing)
- ✅ **libncurses-dev** - Terminal UI library (for `make menuconfig`)
- ✅ **fakeroot** - Simulate superuser for packaging

---

## ✅ INSTALLED - ISO/Distribution Build Tools

### ISO Creation
- ✅ **cubic** - `/usr/bin/cubic` - Custom Ubuntu ISO Creator (GUI tool)
- ✅ **xorriso** - `/usr/bin/xorriso` - ISO 9660 filesystem tool (replaces mkisofs)
- ✅ **genisoimage** - `/usr/bin/genisoimage` - Creates ISO images
- ✅ **squashfs-tools** - `/usr/bin/mksquashfs` - Compress filesystem for live CD

---

## ✅ ALL PREREQUISITES INSTALLED

### Distribution Creation
- ✅ **debootstrap** - Creates minimal Debian/Ubuntu base system
  - **Status**: Installed December 27, 2025
  - **Why needed**: Cubic uses debootstrap to create the base filesystem when customizing Ubuntu ISOs

---

## ✅ KERNEL SOURCE - Ready

### Linux 6.8.0 Source
- ✅ **Tarball**: `kernel_build/linux_6.8.0.orig.tar.gz` (220MB)
- ✅ **Extracted**: `kernel_build/linux-6.8.0/` (full source tree)
- ✅ **Debian Patches**: `kernel_build/linux_6.8.0-90.91.diff.gz`
- ✅ **Debian Control**: `kernel_build/linux_6.8.0-90.91.dsc`

**Status**: Source is ready for patching and compilation.

---

## ✅ BASE ISO - Ready

### Ubuntu 24.04 LTS ISO
- ✅ **Downloaded**: Ubuntu 24.04.x LTS (Noble Numbat) ISO
- ✅ **Location**: Stored outside repo (excluded from git due to size ~4-5GB)
- ✅ **Purpose**: Base image for Cubic customization

**Status**: ISO ready for Cubic import.

---

## ✅ OPTIONAL BUT RECOMMENDED - All Installed

### Virtualization Stack (For Safe Testing)
- ✅ **qemu-system-x86_64** - `/usr/bin/qemu-system-x86_64` (version 8.2.2)
- ✅ **libvirt** - `/usr/bin/virsh` (version 10.0.0)
- ✅ **virt-manager** - `/usr/bin/virt-manager` (GUI VM manager)

**Status**: Full virtualization stack ready for testing AriaX kernel/ISO in VMs.

### Development Tools (Optional)
- 🔶 **kernel-package** - Debian kernel packaging (alternative to `make deb-pkg`)
  - Not strictly required - can use `make bindeb-pkg` instead

- 🔶 **devscripts** - Debian packaging scripts
  - Helpful for `debuild`, `dch`, etc.

- ✅ **grub2-common** - GRUB bootloader (for ISO boot)
  - Already installed on Ubuntu

---

## 📊 DISK SPACE REQUIREMENTS

### Current Status
Check available space:
```bash
df -h /home/randy/._____RANDY_____/REPOS/ariax
```

### Space Needed
- **Kernel Build**: ~10-15 GB (includes build artifacts, modules, debug symbols)
- **Cubic Workspace**: ~8-12 GB (temporary filesystem, squashfs)
- **Final ISO**: ~2-4 GB (depends on packages included)
- **Total Recommended**: 30+ GB free space

---

## 🔧 INSTALLATION COMMAND

To install all missing prerequisites in one go:

```bash
sudo apt-get update
sudo apt-get install -y \
    debootstrap \
    kernel-package \
    devscripts \
    qemu-system-x86-64 \
    linux-headers-$(uname -r)
```

---

## ✅ BUILD PROCESS READINESS

### Phase 1: Kernel Patching & Build - ✅ READY
**Prerequisites Met**: All kernel build tools installed, source extracted.

**Next Steps**:
1. Apply six-stream FD reservation patches to `fs/exec.c` and `fs/file.c`
2. Configure kernel (`.config`)
3. Build kernel: `make -j$(nproc) bindeb-pkg`
4. Install generated `.deb` packages for testing

### Phase 2: Custom ISO Creation - ✅ READY
**Prerequisites Met**: All tools installed (Cubic, xorriso, genisoimage, squashfs-tools, debootstrap).  
**Base ISO Available**: Ubuntu 24.04 LTS downloaded.

**Next Steps**:
1. Launch Cubic GUI (`cubic`)
2. Point to Ubuntu 24.04 ISO
3. Customize with patched kernel
4. Include Aria compiler and runtime
5. Configure six-stream terminal patches
6. Generate final AriaX ISO

### Phase 3: Testing - ✅ READY
**Virtualization Stack Installed**: QEMU 8.2.2 + libvirt 10.0.0 + virt-manager available.

**Next Steps**:
1. Test patched kernel in QEMU VM
2. Test AriaX ISO in virt-manager before physical deployment
3. Verify six-stream FD reservation in VM environment

---

## 📖 REFERENCE DOCUMENTATION

### Research Available
- ✅ `docs/research/gemini/responses/task_01_kernel_implementation_guide.txt` - Kernel patching guide (29KB)
- ✅ `docs/research/gemini/responses/task_02_bash_integration.txt` - Bash integration (29KB)
- ✅ `docs/research/gemini/responses/task_03_systemd_integration.txt` - systemd integration (35KB)
- ✅ `docs/research/gemini/responses/systemToolsForHexStreamModel.txt` - System tools overview (13KB)

### Ecosystem Dependencies
- 📋 `../aria_ecosystem/INTEGRATION_MAP.md` - Cross-repo integration requirements
- 📋 `../aria_ecosystem/.internal/research/responses/04_WindowsBootstrap.txt` - FD reservation spec
- 📋 `../aria_ecosystem/.internal/research/responses/06_TerminalProbe.txt` - Terminal patches
- 📋 `../aria_ecosystem/.internal/research/responses/07_TelemetryDaemon.txt` - Telemetry service

---

## 🎯 READY TO BUILD

### All Prerequisites Complete ✅
1. ✅ **debootstrap installed** (December 27, 2025)
2. ✅ **Virtualization stack ready** (QEMU + libvirt + virt-manager)
3. ✅ **Ubuntu 24.04 ISO downloaded**
4. ✅ **All kernel build tools installed**

### Next Actions

1. **Verify disk space** (need 30+ GB free):
   ```bash
   df -h /home/randy/._____RANDY_____/REPOS/ariax
   ```

2. **Test kernel toolchain** (optional verification):
   ```bash
   cd kernel_build/linux-6.8.0
   make defconfig
   make -j$(nproc) bzImage  # Test compile (doesn't build full kernel)
   ```

3. **Begin kernel patching**:
   - Follow `docs/research/gemini/responses/task_01_kernel_implementation_guide.txt`
   - Apply six-stream FD reservation patches to `fs/exec.c` and `fs/file.c`
   - Build and test in QEMU VM

---

## ✅ SUMMARY

**Status**: 100% Ready for Build 🚀

- ✅ Kernel build tools: Complete (gcc, make, bison, flex, libelf-dev, libssl-dev, etc.)
- ✅ Kernel source: Extracted and ready (Linux 6.8.0)
- ✅ ISO creation tools: Complete (Cubic, xorriso, genisoimage, squashfs-tools, debootstrap)
- ✅ Base ISO: Ubuntu 24.04 LTS downloaded
- ✅ Virtualization: QEMU + libvirt + virt-manager ready for testing
- ✅ Disk space: Verify with `df -h` before starting

**You can now**:
1. ✅ Patch kernel with six-stream FD reservation code
2. ✅ Build custom kernel packages (`make bindeb-pkg`)
3. ✅ Test patched kernel in QEMU VM
4. ✅ Create AriaX distribution with Cubic
5. ✅ Test full ISO in virt-manager before deployment

**No blockers remaining. Ready to start development!** 🎉
