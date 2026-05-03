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
endmodule
