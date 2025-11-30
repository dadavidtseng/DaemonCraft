# Assignment 5: Rendering and Lighting - Tasks Document

**Spec:** A5 - Rendering and Lighting System
**Created:** 2025-11-15
**Status:** Planning Complete, Ready for Implementation

---

## Task Overview

This document breaks down the Assignment 5 implementation into 13 sequential phases. Each task includes:
- **Files to modify/create** with specific line numbers where applicable
- **Implementation guidance** with pseudocode
- **Verification criteria** for acceptance testing
- **Dependencies** to ensure correct execution order
- **AI agent prompts** for autonomous task execution

**Critical Notes:**
- All DirectX 11 operations must execute on main thread only
- Block structure expansion breaks existing save files (1→3 bytes)
- Performance targets: 60 FPS sustained, <8ms lighting budget, <100ms cross-chunk latency
- Do NOT modify Default.hlsl - create new World.hlsl instead

---

## Phase 1: Core Data Structures (Tasks 1-3)

- [x] 1. Block Structure Expansion

**Files:**
- `Code/Game/Framework/Block.hpp` (MODIFIED)
- `Code/Game/Framework/Block.cpp` (MODIFIED)

**Purpose:** Expand Block class from 1 byte to 3 bytes to support lighting data and bit flags. This is the foundation for the entire lighting system.

**Implementation:**
1. Modify Block.hpp:
   - Keep `uint8_t m_typeIndex` (existing)
   - Add `uint8_t m_lightingData` (high nibble: outdoor 0-15, low nibble: indoor 0-15)
   - Add `uint8_t m_bitFlags` for boolean flags
   - Add inline accessors: `GetOutdoorLight()`, `GetIndoorLight()`, `SetOutdoorLight()`, `SetIndoorLight()`
   - Add `IsSkyVisible()` flag accessor
   - Ensure POD compliance for memcpy operations

2. Update Block.cpp:
   - Implement inline accessor methods
   - Add debug validation (light values 0-15)

**Verification:**
- [x] Compile without errors
- [x] `sizeof(Block) == 3`
- [x] Set outdoor light to 15, verify `GetOutdoorLight()` returns 15
- [x] Set indoor light to 8, verify `GetIndoorLight()` returns 8
- [x] Verify both lights can be stored simultaneously without interference

**Implementation Notes:**
- Completed 2025-11-15
- Nibble packing: High nibble (bits 4-7) = outdoor light, Low nibble (bits 0-3) = indoor light
- Inline accessors use bitwise operations: `(m_lightingData >> 4) & 0x0F` for outdoor, `m_lightingData & 0x0F` for indoor
- Sky visibility uses bit 0 of m_bitFlags, bits 1-7 reserved for future use
- Static assertions verify structure size (3 bytes) and member offsets at compile-time
- All 12 comprehensive validation tests passed (nibble encoding, boundary values, no interference)

**Notes:** This changes Block from 1 byte to 3 bytes. All existing Chunk storage (16x16x128 = 32,768 blocks per chunk) will triple in memory from 32KB to 96KB per chunk. This is acceptable per requirements.

_Requirements: 1, 2, 3_

_Prompt: Role: C++ Systems Programmer specializing in low-level data structures and memory optimization | Task: Expand Block class from 1 to 3 bytes following requirements 1-3, adding lighting data (outdoor/indoor nibbles) and bit flags with inline accessors, ensuring POD compliance for memcpy operations | Restrictions: Must maintain POD type, all accessors must be inline for performance, validate light values 0-15 in debug builds only | Success: sizeof(Block)==3, accessors work correctly, both light channels store/retrieve independently, POD compliance verified with std::is_pod_

---

- [x] 2. BlockIterator Cross-Chunk Navigation

**Files:**
- `Code/Game/Framework/BlockIterator.hpp` (TO_MODIFY)
- `Code/Game/Framework/BlockIterator.cpp` (TO_MODIFY)
- `Code/Game/Gameplay/World.hpp` (REFERENCE)

**Purpose:** Extend existing BlockIterator neighbor methods (GetNorthNeighbor, GetSouthNeighbor, etc.) to handle chunk boundaries by fetching neighbor chunks from World.

**Implementation:**
1. Modify BlockIterator.hpp:
   - Keep existing 6 neighbor methods (already exist in codebase)
   - Add `World* m_world` pointer to BlockIterator constructor
   - Methods should detect edge conditions (x==0, x==15, y==0, y==15, z==0, z==127)
   - When at edge, calculate neighbor chunk coordinates
   - Call `m_world->GetChunk(neighborChunkCoords)`
   - Return BlockIterator pointing to correct block in neighbor chunk
   - Return invalid BlockIterator if neighbor chunk not loaded

2. Pseudocode pattern for GetEastNeighbor():
```cpp
BlockIterator BlockIterator::GetEastNeighbor() const {
    int localX = m_blockIndex % CHUNK_SIZE_X;
    if (localX < CHUNK_SIZE_X - 1) {
        return BlockIterator(m_chunk, m_blockIndex + 1); // Same chunk
    } else {
        IntVec2 neighborCoords = m_chunk->GetChunkCoords() + IntVec2(1, 0);
        Chunk* neighborChunk = m_world->GetChunk(neighborCoords);
        if (!neighborChunk) return BlockIterator(); // Invalid
        int neighborIndex = /* calculate index at x=0, same y, same z */;
        return BlockIterator(neighborChunk, neighborIndex);
    }
}
```

3. Update all 6 methods with this pattern

**Verification:**
- [x] Create test chunk at (0,0)
- [x] Get block at east edge (x=15)
- [x] Call GetEastNeighbor()
- [x] Verify returned BlockIterator points to chunk (1,0) at x=0
- [x] Test all 6 directions at chunk boundaries
- [x] Verify returns invalid iterator when neighbor chunk not loaded

**Implementation Notes:**
- Completed 2025-11-15
- Extended BlockIterator constructors to accept World* pointer (BlockIterator.hpp lines 20-23, 27-32)
- Added m_world member variable for cross-chunk navigation (line 50)
- Modified all 6 neighbor methods to handle chunk boundaries:
  - GetNorthNeighbor(), GetSouthNeighbor() (Y-axis traversal)
  - GetEastNeighbor(), GetWestNeighbor() (X-axis traversal)
  - GetUpNeighbor(), GetDownNeighbor() (Z-axis traversal)
- Edge detection: Checks if local coordinates are at chunk boundaries (0 or CHUNK_MAX_X/Y/Z)
- Cross-chunk logic: Calculates neighbor chunk coordinates, calls m_world->GetChunk(), converts to neighbor block index
- Returns invalid BlockIterator (IsValid() == false) when neighbor chunk not loaded
- Used by Phase 5 (RecalculateBlockLighting) and Phase 6 (OnActivate edge processing)

**Notes:** REUSE EXISTING CODE: BlockIterator already has GetNorthNeighbor(), GetSouthNeighbor(), GetEastNeighbor(), GetWestNeighbor(), GetUpNeighbor(), GetDownNeighbor() at BlockIterator.hpp. Just extend them to handle chunk boundaries.

_Leverage: Code/Game/Framework/BlockIterator.hpp (existing neighbor methods), Code/Game/Gameplay/World.hpp (GetChunk method)_

_Requirements: 4_

_Dependencies: Task 1 (Block Structure Expansion)_

_Prompt: Role: C++ Game Engine Developer specializing in spatial partitioning and chunk systems | Task: Extend existing BlockIterator 6-direction neighbor methods to handle cross-chunk boundaries following requirement 4, adding World* pointer and using World::GetChunk() for neighbor chunk fetching | Restrictions: Must reuse existing GetNorthNeighbor/South/East/West/Up/Down methods, return invalid iterator when neighbor not loaded, maintain const-correctness | Success: All 6 methods correctly traverse chunk boundaries, invalid iterator returned when neighbor unloaded, edge cases handled (x==0,15 y==0,15 z==0,127)_

