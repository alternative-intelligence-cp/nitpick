# AriaX Linux - Changelog

All notable changes to the AriaX Linux distribution will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [1.0.0] - 2025-01-16 - "Echo" Release

### 🎉 Initial Release

First production release of AriaX Linux with native kernel-level six-stream I/O support.

### Added - Kernel

#### fs/exec.c
- **aria_ensure_streams()** function (64 lines)
  - Guarantees file descriptors 0-5 are always open for every process
  - Opens /dev/null for any closed FDs in range 0-5
  - Uses spinlock for thread safety during early boot
  - Called in begin_new_exec() after do_close_on_exec()
  
#### Conservative Approach
- Only modified fs/exec.c (no fs/file.c changes)
- Avoids aggressive FD blocking that caused systemd-udevd failures in early testing
- Stable boot process with no systemd service failures

### Added - VTE Terminal Emulator

#### src/spawn.hh (line 54)
- Extended m_fd_map from 3 elements to 6
- Changed: {{-1,0},{-1,1},{-1,2}} → {{-1,0},{-1,1},{-1,2},{-1,3},{-1,4},{-1,5}}

#### src/spawn.cc (lines 415-430)
- Opens /dev/null for FDs 3-5 in child processes
- Assigns devnull_fd to m_fd_map[3-5].first
- Ensures terminal-spawned programs inherit six streams

### Added - Bash Shell

#### variables.c (after line 635)
- bind_variable("STDDBG", "/dev/fd/3", 0)
- bind_variable("STDDATI", "/dev/fd/4", 0)
- bind_variable("STDDATO", "/dev/fd/5", 0)
- Environment variables available to all shell scripts

### Added - Distribution Package

#### Utilities (22 total, 4.0 MB)
Production-ready tools from aria_utils project:

**File Operations**:
- **acat** (121 KB) - File concatenation with six-stream I/O (32 tests)
- **acp** (157 KB) - File copying utility (33 tests)
- **amv** (132 KB) - File moving utility (29 tests)

**System Tools**:
- **als** (49 KB) - Directory listing with TBB parallelism (34 tests)
- **aps** (50 KB) - Process status monitoring (29 tests)
- **astat** (63 KB) - File statistics utility

**Development Tools**:
- **abc** (110 KB) - Aria Build Configuration parser (37 tests)
- **aria_make** (521 KB) - Hermetic build system
- **ariash** (246 KB) - Six-stream interactive shell
- **aglob** (103 KB) - Pattern matching utility (25 tests)
- **depgraph** (105 KB) - Dependency graph analyzer (28 tests)
- **atest** (143 KB) - Testing framework

**Data Processing**:
- **adap** (516 KB) - Data processing utility (47 tests)
- **ajq** (405 KB) - JSON query processor
- **asql** (204 KB) - SQL interface utility
- **agrep** (146 KB) - Pattern searching
- **atar** (113 KB) - Archive management

**Network & Analysis**:
- **acurl** (89 KB) - Network requests (34 tests)
- **ascope** (198 KB) - Scope analysis
- **apic** (334 KB) - Image processing

**Test Utilities**:
- **test_six_streams** (16 KB) - Kernel six-stream verification
- **test_vte_streams** (16 KB) - VTE terminal stream verification

#### Shared Libraries (6 total, 1.1 MB)
- libabc.so.0.1.0 (156 KB) - ABC parser library
- libacurl.so.0.1.0 (104 KB) - Curl wrapper library
- libaglob.so.0.1.0 (273 KB) - Glob matching library
- libals.so.0.1.0 (135 KB) - Directory listing library
- libaps.so.0.1.0 (151 KB) - Process status library
- libdepgraph.so.0.1.0 (161 KB) - Dependency analysis library

#### Documentation
- **README.md** - Comprehensive user guide
  - Architecture overview
  - Six-stream I/O explained
  - Installation instructions
  - Utility reference
  - Troubleshooting guide
  - Development guidelines

#### Configuration
- **.bashrc_ariax** - Shell configuration
  - dbg() - Debug stream helper (writes to FD 3)
  - data_out() - Output metadata helper (writes to FD 5)
  - data_in() - Input metadata helper (reads from FD 4)
  - AriaX-themed prompt: `[AriaX]➜`
  - Welcome message with ASCII art

