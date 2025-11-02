[🏠 Root](../../../CLAUDE.md) > [📂 Code](../../) > [📂 Game](../) > **[🎮 Framework]**

**Navigation:** [Back to Game](../CLAUDE.md) | [Gameplay](../Gameplay/CLAUDE.md) | [Definition](../Definition/CLAUDE.md) | [Root](../../../CLAUDE.md)

---

# Framework Module - Core Game Systems

## Quick Navigation
- **[Game Module](../CLAUDE.md)** - Core game logic and entry points
- **[Gameplay Module](../Gameplay/CLAUDE.md)** - World, Player, Entity systems
- **[Definition Module](../Definition/CLAUDE.md)** - BlockDefinition configuration
- **[Runtime Assets](../../../Run/CLAUDE.md)** - Shaders, models, configurations
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4: World Generation
- **[Task Pointer](../../../.claude/plan/task-pointer.md)** - Quick task reference

---

## Assignment 4: World Generation - PRIMARY IMPLEMENTATION LOCATION

**Status:** Phase 0 - Prerequisites ✅ COMPLETED (2025-11-01)

**CRITICAL:** This module contains the primary implementation files for Assignment 4's world generation system. The `Chunk.cpp::GenerateTerrain()` method will undergo a complete restructure.

### Files to be Modified

**Chunk.cpp** (lines 112-324)
- **Pass 1** (lines 132-217): Per-(x,y) column calculations
  - **ADD:** 6 biome noise layers (Temperature, Humidity, Continentalness, Erosion, Weirdness, Peaks & Valleys)
  - **ADD:** Biome selection using lookup tables
  - **STORE:** BiomeData for each column
- **Pass 2** (lines 220-320): Per-(x,y,z) block placement
  - **REPLACE:** 2D heightmap logic with 3D density formula: D(x,y,z) = N(x,y,z,s) + B(z)
  - **ADD:** Terrain shaping curves based on Continentalness, Erosion, and Peaks & Valleys
  - **ADD:** Top and bottom slides for smooth transitions
  - **ADD:** Cheese and spaghetti cave carving (3D noise-based)
  - **ADD:** Tree placement using hardcoded stamps
  - **ADD:** Biome-appropriate surface block replacement

**Chunk.hpp**
- **ADD:** `BiomeData` struct (6 noise values + BiomeType)
- **ADD:** `BiomeData m_biomeData[CHUNK_SIZE_X * CHUNK_SIZE_Y]` member
- **ADD:** `TreeStamp` struct for hardcoded tree patterns

**GameCommon.hpp**
- **ADD:** Biome noise scale constants (Continentalness, Erosion, Weirdness, PV)
- **ADD:** Density formula parameters (scale, octaves, default height, bias)
- **ADD:** Cave parameters (cheese/spaghetti scales and thresholds)
- **ADD:** Tree placement parameters
- **ADD:** Carver parameters (ravines, rivers)
- **ADD:** `BiomeType` enum

### Technical Approach

**Minecraft-Inspired Pipeline:**
1. **Biomes** - Sample 6 noise layers, determine biome via lookup table
2. **3D Density** - Use density formula with curves/splines for terrain shaping
3. **Surface** - Replace surface blocks based on biome type
4. **Trees** - Place biome-specific tree stamps
5. **Caves** - Carve cheese (caverns) and spaghetti (tunnels) caves
6. **Carvers** - Add ravines and rivers for dramatic features

