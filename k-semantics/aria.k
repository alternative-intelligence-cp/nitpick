requires "domains.md"

// Aria v0.18.0 — executable semantics seed.
//
// Scope of this first definition:
//   - mandatory func:main / func:failsafe envelope
//   - zero-argument helper functions returning through pass/fail
//   - int32/int64/tbb32 declarations, assignments, and bounded numeric values
//   - fixed bindings with dynamic immutability enforcement
//   - integer arithmetic, two's-complement int bounds, and sticky ERR propagation
//   - Result construction and unwrapping: pass/fail/raw/drop/defaults/?!
//   - bounded loop(start, end, step) with implicit $ iterator
//   - if/else, comparisons, and exit semantics
//
// This file is intentionally a small golden-oracle nucleus, not a full Aria
// definition. Later v0.18.x slices should extend it module-by-module.

module ARIA-SYNTAX
  imports INT-SYNTAX
  imports BOOL-SYNTAX
  imports ID-SYNTAX

  syntax Type ::= "int32" | "int64" | "tbb32" | "flt32" | "flt64"

  syntax Val ::= Int
               | Bool
               | "NIL"
               | "ERR"
               | "Unknown"
               | "Result" "(" Val "," Val "," Bool ")"
               | "I32" "(" Int ")"
               | "I64" "(" Int ")"
               | "TBB32" "(" Int ")"

  syntax KResult ::= Val

  syntax Exp ::= Val
               | Id
               | Id "(" ")"
               | "$"
               | "(" Exp ")" [bracket]
               > Exp "*" Exp [strict, left]
               | Exp "/" Exp [strict, left]
               > Exp "+" Exp [strict, left]
               | Exp "-" Exp [strict, left]
               > Exp "<" Exp [strict, non-assoc]
               | Exp "<=" Exp [strict, non-assoc]
               | Exp ">" Exp [strict, non-assoc]
               | Exp ">=" Exp [strict, non-assoc]
               | Exp "==" Exp [strict, non-assoc]
               | Exp "!=" Exp [strict, non-assoc]
               > "pass" Exp [strict]
               | "fail" Exp [strict]
               | "raw" Exp [strict]
               | "drop" Exp [strict]
               | "?!" Exp [strict]
               > Exp "defaults" Exp [strict(1), right]

  syntax Stmt ::= Type ":" Id "=" Exp ";" [strict(3)]
                | "fixed" Type ":" Id "=" Exp ";" [strict(3)]
                | Id "=" Exp ";" [strict(2)]
                | "exit" Exp ";" [strict]
                | "exit" "(" Exp ")" ";" [strict]
                | "if" "(" Exp ")" Block "else" Block [strict(1)]
                | "if" "(" Exp ")" Block [strict(1)]
                | "loop" "(" Exp "," Exp "," Exp ")" Block [strict(1, 2, 3)]
                | Exp ";" [strict]

  syntax Block ::= "{" Stmts "}"
  syntax Stmts ::= List{Stmt, ""}

  syntax FuncDecl     ::= "func:" Id "=" Type "(" ")" Block ";"
  syntax FuncDecls    ::= List{FuncDecl, ""}
  syntax FailsafeDecl ::= "func:failsafe" "=" "int32" "(" "tbb32" ":" Id ")" Block ";"
  syntax MainDecl     ::= "func:main" "=" "int32" "(" ")" Block ";"
  syntax Pgm          ::= FuncDecls FailsafeDecl MainDecl
endmodule

