# Tool Protocol Reference

Lulu Code uses a model-agnostic tool protocol for all agent interactions. The model replies with fenced ```lulu-tool blocks that the agent parses and executes. This design works with any GGUF model that supports structured text output.

## Protocol Format

Every tool block must be enclosed in triple-backticks with the `lulu-tool` marker:

```lulu-tool
{"tool": "tool_name", "args": {"key": "value"}}
```

- **One JSON object per block.** Multiple blocks in one reply are executed in order.
- **No prose mixed with tool blocks** in the same reply.
- **Tool results** come back as `<tool_result>` messages.

## Built-in Tools

| Tool | Signature | Description |
|---|---|---|
| `list_files` | `path=".", depth=2, max_entries=400` | List files and folders in the workspace (ignores `.git`, `node_modules`, `build`, …) |
| `read_file` | `path, start_line?, end_line?` | Read a UTF-8 text file with line numbers. Prefer reading with a line range for big files. |
| `write_file` | `path, content` | Create a file or replace its whole contents. Creates parent folders. |
| `edit_file` | `path, old_text, new_text, replace_all=false` | Replace an exact text fragment in a file. `old_text` must match exactly once unless `replace_all=true`. |
| `search_files` | `query, path=".", glob="*", regex=false, max_results=60` | Search text across workspace files. Returns `file:line: text` matches. |
| `delete_file` | `path` | Delete a single file from the workspace. |
| `run_command` | `command, cwd=".", timeout_seconds=120` | Run a shell command in the workspace and return its combined output and exit code. |
| `run_tests` | `framework="auto"` | Detect and run the project's tests (pytest, Playwright, npm, ctest, cargo, go). |
| `git` | `action, args=[], message=""` | Run a git command: `status`, `diff`, `log`, `show`, `add`, `commit`, `restore`, `stash`, `branch`, `checkout`, `switch`, `ls-files`. |
| `detect_toolchain` | *(no args)* | Report which developer tools are available (git, python, node, npm, pytest, Playwright). |

## Path Safety

- All paths are resolved relative to the **workspace root** that was opened when the agent started.
- Relative paths are joined to the workspace root and cleaned (`..` traversal is rejected).
- Existing symlinks are canonicalized and must still be inside the root.
- `.env`, keys and other secrets should be added to `.gitignore` — the agent is instructed to leave them alone, and approvals remain the hard barrier.

## Approvals

Mutating operations (`write_file`, `edit_file`, `delete_file`, `run_command`) go through an **approval dialog** before execution unless the user has enabled auto-approve in Settings. The dialog shows:

- The tool name and a human-readable title.
- The target path and a preview of what will happen.
- A "Remember for this session" checkbox that toggles the in-memory auto-approve flag.

## Examples

### List files in the current folder

```lulu-tool
{"tool": "list_files", "args": {"path": ".", "depth": 1, "max_entries": 50}}
```

### Read the first 20 lines of a file

```lulu-tool
{"tool": "read_file", "args": {"path": "src/main.cpp", "start_line": 1, "end_line": 20}}
```

### Edit a function signature

```lulu-tool
{"tool": "edit_file", "args": {"path": "src/app.cpp",
    "old_text": "void foo(int x)\n{",
    "new_text": "void foo(int x, int y)\n{",
    "replace_all": false}}
```

### Run a quick test

```lulu-tool
{"tool": "run_tests", "args": {"framework": "auto"}}
```

### Run a specific test suite

```lulu-tool
{"tool": "run_tests", "args": {"framework": "pytest"}}
```

### Git status

```lulu-tool
{"tool": "git", "args": {"action": "status"}}
```

### Git diff

```lulu-tool
{"tool": "git", "args": {"action": "diff"}}
```

### Commit a change

```lulu-tool
{"tool": "git", "args": {"action": "commit", "message": "Fix nil pointer in renderer"}}
```

## Result Format

After each tool execution, the agent receives a `<tool_result>` message:

```text
<tool_result tool="edit_file" ok="true">Edited src/app.cpp (1 replacement).</tool_result>
<tool_result tool="run_command" ok="false" error="Command timed out.">…</tool_result>
<tool_result tool="list_files" ok="true">Workspace: /home/user/project\nsrc/\n├─ main.cpp\n└─ utils.cpp</tool_result>
```

The `ok` field is `true` when the operation succeeded, `false` when it failed or was rejected.

## Protocol Help (shown in the system prompt)

```
HOW TO CALL TOOLS
When you need a tool, reply with one or more fenced blocks in exactly this format:
```lulu-tool
{"tool": "read_file", "args": {"path": "src/main.cpp"}}
```
Rules for tool calls:
- The JSON object must contain "tool" and "args".
- One JSON object per block. Several blocks in one reply are executed in order.
- Do not mix prose and tool blocks in the same reply.
- Tool results come back as <tool_result> messages. Read them, then continue.
- When the task is done, answer the user in plain text with no tool blocks.
- Never guess a file's contents: read it first. Never repeat a call that just failed with the same arguments.
```

---

See also: [ARCHITECTURE.md](ARCHITECTURE.md) for the full design overview.