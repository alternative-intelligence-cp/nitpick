requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-POINTER-PATH-PROOFS

module ARIA-POINTER-PATH-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-POINTER-PATH-PROOFS
  imports ARIA
  imports ARIA-POINTER-PATH-PROOFS-SYNTAX

  // Concrete nested pointer and pin-path claims mirror K tests 072-075 and
  // pin-derived pointer alias claims mirror K tests 104-105.
  // A pointer-valued field read can become a second pointer-member root; the
  // equivalent pin-derived path can read but cannot mutate through the pin root
  // or through an alias that stores the derived PINPATHLOC value.

  claim
    <k> `_->__ARIA-SYNTAX_Exp_Exp_Id`(`_->__ARIA-SYNTAX_Exp_Exp_Id`(PTRLOC(0), #token("leaf", "Id")), #token("x", "Id")) => I32(10) ... </k>
    <store>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> PTRLOC(1) #token("tag", "Id") |-> I32(1)))
      1 |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10)))
    </store>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(`_->__ARIA-SYNTAX_Exp_Exp_Id`(PTRLOC(0), #token("leaf", "Id")), #token("x", "Id"), I32(44)) => .K ... </k>
    <store>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> PTRLOC(1) #token("tag", "Id") |-> I32(1)))
      1 |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10)))
      =>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> PTRLOC(1) #token("tag", "Id") |-> I32(1)))
      1 |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(44)))
    </store>
    <structs> #token("Leaf", "Id") |-> Struct1(int32, #token("x", "Id")) </structs>

  claim
    <k> `_->__ARIA-SYNTAX_Exp_Exp_Id`(`_->__ARIA-SYNTAX_Exp_Exp_Id`(PINPTRLOC(0), #token("leaf", "Id")), #token("x", "Id")) => I32(10) ... </k>
    <store>
      0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> PTRLOC(1) #token("tag", "Id") |-> I32(1)))
      1 |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10)))
    </store>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(`_->__ARIA-SYNTAX_Exp_Exp_Id`(PINPTRLOC(0), #token("leaf", "Id")), #token("x", "Id"), I32(44)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <store> 0 |-> StructVal(#token("Box", "Id"), (#token("leaf", "Id") |-> PTRLOC(1) #token("tag", "Id") |-> I32(1))) </store>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_->__ARIA-SYNTAX_Exp_Exp_Id`(#token("alias", "Id"), #token("x", "Id")) => I32(10) ... </k>
    <env> #token("alias", "Id") |-> 2 </env>
    <store>
      1 |-> StructVal(#token("Leaf", "Id"), (#token("x", "Id") |-> I32(10)))
      2 |-> PINPATHLOC(1)
    </store>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(#token("alias", "Id"), #token("x", "Id"), I32(44)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <env> #token("alias", "Id") |-> 2 </env>
    <store> 2 |-> PINPATHLOC(1) </store>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule
