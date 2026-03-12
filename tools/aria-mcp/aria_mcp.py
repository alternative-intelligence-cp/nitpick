#!/usr/bin/env python3
"""
aria-mcp — MCP stdio server for the Aria language toolchain.

Transport : stdio (JSON-RPC 2.0, newline-delimited)
Zero deps : pure Python 3.8+ stdlib only

Tools:
  aria_compile(source)  — compile Aria source via ariac, structured result
  aria_check(source)    — run aria-safety audit, structured findings
  aria_docs(query)      — section-level search over aria_ref.md

Binary discovery order (for ariac and aria-safety):
  1. Environment variable  ARIAC_BIN / ARIA_SAFETY_BIN
  2. <repo-root>/build/ariac   or   <repo-root>/tools/aria-safety/aria-safety
  3. $PATH
"""

import json
import sys
import os
import subprocess
import tempfile
import re
import shutil
from pathlib import Path

# ── binary / path resolution ──────────────────────────────────────────────

SCRIPT_DIR = Path(__file__).resolve().parent
REPO_ROOT   = SCRIPT_DIR.parent.parent  # tools/aria-mcp/ → tools/ → repo root


def _find_bin(env_var: str, *candidates: Path) -> str | None:
    v = os.environ.get(env_var)
    if v and Path(v).is_file() and os.access(v, os.X_OK):
        return v
    for c in candidates:
        if c.is_file() and os.access(c, os.X_OK):
            return str(c)
    return None


ARIAC_BIN   = _find_bin("ARIAC_BIN",
                         REPO_ROOT / "build" / "ariac") \
              or shutil.which("ariac")

SAFETY_BIN  = _find_bin("ARIA_SAFETY_BIN",
                         REPO_ROOT / "tools" / "aria-safety" / "aria-safety") \
              or shutil.which("aria-safety")

ARIA_REF_MD = os.environ.get("ARIA_REF_MD",
                              str(REPO_ROOT / ".internal" / "aria_ref.md"))

# ── aria_ref.md section index ─────────────────────────────────────────────

def _build_section_index(path: str) -> list[dict]:
    """
    Parse aria_ref.md into sections keyed by their ## heading.
    Returns list of { "heading": str, "body": str } dicts.
    """
    sections: list[dict] = []
    if not os.path.isfile(path):
        return sections

    with open(path) as f:
        raw = f.read()

    parts = re.split(r'^(##[^\n]*)', raw, flags=re.MULTILINE)
    # parts[0] is pre-first-heading text, then alternating heading / body
    for i in range(1, len(parts) - 1, 2):
        sections.append({
            "heading": parts[i].strip(),
            "body":    parts[i + 1].strip(),
        })
    return sections


_SECTION_INDEX: list[dict] = []  # lazy-loaded on first aria_docs call


def _get_sections() -> list[dict]:
    global _SECTION_INDEX
    if not _SECTION_INDEX:
        _SECTION_INDEX = _build_section_index(ARIA_REF_MD)
    return _SECTION_INDEX

# ── tool: aria_compile ────────────────────────────────────────────────────

def aria_compile(source: str) -> dict:
    if not ARIAC_BIN:
        return {
            "success":  False,
            "errors":   ["ariac binary not found — set ARIAC_BIN env var or add to $PATH"],
            "warnings": [],
            "output":   "",
        }

    with tempfile.NamedTemporaryFile(suffix=".aria", mode="w",
                                     delete=False, encoding="utf-8") as f:
        f.write(source)
        src_path = f.name

    out_path = src_path + ".out"
    try:
        proc = subprocess.run(
            [ARIAC_BIN, src_path, "-o", out_path],
            capture_output=True, text=True, timeout=30,
        )
        errors: list[str]   = []
        warnings: list[str] = []
        for line in (proc.stderr + proc.stdout).splitlines():
            l = line.lower()
            if "error" in l:
                errors.append(line)
            elif "warning" in l or "warn" in l:
                warnings.append(line)
            elif line.strip():
                # unclassified non-empty output goes to errors on failure,
                # warnings on success
                (errors if proc.returncode != 0 else warnings).append(line)

        return {
            "success":  proc.returncode == 0,
            "errors":   errors,
            "warnings": warnings,
            "output":   proc.stdout.strip(),
        }

    except subprocess.TimeoutExpired:
        return {
            "success":  False,
            "errors":   ["compiler timed out (> 30 s)"],
            "warnings": [],
            "output":   "",
        }
    finally:
        try:
            os.unlink(src_path)
        except OSError:
            pass
        try:
            os.unlink(out_path)
        except OSError:
            pass

