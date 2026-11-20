<div align="center">

<img src="resources/icons/logo.png" width="104" alt="Lulu Code logo">

# Lulu Code

**A local-first AI coding agent. Your models, your files, your machine.**

[![C++23](https://img.shields.io/badge/C%2B%2B-23-1f1f24?style=flat-square&logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/23)
[![Qt 6](https://img.shields.io/badge/Qt-6-1f1f24?style=flat-square&logo=qt&logoColor=white)](https://www.qt.io/)
[![llama.cpp](https://img.shields.io/badge/llama.cpp-b10333-1f1f24?style=flat-square)](https://github.com/ggml-org/llama.cpp)
[![Windows | Linux](https://img.shields.io/badge/platform-Windows%20%7C%20Linux-1f1f24?style=flat-square)](#build)
[![MIT](https://img.shields.io/badge/license-MIT-1f1f24?style=flat-square)](LICENSE)

[![Playwright](https://img.shields.io/badge/Playwright-4T94Q1?style=flat-square&logo=playwright&logoColor=white)](https://playwright.dev/)
[![pytest](https://img.shields.io/badge/pytest-34JENJ?style=flat-square&logo=pytest&logoColor=white)](https://pytest.org/)
[![Selenium](https://img.shields.io/badge/Selenium-4rc1?style=flat-square&logo=selenium&logoColor=white)](https://selenium.dev/)
[![Git](https://img.shields.io/badge/Git-F34F29?style=flat-square&logo=git&logoColor=white)](https://git-scm.com/)

</div>

---

Lulu Code is a native desktop coding agent that runs entirely on your own hardware.
It pairs a **C++23 + Qt 6** interface with **llama.cpp** inference, so the model,
the conversation and every file it touches stay on your machine.

Point it at a project folder and it can read the codebase, search it, create and
edit files, run your tests, drive Playwright or Selenium browser suites, and
inspect changes with Git — asking for approval before anything is modified.

> No cloud account. No API keys. No telemetry. The only network request Lulu
> Code makes is the one you start yourself when downloading a model.

## Highlights

- **Native, not a web app.** One C++ binary with a Qt Widgets interface and a
  dark monochrome theme. No Electron, no React, no bundled browser.
- **Real local inference.** llama.cpp is linked directly into the app; models
  stream token by token from a worker thread and never block the UI.
- **Agent with hands.** The model gets a documented tool protocol for listing,
  reading, searching, writing and editing files inside the workspace.
- **Built-in exoskeleton.** `run_tests` detects and runs pytest, Playwright,
  Selenium (through pytest), npm, CTest, Cargo and Go suites. `git` and
  `run_command` cover the rest.
- **Approval-first by default.** Writes, edits, deletes and shell commands show
  a diff or command preview before they run. Auto-approve is one keystroke away.
- **Model manager.** Download GGUF builds of Qwen, Llama, GLM and DeepSeek from
  Hugging Face, or import any `.gguf` you already have.
- **Fast agent loops.** The engine reuses the KV-cache prefix between tool
  steps, so continuing a long transcript does not re-process the whole prompt.
- **Workspace aware.** Live file tree, agent activity feed, per-project context
  summary and toolchain detection in the sidebar.

## How it works

```
┌─────────────────────────────── Lulu Code ───────────────────────────────┐
│                                                                         │
│  Qt 6 UI (main thread)                    LlamaEngine (worker thread)   │
│  ┌──────────┐  ┌──────────────────┐       ┌──────────────────────────┐  │
│  │ Sidebar  │  │ Chat + Workspace │       │ llama.cpp model/context  │  │
│  └──────────┘  └────────┬─────────┘       └────────────▲─────────────┘  │
│                         │                              │ tokens         │
│                    ┌────▼─────────┐   ChatMessage   ┌──┴───────────┐    │
│                    │ AgentSession ├────────────────►│  LlamaEngine │    │
│                    │  tool loop   │◄────────────────┤  (queued)    │    │
│                    └────┬─────────┘   finished      └──────────────┘    │
│                         │ ToolCall                                      │
│                    ┌────▼─────────┐   ApprovalDialog                    │
│                    │  ToolRunner  ├──────────────► user                 │
│                    └────┬─────────┘                                     │
│                         │ QProcess / QFile                              │
│                    ┌────▼─────────────────────────┐                     │
│                    │ workspace: files, git, tests │                     │
│                    └──────────────────────────────┘                     │
└─────────────────────────────────────────────────────────────────────────┘
```

The agent does not rely on provider-specific function calling. It teaches the
model a tiny fenced protocol that every GGUF chat model can produce:

````text
```lulu-tool
{"tool": "edit_file", "args": {"path": "src/app.cpp", "old_text": "…", "new_text": "…"}}
```
````

Results come back as `<tool_result>` messages and the loop continues until the
model answers without a tool block. See [docs/TOOLS.md](docs/TOOLS.md) for the
full protocol and tool reference.

## Models

Lulu Code speaks GGUF through llama.cpp, so any architecture llama.cpp supports
will load. The built-in catalog is a curated starting point:

| Model | Family | Quant | Size | Context | License | Notes |
|---|---|---|---:|---:|---|---|
| **Qwen2.5 Coder 1.5B** | Qwen | Q4_K_M | ~1.0 GB | 32k | Apache-2.0 | Tiny and instant; great on laptops |
| **Qwen2.5 Coder 7B** ★ | Qwen | Q4_K_M | ~4.7 GB | 32k | Apache-2.0 | Recommended daily driver |
| **Llama 3.1 8B Instruct** | Llama | Q4_K_M | ~4.9 GB | 128k | Llama 3.1 | Strong general writing |
| **GLM-4 9B** | GLM | Q4_K_M | ~5.5 GB | 128k | MIT | Bilingual reasoning |
| **Qwen2.5 Coder 14B** | Qwen | Q4_K_M | ~9.0 GB | 32k | Apache-2.0 | Better refactors |
| **DeepSeek Coder V2 Lite** | DeepSeek | Q4_K_M | ~10.4 GB | 160k | DeepSeek | 16B MoE, ~2.4B active |
| **Qwen2.5 Coder 32B** | Qwen | Q4_K_M | ~19.9 GB | 32k | Apache-2.0 | Best local coder here |

★ recommended default. Sizes are approximate; check the Models dialog for the
exact download before starting. You can also add a local `.gguf` file without
copying it into Lulu's data folder.

## Tools

| Tool | Purpose |
|---|---|
| `list_files` | Walk the workspace tree (ignores `.git`, `node_modules`, `build`, …) |
| `read_file` | Read a text file with line numbers, optionally a line range |
| `write_file` | Create or replace a file, creating parent folders |
| `edit_file` | Replace an exact fragment; fails if it is ambiguous |
| `search_files` | Grep-style text or regex search across the project |
| `delete_file` | Remove one file |
| `run_command` | Run a shell command in the workspace with a timeout |
| `run_tests` | Detect and run pytest / Playwright / Selenium / npm / CTest / Cargo / Go |
| `git` | `status`, `diff`, `log`, `show`, `add`, `commit`, `restore`, `stash`, … |
| `detect_toolchain` | Report which developer tools are installed |

Every path is resolved inside the workspace root; symlinks that escape it are
rejected. `.env`, keys and other secrets should be added to `.gitignore` — the
agent is instructed to leave them alone, and approvals remain the hard barrier.

## Build

### Requirements

- CMake **3.24+**
- A **C++23** compiler: MSVC 2022, GCC 13+, or Clang 16+
- **Qt 6.5+** with `Core`, `Gui`, `Widgets`, `Network` and `Svg`
- Git (llama.cpp is fetched at configure time unless you provide a checkout)

llama.cpp is pinned to tag **b10333** and built statically by CMake.

### Linux

```bash
# Arch
sudo pacman -S --needed base-devel cmake git qt6-base qt6-svg

# Debian / Ubuntu
sudo apt install build-essential cmake git qt6-base-dev qt6-svg-dev

git clone https://github.com/neofetch-tech/lulu-code.git
cd Lulu
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
./build/lulu_code
```

### Windows

Install Qt 6 with the official online installer (MSVC 2022 64-bit kit), then
from a Developer PowerShell:

```powershell
git clone https://github.com/neofetch-tech/lulu-code.git
cd Lulu

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release `
      -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2022_64"

cmake --build build --config Release -j

# optional: collect Qt DLLs next to the executable
C:\Qt\6.8.0\msvc2022_64\bin\windeployqt.exe build\Release\lulu_code.exe
.\build\Release\lulu_code.exe
```

A convenience script is available at `scripts/build-windows.ps1` (MSVC or
MinGW) and `scripts/build-linux.sh`.

### Downloadable builds

Push to `main`/`master` or start **Build distributables** from the Actions tab to build both packages. Download the Windows installer (`lulu-code-setup.exe`) or the portable Linux bundle (`lulu-code-linux-x86_64.tar.gz`) from the workflow run artifacts. The Linux archive includes `AppRun`; extract it and launch `./AppRun`.
### Build options

| CMake option | Default | Meaning |
|---|---|---|
| `LULU_WITH_LLAMA` | `ON` | Link llama.cpp. `OFF` builds the UI only for fast iteration |
| `LULU_FETCH_LLAMA` | `ON` | Fetch llama.cpp when no local source is given |
| `LULU_LLAMA_SOURCE_DIR` | empty | Use an existing llama.cpp checkout |
| `LULU_LLAMA_TAG` | `b10333` | Tag/branch to fetch |
| `LULU_BUILD_TESTS` | `OFF` | Build the Qt Test unit tests |

GPU offload is inherited from llama.cpp. Configure with `-DGGML_CUDA=ON`,
`-DGGML_VULKAN=ON` or `-DGGML_METAL=ON` (macOS) and set **GPU layers** in
Settings; `0` means pure CPU.

### Tests

```bash
cmake -S . -B build -DLULU_BUILD_TESTS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Using Lulu Code

1. Open **Models** and download a model (Qwen2.5 Coder 7B is a good start).
2. Click **Open folder** and pick the project you want to work on.
3. Ask for a change, for example:
   *“Add input validation to the signup form and run the tests.”*
4. Approve or reject each write, edit and command in the approval dialog.
5. Review the result in the chat, the activity feed and your own `git diff`.

### Shortcuts

| Shortcut | Action |
|---|---|
| `Ctrl+N` | New chat |
| `Ctrl+O` | Open project folder |
| `Ctrl+M` | Manage models |
| `Ctrl+,` | Settings |
| `Ctrl+B` | Toggle the explorer panel |
| `F5` | Reload the current model |
| `Enter` / `Shift+Enter` | Send / new line in the composer |
| `Esc` | Stop the current run |

### Settings

Generation (temperature, top-p, max tokens, repeat penalty), model loading
(context size, GPU layers, CPU threads), agent behaviour (max tool steps,
approval policy, extra system prompt) and model storage are all configurable
in-app. Settings live in the platform's native location via `QSettings`;
downloaded models and logs live under the app data directory
(`Help → Open data folder`).

## Project structure

```text
Lulu/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── app/          AppPaths, Logger
│   ├── core/         ChatMessage, ModelCatalog, ModelStore, LlamaEngine
│   ├── agent/        AgentSession, ToolRunner, ToolCall, Toolchain, ProjectScanner
│   └── ui/           MainWindow, Sidebar, ChatPanel, bubbles, dialogs, theme
├── resources/        theme.qss, logo, Windows icon
├── docs/             ARCHITECTURE.md, TOOLS.md
├── scripts/          build-windows.ps1, build-linux.sh
├── tests/            Qt Test suites
├── legacy/           original Python + React implementation (preserved)
└── .github/          CI build workflow
```

The original Python/pybind11/React app is kept in [`legacy/`](legacy) for
reference; the shipping application is the native C++ tree above.

## Roadmap

- [x] Native Qt 6 shell with the dark monochrome theme
- [x] llama.cpp inference, streaming and KV-cache reuse
- [x] Model catalog, downloads, local GGUF import
- [x] File, search, shell, git and test tools with approvals
- [x] Project scanner and toolchain detection
- [x] Windows and Linux builds
- [ ] Diff view before writes
- [ ] Multi-file patch application and undo
- [ ] Conversation persistence and project sessions
- [ ] LSP diagnostics fed back into the agent loop
- [ ] Bundled GPU builds (CUDA / Vulkan) in releases

## Contributing

Issues and pull requests are welcome. Keep changes focused, match the existing
style, and run the test suite before opening a PR. For anything larger than a
bug fix, open an issue first so the design can be discussed.

## Acknowledgements

Lulu Code stands on the shoulders of [llama.cpp](https://github.com/ggml-org/llama.cpp),
[ggml](https://github.com/ggml-org/ggml), [Qt](https://www.qt.io/) and the model
authors at Qwen, Meta, Zhipu AI and DeepSeek. GGUF model files are downloaded
from the Hugging Face community; each model keeps its own license.

### Project Logos

![AI Model 1](resources/icons/ai-model-1.png width="64" alt="AI Model 1 placeholder")
![AI Model 2](resources/icons/ai-model-2.png width="64" alt="AI Model 2 placeholder")
![AI Model 3](resources/icons/ai-model-3.png width="64" alt="AI Model 3 placeholder")
![AI Model 4](resources/icons/ai-model-4.png width="64" alt="AI Model 4 placeholder")
![AI Model 5](resources/icons/ai-model-5.png width="64" alt="AI Model 5 placeholder")
![AI Model 6](resources/icons/ai-model-6.png width="64" alt="AI Model 6 placeholder")
![AI Model 7](resources/icons/ai-model-7.png width="64" alt="AI Model 7 placeholder")
![AI Model 8](resources/icons/ai-model-8.png width="64" alt="AI Model 8 placeholder")

### Developer Tools Logos

![Playwright](resources/icons/playwright.png width="32" alt="Playwright logo")
![pytest](resources/icons/pytest.png width="32" alt="pytest logo")
![Selenium](resources/icons/selenium.png width="32" alt="Selenium logo")
![Git](resources/icons/git.png width="32" alt="Git logo")

## License

[MIT](LICENSE) © 2026 Liquidarium
