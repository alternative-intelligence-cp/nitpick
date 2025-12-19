; Locals queries for Aria
; Used for scope-aware features like LSP rename

; Scopes
[
  (function_declaration)
  (struct_declaration)
  (enum_declaration)
  (trait_declaration)
  (impl_block)
  (module_declaration)
  (block)
] @scope

; Definitions
(function_declaration
  name: (identifier) @definition.function)

(parameter
  name: (identifier) @definition.parameter)

(variable_declaration
  name: (identifier) @definition.variable)

(const_declaration
  name: (identifier) @definition.constant)

(struct_declaration
  name: (identifier) @definition.type)

(enum_declaration
  name: (identifier) @definition.type)

(trait_declaration
  name: (identifier) @definition.type)

(struct_field
  name: (identifier) @definition.field)

(enum_variant
  name: (identifier) @definition.variant)

; References
(identifier) @reference
