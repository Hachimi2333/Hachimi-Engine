# Hachimi-Engine

> **English** | [简体中文](README.zh-CN.md)

A C++20 3D game engine and editor for Windows, built on OpenGL 4.6 Core and inspired by the architecture of Hazel.

## Features

### Core

- Windows x86_64 only, Debug / Release configurations
- Premake5 generates Visual Studio 2026 solutions
- Application / Entry Point / Layer / LayerStack / Event system
- Custom math library `HachimiEngine::Math` (internally wraps GLM; game code does not depend on GLM directly)
- Scene / ECS based on EnTT
- yaml-cpp scene (`.hscene`) and project (`.hproj`) serialization
- Console logging with dual loggers (engine and client)
- Lua 5.4 scripting with a language-agnostic backend abstraction, ready for future script languages

### Rendering

- OpenGL 4.6 Core backend with an OpenGL-style rendering abstraction: VertexArray, VertexBuffer, IndexBuffer, Shader, Texture2D, TextureCube, Framebuffer
- Built-in meshes: Cube, Sphere, Plane, Grid
- HDR rendering pipeline: ACES tone mapping + gamma post-processing
- Cook-Torrance PBR direct lighting with directional light shadow mapping (3×3 PCF)
- Procedural skybox and image-based lighting from environment maps (irradiance + prefiltered specular)

### Physics

- Box3D integration: Static / Kinematic / Dynamic rigid bodies, Box / Sphere / Capsule / Plane colliders
- Fixed-timestep simulation with Transform sync in Play mode

### Editor

- ImGui Docking-based editor: Project Hub, Viewport, Scene Hierarchy, Inspector, Content Browser, Console
- Game export pipeline: Build Settings popup, Windows_x64 builds, packaged `Data.hpak` assets, and a standalone `Hachimi-Player` runtime
- Large-icon Content Browser grid with texture thumbnails and drag-and-drop to Inspector asset fields
- Native File Dialog Extended system file dialogs
- Inter font and DPI-aware UI scaling
- Debug indicator overlays for selected camera / light entities (frustum, light range and direction)
- ImGuizmo transform gizmos: Translate / Rotate / Scale
- Editor camera: RMB orbit, MMB pan, wheel zoom, WASD fly

## Scripting

Scripts are Lua 5.4 files under `Assets/Scripts`, attached to entities through the Inspector's `Script` component. The path is stored relative to `Assets/Scripts`, so `Player/Controller.lua` works for nested folders.

A script returns a module table with optional lifecycle callbacks:

```lua
local MyScript = {}

function MyScript:OnCreate() end
function MyScript:OnUpdate(deltaTime) end
function MyScript:OnDestroy() end

return MyScript
```

Inside a callback, `self.entity` is the owning entity and the global `HE` table exposes the sandboxed engine API:

- `HE.Log.Info / Warn / Error`
- `HE.Time.DeltaTime / ElapsedTime`
- `HE.Input.IsKeyDown / IsMouseButtonDown / GetMousePosition`
- `HE.Key.*`, `HE.Mouse.*`
- `HE.Math.Vec3` plus `Length / Normalize / Dot / Cross / Clamp / Lerp / Radians / Degrees`
- `HE.Scene.FindEntityByName`
- Entity methods: `GetName / SetName / GetPosition / SetPosition / GetRotation / SetRotation / GetScale / SetScale / Translate / Rotate / GetWorldPosition`

Scripts run only in Play mode. Each Play session creates an isolated Lua VM, and script errors are reported to the Console without crashing the editor.

## Requirements

- Windows
- Visual Studio 2026 (Community or later)
- A driver with OpenGL 4.6 Core support

The repository already includes Premake5 and all third-party library sources; no additional setup is required.

## Getting Started

### Generate the Solution

Double-click `GenerateSolution.bat` at the repository root, or run manually:

```
Vendor\Premake\Bin\premake5.exe vs2026 --file=premake5.lua
```

### Build

