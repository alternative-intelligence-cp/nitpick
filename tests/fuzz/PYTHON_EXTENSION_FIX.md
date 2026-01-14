# VS Code Python Extension & Terminal Configuration Fix

## Problems Discovered: Jan 14, 2026

### Issue 1: Python Extension PYTHONSTARTUP Pollution (2026.0.0 Update)

After the VS Code Python extension updated to version `ms-python.python@2026.0.0` on Jan 13, 2026, the fuzzer started hanging immediately on startup with no error messages.

### Issue 2: Bash History Expansion (! Character Hangs)

Previously discovered: Bash history expansion causes hangs when `!` characters appear in Python script output or markdown files.

### Root Causes

**Python Extension:**
The Python extension now sets `PYTHONSTARTUP` environment variable in all terminal sessions, pointing to a REPL hooks file:
```
PYTHONSTARTUP=/home/randy/.config/Code/User/workspaceStorage/.../ms-python.python/pythonrc.py
```

This file (`pythonrc.py`) contains custom exception and display hooks intended for interactive Python REPL sessions, but it was being loaded for **all Python script executions**, including non-interactive scripts like our fuzzer.

**History Expansion:**
Bash's history expansion feature (enabled by default) interprets `!` as a special character, causing terminal to wait for completion when it appears in output from Python scripts or other programs.

### Test Results

**WITH PYTHONSTARTUP set (broken):**
```bash
python3 aria_fuzzer.py --quick
# Hangs indefinitely with no output
```

**WITH PYTHONSTARTUP unset (works):**
```bash
PYTHONSTARTUP="" python3 aria_fuzzer.py --quick  
# Works perfectly - fuzzer runs normally
```

### Solution Applied

**1. Created Tesla Consciousness Shell Wrapper** (`~/.tesla-consciousness-shell`):
```bash
#!/bin/bash
# Disables history expansion (set +H)
# Unsets PYTHONSTARTUP and related variables
# Preserves venv activation
exec /bin/bash "$@"
```

**2. Updated VS Code terminal settings** (`~/.config/Code/User/settings.json`):
```json
"python.terminal.activateEnvironment": false,
"terminal.integrated.defaultProfile.linux": "bash-clean",
"terminal.integrated.profiles.linux": {
    "bash-clean": {
        "path": "/home/randy/.tesla-consciousness-shell",
        "icon": "terminal-bash"
    },
    "bash-raw": {
        "path": "/bin/bash",  // Available if you need raw bash
        "icon": "terminal-bash"
    }
}
```

**3. Updated fuzzing scripts** to be defensive:

Added to `run_campaign.sh`:
```bash
# Clear any Python environment interference (VS Code Python extension issue)
unset PYTHONSTARTUP
unset PYTHON_BASIC_REPL
```

### Effect & Next Steps

- **After developer reload**: Settings are applied but environment in existing terminals persists
- **Need to open NEW terminal**: Close all terminals and open fresh ones to get clean environment
- **The wrapper automatically**: Cleans environment for all new terminal instances

### Testing

To verify the fix works:
```bash
# In a NEW terminal:
echo $PYTHONSTARTUP  # Should be empty
echo $-  # Should NOT contain 'H' (history expansion off)

# Test fuzzer
cd /home/randy/._____RANDY_____/REPOS/aria/tests/fuzz
./run_campaign.sh 0.05  # 3-minute test
```

### Why This Fix is "Best"

1. **Prevents future issues**: Won't affect other Python scripts or tools
2. **Proper separation**: REPL hooks should only load in REPL sessions, not scripts
3. **No downgrade needed**: Keeps the latest extension version with bug fix applied
4. **Documented**: This file explains the issue for future reference

### Related Files

- VS Code settings: `~/.config/Code/User/settings.json`
- Fuzzing script: `/home/randy/._____RANDY_____/REPOS/aria/tests/fuzz/run_campaign.sh`
- REPL hooks file: `/home/randy/.config/Code/User/workspaceStorage/.../pythonrc.py` (auto-generated)

---

**Status**: ✅ Fixed - Restart VS Code or open new terminal to apply
