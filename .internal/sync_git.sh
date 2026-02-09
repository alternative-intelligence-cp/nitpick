#!/usr/bin/env bash
# Quick git sync script - pull, add, commit, push with merge handling

wdir="$(dirname "$(readlink -f "$0")")"
repo_root="$wdir/../"
cd "$repo_root" || exit 1

# Parse arguments
if [ -z "$1" ]; then
    commit_msg="syncing $(basename "$repo_root")"
else
    commit_msg="$1"
fi

if [ -z "$2" ]; then
    new_files="."
else
    new_files="$2"
fi

# Function to check for merge conflicts
check_merge_conflicts() {
    if git diff --name-only --diff-filter=U | grep -q .; then
        return 0  # Has conflicts
    else
        return 1  # No conflicts
    fi
}

# Pull latest changes
echo "Pulling latest changes..."
if ! git pull; then
    # Pull failed - check for merge conflicts
    if check_merge_conflicts; then
        echo ""
        echo "⚠️  Merge conflicts detected:"
        git diff --name-only --diff-filter=U
        echo ""
        echo "Options:"
        echo "  1. Resolve manually and run: git add <files> && git commit && git push"
        echo "  2. Keep yours:  git checkout --ours <file> && git add <file>"
        echo "  3. Keep theirs: git checkout --theirs <file> && git add <file>"
        echo "  4. Abort merge: git merge --abort"
        exit 1
    else
        echo "❌ Pull failed for unknown reason"
        exit 1
    fi
fi

# Check if there are changes to commit
if ! git diff-index --quiet HEAD -- || [ -n "$(git ls-files --others --exclude-standard)" ]; then
    echo "Adding files: $new_files"
    git add "$new_files"
    
    echo "Committing: $commit_msg"
    git commit -m "$commit_msg"
    
    echo "Pushing changes..."
    git push
    
    echo "✓ Sync complete!"
else
    echo "✓ No changes to commit"
fi