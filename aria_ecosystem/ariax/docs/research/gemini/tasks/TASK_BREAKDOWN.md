# AriaX OS Distribution - Gemini Research Task Breakdown
**Generated**: December 19, 2025
**Source**: gemini_gap_todo.txt (AriaX section) architectural audit

## Overview

This document breaks down the AriaX OS kernel and distribution gaps into 7 discrete, actionable research tasks. Each task addresses a specific technical challenge in implementing the Aria Six-Stream Topology at the OS level.

The AriaX project requires modifications to:
- Linux kernel 6.8 (VFS and process execution)
- systemd (socket activation compatibility)
- Terminal emulators (file descriptor preservation)
- Desktop environment (Cinnamon on Ubuntu 24.04)
- Development toolchain (LLVM 20+, editors)

---

## TODO 1: Verify Terminal Emulator FD Preservation

**Priority**: CRITICAL (Can break kernel work)
**Estimated Complexity**: Medium
**Dependencies**: None (research task)

### Problem Statement
Terminal emulators like gnome-terminal (via libvte) may aggressively close file descriptors 3-5 when spawning shells to prevent FD leaks from the GUI process. If the terminal closes these FDs before the shell starts, the kernel's aria_ensure_streams patch may be ineffective, leaving the shell without stddbg/stddati/stddato.

Research indicates VTE may use close_range() to close all FDs above 2. This would defeat the six-stream topology.

### Required Context Files

**From ariax repository**:
1. `docs/research/gemini/responses/research_033_kernel_bash.txt` - Kernel integration design
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (Section 6)

**From aria repository** (for context):
1. `docs/info/aria_specs.txt` - Six-stream contract specification
2. `docs/info/six_stream_design.md` - If exists

**External Research Needed**:
- VTE library source for Ubuntu 24.04 (libvte-2.91)
- gnome-terminal source
- alacritty configuration documentation

### Gemini Prompt

```
Analyze the file descriptor handling behavior of terminal emulators on Ubuntu 24.04 specifically regarding the spawning of child shells. The AriaX OS project requires file descriptors 3, 4, and 5 to remain open (mapped to /dev/null or appropriate streams) when a shell starts.

Context: The Linux kernel has been patched to preserve these FDs during exec() (see research_033_kernel_bash.txt), but terminal emulators may close them in the parent process before fork/exec.

Research Tasks:
1. Analyze vte_pty_spawn_async in libvte-2.91 source:
   - Does it call close_range() or equivalent?
   - What is the range of FDs closed?
   - Is there a configuration option to preserve specific FDs?

2. Examine gnome-terminal (or gnome-console on 24.04):
   - What VTE version does it use?
   - Does it add additional FD closing beyond VTE?
   - Can we configure it via GSettings to modify this behavior?

3. Research alternative terminals:
   - Does alacritty preserve FDs 3-5?
   - Does kitty or wezterm have configurable FD handling?
   - What about xterm (legacy but simple)?

4. Provide solutions:
   - If VTE closes FDs 3-5: Create a patch specification to exempt these FDs
   - If configurable: Document the configuration method
   - If unfixable: Recommend a shell wrapper that reopens them immediately

Deliverable: A technical report with source code references, VTE version numbers, and either a patch or a workaround strategy.
```

### Expected Deliverables
- Technical report: "VTE_FD_ANALYSIS.md"
- If patch needed: "vte-preserve-aria-streams.patch"
- If workaround: Shell wrapper script and integration guide
- Recommendation for default AriaX terminal emulator

---

## TODO 2: VS Code Global Extension Deployment Strategy

**Priority**: MEDIUM (User experience)
**Estimated Complexity**: Low
**Dependencies**: None

### Problem Statement
VS Code stores extensions in user home directories (~/.vscode/extensions), making it difficult to pre-install them globally in a custom ISO. Multiple strategies exist (bootstrap folder, skeleton directory, global installation), but their effectiveness on Ubuntu 24.04 is unclear.

