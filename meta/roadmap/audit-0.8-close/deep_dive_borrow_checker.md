# Deep Dive: The Borrow Checker (escape + bindings analyses)

Reviewed against nitpick-native at **0.8 closed** (commit `267c329`). Sources read in
full: `src/frontend/analysis/escape.npk` (1231 lines), `analysis/bindings.npk` (1329),
plus D-004, D-065, D-116, D-117, D-118, D-119, D-138 and the 0.5 cycle archives.

**Every finding below marked CONFIRMED was demonstrated against the real checker** —
`tools/check.npk` built by `quickcheck.py` in a scratch clone (the working repo was
not touched). Probe files and how to re-run: §6. A control probe (`pass (@x);`)
was refused with `NITPICK-BORROW-001`, so the accepts below are genuine holes, not
a broken build.

---

## 1. What exists, and its shape

The "borrow checker" is two files, deliberately not one:

| File | Cycle | Carries |
|---|---|---|
| `escape.npk` | 0.5.1 (+0.8.1 relaxations) | D-004's five rules: borrows go down, never up. Marking-to-fixpoint over a per-binding `holds` table, then one reporting pass (D-116). Caller-side rules A/B replace parameter qualifiers (D-117). One narrowing + three relaxations (D-138). |
| `bindings.npk` | 0.5.2–0.5.3 | Definite assignment (`must`/`may`, D-118), moved/freed as one machinery (a free is a move, D-119), the `unknown` taint on `Result.value` (D-007), `fixed`/`const`/`nodrop` qualifier enforcement. |

Design properties worth affirming before the findings — this is a genuinely
well-built pair of analyses:

- **Fail-closed everywhere.** Depth exhaustion, unclassified expression shapes,
  unsettled fixpoints, and untrackable bindings all *refuse* rather than assume
  (`BORROW_TOO_DEEP`, `BORROW_INTERNAL`, `BORROW_UNSETTLED`, `BORROW_UNTRACKED`).
- **Marking/reporting split** with monotone marks and a bounded round count —
  D-116 implemented exactly as written.
- **The five-set merge asymmetry** (`must`/`checked` intersect; `may`/`moved`/
  `freed` union) is written side by side with the reasoning inline, which is the
  right defence for the one thing easiest to get backwards.
- **Future rules pre-written**: the `await` clause, spawn-args, and closure-capture
  rules exist now, unreachable, so 1.1 does not add rules after verification.

The findings below do not contradict that assessment; they are exactly the class
the 0.5 retrospective predicted: *"an analysis that is right on straight-line code
and wrong after a merge passes every test written the easy way."* Each comes with
a minimal repro the current suites lack.

---

## 2. CONFIRMED soundness holes (checker accepts unsafe code)

### F-1. Rule B's single-borrow exemption is defeated by a non-borrow pointer destination — **HIGH, live at the current rung**

`escape_call_like` triggers rule B only when **two or more borrow arguments** are
passed (`borrowed >= 2i32`). But the stored *value* must be a borrow — the
*destination* only needs to be any pointer whose pointee can hold one:

```nitpick
struct:Cell = { int32->:slot; };
func:stash = NIL(Cell->:h, int32->:q) { h.slot = q; pass NIL; };   // legal callee-side (both are the caller's)
func:caller = NIL(Cell->:cell) {
    int32:x = 5i32;
    drop stash(cell, @x);      // borrowed == 1 → exemption → accepted. UNSOUND.
    pass NIL;
};
```

**Verdict: exit 0, zero diagnostics** (probe A). `cell` is a parameter — it points
into the caller's caller, which outlives `x` — and now holds a dangling pointer the
moment `caller` returns. D-117's exemption text ("with nothing else passed in there
is no second borrow to store") conflates "second borrow" with "second pointer": the
stored thing must be a borrow, the *place it is stored through* need not be.

**The exemption is confirmed as the exact hole.** The identical call with *two*
borrow arguments (`stash2(cell, @x)` where `cell = @y`) correctly fires rule B,
marks `cell`, and refuses the subsequent `pass cell` with `NITPICK-BORROW-001`
(probe2 `a1`). The realistic receiver form — `func:caller = Cell->(Cell->:cell)`
returning `cell` after `stash(cell, @x)` — is also accepted (probe2 `a2`). So rule
B works; the single-borrow exemption is what one pointer destination walks through.

