requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-FIELD-ALIAS-PROOFS

module ARIA-FIELD-ALIAS-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-FIELD-ALIAS-PROOFS
  imports ARIA
  imports ARIA-FIELD-ALIAS-PROOFS-SYNTAX

  // Concrete proof terms use generated production labels for statements so
  // kprove parses the LHS as object-language Stmt terms rather than Exp terms.

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("al", "Id"), I32(33)) => .K ... </k>
    <env> #token("al", "Id") |-> 0 #token("pair", "Id") |-> 1 </env>
    <types> #token("al", "Id") |-> int32 </types>
    <limits> .Map </limits>
    <store>
      0 |-> I32(10)
      1 |-> StructVal(#token("Pair", "Id"), (#token("a", "Id") |-> I32(10) #token("b", "Id") |-> I32(20)))
      =>
      0 |-> I32(33)
      1 |-> StructVal(#token("Pair", "Id"), (#token("a", "Id") |-> I32(33) #token("b", "Id") |-> I32(20)))
    </store>
    <borrow-vars> #token("al", "Id") |-> MutBorrow </borrow-vars>
    <borrow-aliases> #token("al", "Id") |-> FieldAlias(#token("pair", "Id"), #token("a", "Id")) </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <fixed-vars> .Set </fixed-vars>
    <pinned-hosts> .Set </pinned-hosts>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("al", "Id"), I32(44)) => .K ... </k>
    <env> #token("al", "Id") |-> 0 #token("box", "Id") |-> 1 </env>
    <limits> .Map </limits>
    <store>
      0 |-> I32(10)
      1 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(20))) #token("tag", "Id") |-> I32(1)))
      =>
      0 |-> I32(44)
      1 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(44) #token("y", "Id") |-> I32(20))) #token("tag", "Id") |-> I32(1)))
    </store>
    <borrow-vars> #token("al", "Id") |-> MutBorrow </borrow-vars>
    <borrow-aliases> #token("al", "Id") |-> NestedFieldAlias(#token("box", "Id"), #token("leaf", "Id"), #token("x", "Id")) </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <fixed-vars> .Set </fixed-vars>
    <pinned-hosts> .Set </pinned-hosts>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("al", "Id"), I32(33)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <limits> .Map </limits>
    <borrow-vars> #token("al", "Id") |-> ImmBorrow </borrow-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
    <fixed-vars> .Set </fixed-vars>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule
