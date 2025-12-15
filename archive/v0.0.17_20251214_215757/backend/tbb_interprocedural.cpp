/**
 * TBB Interprocedural Analysis - Function Summaries
 * 
 * Analyzes functions to determine TBB error propagation properties:
 * - Which arguments must be non-ERR?
 * - Does the function guarantee non-ERR return?
 * - What is the return value range?
 * 
 * Uses function summaries to optimize across function boundaries.
 */

#include "llvm/IR/Module.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/Intrinsics.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/Analysis/LazyValueInfo.h"
#include "llvm/IR/ConstantRange.h"
#include "llvm/Support/raw_ostream.h"
#include <vector>
#include <optional>

using namespace llvm;

namespace aria {
namespace backend {

/// Summary of a function's TBB error behavior
struct TBBSummary {
    /// For each argument, does the function require it to be non-ERR?
    std::vector<bool> ArgRequiresNonErr;
    
    /// Does the function guarantee a non-ERR return if inputs are valid?
    bool ReturnsNonErr;
    
    /// Explicit range of return value if constant-derivable
    std::optional<ConstantRange> ReturnRange;
    
    TBBSummary() : ReturnsNonErr(true) {}
};

/// Analyzes a function to generate its TBB summary
TBBSummary analyzeFunction(Function &F) {
    TBBSummary S;
    S.ReturnsNonErr = true; // Optimistic assumption, disproven by analysis
    
    // Initialize argument tracking
    S.ArgRequiresNonErr.resize(F.arg_size(), false);
    
    // 1. Analyze Return Values
    for (BasicBlock &BB : F) {
        if (auto *Ret = dyn_cast<ReturnInst>(BB.getTerminator())) {
            Value *RV = Ret->getReturnValue();
            if (!RV) continue;
            
            // If we return a constant, check if it's ERR
            if (auto *C = dyn_cast<ConstantInt>(RV)) {
                if (C->getValue().isMinSignedValue()) {
                    S.ReturnsNonErr = false;
                }
            } 
            // If we return an argument, we depend on that argument
            else if (auto *Arg = dyn_cast<Argument>(RV)) {
                // Mark that this argument affects return value
                unsigned ArgNo = Arg->getArgNo();
                if (ArgNo < S.ArgRequiresNonErr.size()) {
                    S.ArgRequiresNonErr[ArgNo] = true;
                }
            }
            // For complex instructions, we would use LazyValueInfo
            // to analyze the dominating conditions of the return block
        }
    }
    
    // 2. Analyze argument usage
    // If an argument is compared against ERR and that controls a crash/unreachable,
    // then the argument requires non-ERR
    for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
            if (auto *Cmp = dyn_cast<ICmpInst>(&I)) {
                // Check if comparing an argument against ERR sentinel
                if (auto *Arg = dyn_cast<Argument>(Cmp->getOperand(0))) {
                    if (auto *C = dyn_cast<ConstantInt>(Cmp->getOperand(1))) {
                        if (C->getValue().isMinSignedValue()) {
                            // This argument is being checked for ERR
                            unsigned ArgNo = Arg->getArgNo();
                            if (ArgNo < S.ArgRequiresNonErr.size()) {
                                S.ArgRequiresNonErr[ArgNo] = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    return S;
}

/// Applies a function summary at a call site using llvm.assume
/// 
/// If the summary proves the return value is non-ERR, we inject an
/// llvm.assume intrinsic that informs later optimization passes.
void applySummary(CallInst *CI, const TBBSummary &S) {
    if (S.ReturnsNonErr && CI->getType()->isIntegerTy()) {
        IRBuilder<> B(CI->getNextNode());
        Value *RetVal = CI;
        unsigned BitWidth = RetVal->getType()->getIntegerBitWidth();
        
        // Create condition: RetVal != ERR
        Value *ErrSentinel = ConstantInt::get(RetVal->getType(), 
            APInt::getSignedMinValue(BitWidth));
        Value *IsNotErr = B.CreateICmpNE(RetVal, ErrSentinel);
        
        // Inject assume(RetVal != ERR)
        Function *AssumeFn = Intrinsic::getDeclaration(
            CI->getModule(), Intrinsic::assume);
        B.CreateCall(AssumeFn, IsNotErr);
    }
}

/// Module-level pass to build and apply TBB function summaries
struct TBBInterproceduralAnalysis : public PassInfoMixin<TBBInterproceduralAnalysis> {
    PreservedAnalyses run(Module &M, ModuleAnalysisManager &MAM) {
        // Map from function to its TBB summary
        DenseMap<Function*, TBBSummary> Summaries;
        
        // 1. Build summaries for all functions in the module
        for (Function &F : M) {
            if (F.isDeclaration()) continue;
            Summaries[&F] = analyzeFunction(F);
        }
        
        // 2. Apply summaries at call sites
        bool Changed = false;
        for (Function &F : M) {
            if (F.isDeclaration()) continue;
            
            for (BasicBlock &BB : F) {
                for (Instruction &I : BB) {
                    if (auto *CI = dyn_cast<CallInst>(&I)) {
                        Function *Callee = CI->getCalledFunction();
                        if (!Callee) continue;
                        
                        auto It = Summaries.find(Callee);
                        if (It != Summaries.end()) {
                            applySummary(CI, It->second);
                            Changed = true;
                        }
                    }
                }
            }
        }
        
        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }
};

} // namespace backend
} // namespace aria
