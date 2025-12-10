/**
 * TBB Loop Optimizer - LLVM Pass for Aria Compiler
 * 
 * Eliminates redundant TBB error checks inside loops by proving that
 * induction variables cannot contain the ERR sentinel value.
 * 
 * For TBB types (tbb8, tbb16, tbb32, tbb64):
 * - Valid range: [MIN_SIGNED+1, MAX_SIGNED]
 * - ERR sentinel: MIN_SIGNED (e.g., -128 for tbb8)
 * 
 * This pass uses LLVM's ScalarEvolution to determine the range of loop
 * induction variables and eliminates checks when the range provably
 * excludes the ERR sentinel.
 */

#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Constants.h"
#include "llvm/Transforms/Utils/LoopUtils.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

namespace aria {
namespace backend {

/// Helper to determine if a range contains the TBB error sentinel.
/// For tbb8, valid is [-127, 127], ERR is -128 (0x80).
static bool rangeContainsTBBError(const ConstantRange &Range, unsigned BitWidth) {
    // TBB Error sentinel is the minimum signed value (e.g., -128 for int8)
    APInt ErrorSentinel = APInt::getSignedMinValue(BitWidth);
    return Range.contains(ErrorSentinel);
}

/// TBB Loop Optimizer Pass
/// 
/// Analyzes loops to eliminate redundant TBB error checks on induction variables.
struct TBBLoopOptimizer : public PassInfoMixin<TBBLoopOptimizer> {
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &FAM) {
        auto &LI = FAM.getResult<LoopAnalysis>(F);
        auto &SE = FAM.getResult<ScalarEvolutionAnalysis>(F);
        auto &DT = FAM.getResult<DominatorTreeAnalysis>(F);

        bool Changed = false;

        // Process loops in post-order (innermost first)
        for (auto *L : LI) {
            Changed |= optimizeLoop(L, SE, DT);
        }

        // If we modified the IR, we must invalidate analyses that cache IR state
        return Changed ? PreservedAnalyses::none() : PreservedAnalyses::all();
    }

private:
    bool optimizeLoop(Loop *L, ScalarEvolution &SE, DominatorTree &DT) {
        // 1. Identify the canonical induction variable (IV)
        PHINode *IndVar = L->getCanonicalInductionVariable();
        if (!IndVar) return false;

        // 2. Determine the maximum trip count of the loop.
        // getConstantMaxBackedgeTakenCount returns the number of times the backedge
        // executes. We add 1 to get the value range of the IV.
        const SCEV *BackedgeCount = SE.getConstantMaxBackedgeTakenCount(L);
        
        // If the loop bound is uncomputable (e.g., depends on complex runtime logic),
        // we cannot optimize safely.
        if (isa<SCEVCouldNotCompute>(BackedgeCount)) {
            return false;
        }

        // 3. Construct the full range of the induction variable.
        // The IV starts at Start and increments by Step.
        // We use SCEV to get the signed range of the variable throughout the loop.
        const SCEV *IndVarSCEV = SE.getSCEV(IndVar);
        ConstantRange IVRange = SE.getSignedRange(IndVarSCEV);

        // 4. Scan the loop body for redundant TBB checks.
        bool LoopChanged = false;
        
        for (BasicBlock *BB : L->blocks()) {
            // Safe iteration allowing instruction removal
            for (auto I = BB->begin(), E = BB->end(); I != E; ) {
                Instruction *Inst = &*I++;
                
                // Identify TBB error checks. 
                // Pattern: %is_err = icmp eq i8 %val, -128
                if (auto *Cmp = dyn_cast<ICmpInst>(Inst)) {
                    LoopChanged |= tryEliminateCheck(Cmp, IndVar, IVRange);
                }
            }
        }
        return LoopChanged;
    }

    bool tryEliminateCheck(ICmpInst *Cmp, PHINode *IndVar, const ConstantRange &IVRange) {
        // We only optimize checks on the induction variable itself
        if (Cmp->getOperand(0) != IndVar) return false;

        // Check if we are testing for the ERR sentinel
        // TBB check pattern: if (iv == ERR) or if (iv != ERR)
        ConstantInt *ConstOp = dyn_cast<ConstantInt>(Cmp->getOperand(1));
        if (!ConstOp) return false;

        APInt Sentinel = ConstOp->getValue();
        // Verify this is actually a check against the TBB error sentinel (MinSignedValue)
        if (Sentinel != APInt::getSignedMinValue(IndVar->getType()->getIntegerBitWidth())) {
            return false;
        }

        // 5. The Core Logic:
        // If the IVRange definitely does NOT contain the Sentinel, the check is redundant.
        if (!IVRange.contains(Sentinel)) {
            // The check "iv == ERR" is always FALSE.
            // The check "iv != ERR" is always TRUE.
            
            Value *Replacement = nullptr;
            if (Cmp->getPredicate() == ICmpInst::ICMP_EQ) {
                Replacement = ConstantInt::getFalse(Cmp->getContext());
            } else if (Cmp->getPredicate() == ICmpInst::ICMP_NE) {
                Replacement = ConstantInt::getTrue(Cmp->getContext());
            }

            if (Replacement) {
                Cmp->replaceAllUsesWith(Replacement);
                Cmp->eraseFromParent();
                return true;
            }
        }
        return false;
    }
};

} // namespace backend
} // namespace aria
