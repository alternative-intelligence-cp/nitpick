/**
 * Tree-sitter Grammar for Aria Programming Language
 * 
 * Provides structural parsing for:
 * - Neovim (nvim-treesitter)
 * - Helix Editor
 * - Other Tree-sitter compatible editors
 * 
 * Features:
 * - TBB types with error sentinels
 * - Hybrid memory model (@wild, @gc, @stack)
 * - String interpolation with &{expression}
 * - Pattern matching (pick/case)
 * - Async/await
 */

module.exports = grammar({
  name: 'aria',

  extras: $ => [
    /\s/,
    $.line_comment,
    $.block_comment,
  ],

  word: $ => $.identifier,

  conflicts: $ => [
    [$.struct_expression, $.block],
    [$.reference_type, $.function_type],
  ],

  rules: {
    source_file: $ => repeat($._statement),

    // Comments
    line_comment: $ => token(seq('//', /.*/)),
    block_comment: $ => token(seq('/*', /[^*]*\*+([^/*][^*]*\*+)*/, '/')),

    // Statements
    _statement: $ => choice(
      $.function_declaration,
      $.struct_declaration,
      $.enum_declaration,
      $.trait_declaration,
      $.impl_block,
      $.module_declaration,
      $.import_statement,
      $.export_statement,
      $.variable_declaration,
      $.const_declaration,
      $.expression_statement,
      $.return_statement,
      $.break_statement,
      $.continue_statement,
      $.defer_statement,
    ),

    // Function declaration: func:name = (params) -> return_type { body }
    function_declaration: $ => seq(
      optional($.visibility),
      optional('async'),
      'func',
      ':',
      field('name', $.identifier),
      optional($.generic_parameters),
      '=',
      field('parameters', $.parameter_list),
      optional(seq('->', field('return_type', $._type))),
      field('body', $.block),
    ),

    parameter_list: $ => seq(
      '(',
      optional(seq(
        $.parameter,
        repeat(seq(',', $.parameter)),
        optional(','),
      )),
      ')',
    ),

    parameter: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $._type),
    ),

    generic_parameters: $ => seq(
      '<',
      seq(
        $.identifier,
        repeat(seq(',', $.identifier)),
        optional(','),
      ),
      '>',
    ),

    // Struct declaration
    struct_declaration: $ => seq(
      optional($.visibility),
      'struct',
      field('name', $.identifier),
      optional($.generic_parameters),
      '{',
      repeat($.struct_field),
      '}',
    ),

    struct_field: $ => seq(
      field('name', $.identifier),
      ':',
      field('type', $._type),
      optional(','),
    ),

    // Enum declaration
    enum_declaration: $ => seq(
      optional($.visibility),
      'enum',
      field('name', $.identifier),
      optional($.generic_parameters),
      '{',
      repeat($.enum_variant),
      '}',
    ),

    enum_variant: $ => seq(
      field('name', $.identifier),
      optional(seq('(', $._type, ')')),
      optional(','),
    ),

    // Trait declaration
    trait_declaration: $ => seq(
      optional($.visibility),
      'trait',
      field('name', $.identifier),
      optional($.generic_parameters),
      '{',
      repeat($.trait_method),
      '}',
    ),

    trait_method: $ => seq(
      'func',
      ':',
      field('name', $.identifier),
      '=',
      $.parameter_list,
      optional(seq('->', $._type)),
      ';',
    ),

    // Impl block
    impl_block: $ => seq(
      'impl',
      optional(seq(field('trait', $.identifier), 'for')),
      field('type', $.identifier),
      optional($.generic_parameters),
      '{',
      repeat($.function_declaration),
      '}',
    ),

    // Module declaration
    module_declaration: $ => seq(
      optional($.visibility),
      'mod',
      field('name', $.identifier),
      choice(
        ';',
        seq('{', repeat($._statement), '}'),
      ),
    ),

    // Import/Export
    import_statement: $ => seq(
      'import',
      $.module_path,
      ';',
    ),

    export_statement: $ => seq(
      'export',
      choice(
        $.function_declaration,
        $.struct_declaration,
        $.enum_declaration,
        seq('{', repeat($.identifier), '}'),
      ),
    ),

    module_path: $ => sep1($.identifier, '.'),

    // Variable declaration
    variable_declaration: $ => seq(
      'var',
      field('name', $.identifier),
      ':',
      field('type', $._type),
      optional(seq('=', field('value', $._expression))),
      ';',
    ),

    const_declaration: $ => seq(
      'const',
      field('name', $.identifier),
      ':',
      field('type', $._type),
      '=',
      field('value', $._expression),
      ';',
    ),

    // Visibility
    visibility: $ => choice('pub', 'priv'),

    // Types
    _type: $ => choice(
      $.primitive_type,
      $.tbb_type,
      $.balanced_type,
      $.pointer_type,
      $.reference_type,
      $.array_type,
      $.generic_type,
      $.function_type,
      $.result_type,
      $.option_type,
      $.future_type,
      $.identifier,
    ),

    primitive_type: $ => choice(
      'bool', 'char', 'str', 'void',
      /int(1|2|4|8|16|32|64|128|256|512)/,
      /uint(1|2|4|8|16|32|64|128|256|512)/,
      /flt(16|32|64|128)/,
    ),

    tbb_type: $ => /tbb(8|16|32|64|128|256)/,
    
    balanced_type: $ => choice('trit', 'tryte', 'nit', 'nyte'),

    pointer_type: $ => seq(
      field('qualifier', $.memory_qualifier),
      field('type', $._type),
      '*',
    ),

    reference_type: $ => seq(
      field('type', $._type),
      '&',
    ),

    array_type: $ => seq(
      '[',
      field('element', $._type),
      optional(seq(';', field('size', $._expression))),
      ']',
    ),

    generic_type: $ => seq(
      field('base', $.identifier),
      '<',
      seq(
        $._type,
        repeat(seq(',', $._type)),
        optional(','),
      ),
      '>',
    ),

    function_type: $ => seq(
      'func',
      $.parameter_list,
      optional(seq('->', $._type)),
    ),

    result_type: $ => seq('result', '<', $._type, '>'),
    option_type: $ => seq('option', '<', $._type, '>'),
    future_type: $ => seq('future', '<', $._type, '>'),

    memory_qualifier: $ => choice(
      '@wild',
      '@gc',
      '@stack',
      '@wildx',
    ),

    // Expressions
    _expression: $ => choice(
      $.identifier,
      $.literal,
      $.binary_expression,
      $.unary_expression,
      $.call_expression,
      $.field_expression,
      $.index_expression,
      $.struct_expression,
      $.array_expression,
      $.parenthesized_expression,
      $.if_expression,
      $.when_expression,
      $.pick_expression,
      $.block,
      $.await_expression,
      $.pin_expression,
      $.safe_ref_expression,
    ),

    binary_expression: $ => {
      const table = [
        [12, '||'],
        [11, '&&'],
        [10, choice('==', '!=', '<', '>', '<=', '>=')],
        [9, choice('|', '^')],
        [8, '&'],
        [7, choice('<<', '>>')],
        [6, choice('+', '-')],
        [5, choice('*', '/', '%')],
      ];

      return choice(...table.map(([precedence, operator]) =>
        prec.left(precedence, seq(
          field('left', $._expression),
          field('operator', operator),
          field('right', $._expression),
        ))
      ));
    },

    unary_expression: $ => prec(13, seq(
      field('operator', choice('-', '!', '~')),
      field('operand', $._expression),
    )),

    call_expression: $ => prec(15, seq(
      field('function', $._expression),
      field('arguments', $.argument_list),
    )),

    argument_list: $ => seq(
      '(',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ')',
    ),

    field_expression: $ => prec(16, seq(
      field('value', $._expression),
      '.',
      field('field', $.identifier),
    )),

    index_expression: $ => prec(16, seq(
      field('value', $._expression),
      '[',
      field('index', $._expression),
      ']',
    )),

    struct_expression: $ => seq(
      optional($.memory_qualifier),
      '{',
      repeat(seq(
        field('field', $.identifier),
        ':',
        field('value', $._expression),
        optional(','),
      )),
      '}',
    ),

    array_expression: $ => seq(
      '[',
      optional(seq(
        $._expression,
        repeat(seq(',', $._expression)),
        optional(','),
      )),
      ']',
    ),

    parenthesized_expression: $ => seq('(', $._expression, ')'),

    // Control flow
    if_expression: $ => seq(
      'if',
      '(',
      field('condition', $._expression),
      ')',
      field('consequence', $.block),
      optional(seq('else', field('alternative', choice($.block, $.if_expression)))),
    ),

    when_expression: $ => seq(
      'when',
      '(',
      field('condition', $._expression),
      ')',
      field('body', $.block),
      optional(seq('then', field('then_block', $.block))),
      optional(seq('end', field('end_block', $.block))),
    ),

    pick_expression: $ => seq(
      'pick',
      '(',
      field('value', $._expression),
      ')',
      '{',
      repeat($.case_clause),
      optional($.default_clause),
      '}',
    ),

    case_clause: $ => seq(
      'case',
      field('pattern', $._expression),
      ':',
      repeat($._statement),
    ),

    default_clause: $ => seq(
      'default',
      ':',
      repeat($._statement),
    ),

    block: $ => seq(
      '{',
      repeat($._statement),
      optional($._expression),
      '}',
    ),

    // Statements
    expression_statement: $ => seq($._expression, ';'),

    return_statement: $ => seq(
      'return',
      optional($._expression),
      ';',
    ),

    break_statement: $ => seq('break', ';'),
    continue_statement: $ => seq('continue', ';'),

    defer_statement: $ => seq(
      'defer',
      $.block,
    ),

    await_expression: $ => prec(14, seq('await', $._expression)),
    pin_expression: $ => prec(14, seq('#', $._expression)),
    safe_ref_expression: $ => prec(14, seq('$', $._expression)),

    // Literals
    literal: $ => choice(
      $.integer_literal,
      $.float_literal,
      $.string_literal,
      $.template_string,
      $.char_literal,
      $.boolean_literal,
      $.null_literal,
      $.error_literal,
    ),

    integer_literal: $ => token(choice(
      /[0-9][0-9_]*/,
      /0[xX][0-9a-fA-F_]+/,
      /0[bB][01_]+/,
      /0[oO][0-7_]+/,
    )),

    float_literal: $ => token(/[0-9][0-9_]*\.[0-9_]+([eE][+-]?[0-9_]+)?/),

    string_literal: $ => token(seq(
      '"',
      repeat(choice(
        /[^"\\]/,
        /\\./,
      )),
      '"',
    )),

    template_string: $ => seq(
      '`',
      repeat(choice(
        $.template_chars,
        $.template_substitution,
      )),
      '`',
    ),

    template_chars: $ => token.immediate(prec(1, /[^`&]+|&[^{]/)),

    template_substitution: $ => seq(
      '&{',
      $._expression,
      '}',
    ),

    char_literal: $ => token(seq(
      "'",
      choice(/[^'\\]/, /\\./),
      "'",
    )),

    boolean_literal: $ => choice('true', 'false'),
    null_literal: $ => 'null',
    error_literal: $ => 'ERR',

    // Identifier
    identifier: $ => /[a-zA-Z_][a-zA-Z0-9_]*/,
  },
});

// Helper function for comma-separated lists
function sep1(rule, separator) {
  return seq(rule, repeat(seq(separator, rule)));
}

function sep(rule, separator) {
  return optional(sep1(rule, separator));
}
