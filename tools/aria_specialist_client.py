"""
Aria Specialist — client library

Connects any Python tool (aria_shell, IDE plugin, build system) to the
aria_specialist_server daemon.

Usage (non-persistent, one query):
    from aria_specialist_client import ask
    answer = ask("How do I write a function that returns a Result?")
    print(answer)

Usage (persistent subprocess, amortises model load time across many queries):
    from aria_specialist_client import AriaSpecialistClient
    with AriaSpecialistClient() as client:
        print(client.ask("What is the syntax for pattern matching?"))
        print(client.ask("Show me a struct definition"))

The server is started on first use and remains alive for the lifetime of the
'with' block (or until .close() is called).
"""

import os
import sys
import json
import subprocess
import threading
import time

TOOLS_DIR      = os.path.dirname(os.path.abspath(__file__))
SERVER_SCRIPT  = os.path.join(TOOLS_DIR, "aria_specialist_server.py")
READY_TIMEOUT  = 180   # seconds to wait for model to load (it's ~7B params)


class AriaSpecialistClient:
    """
    Long-lived client that keeps the specialist subprocess alive.
    Thread-safe.  Each call is sequential (the LLM can only handle one
    generation at a time anyway).
    """

    def __init__(self, python: str = sys.executable):
        self._python   = python
        self._proc     = None
        self._lock     = threading.Lock()
        self._req_id   = 0
        self._ready    = False

    # ------------------------------------------------------------------ #
    #  Context manager support                                             #
    # ------------------------------------------------------------------ #

    def __enter__(self):
        self.start()
        return self

    def __exit__(self, *_):
        self.close()

    # ------------------------------------------------------------------ #
    #  Lifecycle                                                           #
    # ------------------------------------------------------------------ #

    def start(self):
        """Launch the server subprocess and wait for readiness."""
        if self._proc and self._proc.poll() is None:
            return  # already running

        self._proc = subprocess.Popen(
            [self._python, SERVER_SCRIPT],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=sys.stderr,   # pass-through so diagnostics remain visible
            text=True,
            bufsize=1,           # line-buffered
        )

        # Wait for the ready JSON line
        deadline = time.time() + READY_TIMEOUT
        while time.time() < deadline:
            if self._proc.poll() is not None:
                raise RuntimeError(
                    f"aria_specialist_server exited prematurely "
                    f"(code {self._proc.returncode})"
                )
            line = self._proc.stdout.readline()
            if not line:
                time.sleep(0.2)
                continue
            try:
                msg = json.loads(line)
                if msg.get("ready"):
                    self._ready = True
                    return
            except json.JSONDecodeError:
                pass  # model loading progress lines come on stderr

        raise TimeoutError(
            f"aria_specialist_server did not become ready within "
            f"{READY_TIMEOUT}s"
        )

    def close(self):
        """Terminate the server gracefully."""
        if self._proc:
            try:
                self._proc.stdin.close()
                self._proc.wait(timeout=5)
            except Exception:
                self._proc.terminate()
            self._proc = None
            self._ready = False

    # ------------------------------------------------------------------ #
    #  Query                                                               #
    # ------------------------------------------------------------------ #

    def ask(
        self,
        instruction: str,
        context: str = "",
        max_tokens: int = 512,
        temperature: float = 0.2,
    ) -> str:
        """
        Ask the specialist a question.  Returns the model response as a string.
        Raises RuntimeError on server error.
        """
        if not self._ready:
            self.start()

        with self._lock:
            self._req_id += 1
            req = {
                "id":          self._req_id,
                "instruction": instruction,
                "context":     context,
                "max_tokens":  max_tokens,
                "temperature": temperature,
            }
            self._proc.stdin.write(json.dumps(req) + "\n")
            self._proc.stdin.flush()

            raw = self._proc.stdout.readline()
            if not raw:
                raise RuntimeError("aria_specialist_server closed stdout unexpectedly")
            resp = json.loads(raw)

        if not resp.get("ok"):
            raise RuntimeError(f"Specialist error: {resp.get('error', '?')}")
        return resp["response"]


# ------------------------------------------------------------------ #
#  Convenience one-shot function                                       #
# ------------------------------------------------------------------ #

def ask(instruction: str, context: str = "", **kwargs) -> str:
    """
    One-shot query — starts a fresh server process, asks, shuts down.
    Convenient but slow (model loads every call).  Use AriaSpecialistClient
    for interactive tools.
    """
    with AriaSpecialistClient() as c:
        return c.ask(instruction, context, **kwargs)


# ------------------------------------------------------------------ #
#  CLI smoke-test                                                      #
# ------------------------------------------------------------------ #

if __name__ == "__main__":
    question = " ".join(sys.argv[1:]) or "How do I declare a mutable variable in Aria?"
    print(f"Question: {question}\n", flush=True)
    answer = ask(question)
    print(f"Answer:\n{answer}")
