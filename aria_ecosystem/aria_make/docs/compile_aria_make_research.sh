#!/usr/bin/bash
set -e
wdir="$(dirname "$(readlink -f "$0")")" #/home/randy/._____RANDY_____/REPOS/aria_make/docs
odir="$wdir"
ofile="aria_make_research_full.txt"
out="$odir/$ofile"
info="$wdir/info"
research="$wdir/research"
gdir="$research/gemini"
gctx="$gdir/context"
gresp="$gdir/responses"
match="*"
file_type="txt"

# Helper function to safely concatenate .txt files from a directory
# Only processes files directly in the directory (not subdirectories)
# Handles empty directories gracefully without errors
cat_if_exists() {
    local dir="$1"
    local append="$2"
    
    if [ -d "$dir" ]; then
        # Use nullglob to handle empty directories gracefully
        shopt -s nullglob
        local files=("$dir"/*."$file_type")
        shopt -u nullglob
        
        if [ ${#files[@]} -gt 0 ]; then
            if [ "$append" = "true" ]; then
                cat "${files[@]}" >> "$out"
            else
                cat "${files[@]}" > "$out"
            fi
        fi
    fi
}

# Concatenate files in specified order
# First directory creates the file, rest append
cat_if_exists "$info" "false"
cat_if_exists "$research" "true"
cat_if_exists "$gdir" "true"
cat_if_exists "$gctx" "true"
cat_if_exists "$gresp" "true"