# ── tool: aria_check ─────────────────────────────────────────────────────

# Matches:  file.aria:42: [TAG] message description
_FINDING_RE = re.compile(r'^(.+?):(\d+):\s+\[([A-Z_]+)\]\s+(.+)$')


def aria_check(source: str) -> dict:
    if not SAFETY_BIN:
        return {
            "issues": [],
            "error":  "aria-safety binary not found — set ARIA_SAFETY_BIN env var or add to $PATH",
        }

    with tempfile.NamedTemporaryFile(suffix=".aria", mode="w",
                                     delete=False, encoding="utf-8") as f:
        f.write(source)
        src_path = f.name

    try:
        proc = subprocess.run(
            [SAFETY_BIN, src_path],
            capture_output=True, text=True, timeout=10,
        )
        issues: list[dict] = []
        for line in proc.stdout.splitlines():
            line = line.strip()
            if not line:
                continue
            m = _FINDING_RE.match(line)
            if m:
                issues.append({
                    "line":    int(m.group(2)),
                    "tag":     m.group(3),
                    "message": m.group(4),
                })
            else:
                issues.append({"line": 0, "tag": "INFO", "message": line})

        return {"issues": issues}

    except subprocess.TimeoutExpired:
        return {"issues": [], "error": "aria-safety timed out"}
    finally:
        try:
            os.unlink(src_path)
        except OSError:
            pass

# ── tool: aria_docs ───────────────────────────────────────────────────────

def aria_docs(query: str) -> dict:
    sections = _get_sections()
    if not sections:
        return {"excerpt": f"aria_ref.md not found or empty (path: {ARIA_REF_MD})"}

    terms = [t for t in re.split(r'\s+', query.lower().strip()) if t]
    if not terms:
        return {"excerpt": "Empty query — provide search terms, e.g. 'result propagation' or 'pointer system'"}

    # Score each section by weighted term hits:
    #   heading match counts 3×, body match counts 1× per term per occurrence
    scored: list[tuple[int, int, dict]] = []  # (score, original_index, section)
    for idx, sec in enumerate(sections):
        h = sec["heading"].lower()
        b = sec["body"].lower()
        score = 0
        for t in terms:
            score += h.count(t) * 3
            score += b.count(t)
        if score > 0:
            scored.append((score, idx, sec))

    if not scored:
        return {"excerpt": f"No matches found for: {query!r}"}

    scored.sort(key=lambda x: -x[0])

    # Return top 3 sections (truncated to 40 lines each to keep responses manageable)
    parts: list[str] = []
    for _, _, sec in scored[:3]:
        lines = (sec["heading"] + "\n\n" + sec["body"]).splitlines()
        chunk = "\n".join(lines[:40])
        if len(lines) > 40:
            chunk += f"\n\n... ({len(lines) - 40} more lines — query aria_docs more specifically)"
        parts.append(chunk)

    return {"excerpt": "\n\n---\n\n".join(parts)}

# ── MCP JSON-RPC 2.0 protocol ─────────────────────────────────────────────

