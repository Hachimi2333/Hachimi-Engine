# AGENTS.md

This file describes the constraints and development conventions for the Hachimi-Engine repository.
All contributors and automated agents must follow these rules.

## General Constraints

- Language standard: C++20.
- Build system: CMake (3.28 or newer) driven by `CMakePresets.json`.
- Configurations: Debug and Release only.
- Architecture: x86_64 only.
- Target platform: Windows.
- Engine core output: static library `Hachimi-Engine.lib`.
- Editor output: console executable `Hachimi-Editor.exe`.
- Output directory: `Build/<preset>/bin/`, where `<preset>` is the configure preset name
  (for example `Build/x64-debug/bin/`).
- Build files and intermediate objects live under `Build/<preset>/`; CMake manages the
  per-target intermediate directories, so there is no separate `obj` tree to maintain.

## Build Environment and Commands

- Visual Studio 2026 Community install directory:
  `C:\Program Files\Microsoft Visual Studio\18\Community`
- The build runs Ninja directly on the MSVC toolset. CMake and Ninja ship with Visual
  Studio 2026 and are on `PATH` inside a VS 2026 Developer Command Prompt:
  - `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe`
  - `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe`
- MSVC compiler executable:
  `C:\Program Files\Microsoft Visual Studio\18\Community\VC\Tools\MSVC\14.51.36231\bin\Hostx64\x64\cl.exe`

Configure and build Debug from a VS 2026 Developer Command Prompt in the repository root:

```
cmake --preset x64-debug
cmake --build --preset x64-debug
```

Configure and build Release:

```
cmake --preset x64-release
cmake --build --preset x64-release
```

Run the headless verification target:

```
Build/x64-debug/bin/Tests.exe
```

The presets drive Ninja themselves: no Visual Studio solution and no MSBuild project is
generated at any point, and `cmake --build` is the only build command used. `x64-debug` and
`x64-release` are single-config presets, so the configuration is fixed when the build tree
is created. Opening the repository folder in Visual Studio 2026 or VS Code reads the same
`CMakePresets.json`.

Build-system notes:

- The build passes `/showIncludes` nowhere, which the root `CMakeLists.txt` arranges in two
  places: the flag is stripped from `CMAKE_DEPFILE_FLAGS_C` and `CMAKE_DEPFILE_FLAGS_CXX`, and
  `CMAKE_CXX_SCAN_FOR_MODULES` is switched `OFF`. cl.exe prints the header list of every
  translation unit when the flag is passed, and ninja can only delete those lines by matching an
  `msvc_deps_prefix` that CMake stores in `CMakeFiles/rules.ninja`; that prefix is captured in
  the code page the build tree was configured under and does not have to match the one the
  compiler writes while building, in which case the log fills with `注意: 包含文件:` lines.
  Compile rules and the C++20 module scan are the only steps the flag can enter through, so with
  both switched off no header list is printed at all. Do not add the flag back, and keep module
  scanning off while no source imports a module (re-enabling it needs a toolchain that reports in
  English, where the prefix is plain ASCII).
- Ninja therefore learns nothing about headers: editing a header does not recompile the
  sources that include it. Rebuild instead of Build after changing a header, otherwise stale
  object files stay in the build tree.
- The root `CMakeLists.txt` holds the one helper the first-party directories call, named after
  what it does: `set_common_compile_options`. Do not introduce project-prefixed helper names.
- Runtime resources are copied by the single `copy_runtime_resources` custom target that the
  executables depend on. Copying them from a POST_BUILD step of each executable makes three
  parallel copies of one directory into the shared output directory, which fails with
  "Permission denied" while the executables link at the same time.
- `x64-debug` and `x64-release` inherit the Segment Heap manifest injection from Visual
  Studio's own `SegmentHeap.cmake`; it is a tool-owned include, not part of this repository.

## Testing and Verification

- Automated agents must verify changes by configuring with CMake and building both Debug
  and Release, then running `Tests.exe` and confirming it exits with code 0.
- Test suites live in `Tests/`, one directory per subsystem (`Core/`, `Math/`, `Packaging/`,
  `Renderer/`, `Scene/`, `Serialization/`, `Utils/`), and are written with doctest (see
  `Vendor/doctest`). Shared fixtures live in `Tests/Support/`. Use `-ts=<suite>` to run one
  category, `-tc=<pattern>` for single cases and `--list-test-cases` to list them.
- Every suite must stay headless: no window and no OpenGL context. Geometry lives in the
  CPU-side `MeshData`, so constructing a `Scene` needs no context and the ECS, the scene
  serializer and the runtime are all testable here. What stays interactive is only the code
  that creates a GPU resource (`MeshLibrary::GetOrCreate`, `VertexArray::Create`,
  `Texture2D::Create`, `Shader::Create`, a `Framebuffer`). New engine code that can be
  exercised without a window should come with a suite under `Tests/`.
