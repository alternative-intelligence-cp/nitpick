#!/usr/bin/env python3
# attrnorm.py BASE.ll NEW.ll [more pairs...] -- compare two emissions with the function attribute landing 115 (DEF-251) put on
# every `define` STRIPPED from both: `null_pointer_is_valid`, written by `ll_fn_open` beside `"split-stack"`, under which LLVM no
# longer reads a store through `null` as undefined behaviour (the -O2 leg had lowered such a `main` to an empty body that fell
# through into the next function). The attribute moves every emitted define line and nothing else, so an emission comparison
# across the landing is read with it stripped; the residue is what the landing changed beyond it (expected: nothing). The twin
# of tidnorm.py (type ids) and sitenorm.py (the site table). Prints `same`/`DIFFERENT` per pair and a summary; a DIFFERENT pair
# is read (diff the stripped texts). A measurement, outside every gate.
import re, sys
ATTR = re.compile(r'\) "split-stack" null_pointer_is_valid \{')
def norm(text): return ATTR.sub(') "split-stack" {', text)
args = sys.argv[1:]
if len(args) % 2 or not args: sys.exit("usage: attrnorm.py BASE.ll NEW.ll [BASE2.ll NEW2.ll ...]")
same = diff = skipped = stripped = 0
for i in range(0, len(args), 2):
    a, b = open(args[i]).read(), open(args[i+1]).read()
    if not a.strip() or not b.strip(): skipped += 1; print("one side empty (refused there): %s" % args[i+1]); continue
    stripped += len(ATTR.findall(b))
    if norm(a) == norm(b): same += 1; print("same      %s" % args[i+1])
    else:
        diff += 1; print("DIFFERENT %s" % args[i+1])
        open(args[i] + ".norm", "w").write(norm(a)); open(args[i+1] + ".norm", "w").write(norm(b))
print("ATTRNORM: %d same, %d DIFFERENT, %d skipped (one side empty) (%d attributes stripped from the new side)" % (same, diff, skipped, stripped))
