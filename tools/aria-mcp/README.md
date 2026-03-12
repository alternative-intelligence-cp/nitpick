# aria-mcp

MCP (Model Context Protocol) server for the Aria language toolchain.
Exposes `aria_compile`, `aria_check`, and `aria_docs` as MCP tools so any
MCP-capable AI assistant can compile, audit, and answer questions about
Aria code directly within a conversation.

**Zero external dependencies** — pure Python 3.8+ stdlib.

## Tools

| Tool | What it does |
|------|-------------|
| `aria_compile(source)` | Compile Aria source via `ariac`; returns `{ success, errors[], warnings[], output }` |
| `aria_check(source)` | Run `aria-safety` audit; returns `{ issues: [{ line, tag, message }] }` |
| `aria_docs(query)` | Section-level search over `aria_ref.md`; returns matching excerpts |

### aria_compile

```json
{ "source": "func:main = void() { stdout.write(`hello`); }" }
```
```json
{ "success": true, "errors": [], "warnings": [], "output": "" }
```

### aria_check

```json
{ "source": "wild int32*:p = aria.alloc(64);" }
```
```json
{
  "issues": [
    { "line": 1, "tag": "WILD", "message": "wild allocation — no GC safety; manual lifetime required" }
  ]
}
```

### aria_docs

```json
{ "query": "result propagation" }
```
Returns the relevant section(s) from `aria_ref.md`.

## Configuration

### VS Code / GitHub Copilot (`.vscode/mcp.json`)

```json
{
  "servers": {
    "aria": {
      "type": "stdio",
      "command": "python3",
      "args": ["/path/to/aria/tools/aria-mcp/aria_mcp.py"]
    }
  }
}
```

### Claude Desktop (`claude_desktop_config.json`)

```json
{
  "mcpServers": {
    "aria": {
      "command": "python3",
      "args": ["/path/to/aria/tools/aria-mcp/aria_mcp.py"]
    }
  }
}
```

### Cursor (`.cursor/mcp.json`)

```json
{
  "mcpServers": {
    "aria": {
      "command": "python3",
      "args": ["/path/to/aria/tools/aria-mcp/aria_mcp.py"]
    }
  }
}
```

## Binary Discovery

The server finds `ariac` and `aria-safety` automatically:

1. **Environment variable** — `ARIAC_BIN` / `ARIA_SAFETY_BIN`
2. **Repo-relative paths** — `<repo>/build/ariac` and
   `<repo>/tools/aria-safety/aria-safety`
3. **`$PATH`**

Override if needed:

```sh
ARIAC_BIN=/custom/path/ariac python3 aria_mcp.py
```

Override the reference doc path:

```sh
ARIA_REF_MD=/custom/aria_ref.md python3 aria_mcp.py
```

## Running Manually

Start the server (reads JSON-RPC on stdin, writes on stdout):

```sh
python3 aria_mcp.py
```

Quick smoke-test:

```sh
echo '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","clientInfo":{"name":"test","version":"0"},"capabilities":{}}}' \
  | python3 aria_mcp.py
```

List tools:

```sh
printf '%s\n%s\n' \
  '{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2024-11-05","clientInfo":{"name":"test","version":"0"},"capabilities":{}}}' \
  '{"jsonrpc":"2.0","id":2,"method":"tools/list","params":{}}' \
  | python3 aria_mcp.py
```

## Transport

stdio, JSON-RPC 2.0, newline-delimited (one JSON object per line).
Compatible with any MCP client that supports the `2024-11-05` protocol version.