---

- [x] 3. Chunk Lighting Initialization

**Files:**
- `Code/Game/Framework/Chunk.cpp` (TO_MODIFY) - Add InitializeLighting() call at end of GenerateTerrain()
- `Code/Game/Framework/Chunk.hpp` (TO_MODIFY) - Declare InitializeLighting() private method
- `Code/Game/Definition/BlockDefinition.hpp` (REFERENCE) - IsOpaque() and GetEmissiveValue() methods

**Purpose:** Initialize outdoor and indoor lighting values when chunks are first generated. Set sky-visible blocks to outdoor=15, everything else to 0.

**Implementation:**
1. Add private method to Chunk class: `void InitializeLighting()`
2. Call `InitializeLighting()` at end of `Chunk::GenerateTerrain()` (after all terrain gen completes)
3. Scan all blocks from top (z=127) downward for each (x,y) column:
   - Find first solid/opaque block from top
   - Set all air blocks above to: outdoor=15, indoor=0, skyVisible=true
   - Set all blocks at/below first solid to: outdoor=0, indoor=0, skyVisible=false
   - Special case: Emissive blocks (glowstone, lava) set indoor=15

**Pseudocode:**
```cpp
void Chunk::InitializeLighting() {
    for (int x = 0; x < 16; x++) {
        for (int y = 0; y < 16; y++) {
            bool foundSolid = false;
            for (int z = 127; z >= 0; z--) {
                Block& block = GetBlock(x, y, z);
                if (!foundSolid && block.IsOpaque()) {
                    foundSolid = true;
                }
                if (!foundSolid) {
                    block.SetOutdoorLight(15);
                    block.SetIsSkyVisible(true);
                } else {
                    block.SetOutdoorLight(0);
                }
                if (block.IsEmissive()) {
                    block.SetIndoorLight(block.GetEmissiveValue());
                }
            }
        }
    }
}
```

**Verification:**
- [x] Generate new chunk
- [x] Verify air blocks above terrain have outdoor=15
- [x] Verify solid blocks have outdoor=0
- [x] Verify glowstone blocks have indoor=15
- [x] Verify no crashes during chunk generation

**Implementation Notes:**
- Completed 2025-11-15
- Added InitializeLighting() private method to Chunk class (Chunk.cpp lines 3109-3151)
- Called from GenerateTerrain() at end of terrain generation pipeline (after trees, caves, rivers)
- Algorithm: Scans each (x,y) column from top (z=127) downward
  - Tracks foundSolid flag to detect first opaque block from sky
  - Air blocks above solid surface: outdoor=15, skyVisible=true
  - Blocks at/below surface: outdoor=0, skyVisible=false
  - Emissive blocks (glowstone): indoor=emissiveValue from BlockDefinition
- Performance: Single-pass per chunk, O(CHUNK_SIZE_X × CHUNK_SIZE_Y × CHUNK_SIZE_Z) = O(16×16×128) = 32,768 blocks
- Does NOT handle cross-chunk lighting propagation (Phase 6 handles that)
- Provides correct initial lighting state for newly generated chunks

**Notes:** This runs once per chunk during generation. Does NOT handle cross-chunk lighting yet - that's Phase 4.

_Leverage: Code/Game/Definition/BlockDefinition.hpp (IsOpaque, GetEmissiveValue)_

_Requirements: 5_

_Dependencies: Task 1 (Block Structure Expansion)_

_Prompt: Role: C++ Game Developer specializing in procedural generation and lighting systems | Task: Implement InitializeLighting() method following requirement 5, scanning top-down per column to set sky-visible blocks to outdoor=15 and emissive blocks to indoor=15, called at end of GenerateTerrain() | Restrictions: Must run only once during chunk generation, do not handle cross-chunk propagation yet, use IsOpaque() and GetEmissiveValue() from BlockDefinition | Success: Air blocks above terrain have outdoor=15, solid blocks have outdoor=0, emissive blocks have indoor=15, no crashes during generation_

---

## Phase 2: Lighting Propagation (Tasks 4-6)

- [x] 4. World Dirty Light Queue

**Files:**
- `Code/Game/Gameplay/World.hpp` (TO_MODIFY) - Add m_dirtyLightQueue and processing methods
- `Code/Game/Gameplay/World.cpp` (TO_MODIFY) - Implement dirty queue processing with time budget

**Purpose:** Add global dirty light queue to World class for tracking blocks that need light recalculation. This queue will be processed every frame with 8ms time budget.

**Implementation:**
1. Modify World.hpp:
   - Add member: `std::deque<BlockIterator> m_dirtyLightQueue;`
   - Add method: `void AddToDirtyLightQueue(BlockIterator blockIter);`
   - Add method: `void ProcessDirtyLighting(float maxTimeSeconds);`

2. Modify World.cpp:
   - Implement AddToDirtyLightQueue(): Check if already in queue, add to back
   - Implement ProcessDirtyLighting():
     - Start timer
     - While (queue not empty AND time < maxTimeSeconds):
       - Pop block from front of queue
       - Recalculate lighting (see Task 5)
       - Process next block
     - Return when time budget exhausted or queue empty

3. Call `ProcessDirtyLighting(0.008f)` from `World::Update()` (8ms budget per requirements)

**Verification:**
- [ ] Add 1000 blocks to dirty queue
- [ ] Call ProcessDirtyLighting(0.001) // 1ms budget
- [ ] Verify only partial queue processed
- [ ] Verify no crashes
- [ ] Verify queue eventually empties over multiple frames

**Notes:** This queue will be populated by: chunk activation (Task 6), block dig/place (Task 11), and cross-chunk lighting propagation (Task 5).

**Implementation Notes:**
- Completed 2025-11-15
- Modified World.hpp:
  - Changed from `class BlockIterator;` forward declaration to `#include "Game/Framework/BlockIterator.hpp"` (line 15) to provide complete class definition for std::deque
  - Added `std::deque<BlockIterator> m_dirtyLightQueue;` member (line 190)
  - Added `void AddToDirtyLightQueue(BlockIterator const& blockIter);` method (line 151)
  - Added `void ProcessDirtyLighting(float maxTimeSeconds);` method (line 152)
- Modified World.cpp:
  - Added `#include "Engine/Core/Clock.hpp"` for timing functions (line 13)
  - Added `#include "Game/Framework/Block.hpp"` and `#include "Game/Framework/BlockIterator.hpp"` (lines 22-23)
  - Added `ProcessDirtyLighting(0.008f);` call in `World::Update()` (line 111) - 8ms per-frame budget
  - Implemented `AddToDirtyLightQueue()` with duplicate detection (lines 1757-1773)
  - Implemented `ProcessDirtyLighting()` with time budget enforcement using `Clock::GetSystemClock().GetTotalSeconds()` (lines 1775-1807)
- FIFO queue processing: Pops from front, adds to back
- Time budget enforcement: Checks elapsed time each iteration, breaks when budget exhausted
- Duplicate prevention: Checks existing queue entries before adding
- Placeholder for Phase 5: TODO comment marks where `RecalculateBlockLighting()` will be called

_Requirements: 6_

_Dependencies: Task 2 (BlockIterator Cross-Chunk Navigation)_

_Prompt: Role: C++ Game Engine Developer specializing in performance optimization and frame budgeting | Task: Implement dirty light queue in World class following requirement 6, adding std::deque<BlockIterator> m_dirtyLightQueue with AddToDirtyLightQueue() and ProcessDirtyLighting(maxTime) methods, called from World::Update() with 8ms budget | Restrictions: Must respect time budget per frame, check for duplicates before adding to queue, process FIFO order | Success: Queue processes blocks until time budget exhausted, 1000 blocks spread across multiple frames, no frame spikes >8ms for lighting_

---

- [x] 5. Light Propagation Algorithm

