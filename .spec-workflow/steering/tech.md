# Assignment 7: Technology Stack

## Project Type

SimpleMiner is a **3D voxel game engine** built as a desktop application using DirectX 11 for rendering. It combines custom game engine architecture with Minecraft-inspired gameplay mechanics and AI agent integration for autonomous gameplay research.

**Assignment 7 Focus:** Adding gameplay layer (inventory, crafting, mining) and AI integration layer (KADI WebSocket, MCP tools) on top of existing world generation and physics systems.

## Core Technologies

### Primary Language(s)
- **Language**: C++20
- **Compiler**: MSVC (Visual Studio 2022)
- **Compiler Flags**: `/std:c++20`, `/W3`, `/permissive-`
- **Platform**: Windows x64 only (DirectX 11 dependency)

### Key Dependencies/Libraries

#### Engine Systems (Existing)
- **DirectX 11**: GPU rendering, vertex/index buffers, shaders, textures
- **FMOD Studio API**: Professional audio middleware (`fmod.lib`, `fmodstudio.lib`)
- **Winsock2**: TCP/UDP networking, WebSocket client (`ws2_32.lib`)
- **nlohmann/json**: Header-only JSON parsing (`Engine\Code\ThirdParty\json\json.hpp`)
- **V8 JavaScript Engine**: Embedded scripting (currently unused in A7)

#### Third-Party Integration (Assignment 7)
- **KADI Broker**: WebSocket message broker for AI-game communication
- **Agent_TypeScript**: TypeScript agent framework (deployed on DigitalOcean)
- **Claude Desktop/Code**: AI assistant with MCP tool discovery and invocation

#### Assignment 7 New Systems
- **Widget Subsystem**: DirectX 11-based UI rendering (alpha quality, needs refinement)
- **KADI WebSocket Subsystem**: Game-to-AI integration protocol
- **JSON Registry System**: Data-driven block/item/recipe definitions

### Application Architecture

**Multi-Threaded Chunk-Based World Engine:**
- **Main Thread**: Game loop, rendering, input, UI, AI agent update
- **Worker Threads**: (N-1) threads for async chunk generation/loading/saving
  - 1 dedicated I/O thread for disk operations
  - (N-2) generic threads for computation-heavy terrain generation
- **Job System**: Thread-safe queue with LoadChunkJob, SaveChunkJob, ChunkGenerateJob

**Entity-Component System:**
- **Entity Base Class**: Physics-enabled objects (Player, AIAgent, ItemEntity)
- **Component Pattern**: Physics, rendering, inventory, AI command queue
- **World Manager**: Chunk activation/deactivation, entity lifecycle, lighting propagation

**Registry Pattern (Assignment 7):**
- **Generic Registry<T>**: Type-safe ID management for any type
- **BlockRegistry**: Replaces static `s_definitions` vector, JSON-based
- **ItemRegistry**: New system for tools, resources, placeable items
- **RecipeRegistry**: Crafting recipe management with pattern matching

### Data Storage

#### Primary Storage
- **World Data**: RLE-compressed chunk files in `Run/Saves/<worldName>/`
- **Game Configuration**: XML (legacy) and JSON (A7) in `Run/Data/Definitions/`
- **Player State**: JSON serialization in `Run/Saves/<worldName>/player.json`

#### File Formats
- **Chunks**: Custom binary format with RLE compression (`.chunk` files)
- **Definitions**: JSON for blocks, items, recipes (`BlockDefinitions.json`, etc.)
- **Shaders**: HLSL compiled bytecode (`.cso` files)
- **Audio**: WAV, MP3, OGG via FMOD
- **Textures**: PNG, JPG, TGA via DirectX 11

#### Caching
- **In-Memory Chunk Cache**: Active chunks kept in memory with dirty flags
- **Texture Cache**: Loaded textures persist until app shutdown
- **Sound Cache**: FMOD manages audio asset loading/unloading
- **UI Sprite Cache**: Widget textures cached per WidgetSubsystem instance

### External Integrations

#### KADI Broker Integration (Assignment 7)
- **Protocol**: WebSocket (RFC 6455) with KADI protocol v1
- **Authentication**: Public/private key pairs (RSA-based)
- **Message Format**: JSON-RPC 2.0 for tool invocation
- **Connection States**: 7-state flow (DISCONNECTED → READY)
- **Heartbeat**: Ping/pong every 30 seconds, 90-second timeout

#### MCP Tool Registration (Assignment 7)
SimpleMiner exposes these tools to Claude Desktop/Code:
1. `SimpleMiner_Spawn_Agent(name, x, y, z)` → agentID
2. `SimpleMiner_Move_Agent(agentID, dirX, dirY, dirZ, duration)`
3. `SimpleMiner_Mine_Block(agentID, x, y, z)`
4. `SimpleMiner_Place_Block(agentID, blockType, x, y, z)`
5. `SimpleMiner_Get_Agent_Vision(agentID)` → {blocks, entities}
6. `SimpleMiner_Get_Agent_Inventory(agentID)` → {itemStacks}
7. `SimpleMiner_Craft_Item(agentID, recipeID)`