Goal: Determine the canonical method to pre-install Aria language extensions such that every new user created post-install has them available without internet access.

### Required Context Files

**From ariax repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 2)

**External Research**:
- VS Code documentation on extension deployment
- Ubuntu 24.04 /etc/skel best practices

### Gemini Prompt

```
Develop a robust strategy for pre-installing Visual Studio Code extensions in a custom Ubuntu 24.04 ISO such that all users created post-installation have these extensions available immediately without requiring internet access or manual installation.

Context: The AriaX distribution needs to pre-install:
- Aria language server extension (.vsix file, custom)
- clangd (C/C++ LSP)
- lldb-dap (debugger adapter)

Requirements:
1. Research the following methods and rank by reliability:
   
   **Method A: Skeleton Directory**
   - Copy .vsix files to /etc/skel/.vscode/extensions/
   - Set correct permissions (what user:group?)
   - Verify VS Code recognizes them on first run
   
   **Method B: Bootstrap Script**
   - Place .vsix in /usr/share/ariax/vscode/
   - Create /etc/profile.d/ariax-vscode-init.sh
   - Script checks if ~/.vscode/extensions exists
   - If not, run: code --install-extension <path> for each extension
   - Test if this works for headless users (sudo adduser)
   
   **Method C: System Extensions Directory**
   - Research if VS Code supports /usr/share/code/extensions/
   - Test on Ubuntu 24.04 with .deb installation
   - Document any permission issues
   
   **Method D: Extension Marketplace Override**
   - Can we modify product.json to point to a local extension repo?
   - Overkill but ensures updates work

2. Provide a concrete implementation:
   - Bash script for Cubic chroot environment
   - File permissions and ownership requirements
   - Verification command to test in chroot
   
3. Handle edge cases:
   - What if user has VS Code Insiders?
   - What if user installs Codium (open-source VS Code)?
   - How to handle extension updates (user can update normally?)

Deliverable: A complete bash script named "install-vscode-extensions-global.sh" with detailed comments, plus a test procedure.
```

### Expected Deliverables
- `scripts/cubic/install-vscode-extensions-global.sh`
- Test procedure document
- Fallback strategy if primary method fails

---

## TODO 3: LLVM 20 Snapshot ABI Stability

**Priority**: HIGH (Affects compiler)
**Estimated Complexity**: Medium
**Dependencies**: None

### Problem Statement
AriaX requires LLVM 20+, which is currently a development snapshot with unstable ABI. The `ariac` compiler links against LLVM libraries. If the system updates LLVM 20 via apt (pulling a newer snapshot), the ABI may change, breaking the compiler.

### Required Context Files

**From aria repository**:
1. CMakeLists.txt - LLVM linking configuration
2. `docs/building/LLVM_INTEGRATION.md` - If exists

**From ariax repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 3)

### Gemini Prompt

```
The AriaX distribution includes the Aria compiler (ariac) which links against LLVM 20+ libraries. LLVM 20 is a development snapshot with no ABI stability guarantee. Evaluate strategies to ensure ariac remains functional across LLVM 20 updates from the apt.llvm.org repository.

Context: The Aria compiler is built in the ISO creation phase using LLVM 20. Users may later run apt-get upgrade, which could pull a newer LLVM 20 snapshot with breaking ABI changes.

Research and compare:

**Strategy 1: Static Linking**
1. Configure CMake to statically link all LLVM libraries
2. Command: cmake -DLLVM_LINK_LLVM_DYLIB=OFF (or similar)
3. Pros: ariac binary is self-contained, immune to system LLVM changes
4. Cons: Large binary size (~200MB?), longer compile time
5. Feasibility: Does LLVM 20 provide static libraries? Any licensing concerns?

**Strategy 2: Version Pinning**
1. Use apt preferences to pin llvm-20 package to specific version
2. File: /etc/apt/preferences.d/llvm-pin
3. Content: Package: llvm-20* Pin: version 1:20.0.0~... Pin-Priority: 1001
4. Pros: Simple, small binary
5. Cons: User loses security updates, manual maintenance

**Strategy 3: Vendored LLVM**
1. Build LLVM 20 from source during ISO creation
2. Install to /opt/llvm-aria/ (separate from system)
3. ariac uses RPATH to link to vendored LLVM
4. Pros: Complete control, stable ABI
5. Cons: Massive ISO size increase, long build time

**Strategy 4: AppImage or Snap**
1. Distribute ariac as AppImage with bundled LLVM
2. Isolate from system package updates
3. Pros: Clean separation
4. Cons: Startup overhead, complexity

Recommendation: Provide detailed analysis with:
- CMake flags for static linking LLVM
- Binary size comparison (static vs dynamic)
- Patch for ariac CMakeLists.txt if needed
- Test procedure to verify ABI isolation
```

