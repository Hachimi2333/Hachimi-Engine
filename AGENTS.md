# AGENTS.md

This file describes the constraints and development conventions for the Hachimi-Engine repository.
All contributors and automated agents must follow these rules.

## General Constraints

- Language standard: C++20.
- Build system: Premake5, generating Visual Studio 2026 solutions only.
- Configurations: Debug and Release only.
- Architecture: x86_64 only.
- Target platform: Windows.
- Engine core output: static library `Hachimi-Engine.lib`.
- Editor output: console executable `Hachimi-Editor.exe`.
- Output directory: `Bin/<configuration>-<system>-<architecture>`.
- Intermediate directory: `Bin/Obj/<configuration>-<system>-<architecture>/<ProjectName>`.
- Vendor projects must follow the same output/intermediate directory convention.

## Build Environment and Commands

- Visual Studio 2026 Community install directory:
  `C:\Program Files\Microsoft Visual Studio\18\Community`
- MSBuild x64 executable:
  `C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe`
- MSVC compiler executable:
  `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`

Generate the solution from the repository root:

```
cmd /c GenerateSolution.bat
```

or run Premake directly:

```
Vendor\Premake\Bin\premake5.exe vs2026 --file=premake5.lua
```

Build Debug:

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Hachimi-Engine.slnx -p:Configuration=Debug -p:Platform=x64 -v:minimal -nologo
```

Build Release:

```
"C:\Program Files\Microsoft Visual Studio\18\Community\MSBuild\Current\Bin\amd64\MSBuild.exe" Hachimi-Engine.slnx -p:Configuration=Release -p:Platform=x64 -v:minimal -nologo
```

When using the VS 2026 Developer Command Prompt, `msbuild Hachimi-Engine.slnx -p:Configuration=Debug -p:Platform=x64` is equivalent.

## Testing and Verification

- Automated agents must verify changes by generating the solution and building Debug and Release.
- Automated agents must not perform complex GUI tests such as image recognition, screenshot analysis, or pixel-based clicking.
- Leave interactive editor behavior verification to the user; user testing is faster and more reliable.

## Code Style

- Namespace: `HachimiEngine`, alias `HE`. Use `HE::` in implementation code.
- Types, classes, functions, methods, and files: PascalCase.
- Local variables and function parameters: camelCase.
- Non-public member variables: `m_` prefix.
- Static variables: `s_` prefix.
- Smart pointers: `Scope` for unique ownership, `Ref` for shared ownership.

## Comments and Commits

1. Comments must be written in English. Code should contain brief, useful comments.
2. Commit messages must be written in Chinese and follow Conventional Commits.
3. Never create a single oversized "god file" that piles many unrelated features together.
   There is no hard line limit; do not split files merely to reduce line count.
4. Fix bugs with the best root-cause solution, never with a temporary workaround that hides the symptom.

## Rendering and Editor

- Only OpenGL 4.6 Core is implemented in the current phase.
- The renderer abstraction uses a simple OpenGL-like API style, not a Vulkan-style complex abstraction.
- ImGui must use the docking branch and default styling; no visual beautification in the current phase.

## Project Scope

- Audio is not implemented in the current phase.
- Scripting is not implemented in the current phase.
- 3D model import is not implemented in the current phase; built-in mesh primitives are used.
- Mesh optimization (meshoptimizer) is not included in the current phase.

## Vendor Libraries

- Vendor folder names may keep their official third-party naming; do not rename files or folders inside Vendor.
- Never modify third-party library files.
- Non-header-only vendor libraries must have their own `premake5.lua` project file.
- Keep only sources required for compilation and the LICENSE files; remove examples, tests, docs, and other non-essential files.

## Logging

- Use spdlog with two logger categories:
  - `HE_CORE_*` for engine logs.
  - `HE_CLIENT_*` for client/editor logs.
- Log to console only in the current phase. Do not create log files.