#### Claude Agent Communication Flow
```
Claude Desktop → KADI Broker (WebSocket) → SimpleMiner (KADIWebSocketSubsystem)
                                          ↓
                                    Tool Handler (Game Logic)
                                          ↓
                                    AIAgent Entity (Command Queue)
                                          ↓
                                    World Update (Block Manipulation)
                                          ↓
                                    Tool Result (Success/Failure)
                                          ↓
KADI Broker ← SimpleMiner ← Tool Response JSON
     ↓
Claude Desktop (Result Display)
```

## Development Environment

### Build & Development Tools
- **Build System**: Visual Studio 2022 solution (.sln) with MSBuild
- **Project Structure**: SimpleMiner.vcxproj + Engine.vcxproj (submodule)
- **Configurations**: Debug|x64, Release|x64, DebugInline|x64, FastBreak|x64
- **Output**: Executables copied to `Run/` directory post-build
- **Hot Reload**: Not supported (C++ requires full rebuild and restart)

### Code Quality Tools
- **Static Analysis**: Visual Studio Code Analysis (/analyze flag, disabled by default)
- **Formatting**: Manual (no automated formatter configured)
- **Testing Framework**: Manual testing, no unit test framework
- **Documentation**: CLAUDE.md files per module, inline comments for complex logic
- **Memory Leak Detection**: Visual Studio CRT debug heap (Debug builds only)

### Version Control & Collaboration
- **VCS**: Git with GitHub remote
- **Submodules**: Engine as git submodule (`git submodule update --init --recursive`)
- **Branching Strategy**: Main branch for assignments, feature branches for experiments
- **Code Review**: User reviews AI-generated code via Claude Code approval system
- **.gitignore**: Excludes `.vs/`, `x64/`, `Debug/`, `Release/`, `*.user` files

### Assignment 7 Development Workflow
1. **Prerequisites**: Update CLAUDE.md documentation, create steering docs
2. **Steering Approval**: User reviews product.md, tech.md, structure.md via dashboard
3. **Design Phase**: Create design.md with architecture diagrams and rendering bug fix (P-1)
4. **Implementation**: Task-by-task execution with spec-workflow logging
5. **Testing**: MVP demo scenario validation (11 features)
6. **Archive**: Move A7 spec to `.spec-workflow/archive/specs/A7/`

## Deployment & Distribution

### Target Platform(s)
- **Primary**: Windows 10/11 x64 (DirectX 11 hardware required)
- **No Support**: Linux, macOS, mobile, web, consoles

### Distribution Method
- **Academic Submission**: Zip file or GitHub repository link
- **No Public Release**: Assignment project, not intended for distribution

### Installation Requirements
- **Windows SDK**: For DirectX 11 headers and libraries
- **Visual Studio 2022**: C++ development tools
- **Git**: For submodule initialization
- **KADI Broker**: Separate process (optional for non-AI testing)

### Update Mechanism
- **Manual**: User pulls latest code from repository
- **No Auto-Update**: Desktop application with manual rebuild required

## Technical Requirements & Constraints

### Performance Requirements (Assignment 7)
- **Target FPS**: 60 FPS sustained with inventory UI open and 3-10 AI agents
- **UI Rendering**: < 2ms per frame for all widgets (HUD, inventory, crafting)
- **Agent Update**: < 0.5ms per agent per frame (command processing + vision)
- **JSON Loading**: < 100ms for all registries at startup
- **Chunk Generation**: Background threads, non-blocking on main thread
- **Memory Limit**: < 2GB RAM for typical gameplay session

### Compatibility Requirements
- **OS**: Windows 10 (1809+) or Windows 11
- **GPU**: DirectX 11.0 compatible (Intel HD 4000+ minimum)
- **CPU**: Multi-core recommended for worker threads (2+ cores)
- **RAM**: 4GB minimum, 8GB recommended
- **Disk**: 500MB for executable + assets + world saves

### Security & Compliance
- **KADI Authentication**: Public/private key pairs, no plaintext passwords
- **No PII Collection**: Single-player game, no telemetry or user data transmission
- **Local Saves**: All data stored locally in `Run/Saves/` directory
- **WebSocket Security**: localhost connections only (no remote KADI brokers for A7)

### Scalability & Reliability
- **Expected Load**: Single player + 3-10 AI agents
- **World Size**: Theoretically infinite X/Y, finite Z (0-128 blocks)
- **Active Chunks**: ~81 chunks (9×9 around player) with async loading
- **Agent Concurrency**: Up to 10 agents supported, 3 for MVP
- **No Multiplayer**: Single-threaded game logic (no server architecture)

## Technical Decisions & Rationale

### Decision Log

