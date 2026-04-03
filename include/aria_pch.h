// aria_pch.h — Precompiled header for Aria compiler (v0.8.2)
// Includes the heaviest LLVM and C++ stdlib headers to speed up builds.
#pragma once

// C++ standard library (frequently used across all TUs)
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <functional>
#include <variant>
#include <cassert>

// LLVM core headers (expensive to parse, included in most IR/codegen TUs)
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>
#include <llvm/IR/Instructions.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/TargetSelect.h>
