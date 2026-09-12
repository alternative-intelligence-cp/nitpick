#!/usr/bin/env python3
"""tcb_floor.py -- TCB.md's floor table, generated (P-26; D-288 §2.10, 1.5.6).

    python3 bootstrap/harness/tcb_floor.py            # print the region
    python3 bootstrap/harness/tcb_floor.py --write    # rewrite it in meta/specs/TCB.md

The rows are what `check_tcb_floor_current` holds the document to: every
define of runtime/npkrt.ll with its class (`_floor_classes`) and its
disposition from runtime/npkrt.spec and the committed
runtime/npkrt.obligations (`floor.tcb_rows`). Run it after a floor edit, a
spec edit or a `npkg verify --record`, and commit the document with them.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))
sys.path.insert(0, HERE)

import harness   # noqa: E402
import floor     # noqa: E402


def main(argv):
    spec = os.path.join(ROOT, "runtime", "npkrt.spec")
    man = os.path.join(ROOT, "runtime", "npkrt.obligations")
    spec_text = open(spec, encoding="utf-8").read() if os.path.exists(spec) else ""
    man_text = open(man, encoding="utf-8").read() if os.path.exists(man) else ""
    classes = harness._floor_classes()
    floor_ll = open(harness.RUNTIME_LL, encoding="utf-8").read()
    region = floor.tcb_region(spec_text, classes, man_text, floor.read_models(ROOT))
    sysregion = floor.syscalls_region(floor_ll, classes)
    resregion = floor.residue_region(spec_text, man_text, floor.read_models(ROOT))
    if "--write" not in argv:
        print(region)
        print()
        print(sysregion)
        print()
        print(resregion)
        return 0
    path = os.path.join(ROOT, "meta", "specs", "TCB.md")
    doc = open(path, encoding="utf-8").read()
    new = re.sub(r"(<!-- BEGIN floor-table -->\n).*?(\n<!-- END floor-table -->)",
                 lambda m: m.group(1) + region + m.group(2), doc, flags=re.S)
    new = re.sub(r"(<!-- BEGIN floor-syscalls -->\n).*?(\n<!-- END floor-syscalls -->)",
                 lambda m: m.group(1) + sysregion + m.group(2), new, flags=re.S)
    new = re.sub(r"(<!-- BEGIN floor-residue -->\n).*?(\n<!-- END floor-residue -->)",
                 lambda m: m.group(1) + resregion + m.group(2), new, flags=re.S)
    if new == doc:
        print("TCB.md: the floor table and the syscall table are current")
        return 0
    open(path, "w", encoding="utf-8").write(new)
    print("TCB.md: the floor table (%d rows) and the syscall table (%d rows) rewritten"
          % (region.count("\n") - 1, len(floor.syscall_rows(floor_ll, classes))))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
