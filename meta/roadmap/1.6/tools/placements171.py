#!/usr/bin/env python3 -I
"""placements171.py WORKTREE [--claude PATH] [--table new|base] [--control] -- THE PLACEMENT CHECK (management_7, 2026-10-30
04:29: "a script that compares each open row's 'landing N' with the queue, fails on a mismatch, and has a control"; 07:10: let
it find DEF-345's stale "implemented in 171" clause too; 09:52:11 and 09:56:11: past landing 172 the queue is KEYED TO THE
DEFECTS -- "the numbers are set at each cut" -- so a slot past 172 is named by its group or its lead defect, and the bullet is
cut at those names as at `landing NNN =`). A re-ruling of the queue used to leave stale placements behind in the rows and in
CLAUDE.md's queue bullet (nitpick-libs_32's FYI of 2026-10-30); this reads both against ONE table, below.

(1) ROWS. Every OPEN row of meta/roadmap/OPEN_DECISIONS.md the table names, and every OPEN row from DEF-286 on, carries a
    dated note `PLACED: <slot> --`, and the LAST such note in its entry (the header and the lines after it, up to the next
    row or a blank line) names the table's slot. A row the table names that is not OPEN fails; an OPEN row from DEF-286 on
    that the table does not name fails. The older open rows the table does not name are listed and not checked: the
    queue's era begins at DEF-286, and those rows name an owner and no landing.
(2) QUEUE. CLAUDE.md's queue bullet is cut at each `landing NNN =` and at each MARKER (`<slot> =` for a slot of MARKERS), a
    segment running to the next cut. Every DEF the table places at a cut slot is mentioned inside that slot's segment; an
    OPEN DEF mentioned inside a segment that the table places at ANOTHER cut slot fails -- the stale placement. A DEF the
    table places at a slot with no segment (LOW after, unplaced, M3, ...) may be mentioned anywhere, as the prose follows
    the segments. `DEF-a to DEF-b` and `DEF-a … DEF-b` are expanded; a slot's NAME written in the prose (`DEF-318's landing`)
    is no mention of its lead defect.
(3) The table: every slot is one of SLOTS.

--table base: the table of b122540's queue (2026-10-29's order, 171 first and 174 last), the rows check skipped, for a run
against that tree's bullet (`--claude` a copy of b122540's CLAUDE.md): it fails on DEF-345 alone, mentioned inside landing
174's segment ("implemented in 171") and placed at 171. --control: the table with DEF-357 moved from the old list's landing to
DEF-333's landing -- it must fail (rows and queue alike). Exit 0 when every check holds, 1 otherwise."""
import os, re, sys

W = sys.argv[1]
ARGS = sys.argv[2:]
CLAUDE = ARGS[ARGS.index('--claude') + 1] if '--claude' in ARGS else os.path.join(W, 'CLAUDE.md')
MODE = ARGS[ARGS.index('--table') + 1] if '--table' in ARGS else 'new'
CONTROL = '--control' in ARGS

# the named slots past landing 172 that cut the bullet (management_7, 2026-10-30 09:52:11 and 09:56:11)
MARKERS = ["the Result-fields group's landing", "the old list's landing", "DEF-333's landing", "DEF-312's landing",
           "DEF-318's landing"]
SLOTS = ['landing 171', 'landing 172', 'landing 173', 'landing 174', 'landing 175', 'LOW after landing 174',
         'LOW after landing 175', 'unplaced', 'M3', 'after M3', 'after DEF-286', "step 3d's rest"] + MARKERS + [
         "LOW after DEF-318's landing"]

def table(spec):
    t = {}
    for slot, defs in spec:
        for d in defs:
            t[d] = slot
    return t

# management_7's queue, 2026-10-30 06:59 (ruling (b)), confirmed 07:10; DEF-379 at 08:39:44, DEF-380 to DEF-384 at 09:08:19;
# DEF-385, the Result-fields group's own landing and the queue past 172 keyed to the defects at 09:52:11 and 09:56:11.
NEW = table([
    ('landing 172', [364, 365, 378, 383]),
    ("the Result-fields group's landing", [380, 381, 382, 385]),
    ("the old list's landing", [344, 345, 347, 349, 353, 354, 355, 356, 357]),
    ("DEF-333's landing", [333, 334, 335, 336, 384]),
    ("DEF-312's landing", [312, 348, 350, 325]),
    ("DEF-318's landing", [318, 319, 320, 321, 322, 323]),
    ("LOW after DEF-318's landing", [351, 352, 358]),
    ('unplaced', [369, 370, 375, 376, 377, 379]),
    ('M3', [298, 299, 313]),
    ('after M3', [286]),
    ('after DEF-286', [292, 293, 294, 291, 295]),
    ("step 3d's rest", [239]),
])
# b122540's queue (management's order of 2026-10-29 and 2026-10-30 00:42-01:37).
BASE = table([
    ('landing 171', [364, 365, 344, 345, 347, 349, 353, 354, 355, 356, 357]),
    ('landing 172', [333, 334, 335, 336]),
    ('landing 173', [312, 348, 350, 325]),
    ('landing 174', [318, 319, 320, 321, 322, 323]),
    ('LOW after landing 174', [351, 352, 358]),
    ('unplaced', [369, 370]),
    ('M3', [298, 299, 313]),
    ('after M3', [286]),
    ('after DEF-286', [292, 293, 294, 291, 295]),
    ("step 3d's rest", [239]),
])
T = dict(BASE if MODE == 'base' else NEW)
if CONTROL:
    T[357] = "DEF-333's landing"

