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
endmodule
