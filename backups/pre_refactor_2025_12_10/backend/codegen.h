#ifndef ARIA_BACKEND_CODEGEN_H
#define ARIA_BACKEND_CODEGEN_H

#include "../frontend/ast.h"
#include "../frontend/ast/stmt.h"
#include <string>
#include <vector>

namespace aria {
namespace backend {

// Trait context for code generation
struct TraitContext {
    std::vector<frontend::TraitDecl*> traits;
    std::vector<frontend::ImplDecl*> impls;
};

// Generate LLVM IR Code from AST
// Outputs to the specified filename
// Returns true if successful, false if verification fails
bool generate_code(frontend::Block* root, const std::string& filename, bool verify = true);

// Generate LLVM IR Code from AST with trait support
// Outputs to the specified filename
// Returns true if successful, false if verification fails
bool generate_code(frontend::Block* root, const std::string& filename, const TraitContext& traitCtx, bool verify = true);

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_CODEGEN_H
