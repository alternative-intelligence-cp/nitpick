"""1.5.1b step 1 (D-248): give every source file its header.

Every file's FIRST declaration is `mod:<basename>;` (`mod:<dir>;` for a
`dir/mod.npk`). This script inserts the header before the first non-comment
line of every `.npk` under the grammar sweep's roots that lacks it, and shifts
every `// expect-error-at:` / `// expect-note-at:` pin in that file by one --
D-237's exact matching then proves the shift complete, since a missed pin fails
by name. Kept with the plan for the record; `--dry-run` reports only.

    python3 meta/roadmap/1.5/tools/add_headers.py [--dry-run]
"""
import os, re, sys

ROOT = os.path.abspath(os.path.dirname(__file__))
while not os.path.exists(os.path.join(ROOT, "nitpick.toml")):
    ROOT = os.path.dirname(ROOT)
ROOTS = ("src", "lib", "tools", "npkg", "tests")
PIN_RE = re.compile(r"^(\s*//\s*expect-(?:error|note)-at:\s*)(\d+)(.*)$")
IDENT_RE = re.compile(r"^[A-Za-z_][A-Za-z0-9_]*$")
# The two rejection tests whose point is a missing or a wrong header (D-248's
# own cases) must never be "fixed" by this script.
SKIP = {"tests/modules/rejection/header_missing.npk",
        "tests/modules/rejection/header_mismatch.npk"}


def keywords():
    """The lexer's keyword table (`src/frontend/keywords.npk`): a module name is
    an identifier, and a keyword is not one -- `mod:derive;` is a parse error
    -- so a file named after a keyword must be renamed, never headed."""
    text = open(os.path.join(ROOT, "src", "frontend", "keywords.npk"), encoding="utf-8").read()
    return set(re.findall(r'string_eq\(text, "([A-Za-z_][A-Za-z0-9_]*)"\)', text))


def expected_name(path):
    base = os.path.basename(path)[:-4]
    if base == "mod":
        base = os.path.basename(os.path.dirname(path))
    return base


def main(argv):
    dry = "--dry-run" in argv
    had, added, odd, mismatched = 0, 0, [], []
    kw = keywords()
    for root in ROOTS:
        for dirpath, _, names in os.walk(os.path.join(ROOT, root)):
            for n in sorted(names):
                if not n.endswith(".npk"):
                    continue
                p = os.path.join(dirpath, n)
                rel = os.path.relpath(p, ROOT)
                if rel in SKIP:
                    continue
                want = expected_name(p)
                if not IDENT_RE.match(want) or want in kw:
                    odd.append(rel)
                    continue
                with open(p, encoding="utf-8") as fh:
                    lines = fh.read().split("\n")
                k = 0
                while k < len(lines) and (not lines[k].strip() or lines[k].lstrip().startswith("//")):
                    k += 1
                if k < len(lines) and re.match(r"^\s*mod:%s\s*;" % re.escape(want), lines[k]):
                    had += 1
                    continue
                if k < len(lines) and re.match(r"^\s*mod:\w+\s*;", lines[k]):
                    mismatched.append((rel, lines[k].strip()))
                out = []
                for i, line in enumerate(lines):
                    m = PIN_RE.match(line)
                    if m and i != k:
                        line = "%s%d%s" % (m.group(1), int(m.group(2)) + 1, m.group(3))
                    out.append(line)
                out.insert(k, "mod:%s;" % want)
                added += 1
                if not dry:
                    with open(p, "w", encoding="utf-8") as fh:
                        fh.write("\n".join(out))
    print("%s: %d file(s) already had their header, %d gained one%s"
          % ("dry run" if dry else "applied", had, added, "" if not dry else " (would)"))
    for rel, first in mismatched:
        print("  first declaration was `%s` (an import now, after the header): %s" % (first, rel))
    for rel in odd:
        print("  NOT AN IDENTIFIER (or a keyword), left alone -- rename it: %s" % rel)
    return 1 if odd else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
