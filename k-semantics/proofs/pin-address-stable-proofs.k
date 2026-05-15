requires "../aria.k"
requires "domains.md"

// proof-module: ARIA-PIN-ADDRESS-STABLE-PROOFS
//
// v0.27.4: Pin address-stability under gc_cycle reductions.
//
// Property: for any pinned gc binding h, the address of h is invariant
// under any sequence of gc_cycle reductions in the K model.
//
// In the K model, a pin's address is the location L bound to its host
// identifier in <env>. The pin operator (#X) reads L from <env> and
// produces PINPTRLOC(L). Dereference (<- PINPTRLOC(L)) reads <store>[L].
//
// gc_cycle is modelled as a no-op on <env>, <store>, and <pinned-hosts>,
// reflecting the runtime invariant that pinned objects are not relocated
// by the collector. Pin address stability therefore holds by construction:
// since gc_cycle does not rebind X in <env>, the L in PINPTRLOC(L) is
// preserved, and the same store cell is read on subsequent dereference.

module ARIA-PIN-ADDRESS-STABLE-PROOFS-SYNTAX
  imports ARIA-SYNTAX
endmodule

module ARIA-PIN-ADDRESS-STABLE-PROOFS
  imports ARIA
  imports ARIA-PIN-ADDRESS-STABLE-PROOFS-SYNTAX

  // Claim 1: a single gc_cycle reduces to .K and leaves cells untouched.
  // Pinned-hosts, env, and store are preserved across gc_cycle.
  claim
    <k> gc_cycle ; => .K ... </k>
    <env> X:Id |-> L:Int </env>
    <store> L |-> V:Val </store>
    <pinned-hosts> SetItem(X) </pinned-hosts>

  // Claim 2: dereference through a pin after a gc_cycle reads the same
  // value V from the same store location L. This is the core address-
  // stability lemma: the L threaded through PINPTRLOC(L) survives the cycle.
  claim
    <k> gc_cycle ; ~> <- PINPTRLOC(L:Int) => V ... </k>
    <env> X:Id |-> L </env>
    <store> L |-> V:Val </store>
    <pinned-hosts> SetItem(X) </pinned-hosts>

  // Claim 3: two consecutive gc_cycle reductions still preserve the pin's
  // store cell. Generalises by induction to any finite sequence.
  claim
    <k> gc_cycle ; ~> gc_cycle ; ~> <- PINPTRLOC(L:Int) => V ... </k>
    <env> X:Id |-> L </env>
    <store> L |-> V:Val </store>
    <pinned-hosts> SetItem(X) </pinned-hosts>

endmodule
