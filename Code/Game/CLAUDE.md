[🏠 Root](../../CLAUDE.md) > [📂 Code](../) > **[📂 Game]**

**Navigation:** [Framework](Framework/CLAUDE.md) | [Gameplay](Gameplay/CLAUDE.md) | [Definition](Definition/CLAUDE.md) | [Back to Root](../../CLAUDE.md)

---

# Game Module - Core Game Logic and Entry Points

## Quick Navigation
- **[Framework Module](Framework/CLAUDE.md)** - App, Block, Chunk systems
- **[Gameplay Module](Gameplay/CLAUDE.md)** - World, Player, Entity systems
- **[Definition Module](Definition/CLAUDE.md)** - BlockDefinition XML configuration
- **[Runtime Assets](../../Run/CLAUDE.md)** - Shaders, models, configurations
- **[Development Plan](../../.claude/plan/development.md)** - Assignment 4: World Generation

---

## Assignment 4 Context: World Generation

**Status:** Phase 0 - Prerequisites (Upcoming)

This module will be indirectly affected by Assignment 4 as the [Framework module](Framework/CLAUDE.md) and [Gameplay module](Gameplay/CLAUDE.md) undergo significant changes for procedural world generation. The [App class](Framework/CLAUDE.md) manages the JobSystem that will execute new world generation jobs.

**Key Changes Coming:**
- New block types from updated sprite sheets ([Definition module](Definition/CLAUDE.md))
- Enhanced chunk generation jobs in [Framework](Framework/CLAUDE.md)
- Biome system integration affecting [World class](Gameplay/CLAUDE.md)

**Resources:**
- [Development Plan](../../.claude/plan/development.md) - Complete implementation guide
- [Task Pointer](../../.claude/plan/task-pointer.md) - Quick task reference

---

## Module Responsibilities

The Game module serves as the primary entry point and core logic container for SimpleMiner. It manages the application lifecycle, game state, and coordinates between the custom engine and game-specific functionality. This module defines build preferences, handles Windows platform initialization, and orchestrates the main game systems.

See [Framework module](Framework/CLAUDE.md) for core systems implementation and [Gameplay module](Gameplay/CLAUDE.md) for game-specific logic.

## Entry and Startup

### Primary Entry Point
- **File**: [Framework/Main_Windows.cpp](Framework/CLAUDE.md)
- **Function**: `WinMain()` - Windows application entry point
- **Process**:
  1. Creates [App](Framework/CLAUDE.md) instance (`g_app = new App()`)
  2. Calls `App::Startup()` for initialization
  3. Runs main game loop via `App::RunMainLoop()`
  4. Handles shutdown and cleanup

### Application Architecture
- **[App Class](Framework/CLAUDE.md)** (`Framework/App.hpp/cpp`): Main application controller
  - Manages game lifecycle (startup, main loop, shutdown)
  - Initializes JobSystem with specialized worker threads
    - 1 dedicated I/O worker thread (for [ChunkLoadJob, ChunkSaveJob](Gameplay/CLAUDE.md))
    - (N-2) generic worker threads (for [ChunkGenerateJob](Gameplay/CLAUDE.md))
    - Thread count based on `std::thread::hardware_concurrency()`
  - Handles window events and cursor management
  - Coordinates with DevConsole camera system
  - Provides quit request handling

### Game State Management
- **[Game Class](Gameplay/CLAUDE.md)** (`Gameplay/Game.hpp/cpp`): Core game logic controller
  - **States**: `ATTRACT` (menu) and `GAME` (active play)
  - **Update Pipeline**: Input → Entities → [World](Gameplay/CLAUDE.md) updates
  - **Rendering**: Separate attract mode and gameplay rendering paths

## External Interfaces

### Engine Integration
- **Include Path**: `$(SolutionDir)../Engine/Code/` - Separate engine project
- **Dependencies**: Engine.vcxproj as project reference
- **Engine Services**: Rendering, input, audio, math, resource management

### V8 JavaScript Engine
- **Packages**: v8-v143-x64 (13.0.245.25) via NuGet
- **Libraries**: v8.dll, v8_libbase.dll, v8_libplatform.dll
- **Purpose**: Embedded scripting for game logic (implementation TBD)

### DirectX 11 Rendering
- **Shaders**: Custom HLSL shaders in `Run/Data/Shaders/`
  - `Default.hlsl` - Basic unlit rendering with Vertex_PCU
  - `BlinnPhong.hlsl` - Phong lighting model
  - `Bloom.hlsl` - Post-processing effects
- **Resource Management**: Vertex/Index buffers, textures, constant buffers

### FMOD Audio System
- **Libraries**: fmod.dll, fmod64.dll
- **Configuration**: Controlled by `ENGINE_DISABLE_AUDIO` preprocessor directive
- **Assets**: Audio files in `Run/Data/Audio/`

## Key Dependencies and Configuration

### Build Configuration
- **Platform**: Windows x64, Visual Studio 2022
- **C++ Standard**: C++20 (`/std:c++20`, `/Zc:__cplusplus`)
- **Engine Preferences**: `EngineBuildPreferences.hpp`
  - `ENGINE_DEBUG_RENDER` - Enables debug rendering systems
  - Optional `ENGINE_DISABLE_AUDIO` - Disables FMOD integration

### Project Structure
```
Code/Game/
├── Framework/           # Core game framework
├── Gameplay/           # Game-specific logic
├── Definition/         # Data definitions
├── Subsystem/         # Specialized systems
├── Game.vcxproj       # Visual Studio project
└── EngineBuildPreferences.hpp
```

