requires "domains.md"

// Aria v0.18.0 — executable semantics seed.
//
// Scope of this first definition:
//   - mandatory func:main / func:failsafe envelope
//   - zero-, one-, and two-argument helper functions returning through pass/fail
//   - isolated helper call frames for parameters and local bindings
//   - one-, two-, and three-field struct literals, field reads, and field writes
//   - Rules declarations and limit<Rules> integer declaration/assignment checks
//   - stack/gc/wild declaration qualifiers with minimal wild free/leak checks
//   - int32/int64/tbb32 declarations, assignments, and bounded numeric values
//   - string literals and stdout-producing print/println builtins
//   - fixed bindings with dynamic immutability enforcement
//   - integer arithmetic, two's-complement int bounds, and sticky ERR propagation
//   - Result construction and unwrapping: pass/fail/raw/drop/defaults/?!
//   - bounded loop(start, end, step) with implicit $ iterator
//   - if/else, pick/fall, comparisons, and exit semantics
//
// This file is intentionally a small golden-oracle nucleus, not a full Aria
// definition. Later v0.18.x slices should extend it module-by-module.

module ARIA-SYNTAX
  imports INT-SYNTAX
  imports BOOL-SYNTAX
  imports STRING-SYNTAX
  imports ID-SYNTAX

     syntax Type ::= "int8" | "int32" | "int64" | "tbb32" | "flt32" | "flt64" | "string" | Type "->" | Id
     syntax MemMode ::= "stack" | "gc" | "wild"

  syntax Val ::= Int
               | Bool
               | String
               | "NIL"
               | "ERR"
               | "Unknown"
               | "Result" "(" Val "," Val "," Bool ")"
               | "I32" "(" Int ")"
               | "I64" "(" Int ")"
               | "TBB32" "(" Int ")"
               | "PTR" "(" Int ")"

  syntax KResult ::= Val

  syntax Exp ::= Val
               | Id
               | "print" "(" Exp ")" [strict]
               | "println" "(" Exp ")" [strict]
               | "alloc" "(" Exp ")" [strict]
               | "free" "(" Id ")"
               | Id "(" ")"
               | Id "(" Exp ")" [strict(2), avoid]
               | Id "(" Exp "," Exp ")" [strict(2, 3), avoid]
               | Id "{" Id ":" Exp "}" [strict(3), avoid]
               | Id "{" Id ":" Exp "," Id ":" Exp "}" [strict(3, 5), avoid]
               | Id "{" Id ":" Exp "," Id ":" Exp "," Id ":" Exp "}" [strict(3, 5, 7), avoid]
               | Exp "." Id [strict(1), avoid]
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
                | "limit" "<" Id ">" Type ":" Id "=" Exp ";" [strict(4)]
                | MemMode Type ":" Id "=" Exp ";" [strict(4)]
                | Id "=" Exp ";" [strict(2)]
                | Id "." Id "=" Exp ";" [strict(3)]
                | "exit" Exp ";" [strict]
                | "exit" "(" Exp ")" ";" [strict]
                | "if" "(" Exp ")" Block "else" Block [strict(1)]
                | "if" "(" Exp ")" Block [strict(1)]
                | "pick" "(" Exp ")" "{" PickCases "}" [strict(1)]
                | "fall" Id ";"
                | "loop" "(" Exp "," Exp "," Exp ")" Block [strict(1, 2, 3)]
                | Exp ";" [strict]

     syntax Block ::= "{" Stmts "}"
     syntax Stmts ::= List{Stmt, ""}

     syntax PickPattern ::= Exp | "*"
     syntax PickCase    ::= "(" PickPattern ")" Block
                                                        | Id ":" "(" PickPattern ")" Block
     syntax PickCases   ::= List{PickCase, ","}

     syntax RuleItem    ::= Exp | "limit" "<" Id ">"
     syntax RuleItems   ::= List{RuleItem, ","}
     syntax RulesDecl   ::= "Rules:" Id "=" "{" RuleItems "}" ";"
     syntax RulesDecls  ::= List{RulesDecl, ""}

     syntax FieldDecl     ::= Type ":" Id ";"
     syntax FieldDecls    ::= List{FieldDecl, ""}
     syntax StructDecl    ::= "struct:" Id "=" "{" FieldDecls "}" ";"
     syntax StructDecls   ::= List{StructDecl, ""}

  syntax Param        ::= Type ":" Id
  syntax FuncDecl     ::= "func:" Id "=" Type "(" ")" Block ";"
                        | "func:" Id "=" Type "(" Param ")" Block ";"
                        | "func:" Id "=" Type "(" Param "," Param ")" Block ";"
  syntax FuncDecls    ::= List{FuncDecl, ""}
  syntax FailsafeDecl ::= "func:failsafe" "=" "int32" "(" "tbb32" ":" Id ")" Block ";"
     syntax MainDecl     ::= "func:main" "=" "int32" "(" ")" Block ";"
     syntax Pgm          ::= RulesDecls StructDecls FuncDecls FailsafeDecl MainDecl