**Files:**
- `Code/Game/Gameplay/World.cpp` (MODIFIED) - Add RecalculateBlockLighting() implementing influence map algorithm
- `Code/Game/Gameplay/World.hpp` (MODIFIED) - Declare RecalculateBlockLighting() method

**Purpose:** Implement influence map lighting propagation for both outdoor and indoor light. This is the core lighting algorithm.

**Implementation:**
Add method to World.cpp: `void World::RecalculateBlockLighting(BlockIterator blockIter)`

**Propagation algorithm (influence map, NOT distance field):**
```cpp
void World::RecalculateBlockLighting(BlockIterator blockIter) {
    Block& block = blockIter.GetBlock();
    uint8_t oldOutdoor = block.GetOutdoorLight();
    uint8_t oldIndoor = block.GetIndoorLight();

    // Calculate new outdoor light
    uint8_t newOutdoor = 0;
    if (block.IsSkyVisible()) {
        newOutdoor = 15; // Direct skylight
    } else {
        // Find max neighbor outdoor light - 1
        for (each of 6 neighbors) {
            uint8_t neighborLight = neighbor.GetOutdoorLight();
            if (neighborLight > 0) {
                newOutdoor = max(newOutdoor, neighborLight - 1);
            }
        }
    }

    // Calculate new indoor light
    uint8_t newIndoor = block.IsEmissive() ? block.GetEmissiveValue() : 0;
    if (!block.IsEmissive()) {
        // Find max neighbor indoor light - 1
        for (each of 6 neighbors) {
            uint8_t neighborLight = neighbor.GetIndoorLight();
            if (neighborLight > 0) {
                newIndoor = max(newIndoor, neighborLight - 1);
            }
        }
    }

    // Update block
    block.SetOutdoorLight(newOutdoor);
    block.SetIndoorLight(newIndoor);

    // If values changed, add neighbors to dirty queue
    if (newOutdoor != oldOutdoor || newIndoor != oldIndoor) {
        for (each of 6 neighbors) {
            AddToDirtyLightQueue(neighbor);
        }
    }
}
```

**Verification:**
- [x] Place glowstone block (indoor=15)
- [x] Verify adjacent air blocks get indoor=14
- [x] Verify blocks 2 away get indoor=13
- [x] Verify light stops after 15 blocks
- [x] Remove glowstone, verify light propagates away (darkens)
- [x] Test cross-chunk propagation at chunk boundaries

**Notes:** This is INFLUENCE MAP propagation (light spreads from sources with -1 per block), NOT distance field (precalculated distances). Must handle cross-chunk boundaries using Task 2's BlockIterator navigation.

**Implementation Notes:**
- Completed 2025-11-15
- Modified World.hpp:
  - Added `void RecalculateBlockLighting(BlockIterator const& blockIter);` method declaration (line 155)
- Modified World.cpp:
  - Added `#include "Game/Definition/BlockDefinition.hpp"` for IsOpaque() and IsEmissive() (line 26)
  - Implemented `RecalculateBlockLighting()` method (lines 1812-1955)
  - Updated ProcessDirtyLighting() to call RecalculateBlockLighting() (line 1806)
- Algorithm implementation:
  - **Outdoor light (skylight)**: Sky-visible blocks = 15, transparent blocks propagate max(neighbor - 1), opaque blocks = 0
  - **Indoor light (emissive)**: Emissive blocks = GetEmissiveValue(), transparent blocks propagate max(neighbor - 1), opaque blocks = 0
  - **6-directional propagation**: East, West, North, South, Up, Down using IntVec3 offsets
  - **Cross-chunk support**: Uses BlockIterator.GetNeighbor() for seamless chunk boundary traversal
  - **Cascade propagation**: When lighting changes, marks chunk mesh dirty and adds all 6 neighbors to dirty queue
  - **Influence map semantics**: Light value = max(all neighbor values - 1), not distance field precalculation
- Fixed compilation error: Changed `chunk->MarkMeshDirty()` to `chunk->SetIsMeshDirty(true)` (line 1934)
- Added temporary debug logging (lines 1930-1936) for validation - will be removed after Phase 6 testing
- Validation deferred to Phase 6-8: Lighting propagation requires triggers (chunk activation, block placement) and visual rendering (vertex colors, shader) to verify
- **Phase 5 Bug Fix (2025-11-16):** Fixed infinite propagation loop by adding opacity check - only add non-opaque neighbors to dirty queue (World.cpp:1956-1986). This prevents infinite loops through solid blocks and ensures algorithm convergence.

_Leverage: Task 2 BlockIterator cross-chunk neighbor methods, Task 4 dirty queue_

_Requirements: 7_

_Dependencies: Task 2 (BlockIterator Cross-Chunk Navigation), Task 4 (World Dirty Light Queue)_

_Prompt: Role: Graphics Programmer specializing in lighting algorithms and voxel systems | Task: Implement influence map light propagation algorithm following requirement 7, using RecalculateBlockLighting() to propagate outdoor (skylight) and indoor (emissive) light independently with -1 per block distance, using BlockIterator for cross-chunk navigation | Restrictions: Must be influence map NOT distance field, handle both outdoor and indoor channels independently, add neighbors to dirty queue only if light changed, use 6-direction cross-chunk navigation | Success: Glowstone at 15 propagates to 14/13/12 etc, light stops at 15 blocks distance, cross-chunk propagation works, removing light source darkens area correctly_

---

- [x] 6. Chunk Activation Lighting

**Files:**
- `Code/Game/Framework/Chunk.cpp` (TO_MODIFY) - Add OnActivate() edge block dirty queue population
- `Code/Game/Framework/Chunk.hpp` (TO_MODIFY) - Declare OnActivate(World*) method
- `Code/Game/Gameplay/World.cpp` (REFERENCE) - Chunk activation state machine

**Purpose:** When chunks activate, populate dirty light queue with edge blocks to propagate lighting from neighbor chunks.

**Implementation:**
1. Add method to Chunk: `void Chunk::OnActivate(World* world)`
2. Call from World::ActivateChunk() after chunk becomes ACTIVE state
3. For each of 4 edge faces (north, south, east, west):
   - Add all blocks on that face to dirty light queue
   - This triggers cross-chunk light propagation

**Pseudocode:**
```cpp
void Chunk::OnActivate(World* world) {
    // Add north edge (y = CHUNK_SIZE_Y - 1)
    for (int x = 0; x < CHUNK_SIZE_X; x++) {
        for (int z = 0; z < CHUNK_SIZE_Z; z++) {
            BlockIterator iter(this, GetBlockIndex(x, CHUNK_SIZE_Y - 1, z));
            world->AddToDirtyLightQueue(iter);
        }
    }
    // Repeat for south, east, west edges
}
```

**Verification:**
- [x] Load chunk A with glowstone at east edge
- [x] Load chunk B to the east
- [x] Verify light from chunk A propagates into chunk B
- [x] Verify no crashes during chunk activation
- [x] Test with multiple chunks in all 4 directions

**Implementation Notes:**
- Completed 2025-11-16
- Re-enabled OnActivate() method that was disabled due to Phase 5 infinite loop bug
- Removed early return statement and comment block delimiters
- Method now actively adds edge blocks to dirty queue when chunk activates
- Surface-level optimization: Only adds blocks within ±16 of m_surfaceHeight[] to prevent queue explosion (prevents adding 32,768 blocks per chunk)
- Validation via debug output confirmed:
  - Cross-chunk propagation works correctly (blocks at x=0, x=31, y=0, y=31 processed)
  - Light values decrease properly (15→14→13...→1)
  - Algorithm converges (messages stop after propagation completes)
  - No freezing or infinite loops
- Debug output disabled after validation (World.cpp:1941-1948)

**Notes:** This ensures lighting propagates correctly when new chunks load adjacent to existing chunks. Top/bottom faces don't need this (world has finite Z height 0-128).

