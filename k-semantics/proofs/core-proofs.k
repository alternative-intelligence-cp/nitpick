requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-CORE-PROOFS

module ARIA-CORE-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-CORE-PROOFS
  imports ARIA
  imports ARIA-CORE-PROOFS-SYNTAX

  claim <k> ERR + I32(1) => ERR ... </k>
  claim <k> I32(2147483647) + I32(1) => I32(-2147483648) ... </k>
  claim <k> TBB32(2147483647) + TBB32(1) => ERR ... </k>

  claim
    <k> loopIter(0, 10, 0, { .Stmts }) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(1) </k>
    <failsafe> exit 1 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("x", "Id"), I32(2)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(1) </k>
    <fixed-vars> SetItem(#token("x", "Id")) </fixed-vars>
    <failsafe> exit 1 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> ?! Result(I32(7), NIL, true) ; ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(1) </k>
    <failsafe> exit 1 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> println("hi") => I64(3) ... </k>
    <stdout> "" => "hi\n" </stdout>

  claim
    <k> #token("answer", "Id")() => Result(NIL, I32(42), false) ... </k>
    <functions> #token("answer", "Id") |-> Fn0Pass(int32, I32(42)) </functions>
    <env> #token("caller", "Id") |-> 0 </env>
    <types> #token("caller", "Id") |-> int32 </types>
    <store> 0 |-> I32(5) </store>
    <limits> .Map </limits>
    <mem-modes> .Map </mem-modes>
    <stack-live> .Set </stack-live>
    <gc-live> .Set </gc-live>
    <wild-live> .Set </wild-live>
    <borrow-vars> .Map </borrow-vars>
    <borrow-aliases> .Map </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
    <fixed-vars> .Set </fixed-vars>
    <next-loc> 1 </next-loc>

  claim
    <k> #token("broken", "Id")() => Result(I32(17), NIL, true) ... </k>
    <functions> #token("broken", "Id") |-> Fn0Fail(I32(17)) </functions>
    <env> #token("caller", "Id") |-> 0 </env>
    <types> #token("caller", "Id") |-> int32 </types>
    <store> 0 |-> I32(5) </store>
    <limits> .Map </limits>
    <mem-modes> .Map </mem-modes>
    <stack-live> .Set </stack-live>
    <gc-live> .Set </gc-live>
    <wild-live> .Set </wild-live>
    <borrow-vars> .Map </borrow-vars>
    <borrow-aliases> .Map </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
    <fixed-vars> .Set </fixed-vars>
    <next-loc> 1 </next-loc>

  claim
    <k> `_._=_;_ARIA-SYNTAX_Stmt_Id_Id_Exp`(#token("p", "Id"), #token("x", "Id"), I32(42)) => .K ... </k>
    <env> #token("p", "Id") |-> 0 </env>
    <store>
      0 |-> StructVal(#token("Point", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(20)))
      =>
      0 |-> StructVal(#token("Point", "Id"), (#token("x", "Id") |-> I32(42) #token("y", "Id") |-> I32(20)))
    </store>
    <structs> #token("Point", "Id") |-> Struct2(int32, #token("x", "Id"), int32, #token("y", "Id")) </structs>
    <fixed-vars> .Set </fixed-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
endmodule
