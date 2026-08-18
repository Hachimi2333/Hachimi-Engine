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
- Inter font and DPI-aware UI scaling
- Debug indicator overlays for selected camera / light entities (frustum, light range and direction)
- ImGuizmo transform gizmos: Translate / Rotate / Scale
- ImGuiFileDialog file dialogs
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
Bin/Release-windows-x86_64/Hachimi-Editor.exe
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
  Source/Scripting/      # Language-agnostic scripting core + Lua backend
  Vendor/                # Third-party libraries used by the engine
  Vendor/Lua/            # Lua 5.4 runtime
  Vendor/sol2/           # C++ Lua bindings (header-only)
Hachimi-Editor/          # Editor client (executable)
  Source/                # Editor source
  Vendor/                # Third-party libraries used by the editor
Vendor/Premake/          # Premake5 toolchain
Bin/                     # Build output (gitignored)
Vendor/Downloads/        # Third-party library download staging area (gitignored)
Utils/                   # Ad-hoc debugging tools (FramebufferTest, UIAutomation)
```

## Current Scope

The following have reserved architecture slots but are not yet implemented:

- Audio system
- Additional scripting languages beyond Lua (the backend abstraction is in place)
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
- [Dear ImGui](https://github.com/ocornut/imgui) — editor user interface
- [ImGuiFileDialog](https://github.com/aiekick/ImGuiFileDialog) — file dialogs
- [ImGuizmo](https://github.com/CedricGuillemet/ImGuizmo) — transform gizmos
- [spdlog](https://github.com/gabime/spdlog) — logging library
- [stb](https://github.com/nothings/stb) — single-header image library
- [yaml-cpp](https://github.com/jbeder/yaml-cpp) — YAML serialization
- [Inter](https://github.com/rsms/inter) — editor font, licensed under the SIL Open Font License 1.1
- [Premake5](https://github.com/premake/premake-core) — build system generator

The license of each third-party library can be found in its own `LICENSE` file under the corresponding `Vendor` directory.
