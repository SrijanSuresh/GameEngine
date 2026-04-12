# Nova Game Engine

A custom C++ game engine built from scratch with a focus on real-time generative tech-art — procedural and AI-driven materials that react to gameplay, compiled live from GLSL shader sources at runtime.

![C++](https://img.shields.io/badge/C++-20-blue?logo=cplusplus) ![OpenGL](https://img.shields.io/badge/OpenGL-3.3-green) ![CMake](https://img.shields.io/badge/CMake-3.20+-red) ![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Windows-lightgrey)

---

## What is Nova?

Most engines treat materials as static assets. Nova treats them as live programs.

The core idea: type a prompt like `"swirling purple galaxy that pulses with player health"` and get back a real-time GLSL shader — procedurally generated, gameplay-reactive, running at full GPU speed. No baked textures. No offline processing. Just math on the GPU, compiled and hot-swapped at runtime.

Nova is designed with two users in mind:
- **Beginners and kids** — drag objects, connect visual nodes, type a material prompt, hit play.
- **Pros and technical artists** — full access to generated GLSL, hot-reload, raw C++/CUDA when you need it.

---

## Current State

### Architecture
- **C++20** codebase with clean separation between core systems
- **CMake 3.20+** with FetchContent — no manual dependency management, clean repo
- **Modular design** — renderer, ECS, editor, and generative pipeline are independent layers

### Renderer
- OpenGL 3.3 Core Profile forward renderer
- `Shader` class with runtime GLSL compilation (`glShaderSource` → `glCompileShader` → `glLinkProgram`)
- Full shader error logging via `glGetShaderInfoLog` / `glGetProgramInfoLog`
- GLM for math (vec2/3/4, mat4, transforms)
- Uniform system: `SetFloat`, `SetVec3`, `SetMat4`, etc.

### Framebuffer Pipeline
- FBO-based offscreen rendering — scene renders into a texture, not directly to screen
- Dynamic resize: viewport panel drives FBO dimensions at runtime
- Color texture + depth/stencil renderbuffer
- ImGui `Image()` display with corrected UV flip (OpenGL bottom-left origin)

### Editor (ImGui)
- Full dockspace layout with freely dockable panels
- **Viewport** — live scene render via FBO texture
- **Hierarchy** — scene entity list (ECS hookup in progress)
- **Inspector** — component properties, live uniform display
- **Console** — engine log output
- Menu bar (File, Edit)

### First Generative Shader
- `u_Time` uniform drives a `sin()`-based pulse on the triangle material
- This is the seed of the full generative pipeline — every material system feature builds on this pattern

---

## Planned Development

### Near Term
- [ ] EnTT ECS integration — entities with Transform, MeshRenderer, Material components
- [ ] Hierarchy panel wired to live entity list
- [ ] Inspector shows and edits selected entity components
- [ ] Mesh loading (.obj via tinyobjloader)
- [ ] Fly camera with mouse + keyboard input
- [ ] Basic lighting (Phong/Blinn-Phong)
- [ ] Scene save/load (nlohmann/json)
- [ ] Shader & script hot-reload

### Generative Tech-Art Pipeline
- [ ] **Prompt parser** — keyword extraction from natural language input ("galaxy", "fire", "cyberpunk", "lava")
- [ ] **Procedural GLSL generator** — keyword → noise functions, fbm, domain warping, glow, color ramps assembled into a valid fragment shader string
- [ ] **Runtime compilation** — generated GLSL string compiled live, swapped onto any mesh instantly
- [ ] **Gameplay reactivity** — bind shader parameters to ECS values (health, speed, position, time)
- [ ] **Shader editor** — view and edit generated GLSL directly in the inspector
- [ ] **Material prompt UI** — prominent text box in inspector, instant preview on selected object

### Visual Scripting
- [ ] imgui-node-editor integration (thedmd fork)
- [ ] Core nodes: Move, Rotate, OnCollision, SetMaterialParam, Jump, PlaySound
- [ ] Node execution wired to ECS systems
- [ ] Nodes can emit Lua scripts for complex logic

### Scripting
- [ ] Lua + sol2 integration
- [ ] Expose Transform, MeshRenderer, Material to Lua
- [ ] Hot-reload Lua scripts without engine restart

### AI Integration (Later Stage)
- [ ] **Local CUDA model** — small neural network trained on (prompt → GLSL) pairs, runs entirely on-device, no API calls
- [ ] **CUDA/OpenGL interop** — CUDA kernels write directly into OpenGL buffer objects via `cudaGraphicsGLRegisterBuffer`
- [ ] **Compute effects** — GPU particle systems, fluid sims, and texture synthesis driven by CUDA kernels
- [ ] **Behavior by demonstration** — record player actions, train lightweight RL policy, replay as NPC behavior

### Polish & Shipping
- [ ] One-click export to standalone executable (Windows + Linux)
- [ ] Asset store system for generated material packs
- [ ] Early Access release on itch.io
- [ ] Example projects: platformer, arena shooter, ambient demo scene

---

## Tech Stack

| Layer | Library |
|---|---|
| Language | C++20 |
| Build | CMake 3.20+ with FetchContent |
| Window & Input | GLFW 3.4 |
| OpenGL Loader | GLAD (OpenGL 3.3 Core) |
| Math | GLM 1.0.1 |
| ECS | EnTT (planned) |
| Editor UI | Dear ImGui (docking branch) |
| Node Editor | imgui-node-editor (planned) |
| Scripting | Lua + sol2 (planned) |
| Serialization | nlohmann/json (planned) |
| AI/Compute | CUDA (later stage) |

---

## Building

**Requirements:** GCC 13+, CMake 3.20+, OpenGL 3.3 capable GPU

```bash
git clone https://github.com/yourusername/NovaEngine
cd NovaEngine
mkdir build && cd build
cmake .. -DGLFW_BUILD_WAYLAND=OFF   # omit flag on native Linux
make -j$(nproc)
./NovaEngine
```

Dependencies (GLFW, GLAD, GLM, ImGui) are fetched automatically by CMake via FetchContent. No manual downloads required.

---

## Why Not Unity or Unreal?

Unity and Unreal are incredible tools. Nova is not trying to replace them — it's exploring a specific idea they haven't fully committed to: **generative materials as a first-class engine feature**, fused directly into the editor and runtime, accessible to beginners through prompts and to pros through raw shader code.

Roblox has the low floor. Unreal has the high ceiling. Nova is building both, with generative AI as the bridge.

---

## License

MIT