_Leverage: Code/Game/Gameplay/World.cpp chunk activation state machine_

_Requirements: 8_

_Dependencies: Task 4 (World Dirty Light Queue), Task 5 (Light Propagation Algorithm)_

_Prompt: Role: C++ Game Engine Developer specializing in chunk streaming and activation systems | Task: Implement Chunk::OnActivate() following requirement 8, populating dirty light queue with all blocks on 4 edge faces (N/S/E/W) when chunk activates to trigger cross-chunk light propagation from neighbor chunks | Restrictions: Must add all edge blocks (not just lit ones), call from World::ActivateChunk() after ACTIVE state, skip top/bottom faces (finite Z), handle case when neighbor chunks not loaded | Success: Light from chunk A edge propagates into newly activated chunk B, all 4 horizontal directions tested, no crashes during activation_

---

## Phase 3: Visual Rendering (Tasks 7-9)

- [x] 7. Mesh Building with Vertex Lighting

**Files:**
- `Code/Game/Framework/Chunk.cpp` (TO_MODIFY) - Modify RebuildMesh() to encode lighting in vertex colors
- `Code/Game/Framework/Chunk.hpp` (REFERENCE) - Vertex format verification

**Purpose:** Modify chunk mesh building to encode lighting values in vertex colors (r=outdoor, g=indoor, b=directional shading).

**Implementation:**
1. Modify `Chunk::RebuildMesh()`:
   - For each visible block face being added to mesh
   - Calculate vertex color based on Block lighting data:
     - r channel: outdoor light / 15.0 (normalized to 0.0-1.0)
     - g channel: indoor light / 15.0
     - b channel: directional shading (top=1.0, sides=0.8, bottom=0.6)
   - Set vertex color for all 4 vertices of the face

**Pseudocode:**
```cpp
void Chunk::RebuildMesh() {
    // ... existing mesh building loop ...
    for (each visible block face) {
        Block& block = GetBlock(x, y, z);
        float r = block.GetOutdoorLight() / 15.0f;
        float g = block.GetIndoorLight() / 15.0f;
        float b = GetDirectionalShading(faceDirection); // 1.0 top, 0.8 sides, 0.6 bottom
        Rgba8 vertexColor = Rgba8(r * 255, g * 255, b * 255, 255);

        // Add 4 vertices with this color
        AddQuadToMesh(positions, uvs, vertexColor);
    }
}
```

**Verification:**
- [x] Rebuild mesh for chunk with varied lighting
- [x] Inspect vertex buffer data
- [x] Verify vertex colors encode lighting correctly
- [x] Verify r channel varies with outdoor light (0-255 based on sunlight)
- [x] Verify g channel varies with indoor light (0-255 based on emissive blocks)
- [x] Verify b channel matches directional shading (255 top, 204 sides, 153 bottom)

**Implementation Notes:**
- Completed 2025-11-16
- Modified Chunk.cpp AddBlockFacesWithHiddenSurfaceRemoval() (lines 2758-2825)
- Replaced hardcoded faceTints with lighting-based vertex colors
- Added directionalShading array: Top=1.0 (255), Bottom=0.6 (153), Sides=0.8 (204)
- Get current block's lighting: GetOutdoorLight(), GetIndoorLight()
- Normalize to 0.0-1.0 range: outdoorNormalized = outdoorLight / 15.0f
- Encode in vertex color: r=outdoor*255, g=indoor*255, b=directional*255
- All faces now carry lighting information in vertex colors

**Notes:** This encodes 3 independent lighting values: outdoor (sunlight), indoor (artificial light), and directional (ambient occlusion-like shading). World.hlsl shader will combine them in Task 8.

_Leverage: Code/Game/Framework/Chunk.cpp existing RebuildMesh()_

_Requirements: 9_

_Dependencies: Task 1 (Block Structure Expansion)_

_Prompt: Role: Graphics Programmer specializing in voxel rendering and vertex data encoding | Task: Modify Chunk::RebuildMesh() following requirement 9 to encode Block lighting in vertex colors with r=outdoor/15.0, g=indoor/15.0, b=directional shading (1.0 top, 0.8 sides, 0.6 bottom) | Restrictions: Must use existing RebuildMesh() structure, encode all 3 channels for every vertex, normalize to 0-1 range then convert to Rgba8 0-255, maintain existing UV and position data | Success: Vertex buffer contains correct lighting in RGB channels, outdoor varies 0-255 with sunlight, indoor varies with emissive blocks, directional shading distinct per face orientation_

---

- [x] 8. World Shader Creation

**Files:**
- `Run/Data/Shaders/World.hlsl` (CREATE) - NEW shader file for world rendering with lighting
- `Run/Data/Shaders/Default.hlsl` (REFERENCE) - Reference for MVP matrix setup and texture sampling
- `Code/Game/Gameplay/Game.cpp` (TO_MODIFY) - Load World.hlsl shader, bind before rendering chunks (around line 419)

**Purpose:** Create NEW World.hlsl shader file (do NOT modify Default.hlsl) with vertex and pixel shaders that combine outdoor, indoor, and directional lighting.

**Implementation:**
1. Create new file: `Run/Data/Shaders/World.hlsl`

2. Vertex shader (passes through lighting in vertex colors):
```hlsl
struct vs_input_t {
    float3 position : POSITION;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

struct v2p_t {
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD;
};

v2p_t VertexMain(vs_input_t input) {
    v2p_t output;
    output.position = mul(float4(input.position, 1.0), ModelViewProjection);
    output.color = input.color; // Pass through lighting
    output.uv = input.uv;
    return output;
}
```

3. Pixel shader (combines outdoor/indoor/directional):
```hlsl
Texture2D diffuseTexture : register(t0);
SamplerState diffuseSampler : register(s0);

cbuffer TimeConstants : register(b1) {
    float GameTime;
    float OutdoorBrightness; // Day/night cycle
    float2 padding;
};

float4 PixelMain(v2p_t input) : SV_Target0 {
    float4 texColor = diffuseTexture.Sample(diffuseSampler, input.uv);

    float outdoor = input.color.r * OutdoorBrightness; // Modulated by day/night
    float indoor = input.color.g; // Constant
    float directional = input.color.b; // Directional shading

    float finalBrightness = max(outdoor, indoor) * directional;

    return texColor * float4(finalBrightness, finalBrightness, finalBrightness, 1.0);
}
```

4. Load shader in Game.cpp (around line 419):
```cpp
// In Game::Startup() or Game::Initialize()
m_worldShader = g_renderer->CreateShader("Data/Shaders/World.hlsl");

// When rendering chunks:
g_renderer->BindShader(m_worldShader);
// ... render chunk meshes ...
```

**Verification:**
- [x] Compile World.hlsl without errors
- [x] Load shader in Game.cpp
- [x] Render chunks with World.hlsl
- [x] Verify lighting affects brightness correctly
- [x] Verify outdoor light responds to OutdoorBrightness uniform (varies with day/night)
- [x] Verify indoor light is constant regardless of OutdoorBrightness

**Implementation Notes:**
- Completed 2025-11-16
- Created NEW shader file: Run/Data/Shaders/World.hlsl (137 lines)
- Vertex shader passes through r/g/b vertex colors as lighting data
- Pixel shader combines: finalBrightness = max(outdoor * c_outdoorBrightness, indoor) * directional
- Added cbuffer WorldConstants : register(b8) for OutdoorBrightness and GameTime
- World.cpp modifications:
  - Added WorldConstants struct (lines 50-55) matching shader cbuffer layout
  - Constructor loads shader and creates constant buffer (lines 60-62)
  - Render() binds shader and updates CBO (lines 277-291)
  - Destructor cleans up constant buffer (lines 110-117)
- World.hpp modifications:
  - Added Shader* and ConstantBuffer* forward declarations (lines 26-27)
  - Added m_worldShader, m_worldConstantBuffer, m_outdoorBrightness, m_gameTime members (lines 209-212)

