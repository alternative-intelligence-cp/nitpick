requires "domains.md"

// Nitpick v0.19.4 — executable semantics (extends v0.18.0 nucleus).
//
// v0.18.0 scope:
//   - mandatory func:main / func:failsafe envelope
//   - zero-, one-, and two-argument helper functions returning through pass/fail
//   - isolated helper call frames for parameters and local bindings
//   - one-, two-, and three-field struct literals, field reads, and field writes
//   - Rules declarations and limit<Rules> integer declaration/assignment checks
//   - stack/gc/wild/wildx declaration qualifiers with minimal wild free/leak checks
//   - defer { ... } cleanup blocks with block-scoped LIFO execution
//   - @ address-of, <- dereference, and <-ptr = value store-through for local bindings
//   - ptr->field reads/store-through, including nested pointer-valued field paths
//   - # pin registration with dereference plus direct/nested mutation and by-value call blocking
//   - $$i/$$m borrow qualifiers with nested path-sensitive field tracking and call enforcement
//   - fixed two-element array literals plus literal-index borrow path tracking/writeback
//   - int32/int64/tbb32 declarations, assignments, and bounded numeric values
//   - string literals and stdout-producing print/println builtins
//   - fixed bindings with dynamic immutability enforcement
//   - integer arithmetic, two's-complement int bounds, and sticky ERR propagation
//   - Result construction and unwrapping: pass/fail/raw/drop/defaults/?!
//   - bounded loop(start, end, step) with implicit $ iterator
//   - if/else, pick/fall, comparisons, and exit semantics
//
// v0.19.4 additions:
//   - dynamic-index borrow paths: DynIndexPath/DynIndexAlias, conservative
//     array-level conflict tracking (any dyn borrow on H blocks all other borrows on H)
//   - struct update syntax: S{ ...base, field: val } and S{ ...base, f1: v1, f2: v2 }
//   - struct field patterns in pick: (TypeName { F1, F2 }) { body }
//   - lambda (anonymous function) variables: (RT)(P):x = RT(P) { pass E; };
//   - non-pub helper functions and loop variable scope (K already correct; tests added)

module ARIA-SYNTAX
  imports INT-SYNTAX
  imports BOOL-SYNTAX
  imports STRING-SYNTAX
  imports ID-SYNTAX

     syntax Type ::= "int8" | "int32" | "int64" | "tbb8" | "tbb16" | "tbb32" | "tbb64" | "flt32" | "flt64" | "string" | Type "->" | Type "[" Int "]" | Id
     syntax MemMode ::= "stack" | "gc" | "wild" | "wildx"
     syntax BorrowMode ::= "$$i" | "$$m"
     syntax BorrowInfo ::= "CopyBorrow" | "ImmBorrow" | "MutBorrow"
     syntax BorrowPath ::= "FieldPath" "(" Id "," Id ")"
                         | "NestedFieldPath" "(" Id "," Id "," Id ")"
                         | "IndexPath" "(" Id "," Int ")"
                         | "DynIndexPath" "(" Id "," Int ")"
     syntax BorrowAlias ::= "FieldAlias" "(" Id "," Id ")"
                          | "NestedFieldAlias" "(" Id "," Id "," Id ")"
                          | "IndexAlias" "(" Id "," Int ")"
                          | "DynIndexAlias" "(" Id "," Int ")"

  syntax Val ::= Int
               | Bool
               | String
               | "NIL"
               | "ERR"
               | "Unknown"
               | "Result" "(" Val "," Val "," Bool ")"
               | "I32" "(" Int ")"
               | "I64" "(" Int ")"
               | "TBB8" "(" Int ")"
               | "TBB16" "(" Int ")"
               | "TBB32" "(" Int ")"
               | "TBB64" "(" Int ")"
               | "PTR" "(" Int ")"
               | "PTRLOC" "(" Int ")"
               | "PINPTRLOC" "(" Int ")"
               | "PINPATHLOC" "(" Int ")"
               | "Lambda1" "(" BorrowInfo "," Type "," Id "," Type "," Exp ")"
               | "Lambda2" "(" BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Type "," Exp ")"

  syntax KResult ::= Val

  syntax Exp ::= Val
               | Id
               | "print" "(" Exp ")" [strict]
               | "println" "(" Exp ")" [strict]
               | "alloc" "(" Exp ")" [strict]
               | "free" "(" Id ")"
               | "@" Id
               | "#" Id
               | "<-" Exp [strict]
               | Id "(" ")"
               | Id "(" Exp ")" [strict(2), avoid]
               | Id "(" Exp "," Exp ")" [strict(2, 3), avoid]
               | "[" Exp "," Exp "]" [strict(1, 2)]
               | Id "{" Id ":" Exp "}" [strict(3), avoid]
               | Id "{" Id ":" Exp "," Id ":" Exp "}" [strict(3, 5), avoid]
               | Id "{" Id ":" Exp "," Id ":" Exp "," Id ":" Exp "}" [strict(3, 5, 7), avoid]
               | Id "{" "..." Id "," Id ":" Exp "}" [strict(4), avoid]
               | Id "{" "..." Id "," Id ":" Exp "," Id ":" Exp "}" [strict(4, 6), avoid]
               | Exp "[" Exp "]" [strict, avoid]
               | Exp "." Id [strict(1), avoid]
               | Exp "->" Id [strict(1), avoid]
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
               | "await" Exp [strict]
               | "drop" Exp [strict]
               | "?!" Exp [strict]
               | "comptime" "(" Exp ")" [strict]
               > Exp "defaults" Exp [strict(1), right]

  syntax Stmt ::= Type ":" Id "=" Exp ";" [strict(3)]
                | "fixed" Type ":" Id "=" Exp ";" [strict(3)]
                | "limit" "<" Id ">" Type ":" Id "=" Exp ";" [strict(4)]
                | MemMode Type ":" Id "=" Exp ";" [strict(4)]
                | BorrowMode Type ":" Id "=" Id ";"
                | BorrowMode Type ":" Id "=" Id "." Id ";"
                | BorrowMode Type ":" Id "=" Id "." Id "." Id ";"
                | BorrowMode Type ":" Id "=" Id "[" Int "]" ";"
                | BorrowMode Type ":" Id "=" Id "[" Id "]" ";"
                | "(" Type ")" "(" Type ")" ":" Id "=" Type "(" Param ")" "{" "pass" Exp ";" "}" ";"
                | "(" Type ")" "(" Type "," Type ")" ":" Id "=" Type "(" Param "," Param ")" "{" "pass" Exp ";" "}" ";"
                | Id "=" Exp ";" [strict(2)]
                | Id "[" Exp "]" "=" Exp ";" [strict(2, 3)]
                | "<-" Exp "=" Exp ";" [strict(1, 2)]
                | Id "." Id "=" Exp ";" [strict(3)]
                | Id "." Id "." Id "=" Exp ";" [strict(4)]
                | Exp "->" Id "=" Exp ";" [strict(1, 3)]
                | "exit" Exp ";" [strict]
                | "exit" "(" Exp ")" ";" [strict, prefer]
                | "gc_cycle" ";"
                | "if" "(" Exp ")" Block "else" Block [strict(1)]
                | "if" "(" Exp ")" Block [strict(1)]
                | "pick" "(" Exp ")" "{" PickCases "}" [strict(1)]
                | "fall" Id ";"
                | "loop" "(" Exp "," Exp "," Exp ")" Block [strict(1, 2, 3)]
                | "defer" Block
                | "comptime" Block
                | Block
                | Exp ";" [strict]

     syntax Block ::= "{" Stmts "}"
     syntax Stmts ::= List{Stmt, ""}

     syntax PickPattern ::= Exp | "*"
                          | Id "{" Id "," Id "}"          [symbol(structPat2)]
                          | Id "{" Id "," Id "," Id "}"  [symbol(structPat3)]
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

     syntax Param         ::= Type ":" Id
                                                             | BorrowMode Type ":" Id
  syntax FuncDecl     ::= "func:" Id "=" Type "(" ")" Block ";"
                        | "func:" Id "=" Type "(" Param ")" Block ";"
                        | "func:" Id "=" Type "(" Param "," Param ")" Block ";"
                        | "async" "func:" Id "=" Type "(" ")" Block ";"
                        | "async" "func:" Id "=" Type "(" Param ")" Block ";"
                        | "async" "func:" Id "=" Type "(" Param "," Param ")" Block ";"
                        | "comptime" "func:" Id "=" Type "(" ")" Block ";"
                        | "comptime" "func:" Id "=" Type "(" Param ")" Block ";"
                        | "comptime" "func:" Id "=" Type "(" Param "," Param ")" Block ";"
  syntax FuncDecls    ::= List{FuncDecl, ""}
  syntax FailsafeDecl ::= "func:failsafe" "=" "int32" "(" "tbb32" ":" Id ")" Block ";"
     syntax MainDecl     ::= "func:main" "=" "int32" "(" ")" Block ";"

  // -----------------------------------------------------------------------
  // v0.23.7 MACRO-013: Macro declaration syntax
  //   Macros are expanded before K evaluation (compile-time substitution).
  //   These rules model the declaration and expansion as abstract rewriting.
  //   Supported forms: zero-arg and one-arg; variadic rest-params documented
  //   as an implementation-level concern (K models the common case).
  // -----------------------------------------------------------------------
  syntax MacroDecl  ::= "macro:" Id "=" "(" ")" Block ";"
                      | "macro:" Id "=" "(" Id ")" Block ";"
  syntax MacroDecls ::= List{MacroDecl, ""}
  syntax MacroBody  ::= Block

     syntax Pgm          ::= RulesDecls StructDecls MacroDecls FuncDecls FailsafeDecl MainDecl
endmodule

