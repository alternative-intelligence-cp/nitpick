; Textobjects queries for Aria
; Used for smart text selection and navigation

; Functions
(function_declaration) @function.outer
(function_declaration body: (block) @function.inner)

; Parameters
(parameter) @parameter.outer

; Classes/Structs
(struct_declaration) @class.outer
(struct_declaration "{" _ @class.inner "}")

; Comments
(line_comment) @comment.outer
(block_comment) @comment.outer

; Conditionals
[
  (if_expression)
  (when_expression)
  (pick_expression)
] @conditional.outer

; Loops
; (for statement would go here when added)

; Calls
(call_expression) @call.outer
(call_expression arguments: (argument_list) @call.inner)

; Blocks
(block) @block.outer