### Expected Deliverables
- Technical report: "LLVM_STABILITY_STRATEGY.md"
- CMakeLists.txt modifications for static linking
- Or: apt preferences file for version pinning
- Verification script to test ABI breakage

---

## TODO 4: Cinnamon Desktop Default Session Automation

**Priority**: MEDIUM (User experience)
**Estimated Complexity**: Low
**Dependencies**: None

### Problem Statement
Ubuntu 24.04 uses GDM3 and AccountsService to manage desktop sessions. Simply installing cinnamon-desktop-environment doesn't make it the default for new users. The session may default to non-existent GNOME (if removed) or back to Ubuntu session, causing login failures or confusion.

### Required Context Files

**From ariax repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 4)

**External Research**:
- GDM3 configuration on Ubuntu 24.04
- AccountsService user template documentation

### Gemini Prompt

```
Determine the precise configuration method to set Cinnamon as the default desktop session for all new users on Ubuntu 24.04 LTS using GDM3 display manager. This must work in a Cubic ISO build environment.

Context: The AriaX ISO build process:
1. Installs cinnamon-desktop-environment
2. Removes ubuntu-desktop and gnome-shell (optional)
3. Needs to ensure new users (created by installer or useradd) default to Cinnamon

Research the following mechanisms:

**Method 1: GDM3 Configuration**
- File: /etc/gdm3/custom.conf
- Or: /etc/gdm3/greeter.dconf-defaults
- Can we set DefaultSession=cinnamon.desktop?
- Test if this affects new users or just the greeter default

**Method 2: AccountsService Template**
- Directory: /var/lib/AccountsService/users/
- Understand the template mechanism (if any)
- Create a default profile that sets XSession=cinnamon

**Method 3: Skeleton Directory**
- File: /etc/skel/.dmrc (deprecated?)
- Or: /etc/skel/.xsession
- Or: /etc/skel/.config/autostart/ (wrong level)
- Determine if .dmrc is still respected in 24.04

**Method 4: Update Alternatives**
- Command: update-alternatives --set x-session-manager /usr/bin/cinnamon-session
- Test if this sets system-wide default

Requirements:
1. Must work for users created by Subiquity installer
2. Must work for users created by sudo adduser
3. Must survive system updates
4. Must not break if user manually changes session later

Provide:
- Exact file paths and content
- Bash script to apply configuration in Cubic chroot
- Test procedure (how to verify default is set)
```

### Expected Deliverables
- Configuration files (gdm3, AccountsService, or skel)
- Bash script: "set-cinnamon-default.sh"
- Test procedure document

---

## TODO 5: Systemd Shim "Overlap" Edge Case Testing

**Priority**: MEDIUM (Correctness)
**Estimated Complexity**: Low
**Dependencies**: None (testing task)