- A test case owns the state it touches: mount and unmount the virtual file system itself
  (see `ScopedArchiveMount`) and never call `JobSystem::Shutdown()`, which `Tests/Main.cpp`
  owns.
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
- ImGui must use the docking branch. Editor UI styling must be defined only in `Hachimi-Engine/Source/ImGui/ThemeConfig.*` and applied once through `ThemeConfig::Apply` in `ImGuiLayer::OnAttach`; do not scatter hardcoded style or color tweaks across panels.
- Engine-owned shaders must live in `Hachimi-Engine/Resources/Shaders/*.glsl` and be loaded with `Shader::CreateEngineShader`; do not embed GLSL source in `.cpp` or `.h` files.
- Mesh geometry is split in two: `MeshData` holds the CPU vertices, indices and bounds, and
  `Mesh` holds the uploaded GPU vertex array, obtained through `MeshLibrary`. Scene, physics,
  scripting and serialization code only ever touches `MeshData`; never create a GPU resource
  (a `Mesh`, `VertexArray`, `Texture2D`, `Shader` or `Framebuffer`) from those subsystems.

## Project Scope

- Audio is not implemented in the current phase.
- Scripting is implemented with Lua 5.4 as the first backend. All script-facing engine APIs must go through the language-agnostic `ScriptRuntime` abstraction; Lua-specific code stays in `Hachimi-Engine/Source/Scripting/Lua`.
- Additional scripting languages beyond Lua are reserved architecture slots and are not implemented yet.
- 3D model import is not implemented in the current phase; built-in mesh primitives are used.
- Mesh optimization (meshoptimizer) is not included in the current phase.

## Vendor Libraries

- All third-party libraries live in the single root `Vendor/` directory, one directory per
  library, using the library's official name.
- Vendor folder names may keep their official third-party naming; do not rename files or folders inside `Vendor`.
- Never modify third-party library files, with one documented exception: a library's own
  `CMakeLists.txt` may receive a small, clearly marked fix when the upstream file cannot
  work as a subproject. Existing examples are the CRT alignment in `Vendor/Box3D/CMakeLists.txt`
  and the imgui link in `Vendor/ImGuizmo/CMakeLists.txt`.
- Prefer each library's own `CMakeLists.txt`; it is added with `add_subdirectory` from the
  root `CMakeLists.txt`. Do not rewrite an upstream CMake project as a hand-written target.
- `Vendor/doctest` is the test framework and keeps its upstream CMake project unchanged, so
  it is added like every other library: with `add_subdirectory` above `Vendor/sol2`. The
  upstream `doctest_with_main` static library is switched off because `Tests/Main.cpp`
  defines `DOCTEST_CONFIG_IMPLEMENT` and owns `main()`.
- Only when a library ships no usable `CMakeLists.txt` at the directory it is added from does
  the project own a thin one, placed inside that library's own directory: `Vendor/GLAD`,
  `Vendor/Lua`, `Vendor/stb`, `Vendor/imgui` and `Vendor/zstd`.
- `Vendor/sol2` must remain the last library added with `add_subdirectory`. Its
  `CMAKE_PROJECT_INCLUDE` assignment breaks any `project()` call processed after it.
  `Vendor/imgui` must be added before `Vendor/ImGuizmo` so the `imgui` target exists.
- Never call `project()` in any directory below the root: `Vendor/sol2` sets
  `CMAKE_PROJECT_INCLUDE`, and CMake applies that variable to every subsequent `project()`
  call no matter how deep it is.
- Keep only sources required for compilation and the LICENSE files; remove examples, tests,
  docs, and other non-essential files. Other-platform sources (Linux/macOS, unused rendering
  backends) are still sources: keep them.
  A trimmed library directory holds exactly: source and header files (including the
  other-platform and unused-backend ones), the build inputs its own `CMakeLists.txt` refers to
  (`*.in` templates, `*.cmake` modules, `*.xml` protocol definitions), the debugger `.natvis`
  files a library's own `CMakeLists.txt` adds to its target (currently only
  `Vendor/Box3D/src/box3d.natvis`, which `Vendor/Box3D/src/CMakeLists.txt` passes to
  `target_sources` on MSVC), and the license or attribution files. Files that belong to
  another build system (Bazel, Meson, Conan, Make, BUCK, `VS2008`/`VS_scripts` projects), CI
  configuration, editor or formatting configuration, upstream helper scripts, documentation,
  logos, and sample or test data are removed. Check every candidate against the library's own
  `CMakeLists.txt` before deleting it.
- Third-party include paths are never spelled out by hand. Link the library's target and let
  it publish its own include directories.

## Logging

- Use spdlog with two logger categories:
  - `HE_CORE_*` for engine logs.
  - `HE_CLIENT_*` for client/editor logs.
- Log to console only in the current phase. Do not create log files.
