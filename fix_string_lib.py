#!/usr/bin/env python3
"""
Fix stdlib/string_manipulation.aria to use new Aria v0.0.6 syntax:
1. wild int8: -> wild int8@:
2. NULL -> 0
3. return expr; -> pass(expr);
"""

import re

with open('stdlib/string_manipulation.aria.backup', 'r') as f:
    content = f.read()

# Fix 1: wild TYPE: -> wild TYPE@:
content = re.sub(r'\bwild\s+(\w+):', r'wild \1@:', content)

# Fix 2: NULL -> 0
content = re.sub(r'\bNULL\b', '0', content)

# Fix 3: return expr; -> pass(expr);
# Only in code, not in comments
lines = content.split('\n')
result = []

for line in lines:
    # If line is pure comment, skip
    if line.strip().startswith('//'):
        result.append(line)
        continue
    
    # Split into code and comment parts
    if '//' in line:
        code_part, comment_part = line.split('//', 1)
        code_part = re.sub(r'\breturn\s+([^;]+);', r'pass(\1);', code_part)
        result.append(code_part + '//' + comment_part)
    else:
        line = re.sub(r'\breturn\s+([^;]+);', r'pass(\1);', line)
        result.append(line)

content = '\n'.join(result)

with open('stdlib/string_manipulation.aria', 'w') as f:
    f.write(content)

print("Converted string_manipulation.aria successfully")
