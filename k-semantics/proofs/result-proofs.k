requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-RESULT-PROOFS

module ARIA-RESULT-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-RESULT-PROOFS
  imports ARIA
  imports ARIA-RESULT-PROOFS-SYNTAX

  claim <k> pass V:Val => Result(NIL, V, false) </k>
  claim <k> fail E:Val => Result(E, NIL, true) </k>

  claim <k> raw Result(E:Val, V:Val, B:Bool) => V </k>
  claim <k> drop Result(E:Val, V:Val, B:Bool) => NIL </k>

  claim <k> Result(E:Val, V:Val, false) defaults F:Val => V </k>
  claim <k> Result(E:Val, V:Val, true) defaults F:Val => F </k>

  claim <k> ?! Result(E:Val, V:Val, false) => V </k>

  claim
    <k> ?! Result(E:Val, V:Val, true) => invokeFailsafe(E) </k>
    <failsafe-triggered> false => true </failsafe-triggered>
endmodule