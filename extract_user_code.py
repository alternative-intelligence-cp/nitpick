import re

with open('test_clean.ll', 'r') as f:
    content = f.read()

# Find __user_main function
match = re.search(r'define internal %result_int8 @__user_main\(\).*?^}', content, re.MULTILINE | re.DOTALL)
if match:
    user_main = match.group(0)
    # Remove shadow stack calls
    user_main = re.sub(r'\s*call void @aria_shadow_stack_push_frame\(\)', '', user_main)
    user_main = re.sub(r'\s*call void @aria_shadow_stack_pop_frame\(\)', '', user_main)
    
    output = f"""
%result_int8 = type {{ i8, i8 }}

{user_main}

define i32 @main() {{
entry:
  %0 = call %result_int8 @__user_main()
  %1 = extractvalue %result_int8 %0, 1
  %2 = sext i8 %1 to i32
  ret i32 %2
}}
"""
    with open('test_extracted.ll', 'w') as f:
        f.write(output)
    print("✓ Extracted user code")
else:
    print("ERROR: Could not find __user_main")
