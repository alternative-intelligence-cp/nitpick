; unconditional-apartness -- a SPEC control (step 5, D-302): the clause 1.5.6 wrote and 1.5.6c found
; false by reading, planted back. `npk_small_free`'s three list neighbours are `apart-when` the freed
; chunk was full -- only then does the body move it between lists and only then are they three OTHER
; chunks; asserted apart unconditionally, the hypothesis is false for the ordinary free into a chunk
; already on the partial list, whose head IS that chunk. The generated entry checker sees it at the
; first small free of any program: `ASSUMPTION @npk_small_free: (apart (partial-list head) (chunk))`,
; here on `drop_string`'s 27th step, every seed (the prototype measured the same clause false on 5,276
; of 5,276 calls). A spec control patches `runtime/npkrt.spec` in the run's build directory, never the
; tree; the floor is untouched.
program: tests/backend/programs/drop_string.npk
verdict: ASSUMPTION
within: 2
spec-old:
           ((load64 mem (+ npk_cls_part (* 8 (load64 mem (+ (- ip (mod ip 65536)) 8))))) 65536 apart-when (= (load64 mem (+ (- ip (mod ip 65536)) 24)) 0))
spec-new:
           ((load64 mem (+ npk_cls_part (* 8 (load64 mem (+ (- ip (mod ip 65536)) 8))))) 65536)