**Fix shape:** count *pointer-typed arguments whose pointee can carry a pointer*
(borrow or not, receiver included) as potential destinations. Rule B applies when
`borrows ≥ 1 && destinations ≥ 1` (excluding the borrow itself when it is the only
pointer). `tt_intern(@t, k)` — one borrow, scalars otherwise — stays exempt, so the
compiler's own idiom survives. Must be validated against the full acceptance suite
plus a self-compile, exactly as D-138's relaxations were.

### F-2. Expression-`pick` `give` launders a borrow — **HIGH, live when 0.9 lowers pick-expressions**

`escape_expr` walks a `ExprPickExpr`'s arms for internal violations and then
**answers `false` unconditionally** (escape.npk:831–836) — the `give`n value is
never connected to the pick's own value:

```nitpick
func:pf = int32->() {
    int32:x = 5i32;
    int32->:p = pick (1i32) { (*) { give @x; } };
    pass p;                       // dangling return — accepted
};
```

**Verdict: exit 0** (probe F), while the direct `pass (@x);` control is refused.
The code comments defer this to "0.5.4's question, once arm coverage exists" —
0.5.4 landed exhaustiveness and nobody came back. Latent only because the backend
rung-refuses pick expressions until 0.9; the checker's promise is already broken.

**Fix shape:** an expression-pick is borrowy iff any arm's `give` operand is
borrowy; `give` operands also flow into rule-2 checks when the pick is a return
value. The arm walk already visits every `give` — it needs to carry the verdict up
instead of dropping it.

### F-3. A borrow of an inner-block local survives the block — **HIGH, latent until stack coloring / lifetime intrinsics**

Rule 3 compares nothing about *scope depth*. Marking `p` as holding a borrow is
done without recording where the borrow's target lives:

```nitpick
func:pc = int32() {
    int32:zero = 0i32;
    int32->:p = @zero;
    { int32:x = 5i32; p = @x; }   // x's scope ends here
    pass (<-p);                    // reads x after its scope — accepted
};
```

**Verdict: exit 0** (probe C). Note the boundary precisely: the *direct* return of
the borrow — `p = @x` in an inner block, then `pass p` — **is** caught
(`NITPICK-BORROW-001`, probe2 `c2`), because that is rule 2 seeing a borrow leave
the frame. What slips is the **deref-and-use path**: `pass (<-p)` returns the
*loaded int*, not a borrow, so rule 2 sees nothing escape — yet `<-p` reads `x`'s
slot after `x`'s block ended. The same slip covers any in-frame use of `p` after
the inner scope (`arr[<-p] = …`, passing `<-p` onward). Benign *today* because the
backend allocas every local for the whole frame and emits no `llvm.lifetime`
markers — but the moment a rung adds lifetime intrinsics or stack coloring (an
obvious optimization, and LLVM does it unasked at `-O2` once markers exist), this
is silent stack-slot reuse blessed by the checker. `SAFETY_ARCHITECTURE.md` claims
"no dangling references" as a Layer-1 property; this is a dangling reference.