Open the generated `Hachimi-Engine.slnx` in Visual Studio 2026 and build the solution.

Output directories:

- Final output: `Bin/<configuration>-<system>-<architecture>/`
- Intermediate output: `Bin/Obj/<configuration>-<system>-<architecture>/<ProjectName>/`

For example:

```
Bin/Debug-windows-x86_64/Hachimi-Editor.exe
Bin/Debug-windows-x86_64/Hachimi-Player.exe
Bin/Release-windows-x86_64/Hachimi-Editor.exe
Bin/Release-windows-x86_64/Hachimi-Player.exe
```

### Run

Launch `Hachimi-Editor.exe`. The Project Hub appears:

- Create a new project (default location: `%USERPROFILE%\Documents\HachimiProjects`)
- Open a recent project
- Open any `.hproj` project file

A new project is generated with:

```
<ProjectName>/
├── <ProjectName>.hproj
└── Assets/
    ├── Meshes/
    ├── Textures/
    ├── Materials/
    ├── Scripts/
    │   └── Rotator.lua
    └── Scenes/
        └── Default.hscene
```

The default `Default.hscene` showcases rendering and physics features: PBR metal/roughness material balls and cubes, ground plane, directional light shadows (including inter-object shadows on the platform), two point lights, a parented object hierarchy, skybox and IBL. In Play mode, the scene's spheres, cubes and clustered child objects fall, collide and settle under Box3D physics simulation, while the `Scripted Spinner` entity rotates from the bundled `Rotator.lua` script. The Game panel uses the main camera view.

### Exporting a Game

Open `Build → Build Settings...` in the editor menu bar. The popup stores settings per target platform; only Windows is implemented in the current phase. Windows settings include:

- Product Name (used for the exported executable name and window title)
- Start Scene (`.hscene` files under `Assets/Scenes`)
- Window width / height and VSync

Click `Export` to create the build:

```
<ProjectName>/
└── Build/
    └── Windows_x64/
        ├── <ProductName>.exe    # standalone Hachimi-Player runtime
        └── Data.hpak            # packaged project file, Assets, shaders and fonts
```

The exported game runs the start scene immediately with physics and Lua scripting. The Player mounts `Data.hpak` in place and reads every asset straight out of it: nothing is ever extracted, so no cache directory is created, the game runs from read-only media, and only the assets actually loaded are decompressed.

The Player also exposes headless modes, which makes an exported build verifiable without a GPU:

```
<ProductName>.exe [<package>] [--content=<dir>] [--verify] [--list] [--stats]
```

| Option | Effect |
| --- | --- |
| `<package>` | Package to run. Defaults to `Data.hpak` next to the executable |
| `--content=<dir>` | Mount a directory of loose files over the package, for iterating on content without re-exporting |
| `--verify` | Check every entry against its content hash, then exit with 0 or 1 |
| `--list` | Print the package table of contents (path, raw size, packed size, method, blocks) |
| `--stats` | Print package header information |

### Game Package Format