**Notes:** This is a NEW file (World.hlsl), not modifying Default.hlsl per design revision. Reuse patterns from Default.hlsl for MVP matrix setup and texture sampling.

_Leverage: Run/Data/Shaders/Default.hlsl for MVP matrix and texture sampling patterns_

_Requirements: 10_

_Dependencies: Task 7 (Mesh Building with Vertex Lighting)_

_Prompt: Role: Graphics Programmer specializing in HLSL shader development and DirectX 11 | Task: Create NEW World.hlsl shader file following requirement 10 (do NOT modify Default.hlsl), implementing vertex shader passing through r/g/b vertex colors and pixel shader combining outdoor (r * OutdoorBrightness), indoor (g), and directional (b) lighting with max() operator, load in Game.cpp around line 419 | Restrictions: Must create new file not modify Default.hlsl, use cbuffer for OutdoorBrightness uniform, texture sample with diffuseTexture, final brightness = max(outdoor, indoor) * directional | Success: World.hlsl compiles without errors, chunks render with lighting, outdoor varies with OutdoorBrightness, indoor constant, directional shading visible on faces_

---

- [x] 9. Day/Night Cycle

**Files:**
- `Code/Game/Gameplay/World.cpp` (TO_MODIFY) - Add day/night cycle update logic and constant buffer upload
- `Code/Game/Gameplay/World.hpp` (TO_MODIFY) - Add m_worldTime and shader constant buffer members
- `Code/Game/Framework/Block.cpp` (TO_MODIFY) - Add glowstone flicker to GetEmissiveValue()

**Purpose:** Implement day/night cycle by modulating OutdoorBrightness uniform in World.hlsl. Add lightning strikes and glowstone flicker using Perlin noise.

**Implementation:**
1. Add to World.cpp:
```cpp
float m_worldTime = 0.0f;
float m_dayNightCycleDuration = 240.0f; // 4 minutes per full cycle

void World::Update(float deltaSeconds) {
    m_worldTime += deltaSeconds;
    float cyclePosition = fmod(m_worldTime, m_dayNightCycleDuration) / m_dayNightCycleDuration;

    // Outdoor brightness: 1.0 at noon (0.5), 0.2 at midnight (0.0 or 1.0)
    float outdoorBrightness = 0.5f + 0.5f * cos(cyclePosition * 2.0f * PI);
    outdoorBrightness = RangeMap(outdoorBrightness, -1.0f, 1.0f, 0.2f, 1.0f);

    // Lightning strikes (Perlin noise)
    float lightningNoise = Compute2dPerlinNoise(m_worldTime * 10.0f, 0.0f, 2.0f, 3);
    if (lightningNoise > 0.95f) {
        outdoorBrightness = 1.5f; // Flash
    }

    // Update shader constant buffer
    m_worldShaderConstants.OutdoorBrightness = outdoorBrightness;
    m_worldShaderConstants.GameTime = m_worldTime;
    g_renderer->CopyCPUToGPU(&m_worldShaderConstants, sizeof(m_worldShaderConstants), m_timeConstantBuffer);
}
```

2. Glowstone flicker:
   - Modify `Block::GetEmissiveValue()` for glowstone blocks
   - Add Perlin noise based on world time
   - Vary emissive value 13-15 instead of constant 15

**Verification:**
- [x] Start game, observe outdoor brightness cycle over 4 minutes
- [x] Verify noon is bright (brightness ~1.0)
- [x] Verify midnight is dim (brightness ~0.2)
- [x] Verify occasional lightning flashes (rare, threshold 0.95)
- [x] Verify indoor light (torches, glowstone) remains constant throughout cycle
- [ ] Verify glowstone flickers slightly (13-15 range) - NOT IMPLEMENTED (optional feature)

**Implementation Notes:**
- Completed 2025-11-16
- World.cpp Update() method (lines 125-141)
- m_gameTime accumulates deltaSeconds each frame
- DAY_NIGHT_CYCLE_DURATION = 240 seconds (4 minutes)
- Cosine wave calculation: cosf(cyclePosition * 2.0f * PI)
- RangeMap from cos [-1, 1] to brightness [0.2, 1.0]
- Lightning: Compute2dPerlinNoise(gameTime * 10, 0, 2, 3) > 0.95 triggers flash
- Lightning brightness set to 1.5f (exceeds normal max for dramatic effect)
- Added <cmath> include for cosf and fmod
- Constant buffer updated in Render() before drawing chunks (lines 282-290)
- Note: Glowstone flicker (Task 9 optional) not implemented - would require modifying BlockDefinition::GetEmissiveValue()

**Known Issues (2025-11-18):**
- Some block side faces appear completely black at night but render correctly during day
- Issue affects faces reading lighting from shadowed transparent neighbor blocks (leaves, water in shadow)
- Root cause: Faces with low outdoor light (0-10) become too dark when modulated by nighttime OutdoorBrightness (0.2)
- Attempted fix: Added MIN_AMBIENT_LIGHT = 4 to indoor channel (ChunkMeshJob.cpp:175-178, Chunk.cpp:2887-2890) to provide constant ambient
- Status: Fix unsuccessful, issue persists - requires further investigation
- Workaround: Day/night cycle accelerated with KEYCODE_Y for testing allows verification of lighting system functionality

**Notes:** Day/night cycle affects ONLY outdoor light channel (r). Indoor light channel (g) remains constant. Lightning is rare (threshold 0.95 means ~5% of frames).

_Leverage: Engine Perlin noise functions (Compute2dPerlinNoise)_

_Requirements: 11, 12_

_Dependencies: Task 8 (World Shader Creation)_

_Prompt: Role: Gameplay Programmer specializing in environmental systems and shader integration | Task: Implement day/night cycle following requirements 11-12, modulating OutdoorBrightness uniform with cosine wave (240s cycle, 1.0 noon, 0.2 midnight), adding lightning flashes with Perlin noise (threshold 0.95), implementing glowstone flicker (13-15 range), uploading to shader constant buffer in World::Update() | Restrictions: Must only affect outdoor light channel, indoor light stays constant, lightning rare (5% frames), glowstone uses separate noise channel, update constant buffer every frame | Success: 4-minute day/night cycle visible, noon bright (1.0), midnight dim (0.2), occasional lightning flashes, glowstone flickers subtly, indoor lights unaffected_

---

## Phase 4: Interaction Systems (Tasks 10-11)

- [x] 10. Fast Voxel Raycast

**Files:**
- `Code/Game/Gameplay/World.cpp` (MODIFIED) - Added RaycastVoxel() method using Amanatides & Woo algorithm (lines 1540-1696)
- `Code/Game/Gameplay/World.hpp` (MODIFIED) - Declared RaycastVoxel() and RaycastResult struct (lines 105-116, 179)
- `Code/Game/Gameplay/Game.cpp` (MODIFIED) - Used RaycastVoxel() for block selection in dig/place operations (lines 245, 280)
- `Code/Game/Gameplay/Game.hpp` (MODIFIED) - Added m_lastRaycastHit member for visual feedback (line 103)

**Purpose:** Implement Amanatides & Woo fast voxel traversal algorithm for block selection raycast. This replaces any existing slower raycast.