**Key Resources:**
- [Development Plan](../../../.claude/plan/development.md) - Complete technical specifications
- [Task Pointer](../../../.claude/plan/task-pointer.md) - Implementation steps
- [Henrik Kniberg Analysis](../../../Docs/CLAUDE.md) - Minecraft world generation explanation
- [Professor's Blog](../../../Docs/CLAUDE.md) - October 2025 implementation timeline

---

## Module Responsibilities

The Framework module provides the foundational systems that bridge the custom Engine with game-specific functionality. It handles application lifecycle, voxel world representation, and core data structures that enable the chunk-based voxel game architecture. This module contains the most performance-critical code for block and chunk management.

## Entry and Startup

### Application Entry Point
- **Main_Windows.cpp**: Windows platform entry point
  - Standard WinMain function with clean initialization/shutdown pattern
  - Creates and manages global App instance lifecycle
  - Handles command line arguments (currently unused)

### Application Class (App.hpp/cpp)
- **Initialization**: `Startup()` - Sets up engine systems and game state
- **Main Loop**: `RunMainLoop()` - Core game loop with frame processing
- **Frame Processing**: `RunFrame()` → BeginFrame → Update → Render → EndFrame
- **Shutdown**: Clean resource deallocation and engine shutdown
- **Event Handling**: Window close button handling and quit requests

## External Interfaces

### Engine Integration Points
```cpp
#include "Engine/Core/EventSystem.hpp"       // Event-driven architecture
#include "Engine/Core/EngineCommon.hpp"      // Core engine utilities
#include "Engine/Renderer/VertexUtils.hpp"   // Rendering vertex data
#include "Engine/Math/AABB3.hpp"             // 3D bounding boxes
#include "Engine/Math/IntVec2.hpp"           // Integer 2D vectors
#include "Engine/Math/IntVec3.hpp"           // Integer 3D vectors
```

### DirectX 11 Resource Management
- **Vertex Buffers**: Dynamic vertex data for chunk meshes
- **Index Buffers**: Optimized triangle indexing for face rendering
- **Constant Buffers**: Shader parameter updates per frame
- **Shader Resource Views**: Block texture atlas binding

### Camera System Integration
- **DevConsole Camera**: Debug camera for development tools
- **World Camera**: Primary gameplay camera managed through Engine

## Key Dependencies and Configuration

### Block System Architecture
#### Block Class (1 byte ultra-flyweight)
```cpp
class Block {
    uint8_t m_typeIndex = 0;  // Index into global BlockDefinition table
};
```

#### BlockDefinition System
- **XML Configuration**: See [Runtime Assets](../../../Run/CLAUDE.md) - `Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`
- **Properties**: visibility, solidity, opacity, texture coordinates, lighting
- **Performance**: Block instances reference shared [definitions](../Definition/CLAUDE.md) for memory efficiency
- **Assignment 4**: New sprite sheet will add biome-specific blocks (trees, leaves, surface variants)

### Chunk System Architecture

#### Chunk State Machine (Thread-Safe)
```cpp
enum class ChunkState : uint8_t {
    CONSTRUCTING,          // Initial allocation (main thread)
    ACTIVATING,            // Being added to world (main thread)
    LOADING,               // I/O worker loading from disk
    LOAD_COMPLETE,         // Load finished
    TERRAIN_GENERATING,    // Generic worker generating terrain
    LIGHTING_INITIALIZING, // Setting up lighting
    COMPLETE,              // Ready for rendering (main thread)
    DEACTIVATING,          // Being removed (main thread)
    SAVING,                // I/O worker saving to disk
    SAVE_COMPLETE,         // Save finished
    DECONSTRUCTING         // Final cleanup (main thread)
};
```

- **Atomic Transitions**: `std::atomic<ChunkState>` for thread safety
- **Worker Threads**: Only access chunks in LOADING, TERRAIN_GENERATING, SAVING states
- **Main Thread**: Only accesses chunks in COMPLETE state, handles D3D11 operations

#### Chunk Storage (16x16x128 blocks = 32,768 blocks per chunk)
```cpp
Block m_blocks[CHUNK_TOTAL_BLOCKS];  // 1D array for cache efficiency
```

#### Chunk Coordinates
- **2D Chunk Grid**: IntVec2 coordinates for horizontal positioning
- **No Vertical Stacking**: Each chunk extends full world height (0-128)
- **Adjacent Mapping**: Chunk (4,7) borders chunk (3,7) seamlessly

#### Asynchronous Chunk Jobs
- **ChunkGenerateJob**: Generic workers generate terrain procedurally
- **ChunkLoadJob**: I/O worker loads from disk with RLE decompression
- **ChunkSaveJob**: I/O worker saves to disk with RLE compression
- **File Format**: 'GCHK' fourCC + version + dimensions + RLE entries

#### Mesh Generation (Main Thread Only)
- **Face Culling**: Only visible faces are added to vertex buffer
- **Texture Atlas**: UV mapping to sprite sheet coordinates
- **Vertex Format**: Position-Color-UV (PCU) for basic rendering
- **Index Optimization**: Reduces vertex duplication for shared edges

## Data Models

### World Coordinate System
```cpp
// World units: 1 meter per block
// World bounds: Infinite X/Y, finite Z (0.0 to 128.0)
// Chunk size: 16 x 16 x 128 blocks
AABB3 m_worldBounds;  // 3D bounding box for each chunk
```

### Chunk Rendering Pipeline
1. **Terrain Generation**: `GenerateTerrain()` - Procedural block placement
2. **Mesh Building**: `RebuildMesh()` - Convert blocks to renderable triangles
3. **Face Culling**: `AddBlockFacesIfVisible()` - Only render exposed faces
4. **Vertex Buffer Update**: `UpdateVertexBuffer()` - GPU resource management
5. **Render**: GPU-accelerated triangle rendering with texture atlas

### Memory Layout Optimization
- **Block Array**: 1D storage `[x + y*16 + z*16*16]` for cache efficiency
- **Coordinate Conversion**: `GetBlockIndexFromLocalCoords()` and reverse
- **Bit Packing**: Chunk coordinate system uses bit shifts for fast math
  - X: 4 bits (0-15), Y: 4 bits (0-15), Z: 7 bits (0-127)

## Testing and Quality

### Performance Characteristics
- **Block Storage**: 32KB per chunk (32,768 bytes for block array)
- **Mesh Rebuild**: Expensive operation, triggered by `m_needsRebuild` flag
- **Face Culling**: Reduces vertex count by ~80% in typical scenarios
- **Cache Efficiency**: 1D block array provides better memory access patterns

### Debug Features
- **Debug Rendering**: Wireframe chunk boundaries when `m_drawDebug = true`
- **Debug Vertices**: Separate vertex buffer for debug visualization
- **Bounds Visualization**: AABB3 rendering for chunk boundaries
- **Performance Timing**: Mesh rebuild timing for optimization

### Resource Management
- **RAII Pattern**: Automatic cleanup of DirectX vertex/index buffers
- **Double Buffering**: Separate debug and main rendering buffers
- **Memory Leaks**: Recent fixes ensure proper DirectX resource cleanup

## FAQ

**Q: Why is Block only 1 byte?**
A: Ultra-flyweight design pattern. With potentially millions of blocks in memory, size optimization is critical. All shared data lives in BlockDefinition.

**Q: How expensive is chunk mesh rebuilding?**
A: Very expensive. It processes up to 32,768 blocks, performs face culling calculations, and rebuilds GPU vertex buffers. Should be batched and minimized.

**Q: What triggers a chunk mesh rebuild?**
A: The `m_needsRebuild` flag is set when blocks change (terrain generation, player modifications). The rebuild happens during the next Update() call.

**Q: How does face culling work?**
A: `AddBlockFacesIfVisible()` checks adjacent blocks. If a neighboring block is opaque, that face is not added to the vertex buffer, reducing triangle count significantly.

**Q: What is the coordinate system relationship?**
A: World coordinates (float Vec3) → Chunk coordinates (IntVec2) → Local block coordinates (IntVec3) → Block array index (int).

## Related File List

### Core Framework Files
- `App.hpp/cpp` - Application lifecycle and main loop
- `Main_Windows.cpp` - Windows platform entry point
- `GameCommon.hpp/cpp` - Shared utilities and globals (**Assignment 4: Add constants here**)
- `Block.hpp/cpp` - Ultra-lightweight block flyweight
- `Chunk.hpp/cpp` - Chunk storage and mesh generation (**Assignment 4: PRIMARY FILE TO MODIFY**)

### Dependencies
- [Definition/BlockDefinition.hpp/cpp](../Definition/CLAUDE.md) - Block type definitions
- [Gameplay/World.hpp/cpp](../Gameplay/CLAUDE.md) - World and chunk coordination
- `../EngineBuildPreferences.hpp` - Build configuration
- Engine headers for math, rendering, and core systems

### Configuration Files
- [Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml](../../../Run/CLAUDE.md) - Block properties (**Assignment 4: REPLACE with new file**)
- [Run/Data/GameConfig.xml](../../../Run/CLAUDE.md) - Window and display settings

### Related Modules
- **[Game Module](../CLAUDE.md)** - Entry points and build configuration
- **[Gameplay Module](../Gameplay/CLAUDE.md)** - World management and chunk activation
- **[Definition Module](../Definition/CLAUDE.md)** - Block configuration system
- **[Runtime Assets](../../../Run/CLAUDE.md)** - Asset files and executables
- **[Development Plan](../../../.claude/plan/development.md)** - Assignment 4 implementation guide

## Changelog

- **2025-11-01**: Completed Phase 0 prerequisites (Tasks 0.1-0.7) for Assignment 4: World Generation
  - ✅ Chunk preloading, activation, and deactivation optimizations
  - ✅ Mesh rebuilding performance improvements
  - ✅ Job queue limiting and thread-safe chunk state machine
  - ✅ Smart directional preloading based on player movement
  - Ready to begin Phase 1: Foundation (Biome System Implementation)
- **2025-10-26**: Updated documentation for Assignment 4: World Generation
  - Added comprehensive Assignment 4 section detailing GenerateTerrain() modifications
  - Added navigation breadcrumbs and quick navigation
  - Enhanced inline cross-references to Gameplay, Definition, and Run modules
  - Documented specific code line numbers for implementation (Pass 1: lines 132-217, Pass 2: lines 220-320)
  - Added technical approach section with Minecraft-inspired pipeline
  - Linked to development planning resources and Henrik Kniberg analysis
- **2025-09-13**: Initial Framework module documentation created
- **Recent**: Chunk rendering system now properly handles face culling and mesh optimization
- **Recent**: Fixed DirectX 11 memory leaks in vertex/index buffer management
- **Recent**: Optimized block face mapping for better rendering performance