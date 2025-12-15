#ifndef ARIA_FRONTEND_AST_MODULE_H
#define ARIA_FRONTEND_AST_MODULE_H

#include "../ast.h"
#include "stmt.h"
#include <string>
#include <memory>
#include <vector>

namespace aria {
namespace frontend {

// Use Statement (Import)
// Example: use std.io; or use std.io.{read, write};
class UseStmt : public Statement {
public:
    std::string module_path;  // e.g., "std.io"
    std::vector<std::string> imports;  // Empty = import all, otherwise specific items

    UseStmt(const std::string& path, const std::vector<std::string>& imps = {})
        : module_path(path), imports(imps) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Extern Block (Foreign Function Interface)
// Example: extern { fn c_function(int32) -> int32; }
class ExternBlock : public Statement {
public:
    std::vector<std::unique_ptr<Statement>> declarations;

    ExternBlock() = default;

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

// Module Definition
// Example: mod utils { ... }
class ModDef : public Statement {
public:
    std::string name;
    std::unique_ptr<Block> body;

    ModDef(const std::string& n, std::unique_ptr<Block> b)
        : name(n), body(std::move(b)) {}

    void accept(AstVisitor& visitor) override {
        visitor.visit(this);
    }
};

} // namespace frontend
} // namespace aria

#endif // ARIA_FRONTEND_AST_MODULE_H
