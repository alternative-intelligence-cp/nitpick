requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-POINTER-PROOFS

module ARIA-POINTER-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-POINTER-PROOFS
  imports ARIA
  imports ARIA-POINTER-PROOFS-SYNTAX

  // Concrete local-pointer claims mirror the executable rules behind @value,
  // <-ptr, <-ptr = value, and ptr->field operations.

  claim
    <k> `@__ARIA-SYNTAX_Exp_Id`(#token("value", "Id")) => PTRLOC(0) ... </k>
    <env> #token("value", "Id") |-> 0 </env>

  claim
    <k> `<-__ARIA-SYNTAX_Exp_Exp`(PTRLOC(0)) => I32(42) ... </k>
    <store> 0 |-> I32(42) </store>

  claim
    <k> `<-_=_;_ARIA-SYNTAX_Stmt_Exp_Exp`(PTRLOC(0), I32(37)) => .K ... </k>
    <store> 0 |-> (I32(5) => I32(37)) </store>

  claim
    <k> `<-__ARIA-SYNTAX_Exp_Exp`(I32(5)) ~> `exit_;_ARIA-SYNTAX_Stmt_Exp`(0) => `exit_;_ARIA-SYNTAX_Stmt_Exp`(7) </k>
    <failsafe> exit 7 ; </failsafe>
    <failsafe-triggered> false => true </failsafe-triggered>

  claim
    <k> `_->__ARIA-SYNTAX_Exp_Exp_Id`(PTRLOC(0), #token("x", "Id")) => I32(10) ... </k>
    <store> 0 |-> StructVal(#token("Point", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(20))) </store>

  claim
    <k> `_->_=_;_ARIA-SYNTAX_Stmt_Exp_Id_Exp`(PTRLOC(0), #token("x", "Id"), I32(37)) => .K ... </k>
    <store>
      0 |-> StructVal(#token("Point", "Id"), (#token("x", "Id") |-> I32(10) #token("y", "Id") |-> I32(20)))
      =>
      0 |-> StructVal(#token("Point", "Id"), (#token("x", "Id") |-> I32(37) #token("y", "Id") |-> I32(20)))
    </store>
    <structs> #token("Point", "Id") |-> Struct2(int32, #token("x", "Id"), int32, #token("y", "Id")) </structs>
endmodule
