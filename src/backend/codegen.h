#ifndef ARIA_BACKEND_CODEGEN_H
#define ARIA_BACKEND_CODEGEN_H

#include "../frontend/ast.h"
#include <string>

namespace aria {
namespace backend {

// Generate LLVM IR Code from AST
// Outputs to the specified filename
// Returns true if successful, false if verification fails
bool generate_code(frontend::Block* root, const std::string& filename, bool verify = true);

} // namespace backend
} // namespace aria

#endif // ARIA_BACKEND_CODEGEN_H
