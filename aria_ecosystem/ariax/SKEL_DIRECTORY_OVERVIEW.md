# AriaX /etc/skel Directory Structure

## Overview

The `/etc/skel` directory is perfectly set up for new AriaX users, combining Linux Mint defaults with AriaX-specific enhancements and productivity tools.

## Directory Structure

```
/etc/skel/
├── Standard Linux Mint Directories
│   ├── Desktop/
│   ├── Documents/
│   ├── Downloads/
│   ├── Music/
│   ├── Pictures/
│   ├── Public/
│   ├── Templates/
│   └── Videos/
│
├── Configuration Files
│   ├── .bashrc              # Standard bash config
│   ├── .bashrc_ariax        # AriaX six-stream extensions
│   ├── .bash_logout
│   ├── .profile
│   ├── .gtkrc-2.0
│   └── .gtkrc-xfce
│
├── AriaX Additions
│   ├── Workspace/           # ⭐ NEW: Project creation hub
│   │   ├── mkwrk -> /usr/local/bin/mkwrk
│   │   ├── mkweb -> /usr/local/bin/mkweb
│   │   ├── mkpye -> /usr/local/bin/mkpye
│   │   └── README.md        # Comprehensive tool documentation
│   │
│   ├── .scripts/            # ⭐ NEW: User productivity scripts
│   │   ├── my_notes.sh      # Opens notes file (Ctrl+N binding)
│   │   └── xkill.sh         # Kill unresponsive windows (Ctrl+K binding)
│   │
│   └── .my_notes            # Personal notes file
│
├── Desktop Environment Config
│   ├── .config/
│   │   └── caja/
│   │       └── desktop-metadata
│   │
│   └── .local/
│       └── share/
│           ├── applications/
│           └── ice/        # Web app integration
│
└── SSH
    └── .ssh/
```

## AriaX-Specific Features

### 1. Six-Stream Bash Integration (.bashrc_ariax)

**Automatically sourced on login**, provides:

- **Environment Variables**:
  - `$STDDBG` = /dev/fd/3 (debug stream)
  - `$STDDATI` = /dev/fd/4 (data input stream)
  - `$STDDATO` = /dev/fd/5 (data output stream)

- **Helper Functions**:
  ```bash
  dbg "message"        # Write to debug stream (FD 3)
  data_out "data"      # Write to data output (FD 5)
  result=$(data_in)    # Read from data input (FD 4)
  ```

- **Custom Prompt**: Shows "AriaX" in cyan with current directory
  ```
  AriaX:~/Documents$
  ```

- **Welcome Banner**: Beautiful ASCII art banner showing:
  - Six-stream FD layout
  - Available helper functions
  - Installed AriaX tools
  - Documentation location

### 2. Workspace Directory

**Purpose**: Central hub for project creation with pre-configured tools

**Tools Available**:
- **mkwrk**: General project creator with git/Python support
- **mkweb**: ShaggyJS web framework setup
- **mkpye**: Python virtual environment activator

**Documentation**: 175-line comprehensive README.md with:
- Usage examples for each tool
- Quick start guide
- Workflow combinations
- Tips and best practices

### 3. Productivity Scripts (.scripts/)

**my_notes.sh** (37 bytes):
```bash
#!/usr/bin/env bash
xed ~/.my_notes
```
- Opens personal notes file in xed text editor
- Recommended keybinding: **Ctrl+N**
- Quick access to scratchpad/TODO list

**xkill.sh** (27 bytes):
```bash
#!/usr/bin/env bash
xkill
```
- Launches xkill to terminate unresponsive windows
- Recommended keybinding: **Ctrl+K**
- Essential troubleshooting tool

## Recommended Keyboard Shortcuts

Set these in Linux Mint's Keyboard Settings:

| Shortcut | Command | Description |
|----------|---------|-------------|
| **Ctrl+N** | `~/.scripts/my_notes.sh` | Open notes file |
| **Ctrl+K** | `~/.scripts/xkill.sh` | Kill frozen windows |

### Setting Up Shortcuts (Cinnamon)

1. Open System Settings → Keyboard → Shortcuts
2. Click "Custom Shortcuts"
3. Add new shortcuts:
   - Name: "My Notes", Command: `~/.scripts/my_notes.sh`, Shortcut: Ctrl+N
   - Name: "XKill", Command: `~/.scripts/xkill.sh`, Shortcut: Ctrl+K

## New User Experience

When a new user account is created:

1. **Home directory populated** with all /etc/skel contents
2. **First login** shows AriaX welcome banner with:
   - Six-stream architecture explanation
   - Available helper functions
   - Installed tools overview
3. **Workspace directory ready** with project creation tools
4. **Scripts executable** and ready for keybindings
5. **Notes file created** (empty, ready to use)

## Comparison with Standard Linux Mint

### Standard Mint Directories ✅
- Desktop, Documents, Downloads, Music, Pictures, Public, Templates, Videos
- Standard .bashrc, .profile, .bash_logout
- GTK configuration files
- .config and .local directories
- .ssh directory

### AriaX Enhancements ⭐
- **.bashrc_ariax** - Six-stream I/O support
- **Workspace/** - Project creation tools with documentation
- **.scripts/** - Productivity helper scripts
- **.my_notes** - Personal scratchpad file
- **AriaX tooling** - Pre-installed six-stream utilities

## Benefits

### For Developers
- **Instant productivity**: Project creators ready to use
- **Consistent workflow**: All projects follow same structure
- **Six-stream aware**: Helper functions for advanced I/O

### For Users
- **Quick notes**: Ctrl+N for instant scratchpad
- **Window management**: Ctrl+K to kill frozen apps
- **Professional setup**: Developer tools pre-configured

### For System Admins
- **Reproducible**: Every user gets same excellent setup
- **Documented**: README explains all tools
- **Maintainable**: Symlinks to central binaries

## Philosophy

This /etc/skel configuration embodies:

1. **Minimal but powerful**: Small scripts, big productivity gains
2. **User-friendly**: Clear documentation, intuitive structure
3. **Developer-focused**: Tools for rapid project initialization
4. **AriaX-native**: Six-stream I/O integrated from day one
5. **Mint-compatible**: Respects default structure, adds enhancements

## Files Summary

| File/Directory | Size | Purpose |
|---------------|------|---------|
| .bashrc_ariax | 2.9K | Six-stream bash integration |
| Workspace/README.md | 3.8K | Tool documentation |
| .scripts/my_notes.sh | 37 B | Notes launcher |
| .scripts/xkill.sh | 27 B | Window killer |
| .my_notes | 1 B | Empty notes file |

**Total AriaX additions**: ~7 KB of pure productivity

## Conclusion

The /etc/skel directory successfully combines:
- ✅ Linux Mint defaults (familiar, tested)
- ✅ AriaX six-stream architecture (innovative, powerful)
- ✅ Developer productivity tools (mkwrk, mkweb, mkpye)
- ✅ User convenience scripts (notes, xkill)

**Result**: New users get a professional, productive, AriaX-native environment from their first login.

---

**Status**: 🎯 Production-ready for AriaX v1.0.0 ISO