`Data.hpak` is a custom container built on [Zstandard](https://github.com/facebook/zstd), not a ZIP:

- A header with magic, format version, package id and dictionary information, plus a footer that mirrors the table of contents location so the directory is reachable from either end.
- A table of contents with per-entry path hash, offsets, sizes, a content hash and a per-entry block table.
- Each entry is split into independently decompressible blocks, which is what enables partial and streaming reads.
- An optional shared zstd dictionary is trained from the package contents, a large win for projects made of many small text assets.
- Already-compressed payloads (PNG, TTF, ...) are stored rather than deflated again.
- Packages are memory mapped read-only, so stored entries are served with zero copies.
- The writing phase compresses entries in parallel across the engine job system, and identical inputs produce byte-identical packages.

### Viewport Controls

| Action | Input |
| --- | --- |
| Orbit | Right mouse button drag (or Alt + LMB drag) |
| Pan | Middle mouse button drag |
| Zoom | Mouse wheel (or Alt + RMB drag up/down) |
| Move | RMB held + W / A / S / D / Q / E |
| Fast move | Hold Left Shift |
| Translate gizmo | Translate button |
| Rotate gizmo | Rotate button |
| Scale gizmo | Scale button |

## Repository Layout

```text
Hachimi-Engine/          # Engine core (static library)
  Resources/Shaders/     # Engine-owned GLSL shaders
  Source/                # Engine source
  Source/Packaging/      # Game build settings, .hpak format, reader/writer
  Source/Scripting/      # Language-agnostic scripting core + Lua backend
  Vendor/                # Third-party libraries used by the engine
  Vendor/Lua/            # Lua 5.4 runtime
  Vendor/sol2/           # C++ Lua bindings (header-only)
  Vendor/zstd/           # Zstandard: package compression and xxHash
Hachimi-Editor/          # Editor client (executable)
  Source/                # Editor source
  Vendor/                # Third-party libraries used by the editor
Hachimi-Player/          # Standalone game runtime used by exported builds
  Source/                # Player source
Hachimi-Tests/           # Headless verification target (package round trip, VFS)
  Source/                # Test source
Vendor/Premake/          # Premake5 toolchain
Bin/                     # Build output (gitignored)
Vendor/Downloads/        # Third-party library download staging area (gitignored)
Utils/                   # Ad-hoc debugging tools (FramebufferTest, UIAutomation)
```

Runtime asset access goes through `HachimiEngine::VirtualFileSystem`, a read-only
mount table. Paths below a mount point are served from the mounted package (or
loose-content overlay) and everything else falls back to the operating system, so
the editor needs no special casing and packaged games get the package
transparently.

## Current Scope

The following have reserved architecture slots but are not yet implemented:

- Audio system
- Additional scripting languages beyond Lua (the backend abstraction is in place)
- Export targets beyond Windows (the Build Settings platform layout is reserved; Windows_x64 is implemented)
- Rendering backends other than OpenGL 4.6 Core
- External 3D model import (built-in meshes are used)
- Log file output (console only)
- Physics joints / character mover / mesh / heightfield colliders / physics debug draw / Box3D multithreading (basic rigid bodies and convex colliders are used)

See `FUTURE.md` for the planned rendering effects and engine systems.

## Development

Read `AGENTS.md` at the repository root for the complete development constraints and conventions before contributing.

## License

Hachimi-Engine is licensed under the [MIT License](LICENSE). Copyright (c) 2026 Hachimi2333.

## Acknowledgments

Hachimi-Engine builds upon the following open-source projects. Special thanks to their authors and contributors:

- [Hazel](https://github.com/TheCherno/Hazel) — architectural reference for the engine design
- [Box3D](https://github.com/erincatto/box3d) — physics engine
- [EnTT](https://github.com/skypjack/entt) — ECS framework
- [GLAD](https://github.com/Dav1dde/glad) — OpenGL function loader
- [GLFW](https://github.com/glfw/glfw) — window and input handling
- [GLM](https://github.com/g-truc/glm) — math library (wrapped internally by `HachimiEngine::Math`)
- [Lua](https://www.lua.org/) — Lua 5.4 scripting language runtime
- [sol2](https://github.com/ThePhD/sol2) — modern C++ Lua bindings
- [zstd](https://github.com/facebook/zstd) — Zstandard compression for the game package container, plus its bundled xxHash for path and content hashing
- [Dear ImGui](https://github.com/ocornut/imgui) — editor user interface
- [Native File Dialog Extended](https://github.com/btzy/nativefiledialog-extended) — system native file dialogs
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) — transform gizmos
- [spdlog](https://github.com/gabime/spdlog) — logging library
- [stb](https://github.com/nothings/stb) — single-header image library
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) — YAML serialization
- [Inter](https://github.com/rsms/inter) — editor font, licensed under the SIL Open Font License 1.1
- [Premake5](https://github.com/premake/premake-core) — build system generator

The license of each third-party library can be found in its own `LICENSE` file under the corresponding `Vendor` directory.