module ARIA
  imports ARIA-SYNTAX
  imports INT
  imports BOOL
  imports STRING
  imports MAP
  imports SET
     imports LIST
     imports K-EQUAL

  configuration <T>
    <k> $PGM:Pgm </k>
    <functions> .Map </functions>
     <rules> .Map </rules>
     <limits> .Map </limits>
     <mem-modes> .Map </mem-modes>
     <stack-live> .Set </stack-live>
     <gc-live> .Set </gc-live>
     <wild-live> .Set </wild-live>
     <defer-stack> .List </defer-stack>
     <borrow-vars> .Map </borrow-vars>
     <borrow-aliases> .Map </borrow-aliases>
     <imm-borrow-hosts> .Set </imm-borrow-hosts>
     <mut-borrow-hosts> .Set </mut-borrow-hosts>
     <pinned-hosts> .Set </pinned-hosts>
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
    // v0.23.7: MACRO-013 — macro registry cell (name -> body block)
    <macros> .Map </macros>
    // v0.24.7: COMPTIME-014 — comptime function registry (name -> arity tag)
    <comptime-env> .Map </comptime-env>
  </T>

  // -------------------------------------------------------------------------
  // Program envelope
  // -------------------------------------------------------------------------

  rule <k> RULES:RulesDecls STRUCTS:StructDecls MACROS:MacroDecls FUNCS:FuncDecls func:failsafe = int32(tbb32:_ERRID) FS ; func:main = int32() MAIN ; => loadRules(RULES) ~> loadStructs(STRUCTS) ~> loadMacros(MACROS) ~> loadFuncs(FUNCS) ~> MAIN </k>
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
       <pinned-hosts> Pinned </pinned-hosts>
    requires notBool(X in Fixed) andBool notBool(X in Pinned)

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
     // Memory allocation qualifiers: stack / gc / wild / wildx
  // -------------------------------------------------------------------------

     syntax MemInfo ::= "StackMem" | "GCMem" | "WildMem" | "WildXMem"

  syntax KItem ::= "storeMemory" "(" MemInfo "," Type "," Id "," Val ")"

  rule <k> stack T:Type : X:Id = V:Val ; => storeMemory(StackMem, T, X, normalizeValue(T, V)) ... </k>
  rule <k> gc    T:Type : X:Id = V:Val ; => storeMemory(GCMem,    T, X, normalizeValue(T, V)) ... </k>
  rule <k> wild  T:Type : X:Id = V:Val ; => storeMemory(WildMem,  T, X, normalizeValue(T, V)) ... </k>
     rule <k> wildx T:Type : X:Id = V:Val ; => storeMemory(WildXMem, T, X, normalizeValue(T, V)) ... </k>

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

  rule <k> storeMemory(WildXMem, T:Type, X:Id, V:Val) => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <types> Types => Types [ X <- T ] </types>
       <store> Store => Store [ L <- V ] </store>
       <mem-modes> Modes => Modes [ X <- WildXMem ] </mem-modes>
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

     // Local pointer slice: @x captures the store location of a local binding,
     // <-ptr reads the current value at that location, and <-ptr = value writes
     // back through that captured location. #x returns a pinned pointer that can
     // read the host but cannot write through the pin reference.
  syntax Bool ::= isStorePointer(Val) [function]

  rule isStorePointer(PTRLOC(_:Int)) => true
     rule isStorePointer(PINPTRLOC(_:Int)) => true
       rule isStorePointer(PINPATHLOC(_:Int)) => true
  rule isStorePointer(_) => false [owise]

     rule <k> @ X:Id => PTRLOC(L) ... </k>
          <env> ... X |-> L ... </env>

     rule <k> # X:Id => PINPTRLOC(L) ... </k>
          <env> ... X |-> L ... </env>
          <pinned-hosts> Pinned => Pinned SetItem(X) </pinned-hosts>
     requires notBool(X in Pinned)

  rule <k> # X:Id ~> _ => FS </k>
       <env> Env </env>
       <pinned-hosts> Pinned </pinned-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(X in_keys(Env)) orBool X in Pinned

  rule <k> <- PTRLOC(L:Int) => V ... </k>
       <store> ... L |-> V ... </store>

  rule <k> <- PINPTRLOC(L:Int) => V ... </k>
       <store> ... L |-> V ... </store>

  rule <k> <- PINPATHLOC(L:Int) => V ... </k>
       <store> ... L |-> V ... </store>

  rule <k> <- PTRLOC(L:Int) = V:Val ; => .K ... </k>
       <store> ... L |-> (OLD:Val => normalizeLike(OLD, V)) ... </store>

  rule <k> <- PINPTRLOC(_:Int) = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> <- PINPATHLOC(_:Int) = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> <- V:Val = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(isStorePointer(V))

  rule <k> <- V:Val ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(isStorePointer(V))

  // -------------------------------------------------------------------------
  // gc_cycle: abstract garbage-collector cycle.
  // Pinned objects are address-stable by construction: a gc_cycle reduces
  // to .K and touches none of <env>, <store>, <pinned-hosts>, so any pin's
  // location is invariant across any sequence of gc_cycle reductions.
  // -------------------------------------------------------------------------
  rule <k> gc_cycle ; => .K ... </k>

  // -------------------------------------------------------------------------
  // Borrow permission qualifiers: $$i / $$m
  // -------------------------------------------------------------------------

  syntax Type ::= paramType(Param) [function]
  syntax Id ::= paramName(Param) [function]
  syntax BorrowInfo ::= paramBorrow(Param) [function]
  syntax Val ::= argumentValue(Val) [function]
  syntax Bool ::= acceptsBorrowArg(BorrowInfo, Val) [function]
                | isImmutableBorrowVar(Id, Map) [function]
                | isStructVal(Val) [function]
                | isLambdaVal(Val) [function]
                          | hasBorrowOnHost(Id, Set) [function]
                          | hasBorrowOnFieldPath(Id, Id, Set) [function]
                          | hasBorrowOnNestedPath(Id, Id, Id, Set) [function]
                                          | hasBorrowOnIndexPath(Id, Int, Set) [function]
                                          | hasDynBorrowOnArray(Id, Set) [function]
     syntax Int ::= envLoc(Map, Id) [function]
  syntax Val ::= fieldValue(Map, Int, Id) [function]
                         | nestedFieldValue(Map, Int, Id, Id) [function]
                                         | arrayValue(Map, Int, Int) [function]
                         | storeGet(Map, Int) [function]
  syntax Int ::= extractInt(Val) [function]
  syntax Map ::= paramBorrowMap(Id, BorrowInfo) [function]
               | callParamStore(Map, Type, BorrowInfo, Int, Val) [function]
               | callWritebacks(BorrowInfo, Val) [function]
               | writebackStore(Map, Map, Map) [function]
  syntax Int ::= callParamLoc(Int, BorrowInfo, Val) [function]

  rule paramType(T:Type : _:Id) => T
  rule paramType($$i T:Type : _:Id) => T
  rule paramType($$m T:Type : _:Id) => T

  rule paramName(_:Type : X:Id) => X
  rule paramName($$i _:Type : X:Id) => X
  rule paramName($$m _:Type : X:Id) => X

  rule paramBorrow(_:Type : _:Id) => CopyBorrow
  rule paramBorrow($$i _:Type : _:Id) => ImmBorrow
  rule paramBorrow($$m _:Type : _:Id) => MutBorrow

  rule argumentValue(V:Val) => V [owise]

  rule acceptsBorrowArg(MutBorrow, _) => false [owise]
  rule acceptsBorrowArg(ImmBorrow, _) => true
  rule acceptsBorrowArg(CopyBorrow, _) => true

     rule callParamLoc(L:Int, _:BorrowInfo, _:Val) => L [owise]

     rule callParamStore(Store:Map, PT:Type, B:BorrowInfo, L:Int, A:Val) => Store [ callParamLoc(L, B, A) <- normalizeValue(PT, argumentValue(A)) ]

     rule callWritebacks(_:BorrowInfo, _:Val) => .Map [owise]

     rule writebackStore(Saved:Map, _:Map, .Map) => Saved
     rule writebackStore(Saved:Map, Current:Map, (L:Int |-> _:Bool REST:Map)) => writebackStore(Saved [ L <- Current [ L ] ], Current, REST)

  rule paramBorrowMap(_:Id, CopyBorrow) => .Map
  rule paramBorrowMap(X:Id, ImmBorrow) => X |-> ImmBorrow
  rule paramBorrowMap(X:Id, MutBorrow) => X |-> MutBorrow

  rule isImmutableBorrowVar(X:Id, (X |-> ImmBorrow _:Map)) => true
  rule isImmutableBorrowVar(X:Id, (X |-> MutBorrow _:Map)) => false
  rule isImmutableBorrowVar(_:Id, _:Map) => false [owise]

  rule isStructVal(StructVal(_:Id, _:Map)) => true
  rule isStructVal(_:Val) => false [owise]

  rule isLambdaVal(Lambda1(_:BorrowInfo, _:Type, _:Id, _:Type, _:Exp)) => true
  rule isLambdaVal(Lambda2(_:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Type, _:Exp)) => true
  rule isLambdaVal(_:Val) => false [owise]

     rule hasBorrowOnHost(_:Id, .Set) => false
     rule hasBorrowOnHost(H:Id, SetItem(H) _:Set) => true
     rule hasBorrowOnHost(H:Id, SetItem(FieldPath(H, _:Id)) _:Set) => true
     rule hasBorrowOnHost(H:Id, SetItem(NestedFieldPath(H, _:Id, _:Id)) _:Set) => true
     rule hasBorrowOnHost(H:Id, SetItem(IndexPath(H, _:Int)) _:Set) => true
     rule hasBorrowOnHost(H:Id, SetItem(DynIndexPath(H, _:Int)) _:Set) => true
     rule hasBorrowOnHost(H:Id, SetItem(_:KItem) REST:Set) => hasBorrowOnHost(H, REST) [owise]

     rule hasBorrowOnFieldPath(_:Id, _:Id, .Set) => false
     rule hasBorrowOnFieldPath(H:Id, F:Id, SetItem(FieldPath(H, F)) _:Set) => true
     rule hasBorrowOnFieldPath(H:Id, F:Id, SetItem(NestedFieldPath(H, F, _:Id)) _:Set) => true
     rule hasBorrowOnFieldPath(H:Id, F:Id, SetItem(_:KItem) REST:Set) => hasBorrowOnFieldPath(H, F, REST) [owise]

     rule hasBorrowOnNestedPath(_:Id, _:Id, _:Id, .Set) => false
     rule hasBorrowOnNestedPath(H:Id, F1:Id, _:Id, SetItem(FieldPath(H, F1)) _:Set) => true
     rule hasBorrowOnNestedPath(H:Id, F1:Id, F2:Id, SetItem(NestedFieldPath(H, F1, F2)) _:Set) => true
     rule hasBorrowOnNestedPath(H:Id, F1:Id, F2:Id, SetItem(_:KItem) REST:Set) => hasBorrowOnNestedPath(H, F1, F2, REST) [owise]

     rule hasBorrowOnIndexPath(_:Id, _:Int, .Set) => false
     rule hasBorrowOnIndexPath(H:Id, I:Int, SetItem(IndexPath(H, I)) _:Set) => true
     rule hasBorrowOnIndexPath(H:Id, I:Int, SetItem(_:KItem) REST:Set) => hasBorrowOnIndexPath(H, I, REST) [owise]

     syntax Bool ::= "hasDynBorrowOnArray" "(" Id "," Set ")" [function]
     rule hasDynBorrowOnArray(_:Id, .Set) => false
     rule hasDynBorrowOnArray(H:Id, SetItem(DynIndexPath(H, _:Int)) _:Set) => true
     rule hasDynBorrowOnArray(H:Id, SetItem(_:KItem) REST:Set) => hasDynBorrowOnArray(H, REST) [owise]

     rule envLoc((H:Id |-> L:Int _:Map), H) => L
     rule fieldValue((L:Int |-> StructVal(_:Id, (F:Id |-> V:Val _:Map)) _:Map), L, F) => V
     rule nestedFieldValue((L:Int |-> StructVal(_:Id, (F1:Id |-> StructVal(_:Id, (F2:Id |-> V:Val _:Map)) _:Map)) _:Map), L, F1, F2) => V
     rule arrayValue((L:Int |-> ArrayVal((I:Int |-> V:Val _:Map)) _:Map), L, I) => V

     // storeGet: map lookup without M[K] syntax (avoids parse ambiguity in function args)
     rule storeGet((L:Int |-> V:Val _:Map), L) => V
     // extractInt: extract bare Int from typed value (for array index from int32/int64 variable)
     rule extractInt(I32(I:Int)) => I
     rule extractInt(I64(I:Int)) => I
     rule extractInt(I:Int) => I

  rule <k> $$i T:Type : X:Id = H:Id ; => .K ... </k>
       <env> Env => Env [ X <- Env [ H ] ] </env>
       <types> Types => Types [ X <- T ] </types>
       <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
       <imm-borrow-hosts> Imm => Imm SetItem(H) </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
      requires H in_keys(Env) andBool notBool(hasBorrowOnHost(H, Mut))

  rule <k> $$m T:Type : X:Id = H:Id ; => .K ... </k>
       <env> Env => Env [ X <- Env [ H ] ] </env>
       <types> Types => Types [ X <- T ] </types>
       <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
       <mut-borrow-hosts> Mut => Mut SetItem(H) </mut-borrow-hosts>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires H in_keys(Env) andBool notBool(hasBorrowOnHost(H, Mut)) andBool notBool(hasBorrowOnHost(H, Imm)) andBool notBool(H in Pinned)

  rule <k> $$i T:Type : X:Id = H:Id . F:Id ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, fieldValue(Store, envLoc(Env, H), F)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
           <imm-borrow-hosts> Imm => Imm SetItem(FieldPath(H, F)) </imm-borrow-hosts>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(FieldPath(H, F) in Mut)

  rule <k> $$m T:Type : X:Id = H:Id . F:Id ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, fieldValue(Store, envLoc(Env, H), F)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
           <borrow-aliases> Aliases => Aliases [ X <- FieldAlias(H, F) ] </borrow-aliases>
           <mut-borrow-hosts> Mut => Mut SetItem(FieldPath(H, F)) </mut-borrow-hosts>
           <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(H in Imm) andBool notBool(H in Pinned)
       andBool notBool(FieldPath(H, F) in Mut) andBool notBool(FieldPath(H, F) in Imm)

  rule <k> $$i T:Type : X:Id = H:Id . F1:Id . F2:Id ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, nestedFieldValue(Store, envLoc(Env, H), F1, F2)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
           <imm-borrow-hosts> Imm => Imm SetItem(NestedFieldPath(H, F1, F2)) </imm-borrow-hosts>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(hasBorrowOnNestedPath(H, F1, F2, Mut))

  rule <k> $$m T:Type : X:Id = H:Id . F1:Id . F2:Id ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, nestedFieldValue(Store, envLoc(Env, H), F1, F2)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
           <borrow-aliases> Aliases => Aliases [ X <- NestedFieldAlias(H, F1, F2) ] </borrow-aliases>
           <mut-borrow-hosts> Mut => Mut SetItem(NestedFieldPath(H, F1, F2)) </mut-borrow-hosts>
           <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(H in Imm) andBool notBool(H in Pinned)
       andBool notBool(hasBorrowOnNestedPath(H, F1, F2, Mut)) andBool notBool(hasBorrowOnNestedPath(H, F1, F2, Imm))

  rule <k> $$i T:Type : X:Id = H:Id [ I:Int ] ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, arrayValue(Store, envLoc(Env, H), I)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
           <imm-borrow-hosts> Imm => Imm SetItem(IndexPath(H, I)) </imm-borrow-hosts>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(hasBorrowOnIndexPath(H, I, Mut))
       andBool notBool(hasDynBorrowOnArray(H, Mut))

  rule <k> $$m T:Type : X:Id = H:Id [ I:Int ] ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, arrayValue(Store, envLoc(Env, H), I)) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
           <borrow-aliases> Aliases => Aliases [ X <- IndexAlias(H, I) ] </borrow-aliases>
           <mut-borrow-hosts> Mut => Mut SetItem(IndexPath(H, I)) </mut-borrow-hosts>
           <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
      requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(H in Imm) andBool notBool(H in Pinned)
       andBool notBool(hasBorrowOnIndexPath(H, I, Mut)) andBool notBool(hasBorrowOnIndexPath(H, I, Imm))
       andBool notBool(hasDynBorrowOnArray(H, Mut)) andBool notBool(hasDynBorrowOnArray(H, Imm))

  // -------------------------------------------------------------------------
  // Dynamic-index borrow paths: $$i/$$m T:X = H[Idx] where Idx is a variable
  // Conservative model: any dynamic borrow on H conflicts with any other borrow on H.
  // -------------------------------------------------------------------------

  rule <k> $$i T:Type : X:Id = H:Id [ Idx:Id ] ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, arrayValue(Store, envLoc(Env, H), extractInt(storeGet(Store, envLoc(Env, Idx))))) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
           <imm-borrow-hosts> Imm => Imm SetItem(DynIndexPath(H, extractInt(storeGet(Store, envLoc(Env, Idx))))) </imm-borrow-hosts>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
      requires H in_keys(Env) andBool Idx in_keys(Env)
       andBool notBool(H in Mut)
       andBool notBool(hasDynBorrowOnArray(H, Mut))
       andBool notBool(hasBorrowOnHost(H, Mut)) [priority(25)]

  rule <k> $$m T:Type : X:Id = H:Id [ Idx:Id ] ; => .K ... </k>
           <env> Env => Env [ X <- L ] </env>
           <types> Types => Types [ X <- T ] </types>
           <store> Store => Store [ L <- normalizeValue(T, arrayValue(Store, envLoc(Env, H), extractInt(storeGet(Store, envLoc(Env, Idx))))) ] </store>
           <next-loc> L => L +Int 1 </next-loc>
           <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
           <borrow-aliases> Aliases => Aliases [ X <- DynIndexAlias(H, extractInt(storeGet(Store, envLoc(Env, Idx)))) ] </borrow-aliases>
           <mut-borrow-hosts> Mut => Mut SetItem(DynIndexPath(H, extractInt(storeGet(Store, envLoc(Env, Idx))))) </mut-borrow-hosts>
           <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
      requires H in_keys(Env) andBool Idx in_keys(Env)
       andBool notBool(H in Mut) andBool notBool(H in Imm) andBool notBool(H in Pinned)
       andBool notBool(hasDynBorrowOnArray(H, Mut)) andBool notBool(hasDynBorrowOnArray(H, Imm))
       andBool notBool(hasBorrowOnHost(H, Mut)) andBool notBool(hasBorrowOnHost(H, Imm)) [priority(25)]

  rule <k> $$i _:Type : _:Id = H:Id ; ~> _ => FS </k>
       <env> Env </env>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
      requires notBool(H in_keys(Env)) orBool hasBorrowOnHost(H, Mut)

  rule <k> $$m _:Type : _:Id = H:Id ; ~> _ => FS </k>
       <env> Env </env>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
         requires notBool(H in_keys(Env)) orBool hasBorrowOnHost(H, Mut) orBool hasBorrowOnHost(H, Imm) orBool H in Pinned

       rule <k> $$i _:Type : _:Id = H:Id . F:Id ; ~> _ => FS </k>
           <env> Env </env>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
           <failsafe> FS </failsafe>
           <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool H in Mut orBool hasBorrowOnFieldPath(H, F, Mut) [priority(25)]

       rule <k> $$m _:Type : _:Id = H:Id . F:Id ; ~> _ => FS </k>
           <env> Env </env>
           <mut-borrow-hosts> Mut </mut-borrow-hosts>
           <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
           <failsafe> FS </failsafe>
           <failsafe-triggered> _ => true </failsafe-triggered>
         requires notBool(H in_keys(Env)) orBool H in Mut orBool H in Imm orBool H in Pinned
                         orBool hasBorrowOnFieldPath(H, F, Mut) orBool hasBorrowOnFieldPath(H, F, Imm) [priority(25)]

                rule <k> $$i _:Type : _:Id = H:Id . F1:Id . F2:Id ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool H in Mut orBool hasBorrowOnNestedPath(H, F1, F2, Mut) [priority(25)]

                rule <k> $$m _:Type : _:Id = H:Id . F1:Id . F2:Id ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <imm-borrow-hosts> Imm </imm-borrow-hosts>
                          <pinned-hosts> Pinned </pinned-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool H in Mut orBool H in Imm orBool H in Pinned
                         orBool hasBorrowOnNestedPath(H, F1, F2, Mut) orBool hasBorrowOnNestedPath(H, F1, F2, Imm) [priority(25)]

                rule <k> $$i _:Type : _:Id = H:Id [ I:Int ] ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool H in Mut orBool hasBorrowOnIndexPath(H, I, Mut)
                               orBool hasDynBorrowOnArray(H, Mut) [priority(25)]

                rule <k> $$m _:Type : _:Id = H:Id [ I:Int ] ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <imm-borrow-hosts> Imm </imm-borrow-hosts>
                          <pinned-hosts> Pinned </pinned-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool H in Mut orBool H in Imm orBool H in Pinned
                               orBool hasBorrowOnIndexPath(H, I, Mut) orBool hasBorrowOnIndexPath(H, I, Imm)
                               orBool hasDynBorrowOnArray(H, Mut) orBool hasDynBorrowOnArray(H, Imm) [priority(25)]

                // DynIndex borrow failsafe rules
                rule <k> $$i _:Type : _:Id = H:Id [ Idx:Id ] ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool notBool(Idx in_keys(Env))
                               orBool H in Mut orBool hasDynBorrowOnArray(H, Mut)
                               orBool hasBorrowOnHost(H, Mut) [priority(24)]

                rule <k> $$m _:Type : _:Id = H:Id [ Idx:Id ] ; ~> _ => FS </k>
                          <env> Env </env>
                          <mut-borrow-hosts> Mut </mut-borrow-hosts>
                          <imm-borrow-hosts> Imm </imm-borrow-hosts>
                          <pinned-hosts> Pinned </pinned-hosts>
                          <failsafe> FS </failsafe>
                          <failsafe-triggered> _ => true </failsafe-triggered>
                     requires notBool(H in_keys(Env)) orBool notBool(Idx in_keys(Env))
                               orBool H in Mut orBool H in Imm orBool H in Pinned
                               orBool hasDynBorrowOnArray(H, Mut) orBool hasDynBorrowOnArray(H, Imm)
                               orBool hasBorrowOnHost(H, Mut) orBool hasBorrowOnHost(H, Imm) [priority(24)]

     // -------------------------------------------------------------------------
     // Fixed two-element arrays and literal index paths
     // -------------------------------------------------------------------------

     syntax Val ::= "ArrayVal" "(" Map ")"

     rule <k> [ V1:Val , V2:Val ] => ArrayVal(0 |-> V1 1 |-> V2) ... </k>

     rule <k> ArrayVal(Items:Map) [ I:Int ] => Items [ I ] ... </k>
          requires I in_keys(Items)

     rule <k> ArrayVal(Items:Map) [ I:Int ] ~> _ => FS </k>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires notBool(I in_keys(Items))

     rule <k> X:Id [ I:Int ] = V:Val ; => .K ... </k>
                <env> ... X |-> L ... </env>
                <store> ... L |-> (ArrayVal((I |-> OLD:Val REST:Map)) => ArrayVal((I |-> normalizeLike(OLD, V)) REST)) ... </store>
                <fixed-vars> Fixed </fixed-vars>
                <imm-borrow-hosts> Imm </imm-borrow-hosts>
                <mut-borrow-hosts> Mut </mut-borrow-hosts>
                <pinned-hosts> Pinned </pinned-hosts>
               requires notBool(X in Fixed) andBool notBool(X in Pinned)
                andBool notBool(X in Imm) andBool notBool(X in Mut)
                andBool notBool(hasBorrowOnIndexPath(X, I, Imm)) andBool notBool(hasBorrowOnIndexPath(X, I, Mut))

     rule <k> X:Id [ I:Int ] = _:Val ; ~> _ => FS </k>
                <imm-borrow-hosts> Imm </imm-borrow-hosts>
                <mut-borrow-hosts> Mut </mut-borrow-hosts>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Imm orBool X in Mut orBool X in Pinned orBool hasBorrowOnIndexPath(X, I, Imm) orBool hasBorrowOnIndexPath(X, I, Mut) [priority(25)]

     rule <k> X:Id [ _:Int ] = _:Val ; ~> _ => FS </k>
                <fixed-vars> Fixed </fixed-vars>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Fixed [priority(25)]

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

     rule <k> PTRLOC(L:Int) -> F:Id => V ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> V:Val _:Map)) ... </store>

     rule <k> PINPTRLOC(L:Int) -> F:Id => V ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> V:Val _:Map)) ... </store>
    requires notBool(isStorePointer(V))

     rule <k> PINPTRLOC(L:Int) -> F:Id => PINPATHLOC(PL) ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> PTRLOC(PL) _:Map)) ... </store>

     rule <k> PINPTRLOC(L:Int) -> F:Id => PINPATHLOC(PL) ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> PINPTRLOC(PL) _:Map)) ... </store>

     rule <k> PINPTRLOC(L:Int) -> F:Id => PINPATHLOC(PL) ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> PINPATHLOC(PL) _:Map)) ... </store>

     rule <k> PINPATHLOC(L:Int) -> F:Id => V ... </k>
          <store> ... L |-> StructVal(_:Id, (F |-> V:Val _:Map)) ... </store>

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct1(T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(T, F, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(_:Type, _:Id, T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(T, F, _:Type, _:Id, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, T, F, _:Type, _:Id) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . F:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, _:Type, _:Id, T, F) ... </structs>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(FieldPath(X, F) in Imm) andBool notBool(FieldPath(X, F) in Mut)

  rule <k> X:Id . _:Id = _:Val ; ~> _ => FS </k>
           <pinned-hosts> Pinned </pinned-hosts>
           <failsafe> FS </failsafe>
           <failsafe-triggered> _ => true </failsafe-triggered>
      requires X in Pinned

  rule <k> X:Id . F:Id = _:Val ; ~> _ => FS </k>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Imm orBool X in Mut orBool hasBorrowOnFieldPath(X, F, Imm) orBool hasBorrowOnFieldPath(X, F, Mut) [priority(25)]

  rule <k> X:Id . _:Id = _:Val ; ~> _ => FS </k>
       <fixed-vars> Fixed </fixed-vars>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Fixed

  rule <k> X:Id . F1:Id . F2:Id = V:Val ; => .K ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> (StructVal(S, (F1 |-> StructVal(NS, (F2 |-> OLD:Val NFields:Map)) Fields:Map)) => StructVal(S, (F1 |-> StructVal(NS, (F2 |-> normalizeLike(OLD, V)) NFields)) Fields)) ... </store>
       <fixed-vars> Fixed </fixed-vars>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in Pinned)
       andBool notBool(X in Imm) andBool notBool(X in Mut)
       andBool notBool(hasBorrowOnNestedPath(X, F1, F2, Imm)) andBool notBool(hasBorrowOnNestedPath(X, F1, F2, Mut))

  rule <k> X:Id . _:Id . _:Id = _:Val ; ~> _ => FS </k>
           <pinned-hosts> Pinned </pinned-hosts>
           <failsafe> FS </failsafe>
           <failsafe-triggered> _ => true </failsafe-triggered>
      requires X in Pinned [priority(25)]

  rule <k> X:Id . F1:Id . F2:Id = _:Val ; ~> _ => FS </k>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Imm orBool X in Mut orBool hasBorrowOnNestedPath(X, F1, F2, Imm) orBool hasBorrowOnNestedPath(X, F1, F2, Mut) [priority(25)]

  rule <k> X:Id . _:Id . _:Id = _:Val ; ~> _ => FS </k>
       <fixed-vars> Fixed </fixed-vars>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires X in Fixed [priority(25)]

  // -------------------------------------------------------------------------
  // Struct update expressions: S{ ...base, field: val }
  // Reads base variable's StructVal, returns new StructVal with field(s) updated.
  // -------------------------------------------------------------------------

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct1(T, F) ... </structs>

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct2(T, F, _:Type, _:Id) ... </structs>

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct2(_:Type, _:Id, T, F) ... </structs>

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(T, F, _:Type, _:Id, _:Type, _:Id) ... </structs>

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, T, F, _:Type, _:Id) ... </structs>

  rule <k> S:Id { ... H:Id , F:Id : V:Val } => StructVal(S, Fields [ F <- normalizeValue(T, V) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, _:Type, _:Id, T, F) ... </structs>

  // Two-field struct update
  rule <k> S:Id { ... H:Id , F1:Id : V1:Val , F2:Id : V2:Val } => StructVal(S, Fields [ F1 <- normalizeValue(T1, V1) ] [ F2 <- normalizeValue(T2, V2) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct2(T1, F1, T2, F2) ... </structs>

  rule <k> S:Id { ... H:Id , F1:Id : V1:Val , F2:Id : V2:Val } => StructVal(S, Fields [ F1 <- normalizeValue(T1, V1) ] [ F2 <- normalizeValue(T2, V2) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(T1, F1, T2, F2, _:Type, _:Id) ... </structs>

  rule <k> S:Id { ... H:Id , F1:Id : V1:Val , F2:Id : V2:Val } => StructVal(S, Fields [ F1 <- normalizeValue(T1, V1) ] [ F2 <- normalizeValue(T2, V2) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(T1, F1, _:Type, _:Id, T2, F2) ... </structs>

  rule <k> S:Id { ... H:Id , F1:Id : V1:Val , F2:Id : V2:Val } => StructVal(S, Fields [ F1 <- normalizeValue(T1, V1) ] [ F2 <- normalizeValue(T2, V2) ]) ... </k>
       <env> ... H |-> L ... </env>
       <store> ... L |-> StructVal(S, Fields) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, T1, F1, T2, F2) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct1(T, F) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(T, F, _:Type, _:Id) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct2(_:Type, _:Id, T, F) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(T, F, _:Type, _:Id, _:Type, _:Id) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, T, F, _:Type, _:Id) ... </structs>

  rule <k> PTRLOC(L:Int) -> F:Id = V:Val ; => .K ... </k>
       <store> ... L |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
       <structs> ... S |-> Struct3(_:Type, _:Id, _:Type, _:Id, T, F) ... </structs>

  rule <k> PINPTRLOC(_:Int) -> _:Id = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> PINPATHLOC(_:Int) -> _:Id = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> V:Val -> _:Id = _:Val ; ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(isStorePointer(V))

  rule <k> V:Val -> _:Id ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(isStorePointer(V))

  // -------------------------------------------------------------------------
  // Helper functions and isolated call frames
  // -------------------------------------------------------------------------

  syntax Func ::= "Fn0Pass" "(" Type "," Exp ")"
                          | "Fn0Fail" "(" Exp ")"
                          | "Fn1Pass" "(" Type "," BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn1StmtPass" "(" Type "," BorrowInfo "," Type "," Id "," Stmt "," Exp ")"
                          | "Fn1Fail" "(" BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn2Pass" "(" Type "," BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn2StmtPass" "(" Type "," BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Stmt "," Exp ")"
                          | "Fn2Fail" "(" BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Exp ")"

     syntax FrameState ::= "FrameState" "(" Map "," Map "," Map "," Map "," Map "," Set "," Set "," Set "," Map "," Map "," Set "," Set "," Set "," Set "," Int ")"

  syntax KItem ::= "loadFuncs" "(" FuncDecls ")"
                 | "finishCallPass" "(" Type ")"
                 | "finishCallFail"
                 | "finishCallPassFrame" "(" Type "," FrameState ")"
                 | "finishCallFailFrame" "(" FrameState ")"
                 | "finishCallPassFrameWriteback" "(" Type "," FrameState "," Map ")"
                 | "finishCallFailFrameWriteback" "(" FrameState "," Map ")"
                 | "finishFn2MutFirstArg" "(" Id "," Id ")"

  // -----------------------------------------------------------------------
  // v0.23.7 MACRO-013: Macro loading and expansion rules
  //
  // Macro semantics: macros are compile-time rewriting rules that substitute
  // their body at call sites before program evaluation.  In the K model we
  // represent this as a two-phase process:
  //   1. loadMacros  — populate the <macros> cell from MacroDecls
  //   2. macroExpand — substitute a zero- or one-argument macro call inline
  //
  // Hygiene (variable isolation) is an implementation-level property ensured
  // by the compiler's cloneAST / gensym mechanism before K evaluation begins;
  // the K rules below model the logical substitution only.
  //
  // Built-in macros (assert!, todo!, unreachable!, cfg!) are pre-resolved by
  // the type checker and do not appear in the <macros> cell.
  // -----------------------------------------------------------------------

  syntax KItem ::= "loadMacros" "(" MacroDecls ")"
  syntax KItem ::= "macroExpand" "(" Id "," MacroBody ")"
                 | "macroExpand1" "(" Id "," Id "," Exp "," MacroBody ")"

  // Base case: no more macros to load
  rule <k> loadMacros(.MacroDecls) => .K ... </k>

  // Zero-arg macro: register body block in <macros>
  rule <k> loadMacros(macro: M:Id = () B:Block ; REST:MacroDecls) => loadMacros(REST) ... </k>
       <macros> Ms => Ms [ M <- B ] </macros>

  // One-arg macro: register as a two-element tuple (param name, body block)
  rule <k> loadMacros(macro: M:Id = (P:Id) B:Block ; REST:MacroDecls) => loadMacros(REST) ... </k>
       <macros> Ms => Ms [ M <- MacroFn1(P, B) ] </macros>

  syntax MacroVal ::= "MacroFn1" "(" Id "," MacroBody ")"

  // macroExpand(M, Body) — inline the body of a zero-arg macro as a statement
  // The result block is evaluated in the current environment; since macros were
  // already expanded by the compiler, this rule fires only when the K model is
  // used in standalone verification mode.
  rule <k> macroExpand(M:Id, B:Block) => B ... </k>
       <macros> Ms </macros>
    requires M in_keys(Ms)

  // macroExpand1(M, Param, Arg, Body) — substitute Arg for Param in Body
  // Hygiene: in the compiler, cloneAST introduces fresh gensym names before
  // substitution; in this K model we document the logical substitution only.
  rule <k> macroExpand1(M:Id, _P:Id, _A:Exp, B:Block) => B ... </k>
       <macros> Ms </macros>
    requires M in_keys(Ms)

  rule <k> loadFuncs(.FuncDecls) => .K ... </k>

  rule <k> loadFuncs(func: F:Id = T:Type() { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Pass(T, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type() { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Fail(E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(P:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Pass(T, paramBorrow(P), paramType(P), paramName(P), E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(P:Param) { S:Stmt pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1StmtPass(T, paramBorrow(P), paramType(P), paramName(P), S, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type(P:Param) { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Fail(paramBorrow(P), paramType(P), paramName(P), E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(P1:Param, P2:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Pass(T, paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), E) ] </functions>

  rule <k> loadFuncs(func: F:Id = T:Type(P1:Param, P2:Param) { S:Stmt pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2StmtPass(T, paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), S, E) ] </functions>

  rule <k> loadFuncs(func: F:Id = _T:Type(P1:Param, P2:Param) { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Fail(paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), E) ] </functions>

  // async func: declarations — synchronous model: semantics identical to sync func:
  rule <k> loadFuncs(async func: F:Id = T:Type() { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Pass(T, E) ] </functions>

  rule <k> loadFuncs(async func: F:Id = T:Type(P:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Pass(T, paramBorrow(P), paramType(P), paramName(P), E) ] </functions>

  rule <k> loadFuncs(async func: F:Id = T:Type(P:Param) { S:Stmt pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1StmtPass(T, paramBorrow(P), paramType(P), paramName(P), S, E) ] </functions>

  rule <k> loadFuncs(async func: F:Id = T:Type(P1:Param, P2:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Pass(T, paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), E) ] </functions>

  rule <k> loadFuncs(async func: F:Id = T:Type(P1:Param, P2:Param) { S:Stmt pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2StmtPass(T, paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), S, E) ] </functions>

  // v0.24.7: COMPTIME-014 — comptime func: declarations
  //   Same runtime semantics as func: (Aria comptime is CTFE; in K we model
  //   it as ordinary call evaluation), but additionally registered in
  //   <comptime-env> so the model exposes the comptime callability set.
  rule <k> loadFuncs(comptime func: F:Id = T:Type() { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn0Pass(T, E) ] </functions>
       <comptime-env> CE => CE [ F <- 0 ] </comptime-env>

  rule <k> loadFuncs(comptime func: F:Id = T:Type(P:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn1Pass(T, paramBorrow(P), paramType(P), paramName(P), E) ] </functions>
       <comptime-env> CE => CE [ F <- 1 ] </comptime-env>

  rule <k> loadFuncs(comptime func: F:Id = T:Type(P1:Param, P2:Param) { pass E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Pass(T, paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), E) ] </functions>
       <comptime-env> CE => CE [ F <- 2 ] </comptime-env>

  // v0.24.7: COMPTIME-014 — comptime expression / block passthrough
  //   Aria's comptime(E) folds E to a Val at compile time; in K's small-step
  //   model the value-yielding rule is the same as runtime evaluation.
  rule <k> comptime ( V:Val ) => V ... </k>
  rule <k> comptime B:Block => B ... </k>

  // await: synchronous model — extracts success value from Result, or passes plain Val through
  rule <k> await Result(_:Val, V:Val, _:Bool) => V ... </k>
  rule <k> await V:Val => V ... </k> [owise]

     rule <k> F:Id() => E ~> finishCallPassFrame(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
       <functions> ... F |-> Fn0Pass(T, E) ... </functions>
       <env> Env => .Map </env>
       <types> Types => .Map </types>
       <store> Store => .Map </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => .Map </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

     rule <k> F:Id() => E ~> finishCallFailFrame(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
       <functions> ... F |-> Fn0Fail(E) ... </functions>
       <env> Env => .Map </env>
       <types> Types => .Map </types>
       <store> Store => .Map </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => .Map </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

           rule <k> F:Id(X:Id) => E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), envLoc(Env, X) |-> true) ... </k>
                <functions> ... F |-> Fn1Pass(T, MutBorrow, PT, PX, E) ... </functions>
                <env> Env => PX |-> envLoc(Env, X) </env>
                <types> Types => PX |-> PT </types>
                <store> Store </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX, MutBorrow) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires X in_keys(Env) andBool notBool(X in PinnedHosts) [priority(25)]

           rule <k> F:Id(X:Id) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), envLoc(Env, X) |-> true) ... </k>
                                        <functions> ... F |-> Fn1StmtPass(T, MutBorrow, PT, PX, S, E) ... </functions>
                                        <env> Env => PX |-> envLoc(Env, X) </env>
                                        <types> Types => PX |-> PT </types>
                                        <store> Store </store>
                                        <limits> Limits => .Map </limits>
                                        <mem-modes> Modes => .Map </mem-modes>
                                        <stack-live> Stack => .Set </stack-live>
                                        <gc-live> GC => .Set </gc-live>
                                        <wild-live> Wild => .Set </wild-live>
                                        <borrow-vars> BorrowVars => paramBorrowMap(PX, MutBorrow) </borrow-vars>
                                        <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                                        <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                                        <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                                        <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                                        <fixed-vars> Fixed => .Set </fixed-vars>
                                        <next-loc> L => L +Int 1 </next-loc>
                         requires X in_keys(Env) andBool notBool(X in PinnedHosts) [priority(25)]

           rule <k> F:Id(X:Id) => E ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), envLoc(Env, X) |-> true) ... </k>
                <functions> ... F |-> Fn1Fail(MutBorrow, PT, PX, E) ... </functions>
                                        <env> Env => PX |-> envLoc(Env, X) </env>
                <types> Types => PX |-> PT </types>
                                        <store> Store </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX, MutBorrow) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires X in_keys(Env) andBool notBool(X in PinnedHosts) [priority(25)]

     rule <k> F:Id(A:Val) => E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
       <functions> ... F |-> Fn1Pass(T, B, PT, PX, E) ... </functions>
       <env> Env => PX |-> callParamLoc(L, B, A) </env>
       <types> Types => PX |-> PT </types>
       <store> Store => callParamStore(Store, PT, B, L, A) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX, B) </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>
    requires acceptsBorrowArg(B, A)

     rule <k> F:Id(A:Val) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
                <functions> ... F |-> Fn1StmtPass(T, B, PT, PX, S, E) ... </functions>
                <env> Env => PX |-> callParamLoc(L, B, A) </env>
                <types> Types => PX |-> PT </types>
                <store> Store => callParamStore(Store, PT, B, L, A) </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX, B) </borrow-vars>
                <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires acceptsBorrowArg(B, A)

     rule <k> F:Id(A:Val) => E ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
       <functions> ... F |-> Fn1Fail(B, PT, PX, E) ... </functions>
                <env> Env => PX |-> callParamLoc(L, B, A) </env>
       <types> Types => PX |-> PT </types>
                <store> Store => callParamStore(Store, PT, B, L, A) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX, B) </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>
    requires acceptsBorrowArg(B, A)

     rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) callWritebacks(B2, A2)) ... </k>
       <functions> ... F |-> Fn2Pass(T, B1, PT1, PX1, B2, PT2, PX2, E) ... </functions>
       <env> Env => (PX1 |-> callParamLoc(L, B1, A1) PX2 |-> callParamLoc(L +Int 1, B2, A2)) </env>
       <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
       <store> Store => callParamStore(callParamStore(Store, PT1, B1, L, A1), PT2, B2, L +Int 1, A2) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX1, B1) paramBorrowMap(PX2, B2) </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
     <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>
    requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

           rule <k> F:Id(X:Id, A2:Exp) => A2 ~> finishFn2MutFirstArg(F, X) ... </k>
                <functions> ... F |-> Fn2StmtPass(_:Type, MutBorrow, _:Type, _:Id, CopyBorrow, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                <env> Env </env>
                <pinned-hosts> PinnedHosts </pinned-hosts>
          requires X in_keys(Env) andBool notBool(X in PinnedHosts) [priority(30)]

           rule <k> F:Id(X:Id, A2:Exp) => A2 ~> finishFn2MutFirstArg(F, X) ... </k>
                <functions> ... F |-> Fn2StmtPass(_:Type, MutBorrow, _:Type, _:Id, ImmBorrow, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                <env> Env </env>
                <pinned-hosts> PinnedHosts </pinned-hosts>
          requires X in_keys(Env) andBool notBool(X in PinnedHosts) [priority(30)]

     rule <k> V:Val ~> finishFn2MutFirstArg(F:Id, X:Id) => F(X, V) ... </k>

           rule <k> F:Id(X:Id, A2:Val) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), (envLoc(Env, X) |-> true) callWritebacks(B2, A2)) ... </k>
                <functions> ... F |-> Fn2StmtPass(T, MutBorrow, PT1, PX1, B2, PT2, PX2, S, E) ... </functions>
                <env> Env => (PX1 |-> envLoc(Env, X) PX2 |-> callParamLoc(L, B2, A2)) </env>
                <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
                <store> Store => callParamStore(Store, PT2, B2, L, A2) </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX1, MutBorrow) paramBorrowMap(PX2, B2) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
           <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires X in_keys(Env) andBool notBool(X in PinnedHosts) andBool acceptsBorrowArg(B2, A2) [priority(25)]

           rule <k> F:Id(A1:Val, Y:Id) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) (envLoc(Env, Y) |-> true)) ... </k>
                <functions> ... F |-> Fn2StmtPass(T, B1, PT1, PX1, MutBorrow, PT2, PX2, S, E) ... </functions>
                <env> Env => (PX1 |-> callParamLoc(L, B1, A1) PX2 |-> envLoc(Env, Y)) </env>
                <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
                <store> Store => callParamStore(Store, PT1, B1, L, A1) </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX1, B1) paramBorrowMap(PX2, MutBorrow) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
           <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires Y in_keys(Env) andBool notBool(Y in PinnedHosts) andBool acceptsBorrowArg(B1, A1) [priority(25)]

           rule <k> F:Id(X:Id, Y:Id) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), (envLoc(Env, X) |-> true) (envLoc(Env, Y) |-> true)) ... </k>
                <functions> ... F |-> Fn2StmtPass(T, MutBorrow, PT1, PX1, MutBorrow, PT2, PX2, S, E) ... </functions>
                <env> Env => (PX1 |-> envLoc(Env, X) PX2 |-> envLoc(Env, Y)) </env>
                <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
                <store> Store </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX1, MutBorrow) paramBorrowMap(PX2, MutBorrow) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
           <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 2 </next-loc>
          requires X in_keys(Env) andBool Y in_keys(Env) andBool notBool(X in PinnedHosts) andBool notBool(Y in PinnedHosts) andBool envLoc(Env, X) =/=Int envLoc(Env, Y) [priority(25)]

           rule <k> F:Id(A1:Val, A2:Val) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) callWritebacks(B2, A2)) ... </k>
                <functions> ... F |-> Fn2StmtPass(T, B1, PT1, PX1, B2, PT2, PX2, S, E) ... </functions>
                <env> Env => (PX1 |-> callParamLoc(L, B1, A1) PX2 |-> callParamLoc(L +Int 1, B2, A2)) </env>
                <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
                <store> Store => callParamStore(callParamStore(Store, PT1, B1, L, A1), PT2, B2, L +Int 1, A2) </store>
                <limits> Limits => .Map </limits>
                <mem-modes> Modes => .Map </mem-modes>
                <stack-live> Stack => .Set </stack-live>
                <gc-live> GC => .Set </gc-live>
                <wild-live> Wild => .Set </wild-live>
                <borrow-vars> BorrowVars => paramBorrowMap(PX1, B1) paramBorrowMap(PX2, B2) </borrow-vars>
           <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
           <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 2 </next-loc>
          requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

     rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) callWritebacks(B2, A2)) ... </k>
       <functions> ... F |-> Fn2Fail(B1, PT1, PX1, B2, PT2, PX2, E) ... </functions>
       <env> Env => (PX1 |-> callParamLoc(L, B1, A1) PX2 |-> callParamLoc(L +Int 1, B2, A2)) </env>
       <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
       <store> Store => callParamStore(callParamStore(Store, PT1, B1, L, A1), PT2, B2, L +Int 1, A2) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX1, B1) paramBorrowMap(PX2, B2) </borrow-vars>
     <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>
    requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

     rule <k> F:Id(X:Id) ~> _ => FS </k>
                <functions> ... F |-> Fn1Pass(_:Type, _:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> F:Id(X:Id) ~> _ => FS </k>
                <functions> ... F |-> Fn1StmtPass(_:Type, _:BorrowInfo, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> F:Id(X:Id) ~> _ => FS </k>
                <functions> ... F |-> Fn1Fail(_:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> F:Id(X:Id, _:Exp) ~> _ => FS </k>
                <functions> ... F |-> Fn2Pass(_:Type, _:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> F:Id(_:Exp, X:Id) ~> _ => FS </k>
                <functions> ... F |-> Fn2Pass(_:Type, _:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

          rule <k> F:Id(X:Id, _:Exp) ~> _ => FS </k>
                   <functions> ... F |-> Fn2StmtPass(_:Type, _:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                   <pinned-hosts> Pinned </pinned-hosts>
                   <failsafe> FS </failsafe>
                   <failsafe-triggered> _ => true </failsafe-triggered>
              requires X in Pinned [priority(25)]

          rule <k> F:Id(_:Exp, X:Id) ~> _ => FS </k>
                   <functions> ... F |-> Fn2StmtPass(_:Type, _:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                   <pinned-hosts> Pinned </pinned-hosts>
                   <failsafe> FS </failsafe>
                   <failsafe-triggered> _ => true </failsafe-triggered>
              requires X in Pinned [priority(25)]

     rule <k> F:Id(X:Id, _:Exp) ~> _ => FS </k>
                <functions> ... F |-> Fn2Fail(_:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> F:Id(_:Exp, X:Id) ~> _ => FS </k>
                <functions> ... F |-> Fn2Fail(_:BorrowInfo, _:Type, _:Id, _:BorrowInfo, _:Type, _:Id, _:Exp) ... </functions>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

  rule <k> F:Id(A:Val) ~> _ => FS </k>
       <functions> ... F |-> Fn1Pass(_:Type, B, _:Type, _:Id, _:Exp) ... </functions>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B, A))

     rule <k> F:Id(A:Val) ~> _ => FS </k>
                <functions> ... F |-> Fn1StmtPass(_:Type, B, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires notBool(acceptsBorrowArg(B, A))

  rule <k> F:Id(A:Val) ~> _ => FS </k>
       <functions> ... F |-> Fn1Fail(B, _:Type, _:Id, _:Exp) ... </functions>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B, A))

  rule <k> F:Id(A1:Val, A2:Val) ~> _ => FS </k>
       <functions> ... F |-> Fn2Pass(_:Type, B1, _:Type, _:Id, B2, _:Type, _:Id, _:Exp) ... </functions>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2))

     rule <k> F:Id(A1:Val, A2:Val) ~> _ => FS </k>
                <functions> ... F |-> Fn2StmtPass(_:Type, B1, _:Type, _:Id, B2, _:Type, _:Id, _:Stmt, _:Exp) ... </functions>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires notBool(acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2))

  rule <k> F:Id(A1:Val, A2:Val) ~> _ => FS </k>
       <functions> ... F |-> Fn2Fail(B1, _:Type, _:Id, B2, _:Type, _:Id, _:Exp) ... </functions>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2))

  // -------------------------------------------------------------------------
  // Lambda variable declarations and invocation (v0.19.4)
  // Function pointer variables: (RT)(P):x = RT(P) { pass E; };
  // These store a Lambda1/Lambda2 Val in a local variable; calling x(arg)
  // dispatches through invokeLambda, which uses the same call-frame mechanism
  // as named helper functions.
  // -------------------------------------------------------------------------

  syntax KItem ::= "invokeLambda"  "(" Val "," Val ")"
                 | "invokeLambda2" "(" Val "," Val "," Val ")"

  // 1-param lambda declaration
  rule <k> ( RT:Type ) ( _:Type ) : X:Id = _:Type ( P:Param ) { pass E:Exp ; } ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <store> Store => Store [ L <- Lambda1(paramBorrow(P), paramType(P), paramName(P), RT, E) ] </store>
       <next-loc> L => L +Int 1 </next-loc>

  // 2-param lambda declaration
  rule <k> ( RT:Type ) ( _:Type , _:Type ) : X:Id = _:Type ( P1:Param , P2:Param ) { pass E:Exp ; } ; => .K ... </k>
       <env> Env => Env [ X <- L ] </env>
       <store> Store => Store [ L <- Lambda2(paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), RT, E) ] </store>
       <next-loc> L => L +Int 1 </next-loc>

  // Dispatch: call through a lambda variable (1-arg)
  // Use env+store cell pattern to bind the lambda value — avoids M[K] in function args
  rule <k> X:Id ( A:Val ) => invokeLambda(LV, A) ... </k>
       <env> ... X |-> XL:Int ... </env>
       <store> ... XL |-> LV:Val ... </store>
    requires isLambdaVal(LV) [priority(100)]

  // Dispatch: call through a lambda variable (2-arg)
  rule <k> X:Id ( A1:Val , A2:Val ) => invokeLambda2(LV, A1, A2) ... </k>
       <env> ... X |-> XL:Int ... </env>
       <store> ... XL |-> LV:Val ... </store>
    requires isLambdaVal(LV) [priority(100)]

  // Invoke 1-param lambda — set up call frame and evaluate body
  rule <k> invokeLambda(Lambda1(B:BorrowInfo, PT:Type, PX:Id, RT:Type, E:Exp), A:Val) =>
           E ~> finishCallPassFrame(RT, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
       <env> Env => PX |-> callParamLoc(Next, B, A) </env>
       <types> Types => PX |-> PT </types>
       <store> Store => callParamStore(Store, PT, B, Next, A) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX, B) </borrow-vars>
       <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next => Next +Int 1 </next-loc>
    requires acceptsBorrowArg(B, A)

  // Invoke 1-param lambda failsafe (borrow mismatch)
  rule <k> invokeLambda(Lambda1(B:BorrowInfo, _:Type, _:Id, _:Type, _:Exp), A:Val) ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B, A))

  // Invoke 2-param lambda — set up call frame and evaluate body
  rule <k> invokeLambda2(Lambda2(B1:BorrowInfo, PT1:Type, PX1:Id, B2:BorrowInfo, PT2:Type, PX2:Id, RT:Type, E:Exp), A1:Val, A2:Val) =>
           E ~> finishCallPassFrame(RT, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
       <env> Env => (PX1 |-> callParamLoc(Next, B1, A1) PX2 |-> callParamLoc(Next +Int 1, B2, A2)) </env>
       <types> Types => (PX1 |-> PT1 PX2 |-> PT2) </types>
       <store> Store => callParamStore(callParamStore(Store, PT1, B1, Next, A1), PT2, B2, Next +Int 1, A2) </store>
       <limits> Limits => .Map </limits>
       <mem-modes> Modes => .Map </mem-modes>
       <stack-live> Stack => .Set </stack-live>
       <gc-live> GC => .Set </gc-live>
       <wild-live> Wild => .Set </wild-live>
       <borrow-vars> BorrowVars => paramBorrowMap(PX1, B1) paramBorrowMap(PX2, B2) </borrow-vars>
       <borrow-aliases> BorrowAliases => .Map </borrow-aliases>
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next => Next +Int 2 </next-loc>
    requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

  // Invoke 2-param lambda failsafe (borrow mismatch)
  rule <k> invokeLambda2(Lambda2(B1:BorrowInfo, _:Type, _:Id, B2:BorrowInfo, _:Type, _:Id, _:Type, _:Exp), A1:Val, A2:Val) ~> _ => FS </k>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2))

  rule <k> V:Val ~> finishCallPass(T) => Result(NIL, normalizeValue(T, V), false) ... </k>
  rule <k> V:Val ~> finishCallFail => Result(V, NIL, true) ... </k>

     rule <k> V:Val ~> finishCallPassFrame(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) => Result(NIL, normalizeValue(T, V), false) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <mem-modes> _ => Modes </mem-modes>
       <stack-live> _ => Stack </stack-live>
       <gc-live> _ => GC </gc-live>
       <wild-live> _ => Wild </wild-live>
       <borrow-vars> _ => BorrowVars </borrow-vars>
     <borrow-aliases> _ => BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
     <pinned-hosts> _ => PinnedHosts </pinned-hosts>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

           rule <k> V:Val ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next), W:Map) => Result(NIL, normalizeValue(T, V), false) ... </k>
                <env> _ => Env </env>
                <types> _ => Types </types>
                <store> Current => writebackStore(Store, Current, W) </store>
                <limits> _ => Limits </limits>
                <mem-modes> _ => Modes </mem-modes>
                <stack-live> _ => Stack </stack-live>
                <gc-live> _ => GC </gc-live>
                <wild-live> _ => Wild </wild-live>
                <borrow-vars> _ => BorrowVars </borrow-vars>
                <borrow-aliases> _ => BorrowAliases </borrow-aliases>
                <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
                <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
           <pinned-hosts> _ => PinnedHosts </pinned-hosts>
                <fixed-vars> _ => Fixed </fixed-vars>
                <next-loc> _ => Next </next-loc>

     rule <k> V:Val ~> finishCallFailFrame(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) => Result(V, NIL, true) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <mem-modes> _ => Modes </mem-modes>
       <stack-live> _ => Stack </stack-live>
       <gc-live> _ => GC </gc-live>
       <wild-live> _ => Wild </wild-live>
       <borrow-vars> _ => BorrowVars </borrow-vars>
     <borrow-aliases> _ => BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
     <pinned-hosts> _ => PinnedHosts </pinned-hosts>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

     rule <k> V:Val ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts, Fixed, Next), W:Map) => Result(V, NIL, true) ... </k>
                <env> _ => Env </env>
                <types> _ => Types </types>
                <store> Current => writebackStore(Store, Current, W) </store>
                <limits> _ => Limits </limits>
                <mem-modes> _ => Modes </mem-modes>
                <stack-live> _ => Stack </stack-live>
                <gc-live> _ => GC </gc-live>
                <wild-live> _ => Wild </wild-live>
                <borrow-vars> _ => BorrowVars </borrow-vars>
                <borrow-aliases> _ => BorrowAliases </borrow-aliases>
                <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
                <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
           <pinned-hosts> _ => PinnedHosts </pinned-hosts>
                <fixed-vars> _ => Fixed </fixed-vars>
                <next-loc> _ => Next </next-loc>

  // -------------------------------------------------------------------------
  // Statement sequencing and blocks
  // -------------------------------------------------------------------------

     syntax KItem ::= "DeferScope" "(" List "," Map "," Map "," Set "," Set "," Set ")"
                 | "Deferred" "(" Stmts ")"
                 | "enterBlock"
                 | "leaveBlock"
                 | "popBlock"
                 | "runDefers" "(" List ")"

  rule <k> { SS:Stmts } => enterBlock ~> SS ~> leaveBlock ... </k>

  rule <k> enterBlock => .K ... </k>
       <defer-stack> DS => ListItem(DeferScope(.List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) DS </defer-stack>
       <borrow-vars> BorrowVars </borrow-vars>
       <borrow-aliases> BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts </pinned-hosts>

  rule <k> defer { SS:Stmts } => .K ... </k>
       <defer-stack> ListItem(DeferScope(OLD:List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(ListItem(Deferred(SS)) OLD, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> defer _:Block ~> _ => FS </k>
       <defer-stack> .List </defer-stack>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> leaveBlock => runDefers(DS) ~> popBlock ... </k>
       <defer-stack> ListItem(DeferScope(DS:List, _:Map, _:Map, _:Set, _:Set, _:Set)) _:List </defer-stack>

  rule <k> runDefers(.List) => .K ... </k>
  rule <k> runDefers(ListItem(Deferred(SS:Stmts)) REST:List) => SS ~> runDefers(REST) ... </k>

  rule <k> popBlock => .K ... </k>
       <defer-stack> ListItem(DeferScope(_:List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <borrow-aliases> _ => BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
       <pinned-hosts> _ => PinnedHosts </pinned-hosts>

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
        <borrow-vars> BorrowVars </borrow-vars>
        <borrow-aliases> BorrowAliases </borrow-aliases>
        <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
        <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
        <pinned-hosts> PinnedHosts </pinned-hosts>
     requires notBool(X in Fixed) andBool notBool(X in_keys(Limits))
      andBool notBool(isImmutableBorrowVar(X, BorrowVars))
       andBool notBool(X in_keys(BorrowAliases))
     andBool notBool(hasBorrowOnHost(X, ImmHosts)) andBool notBool(hasBorrowOnHost(X, MutHosts))
      andBool notBool(X in PinnedHosts)

  rule <k> X:Id = V:Val ; => .K ... </k>
        <env> ... X |-> AL H |-> HL ... </env>
        <types> ... X |-> T ... </types>
        <store> ... AL |-> (_:Val => normalizeValue(T, V)) HL |-> (StructVal(S, Fields) => StructVal(S, Fields [ F <- normalizeValue(T, V) ])) ... </store>
        <borrow-vars> ... X |-> MutBorrow ... </borrow-vars>
        <borrow-aliases> ... X |-> FieldAlias(H, F) ... </borrow-aliases>
        <fixed-vars> Fixed </fixed-vars>
        <pinned-hosts> PinnedHosts </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in PinnedHosts)

  rule <k> X:Id = V:Val ; => .K ... </k>
        <env> ... X |-> AL H |-> HL ... </env>
        <store> ... AL |-> (_:Val => normalizeLike(OLD, V)) HL |-> (StructVal(S, (F1 |-> StructVal(NS, (F2 |-> OLD:Val NFields:Map)) Fields:Map)) => StructVal(S, (F1 |-> StructVal(NS, (F2 |-> normalizeLike(OLD, V)) NFields)) Fields)) ... </store>
        <borrow-vars> ... X |-> MutBorrow ... </borrow-vars>
        <borrow-aliases> ... X |-> NestedFieldAlias(H, F1, F2) ... </borrow-aliases>
        <fixed-vars> Fixed </fixed-vars>
        <pinned-hosts> PinnedHosts </pinned-hosts>
      requires notBool(X in Fixed) andBool notBool(X in PinnedHosts)

     rule <k> X:Id = V:Val ; => .K ... </k>
                    <env> ... X |-> AL H |-> HL ... </env>
                    <types> ... X |-> T ... </types>
                    <store> ... AL |-> (_:Val => normalizeValue(T, V)) HL |-> (ArrayVal(Items) => ArrayVal(Items [ I <- normalizeValue(T, V) ])) ... </store>
                    <borrow-vars> ... X |-> MutBorrow ... </borrow-vars>
                    <borrow-aliases> ... X |-> IndexAlias(H, I) ... </borrow-aliases>
                    <fixed-vars> Fixed </fixed-vars>
                    <pinned-hosts> PinnedHosts </pinned-hosts>
               requires notBool(X in Fixed) andBool notBool(X in PinnedHosts) andBool I in_keys(Items)

     // DynIndexAlias writeback: same as IndexAlias but tagged as DynIndexAlias
     rule <k> X:Id = V:Val ; => .K ... </k>
                    <env> ... X |-> AL H |-> HL ... </env>
                    <types> ... X |-> T ... </types>
                    <store> ... AL |-> (_:Val => normalizeValue(T, V)) HL |-> (ArrayVal(Items) => ArrayVal(Items [ I <- normalizeValue(T, V) ])) ... </store>
                    <borrow-vars> ... X |-> MutBorrow ... </borrow-vars>
                    <borrow-aliases> ... X |-> DynIndexAlias(H, I) ... </borrow-aliases>
                    <fixed-vars> Fixed </fixed-vars>
                    <pinned-hosts> PinnedHosts </pinned-hosts>
               requires notBool(X in Fixed) andBool notBool(X in PinnedHosts) andBool I in_keys(Items)

  rule <k> X:Id = _:Val ; ~> _ => FS </k>
        <borrow-vars> BorrowVars </borrow-vars>
        <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
        <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
        <pinned-hosts> PinnedHosts </pinned-hosts>
        <failsafe> FS </failsafe>
        <failsafe-triggered> _ => true </failsafe-triggered>
       requires isImmutableBorrowVar(X, BorrowVars) orBool hasBorrowOnHost(X, ImmHosts) orBool hasBorrowOnHost(X, MutHosts) orBool X in PinnedHosts

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
               | normalizeLike(Val, Val) [function]
               | normalizeTBB8(Int) [function]
               | normalizeTBB16(Int) [function]
               | normalizeTBB32(Int) [function]
               | normalizeTBB64(Int) [function]

  rule normalizeValue(int32, I:Int) => I32(wrapI32(I))
  rule normalizeValue(int64, I:Int) => I64(wrapI64(I))
  rule normalizeValue(tbb8, I:Int) => normalizeTBB8(I)
  rule normalizeValue(tbb16, I:Int) => normalizeTBB16(I)
  rule normalizeValue(tbb32, I:Int) => normalizeTBB32(I)
  rule normalizeValue(tbb64, I:Int) => normalizeTBB64(I)
  rule normalizeValue(int32, I32(I)) => I32(wrapI32(I))
  rule normalizeValue(int64, I64(I)) => I64(wrapI64(I))
  rule normalizeValue(tbb8, TBB8(I)) => normalizeTBB8(I)
  rule normalizeValue(tbb16, TBB16(I)) => normalizeTBB16(I)
  rule normalizeValue(tbb32, TBB32(I)) => normalizeTBB32(I)
  rule normalizeValue(tbb64, TBB64(I)) => normalizeTBB64(I)
     rule normalizeValue(_:Type [ _:Int ], V:Val) => V
  rule normalizeValue(tbb8, ERR) => ERR
  rule normalizeValue(tbb16, ERR) => ERR
  rule normalizeValue(tbb32, ERR) => ERR
  rule normalizeValue(tbb64, ERR) => ERR
  rule normalizeValue(string, S:String) => S
  rule normalizeValue(_:Type, V:Val) => V [owise]

     rule normalizeLike(I32(_:Int), V:Val) => normalizeValue(int32, V)
     rule normalizeLike(I64(_:Int), V:Val) => normalizeValue(int64, V)
     rule normalizeLike(TBB8(_:Int), V:Val) => normalizeValue(tbb8, V)
     rule normalizeLike(TBB16(_:Int), V:Val) => normalizeValue(tbb16, V)
     rule normalizeLike(TBB32(_:Int), V:Val) => normalizeValue(tbb32, V)
     rule normalizeLike(TBB64(_:Int), V:Val) => normalizeValue(tbb64, V)
     rule normalizeLike(ERR, V:Val) => normalizeValue(tbb32, V)
     rule normalizeLike(_:Val, V:Val) => V [owise]

  rule normalizeTBB8(I) => ERR requires I <=Int -128 orBool I >Int 127
  rule normalizeTBB8(I) => TBB8(I) requires I >Int -128 andBool I <=Int 127
  rule normalizeTBB16(I) => ERR requires I <=Int -32768 orBool I >Int 32767
  rule normalizeTBB16(I) => TBB16(I) requires I >Int -32768 andBool I <=Int 32767
  rule normalizeTBB32(I) => ERR requires I <=Int -2147483648 orBool I >Int 2147483647
  rule normalizeTBB32(I) => TBB32(I) requires I >Int -2147483648 andBool I <=Int 2147483647
  rule normalizeTBB64(I) => ERR requires I <=Int -9223372036854775808 orBool I >Int 9223372036854775807
  rule normalizeTBB64(I) => TBB64(I) requires I >Int -9223372036854775808 andBool I <=Int 9223372036854775807

  syntax Int ::= numericValue(Val) [function]
  syntax Bool ::= isNumericValue(Val) [function]

  rule numericValue(I:Int) => I
  rule numericValue(I32(I)) => I
  rule numericValue(I64(I)) => I
  rule numericValue(TBB8(I)) => I
  rule numericValue(TBB16(I)) => I
  rule numericValue(TBB32(I)) => I
  rule numericValue(TBB64(I)) => I

  rule isNumericValue(_:Int) => true
  rule isNumericValue(I32(_:Int)) => true
  rule isNumericValue(I64(_:Int)) => true
  rule isNumericValue(TBB8(_:Int)) => true
  rule isNumericValue(TBB16(_:Int)) => true
  rule isNumericValue(TBB32(_:Int)) => true
  rule isNumericValue(TBB64(_:Int)) => true
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

  rule <k> TBB8(I1) + TBB8(I2) => normalizeTBB8(I1 +Int I2) ... </k>
  rule <k> TBB8(I1) + I2:Int => normalizeTBB8(I1 +Int I2) ... </k>
  rule <k> I1:Int + TBB8(I2) => normalizeTBB8(I1 +Int I2) ... </k>
  rule <k> TBB8(I1) - TBB8(I2) => normalizeTBB8(I1 -Int I2) ... </k>
  rule <k> TBB8(I1) - I2:Int => normalizeTBB8(I1 -Int I2) ... </k>
  rule <k> I1:Int - TBB8(I2) => normalizeTBB8(I1 -Int I2) ... </k>
  rule <k> TBB8(I1) * TBB8(I2) => normalizeTBB8(I1 *Int I2) ... </k>
  rule <k> TBB8(I1) * I2:Int => normalizeTBB8(I1 *Int I2) ... </k>
  rule <k> I1:Int * TBB8(I2) => normalizeTBB8(I1 *Int I2) ... </k>
  rule <k> TBB8(I1) / TBB8(I2) => normalizeTBB8(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB8(I1) / I2:Int => normalizeTBB8(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB8(_) / TBB8(0) => ERR ... </k>
  rule <k> TBB8(_) / 0 => ERR ... </k>

  rule <k> TBB16(I1) + TBB16(I2) => normalizeTBB16(I1 +Int I2) ... </k>
  rule <k> TBB16(I1) + I2:Int => normalizeTBB16(I1 +Int I2) ... </k>
  rule <k> I1:Int + TBB16(I2) => normalizeTBB16(I1 +Int I2) ... </k>
  rule <k> TBB16(I1) - TBB16(I2) => normalizeTBB16(I1 -Int I2) ... </k>
  rule <k> TBB16(I1) - I2:Int => normalizeTBB16(I1 -Int I2) ... </k>
  rule <k> I1:Int - TBB16(I2) => normalizeTBB16(I1 -Int I2) ... </k>
  rule <k> TBB16(I1) * TBB16(I2) => normalizeTBB16(I1 *Int I2) ... </k>
  rule <k> TBB16(I1) * I2:Int => normalizeTBB16(I1 *Int I2) ... </k>
  rule <k> I1:Int * TBB16(I2) => normalizeTBB16(I1 *Int I2) ... </k>
  rule <k> TBB16(I1) / TBB16(I2) => normalizeTBB16(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB16(I1) / I2:Int => normalizeTBB16(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB16(_) / TBB16(0) => ERR ... </k>
  rule <k> TBB16(_) / 0 => ERR ... </k>

  rule <k> TBB64(I1) + TBB64(I2) => normalizeTBB64(I1 +Int I2) ... </k>
  rule <k> TBB64(I1) + I2:Int => normalizeTBB64(I1 +Int I2) ... </k>
  rule <k> I1:Int + TBB64(I2) => normalizeTBB64(I1 +Int I2) ... </k>
  rule <k> TBB64(I1) - TBB64(I2) => normalizeTBB64(I1 -Int I2) ... </k>
  rule <k> TBB64(I1) - I2:Int => normalizeTBB64(I1 -Int I2) ... </k>
  rule <k> I1:Int - TBB64(I2) => normalizeTBB64(I1 -Int I2) ... </k>
  rule <k> TBB64(I1) * TBB64(I2) => normalizeTBB64(I1 *Int I2) ... </k>
  rule <k> TBB64(I1) * I2:Int => normalizeTBB64(I1 *Int I2) ... </k>
  rule <k> I1:Int * TBB64(I2) => normalizeTBB64(I1 *Int I2) ... </k>
  rule <k> TBB64(I1) / TBB64(I2) => normalizeTBB64(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB64(I1) / I2:Int => normalizeTBB64(I1 /Int I2) ... </k> requires I2 =/=Int 0
  rule <k> TBB64(_) / TBB64(0) => ERR ... </k>
  rule <k> TBB64(_) / 0 => ERR ... </k>

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

     // 2-field struct pattern match
     rule <k> selectPick(StructVal(S:Id, ((F1:Id |-> V1:Val) (F2:Id |-> V2:Val) _:Map)), (S2:Id { F1 , F2 }) { SS:Stmts } , _:PickCases, ALL:PickCases) =>
              enterBlock ~> int32:F1 = V1; int32:F2 = V2; SS ~> leaveBlock ~> endPick(ALL) ... </k>
       requires S ==K S2

     // 2-field struct pattern no-match (wrong type name)
     rule <k> selectPick(StructVal(S:Id, Fields:Map), (S2:Id { _:Id , _:Id }) _:Block , REST:PickCases, ALL:PickCases) =>
              selectPick(StructVal(S, Fields), REST, ALL) ... </k>
       requires S =/=K S2

     // non-struct value — skip 2-field struct pattern case
     rule <k> selectPick(V:Val, (_:Id { _:Id , _:Id }) _:Block , REST:PickCases, ALL:PickCases) =>
              selectPick(V, REST, ALL) ... </k>
       requires notBool(isStructVal(V))

     // 3-field struct pattern match
     rule <k> selectPick(StructVal(S:Id, ((F1:Id |-> V1:Val) (F2:Id |-> V2:Val) (F3:Id |-> V3:Val) _:Map)), (S2:Id { F1 , F2 , F3 }) { SS:Stmts } , _:PickCases, ALL:PickCases) =>
              enterBlock ~> int32:F1 = V1; int32:F2 = V2; int32:F3 = V3; SS ~> leaveBlock ~> endPick(ALL) ... </k>
       requires S ==K S2

     // 3-field struct pattern no-match (wrong type name)
     rule <k> selectPick(StructVal(S:Id, Fields:Map), (S2:Id { _:Id , _:Id , _:Id }) _:Block , REST:PickCases, ALL:PickCases) =>
              selectPick(StructVal(S, Fields), REST, ALL) ... </k>
       requires S =/=K S2

     // non-struct value — skip 3-field struct pattern case
     rule <k> selectPick(V:Val, (_:Id { _:Id , _:Id , _:Id }) _:Block , REST:PickCases, ALL:PickCases) =>
              selectPick(V, REST, ALL) ... </k>
       requires notBool(isStructVal(V))

     rule <k> P:Val ~> finishPickCase(V:Val, B:Block, _:PickCases, ALL:PickCases) => B ~> endPick(ALL) ... </k>
          requires pickMatches(V, P)
     rule <k> P:Val ~> finishPickCase(V:Val, _:Block, REST:PickCases, ALL:PickCases) => selectPick(V, REST, ALL) ... </k>
          requires notBool(pickMatches(V, P))

     rule <k> fall L:Id ; ~> _:Stmts ~> leaveBlock ~> endPick(ALL:PickCases) => leaveBlock ~> selectPickLabel(L, ALL) ~> endPick(ALL) ... </k>
     rule <k> fall L:Id ; ~> _:Stmts ~> endPick(ALL:PickCases) => selectPickLabel(L, ALL) ~> endPick(ALL) ... </k>

     rule <k> selectPickLabel(_:Id, .PickCases) => .K ... </k>
     rule <k> selectPickLabel(L:Id, L : (_:PickPattern) B:Block , _:PickCases) => B ... </k>
     rule <k> selectPickLabel(L:Id, L2:Id : (_:PickPattern) _:Block , REST:PickCases) => selectPickLabel(L, REST) ... </k>
          requires L =/=K L2
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

     rule <k> exit X:Id ; ~> _ => FS </k>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

     rule <k> exit ( X:Id ) ; ~> _ => FS </k>
                <pinned-hosts> Pinned </pinned-hosts>
                <failsafe> FS </failsafe>
                <failsafe-triggered> _ => true </failsafe-triggered>
          requires X in Pinned [priority(25)]

  rule <k> exit V:Val ; => runDefers(ListItem(Deferred(SS)) DS) ~> exit V ; ... </k>
       <defer-stack> ListItem(DeferScope(ListItem(Deferred(SS:Stmts)) DS:List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(.List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> exit ( V:Val ) ; => runDefers(ListItem(Deferred(SS)) DS) ~> exit ( V ) ; ... </k>
       <defer-stack> ListItem(DeferScope(ListItem(Deferred(SS:Stmts)) DS:List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(.List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> exit V:Val ; => exit V ; ... </k>
       <defer-stack> ListItem(DeferScope(.List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <borrow-aliases> _ => BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
       <pinned-hosts> _ => PinnedHosts </pinned-hosts>

  rule <k> exit ( V:Val ) ; => exit ( V ) ; ... </k>
       <defer-stack> ListItem(DeferScope(.List, BorrowVars, BorrowAliases, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <borrow-aliases> _ => BorrowAliases </borrow-aliases>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
       <pinned-hosts> _ => PinnedHosts </pinned-hosts>

  rule <k> exit V:Val ; ~> _ => .K </k>
       <exit-code> _ => numericValue(V) </exit-code>
       <wild-live> .Set </wild-live>
       <defer-stack> .List </defer-stack>
    requires isNumericValue(V)

  rule <k> exit ( V:Val ) ; ~> _ => .K </k>
       <exit-code> _ => numericValue(V) </exit-code>
       <wild-live> .Set </wild-live>
       <defer-stack> .List </defer-stack>
    requires isNumericValue(V)

  rule <k> exit _:Val ; ~> _ => FS </k>
       <wild-live> SetItem(_:Int) _:Set => .Set </wild-live>
       <defer-stack> .List </defer-stack>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> exit ( _:Val ) ; ~> _ => FS </k>
       <wild-live> SetItem(_:Int) _:Set => .Set </wild-live>
       <defer-stack> .List </defer-stack>
       <failsafe> FS </failsafe>
     <failsafe-triggered> _ => true </failsafe-triggered>
endmodule