TOOL_DEFINITIONS = [
    {
        "name": "aria_compile",
        "description": (
            "Compile Aria source code using the ariac compiler. "
            "Returns { success, errors[], warnings[], output }. "
            "Aria uses a Result<T> type system — all user-defined functions implicitly "
            "return Result<T>. Use aria_docs to look up syntax before writing code."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "source": {
                    "type":        "string",
                    "description": "Complete Aria source code to compile.",
                }
            },
            "required": ["source"],
        },
    },
    {
        "name": "aria_check",
        "description": (
            "Run the aria-safety static audit tool on Aria source code. "
            "Returns { issues: [{ line, tag, message }] }. "
            "Tags: [WILD] manual memory, [RAW] raw() Result bypass, "
            "[DROP] discarded Result<T>, [OK] unknown type bypass, "
            "[WEAK_CAS] weak CAS must be in retry loop, "
            "[RELAXED] relaxed atomic ordering, "
            "[FAILSAFE] empty or trivial failsafe block."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "source": {
                    "type":        "string",
                    "description": "Aria source code to audit for safety issues.",
                }
            },
            "required": ["source"],
        },
    },
    {
        "name": "aria_docs",
        "description": (
            "Search the Aria language reference card (aria_ref.md) for documentation "
            "on any type, operator, or language construct. "
            "Returns relevant section excerpts. "
            "Example queries: 'result propagation', 'pointer system', "
            "'string interpolation', 'atomic operations', 'control flow', "
            "'wild allocation', 'failsafe block', 'generics contracts'."
        ),
        "inputSchema": {
            "type": "object",
            "properties": {
                "query": {
                    "type":        "string",
                    "description": "Terms to search for in the Aria language reference.",
                }
            },
            "required": ["query"],
        },
    },
]

SERVER_INFO  = {"name": "aria-mcp", "version": "0.1.0"}
CAPABILITIES = {"tools": {}}


def _handle(req: dict) -> dict | None:
    method = req.get("method", "")
    rid    = req.get("id")          # None for notifications
    params = req.get("params") or {}

    def ok(result: object) -> dict | None:
        if rid is None:
            return None
        return {"jsonrpc": "2.0", "id": rid, "result": result}

    def err(code: int, message: str, data: object = None) -> dict:
        e: dict = {"code": code, "message": message}
        if data is not None:
            e["data"] = data
        return {"jsonrpc": "2.0", "id": rid, "error": e}

    # ── handshake ─────────────────────────────────────────────
    if method == "initialize":
        return ok({
            "protocolVersion": "2024-11-05",
            "serverInfo":      SERVER_INFO,
            "capabilities":    CAPABILITIES,
        })

    if method in ("notifications/initialized", "initialized"):
        return None  # notification — no response

    if method == "ping":
        return ok({})

    # ── tools ─────────────────────────────────────────────────
    if method == "tools/list":
        return ok({"tools": TOOL_DEFINITIONS})

    if method == "tools/call":
        name = params.get("name", "")
        args = params.get("arguments") or {}

        if name == "aria_compile":
            r    = aria_compile(args.get("source", ""))
            text = json.dumps(r, indent=2)
            return ok({"content": [{"type": "text", "text": text}]})

        if name == "aria_check":
            r    = aria_check(args.get("source", ""))
            text = json.dumps(r, indent=2)
            return ok({"content": [{"type": "text", "text": text}]})

        if name == "aria_docs":
            r = aria_docs(args.get("query", ""))
            return ok({"content": [{"type": "text", "text": r["excerpt"]}]})

        return err(-32601, f"Unknown tool: {name!r}")

    # ── fallback ───────────────────────────────────────────────
    if rid is not None:
        return err(-32601, f"Method not found: {method!r}")
    return None   # unknown notification — silently ignore


# ── stdio loop ────────────────────────────────────────────────────────────

def main() -> None:
    log = lambda msg: print(f"[aria-mcp] {msg}", file=sys.stderr, flush=True)
    log(f"ariac      = {ARIAC_BIN or '(not found)'}")
    log(f"aria-safety= {SAFETY_BIN or '(not found)'}")
    log(f"aria_ref   = {ARIA_REF_MD}")
    log("ready — waiting for JSON-RPC requests on stdin")

    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue

        try:
            req = json.loads(raw)
        except json.JSONDecodeError as exc:
            resp: dict = {
                "jsonrpc": "2.0",
                "id":      None,
                "error":   {"code": -32700, "message": f"Parse error: {exc}"},
            }
            print(json.dumps(resp), flush=True)
            continue

        try:
            resp = _handle(req)
        except Exception as exc:
            resp = {
                "jsonrpc": "2.0",
                "id":      req.get("id"),
                "error":   {"code": -32603, "message": f"Internal error: {exc}"},
            }

        if resp is not None:
            print(json.dumps(resp), flush=True)


if __name__ == "__main__":
    main()
