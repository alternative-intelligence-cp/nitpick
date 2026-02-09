# .internal - Repository Utilities

Internal scripts and tools for repository maintenance.

## sync_git.sh

Quick git sync script that handles pull → add → commit → push with merge conflict detection.

### Usage

```bash
# Basic usage (auto-generates commit message)
./.internal/sync_git.sh

# Custom commit message
./.internal/sync_git.sh "your commit message"

# Sync specific files
./.internal/sync_git.sh "update docs" "docs/"
```

### Features

- Auto-pulls latest changes
- Detects merge conflicts with helpful resolution options
- Only commits if there are actual changes
- Proper error handling and exit codes

### Merge Conflict Handling

If merge conflicts are detected, the script will:
1. List conflicting files
2. Provide resolution options:
   - Manual resolution
   - Keep yours: `git checkout --ours <file>`
   - Keep theirs: `git checkout --theirs <file>`
   - Abort: `git merge --abort`

### Examples

```bash
# Quick sync with generated message
./.internal/sync_git.sh

# Sync with custom message
./.internal/sync_git.sh "fix: resolve type checker bug"

# Sync only source files
./.internal/sync_git.sh "update src" "src/"
```
