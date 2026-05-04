requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-ARITHMETIC-PROOFS

module ARIA-ARITHMETIC-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-ARITHMETIC-PROOFS
  imports ARIA
  imports ARIA-ARITHMETIC-PROOFS-SYNTAX

  claim <k> I32(I1:Int) + I32(I2:Int) => I32(wrapI32(I1 +Int I2)) </k>
  claim <k> I32(I1:Int) - I32(I2:Int) => I32(wrapI32(I1 -Int I2)) </k>
  claim <k> I32(I1:Int) * I32(I2:Int) => I32(wrapI32(I1 *Int I2)) </k>

  claim <k> I64(I1:Int) + I64(I2:Int) => I64(wrapI64(I1 +Int I2)) </k>
  claim <k> I64(I1:Int) - I64(I2:Int) => I64(wrapI64(I1 -Int I2)) </k>
  claim <k> I64(I1:Int) * I64(I2:Int) => I64(wrapI64(I1 *Int I2)) </k>

  claim <k> TBB32(I1:Int) + TBB32(I2:Int) => normalizeTBB32(I1 +Int I2) </k>
  claim <k> TBB32(I1:Int) - TBB32(I2:Int) => normalizeTBB32(I1 -Int I2) </k>
  claim <k> TBB32(I1:Int) * TBB32(I2:Int) => normalizeTBB32(I1 *Int I2) </k>

  claim <k> I32(_:Int) / I32(0) => ERR </k>
  claim <k> I64(_:Int) / I64(0) => ERR </k>
  claim <k> TBB32(_:Int) / TBB32(0) => ERR </k>

  claim <k> ERR + V:Val => ERR </k>
    requires isNumericValue(V)
  claim <k> V:Val + ERR => ERR </k>
    requires isNumericValue(V)
  claim <k> Unknown + V:Val => Unknown </k>
    requires isNumericValue(V)
  claim <k> V:Val + Unknown => Unknown </k>
    requires isNumericValue(V)
endmodule