module ARIA
  imports ARIA-SYNTAX
  imports INT
  imports BOOL
  imports MAP
  imports SET

  configuration <T>
    <k> $PGM:Pgm </k>
    <functions> .Map </functions>
    <env> .Map </env>
    <types> .Map </types>
    <store> .Map </store>
    <fixed-vars> .Set </fixed-vars>
    <next-loc> 0 </next-loc>
    <loop-index> 0 </loop-index>
    <failsafe> .K </failsafe>
    <failsafe-triggered> false </failsafe-triggered>
    <exit-code> 0 </exit-code>
  </T>

  // -------------------------------------------------------------------------
  // Program envelope
  // -------------------------------------------------------------------------

    rule <k> FUNCS:FuncDecls func:failsafe = int32(tbb32:_ERRID) FS ; func:main = int32() MAIN ; => loadFuncs(FUNCS) ~> MAIN </k>
       <failsafe> _ => FS </failsafe>

    // -------------------------------------------------------------------------
    // Zero-argument helper functions
    // -------------------------------------------------------------------------

    syntax Func ::= "Fn0Pass" "(" Type "," Exp ")"
         | "Fn0Fail" "(" Exp ")"

    syntax KItem ::= "loadFuncs" "(" FuncDecls ")"
          | "finishCallPass" "(" Type ")"
          | "finishCallFail"

    rule <k> loadFuncs(.FuncDecls) => .K ... </k>

    rule <k> loadFuncs(func: F:Id = T:Type() { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
      <functions> Funcs => Funcs [ F <- Fn0Pass(T, E) ] </functions>

    rule <k> loadFuncs(func: F:Id = _T:Type() { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
      <functions> Funcs => Funcs [ F <- Fn0Fail(E) ] </functions>

    rule <k> F:Id() => E ~> finishCallPass(T) ... </k>
      <functions> ... F |-> Fn0Pass(T, E) ... </functions>

    rule <k> F:Id() => E ~> finishCallFail ... </k>
      <functions> ... F |-> Fn0Fail(E) ... </functions>

    rule <k> V:Val ~> finishCallPass(T) => Result(NIL, normalizeValue(T, V), false) ... </k>
    rule <k> V:Val ~> finishCallFail => Result(V, NIL, true) ... </k>

  // -------------------------------------------------------------------------
  // Statement sequencing and blocks
  // -------------------------------------------------------------------------

  rule <k> { SS:Stmts } => SS ... </k>
  rule <k> S:Stmt SS:Stmts => S ~> SS ... </k>
  rule <k> .Stmts => .K ... </k>
  rule <k> _:Val ; => .K ... </k>

  // -------------------------------------------------------------------------
  // Values and variables
  // -------------------------------------------------------------------------

  rule <k> X:Id => V ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> V ... </store>

  rule <k> T:Type : X:Id = V:Val ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- normalizeValue(T, V) ] </store>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> fixed T:Type : X:Id = V:Val ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- normalizeValue(T, V) ] </store>
       <fixed-vars> Fixed => Fixed SetItem(X) </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> X:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <types> ... X |-> T ... </types>
       <store> Store => Store [ L <- normalizeValue(T, V) ] </store>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  // Fixed reassignment is a semantic error in this executable model. ariac
  // should normally reject it statically; the oracle routes it to failsafe so
  // differential tests can detect missed compile-time enforcement.
  rule <k> X:Id = _:Val ; ~> _ => FS </k>
       <fixed-vars> Fixed </fixed-vars>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Fixed

  // -------------------------------------------------------------------------
  // Typed integer normalization and arithmetic
  // -------------------------------------------------------------------------

  syntax Int ::= wrapI32(Int) [function]
               | wrapI64(Int) [function]

  rule wrapI32(I) => wrapI32(I -Int 4294967296) requires I >Int 2147483647
  rule wrapI32(I) => wrapI32(I +Int 4294967296) requires I <Int -2147483648
  rule wrapI32(I) => I requires I >=Int -2147483648 andBool I <=Int 2147483647

  rule wrapI64(I) => wrapI64(I -Int 18446744073709551616) requires I >Int 9223372036854775807
  rule wrapI64(I) => wrapI64(I +Int 18446744073709551616) requires I <Int -9223372036854775808
  rule wrapI64(I) => I requires I >=Int -9223372036854775808 andBool I <=Int 9223372036854775807

  syntax Val ::= normalizeValue(Type, Val) [function]
               | normalizeTBB32(Int) [function]

  rule normalizeValue(int32, I:Int) => I32(wrapI32(I))
  rule normalizeValue(int64, I:Int) => I64(wrapI64(I))
  rule normalizeValue(tbb32, I:Int) => normalizeTBB32(I)
  rule normalizeValue(int32, I32(I)) => I32(wrapI32(I))
  rule normalizeValue(int64, I64(I)) => I64(wrapI64(I))
  rule normalizeValue(tbb32, TBB32(I)) => normalizeTBB32(I)
  rule normalizeValue(tbb32, ERR) => ERR
  rule normalizeValue(_:Type, V:Val) => V [owise]

  rule normalizeTBB32(I) => ERR requires I <=Int -2147483648 orBool I >Int 2147483647
  rule normalizeTBB32(I) => TBB32(I) requires I >Int -2147483648 andBool I <=Int 2147483647

  syntax Int ::= numericValue(Val) [function]
  syntax Bool ::= isNumericValue(Val) [function]

  rule numericValue(I:Int) => I
  rule numericValue(I32(I)) => I
  rule numericValue(I64(I)) => I
  rule numericValue(TBB32(I)) => I

  rule isNumericValue(_:Int) => true
  rule isNumericValue(I32(_:Int)) => true
  rule isNumericValue(I64(_:Int)) => true
  rule isNumericValue(TBB32(_:Int)) => true
  rule isNumericValue(_) => false [owise]

  rule <k> I1:Int + I2:Int => I1 +Int I2 ... </k>
  rule <k> I1:Int - I2:Int => I1 -Int I2 ... </k>
  rule <k> I1:Int * I2:Int => I1 *Int I2 ... </k>
  rule <k> I1:Int / I2:Int => I1 /Int I2 ... </k> requires I2 =/=Int 0
  rule <k> _I:Int / 0 => ERR ... </k>

  rule <k> I32(I1) + I32(I2) => I32(wrapI32(I1 +Int I2)) ... </k>
  rule <k> I32(I1) + I2:Int => I32(wrapI32(I1 +Int I2)) ... </k>
  rule <k> I1:Int + I32(I2) => I32(wrapI32(I1 +Int I2)) ... </k>
  rule <k> I32(I1) - I32(I2) => I32(wrapI32(I1 -Int I2)) ... </k>
  rule <k> I32(I1) - I2:Int => I32(wrapI32(I1 -Int I2)) ... </k>
  rule <k> I1:Int - I32(I2) => I32(wrapI32(I1 -Int I2)) ... </k>
  rule <k> I32(I1) * I32(I2) => I32(wrapI32(I1 *Int I2)) ... </k>
  rule <k> I32(I1) * I2:Int => I32(wrapI32(I1 *Int I2)) ... </k>
  rule <k> I1:Int * I32(I2) => I32(wrapI32(I1 *Int I2)) ... </k>
  rule <k> I32(I1) / I32(I2) => I32(wrapI32(I1 /Int I2)) ... </k> requires I2 =/=Int 0
  rule <k> I32(I1) / I2:Int => I32(wrapI32(I1 /Int I2)) ... </k> requires I2 =/=Int 0
  rule <k> I32(_) / I32(0) => ERR ... </k>
  rule <k> I32(_) / 0 => ERR ... </k>

  rule <k> I64(I1) + I64(I2) => I64(wrapI64(I1 +Int I2)) ... </k>
  rule <k> I64(I1) + I2:Int => I64(wrapI64(I1 +Int I2)) ... </k>
  rule <k> I1:Int + I64(I2) => I64(wrapI64(I1 +Int I2)) ... </k>
  rule <k> I64(I1) - I64(I2) => I64(wrapI64(I1 -Int I2)) ... </k>
  rule <k> I64(I1) - I2:Int => I64(wrapI64(I1 -Int I2)) ... </k>
  rule <k> I1:Int - I64(I2) => I64(wrapI64(I1 -Int I2)) ... </k>
  rule <k> I64(I1) * I64(I2) => I64(wrapI64(I1 *Int I2)) ... </k>
  rule <k> I64(I1) * I2:Int => I64(wrapI64(I1 *Int I2)) ... </k>
  rule <k> I1:Int * I64(I2) => I64(wrapI64(I1 *Int I2)) ... </k>
  rule <k> I64(I1) / I64(I2) => I64(wrapI64(I1 /Int I2)) ... </k> requires I2 =/=Int 0
  rule <k> I64(I1) / I2:Int => I64(wrapI64(I1 /Int I2)) ... </k> requires I2 =/=Int 0
  rule <k> I64(_) / I64(0) => ERR ... </k>
  rule <k> I64(_) / 0 => ERR ... </k>

  rule <k> TBB32(I1) + TBB32(I2) => normalizeTBB32(I1 +Int I2) ... </k>
  rule <k> TBB32(I1) + I2:Int => normalizeTBB32(I1 +Int I2) ... </k>
  rule <k> I1:Int + TBB32(I2) => normalizeTBB32(I1 +Int I2) ... </k>
  rule <k> TBB32(I1) - TBB32(I2) => normalizeTBB32(I1 -Int I2) ... </k>
  rule <k> TBB32(I1) - I2:Int => normalizeTBB32(I1 -Int I2) ... </k>
  rule <k> I1:Int - TBB32(I2) => normalizeTBB32(I1 -Int I2) ... </k>
  rule <k> TBB32(I1) * TBB32(I2) => normalizeTBB32(I1 *Int I2) ... </k>
  rule <k> TBB32(I1) * I2:Int => normalizeTBB32(I1 *Int I2) ... </k>
  rule <k> I1:Int * TBB32(I2) => normalizeTBB32(I1 *Int I2) ... </k>
  rule <k> TBB32(I1) / TBB32(I2) => normalizeTBB32(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB32(I1) / I2:Int => normalizeTBB32(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB32(_) / TBB32(0) => ERR ... </k>
  rule <k> TBB32(_) / 0 => ERR ... </k>

  rule <k> ERR + _:Val => ERR ... </k>
  rule <k> _:Val + ERR => ERR ... </k>
  rule <k> ERR - _:Val => ERR ... </k>
  rule <k> _:Val - ERR => ERR ... </k>
  rule <k> ERR * _:Val => ERR ... </k>
  rule <k> _:Val * ERR => ERR ... </k>
  rule <k> ERR / _:Val => ERR ... </k>
  rule <k> _:Val / ERR => ERR ... </k>

  rule <k> Unknown + _:Val => Unknown ... </k>
  rule <k> _:Val + Unknown => Unknown ... </k>
  rule <k> Unknown - _:Val => Unknown ... </k>
  rule <k> _:Val - Unknown => Unknown ... </k>
  rule <k> Unknown * _:Val => Unknown ... </k>
  rule <k> _:Val * Unknown => Unknown ... </k>
  rule <k> Unknown / _:Val => Unknown ... </k>
  rule <k> _:Val / Unknown => Unknown ... </k>

  // -------------------------------------------------------------------------
  // Comparisons and conditionals
  // -------------------------------------------------------------------------

  rule <k> V1:Val <  V2:Val => numericValue(V1) <Int  numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> V1:Val <= V2:Val => numericValue(V1) <=Int numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> V1:Val >  V2:Val => numericValue(V1) >Int  numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> V1:Val >= V2:Val => numericValue(V1) >=Int numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> V1:Val == V2:Val => numericValue(V1) ==Int numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> V1:Val != V2:Val => numericValue(V1) =/=Int numericValue(V2) ... </k>
    requires isNumericValue(V1) andBool isNumericValue(V2)
  rule <k> B1:Bool == B2:Bool => B1 ==Bool B2 ... </k>
  rule <k> B1:Bool != B2:Bool => B1 =/=Bool B2 ... </k>
  rule <k> ERR == ERR => true ... </k>
  rule <k> ERR != ERR => false ... </k>
  rule <k> ERR == V:Val => false ... </k> requires isNumericValue(V)
  rule <k> V:Val == ERR => false ... </k> requires isNumericValue(V)
  rule <k> ERR != V:Val => true ... </k> requires isNumericValue(V)
  rule <k> V:Val != ERR => true ... </k> requires isNumericValue(V)
  rule <k> Unknown == Unknown => true ... </k>
  rule <k> Unknown != Unknown => false ... </k>

  rule <k> if ( true )  B:Block else _ => B ... </k>
  rule <k> if ( false ) _ else B:Block => B ... </k>
  rule <k> if ( true )  B:Block => B ... </k>
  rule <k> if ( false ) _:Block => .K ... </k>

  // -------------------------------------------------------------------------
  // Result<T> construction, unwrapping, and fallback
  // -------------------------------------------------------------------------

  rule <k> pass V:Val => Result(NIL, V, false) ... </k>
  rule <k> fail E:Val => Result(E, NIL, true) ... </k>
  rule <k> raw Result(_:Val, V:Val, _:Bool) => V ... </k>
  rule <k> drop Result(_:Val, _:Val, _:Bool) => NIL ... </k>
  rule <k> Result(_:Val, V:Val, false) defaults _:Exp => V ... </k>
  rule <k> Result(_:Val, _:Val, true) defaults Fallback:Exp => Fallback ... </k>
  rule <k> ?! Result(_:Val, V:Val, false) => V ... </k>

  syntax Exp ::= "invokeFailsafe" "(" Val ")" [symbol(invokeFailsafe)]

  rule <k> ?! Result(E:Val, _:Val, true) => invokeFailsafe(E) ... </k>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> invokeFailsafe(_:Val) ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> invokeFailsafe(_:Val) ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  // -------------------------------------------------------------------------
  // loop(start, end, step) with implicit $ iterator
  // -------------------------------------------------------------------------

  syntax Stmt ::= "loopIter" "(" Int "," Int "," Int "," Block ")" [symbol(loopIter)]

  rule <k> $ => I ... </k>
       <loop-index> I </loop-index>

  rule <k> loop(Start:Int, End:Int, Step:Int) B:Block => loopIter(Start, End, Step, B) ... </k>

  rule <k> loopIter(Start:Int, End:Int, Step:Int, B:Block) =>
             B ~> loopIter(Start +Int Step, End, Step, B)
       ... </k>
       <loop-index> _ => Start </loop-index>
    requires Step >Int 0 andBool Start <Int End

  rule <k> loopIter(Start:Int, End:Int, Step:Int, B:Block) =>
             B ~> loopIter(Start +Int Step, End, Step, B)
       ... </k>
       <loop-index> _ => Start </loop-index>
    requires Step <Int 0 andBool Start >Int End

  rule <k> loopIter(Start:Int, End:Int, Step:Int, _:Block) => .K ... </k>
    requires (Step >Int 0 andBool Start >=Int End) orBool (Step <Int 0 andBool Start <=Int End)

  // A zero-step loop would not make progress. Route to failsafe.
  rule <k> loopIter(_:Int, _:Int, 0, _:Block) ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  // -------------------------------------------------------------------------
  // Terminal process semantics
  // -------------------------------------------------------------------------

  rule <k> exit V:Val ; ~> _ => .K </k>
       <exit-code> _ => numericValue(V) </exit-code>
    requires isNumericValue(V)

  rule <k> exit ( V:Val ) ; ~> _ => .K </k>
       <exit-code> _ => numericValue(V) </exit-code>
    requires isNumericValue(V)
endmodule