fails = []
def fail(msg):
    fails.append(msg)
    print('FAIL ' + msg)

for d, s in sorted(T.items()):
    if s not in SLOTS:
        fail('table: DEF-%d at an unknown slot %r' % (d, s))

# --- the register ---------------------------------------------------------------------------------------------------------
OD = open(os.path.join(W, 'meta/roadmap/OPEN_DECISIONS.md'), encoding='utf-8').read().split('\n')
HDR = re.compile(r'> \*\*DEF-(\d+)(?: (?:…|\.\.\.|to) DEF-(\d+))? — ([A-Z]+)')
status = {}; entry = {}
for i, l in enumerate(OD):
    m = HDR.match(l)
    if not m:
        continue
    j = i + 1
    while j < len(OD) and OD[j].strip() != '' and not OD[j].startswith('> **DEF-'):
        j += 1
    lo = int(m.group(1)); hi = int(m.group(2)) if m.group(2) else lo
    text = ' '.join(x[2:] if x.startswith('> ') else x for x in OD[i:j])
    for d in range(lo, hi + 1):
        status[d] = m.group(3)
        entry[d] = text

if MODE != 'base':
    checked = 0
    for d in sorted(status):
        if status[d] != 'OPEN':
            continue
        if d not in T:
            if d >= 286:
                fail('row DEF-%d is OPEN and the table places it nowhere' % d)
            else:
                print('older open row DEF-%d: not in the queue, not checked' % d)
            continue
        checked += 1
        placed = re.findall(r'PLACED: (.+?) --', entry[d])
        if not placed:
            fail('row DEF-%d carries no `PLACED: <slot> --` note (the table: %s)' % (d, T[d]))
        elif placed[-1] != T[d]:
            fail('row DEF-%d: its last PLACED note says %r, the table %r' % (d, placed[-1], T[d]))
        else:
            print('ok   row DEF-%d PLACED: %s' % (d, T[d]))
    for d in sorted(T):
        if status.get(d) != 'OPEN':
            fail('the table places DEF-%d, whose row is %s' % (d, status.get(d, 'MISSING')))
    print('ROWS: %d open rows checked against the table' % checked)

# --- the queue bullet -----------------------------------------------------------------------------------------------------
C = open(CLAUDE, encoding='utf-8').read()
a = C.index('- **The queue**')
b = C.index('\n- **', a + 1)
Q = ' '.join(C[a:b].split())
cuts = [(m.start(), 'landing %s' % m.group(1)) for m in re.finditer(r'landing (\d+) =', Q)]
for mk in MARKERS:
    cuts += [(m.start(), mk) for m in re.finditer(re.escape(mk + ' ='), Q)]
cuts.sort()
def mentions(seg):
    seg = re.sub(r"DEF-\d+'s landing", ' ', seg)
    out = set()
    for m in re.finditer(r'DEF-(\d+) (?:to|…|\.\.\.) DEF-(\d+)', seg):
        out |= set(range(int(m.group(1)), int(m.group(2)) + 1))
    out |= set(int(x) for x in re.findall(r'DEF-(\d+)', seg))
    return out
cut_slots = set(s for s in T.values() if s.startswith('landing ') or s in MARKERS)
seen = [s for _, s in cuts]
for s in set(seen):
    if seen.count(s) > 1:
        fail('queue: %d `%s =` cuts in the bullet, where one is owed' % (seen.count(s), s))
for k, (pos, slot) in enumerate(cuts):
    end = cuts[k + 1][0] if k + 1 < len(cuts) else len(Q)
    seg = Q[pos:end]
    got = mentions(seg)
    want = set(d for d, s in T.items() if s == slot)
    before = len(fails)
    for d in sorted(want - got):
        fail('queue: DEF-%d is placed at %s and not mentioned in its segment' % (d, slot))
    for d in sorted(got):
        s = T.get(d)
        if status.get(d) == 'OPEN' and s is not None and s in cut_slots and s != slot:
            fail('queue: DEF-%d is mentioned in %s\'s segment and placed at %s -- a stale placement' % (d, slot, s))
    print('%s queue segment %s: %d DEFs placed there, %d mentioned' % ('ok  ' if len(fails) == before else 'BAD ', slot, len(want), len(got)))
for slot in sorted(cut_slots):
    if slot not in seen:
        fail('queue: no `%s =` segment, and the table places %s there' % (slot, ', '.join('DEF-%d' % d for d in sorted(T) if T[d] == slot)))
print('PLACEMENTS171 (%s%s, %s): %d failure(s)' % (MODE, ' --control' if CONTROL else '', os.path.relpath(CLAUDE, W) if CLAUDE.startswith(W) else CLAUDE, len(fails)))
sys.exit(1 if fails else 0)
