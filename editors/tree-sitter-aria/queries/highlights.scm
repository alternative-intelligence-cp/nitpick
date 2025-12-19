; Highlighting queries for Aria
; Used by nvim-treesitter and other Tree-sitter editors

; Keywords
[
  "func"
  "struct"
  "enum"
  "trait"
  "impl"
  "mod"
  "import"
  "export"
  "var"
  "const"
  "if"
  "else"
  "when"
  "then"
  "end"
  "pick"
  "case"
  "default"
  "return"
  "break"
  "continue"
  "defer"
  "await"
  "async"
  "for"
] @keyword

; Visibility
[
  "pub"
  "priv"
] @keyword.modifier

; Types - TBB types
(tbb_type) @type.builtin

; Types - Balanced types
(balanced_type) @type.builtin

; Types - Primitive types
(primitive_type) @type.builtin

; Types - User-defined
(struct_declaration name: (identifier) @type)
(enum_declaration name: (identifier) @type)
(trait_declaration name: (identifier) @type)

; Type annotations
(parameter type: (identifier) @type)
(variable_declaration type: (identifier) @type)
(const_declaration type: (identifier) @type)
(struct_field type: (identifier) @type)

; Memory qualifiers
(memory_qualifier) @keyword.storage

; Functions
(function_declaration name: (identifier) @function)
(call_expression function: (identifier) @function.call)
(trait_method name: (identifier) @function.method)

; Parameters
(parameter name: (identifier) @variable.parameter)

; Variables
(variable_declaration name: (identifier) @variable)
(const_declaration name: (identifier) @constant)

; Fields
(field_expression field: (identifier) @property)
(struct_field name: (identifier) @property)

; Operators
[
  "="
  "+"
  "-"
  "*"
  "/"
  "%"
  "||"
  "&&"
  "!"
  "=="
  "!="
  "<"
  ">"
  "<="
  ">="
  "|"
  "&"
  "^"
  "~"
  "<<"
  ">>"
  "->"
] @operator

; Punctuation
[
  "("
  ")"
  "["
  "]"
  "{"
  "}"
] @punctuation.bracket

[
  "."
  ","
  ":"
  ";"
] @punctuation.delimiter

; Literals
(integer_literal) @number
(float_literal) @number.float
(string_literal) @string
(char_literal) @character
(boolean_literal) @boolean
(null_literal) @constant.builtin
(error_literal) @constant.builtin

; Template strings
(template_string) @string
(template_chars) @string
(template_substitution
  "&{" @punctuation.special
  "}" @punctuation.special) @embedded

; Comments
(line_comment) @comment
(block_comment) @comment

; Special operators for memory safety
"#" @operator.special ; pin
"$" @operator.special ; safe ref
"*" @operator.special ; dereference (in pointer context)
"&" @operator.special ; reference (in type context)

; Generic parameters
(generic_parameters
  (identifier) @type.parameter)

; Module paths
(module_path (identifier) @namespace)

; Error handling
(result_type) @type.builtin
(option_type) @type.builtin
(future_type) @type.builtin