**Fix shape:** record scope depth (or declaring-block id) per binding; an
assignment that stores a borrow rooted at a *strictly inner* block into an outer
binding is refused (`p = @x` above), same code as `BORROW_STORED`. Declarations
can never trip it (the initialiser's roots are visible at declaration). Loop
bodies re-entering count as inner. This also covers the same shape reached
through `defer` bodies referencing inner-block borrows.

### F-4. The `unknown` taint does not track param-rooted or field-rooted `Result`s — **MEDIUM-HIGH, live**

`check_value_read` / `err_test_of` resolve the tracked slot through
`binding_slot`, which requires the place root to be a **`SYM_STMT` local** and
collapses any member path to its root:

```nitpick
struct:Two = { Result<int32>:a; Result<int32>:b; };
func:pd1 = int32(Two->:t)  { pass (t.b.value); };          // param-rooted: never tracked — accepted
func:pd2 = int32() {
    Two:t = Two{ a: mk(), b: mk() };
    if (t.a.is_error) { pass 0i32; }
    pass (t.b.value);              // checked a, read b — root-conflated — accepted
};
```

**Verdict: exit 0 for both** (probe D; the bare-local control `pass (r.value);`
correctly fired `NITPICK-TAINT-001`). Two distinct gaps: (a) any `Result` reached
through a parameter is invisible to the discipline D-007 rests its actuator
argument on; (b) for local aggregates, checking *one* field licenses reading
*every* field's `.value`.

**Fix shape:** track `(root-slot, interned field path)` pairs instead of bare
slots — the `checked` set becomes a small per-function list rather than a bitmap
(paths are few). Param-rooted reads: either track param slots the same way
(params have DeclIds to key on) or, minimally, refuse `.value` through a param
root as "cannot be shown checked" — conservative, matching the untracked-binding
posture elsewhere. Gap (b)'s conflation also affects `moved`/`freed` root
collapsing, but there it is *conservative* (documented, safe); only `checked`
collapses in the unsafe direction. That asymmetry is worth a comment where both
walks share `binding_slot`.

### F-5. A binding named in a `defer` can be moved/freed after registration — **MEDIUM-HIGH, live when the real allocator lands (dalloc becomes a real free)**

D-118 checks a defer body **at registration** (correct for use-before-write) but
nothing re-examines invalidations that happen *after*:

```nitpick
func:pe = NIL() {
    wild int8->:p = alloc(16i64);
    defer { dalloc(p); }          // will run at scope exit
    wild int8->:q = move(p);      // p invalidated after registration
    dalloc(q);
    pass NIL;                     // scope exit: dalloc(p) — double-free at runtime
};
```

**Verdict: exit 0** (probe E). Today `dalloc` is a no-op so nothing burns; the day
the real allocator lands, this is a checker-blessed double-free that the
allocator's own runtime detection then catches by trapping — the exact inversion
of "compile-time where possible."

**Fix shape:** collect the binding slots a defer body *reads* at registration
(one extra walk); a later `move`/`dalloc`/`ralloc` of a collected slot in the
same scope is refused ("this binding is read by a `defer` registered above; it
cannot be invalidated after that point"). Reinitialisation (assignment) stays
legal — the deferred read then sees the new value, which is the D-065 semantics.

---

## 3. CONFIRMED precision defect (checker refuses correct code)

### F-6. `pick` arms leak `may`/`moved`/`freed` into their siblings — **MEDIUM**

`assign_pick` absorbs each arm's state into the *walk state* before the next arm
copies from it (bindings.npk:789–814: `state_copy(st)` after prior
`state_absorb_may(st, one)`), so exclusive arms see each other's effects:

```nitpick
fixed int32:k;
pick (n) {
    (0i32) { k = 1i32; },
    (*)    { k = 2i32; }     // NITPICK-ASSIGN-002 — "may be the second time". It cannot be.
}
```

**Verdict: refused at the second arm** (probe B), while the identical `if`/`else`
in the same file is accepted. Beyond `fixed`: an arm that `dalloc`s a pointer
makes a *sibling* arm's use read as use-after-free. Initialise-`fixed`-per-case is
exactly the idiom `pick` exists for, so this pushes people to `if` chains — the
opposite of what the analysis should do (bindings.npk:794 makes the same argument
for taint refinement).

**Fix shape:** copy every arm from the pristine pre-pick state (as `if` does);
accumulate `may`/`moved`/`freed` in a side accumulator; apply to `st` once after
the arm loop. The `must` accumulator already works this way — the asymmetry was
the bug.

---

## 4. Reasoned observations (not probe-confirmed; carried into planning)

1. **The `await` clause vs the I/O traits — a decision is owed before 1.1.**
   `escape_expr` refuses any borrow in an `await` operand (`BORROW_SUSPENDED`,
   pre-written for 1.1). But `IO_REFERENCE.md`'s own traits are
   `async func:read = int64(Self, uint8[]:dest, Duration)` — a slice is a borrow
   (D-070), so `await src.read(dest, deadline)` is refused by the rule as written.
   The whole async I/O surface is unwritable under it. Note D-004 rule 4 predates
   D-032 (pinning), D-034 (arena-stable frames), D-062/D-083 (lexical task/thread
   lifetime); under those, an intra-task borrow cannot outlive its frame across a
   suspension — the caller's frame cannot resume, move, or die while the awaited
   callee holds its borrow. **Proposed decision for 1.1** (drafted in the roadmap
   work): a borrow may be passed *into* an awaited call and held *by* the awaited
   callee; what remains refused is a borrow crossing a **spawn** (task or thread)
   — `escape_spawn_args` already implements exactly that. The refusal-shaped
   `BORROW_SUSPENDED` code is kept for anything the narrowed rule still catches.
2. **Aliasing exclusivity (`$$m` exclusive / `$$i` shared) is implemented
   nowhere and scheduled nowhere.** D-117 explicitly parks it ("a different
   question with a different answer, not settled here"). Today two `$$m` borrows
   of the same binding in one call are accepted. The Z3 disjointness story is
   1.3; the *structural* rule (no two `$$m` of overlapping places in one
   expression; no `$$m` aliasing a live `$$i`) is implementable in `escape.npk`'s
   walk and belongs on the roadmap explicitly (planned into 0.9.x as a
   subcycle in the mirrored roadmap).
3. **`stack`'s escape rule** is folded into the same walk, but nothing yet
   connects it to *sizes* (a `stack` binding of unbounded size). Noted for 0.9
   layout work.
4. **Pattern/`for` bindings can be `move`d without tracking** (`invalidate_place`
   requires `SYM_STMT`) — silently allowed, harmless today (per-iteration value
   copies), inconsistent with "no untracked invalidation." One-line refusal.
5. **io_uring buffer lifecycle** (for the 1.1 executor): a submitted SQE's buffer
   is retained by the kernel past the submitting call's return. Executor-internal
   buffers must be owned/`wild`, never borrow-backed — the borrow rules protect
   the *language* surface, and the executor must not launder around them
   internally. Recorded as a 1.1 design constraint.
6. **`escape_binding` answers `false` for empty-symbol identifiers only via the
   `ident_holds` inversion** — `ident_holds(0) == true` (conservative) but
   `escape_binding(a==0) == false` (assumes clean). The two defaults point in
   opposite directions for the same "unresolved name" case. Unreachable while
   resolution refuses unresolved names earlier; worth unifying toward
   conservative for defence in depth.

---

## 5. Where the fixes land (feeds the mirrored roadmap)

| Finding | Proposed home | Rationale |
|---|---|---|
| F-1 rule B destinations | **0.9.0** (opens Phase B's next cycle with analysis repairs) | live now; small, testable change + acceptance-suite revalidation |
| F-6 pick arm pollution | **0.9.0** | live now; refuses correct code the 0.9 test corpus will want to write |
| F-4 taint paths | **0.9.0** | live now; needed before `?`-family operators (0.9) widen taint-relevant idioms |
| F-5 defer invalidation | **0.9.x with the real allocator** | becomes dangerous exactly when `dalloc` does |
| F-2 pick-expr give | **0.9.x before pick-expression lowering** | must precede the rung that makes it reachable |
| F-3 scope depth | **0.9.x** (before any lifetime-intrinsic work) | cheap now; a prerequisite gate is recorded on the future stack-coloring item |
| await-clause narrowing | **1.1 opening decision** | blocks the entire async I/O surface otherwise |
| `$$m`/`$$i` structural exclusivity | **0.9.x** | rules-before-constructs, per the escape analysis's own precedent |

Each of the six confirmed findings ships with its probe as the rejection/acceptance
test (probes A, C, E, F become rejection cases; B's pick half becomes an
*acceptance* case; D's pd1/pd2 become rejections and pd3 stays the control).

## 6. Reproducing the verdicts

Scratch clone + probes (the working repo is never touched):

```
git clone /home/randy/Workspace/REPOS/nitpick-native  <scratch>/nn-probe
cd <scratch>/nn-probe
python3 bootstrap/harness/quickcheck.py  <this-folder>/probes/probe_*.npk
```

Probe files are archived beside this document in `probes/` (copied from the
scratchpad). Observed verdicts, 2026-08-20, at commit `267c329`:
A exit 0 · B `NITPICK-ASSIGN-002` at 6:18 (pick arm) with `pb_if` silent ·
C exit 0 · D `NITPICK-TAINT-001` at 14:12 (control only) · E exit 0 · F exit 0 ·
G (control) `NITPICK-BORROW-001`.
