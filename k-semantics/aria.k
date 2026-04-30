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
//   - defer { ... } cleanup blocks with block-scoped LIFO execution
//   - @ address-of, <- dereference, and <-ptr = value store-through for local bindings
//   - # pin registration with dereference plus reassignment / mutation blocking
//   - $$i/$$m borrow qualifiers with minimal aliasing and call enforcement
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
     syntax BorrowMode ::= "$$i" | "$$m"
     syntax BorrowInfo ::= "CopyBorrow" | "ImmBorrow" | "MutBorrow"

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
               | "PTRLOC" "(" Int ")"
               | "PINPTRLOC" "(" Int ")"
               | "BorrowVal" "(" BorrowInfo "," Id "," Val ")"
               | "BorrowLoc" "(" BorrowInfo "," Id "," Int "," Val ")"

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
               | "$" Id
               | "!" "$" Id
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
                | BorrowMode Type ":" Id "=" Id ";"
                | Id "=" Exp ";" [strict(2)]
                | "<-" Exp "=" Exp ";" [strict(1, 2)]
                | Id "." Id "=" Exp ";" [strict(3)]
                | "exit" Exp ";" [strict]
                | "exit" "(" Exp ")" ";" [strict]
                | "if" "(" Exp ")" Block "else" Block [strict(1)]
                | "if" "(" Exp ")" Block [strict(1)]
                | "pick" "(" Exp ")" "{" PickCases "}" [strict(1)]
                | "fall" Id ";"
                | "loop" "(" Exp "," Exp "," Exp ")" Block [strict(1, 2, 3)]
                | "defer" Block
                | Block
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

     syntax Param         ::= Type ":" Id
                                                             | BorrowMode Type ":" Id
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
     imports LIST

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

     // Local pointer slice: @x captures the store location of a local binding,
     // <-ptr reads the current value at that location, and <-ptr = value writes
     // back through that captured location. #x returns a pinned pointer that can
     // read the host but cannot write through the pin reference.
  syntax Bool ::= isStorePointer(Val) [function]

  rule isStorePointer(PTRLOC(_:Int)) => true
     rule isStorePointer(PINPTRLOC(_:Int)) => true
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

  rule <k> <- PTRLOC(L:Int) = V:Val ; => .K ... </k>
       <store> ... L |-> (OLD:Val => normalizeLike(OLD, V)) ... </store>

  rule <k> <- PINPTRLOC(_:Int) = _:Val ; ~> _ => FS </k>
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
  // Borrow permission qualifiers: $$i / $$m
  // -------------------------------------------------------------------------

  syntax Type ::= paramType(Param) [function]
  syntax Id ::= paramName(Param) [function]
  syntax BorrowInfo ::= paramBorrow(Param) [function]
  syntax Val ::= argumentValue(Val) [function]
  syntax Bool ::= acceptsBorrowArg(BorrowInfo, Val) [function]
                | isImmutableBorrowVar(Id, Map) [function]
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

     rule argumentValue(BorrowLoc(_:BorrowInfo, _:Id, _:Int, V:Val)) => V
     rule argumentValue(BorrowVal(_:BorrowInfo, _:Id, V:Val)) => V
  rule argumentValue(V:Val) => V [owise]

     rule acceptsBorrowArg(MutBorrow, BorrowLoc(MutBorrow, _:Id, _:Int, _:Val)) => true
  rule acceptsBorrowArg(MutBorrow, BorrowVal(MutBorrow, _:Id, _:Val)) => true
  rule acceptsBorrowArg(MutBorrow, _) => false [owise]
  rule acceptsBorrowArg(ImmBorrow, _) => true
  rule acceptsBorrowArg(CopyBorrow, _) => true

     rule callParamLoc(_:Int, MutBorrow, BorrowLoc(MutBorrow, _:Id, BL:Int, _:Val)) => BL
     rule callParamLoc(L:Int, _:BorrowInfo, _:Val) => L [owise]

     rule callParamStore(Store:Map, PT:Type, B:BorrowInfo, L:Int, A:Val) => Store [ callParamLoc(L, B, A) <- normalizeValue(PT, argumentValue(A)) ]

     rule callWritebacks(MutBorrow, BorrowLoc(MutBorrow, _:Id, L:Int, _:Val)) => L |-> true
     rule callWritebacks(_:BorrowInfo, _:Val) => .Map [owise]

     rule writebackStore(Saved:Map, _:Map, .Map) => Saved
     rule writebackStore(Saved:Map, Current:Map, (L:Int |-> _:Bool REST:Map)) => writebackStore(Saved [ L <- Current [ L ] ], Current, REST)

  rule paramBorrowMap(_:Id, CopyBorrow) => .Map
  rule paramBorrowMap(X:Id, ImmBorrow) => X |-> ImmBorrow
  rule paramBorrowMap(X:Id, MutBorrow) => X |-> MutBorrow

  rule isImmutableBorrowVar(X:Id, (X |-> ImmBorrow _:Map)) => true
  rule isImmutableBorrowVar(X:Id, (X |-> MutBorrow _:Map)) => false
  rule isImmutableBorrowVar(_:Id, _:Map) => false [owise]

  rule <k> $ X:Id => BorrowLoc(MutBorrow, X, L, V) ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> V ... </store>

  rule <k> ! $ X:Id => BorrowLoc(ImmBorrow, X, L, V) ... </k>
       <env> ... X |-> L ... </env>
       <store> ... L |-> V ... </store>

  rule <k> $$i T:Type : X:Id = H:Id ; => .K ... </k>
       <env> Env => Env [ X <- Env [ H ] ] </env>
       <types> Types => Types [ X <- T ] </types>
       <borrow-vars> BorrowVars => BorrowVars [ X <- ImmBorrow ] </borrow-vars>
       <imm-borrow-hosts> Imm => Imm SetItem(H) </imm-borrow-hosts>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
    requires H in_keys(Env) andBool notBool(H in Mut)

  rule <k> $$m T:Type : X:Id = H:Id ; => .K ... </k>
       <env> Env => Env [ X <- Env [ H ] ] </env>
       <types> Types => Types [ X <- T ] </types>
       <borrow-vars> BorrowVars => BorrowVars [ X <- MutBorrow ] </borrow-vars>
       <mut-borrow-hosts> Mut => Mut SetItem(H) </mut-borrow-hosts>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
       <pinned-hosts> Pinned </pinned-hosts>
    requires H in_keys(Env) andBool notBool(H in Mut) andBool notBool(H in Imm) andBool notBool(H in Pinned)

  rule <k> $$i _:Type : _:Id = H:Id ; ~> _ => FS </k>
       <env> Env </env>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(H in_keys(Env)) orBool H in Mut

  rule <k> $$m _:Type : _:Id = H:Id ; ~> _ => FS </k>
       <env> Env </env>
       <mut-borrow-hosts> Mut </mut-borrow-hosts>
       <imm-borrow-hosts> Imm </imm-borrow-hosts>
           <pinned-hosts> Pinned </pinned-hosts>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
      requires notBool(H in_keys(Env)) orBool H in Mut orBool H in Imm orBool H in Pinned

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
                          | "Fn1Pass" "(" Type "," BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn1StmtPass" "(" Type "," BorrowInfo "," Type "," Id "," Stmt "," Exp ")"
                          | "Fn1Fail" "(" BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn2Pass" "(" Type "," BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Exp ")"
                          | "Fn2Fail" "(" BorrowInfo "," Type "," Id "," BorrowInfo "," Type "," Id "," Exp ")"

     syntax FrameState ::= "FrameState" "(" Map "," Map "," Map "," Map "," Map "," Set "," Set "," Set "," Map "," Set "," Set "," Set "," Set "," Int ")"

  syntax KItem ::= "loadFuncs" "(" FuncDecls ")"
                 | "finishCallPass" "(" Type ")"
                 | "finishCallFail"
                 | "finishCallPassFrame" "(" Type "," FrameState ")"
                 | "finishCallFailFrame" "(" FrameState ")"
                 | "finishCallPassFrameWriteback" "(" Type "," FrameState "," Map ")"
                 | "finishCallFailFrameWriteback" "(" FrameState "," Map ")"

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

  rule <k> loadFuncs(func: F:Id = _T:Type(P1:Param, P2:Param) { fail E:Exp ; }; REST:FuncDecls) => loadFuncs(REST) ... </k>
       <functions> Funcs => Funcs [ F <- Fn2Fail(paramBorrow(P1), paramType(P1), paramName(P1), paramBorrow(P2), paramType(P2), paramName(P2), E) ] </functions>

     rule <k> F:Id() => E ~> finishCallPassFrame(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

  rule <k> F:Id() => E ~> finishCallFailFrame(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> Next </next-loc>

  rule <k> F:Id(A:Val) => E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>
    requires acceptsBorrowArg(B, A)

     rule <k> F:Id(A:Val) => S ~> E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
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
                <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
                <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
                <fixed-vars> Fixed => .Set </fixed-vars>
                <next-loc> L => L +Int 1 </next-loc>
          requires acceptsBorrowArg(B, A)

     rule <k> F:Id(A:Val) => E ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B, A)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 1 </next-loc>
    requires acceptsBorrowArg(B, A)

  rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) callWritebacks(B2, A2)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
     <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>
    requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

  rule <k> F:Id(A1:Val, A2:Val) => E ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, L), callWritebacks(B1, A1) callWritebacks(B2, A2)) ... </k>
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
       <imm-borrow-hosts> ImmHosts => .Set </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts => .Set </mut-borrow-hosts>
                <pinned-hosts> PinnedHosts => .Set </pinned-hosts>
       <fixed-vars> Fixed => .Set </fixed-vars>
       <next-loc> L => L +Int 2 </next-loc>
    requires acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2)

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
       <functions> ... F |-> Fn2Fail(B1, _:Type, _:Id, B2, _:Type, _:Id, _:Exp) ... </functions>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>
    requires notBool(acceptsBorrowArg(B1, A1) andBool acceptsBorrowArg(B2, A2))

  rule <k> V:Val ~> finishCallPass(T) => Result(NIL, normalizeValue(T, V), false) ... </k>
  rule <k> V:Val ~> finishCallFail => Result(V, NIL, true) ... </k>

     rule <k> V:Val ~> finishCallPassFrame(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) => Result(NIL, normalizeValue(T, V), false) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <mem-modes> _ => Modes </mem-modes>
       <stack-live> _ => Stack </stack-live>
       <gc-live> _ => GC </gc-live>
       <wild-live> _ => Wild </wild-live>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
     <pinned-hosts> _ => PinnedHosts </pinned-hosts>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

           rule <k> V:Val ~> finishCallPassFrameWriteback(T, FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next), W:Map) => Result(NIL, normalizeValue(T, V), false) ... </k>
                <env> _ => Env </env>
                <types> _ => Types </types>
                <store> Current => writebackStore(Store, Current, W) </store>
                <limits> _ => Limits </limits>
                <mem-modes> _ => Modes </mem-modes>
                <stack-live> _ => Stack </stack-live>
                <gc-live> _ => GC </gc-live>
                <wild-live> _ => Wild </wild-live>
                <borrow-vars> _ => BorrowVars </borrow-vars>
                <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
                <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
           <pinned-hosts> _ => PinnedHosts </pinned-hosts>
                <fixed-vars> _ => Fixed </fixed-vars>
                <next-loc> _ => Next </next-loc>

  rule <k> V:Val ~> finishCallFailFrame(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next)) => Result(V, NIL, true) ... </k>
       <env> _ => Env </env>
       <types> _ => Types </types>
       <store> _ => Store </store>
       <limits> _ => Limits </limits>
       <mem-modes> _ => Modes </mem-modes>
       <stack-live> _ => Stack </stack-live>
       <gc-live> _ => GC </gc-live>
       <wild-live> _ => Wild </wild-live>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
     <pinned-hosts> _ => PinnedHosts </pinned-hosts>
       <fixed-vars> _ => Fixed </fixed-vars>
       <next-loc> _ => Next </next-loc>

     rule <k> V:Val ~> finishCallFailFrameWriteback(FrameState(Env, Types, Store, Limits, Modes, Stack, GC, Wild, BorrowVars, ImmHosts, MutHosts, PinnedHosts, Fixed, Next), W:Map) => Result(V, NIL, true) ... </k>
                <env> _ => Env </env>
                <types> _ => Types </types>
                <store> Current => writebackStore(Store, Current, W) </store>
                <limits> _ => Limits </limits>
                <mem-modes> _ => Modes </mem-modes>
                <stack-live> _ => Stack </stack-live>
                <gc-live> _ => GC </gc-live>
                <wild-live> _ => Wild </wild-live>
                <borrow-vars> _ => BorrowVars </borrow-vars>
                <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
                <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
           <pinned-hosts> _ => PinnedHosts </pinned-hosts>
                <fixed-vars> _ => Fixed </fixed-vars>
                <next-loc> _ => Next </next-loc>

  // -------------------------------------------------------------------------
  // Statement sequencing and blocks
  // -------------------------------------------------------------------------

     syntax KItem ::= "DeferScope" "(" List "," Map "," Set "," Set "," Set ")"
                 | "Deferred" "(" Stmts ")"
                 | "enterBlock"
                 | "leaveBlock"
                 | "popBlock"
                 | "runDefers" "(" List ")"

  rule <k> { SS:Stmts } => enterBlock ~> SS ~> leaveBlock ... </k>

  rule <k> enterBlock => .K ... </k>
       <defer-stack> DS => ListItem(DeferScope(.List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) DS </defer-stack>
       <borrow-vars> BorrowVars </borrow-vars>
       <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
       <pinned-hosts> PinnedHosts </pinned-hosts>

  rule <k> defer { SS:Stmts } => .K ... </k>
       <defer-stack> ListItem(DeferScope(OLD:List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(ListItem(Deferred(SS)) OLD, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> defer _:Block ~> _ => FS </k>
       <defer-stack> .List </defer-stack>
       <failsafe> FS </failsafe>
       <failsafe-triggered> _ => true </failsafe-triggered>

  rule <k> leaveBlock => runDefers(DS) ~> popBlock ... </k>
       <defer-stack> ListItem(DeferScope(DS:List, _:Map, _:Set, _:Set, _:Set)) _:List </defer-stack>

  rule <k> runDefers(.List) => .K ... </k>
  rule <k> runDefers(ListItem(Deferred(SS:Stmts)) REST:List) => SS ~> runDefers(REST) ... </k>

  rule <k> popBlock => .K ... </k>
       <defer-stack> ListItem(DeferScope(_:List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
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
        <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
        <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
        <pinned-hosts> PinnedHosts </pinned-hosts>
     requires notBool(X in Fixed) andBool notBool(X in_keys(Limits))
      andBool notBool(isImmutableBorrowVar(X, BorrowVars))
      andBool notBool(X in ImmHosts) andBool notBool(X in MutHosts)
      andBool notBool(X in PinnedHosts)

  rule <k> X:Id = _:Val ; ~> _ => FS </k>
        <borrow-vars> BorrowVars </borrow-vars>
        <imm-borrow-hosts> ImmHosts </imm-borrow-hosts>
        <mut-borrow-hosts> MutHosts </mut-borrow-hosts>
        <pinned-hosts> PinnedHosts </pinned-hosts>
        <failsafe> FS </failsafe>
        <failsafe-triggered> _ => true </failsafe-triggered>
     requires isImmutableBorrowVar(X, BorrowVars) orBool X in ImmHosts orBool X in MutHosts orBool X in PinnedHosts

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

     rule normalizeLike(I32(_:Int), V:Val) => normalizeValue(int32, V)
     rule normalizeLike(I64(_:Int), V:Val) => normalizeValue(int64, V)
     rule normalizeLike(TBB32(_:Int), V:Val) => normalizeValue(tbb32, V)
     rule normalizeLike(ERR, V:Val) => normalizeValue(tbb32, V)
     rule normalizeLike(_:Val, V:Val) => V [owise]

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

     rule <k> fall L:Id ; ~> _:Stmts ~> leaveBlock ~> endPick(ALL:PickCases) => leaveBlock ~> selectPickLabel(L, ALL) ~> endPick(ALL) ... </k>
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

  rule <k> exit V:Val ; => runDefers(ListItem(Deferred(SS)) DS) ~> exit V ; ... </k>
       <defer-stack> ListItem(DeferScope(ListItem(Deferred(SS:Stmts)) DS:List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(.List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> exit ( V:Val ) ; => runDefers(ListItem(Deferred(SS)) DS) ~> exit ( V ) ; ... </k>
       <defer-stack> ListItem(DeferScope(ListItem(Deferred(SS:Stmts)) DS:List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => ListItem(DeferScope(.List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST </defer-stack>

  rule <k> exit V:Val ; => exit V ; ... </k>
       <defer-stack> ListItem(DeferScope(.List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
       <imm-borrow-hosts> _ => ImmHosts </imm-borrow-hosts>
       <mut-borrow-hosts> _ => MutHosts </mut-borrow-hosts>
       <pinned-hosts> _ => PinnedHosts </pinned-hosts>

  rule <k> exit ( V:Val ) ; => exit ( V ) ; ... </k>
       <defer-stack> ListItem(DeferScope(.List, BorrowVars, ImmHosts, MutHosts, PinnedHosts)) REST => REST </defer-stack>
       <borrow-vars> _ => BorrowVars </borrow-vars>
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