### Problem Statement
The aria-activator shim relocates systemd-provided file descriptors from positions 3+ to positions 6+. When LISTEN_FDS > 3, the source and destination ranges overlap (e.g., moving FDs 3,4,5,6,7 to 6,7,8,9,10). The shim uses backward iteration to avoid corruption, but this logic needs stress testing.

### Required Context Files

**From ariax repository**:
1. `docs/research/gemini/responses/research_033_kernel_bash.txt` - Shim design
2. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 5)

### Gemini Prompt

```
Write a comprehensive C unit test to validate the file descriptor relocation logic of the aria-activator shim, particularly testing the edge case where source and destination FD ranges overlap.

Context: The shim receives N file descriptors from systemd starting at FD 3 (e.g., FDs 3,4,5,6,7 for N=5). It must relocate them to start at FD 6 (resulting in FDs 6,7,8,9,10), freeing up FDs 3,4,5 for Aria's stddbg/stddati/stddato.

The overlap occurs when N > 3:
- FD 3 → FD 6 (no conflict)
- FD 4 → FD 7 (no conflict)
- FD 5 → FD 8 (no conflict)
- FD 6 → FD 9 (conflict: source FD 6 was just created as destination!)
- FD 7 → FD 10 (same issue)

The design iterates backward (start from highest FD) to avoid overwriting.

Test Requirements:
1. Simulate systemd's FD passing:
   - Create N pipe file descriptors at positions 3 through 3+N-1
   - Write unique data to each pipe's write end
   - Close write ends
   
2. Implement shim logic:
   - Iterate backward: for i = N-1 down to 0
   - dup2(3+i, 6+i)
   - close(3+i)
   
3. Validate results:
   - Read from FDs 6 through 6+N-1
   - Verify data matches what was written
   - Ensure FDs 3,4,5 are closed (return EBADF on read)
   
4. Test cases:
   - N=1 (single socket, no overlap)
   - N=3 (edge of overlap)
   - N=5 (overlap at FDs 6,7)
   - N=10 (large overlap)

Provide:
- Complete C program (can compile standalone)
- Use assert() for validation
- Print diagnostic info for each test case
- Test on Linux system
```

### Expected Deliverables
- `tests/shim/test_fd_relocation.c` - Unit test
- Makefile or compile command
- Test output showing all cases passing

---

## TODO 6: Aria Debug Adapter Protocol Implementation

**Priority**: LOW (Future feature)
**Estimated Complexity**: Very High
**Dependencies**: Requires debugger implementation in aria

### Problem Statement
The AriaX distribution pre-configures editors (VS Code, Neovim) with debug support via Debug Adapter Protocol (DAP). However, the actual ariadbg debugger and its DAP implementation are not yet specified. This task defines the protocol mapping for Aria-specific features.

### Required Context Files

**From aria repository**:
1. `docs/info/aria_specs.txt` - Language features (TBB types, wild pointers)
2. `docs/runtime/MEMORY_MODEL.md` - If exists
3. Any existing debugger design docs

**From ariax repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 6)

**External**:
- DAP specification: https://microsoft.github.io/debug-adapter-protocol/

### Gemini Prompt

