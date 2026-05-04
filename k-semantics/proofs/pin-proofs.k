requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-PIN-PROOFS

module ARIA-PIN-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-PIN-PROOFS
  imports ARIA
  imports ARIA-PIN-PROOFS-SYNTAX

  // Concrete proof terms use generated production labels for statements so
  // kprove parses the LHS as object-language terms with the intended sorts.

  claim
    <k> `#__ARIA-SYNTAX_Exp_Id`(#token("value", "Id")) => PINPTRLOC(0) ... </k>
    <env> #token("value", "Id") |-> 0 </env>
    <pinned-hosts> .Set => SetItem(#token("value", "Id")) </pinned-hosts>

  claim
    <k> `<-_=_;_ARIA-SYNTAX_Stmt_Exp_Exp`(PINPTRLOC(0), I32(9)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(PINPTRLOC(0), #token("x", "Id"), I32(9)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(PINPATHLOC(0), #token("x", "Id"), I32(9)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("value", "Id"), I32(9)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <limits> .Map </limits>
    <borrow-vars> .Map </borrow-vars>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <pinned-hosts> SetItem(#token("value", "Id")) </pinned-hosts>
    <fixed-vars> .Set </fixed-vars>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule
