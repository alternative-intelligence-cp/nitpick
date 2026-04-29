requires "domains.md"

// Aria v0.18.0 — executable semantics seed.
//
// Scope of this first definition:
//   - mandatory func:main / func:failsafe envelope
//   - int32/int64/tbb32 declarations and assignments
//   - fixed bindings with dynamic immutability enforcement
//   - integer arithmetic and sticky ERR propagation
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

     syntax KResult ::= Val

  syntax Exp ::= Val
               | Id
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

  syntax FailsafeDecl ::= "func:failsafe" "=" "int32" "(" "tbb32" ":" Id ")" Block ";"
  syntax MainDecl     ::= "func:main" "=" "int32" "(" ")" Block ";"
  syntax Pgm          ::= FailsafeDecl MainDecl
endmodule

module ARIA
  imports ARIA-SYNTAX
  imports INT
  imports BOOL
  imports MAP
  imports SET

  configuration <T>
    <k> $PGM:Pgm </k>
    <env> .Map </env>
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

     rule <k> func:failsafe = int32(tbb32:_ERRID) FS ; func:main = int32() MAIN ; => MAIN </k>
       <failsafe> _ => FS </failsafe>

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

  rule <k> _:Type : X:Id = V:Val ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <store> Store => Store [ L <- V ] </store>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> fixed _:Type : X:Id = V:Val ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <store> Store => Store [ L <- V ] </store>
       <fixed-vars> Fixed => Fixed SetItem(X) </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> X:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> Store => Store [ L <- V ] </store>
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
  // Integer arithmetic and sticky sentinel propagation
  // -------------------------------------------------------------------------

  rule <k> I1:Int + I2:Int => I1 +Int I2 ... </k>
  rule <k> I1:Int - I2:Int => I1 -Int I2 ... </k>
  rule <k> I1:Int * I2:Int => I1 *Int I2 ... </k>
  rule <k> I1:Int / I2:Int => I1 /Int I2 ... </k> requires I2 =/=Int 0
  rule <k> _I:Int / 0 => ERR ... </k>

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

  rule <k> I1:Int <  I2:Int => I1 <Int  I2 ... </k>
  rule <k> I1:Int <= I2:Int => I1 <=Int I2 ... </k>
  rule <k> I1:Int >  I2:Int => I1 >Int  I2 ... </k>
  rule <k> I1:Int >= I2:Int => I1 >=Int I2 ... </k>
  rule <k> I1:Int == I2:Int => I1 ==Int I2 ... </k>
  rule <k> I1:Int != I2:Int => I1 =/=Int I2 ... </k>
  rule <k> B1:Bool == B2:Bool => B1 ==Bool B2 ... </k>
  rule <k> B1:Bool != B2:Bool => B1 =/=Bool B2 ... </k>
  rule <k> ERR == ERR => true ... </k>
  rule <k> ERR != ERR => false ... </k>
     rule <k> ERR == _:Int => false ... </k>
     rule <k> _:Int == ERR => false ... </k>
     rule <k> ERR != _:Int => true ... </k>
     rule <k> _:Int != ERR => true ... </k>
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

  rule <k> exit V:Int ; ~> _ => .K </k>
       <exit-code> _ => V </exit-code>

  rule <k> exit ( V:Int ) ; ~> _ => .K </k>
       <exit-code> _ => V </exit-code>
endmodule