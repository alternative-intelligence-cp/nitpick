#!/bin/bash
# Regenerate Aria research context files for Gemini
# Updated: December 19, 2025 - Session 14 & 15

REPO_ROOT="/home/randy/._____RANDY_____/REPOS/aria"
OUTPUT_DIR="$REPO_ROOT/docs/gemini/research_context"

echo "Regenerating Aria research context files..."
echo "Output directory: $OUTPUT_DIR"

# Helper function to add file to output
add_file() {
    local file="$1"
    local output="$2"
    
    if [ -f "$file" ]; then
        echo "" >> "$output"
        echo "FILE: ${file#$REPO_ROOT/}" >> "$output"
        echo "================================================================================" >> "$output"
        cat "$file" >> "$output"
    else
        echo "" >> "$output"
        echo "FILE: ${file#$REPO_ROOT/}" >> "$output"
        echo "================================================================================" >> "$output"
        echo "[FILE NOT FOUND]" >> "$output"
    fi
}

# Part 1: Frontend - Lexer
echo "Generating Part 1: Frontend Lexer..."
OUTPUT="$OUTPUT_DIR/aria_source_part1_frontend_lexer.txt"
echo "=== ARIA SOURCE PART1 - FRONTEND - LEXER ===" > "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/token.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/lexer/lexer.h" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/lexer/token.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/lexer/lexer.cpp" "$OUTPUT"

# Part 2: Frontend - AST
echo "Generating Part 2: Frontend AST..."
OUTPUT="$OUTPUT_DIR/aria_source_part2_frontend_ast.txt"
echo "=== ARIA SOURCE PART2 - FRONTEND - AST ===" > "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/ast/ast_node.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/ast/expr.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/ast/stmt.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/ast/type.h" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/ast/ast_node.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/ast/expr.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/ast/stmt.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/ast/type.cpp" "$OUTPUT"

# Part 3: Frontend - Parser
echo "Generating Part 3: Frontend Parser..."
OUTPUT="$OUTPUT_DIR/aria_source_part3_frontend_parser.txt"
echo "=== ARIA SOURCE PART3 - FRONTEND - PARSER ===" > "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/parser/parser.h" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/parser/parser.cpp" "$OUTPUT"

# Part 4: Semantic Analysis
echo "Generating Part 4: Semantic Analysis..."
OUTPUT="$OUTPUT_DIR/aria_source_part4_semantic_analysis.txt"
echo "=== ARIA SOURCE PART4 - SEMANTIC - ANALYSIS ===" > "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/sema/type.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/sema/symbol_table.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/sema/type_checker.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/sema/module_table.h" "$OUTPUT"
add_file "$REPO_ROOT/include/frontend/sema/generic_resolver.h" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/type.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/symbol_table.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/type_checker.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/module_table.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/module_resolver.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/generic_resolver.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/frontend/sema/monomorphizer.cpp" "$OUTPUT"

# Part 5: Backend - Codegen (CRITICAL - includes TBBCodegen!)
echo "Generating Part 5: Backend Codegen..."
OUTPUT="$OUTPUT_DIR/aria_source_part5_backend_codegen.txt"
echo "=== ARIA SOURCE PART5 - BACKEND - CODEGEN ===" > "$OUTPUT"
add_file "$REPO_ROOT/include/backend/ir/ir_generator.h" "$OUTPUT"
add_file "$REPO_ROOT/include/backend/ir/tbb_codegen.h" "$OUTPUT"
add_file "$REPO_ROOT/src/backend/ir/ir_generator.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/backend/ir/tbb_codegen.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/backend/ir/codegen_expr.cpp" "$OUTPUT"
add_file "$REPO_ROOT/src/backend/ir/codegen_stmt.cpp" "$OUTPUT"

# Part 6: Runtime
echo "Generating Part 6: Runtime..."
OUTPUT="$OUTPUT_DIR/aria_source_part6_runtime.txt"
echo "=== ARIA SOURCE PART6 - RUNTIME ===" > "$OUTPUT"
add_file "$REPO_ROOT/lib/runtime/runtime.c" "$OUTPUT"

# Part 7: LSP & Tooling
echo "Generating Part 7: LSP & Tooling..."
OUTPUT="$OUTPUT_DIR/aria_source_part7_lsp_tooling.txt"
echo "=== ARIA SOURCE PART7 - LSP - TOOLING ===" > "$OUTPUT"
add_file "$REPO_ROOT/tools/aria-lsp/README.md" "$OUTPUT"

# Part 8: Compiler Driver
echo "Generating Part 8: Compiler Driver..."
OUTPUT="$OUTPUT_DIR/aria_source_part8_compiler_driver.txt"
echo "=== ARIA SOURCE PART8 - COMPILER - DRIVER ===" > "$OUTPUT"
add_file "$REPO_ROOT/src/main.cpp" "$OUTPUT"
add_file "$REPO_ROOT/include/driver/compiler.h" "$OUTPUT"
add_file "$REPO_ROOT/src/driver/compiler.cpp" "$OUTPUT"

echo ""
echo "✅ Research context files regenerated successfully!"
echo "Files updated with latest changes from:"
echo "  - Session 14 Part 2: TBB overflow detection"
echo "  - Session 15: Balanced ternary types"
echo ""
echo "Summary:"
ls -lh "$OUTPUT_DIR"/*.txt | awk '{print "  " $9 " (" $5 ")"}'