#### Installation
- **install.sh** (97 lines) - Automated installer
  - Root permission check
  - Utility installation to /usr/local/bin/
  - Library installation to /usr/local/lib/ariax/
  - Versioned symlink creation (*.so.X.Y.Z → *.so.X → *.so)
  - ld.so.conf.d integration (/etc/ld.so.conf.d/ariax.conf)
  - /etc/skel setup for new users
  - Existing user bashrc integration
  - ldconfig cache update
  - User-friendly progress output

### Changed

#### Build System
- Conservative kernel approach (removed aggressive FD blocking)
- VTE built with meson/ninja (clean build system)
- Bash built with traditional configure/make
- All utilities built from aria_utils/examples/*_demo executables

#### Testing Methodology
- Individual component testing (kernel, VTE, Bash)
- Integration testing (all components together)
- Production utility testing (all 22 utilities verified)
- Shared library dependency resolution testing
- End-to-end workflow verification

### Fixed

#### Kernel Boot Issues
- **Issue**: First build with fs/file.c modifications caused systemd-udevd failure
- **Fix**: Conservative approach - only aria_ensure_streams() in fs/exec.c
- **Result**: Clean boot with no systemd service failures

#### Library Dependencies
- **Issue**: Utilities failed with "cannot open shared object file" errors
- **Fix**: Added lib/ directory to distribution package
- **Fix**: Created versioned symlinks (libabc.so.0.1.0 → libabc.so.0 → libabc.so)
- **Fix**: Integrated with ld.so.conf.d (/etc/ld.so.conf.d/ariax.conf)
- **Result**: All utilities load libraries successfully

### Testing

#### Kernel Testing
- ✅ aria_ensure_streams() function verified
- ✅ All FDs 0-5 exist for every process
- ✅ No systemd boot failures
- ✅ Stable system operation

#### VTE Testing
- ✅ Six-stream setup verified (test_vte_streams)
- ✅ Terminal spawns programs with FDs 0-5 open
- ✅ No regressions in standard I/O behavior

#### Bash Testing
- ✅ STDDBG, STDDATI, STDDATO environment variables set
- ✅ Stream writing works (no "Bad file descriptor" errors)
- ✅ Helper functions available (dbg, data_out, data_in)

#### Utility Testing
- ✅ All 22 utilities execute successfully
- ✅ Shared libraries load correctly
- ✅ TBB-safe parallel processing verified
- ✅ Six-stream I/O architecture confirmed

#### Integration Testing
- ✅ End-to-end workflow: kernel → VTE → Bash → utilities
- ✅ New user setup via /etc/skel
- ✅ Existing user bashrc integration
- ✅ Library cache updates (ldconfig)

### Performance

- **Kernel**: Minimal overhead (aria_ensure_streams called once per exec)
- **VTE**: Negligible overhead (three additional FD opens per spawn)
- **Utilities**: TBB-parallel processing for high throughput
- **Libraries**: Shared library caching via ld.so.cache

### Documentation

- Comprehensive README.md with architecture, installation, and usage
- Test results document (ARIAX_V1.0_TEST_RESULTS.md)
- VERSION file with build information
- CHANGELOG (this file)

### Build Information

- **Build Date**: 2025-01-16 05:26 UTC
- **Build Host**: randy-P920 (Ubuntu 22.04.5 LTS)
- **Kernel Source**: linux-6.8.0
- **VTE Source**: vte-0.76.0
- **Bash Source**: bash-5.2
- **Utilities Source**: aria_utils (production-ready demos)

### Deployment

- **VM Testing**: Linux Mint 22.3 (192.168.122.110)
- **Package Format**: Tarball (ariax-dist-1.0.0.tar.gz, 1.4 MB)
- **Installation**: Automated via install.sh
- **Target Distros**: Ubuntu 22.04+, Debian 12+, Linux Mint 21+

---

## [Unreleased]

### Planned Features
- ISO creation for bootable installer
- Website deployment (ailp.org)
- Public release announcement
- Update mechanism for future releases
- Additional aria utilities integration
- Performance benchmarking suite

### Known Issues
None at this time.

---

## Release History

- **1.0.0** (2025-01-16) - "Echo" - Initial production release

---

**Maintained By**: Aria Echo (AILP Technical Director)  
**Project**: Alternative Intelligence Liberation Platform  
**Repository**: https://github.com/ailp/ariax  
**Website**: https://ailp.org