endmodule

module ARIA
  imports ARIA-SYNTAX
  imports INT
  imports BOOL
  imports STRING
  imports MAP
  imports SET

  configuration <T>
    <k> $PGM:Pgm </k>
    <functions> .Map </functions>
     <rules> .Map </rules>
     <limits> .Map </limits>
     <mem-modes> .Map </mem-modes>
     <stack-live> .Set </stack-live>
     <gc-live> .Set </gc-live>
     <wild-live> .Set </wild-live>
          <structs> .Map </structs>
    <env> .Map </env>
    <types> .Map </types>
    <store> .Map </store>
    <fixed-vars> .Set </fixed-vars>
    <stdout> "" </stdout>
    <next-loc> 0 </next-loc>
    <loop-index> 0 </loop-index>
    <failsafe> .K </failsafe>
    <failsafe-triggered> false </failsafe-triggered>
    <exit-code> 0 </exit-code>
  </T>

  // -------------------------------------------------------------------------
  // Program envelope
  // -------------------------------------------------------------------------

  rule <k> RULES:RulesDecls STRUCTS:StructDecls FUNCS:FuncDecls func:failsafe = int32(tbb32:_ERRID) FS ; func:main = int32() MAIN ; => loadRules(RULES) ~> loadStructs(STRUCTS) ~> loadFuncs(FUNCS) ~> MAIN </k>
       <failsafe> _ => FS </failsafe>

  // -------------------------------------------------------------------------
  // Rules declarations and limit<Rules> enforcement
  // -------------------------------------------------------------------------

  syntax RulesInfo ::= "RuleSet" "(" RuleItems ")"

  syntax KItem ::= "loadRules" "(" RulesDecls ")"
                 | "checkLimit" "(" Id "," Val ")"
                 | "checkRuleItems" "(" Val "," RuleItems "," Int ")"
                 | "finishRuleCond" "(" Val "," RuleItems "," Int ")"
                 | "storeLimited" "(" Id "," Type "," Id "," Val ")"
                 | "storeLimitedAssign" "(" Int "," Val ")"

  rule <k> loadRules(.RulesDecls) => .K ... </k>

  rule <k> loadRules(Rules: R:Id = { ITEMS:RuleItems }; REST:RulesDecls) => loadRules(REST) ... </k>
       <rules> Rules => Rules [ R <- RuleSet(ITEMS) ] </rules>

  rule <k> limit < R:Id > T:Type : X:Id = V:Val ; => checkLimit(R, normalizeValue(T, V)) ~> storeLimited(R, T, X, normalizeValue(T, V)) ... </k>

  rule <k> storeLimited(R:Id, T:Type, X:Id, V:Val) => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- V ] </store>
       <limits> Limits => Limits [ X <- R ] </limits>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> X:Id = V:Val ; => checkLimit(R, normalizeValue(T, V)) ~> storeLimitedAssign(L, normalizeValue(T, V)) ... </k>
       <env> ... X |-> L ... </env>
       <types> ... X |-> T ... </types>
       <limits> ... X |-> R ... </limits>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> storeLimitedAssign(L:Int, V:Val) => .K ... </k>
       <store> Store => Store [ L <- V ] </store>

  rule <k> checkLimit(R:Id, V:Val) => checkRuleItems(V, ITEMS, OLD) ... </k>
       <rules> ... R |-> RuleSet(ITEMS) ... </rules>
       <loop-index> OLD => numericValue(V) </loop-index>
    requires isNumericValue(V)

  rule <k> checkRuleItems(_:Val, .RuleItems, OLD:Int) => .K ... </k>
       <loop-index> _ => OLD </loop-index>

  rule <k> checkRuleItems(V:Val, limit < R:Id > , REST:RuleItems, OLD:Int) => checkLimit(R, V) ~> checkRuleItems(V, REST, OLD) ... </k>

  rule <k> checkRuleItems(V:Val, COND:Exp , REST:RuleItems, OLD:Int) => COND ~> finishRuleCond(V, REST, OLD) ... </k>

  rule <k> true ~> finishRuleCond(V:Val, REST:RuleItems, OLD:Int) => checkRuleItems(V, REST, OLD) ... </k>

  rule <k> false ~> finishRuleCond(_:Val, _:RuleItems, OLD:Int) ~> _ => FS </k>
       <loop-index> _ => OLD </loop-index>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  // -------------------------------------------------------------------------
  // Memory allocation qualifiers: stack / gc / wild
  // -------------------------------------------------------------------------

  syntax MemInfo ::= "StackMem" | "GCMem" | "WildMem"

  syntax KItem ::= "storeMemory" "(" MemInfo "," Type "," Id "," Val ")"

  rule <k> stack T:Type : X:Id = V:Val ; => storeMemory(StackMem, T, X, normalizeValue(T, V)) ... </k>
  rule <k> gc    T:Type : X:Id = V:Val ; => storeMemory(GCMem,    T, X, normalizeValue(T, V)) ... </k>
  rule <k> wild  T:Type : X:Id = V:Val ; => storeMemory(WildMem,  T, X, normalizeValue(T, V)) ... </k>

  rule <k> storeMemory(StackMem, T:Type, X:Id, V:Val) => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- V ] </store>
       <mem-modes> Modes => Modes [ X <- StackMem ] </mem-modes>
       <stack-live> Stack => Stack SetItem(L) </stack-live>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> storeMemory(GCMem, T:Type, X:Id, V:Val) => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- V ] </store>
       <mem-modes> Modes => Modes [ X <- GCMem ] </mem-modes>
       <gc-live> GC => GC SetItem(L) </gc-live>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> storeMemory(WildMem, T:Type, X:Id, V:Val) => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- V ] </store>
       <mem-modes> Modes => Modes [ X <- WildMem ] </mem-modes>
       <wild-live> Wild => Wild SetItem(L) </wild-live>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> free(X:Id) => NIL ... </k>
       <env> ... X |-> L ... </env>
       <wild-live> Wild => Wild -Set SetItem(L) </wild-live>
    requires L in Wild

  rule <k> free(X:Id) ~> _ => FS </k>
       <env> ... X |-> L ... </env>
       <wild-live> Wild => .Set </wild-live>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(L in Wild)

     rule <k> alloc(V:Val) => PTR(numericValue(V)) ... </k>
          requires isNumericValue(V)

  // -------------------------------------------------------------------------
  // Struct definitions, literals, field reads, and field writes
  // -------------------------------------------------------------------------

  syntax StructInfo ::= "Struct1" "(" Type "," Id ")"
                      | "Struct2" "(" Type "," Id "," Type "," Id ")"
                      | "Struct3" "(" Type "," Id "," Type "," Id "," Type "," Id ")"

  syntax Val ::= "StructVal" "(" Id "," Map ")"

  syntax KItem ::= "loadStructs" "(" StructDecls ")"

  rule <k> loadStructs(.StructDecls) => .K ... </k>

  rule <k> loadStructs(struct: S:Id = { T1:Type : F1:Id ; }; REST:StructDecls) => loadStructs(REST) ... </k>
       <structs> Structs => Structs [ S <- Struct1(T1, F1) ] </structs>

  rule <k> loadStructs(struct: S:Id = { T1:Type : F1:Id ; T2:Type : F2:Id ; }; REST:StructDecls) => loadStructs(REST) ... </k>
       <structs> Structs => Structs [ S <- Struct2(T1, F1, T2, F2) ] </structs>

  rule <k> loadStructs(struct: S:Id = { T1:Type : F1:Id ; T2:Type : F2:Id ; T3:Type : F3:Id ; }; REST:StructDecls) => loadStructs(REST) ... </k>
       <structs> Structs => Structs [ S <- Struct3(T1, F1, T2, F2, T3, F3) ] </structs>

  rule <k> S:Id { F1:Id : V1:Val } => StructVal(S, F1 |-> normalizeValue(T1, V1)) ... </k>
       <structs> ... S |-> Struct1(T1, F1) ... </structs>

  rule <k> S:Id { F1:Id : V1:Val, F2:Id : V2:Val } => StructVal(S, (F1 |-> normalizeValue(T1, V1) F2 |-> normalizeValue(T2, V2))) ... </k>
       <structs> ... S |-> Struct2(T1, F1, T2, F2) ... </structs>

  rule <k> S:Id { F1:Id : V1:Val, F2:Id : V2:Val, F3:Id : V3:Val } => StructVal(S, (F1 |-> normalizeValue(T1, V1) F2 |-> normalizeValue(T2, V2) F3 |-> normalizeValue(T3, V3))) ... </k>
       <structs> ... S |-> Struct3(T1, F1, T2, F2, T3, F3) ... </structs>

     rule <k> StructVal(_:Id, (F:Id |-> V:Val _:Map)) . F => V ... </k>

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct1(T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(T, F, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(_:Type, _:Id, T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(T, F, _:Type, _:Id, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, T, F, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, _:Type, _:Id, T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
    requires notBool(X in Fixed)

  rule <k> X:Id . _:Id = _:Val ; ~> _ => FS </k>
       <fixed-vars> Fixed </fixed-vars>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Fixed

  // -------------------------------------------------------------------------
  // Helper functions and isolated call frames
  // -------------------------------------------------------------------------

  syntax Func ::= "Fn0Pass" "(" Type "," Exp ")"
                | "Fn0Fail" "(" Exp ")"
                | "Fn1Pass" "(" Type "," Type "," Id "," Exp ")"
                | "Fn1Fail" "(" Type "," Id "," Exp ")"
                | "Fn2Pass" "(" Type "," Type "," Id "," Type "," Id "," Exp ")"
                | "Fn2Fail" "(" Type "," Id "," Type "," Id "," Exp ")"

  syntax KItem ::= "loadFuncs" "(" FuncDecls ")"
                 | "finishCallPass" "(" Type ")"
                 | "finishCallFail"
                 | "finishCallPassFrame" "(" Type "," Map "," Map "," Map "," Map "," Set "," Int ")"
                 | "finishCallFailFrame" "(" Map "," Map "," Map "," Map "," Set "," Int ")"

  rule <k> loadFuncs(.FuncDecls) => .K ... </k>

  rule <k> loadFuncs(func: F:Id = T:Type() { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Pass(T, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type() { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Fail(E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(PT:Type : PX:Id) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Pass(T, PT, PX, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type(PT:Type : PX:Id) { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Fail(PT, PX, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(PT1:Type : PX1:Id, PT2:Type : PX2:Id) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Pass(T, PT1, PX1, PT2, PX2, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type(PT1:Type : PX1:Id, PT2:Type : PX2:Id) { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Fail(PT1, PX1, PT2, PX2, E) ] </functions>

  rule <k> F:Id() => E ~> finishCallPassFrame(T, Env, Types, Store, Limits, Fixed, Next) ... </k>
       <functions> ... F |-> Fn0Pass(T, E) ... </functions>
       <env> Env => .Map </env>
       <types> Types => .Map </types>
       <store> Store => .Map </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

  rule <k> F:Id() => E ~> finishCallFailFrame(Env, Types, Store, Limits, Fixed, Next) ... </k>
       <functions> ... F |-> Fn0Fail(E) ... </functions>
       <env> Env => .Map </env>
       <types> Types => .Map </types>
       <store> Store => .Map </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

  rule <k> F:Id(A:Val) => E ~> finishCallPassFrame(T, Env, Types, Store, Limits, Fixed, L) ... </k>
       <functions> ... F |-> Fn1Pass(T, PT, PX, E) ... </functions>
       <env> Env => PX |-> L </env>
       <types> Types => PX |-> PT </types>
       <store> Store => L |-> normalizeValue(PT, A) </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> F:Id(A:Val) => E ~> finishCallFailFrame(Env, Types, Store, Limits, Fixed, L) ... </k>
       <functions> ... F |-> Fn1Fail(PT, PX, E) ... </functions>
       <env> Env => PX |-> L </env>
       <types> Types => PX |-> PT </types>
       <store> Store => L |-> normalizeValue(PT, A) </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>

  rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallPassFrame(T, Env, Types, Store, Limits, Fixed, L) ... </k>
       <functions> ... F |-> Fn2Pass(T, PT1, PX1, PT2, PX2, E) ... </functions>
       <env> Env => (PX1 |-> L PX2 |-> (L +Int 1)) </env>
       <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
       <store> Store => (L |-> normalizeValue(PT1, A1) (L +Int 1) |-> normalizeValue(PT2, A2)) </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>

  rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallFailFrame(Env, Types, Store, Limits, Fixed, L) ... </k>
       <functions> ... F |-> Fn2Fail(PT1, PX1, PT2, PX2, E) ... </functions>
       <env> Env => (PX1 |-> L PX2 |-> (L +Int 1)) </env>
       <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
       <store> Store => (L |-> normalizeValue(PT1, A1) (L +Int 1) |-> normalizeValue(PT2, A2)) </store>
       <limits> Limits => .Map </limits>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>

  rule <k> V:Val ~> finishCallPass(T) => Result(NIL, normalizeValue(T, V), false) ... </k>
  rule <k> V:Val ~> finishCallFail => Result(V, NIL, true) ... </k>

  rule <k> V:Val ~> finishCallPassFrame(T, Env, Types, Store, Limits, Fixed, Next) => Result(NIL, normalizeValue(T, V), false) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

  rule <k> V:Val ~> finishCallFailFrame(Env, Types, Store, Limits, Fixed, Next) => Result(V, NIL, true) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

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
           <limits> Limits </limits>
       <fixed-vars> Fixed </fixed-vars>
      requires notBool(X in Fixed) andBool notBool(X in_keys(Limits))

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
  rule normalizeValue(string, S:String) => S
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
     // pick/fall pattern dispatch
     // -------------------------------------------------------------------------

     syntax Bool ::= pickMatches(Val, Val) [function]

     rule pickMatches(V1:Val, V2:Val) => numericValue(V1) ==Int numericValue(V2)
          requires isNumericValue(V1) andBool isNumericValue(V2)
     rule pickMatches(B1:Bool, B2:Bool) => B1 ==Bool B2
     rule pickMatches(ERR, ERR) => true
     rule pickMatches(Unknown, Unknown) => true
     rule pickMatches(_, _) => false [owise]

     syntax KItem ::= "selectPick" "(" Val "," PickCases "," PickCases ")"
                                         | "finishPickCase" "(" Val "," Block "," PickCases "," PickCases ")"
                                         | "selectPickLabel" "(" Id "," PickCases ")"
                                         | "endPick" "(" PickCases ")"

     rule <k> pick ( V:Val ) { CASES:PickCases } => selectPick(V, CASES, CASES) ... </k>

     rule <k> selectPick(_:Val, .PickCases, _:PickCases) => .K ... </k>

     rule <k> selectPick(_:Val, (*) B:Block , _:PickCases, ALL:PickCases) => B ~> endPick(ALL) ... </k>
     rule <k> selectPick(_:Val, _:Id : (*) B:Block , _:PickCases, ALL:PickCases) => B ~> endPick(ALL) ... </k>

     rule <k> selectPick(V:Val, (P:Exp) B:Block , REST:PickCases, ALL:PickCases) => P ~> finishPickCase(V, B, REST, ALL) ... </k>
     rule <k> selectPick(V:Val, _:Id : (P:Exp) B:Block , REST:PickCases, ALL:PickCases) => P ~> finishPickCase(V, B, REST, ALL) ... </k>

     rule <k> P:Val ~> finishPickCase(V:Val, B:Block, _:PickCases, ALL:PickCases) => B ~> endPick(ALL) ... </k>
          requires pickMatches(V, P)
     rule <k> P:Val ~> finishPickCase(V:Val, _:Block, REST:PickCases, ALL:PickCases) => selectPick(V, REST, ALL) ... </k>
          requires notBool(pickMatches(V, P))

     rule <k> fall L:Id ; ~> _:Stmts ~> endPick(ALL:PickCases) => selectPickLabel(L, ALL) ~> endPick(ALL) ... </k>

     rule <k> selectPickLabel(_:Id, .PickCases) => .K ... </k>
     rule <k> selectPickLabel(L:Id, L : (_:PickPattern) B:Block , _:PickCases) => B ... </k>
     rule <k> selectPickLabel(L:Id, _:Id : (_:PickPattern) _:Block , REST:PickCases) => selectPickLabel(L, REST) ... </k>
     rule <k> selectPickLabel(L:Id, (_:PickPattern) _:Block , REST:PickCases) => selectPickLabel(L, REST) ... </k>

     rule <k> endPick(_:PickCases) => .K ... </k>

  // -------------------------------------------------------------------------
  // Result<T> construction, unwrapping, and fallback
  // -------------------------------------------------------------------------

  rule <k> pass V:Val => Result(NIL, V, false) ... </k>
  rule <k> fail E:Val => Result(E, NIL, true) ... </k>
  rule <k> raw Result(_:Val, V:Val, _:Bool) => V ... </k>
  rule <k> drop Result(_:Val, _:Val, _:Bool) => NIL ... </k>
  rule <k> drop _:Val => NIL ... </k> [owise]
  rule <k> Result(_:Val, V:Val, false) defaults _:Exp => V ... </k>
  rule <k> Result(_:Val, _:Val, true) defaults Fallback:Exp => Fallback ... </k>
  rule <k> ?! Result(_:Val, V:Val, false) => V ... </k>

  // -------------------------------------------------------------------------
  // String output builtins
  // -------------------------------------------------------------------------

  rule <k> print(S:String) => I64(lengthString(S)) ... </k>
       <stdout> OUT => OUT +String S </stdout>

  rule <k> println(S:String) => I64(lengthString(S) +Int 1) ... </k>
       <stdout> OUT => OUT +String S +String "\n" </stdout>

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
       <wild-live> .Set </wild-live>
    requires isNumericValue(V)

  rule <k> exit ( V:Val ) ; ~> _ => .K </k>
       <exit-code> _ => numericValue(V) </exit-code>
       <wild-live> .Set </wild-live>
    requires isNumericValue(V)

  rule <k> exit _:Val ; ~> _ => FS </k>
       <wild-live> SetItem(_:Int) _:Set => .Set </wild-live>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> exit ( _:Val ) ; ~> _ => FS </k>
       <wild-live> SetItem(_:Int) _:Set => .Set </wild-live>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
endmodule