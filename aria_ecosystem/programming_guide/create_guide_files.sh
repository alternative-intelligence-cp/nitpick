#!/bin/bash

# TYPES - Primitive Integers
touch types/int1.md types/int2.md types/int4.md types/int8.md types/int16.md types/int32.md types/int64.md
touch types/int128.md types/int256.md types/int512.md

# TYPES - Unsigned Integers
touch types/uint8.md types/uint16.md types/uint32.md types/uint64.md
touch types/uint128.md types/uint256.md types/uint512.md

# TYPES - TBB (Twisted Balanced Binary)
touch types/tbb8.md types/tbb16.md types/tbb32.md types/tbb64.md
touch types/tbb_overview.md types/tbb_err_sentinel.md types/tbb_sticky_errors.md

# TYPES - Floating Point
touch types/flt32.md types/flt64.md types/flt128.md types/flt256.md types/flt512.md

# TYPES - Boolean and Vectors
touch types/bool.md types/vec2.md types/vec3.md types/vec9.md

# TYPES - Dynamic and Object
touch types/dyn.md types/obj.md

# TYPES - Structured Types
touch types/struct.md types/struct_declaration.md types/struct_fields.md types/struct_generics.md types/struct_pointers.md

# TYPES - String and Collections
touch types/string.md types/array.md types/array_declaration.md types/array_operations.md

# TYPES - Result and Function
touch types/result.md types/result_err_val.md types/result_unwrap.md
touch types/func.md types/func_declaration.md types/func_return.md

# TYPES - Balanced Number Systems
touch types/trit.md types/tryte.md types/nit.md types/nyte.md
touch types/balanced_ternary.md types/balanced_nonary.md

# TYPES - Advanced
touch types/tensor.md types/matrix.md types/void.md types/pointer.md

# SPECIAL VALUES
touch types/NIL.md types/NULL.md types/ERR.md
touch types/nil_vs_null_vs_void.md

# MEMORY MODEL
touch memory_model/wild.md memory_model/gc.md memory_model/stack.md memory_model/wildx.md
touch memory_model/defer.md memory_model/raii.md
touch memory_model/pinning.md memory_model/pin_operator.md
touch memory_model/borrowing.md memory_model/borrow_operator.md memory_model/immutable_borrow.md memory_model/mutable_borrow.md
touch memory_model/pointer_syntax.md memory_model/address_operator.md
touch memory_model/allocators.md memory_model/aria_alloc.md memory_model/aria_free.md
touch memory_model/aria_gc_alloc.md memory_model/aria_alloc_buffer.md
touch memory_model/aria_alloc_string.md memory_model/aria_alloc_array.md

# CONTROL FLOW
touch control_flow/if_else.md control_flow/if_syntax.md
touch control_flow/while.md control_flow/while_syntax.md
touch control_flow/for.md control_flow/for_syntax.md
touch control_flow/loop.md control_flow/loop_syntax.md control_flow/loop_direction.md
touch control_flow/till.md control_flow/till_syntax.md control_flow/till_direction.md
touch control_flow/when_then_end.md control_flow/when_syntax.md
touch control_flow/pick.md control_flow/pick_syntax.md control_flow/pick_patterns.md
touch control_flow/fall.md control_flow/fallthrough.md
touch control_flow/break.md control_flow/continue.md control_flow/return.md
touch control_flow/pass.md control_flow/fail.md
touch control_flow/iteration_variable.md control_flow/dollar_variable.md

# OPERATORS - Assignment
touch operators/assign.md operators/plus_assign.md operators/minus_assign.md
touch operators/mult_assign.md operators/div_assign.md operators/mod_assign.md

# OPERATORS - Arithmetic
touch operators/plus.md operators/minus.md operators/multiply.md operators/divide.md
touch operators/modulo.md operators/increment.md operators/decrement.md

# OPERATORS - Comparison
touch operators/equal.md operators/not_equal.md
touch operators/less_than.md operators/greater_than.md
touch operators/less_equal.md operators/greater_equal.md
touch operators/spaceship.md operators/three_way_comparison.md

# OPERATORS - Logical
touch operators/logical_and.md operators/logical_or.md operators/logical_not.md

# OPERATORS - Bitwise
touch operators/bitwise_and.md operators/bitwise_or.md operators/bitwise_xor.md
touch operators/bitwise_not.md operators/left_shift.md operators/right_shift.md

# OPERATORS - Special
touch operators/address.md operators/at_operator.md
touch operators/pin.md operators/hash_operator.md
touch operators/iteration.md operators/dollar_operator.md
touch operators/safe_nav.md operators/safe_navigation.md
touch operators/null_coalesce.md operators/null_coalescing.md
touch operators/unwrap.md operators/question_operator.md
touch operators/pipe_forward.md operators/pipe_backward.md operators/pipeline.md
touch operators/range_inclusive.md operators/range_exclusive.md operators/range.md
touch operators/ternary_is.md operators/is_operator.md
touch operators/arrow.md operators/pointer_member.md
touch operators/colon.md operators/type_annotation.md
touch operators/dot.md operators/member_access.md
touch operators/ampersand.md operators/interpolation.md

