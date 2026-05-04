requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-BORROW-PATH-PROOFS

module ARIA-BORROW-PATH-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-BORROW-PATH-PROOFS
  imports ARIA
  imports ARIA-BORROW-PATH-PROOFS-SYNTAX

  // Concrete path-sensitive borrow claims mirror K tests 079-080 and 094-095:
  // sibling fields remain assignable, while exact borrowed paths route to failsafe.

  claim
    <k> `_._=_;_ARIA-SYNTAX_Stmt_Id_Id_Exp`(#token("pair", "Id"), #token("b", "Id"), I32(22)) => .K ... </k>
    <env> #token("pair", "Id") |-> 0 </env>
    <store>
      0 |-> StructVal(#token("Pair", "Id"), (#token("a", "Id") |-> I32(10) #token("b", "Id") |-> I32(20)))
      =>
      0 |-> StructVal(#token("Pair", "Id"), (#token("a", "Id") |-> I32(10) #token("b", "Id") |-> I32(22)))
    </store>
    <structs> #token("Pair", "Id") |-> Struct2(int32, #token("a", "Id"), int32, #token("b", "Id")) </structs>
    <fixed-vars> .Set </fixed-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> SetItem(FieldPath(#token("pair", "Id"), #token("a", "Id"))) </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>

  claim
    <k> `_._=_;_ARIA-SYNTAX_Stmt_Id_Id_Exp`(#token("pair", "Id"), #token("a", "Id"), I32(33)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <fixed-vars> .Set </fixed-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> SetItem(FieldPath(#token("pair", "Id"), #token("a", "Id"))) </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_._._=_;_ARIA-SYNTAX_Stmt_Id_Id_Id_Exp`(#token("box", "Id"), #token("leaf", "Id"), #token("y", "Id"), I32(22)) => .K ... </k>
    <env> #token("box", "Id") |-> 0 </env>
    <store>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(20))) #token("tag", "Id") |-> I32(1)))
      =>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(22))) #token("tag", "Id") |-> I32(1)))
    </store>
    <fixed-vars> .Set </fixed-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> SetItem(NestedFieldPath(#token("box", "Id"), #token("leaf", "Id"), #token("x", "Id"))) </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>

  claim
    <k> `_._._=_;_ARIA-SYNTAX_Stmt_Id_Id_Id_Exp`(#token("box", "Id"), #token("leaf", "Id"), #token("x", "Id"), I32(33)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <fixed-vars> .Set </fixed-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> SetItem(NestedFieldPath(#token("box", "Id"), #token("leaf", "Id"), #token("x", "Id"))) </mut-borrow-hosts>
    <pinned-hosts> .Set </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule
