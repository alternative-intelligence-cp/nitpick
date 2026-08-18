# Type rejection

Whole programs that **load and resolve cleanly** and are refused by the **type
checker**. Driven by `tools/check.npk`, which is the real frontend.

## Why this is its own suite

Three rejection suites, three stages, and collapsing any two of them would make
"correctly refused" mean less — a file that stopped early would satisfy a test
written about a later stage.

| Suite | Refused by | Reaches a type checker? |
|---|---|---|
| `tests/modules/rejection/` | the **loader** — a missing module, an unresolvable name | no |
| `tests/types/rejection/` | the **type checker** and the static analyses — this suite | yes, and a rule says no |
| `tests/rejection/` | the **backend** — a correct program at a rung that cannot lower it yet (D-085) | yes, and it passes |

**And this suite has a counterweight.** `tests/accept/` holds programs the
frontend must accept in full silence, because no number of rejections can show
that a rule fires *only* when it should — a checker that refused every program
would pass every case here. That distinction became load-bearing with cycle 0.5's
analyses, which fail closed by design and are therefore likeliest to over-refuse.

A file here that fails to *resolve* is a broken test, not a passing one: the
harness asserts on the codes emitted, and a resolve error carries a different code
from the type error the test was written about.

The same applies one stage further in. **The analyses run only over a program the
type checker accepted**, so a case written for a `NITPICK-BORROW` code must
type-check cleanly — a type error in it stops the program before any analysis
sees it, and the test then fails for a reason unrelated to what it guards.

## Writing one

Expectations live in the file, assert on **codes and spans, never on message
text** (`BUILD_REFERENCE.md` §7.1), and a file with no `// expect-error:` is a
fixture another test imports rather than a test:

```nitpick
// expect-error: NITPICK-TYPE-007
// expect-error-at: 4:5
```

**Write each case from the specification's own example.** Cycle 0.4.6 lost three
defects to tests written from whatever was convenient, and 0.4.7 lost five to
constructs that parsed and were never read downstream. A case that exercises the
form the reference actually shows is the one that catches those.
