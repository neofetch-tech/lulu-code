# Architecture

Lulu Code is a single native process: a Qt 6 application with llama.cpp linked
directly into it. There is no Python runtime, no local HTTP server and no
embedded browser. This document describes how the pieces fit together.

## Design principles

1. **Local by construction.** Weights, prompts, files and tool output never
   leave the machine. The only network code downloads GGUF files on request.
2. **One job per thread.** The UI never waits on inference. The engine owns the
   model and blocks only its own worker thread.
3. **Model-agnostic agent.** Tool use is a text protocol, not provider-specific
   function calling, so any capable GGUF chat model can participate.
4. **Consent before mutation.** Every write, edit, delete and command goes
   through an approval callback unless the user explicitly opted out.
5. **Small, replaceable parts.** Core, agent and UI layers do not depend on each
   other in a cycle. Each can be tested or swapped independently.

## Module map

```text
src/
├── main.cpp              QApplication, logging, theme, MainWindow
├── app/
│   ├── AppPaths          data/models/logs directories, manifest path
│   └── Logger            rotating file log + console, installed as Qt handler
├── core/
│   ├── ChatMessage       Role, ChatMessage, GenerationParams, TokenStats
│   ├── ModelCatalog      built-in GGUF catalog (id, repo, quant, size, license)
│   ├── ModelStore        Hugging Face downloads, manifest, local GGUF import
│   └── LlamaEngine       llama.cpp model/context, chat templates, sampling, KV cache
├── agent/
│   ├── ToolCall          tool structs + tolerant fenced-JSON parser
│   ├── ToolRunner        file/search/shell/git/test tools + workspace sandbox
│   ├── Toolchain         detects git, python, node, npm, pytest, Playwright
│   ├── ProjectScanner    detects language, manifests, entry points, file counts
│   └── AgentSession      conversation state machine and tool loop
└── ui/
    ├── Theme             Fusion style, palette, fonts, theme.qss
    ├── MainWindow        composition root: wiring, settings, menus, approvals
    ├── Sidebar           model, workspace, exoskeleton, settings
    ├── ChatPanel         header, scroll canvas, welcome state, composer
    ├── MessageBubble     streaming assistant/user bubbles with Markdown
    ├── ToolCard          one collapsible tool call + result
    ├── Composer          auto-growing input, send/stop
    ├── WorkspacePanel    file tree + agent activity feed
    ├── ModelsDialog      catalog, downloads, import, activate
    ├── SettingsDialog    generation, model loading, agent policy
    └── ApprovalDialog    diff/command preview with remember-for-session
```

## Threading model

| Object | Thread | Notes |
|---|---|---|
| `QApplication`, all widgets | GUI | Everything the user can see |
| `MainWindow`, `AgentSession`, `ToolRunner`, `ModelStore` | GUI | Event-driven, no blocking waits |
| `LlamaEngine` | dedicated `QThread` | `loadModel`, `unloadModel`, `generate` block only this thread |
| `QProcess` children | GUI event loop | Asynchronous: tests and builds never freeze the app |

`LlamaEngine` is moved to `engineThread_` at startup. UI code never calls its
slots directly: it uses `QMetaObject::invokeMethod(..., Qt::QueuedConnection)`
with `Q_ARG` values. Signals from the engine (`token`, `finished`, `loaded`, …)
are delivered to the GUI thread through queued connections.

The one exception is `LlamaEngine::cancel()`, which only stores an atomic flag
and is explicitly safe to call from any thread.

Engine lifetime is tied to the thread:

```cpp
engine_ = new LlamaEngine;
engine_->moveToThread(&engineThread_);
connect(&engineThread_, &QThread::finished, engine_, &QObject::deleteLater);
engineThread_.start();
```

On shutdown the window cancels generation, quits the thread and waits, so no
model memory is freed while a decode is in flight.

## A turn, step by step

```text
user presses Enter
   │
   ▼
AgentSession::send(text)
   ├─ append user message to history
   ├─ emit userMessage(text) ───────────────► ChatPanel shows the bubble
   └─ startGeneration()
        ├─ build system prompt (workspace + toolchain + tool guide + rules)
        ├─ emit assistantStarted(id) ────────► ChatPanel creates a streaming bubble
        ├─ emit generationRequested(messages, params)
        │        (queued to the engine thread)
        ▼
LlamaEngine::generate()
        ├─ apply the model's chat template
        ├─ trim history to fit the context window
        ├─ reuse the KV-cache prefix, decode only new tokens
        ├─ sample tokens → emit token(delta) ─► AgentSession::onEngineToken
        │                                          └─ emit assistantDelta ─► UI
        └─ emit finished(text, stats, cancelled)
   │
   ▼
AgentSession::onEngineFinished()
   ├─ parse ```lulu-tool blocks
   ├─ emit assistantFinished(displayText, hadToolCalls)
   ├─ no calls?  → finishTurn() → Idle
   └─ calls?     → runNextTool()
        ├─ emit toolStarted(id, name, args) ──► ChatPanel tool card
        │                                        WorkspacePanel activity row
        ├─ ToolRunner::execute(call, done)
        │     ├─ approval dialog for mutations (unless auto-approved)
        │     ├─ QFile / QProcess work
        │     └─ done(result)
        ├─ append <tool_result> message to history
        ├─ emit toolFinished(id, ok, output)
        └─ runNextTool() again, or startGeneration() when the queue is empty