```
Draft a technical specification for the Aria Debug Adapter (ariadbg) defining how Aria language runtime state maps to the Debug Adapter Protocol (DAP) JSON-RPC messages.

Context: Aria has unique features not present in C/C++:
- TBB types (tbb8, tbb16, etc.) with special ERR and NaN sentinel values
- wild keyword for opt-out garbage collection
- Six-stream I/O topology
- Memory model with borrow checker

The debugger must expose these features through DAP to editors like VS Code and Neovim.

Specification Requirements:

**1. DAP Messages to Implement**:
- initialize: Advertise support for Aria-specific features
- launch/attach: Start debugging an Aria binary (via lli or native)
- setBreakpoints: File/line breakpoints
- continue, next, stepIn, stepOut: Standard stepping
- stackTrace: Show call stack with Aria function names
- scopes: Local variables, globals, this (if applicable)
- variables: Retrieve variable values (critical for TBB)
- evaluate: REPL-like expression evaluation

**2. Aria-Specific Mappings**:

**Variables Request**:
- For tbb8 variable, return:
  ```json
  {
    "name": "x",
    "value": "tbb8: 42",
    "type": "tbb8",
    "variablesReference": 0
  }
  ```
- If TBB holds ERR sentinel, display as:
  ```json
  {
    "name": "x",
    "value": "tbb8: ERR (sentinel)",
    "type": "tbb8",
    "variablesReference": 0
  }
  ```
- For wild pointers, show:
  ```json
  {
    "name": "ptr",
    "value": "0x7f... (wild, no borrow check)",
    "type": "*int32",
    "variablesReference": 0
  }
  ```

**Evaluate Request**:
- Support Aria expression syntax
- Handle TBB operations (wrapping, error propagation)
- Return result with correct type

**Output Events**:
- Map Aria's six streams to DAP categories:
  - stdout → "stdout"
  - stderr → "stderr"
  - stddbg → "console" (with special marker?)
  - stddati/stddato → custom category? (may not fit DAP)

**3. Implementation Plan**:
- What protocol transport? (stdio, TCP, named pipe)
- Threading model (DAP server on separate thread?)
- State synchronization with Aria runtime/VM
- Breakpoint injection mechanism (LLVM JIT modification?)

Deliverable:
- Formal specification document: "ARIA_DAP_SPEC.md"
- JSON schema for Aria-specific extension messages
- Pseudocode for Variables and Evaluate handlers
- High-level architecture diagram (components, data flow)
```

### Expected Deliverables
- `docs/debugger/ARIA_DAP_SPEC.md` - Formal specification
- JSON schemas for messages
- Architecture diagram
- Proof-of-concept implementation plan

---

## TODO 7: Terminal Emulator Display of Extra Streams

**Priority**: MEDIUM (User experience)
**Estimated Complexity**: Medium
**Dependencies**: TODO 1 (must preserve FDs first)

### Problem Statement
Standard terminal emulators only display stdout and stderr. The Aria six-stream topology adds stddbg (FD 3), stddati (FD 4), and stddato (FD 5). Users need a way to visualize stddbg output (debug messages) separately from stdout (program output) to take advantage of the six-stream design.

### Required Context Files

**From aria repository**:
1. `docs/info/aria_specs.txt` - Six-stream contract
2. `docs/info/six_stream_design.md` - If exists

**From ariax repository**:
1. `docs/research/gemini/tasks/gemini_gap_todo.txt` - Gap analysis (TODO 7)

**External Research**:
- tmux configuration and scripting
- multitail documentation
- tilix panes

### Gemini Prompt

```
Research and develop a terminal multiplexing solution that automatically displays Aria's stddbg stream (file descriptor 3) in a separate pane when running Aria programs.

Context: Aria programs write:
- stdout (FD 1): Primary program output
- stderr (FD 2): Error messages (user-facing)
- stddbg (FD 3): Debug telemetry (logs, traces, not errors)

Current problem: Standard terminals only show FD 1 and 2. FD 3 writes go nowhere unless explicitly redirected.

Requirements:
1. Develop a shell wrapper or tmux configuration that:
   - Detects when an Aria binary is executed
   - Automatically creates a split pane
   - Pipes FD 3 to the split pane
   - Maintains stdout/stderr in main pane

2. Research these approaches:

**Approach A: tmux Wrapper**
```bash
#!/bin/bash
# aria-dbg-run: Execute Aria program with split stddbg pane
FIFO=$(mktemp -u)
mkfifo $FIFO
tmux split-window -v "tail -f $FIFO"
"$@" 3>$FIFO
```
- Test if this works
- Handle cleanup (remove FIFO on exit)
- Make pane size configurable

**Approach B: Named Pipes + multitail**
- Create three FIFOs: /tmp/aria-stdout, /tmp/aria-stderr, /tmp/aria-stddbg
- Redirect Aria program: `aria-prog 1>stdout 2>stderr 3>stddbg`
- Run: `multitail -i stdout -i stderr -i stddbg`
- Challenge: Integrate with shell workflow

**Approach C: Custom Terminal Emulator**
- Fork alacritty or wezterm
- Add native support for FD 3 in separate pane
- Overkill but cleanest UX

**Approach D: Shell Integration**
- Modify bash/zsh precmd hook
- Detect Aria binaries (check ELF note: .note.aria.properties)
- Automatically set up redirection
- Example: `exec 3> >(tee /dev/tty3)` (doesn't split, just displays)

3. Provide:
- Working wrapper script
- Integration with AriaX shell (add to /etc/profile.d/)
- Documentation for users
- Optional: man page for aria-dbg-run command

Bonus: Colorize output (stdout=white, stderr=red, stddbg=cyan)
```

