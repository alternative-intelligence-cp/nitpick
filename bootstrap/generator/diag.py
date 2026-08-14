"""Structured diagnostics for the Nitpick bootstrap seed.

THROWAWAY (D-085), and deliberately minimal -- the real diagnostics core is
cycle 0.0.6, written in Nitpick.

What matters here is that a diagnostic carries a STABLE CODE and a location,
separately from its message. The harness asserts on codes and spans, never on
message text, so that messages stay free to improve without breaking the suite
(0.0.5). A diagnostic that is only a formatted string cannot be asserted against.
"""


class Diag:
    __slots__ = ("code", "path", "line", "col", "msg", "phase")

    def __init__(self, code, path, line, col, msg, phase):
        self.code = code
        self.path = path
        self.line = line
        self.col = col
        self.msg = msg
        self.phase = phase       # lex | parse | check | emit

    def __str__(self):
        return "%s:%d:%d: %s: %s" % (self.path, self.line, self.col, self.code, self.msg)


class NpkError(Exception):
    """Any seed diagnostic. Carries a Diag; str() renders it."""

    def __init__(self, diag):
        super().__init__(str(diag))
        self.diag = diag
