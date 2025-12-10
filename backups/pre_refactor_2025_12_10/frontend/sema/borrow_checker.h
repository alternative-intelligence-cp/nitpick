/**
 * src/frontend/sema/borrow_checker.h
 * 
 * Aria Compiler - Borrow Checker Header
 * Version: 0.0.6
 * 
 * Public interface for the Aria borrow checker which enforces:
 * - Lifetime Analysis: Safe References ($) must not outlive Pinned (#) hosts
 * - Wild Safety: Detect Use-After-Free errors for manual memory
 * - Pinning Enforcement: Prevent movement of pinned objects
 * - Type Validation: Ensure strict typing rules
 */

#ifndef ARIA_FRONTEND_SEMA_BORROW_CHECKER_H
#define ARIA_FRONTEND_SEMA_BORROW_CHECKER_H

namespace aria {
namespace frontend {
    // Forward declaration
    class Block;
}

namespace sema {

/**
 * Run borrow checking analysis on the AST.
 * 
 * @param root The root Block node of the AST to analyze
 * @return true if borrow checking passed without errors, false otherwise
 */
bool check_borrow_rules(aria::frontend::Block* root);

} // namespace sema
} // namespace aria

#endif // ARIA_FRONTEND_SEMA_BORROW_CHECKER_H
