[Root Directory](../../CLAUDE.md) > [Code](../) > **Game**

# Game Module - Core Game Logic and Entry Points

## Module Responsibilities

The Game module serves as the primary entry point and core logic container for SimpleMiner. It manages the application lifecycle, game state, and coordinates between the custom engine and game-specific functionality. This module defines build preferences, handles Windows platform initialization, and orchestrates the main game systems.

## Entry and Startup

### Primary Entry Point
- **File**: `Framework/Main_Windows.cpp`
- **Function**: `WinMain()` - Windows application entry point
- **Process**:
  1. Creates App instance (`g_app = new App()`)
  2. Calls `App::Startup()` for initialization
  3. Runs main game loop via `App::RunMainLoop()`
  4. Handles shutdown and cleanup

### Application Architecture
- **App Class** (`Framework/App.hpp/cpp`): Main application controller
  - Manages game lifecycle (startup, main loop, shutdown)
  - Initializes JobSystem with specialized worker threads
    - 1 dedicated I/O worker thread (for ChunkLoadJob, ChunkSaveJob)
    - (N-2) generic worker threads (for ChunkGenerateJob)
    - Thread count based on `std::thread::hardware_concurrency()`
  - Handles window events and cursor management
  - Coordinates with DevConsole camera system
  - Provides quit request handling

### Game State Management
- **Game Class** (`Gameplay/Game.hpp/cpp`): Core game logic controller
  - **States**: `ATTRACT` (menu) and `GAME` (active play)
  - **Update Pipeline**: Input → Entities → World updates
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
- **Block Class**: Ultra-lightweight 1-byte flyweight pattern
  - Single `uint8_t m_typeIndex` member
  - References global `BlockDefinition` table
- **BlockDefinition**: Data-driven block properties from XML
  - Visual properties (sprites, visibility, opacity)
  - Physical properties (solid, collision)
  - Lighting properties (emission values)

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
- `Framework/App.hpp/cpp` - Application lifecycle management
- `Framework/Main_Windows.cpp` - Windows entry point
- `Framework/GameCommon.hpp/cpp` - Shared game utilities
- `Framework/Block.hpp/cpp` - Block flyweight implementation
- `Framework/Chunk.hpp/cpp` - Chunk storage and rendering

### Gameplay Systems  
- `Gameplay/Game.hpp/cpp` - Main game state machine
- `Gameplay/World.hpp/cpp` - World and chunk management
- `Gameplay/Player.hpp/cpp` - Player character controller
- `Gameplay/Entity.hpp/cpp` - Base entity system
- `Gameplay/Prop.hpp/cpp` - Static/dynamic world objects

### Data and Configuration
- `Definition/BlockDefinition.hpp/cpp` - Block type definitions
- `EngineBuildPreferences.hpp` - Engine build configuration
- `Game.vcxproj` - Visual Studio project file

## Changelog

- **2025-09-13**: Initial module documentation created
- **Recent**: Fixed relative paths in Game.vcxproj for better portability
- **Recent**: Resolved all compiler warnings and DirectX 11 memory leaks
- **Recent**: Added NuGet configuration for V8 JavaScript engine integration
- **Recent**: Implemented proper chunk rendering with face culling optimization