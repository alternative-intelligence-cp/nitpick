#!/bin/bash
# Simple direct test of command execution

cd /home/randy/._____RANDY_____/REPOS/aria_shell/build

echo "Testing direct command execution..."

# Create a test program that uses the executor directly
cat > /tmp/test_cmd.cpp << 'EOF'
#include "../inc/parser/lexer.hpp"
#include "../inc/parser/parser.hpp"
#include "../inc/executor/executor.hpp"
#include <iostream>

using namespace ariash;

int main() {
    std::cout << "=== Testing Command Execution ===\n\n";
    
    // Test 1: echo
    std::cout << "Test 1: echo command\n";
    parser::Lexer lexer1("echo \"Hello from ariash!\"");
    auto tokens1 = lexer1.tokenize();
    parser::Parser parser1(tokens1);
    auto ast1 = parser1.parse();
    executor::Executor exec1;
    exec1.execute(*ast1);
    
    std::cout << "\nTest 2: ls command\n";
    parser::Lexer lexer2("ls -la /tmp");
    auto tokens2 = lexer2.tokenize();
    parser::Parser parser2(tokens2);
    auto ast2 = parser2.parse();
    executor::Executor exec2;
    exec2.execute(*ast2);
    
    std::cout << "\nTest 3: pwd command\n";
    parser::Lexer lexer3("pwd");
    auto tokens3 = lexer3.tokenize();
    parser::Parser parser3(tokens3);
    auto ast3 = parser3.parse();
    executor::Executor exec3;
    exec3.execute(*ast3);
    
    std::cout << "\n=== All Tests Complete ===\n";
    return 0;
}
EOF

echo "Compiling test..."
g++ -std=c++20 -I.. /tmp/test_cmd.cpp \
    ../src/parser/lexer.cpp \
    ../src/parser/parser.cpp \
    libaria_shell_job.a \
    -lpthread -o /tmp/test_cmd

if [ $? -eq 0 ]; then
    echo "Running test..."
    /tmp/test_cmd
else
    echo "Compilation failed"
    exit 1
fi