# OPERATORS - Template
touch operators/backtick.md operators/template_literal.md
touch operators/string_interpolation.md operators/template_syntax.md

# FUNCTIONS
touch functions/function_declaration.md functions/func_keyword.md
touch functions/function_syntax.md functions/function_params.md functions/function_return_type.md
touch functions/anonymous_functions.md functions/lambda.md functions/lambda_syntax.md
touch functions/closures.md functions/closure_capture.md
touch functions/higher_order_functions.md functions/function_arguments.md
touch functions/immediate_execution.md
touch functions/pass_keyword.md functions/fail_keyword.md
touch functions/async_functions.md functions/async_keyword.md

# GENERICS
touch functions/generics.md functions/generic_functions.md functions/generic_syntax.md
touch functions/generic_types.md functions/generic_parameters.md
touch functions/monomorphization.md functions/type_inference.md
touch functions/generic_structs.md functions/multiple_generics.md
touch functions/generic_star_prefix.md

# MODULES
touch modules/use.md modules/use_syntax.md modules/import.md
touch modules/mod.md modules/mod_keyword.md modules/module_definition.md
touch modules/pub.md modules/public_visibility.md
touch modules/module_paths.md modules/relative_imports.md modules/absolute_imports.md
touch modules/wildcard_imports.md modules/selective_imports.md
touch modules/module_aliases.md
touch modules/nested_modules.md
touch modules/conditional_compilation.md modules/cfg.md

# EXTERN / FFI
touch modules/extern.md modules/extern_blocks.md modules/extern_syntax.md
touch modules/ffi.md modules/c_interop.md modules/c_pointers.md
touch modules/extern_functions.md modules/libc_integration.md

# I/O SYSTEM
touch io_system/io_overview.md io_system/six_stream_topology.md io_system/hex_stream.md
touch io_system/stdin.md io_system/stdout.md io_system/stderr.md
touch io_system/stddbg.md io_system/stddati.md io_system/stddato.md
touch io_system/text_io.md io_system/binary_io.md io_system/debug_io.md
touch io_system/stream_separation.md io_system/data_plane.md io_system/control_plane.md

# STANDARD LIBRARY - I/O
touch standard_library/print.md standard_library/readFile.md standard_library/writeFile.md
touch standard_library/readJSON.md standard_library/readCSV.md
touch standard_library/openFile.md standard_library/stream_io.md

# STANDARD LIBRARY - Process
touch standard_library/spawn.md standard_library/fork.md standard_library/exec.md
touch standard_library/createPipe.md standard_library/wait.md
touch standard_library/process_management.md

# STANDARD LIBRARY - HTTP
touch standard_library/httpGet.md standard_library/http_client.md

# STANDARD LIBRARY - Collections
touch standard_library/filter.md standard_library/transform.md standard_library/reduce.md
touch standard_library/sort.md standard_library/reverse.md standard_library/unique.md
touch standard_library/functional_programming.md

# STANDARD LIBRARY - Math
touch standard_library/math.md standard_library/math_round.md

# STANDARD LIBRARY - System
touch standard_library/getMemoryUsage.md standard_library/getActiveConnections.md
touch standard_library/system_diagnostics.md

# STANDARD LIBRARY - Logging
touch standard_library/createLogger.md standard_library/structured_logging.md
touch standard_library/log_levels.md

# ADVANCED FEATURES
touch advanced_features/const.md advanced_features/comptime.md advanced_features/compile_time.md
touch advanced_features/async_await.md advanced_features/async.md advanced_features/await.md
touch advanced_features/coroutines.md
touch advanced_features/threading.md advanced_features/concurrency.md
touch advanced_features/atomics.md
touch advanced_features/macros.md advanced_features/nasm_macros.md advanced_features/context_stack.md
touch advanced_features/metaprogramming.md
touch advanced_features/pattern_matching.md advanced_features/destructuring.md
touch advanced_features/error_handling.md advanced_features/error_propagation.md

# SYNTAX FEATURES
touch advanced_features/whitespace_insensitive.md advanced_features/brace_delimited.md
touch advanced_features/semicolons.md advanced_features/colons.md
touch advanced_features/comments.md advanced_features/multiline_comments.md

# COMPLETE TOKEN REFERENCE
touch advanced_features/tokens.md advanced_features/lexer.md advanced_features/parser.md
touch advanced_features/ast.md

# EXAMPLES AND PATTERNS
touch advanced_features/code_examples.md advanced_features/best_practices.md
touch advanced_features/common_patterns.md advanced_features/idioms.md

echo "Created $(find . -name '*.md' | wc -l) guide files"