### External Dependencies
- **Engine Project**: `../Engine/Code/Engine/Engine.vcxproj`
- **V8 Runtime**: NuGet packages with automatic DLL deployment
- **Windows SDK**: DirectX 11, Windows APIs

## Data Models

### Block System Architecture
- **[Block Class](Framework/CLAUDE.md)**: Ultra-lightweight 1-byte flyweight pattern
  - Single `uint8_t m_typeIndex` member
  - References global [BlockDefinition](Definition/CLAUDE.md) table
- **[BlockDefinition](Definition/CLAUDE.md)**: Data-driven block properties from XML
  - Visual properties (sprites, visibility, opacity)
  - Physical properties (solid, collision)
  - Lighting properties (emission values)
  - See [Definition module](Definition/CLAUDE.md) for details

### World Coordinate System
- **World Units**: 1 meter per block (1.0 x 1.0 x 1.0)
- **Coordinates**: Float-based Vec3 for precise positioning
- **Chunk System**: 16x16x128 block regions for efficient storage
- **Bounds**: Infinite X/Y (east/west, north/south), finite Z (0-128 vertical)

### Entity System
- **Entity Base Class**: Foundation for dynamic game objects
- **Player**: Specialized entity with camera and input handling
- **Prop**: Static/dynamic world objects and decorations
- **World**: Container and manager for all active chunks and entities

## Testing and Quality

### Debug Features
- **Debug Rendering**: Enabled via `ENGINE_DEBUG_RENDER`
- **DevConsole**: Built-in debugging interface with camera system
- **Memory Tracking**: DirectX 11 resource leak detection
- **Performance Monitoring**: Frame timing and chunk update metrics

### Build Quality
- **Warning Level**: Level 4 (`/W4`) for strict code quality
- **Static Analysis**: SDL security checks enabled
- **Memory Safety**: RAII patterns for all resource management
- **Platform Compliance**: Unicode character set, Windows subsystem

### Error Handling
- **Resource Management**: Exception-safe DirectX resource handling
- **File I/O**: Robust XML parsing with error reporting
- **Memory Allocation**: Proper cleanup of engine and V8 resources

## FAQ

**Q: How does the game coordinate with the separate Engine project?**
A: The Engine project is referenced as a separate Visual Studio project. Game code can include Engine headers but Engine cannot depend on Game code, maintaining clear architectural boundaries.

**Q: What is the role of V8 JavaScript engine?**
A: V8 provides embedded scripting capabilities for game logic. The integration includes automatic DLL deployment and library linking, but specific scripting APIs need further investigation.

**Q: How are DirectX resources managed?**
A: Resources are managed through the Engine's rendering system with RAII patterns. The game provides vertex data and shader specifications, while Engine handles DirectX object lifecycles.

**Q: What triggers chunk mesh rebuilding?**
A: Chunk mesh rebuilding occurs when `m_needsRebuild` flag is set, typically after terrain generation or block modifications. This is an expensive operation that should be batched.

## Related File List

### Core Framework
- [Framework/App.hpp/cpp](Framework/CLAUDE.md) - Application lifecycle management
- [Framework/Main_Windows.cpp](Framework/CLAUDE.md) - Windows entry point
- [Framework/GameCommon.hpp/cpp](Framework/CLAUDE.md) - Shared game utilities
- [Framework/Block.hpp/cpp](Framework/CLAUDE.md) - Block flyweight implementation
- [Framework/Chunk.hpp/cpp](Framework/CLAUDE.md) - Chunk storage and rendering

### Gameplay Systems
- [Gameplay/Game.hpp/cpp](Gameplay/CLAUDE.md) - Main game state machine
- [Gameplay/World.hpp/cpp](Gameplay/CLAUDE.md) - World and chunk management
- [Gameplay/Player.hpp/cpp](Gameplay/CLAUDE.md) - Player character controller
- [Gameplay/Entity.hpp/cpp](Gameplay/CLAUDE.md) - Base entity system
- [Gameplay/Prop.hpp/cpp](Gameplay/CLAUDE.md) - Static/dynamic world objects

### Data and Configuration
- [Definition/BlockDefinition.hpp/cpp](Definition/CLAUDE.md) - Block type definitions
- `EngineBuildPreferences.hpp` - Engine build configuration
- `Game.vcxproj` - Visual Studio project file

### Related Modules
- **[Framework Module](Framework/CLAUDE.md)** - Core systems implementation
- **[Gameplay Module](Gameplay/CLAUDE.md)** - Game logic and world management
- **[Definition Module](Definition/CLAUDE.md)** - Data-driven block configuration
- **[Runtime Assets](../../Run/CLAUDE.md)** - Assets and executables
- **[Development Plan](../../.claude/plan/development.md)** - Assignment 4: World Generation

## Changelog

- **2025-10-26**: Updated documentation structure with comprehensive linking and Assignment 4 context
  - Added navigation breadcrumbs and quick navigation section
  - Added Assignment 4 context explaining upcoming world generation changes
  - Enhanced inline cross-references to Framework, Gameplay, and Definition modules
  - Added links to development planning resources
  - Reorganized Related File List with hyperlinks to module documentation
- **2025-09-13**: Initial module documentation created
- **Recent**: Fixed relative paths in Game.vcxproj for better portability
- **Recent**: Resolved all compiler warnings and DirectX 11 memory leaks
- **Recent**: Added NuGet configuration for V8 JavaScript engine integration
- **Recent**: Implemented proper chunk rendering with face culling optimization