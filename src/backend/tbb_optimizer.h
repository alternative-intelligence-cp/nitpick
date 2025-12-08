/**
 * src/backend/tbb_optimizer.h
 * 
 * Aria Compiler - TBB Optimization Pass
 * 
 * This LLVM optimization pass performs Value Range Propagation (VRP) to
 * eliminate redundant TBB (Twisted Balanced Binary) safety checks.
 * 
 * Optimizations:
 * 1. Input Sentinel Elision: If value provably cannot be ERR sentinel
 * 2. Overflow Elision: If value range guarantees no overflow
 * 3. Result Sentinel Elision: If result provably won't collide with sentinel
 * 
 * Example:
 *   tbb8 x = 5;           // Known to be 5 (not ERR)
 *   tbb8 y = 10;          // Known to be 10 (not ERR)
 *   tbb8 z = x + y;       // Can elide input checks, result is 15 (no overflow)
 * 
 * Dependencies:
 * - LLVM 18 Analysis (ValueRange, KnownBits)
 * - LLVM IR Patterns (Select, ICmp)
 */

#pragma once

#include <llvm/IR/PassManager.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Constants.h>
#include <llvm/Analysis/ValueTracking.h>
#include <llvm/Support/KnownBits.h>

namespace aria {
namespace backend {

using namespace llvm;

/**
 * TBB Value Range Tracker
 * 
 * Tracks the possible range of TBB values through the IR to determine
 * when safety checks can be safely elided.
 */
class TBBValueRange {
public:
    int64_t min;
    int64_t max;
    bool canBeErr;  // Can this value be the ERR sentinel?
    unsigned bitWidth;
    
    TBBValueRange(unsigned bits) 
        : bitWidth(bits), canBeErr(true) {
        // Start with full range
        int64_t sentinel = getSignedMin(bits);
        min = sentinel + 1;  // Smallest valid TBB value
        max = -sentinel;     // Largest valid TBB value (symmetric)
    }
    
    // Get sentinel value for this bit width
    static int64_t getSignedMin(unsigned bits) {
        return -(1LL << (bits - 1));
    }
    
    // Create range from constant
    static TBBValueRange fromConstant(ConstantInt* CI) {
        unsigned bits = CI->getBitWidth();
        TBBValueRange range(bits);
        int64_t value = CI->getSExtValue();
        range.min = value;
        range.max = value;
        range.canBeErr = (value == getSignedMin(bits));
        return range;
    }
    
    // Check if addition can overflow
    bool addWillOverflow(const TBBValueRange& other) const {
        if (bitWidth != other.bitWidth) return true;
        
        // Check upper bound: max + other.max
        int64_t maxTBB = -getSignedMin(bitWidth);  // e.g., 127 for tbb8
        if (max > 0 && other.max > 0 && max > maxTBB - other.max) {
            return true;
        }
        
        // Check lower bound: min + other.min
        int64_t minTBB = getSignedMin(bitWidth) + 1;  // e.g., -127 for tbb8
        if (min < 0 && other.min < 0 && min < minTBB - other.min) {
            return true;
        }
        
        return false;
    }
    
    // Check if subtraction can overflow
    bool subWillOverflow(const TBBValueRange& other) const {
        if (bitWidth != other.bitWidth) return true;
        
        int64_t maxTBB = -getSignedMin(bitWidth);
        int64_t minTBB = getSignedMin(bitWidth) + 1;
        
        // Check upper bound: max - other.min
        if (max > 0 && other.min < 0 && max > maxTBB + other.min) {
            return true;
        }
        
        // Check lower bound: min - other.max
        if (min < 0 && other.max > 0 && min < minTBB + other.max) {
            return true;
        }
        
        return false;
    }
    
    // Check if multiplication can overflow (conservative)
    bool mulWillOverflow(const TBBValueRange& other) const {
        if (bitWidth != other.bitWidth) return true;
        
        int64_t maxTBB = -getSignedMin(bitWidth);
        int64_t minTBB = getSignedMin(bitWidth) + 1;
        
        // Calculate all corner products
        int64_t products[4] = {
            min * other.min,
            min * other.max,
            max * other.min,
            max * other.max
        };
        
        for (int64_t prod : products) {
            if (prod > maxTBB || prod < minTBB) {
                return true;
            }
        }
        
        return false;
    }
    
    // Check if result could collide with sentinel
    bool resultCanBeSentinel(const TBBValueRange& other, unsigned opcode) const {
        int64_t sentinel = getSignedMin(bitWidth);
        
        switch (opcode) {
            case 0: // Add
                return (min + other.min <= sentinel) && (max + other.max >= sentinel);
            case 1: // Sub
                return (min - other.max <= sentinel) && (max - other.min >= sentinel);
            case 2: // Mul
                // Conservative: if range includes values that could multiply to sentinel
                return (min * other.min <= sentinel && max * other.max >= sentinel) ||
                       (min * other.max <= sentinel && max * other.min >= sentinel);
            default:
                return true;
        }
    }
};

/**
 * TBB Arithmetic Optimizer Pass
 * 
 * This is a Function-level optimization pass that analyzes TBB arithmetic
 * patterns and eliminates provably redundant safety checks.
 */
class TBBOptimizerPass : public PassInfoMixin<TBBOptimizerPass> {
public:
    PreservedAnalyses run(Function &F, FunctionAnalysisManager &AM);
    
private:
    /**
     * Analyze a Select instruction to determine if it's a TBB safety pattern
     * Pattern: select(error_condition, sentinel, raw_result)
     * 
     * Returns true if this is a TBB pattern and optimization was applied
     */
    bool optimizeTBBSelect(SelectInst* SI, Function& F);
    
    /**
     * Analyze ICmp instructions feeding into TBB error logic
     * Returns true if the comparison is provably always false (check is redundant)
     */
    bool isRedundantErrorCheck(ICmpInst* cmp, Value* sentinel);
    
    /**
     * Try to compute value range for a given value
     * Uses LLVM's KnownBits and constant propagation
     */
    std::optional<TBBValueRange> computeValueRange(Value* V, unsigned bitWidth);
    
    /**
     * Check if a value is definitely not the sentinel
     * Uses range analysis and known bits
     */
    bool cannotBeSentinel(Value* V, ConstantInt* sentinel);
    
    /**
     * Statistics tracking
     */
    unsigned numInputChecksElided = 0;
    unsigned numOverflowChecksElided = 0;
    unsigned numSentinelChecksElided = 0;
};

} // namespace backend
} // namespace aria