### Expected Deliverables
- `scripts/aria-dbg-run` - Wrapper script
- tmux or terminal configuration
- Integration guide for /etc/profile.d/
- User documentation

---

## Task Dependency Graph (AriaX)

```
TODO 1 (Terminal FD) ──→ TODO 7 (Display Streams)
        ↓
    [Kernel Work]
        ↓
TODO 5 (Shim Testing)


TODO 2 (VS Code) ──┐
TODO 3 (LLVM)     ├──→ [ISO Build]
TODO 4 (Cinnamon) ─┘


TODO 6 (DAP Spec) ──→ [Future Debugger Work]
```

**Critical Path**: TODO 1 → [Kernel implementation] → TODO 5 → TODO 7
**Independent**: TODO 2, 3, 4 (can be done in parallel, only affect ISO)
**Future Work**: TODO 6 (requires debugger, not blocking)

---

## Implementation Priority Order

### Phase 1: Research (Week 1)
1. **TODO 1**: Terminal FD preservation (must know if kernel approach works)
2. **TODO 3**: LLVM stability strategy (affects compiler packaging)

### Phase 2: ISO Build (Week 2-3)
3. **TODO 4**: Cinnamon default session (quick win)
4. **TODO 2**: VS Code global extensions (user experience)

### Phase 3: Runtime (Week 4)
5. **TODO 5**: Shim testing (validate core functionality)
6. **TODO 7**: Stream display (UX for six-stream)

### Phase 4: Future (Post-MVP)
7. **TODO 6**: DAP specification (debugger not yet implemented)

---

## File Upload Checklist for Each Task

### General Files (Upload for ALL tasks)
- `docs/research/gemini/tasks/gemini_gap_todo.txt` (AriaX section)
- `docs/research/gemini/responses/research_033_kernel_bash.txt` - Core design

### From aria repository (for context)
- `docs/info/aria_specs.txt` - Six-stream contract
- Relevant to specific tasks as listed above

### Task-Specific Files
See each TODO section for specific requirements

---

## Success Criteria

Each task is complete when:
1. ✅ Research completed with source references
2. ✅ Recommendation documented with rationale
3. ✅ Implementation provided (script/patch/config)
4. ✅ Test procedure documented
5. ✅ Integration method specified (how to add to ISO)

---

## Notes for Gemini Interaction

**Kernel Tasks**:
- TODO 1 is primarily research (source code analysis)
- May need to provide VTE library source or links
- Focus on actionable outcome (patch or workaround)

**ISO Build Tasks**:
- TODO 2, 3, 4 are practical implementations
- Can test in Cubic chroot before full ISO build
- Provide bash scripts that work in automated build pipeline

**Future Work**:
- TODO 6 is design/specification, not implementation
- Useful for planning but not blocking current work
- Can be lowest priority

**Stream Display**:
- TODO 7 is about UX, not core functionality
- Multiple valid approaches, choose simplest
- Should integrate seamlessly (no manual user action)
