requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-CONTROL-RULES-PROOFS

module ARIA-CONTROL-RULES-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-CONTROL-RULES-PROOFS
  imports ARIA
  imports ARIA-CONTROL-RULES-PROOFS-SYNTAX

  claim
    <k>
      I32(1) ~> finishPickCase(I32(2), { exit 10 ; }, (I32(2)) { exit 20 ; }, .PickCases)
      => selectPick(I32(2), (I32(2)) { exit 20 ; }, .PickCases)
    </k>

  claim
    <k>
      selectPick(I32(2), (I32(2)) { exit 20 ; }, .PickCases)
      => { exit 20 ; } ~> endPick(.PickCases)
    </k>

  claim
    <k>
      `fall_;_ARIA-SYNTAX_Stmt_Id`(#token("two", "Id")) ~> .Stmts ~> endPick(.PickCases)
      => selectPickLabel(#token("two", "Id"), .PickCases) ~> endPick(.PickCases)
    </k>

  claim
    <k>
      selectPickLabel(#token("two", "Id"), `_,__ARIA-SYNTAX_PickCases_PickCase_PickCases`(`_:(_)__ARIA-SYNTAX_PickCase_Id_PickPattern_Block`(#token("two", "Id"), `I32(_)_ARIA-SYNTAX_Val_Int`(#token("2", "Int")), `{_}_ARIA-SYNTAX_Block_Stmts`(`___ARIA-SYNTAX_Stmts_Stmt_Stmts`(`exit_;_ARIA-SYNTAX_Stmt_Exp`(#token("30", "Int")), `.List{"___ARIA-SYNTAX_Stmts_Stmt_Stmts"}_Stmts`(.KList)))), .PickCases)) ~> endPick(.PickCases)
      => .K
    </k>
    <defer-stack> .List </defer-stack>
    <wild-live> .Set </wild-live>
    <exit-code> 0 => 30 </exit-code>

  claim
    <k> limit < #token("r_lt_100", "Id") > int32 : #token("a", "Id") = I32(50) ; => .K ... </k>
    <rules> #token("r_lt_100", "Id") |-> RuleSet($ < 100) </rules>
    <env> .Map => #token("a", "Id") |-> 0 </env>
    <types> .Map => #token("a", "Id") |-> int32 </types>
    <store> .Map => 0 |-> I32(50) </store>
    <limits> .Map => #token("a", "Id") |-> #token("r_lt_100", "Id") </limits>
    <loop-index> 123 </loop-index>
    <next-loc> 0 => 1 </next-loc>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("a", "Id"), I32(75)) => .K ... </k>
    <rules> #token("r_lt_100", "Id") |-> RuleSet($ < 100) </rules>
    <env> #token("a", "Id") |-> 0 </env>
    <types> #token("a", "Id") |-> int32 </types>
    <store> 0 |-> I32(50) => 0 |-> I32(75) </store>
    <limits> #token("a", "Id") |-> #token("r_lt_100", "Id") </limits>
    <fixed-vars> .Set </fixed-vars>
    <pinned-hosts> .Set </pinned-hosts>
    <borrow-vars> .Map </borrow-vars>
    <borrow-aliases> .Map </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <loop-index> 123 </loop-index>

  claim
    <k> `_=_;_ARIA-SYNTAX_Stmt_Id_Exp`(#token("a", "Id"), I32(200)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <rules> #token("r_lt_100", "Id") |-> RuleSet($ < 100) </rules>
    <env> #token("a", "Id") |-> 0 </env>
    <types> #token("a", "Id") |-> int32 </types>
    <store> 0 |-> I32(50) </store>
    <limits> #token("a", "Id") |-> #token("r_lt_100", "Id") </limits>
    <fixed-vars> .Set </fixed-vars>
    <pinned-hosts> .Set </pinned-hosts>
    <borrow-vars> .Map </borrow-vars>
    <borrow-aliases> .Map </borrow-aliases>
    <imm-borrow-hosts> .Set </imm-borrow-hosts>
    <mut-borrow-hosts> .Set </mut-borrow-hosts>
    <loop-index> 123 </loop-index>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> limit < #token("r_small_positive", "Id") > int32 : #token("a", "Id") = I32(42) ; => .K ... </k>
    <rules>
      #token("r_positive", "Id") |-> RuleSet($ > 0)
      #token("r_small_positive", "Id") |-> RuleSet(limit < #token("r_positive", "Id") > , $ < 100)
    </rules>
    <env> .Map => #token("a", "Id") |-> 0 </env>
    <types> .Map => #token("a", "Id") |-> int32 </types>
    <store> .Map => 0 |-> I32(42) </store>
    <limits> .Map => #token("a", "Id") |-> #token("r_small_positive", "Id") </limits>
    <loop-index> 123 </loop-index>
    <next-loc> 0 => 1 </next-loc>
endmodule