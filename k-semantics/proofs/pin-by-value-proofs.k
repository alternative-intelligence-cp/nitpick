requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-PIN-BY-VALUE-PROOFS

module ARIA-PIN-BY-VALUE-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-PIN-BY-VALUE-PROOFS
  imports ARIA
  imports ARIA-PIN-BY-VALUE-PROOFS-SYNTAX

  // Pinned hosts are blocked before strict argument or exit-value evaluation can
  // erase the source identifier. These claims mirror K tests 085-090.

  claim
    <k> `_(_)_ARIA-SYNTAX_Exp_Id_Exp`(#token("identity", "Id"), #token("value", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <functions> #token("identity", "Id") |-> Fn1Pass(int32, CopyBorrow, int32, #token("x", "Id"), #token("x", "Id")) </functions>
    <pinned-hosts> SetItem(#token("value", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_(_)_ARIA-SYNTAX_Exp_Id_Exp`(#token("read_ref", "Id"), #token("value", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <functions> #token("read_ref", "Id") |-> Fn1Pass(int32, ImmBorrow, int32, #token("x", "Id"), #token("x", "Id")) </functions>
    <pinned-hosts> SetItem(#token("value", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_(_,_)_ARIA-SYNTAX_Exp_Id_Exp_Exp`(#token("combine", "Id"), #token("left", "Id"), #token("right", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <functions> #token("combine", "Id") |-> Fn2Pass(int32, CopyBorrow, int32, #token("x", "Id"), CopyBorrow, int32, #token("y", "Id"), #token("x", "Id")) </functions>
    <pinned-hosts> SetItem(#token("left", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_(_,_)_ARIA-SYNTAX_Exp_Id_Exp_Exp`(#token("combine", "Id"), #token("left", "Id"), #token("right", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <functions> #token("combine", "Id") |-> Fn2Pass(int32, CopyBorrow, int32, #token("x", "Id"), CopyBorrow, int32, #token("y", "Id"), #token("x", "Id")) </functions>
    <pinned-hosts> SetItem(#token("right", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `exit_;_ARIA-SYNTAX_Stmt_Exp`(#token("value", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <pinned-hosts> SetItem(#token("value", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `exit(_);_ARIA-SYNTAX_Stmt_Exp`(#token("value", "Id")) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <pinned-hosts> SetItem(#token("value", "Id")) </pinned-hosts>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule
