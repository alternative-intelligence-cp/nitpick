/**
 * aria-ls - Aria Language Server
 * 
 * LSP server executable that provides IDE features for Aria.
 * Communicates with editors via stdin/stdout using JSON-RPC 2.0.
 * 
 * Usage: Run from VS Code extension or other LSP client
 */

#include "tools/lsp/server.h"
#include <iostream>

int main(int argc, char** argv) {
    try {
        aria::lsp::Server server;
        server.run();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << std::endl;
        return 1;
    }
}