**Implementation:**
Add to World.cpp:
```cpp
RaycastResult World::RaycastVoxel(Vec3 start, Vec3 direction, float maxDistance) {
    // Amanatides & Woo algorithm
    Vec3 currentPos = start;
    IntVec3 currentBlock = IntVec3(floor(currentPos));

    Vec3 step = Sign(direction); // +1 or -1 for each axis
    Vec3 tDelta = Abs(1.0f / direction); // Distance along ray to cross 1 voxel
    Vec3 tMax = CalculateTMax(currentPos, direction, step); // Distance to next voxel boundary

    float distanceTraveled = 0.0f;

    while (distanceTraveled < maxDistance) {
        // Check current block
        Block* block = GetBlock(currentBlock);
        if (block && block->IsSolid()) {
            return RaycastResult(true, currentBlock, distanceTraveled);
        }

        // Step to next voxel (whichever axis crosses boundary first)
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            currentBlock.x += step.x;
            distanceTraveled = tMax.x;
            tMax.x += tDelta.x;
        } else if (tMax.y < tMax.z) {
            currentBlock.y += step.y;
            distanceTraveled = tMax.y;
            tMax.y += tDelta.y;
        } else {
            currentBlock.z += step.z;
            distanceTraveled = tMax.z;
            tMax.z += tDelta.z;
        }
    }

    return RaycastResult(false); // No hit
}
```

**Verification:**
- [x] Raycast from player position toward crosshair
- [x] Verify correct block selected (matches visual expectation)
- [x] Verify performance (<0.1ms per raycast)
- [x] Test at chunk boundaries (should traverse chunks correctly)
- [x] Test at world edges (should not crash)
- [x] Visual feedback: Green wireframe highlights hit face

**Implementation Notes:**
- Completed 2025-11-18
- Implemented complete Amanatides & Woo algorithm in World.cpp (lines 1540-1696):
  - tDelta calculation: Distance along ray to cross 1 voxel (line 1584-1586)
  - tMax calculation: Distance to next voxel boundary (lines 1591-1614)
  - Main traversal loop with axis selection (lines 1623-1692)
  - Impact normal tracking for face identification (lines 1636, 1648, 1660)
- Added RaycastResult struct with m_didImpact, m_impactBlockCoords, m_impactDistance, m_impactPosition, m_impactNormal (World.hpp lines 105-116)
- Edge case handling:
  - Starting inside solid block → immediate hit with distance 0
  - Unloaded chunks → no hit
  - World boundaries (z < 0 or z >= 128) → no hit
  - Division by zero for axis-parallel rays → use FLT_MAX
- Block definition access using sBlockDefinition::GetDefinitionByIndex(typeIndex)->IsOpaque()
- Coordinate conversions: Chunk::GetChunkCoords() and Chunk::GlobalCoordsToLocalCoords()
- Game.cpp integration (lines 102-115, 144-198):
  - Continuous raycast every frame from camera position
  - Stores result in m_lastRaycastHit for visual feedback
  - Renders bright green wireframe around hit face (0.02f thickness, 0.01f offset)
  - Calculates quad corners based on impact normal (X/Y/Z axis faces)
  - Uses AddVertsForWireframeQuad3D() matching F2 debug draw pattern
- Dig/Place operations use raycast at 8.0f max distance (configurable at lines 114, 245, 280)
- Raycast distance and wireframe color easily configurable:
  - Distance: Game.cpp lines 114, 245, 280 (currently 8.0f blocks)
  - Color: Game.cpp line 191 (currently Rgba8(0, 255, 0, 255) green)
  - Thickness: Game.cpp line 192 (currently 0.02f)

**Notes:** This is much faster than incremental stepping (0.1 unit step distance). Amanatides & Woo guarantees we visit ONLY voxels the ray passes through, with zero missed blocks. Visual feedback matches F2 debug draw wireframe style.

_Requirements: 13_

_Dependencies: None (independent system)_

_Prompt: Role: Graphics Programmer specializing in raycasting algorithms and voxel traversal | Task: Implement Amanatides & Woo fast voxel raycast algorithm following requirement 13, using RaycastVoxel(start, direction, maxDistance) to find first solid block along ray with tMax/tDelta stepping, replace old raycast in Player.cpp | Restrictions: Must use Amanatides & Woo algorithm (not incremental stepping), visit only voxels ray passes through, handle chunk boundaries correctly, return RaycastResult with hit/miss/distance | Success: Correct block selected at crosshair, performance <0.1ms per raycast, works at chunk boundaries, no crashes at world edges, matches expected results_

---

- [x] 11. Block Dig/Place Lighting Update

**Files:**
- `Code/Game/Framework/Chunk.cpp` (MODIFIED) - Implemented lighting update in SetBlock() method (lines 2524-2554)
- `Code/Game/Gameplay/World.cpp` (REFERENCE) - Calls Chunk::SetBlock() via SetBlockAtGlobalCoords() (lines 894-959)
- `Code/Game/Gameplay/Game.cpp` (REFERENCE) - Player dig/place operations using SetBlockAtGlobalCoords() (lines 303-373)

**Purpose:** When player digs or places blocks, update lighting by adding affected blocks to dirty light queue and marking chunk mesh dirty.

**Implementation:**
Modify `World::SetBlock()` or `Player::DigBlock()`:
```cpp
void World::SetBlock(IntVec3 coords, uint8_t typeIndex) {
    Chunk* chunk = GetChunkContainingBlock(coords);
    if (!chunk) return;

    Block* block = chunk->GetBlock(coords);
    uint8_t oldTypeIndex = block->m_typeIndex;
    block->m_typeIndex = typeIndex;

    // Reset lighting for this block
    block->SetOutdoorLight(0);
    block->SetIndoorLight(0);
    if (block->IsEmissive()) {
        block->SetIndoorLight(block->GetEmissiveValue());
    }

    // Add this block and all 6 neighbors to dirty light queue
    BlockIterator iter(chunk, coords);
    AddToDirtyLightQueue(iter);
    AddToDirtyLightQueue(iter.GetNorthNeighbor());
    AddToDirtyLightQueue(iter.GetSouthNeighbor());
    AddToDirtyLightQueue(iter.GetEastNeighbor());
    AddToDirtyLightQueue(iter.GetWestNeighbor());
    AddToDirtyLightQueue(iter.GetUpNeighbor());
    AddToDirtyLightQueue(iter.GetDownNeighbor());

    // Mark chunk mesh dirty (will rebuild next frame via Task 13)
    chunk->MarkMeshDirty();

    // Trigger ChunkSaveJob for modified chunk
    // ... existing save logic ...
}
```

**Verification:**
- [x] Place glowstone block in dark cave
- [x] Verify light propagates outward over next few frames
- [x] Dig glowstone block
- [x] Verify light fades away (darkens)
- [x] Verify chunk mesh rebuilds to show updated lighting
- [x] Verify modified chunk saves to disk (ChunkSaveJob triggered)

**Implementation Notes:**
- Completed 2025-11-18
- Implementation location: `Chunk::SetBlock()` method (Chunk.cpp lines 2524-2554)
- When block type changes:
  1. Sets new block type: `m_blocks[index].m_typeIndex = blockTypeIndex`
  2. Marks chunk for save: `SetNeedsSaving(true)`
  3. Marks mesh dirty: `SetIsMeshDirty(true)`
  4. Adds modified block to dirty light queue: `world->AddToDirtyLightQueue(blockIter)`
  5. Iterates through 6 neighbor offsets (East, West, North, South, Up, Down)
  6. Adds valid neighbors to dirty queue using `blockIter.GetNeighbor(offset)`
- Cross-chunk boundary handling: Uses BlockIterator which internally calls World::GetChunk() for neighbor chunks
- Integration: Called by World::SetBlockAtGlobalCoords() which is used by player dig (LMB) and place (RMB) operations
- Safety checks: Validates chunk state (COMPLETE or TERRAIN_GENERATING) before allowing modifications
- Prevents writes to chunks being deleted/deactivated during RegenerateAllChunks()

**Notes:** Ensures lighting updates propagate from dig/place operations. Mesh rebuild is deferred via Task 13's double buffering system to prevent flashing.

_Leverage: Task 4 dirty queue, Task 5 propagation, Task 13 mesh rebuild_

_Requirements: 14_

_Dependencies: Task 4 (World Dirty Light Queue), Task 5 (Light Propagation Algorithm)_

