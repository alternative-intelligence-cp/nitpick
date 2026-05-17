import os
import re

def check_file(filepath):
    with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
        content = f.read()
    
    # Pattern for struct Name { ... };
    struct_decl = re.compile(r'struct\s+([A-Za-z0-9_]+)\s*\{[^}]*\};', re.MULTILINE)
    
    for match in struct_decl.finditer(content):
        name = match.group(1)
        # Search for Name{ ... }
        # Looking for binding: let x = Name { ... } or var x = Name { ... } or similar
        # and then return x or similar. Just Name{ ... } for now.
        construction = re.compile(rf'{name}\s*\{{[^}}]*\}}', re.MULTILINE)
        if construction.search(content):
            print(f"FILE: {filepath}")
            # print snippet around construction
            start = max(0, match.start() - 100)
            end = min(len(content), match.end() + 500)
            print(content[start:end])
            return True
    return False

root = "/home/randy/Workspace/REPOS/aria/tests"
for r, ds, fs in os.walk(root):
    for f in fs:
        if f.endswith(".p") or f.endswith(".npk"): # Checking both if npk is not strictly found
            check_file(os.path.join(r, f))