#### 1. **JSON Over XML for Registries (Assignment 7)**
**Decision:** Migrate BlockDefinitions from XML to JSON, use JSON for all A7 data files.
**Rationale:**
- User explicitly requested JSON (3 comments)
- Engine already has `nlohmann/json` library
- Consistency with KADI protocol (JSON-RPC)
- Modern C++ STL-like API easier to use than XML parsing
**Alternatives Considered:**
- Keep XML (rejected: inconsistent with A7 direction)
- Use binary formats (rejected: not human-readable for debugging)

#### 2. **KADI WebSocket Over Direct API for AI Integration**
**Decision:** All AI agent control routes through KADIWebSocketSubsystem with MCP tool registration.
**Rationale:**
- Establishes standard protocol for AI-game integration
- User has existing KADI infrastructure (Agent_TypeScript on DigitalOcean)
- Enables Claude Desktop/Code integration via MCP
- Decouples AI logic from game engine (agents run externally)
**Alternatives Considered:**
- Direct function calls (rejected: tight coupling, no remote agents)
- Custom TCP protocol (rejected: reinventing WebSocket)

#### 3. **Use Existing WidgetSubsystem Despite "Very Rough" Status**
**Decision:** Build A7 UI using existing WidgetSubsystem, refine only blocking features.
**Rationale:**
- Time constraint (due 12/12) prevents UI framework rewrite
- Core lifecycle (z-order, owner grouping) functional
- User acknowledged system needs work but wants to use it
- Defer comprehensive redesign to post-A7
**Alternatives Considered:**
- Full WidgetSubsystem redesign (rejected: scope too large for deadline)
- Immediate mode GUI (ImGui for game UI, rejected: not Minecraft-authentic)

#### 4. **Command Queue Over Goal-Based AI (Assignment 7)**
**Decision:** Agents execute commands sequentially (MOVE, MINE, PLACE), not autonomous goal-based planning.
**Rationale:**
- KISS principle for MVP (simpler to implement and debug)
- User explicitly stated "command execution for now, goal-based for later"
- Allows testing AI-game integration without complex planning logic
- Easier to validate behavior (predictable command sequences)
**Alternatives Considered:**
- GOAP (Goal-Oriented Action Planning, rejected: too complex for A7 scope)
- Behavior trees (rejected: overkill for command execution)

#### 5. **2×2 Crafting Grid Only (No 3×3 Yet)**
**Decision:** Implement 2×2 inventory crafting, defer 3×3 workbench crafting to future.
**Rationale:**
- YAGNI principle (only 10 recipes required for MVP)
- 2×2 sufficient for all MVP recipes (planks, sticks, tools, crafting table)
- 3×3 requires additional UI work (workbench block, separate screen)
**Alternatives Considered:**
- Full 3×3 system (rejected: not required for A7 MVP, adds complexity)

## Known Limitations

### Technical Debt

#### 1. **WidgetSubsystem Alpha Quality**
**Limitation:** Missing modal dialog support, input event routing, layout system.
**Impact:** Inventory screen requires custom mouse capture logic, no automatic positioning.
**Future Solution:** Post-A7 comprehensive redesign with modal system, event bubbling, flex layouts.

#### 2. **Single-Threaded Game Logic**
**Limitation:** Main thread handles all gameplay (player, agents, inventory, crafting).
**Impact:** 10+ agents may cause frame drops if command processing is heavy.
**Future Solution:** Agent update on worker threads with thread-safe world queries.

#### 3. **No Network Multiplayer**
**Limitation:** World state not replicated, no client-server architecture.
**Impact:** Cannot have multiple human players + AI agents simultaneously.
**Future Solution:** Requires significant refactoring (authoritative server, state replication).

#### 4. **KADI Broker External Dependency**
**Limitation:** Requires separate KADI broker process for AI agent functionality.
**Impact:** Cannot test AI features without running broker separately.
**Future Solution:** Consider embedded broker mode or mock adapter for testing.

#### 5. **Windows-Only DirectX 11**
**Limitation:** No cross-platform support (no Vulkan/OpenGL backend).
**Impact:** Cannot run on Linux/macOS.
**Future Solution:** Abstract rendering backend (out of scope for academic project).

### Performance Considerations

#### Potential Bottlenecks
1. **UI Rendering**: Every widget rendered every frame (no dirty flag optimization)
2. **Agent Vision**: Each agent scans nearby blocks every 0.5s (can be expensive with 10 agents)
3. **JSON Parsing**: Loading registries at startup blocks main thread
4. **Chunk Mesh Rebuilding**: Frequent block changes (agent mining) triggers mesh regeneration

#### Mitigation Strategies (Implemented or Planned)
- **UI**: Z-order sorting only when widgets added/removed, not every frame
- **Agent Vision**: Cached results, 0.5s refresh rate, spatial partitioning for queries
- **JSON Loading**: One-time cost at startup, acceptable for < 100ms
- **Chunk Mesh**: Dirty flag system, rebuild on next frame (not immediate)

---

**Document Version:** 1.0
**Created:** 2025-11-24
**Assignment:** A7 - Registry, Inventory, UI, AI Agents
**Due Date:** December 12, 2025