```

The loop is bounded by **Max tool steps per turn** (default 14, configurable
2–40). When the budget is exhausted the agent stops and tells the user, rather
than looping forever.

## Prompt construction

`AgentSession::buildSystemPrompt()` contains, in order:

1. Identity and operating principles.
2. A workspace summary from `ProjectScanner` (language, manifests, entry
   points, notable folders).
3. The detected toolchain (`Toolchain::summary`).
4. The tool catalog generated from `ToolRunner::specs()`.
5. The exact tool-call protocol, with examples and failure rules.
6. Hard rules: inspect before editing, surgical changes, run tests, never touch
   secrets or `.git`, never commit without being asked, explain failures.
7. Optional user-supplied extra instructions from Settings.

Tool results are appended as user-role messages wrapped in
`<tool_result tool="…" ok="true|false">…</tool_result>`. Closing tags inside
model output are escaped so a file cannot forge a tool result.

## Context management

Two layers keep long sessions inside the window:

- **Message trimming (agent).** History is capped at 80 entries; tool output is
  clipped to 24k characters with an explicit truncation marker.
- **Token trimming (engine).** Before decoding, the engine formats the prompt
  and tokenizes it. If `prompt + maxTokens` exceeds the context, it drops the
  oldest non-system messages and retries. As a last resort it keeps the tail of
  the token stream and lowers `maxTokens`.

### KV-cache prefix reuse

Agent loops send almost the same prompt on every step: a fixed system prompt,
the growing history and one new tool result. Re-processing all of it is wasted
work, so the engine keeps `cachedTokens_` — the exact token sequence currently
decoded in the model's memory.

For each request it computes the longest common prefix with the new prompt:

- no common prefix → clear the memory and decode everything;
- partial prefix → `llama_memory_seq_rm(memory, 0, common, -1)`, then decode the
  suffix;
- identical prefix → step back one token so the final prompt token gets fresh
  logits.

On a typical tool loop this turns an 8k-token re-process into a few hundred
tokens per step. The generated token is appended to `cachedTokens_` as it is
decoded, so the cache stays in sync with memory.

## Tool execution and safety

`ToolRunner` resolves every path against the workspace root:

- relative paths are joined to the root and cleaned;
- `..` traversal is rejected before touching the filesystem;
- existing symlinks are canonicalized and must still be inside the root;
- missing paths are rejected for read/delete operations.

Mutating operations call the approval handler supplied by `MainWindow`. The
dialog shows the tool, the target file and a preview (full new content, the
old/new fragment, or the exact command). "Remember for this session" flips the
corresponding in-memory auto-approve flag, which is not persisted.

Commands run through `QProcess` with:

- merged stdout/stderr;
- `NO_COLOR=1`, non-interactive git pager and unbuffered Python;
- a per-call timeout (default 120 s), after which the process is killed;
- output capped at 64 KiB with a truncation marker;
- cancellation from the Stop button, which kills the active process.

`git` is restricted to a safe action list; `commit` requires a message;
`run_tests` detects the project type instead of guessing.

## UI composition

`MainWindow` is the composition root and the only class that knows about every
other layer. It owns:

- the engine thread and the three domain objects (`ModelStore`, `ToolRunner`,
  `AgentSession`);
- the splitter layout: `Sidebar | ChatPanel | WorkspacePanel`;
- menu actions, shortcuts and the status bar;
- QSettings persistence (geometry, splitter, workspace, all settings, last
  model);
- the approval handler that opens `ApprovalDialog`.

Widgets are deliberately dumb: they expose slots for state changes and signals
for user intent. `ChatPanel` never talks to the engine, and `LlamaEngine` never
touches widgets.

Styling is one QSS file (`resources/theme.qss`) with object names and dynamic
properties (`variant`, `state`, `active`, `installed`) rather than per-widget
styles. `Theme::apply` installs Fusion, the dark palette, the font choices and
the stylesheet once at startup.

## Storage layout

All writable data goes through `AppPaths`:

| Platform | Location |
|---|---|
| Windows | `%APPDATA%/Lulu/Lulu Code/` |
| Linux | `~/.local/share/Lulu/Lulu Code/` |

```text
Lulu Code/
├── models/
│   └── <model-id>/<file>.gguf
├── models.json          installed model manifest
└── logs/lulu-code.log   rotating at ~2 MB
```

Small UI and agent settings use the platform-native `QSettings` store
(registry on Windows, INI under `~/.config` on Linux).

## Adding things

**A model** — append a `ModelInfo` to `kModels` in `ModelCatalog.cpp` with a
stable id, Hugging Face repo, exact GGUF filename, size, context and license.
The Models dialog picks it up automatically. Arbitrary local GGUF files can be
imported at runtime without a catalog entry.

**A tool** — add a `ToolSpec` to `ToolRunner::specs()`, a handler method, and a
branch in `ToolRunner::handleTool()`. Mutating tools must call `approve()` and
resolve paths with `resolvePath()`. The system prompt updates itself because it
is generated from `specs()`.

**A setting** — add a field to `AppSettings`, a control in `SettingsDialog`,
defaults in `MainWindow::readSettings`, persistence in `saveSettings`, and apply
it in `applyGenerationSettings` or `applySettings`.

**A backend** — `LlamaEngine` is the only file that includes `llama.h`. Keep the
public signals (`loaded`, `token`, `finished`, `failed`, `contextUsed`) and the
rest of the app is backend-agnostic.

## Testing

`tests/` uses Qt Test. The pure pieces (tool-call parser, project scanner,
catalog, message roles, path sandbox) run without a model or a display. GUI
smoke tests run with `QT_QPA_PLATFORM=offscreen` in CI.