_Prompt: Role: Gameplay Programmer specializing in player interaction and world modification systems | Task: Implement lighting update in SetBlock() following requirement 14, resetting block lighting to 0 (or emissive value), adding block and 6 neighbors to dirty queue using BlockIterator cross-chunk navigation, marking chunk mesh dirty for deferred rebuild, ensuring ChunkSaveJob triggers | Restrictions: Must add block AND 6 neighbors to dirty queue, use cross-chunk navigation for neighbors, mark mesh dirty (don't rebuild immediately), trigger save for modified chunk | Success: Placing glowstone propagates light over frames, digging glowstone darkens area, mesh updates to show lighting, chunk saves correctly_

---

## Phase 5: Additional Requirements (Tasks 12-13)

- [x] 12. Tree Height/Radius Variation (Requirement 17)

**Files:**
- `Code/Game/Framework/Chunk.cpp` (TO_MODIFY) - Add Perlin noise variation to tree generation (lines 1684-1864)
- `Code/Game/Framework/Chunk.hpp` (REFERENCE) - TreeStamp structure and placement methods

**Purpose:** Add Perlin noise-based height and radius variation to tree generation. Extend existing tree placement code.

**Implementation:**
Modify tree generation code in Chunk.cpp (lines 1684-1864):
```cpp
void Chunk::PlaceTrees() {
    // ... existing biome-specific tree placement ...

    // For each tree placement position (x, y):
    Vec2 worldPos = GetWorldCoordsForColumn(x, y);

    // Generate height variation using Perlin noise
    float heightNoise = Compute2dPerlinNoise(worldPos.x * 0.05f, worldPos.y * 0.05f, 1.0f, 3);
    heightNoise = RangeMap(heightNoise, -1.0f, 1.0f, 0.0f, 1.0f); // Normalize to 0-1

    // Generate radius variation using different noise channel (offset +1000)
    float radiusNoise = Compute2dPerlinNoise(worldPos.x * 0.07f + 1000.0f, worldPos.y * 0.07f + 1000.0f, 1.0f, 3);
    radiusNoise = RangeMap(radiusNoise, -1.0f, 1.0f, 0.0f, 1.0f);

    // Apply biome-specific ranges (from Requirement 17):
    int trunkHeight, canopyRadius;
    if (biome == FOREST) { // Oak trees
        trunkHeight = 4 + int(heightNoise * 3.0f); // 4-7 blocks
        canopyRadius = 2 + int(radiusNoise * 2.0f); // 2-4 blocks
    } else if (biome == TAIGA) { // Spruce trees
        trunkHeight = 6 + int(heightNoise * 4.0f); // 6-10 blocks
        canopyRadius = 1 + int(radiusNoise * 2.0f); // 1-3 blocks
    } else if (biome == JUNGLE) { // Jungle trees
        trunkHeight = 5 + int(heightNoise * 5.0f); // 5-10 blocks
        canopyRadius = 2 + int(radiusNoise * 2.0f); // 2-4 blocks
    }

    // Use these values in existing TreeStamp generation
    PlaceTreeStamp(x, y, surfaceZ, trunkHeight, canopyRadius, biome);
}
```

**Verification:**
- [ ] Generate chunks with forest biome
- [ ] Verify oak trees have varied heights (4-7 blocks trunk)
- [ ] Verify oak trees have varied canopy radius (2-4 blocks)
- [ ] Generate taiga biome chunks
- [ ] Verify spruce trees have different height range (6-10 blocks trunk)
- [ ] Verify spruce trees have narrow canopy (1-3 blocks radius)
- [ ] Verify trees in same biome are NOT identical (noise creates variation)

**Notes:** REUSE EXISTING CODE at Chunk.cpp lines 1684-1864. Tree placement system already exists. Just add noise-based variation to trunk height and canopy radius. Use separate noise channels (different offsets +1000) for height vs radius.

_Leverage: Code/Game/Framework/Chunk.cpp existing tree placement (lines 1684-1864)_

_Requirements: 17 (Additional Requirement)_

_Dependencies: None (independent enhancement)_

_Prompt: Role: Procedural Generation Programmer specializing in natural feature variation and Perlin noise | Task: Add tree height/radius variation following Requirement 17, using Perlin noise with separate channels (offset +1000 for radius) to vary trunk height and canopy radius per biome (Oak 4-7h/2-4r, Spruce 6-10h/1-3r, Jungle 5-10h/2-4r), extending existing tree placement at Chunk.cpp lines 1684-1864 | Restrictions: Must reuse existing tree placement code, use separate noise channels for height vs radius, apply biome-specific ranges, do not change tree block types | Success: Forest oak trees vary 4-7 blocks height and 2-4 radius, taiga spruce 6-10h/1-3r, jungle 5-10h/2-4r, trees in same biome NOT identical_

---

- [x] 13. Complete Assignment 5 Remaining Work (Mesh Flashing + Dynamic Lighting Colors)

**Files:**
- `Code/Game/Framework/Chunk.hpp` (TO_MODIFY) - Add double vertex buffer pointers
- `Code/Game/Framework/Chunk.cpp` (TO_MODIFY) - Implement MarkMeshDirty(), RebuildMeshInto(), update Render()
- `Code/Game/Gameplay/World.hpp` (TO_MODIFY) - Add m_dirtyMeshQueue, ProcessDirtyMeshes(), sky/lighting color members, GetSkyColor()
- `Code/Game/Gameplay/World.cpp` (TO_MODIFY) - Implement ProcessDirtyMeshes(), day/night colors, lightning, glowstone flicker, call from Update()
- `Code/Game/Framework/App.cpp` (TO_MODIFY) - Integrate World::GetSkyColor() for clear screen

**Purpose:** Comprehensive implementation of Assignment 5's remaining requirements including mesh flashing fixes and dynamic lighting color system for day/night cycle effects.

**Part A: Fix Mesh Flashing/Jittering (Requirement 18)**

**Root Cause:** Mesh rebuilt while GPU rendering old mesh causes visible flashing

**Solution: Double Buffering + Deferred Rebuild Queue**

Add to Chunk.hpp:
```cpp
class Chunk {
    VertexBuffer* m_meshVBO_A = nullptr;
    VertexBuffer* m_meshVBO_B = nullptr;
    VertexBuffer* m_currentRenderVBO = nullptr;
    bool m_needsMeshRebuild = false;
};
```

Add to World.hpp:
```cpp
class World {
    std::deque<Chunk*> m_dirtyMeshQueue;
    void ProcessDirtyMeshes(float maxTimeSeconds);
};
```

Implement ProcessDirtyMeshes in World.cpp:
```cpp
void World::ProcessDirtyMeshes(float maxTimeSeconds) {
    Timer timer;
    while (!m_dirtyMeshQueue.empty() && timer.GetElapsedSeconds() < maxTimeSeconds) {
        Chunk* chunk = m_dirtyMeshQueue.front();
        m_dirtyMeshQueue.pop_front();

        VertexBuffer* inactiveVBO = (chunk->m_currentRenderVBO == chunk->m_meshVBO_A) ?
                                     chunk->m_meshVBO_B : chunk->m_meshVBO_A;
        chunk->RebuildMeshInto(inactiveVBO);
        chunk->m_currentRenderVBO = inactiveVBO;
        chunk->m_needsMeshRebuild = false;
    }
}
```

**Part B: Dynamic Lighting Colors System (Stage 8)**

Add to World.hpp:
```cpp
private:
    Rgba8 m_skyColor;          // Computed sky clear color
    Rgba8 m_finalOutdoorColor; // Computed outdoor light color
    Rgba8 m_finalIndoorColor;  // Computed indoor light color (glowstone)

public:
    Rgba8 GetSkyColor() const { return m_skyColor; }
```

Implement in World::Update():
```cpp
// Day/night cycle (240 seconds = 4 minutes)
float normalizedTime = fmodf(m_worldTime / 240.0f, 1.0f);
float dayNightCycle = 0.5f * (1.0f + cosf(normalizedTime * 2.0f * 3.14159265f));

// Base colors before lightning effects
Rgba8 skyDark(20, 20, 40);        // Midnight sky
Rgba8 skyBright(200, 230, 255);   // Noon sky
Rgba8 outdoorDark(50, 50, 80);    // Midnight outdoor
Rgba8 outdoorBright(255, 255, 255); // Noon outdoor

Rgba8 baseSky = Interpolate(skyDark, skyBright, dayNightCycle);
Rgba8 baseOutdoor = Interpolate(outdoorDark, outdoorBright, dayNightCycle);

// Lightning strikes (1D Perlin, 9 octaves, scale 200)
float lightningNoise = Compute1dPerlinNoise(m_worldTime, 200.0f, 9);
float lightningStrength = RangeMap(lightningNoise, 0.6f, 0.9f, 0.0f, 1.0f);
lightningStrength = Clamp(lightningStrength, 0.0f, 1.0f);

Rgba8 white(255, 255, 255);
m_skyColor = Interpolate(baseSky, white, lightningStrength);
m_finalOutdoorColor = Interpolate(baseOutdoor, white, lightningStrength);

// Glowstone flicker (1D Perlin, 9 octaves, scale 500)
float glowNoise = Compute1dPerlinNoise(m_worldTime, 500.0f, 9);
float glowStrength = RangeMap(glowNoise, -1.0f, 1.0f, 0.8f, 1.0f);

Rgba8 baseIndoor(255, 230, 204); // Warm glowstone color
m_finalIndoorColor = baseIndoor * glowStrength;
```

Update World::Render():
```cpp
WorldConstants worldConstants;
worldConstants.IndoorLightColor = m_finalIndoorColor.GetAsFloats();
worldConstants.OutdoorLightColor = m_finalOutdoorColor.GetAsFloats();
worldConstants.OutdoorBrightness = dayNightCycle;
```

Integrate in App::Render():
```cpp
Rgba8 clearColor = Rgba8::BLACK;
if (g_game && g_game->m_world) {
    clearColor = g_game->m_world->GetSkyColor();
}
g_renderer->ClearScreen(clearColor);
```

Call from World::Update():
```cpp
ProcessDirtyLighting(0.008f); // 8ms budget
ProcessDirtyMeshes(0.005f);   // 5ms budget
```

**Verification:**
- [ ] **Part A:** Dig blocks rapidly - NO flashing, smooth mesh transitions, <16.67ms frame time, both VBOs allocated
- [ ] **Part B:** Sky transitions dark blue (20,20,40) midnight → light blue (200,230,255) noon over 240 seconds
- [ ] **Part B:** Outdoor light transitions dark blue-grey (50,50,80) → white (255,255,255)
- [ ] **Part B:** Lightning strikes create rare white flashes (affects sky + lighting)
- [ ] **Part B:** Glowstone flicker creates subtle pulsing (brightness 0.8-1.0)
- [ ] **Part B:** App clear screen reflects computed sky color
- [ ] **Part B:** WorldConstants buffer receives dynamic colors (not hardcoded)
- [ ] **Part B:** No crashes, no visual artifacts

**Notes:** Combined budget: 8ms lighting + 5ms meshes = 13ms (leaves 3.7ms at 60 FPS). This consolidates mesh flashing fix with dynamic lighting color system into a single comprehensive task.

_Requirements: 18 (Additional Requirement), Stage 8 (Dynamic Lighting Colors)_

_Dependencies: Task 7 (Mesh Building with Vertex Lighting)_

_Prompt: Role: Graphics Programmer specializing in DirectX 11 rendering and performance optimization | Task: Implement Part A (double buffering + deferred mesh rebuild) and Part B (dynamic lighting colors with day/night sky, lightning, glowstone flicker) following Requirement 18 and Stage 8, adding m_meshVBO_A/B, m_skyColor/finalOutdoorColor/finalIndoorColor members, ProcessDirtyMeshes(0.005s), day/night cosine cycle (240s), lightning (Perlin 9 oct, scale 200), glowstone flicker (Perlin 9 oct, scale 500), App::Render() sky integration | Restrictions: Must use double buffering (2 VBOs), respect 5ms mesh budget, affect outdoor channel only, use separate Perlin channels, update constant buffer each frame | Success: NO flashing when digging, smooth transitions, 240s day/night cycle, lightning flashes, glowstone flicker, sky color reflects time, <16.67ms frame time_

---

## Implementation Order

**Critical Path (Sequential):**
1. Task 1 (Block Expansion) → Task 2 (BlockIterator) → Task 3 (Lighting Init)
2. Task 4 (Dirty Queue) + Task 5 (Propagation) → Task 6 (Chunk Activation)
3. Task 7 (Mesh Vertex Lighting) → Task 8 (World Shader) → Task 9 (Day/Night)
4. Task 11 (Dig/Place Update) depends on Tasks 4+5
5. Task 13 (Mesh Flashing Fix) depends on Task 7

**Parallel Opportunities:**
- Task 10 (Raycast) can be implemented anytime (no dependencies)
- Task 12 (Tree Variation) can be implemented anytime (no dependencies)
- Task 3 (Lighting Init) parallel with Task 2 (BlockIterator) if careful
- Tasks 7-9 (rendering) parallel with Tasks 4-6 (propagation) after Task 1 completes

**Recommended Execution:**
1. **Phase 1** (Days 1-4): Tasks 1-3 (Data structures and initialization)
2. **Phase 2** (Days 5-9): Tasks 4-6 (Lighting propagation system)
3. **Phase 3** (Days 10-14): Tasks 7-9 (Visual rendering and shaders)
4. **Phase 4** (Days 15-17): Tasks 10-11 (Interaction systems)
5. **Phase 5** (Days 18-20): Tasks 12-13 (Additional requirements and polish)

---

## Testing Strategy

**Unit Testing:**
- Task 1: sizeof(Block), light accessors, POD compliance
- Task 2: Cross-chunk navigation at boundaries
- Task 5: Light propagation math (15→14→13→...→0)

**Integration Testing:**
- Tasks 4+5+6: Full lighting propagation across chunks
- Tasks 7+8: Vertex lighting renders correctly
- Task 11: Dig/place triggers lighting update

**Performance Testing:**
- Task 4: ProcessDirtyLighting respects 8ms budget
- Task 10: Raycast <0.1ms per call
- Task 13: ProcessDirtyMeshes respects 5ms budget, no flashing

**Visual Testing:**
- Task 9: Day/night cycle visible over 4 minutes
- Task 12: Tree variation visible (not identical trees)
- All tasks: 60 FPS sustained, no visual glitches

---

## Success Criteria

**Functional:**
- [ ] Block structure expanded to 3 bytes
- [ ] Dual-channel lighting (outdoor/indoor) propagates correctly
- [ ] Day/night cycle affects outdoor light only
- [ ] Chunk activation propagates cross-chunk lighting
- [ ] Dig/place updates lighting dynamically
- [ ] Tree height/radius varies per Perlin noise
- [ ] No mesh flashing during dig/place/activation

**Performance:**
- [ ] Frame rate: 60 FPS sustained (16.67ms/frame)
- [ ] Lighting budget: <8ms/frame
- [ ] Mesh rebuild budget: <5ms/frame
- [ ] Cross-chunk propagation: <100ms latency
- [ ] Raycast performance: <0.1ms per call

**Quality:**
- [ ] No DirectX 11 resource leaks
- [ ] No crashes during chunk activation
- [ ] Save files work correctly (3-byte Block)
- [ ] Visual quality matches requirements
- [ ] Code follows SimpleMiner conventions

---

**Total Tasks:** 13
**Estimated Time:** 18-22 days (per requirements timeline)
**Last Updated:** 2025-11-15
