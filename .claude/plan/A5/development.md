# SimpleMiner - Assignment 05: Rendering and Lighting Development Plan

## Overview

Assignment 5 adds volumetric lighting to SimpleMiner with indoor/outdoor light influence propagation, day/night cycles, lightning strikes, and comprehensive lighting effects. This builds on A4's world generation to create a dynamic, atmospheric game world.

**Official Assignment Specification:** See `.claude/docs/A5/Assignment 05 - Rendering and Lighting.txt` and `.claude/docs/A5/Light Propogation.txt`

**Key Technical Concepts:**
- **Influence Maps**: Count-down values (15→0) representing light intensity
- **Dual Lighting Systems**: Separate indoor (glowstone) and outdoor (sky) light channels
- **Dirty Lighting Queue**: Efficient propagation using block iterators
- **Vertex Coloring**: Per-face lighting based on neighboring block influences

---

## Architecture Changes

### Block Data Expansion
- **Current**: 1 byte (block type)
- **New**: 3 bytes total
  - Byte 1: Block type (unchanged)
  - Byte 2: Light influences (dual nibbles)
    - High nibble (bits 4-7): Outdoor light (0-15)
    - Low nibble (bits 0-3): Indoor light (0-15)
  - Byte 3: Bit flags (8 boolean properties)

### New Bit Flags
1. `BLOCK_BIT_IS_SKY`: Non-opaque block with no opaque blocks above
2. `BLOCK_BIT_IS_LIGHT_DIRTY`: Currently in dirty lighting queue
3. `BLOCK_BIT_IS_FULL_OPAQUE`: Blocks light, visibility, hides neighbor faces
4. `BLOCK_BIT_IS_SOLID`: Physical collisions enabled
5. `BLOCK_BIT_IS_VISIBLE`: Rendered during mesh rebuild

---

## Pipeline Stages

### Stage 1: Foundation & Data Structures (**HIGH PRIORITY**)
**Goal**: Expand Block class, add lighting infrastructure

**Tasks**:
- [ ] 1.1: Expand Block class to 3 bytes
  - Add `uint8_t m_lightInfluences` (indoor/outdoor nibbles)
  - Add `uint8_t m_bitFlags` (8 boolean flags)
  - Implement getter/setter methods for light values
  - Implement IsXXX()/SetXXX() methods for bit flags
- [ ] 1.2: Update BlockDefinition with light emission
  - Add `uint8_t m_indoorLightEmission` (0-15)
  - Add `uint8_t m_outdoorLightEmission` (0-15, typically 0)
  - Glowstone: indoorLight=15, opaque=true
  - Cobblestone: all defaults
- [ ] 1.3: Add World dirty lighting queue
  - `std::deque<BlockIterator> m_dirtyLightingQueue`
  - Methods: MarkLightingDirty(), ProcessDirtyLighting(), ProcessNextDirtyLightBlock()
  - Prevent duplicates using BLOCK_BIT_IS_LIGHT_DIRTY flag
- [ ] 1.4: Testing checkpoint
  - Verify Block sizeof() == 3 bytes
  - Test bit flag get/set operations
  - Confirm light influence nibble packing/unpacking

**Technical Notes**:
- Use bit masks for nibble extraction: `outdoor = (m_lightInfluences >> 4) & 0x0F`
- Use bitwise operations for flags: `(m_bitFlags & BLOCK_BIT_IS_SKY) != 0`
- Block iterator must support cross-chunk neighbor lookups

---

### Stage 2: Block Iterators & Hidden Surface Removal (**HIGH PRIORITY**)
**Goal**: Cross-chunk block iteration and improved mesh generation

**Tasks**:
- [ ] 2.1: Implement cross-chunk block iterators
  - Support 6-directional neighbor lookup
  - Handle chunk boundary crossings
  - Return null/invalid when no valid neighbor exists
  - Preserve other coordinate bits when updating one axis
- [ ] 2.2: Chunk hidden surface removal at boundaries
  - Only build meshes for chunks with all 4 horizontal neighbors active
  - Allow active chunks without meshes (deferred mesh build)
  - Build 2 nearest dirty chunks per frame (camera-distance sorted)
  - Mark chunks dirty when neighbors activate
- [ ] 2.3: Mipmapping (optional but recommended)
  - Enable auto-generate mipmaps for textures
  - 5 levels for 32px sprites, 6 levels for 64px sprites
  - Use GPU auto-generation: `D3D11_RESOURCE_MISC_GENERATE_MIPS`
- [ ] 2.4: Testing checkpoint
  - Verify neighbor lookups work across chunk edges
  - Confirm no visual seams at chunk boundaries
  - Test mipmap quality at various distances

**Technical Notes**:
- Example: Iterating north across chunk boundary:
  ```cpp
  if (y == CHUNK_MAX_Y)
      return BlockIterator(northNeighbor, blockIndex & ~CHUNK_MASK_Y);
  else
      return BlockIterator(chunk, (blockIndex & ~CHUNK_MASK_Y) | ((y+1) << CHUNK_BITS_X));
  ```

---

### Stage 3: Light Influence Initialization (**HIGH PRIORITY**)
**Goal**: Efficiently initialize lighting when chunks activate

**Tasks**:
- [ ] 3.1: Chunk activation lighting setup
  - Default all blocks to 0 indoor/outdoor light, not dirty
  - Mark non-opaque boundary blocks touching neighbors as dirty
  - Descend columns to flag SKY blocks (stop at first opaque)
  - Set outdoor light=15 for SKY blocks, mark horizontal neighbors dirty
  - Mark light-emitting blocks (glowstone) as dirty
- [ ] 3.2: SKY flag propagation
  - Implement downward SKY column marking algorithm
  - Handle terrain changes (digging creates SKY beam)
  - Handle block placement (opaque blocks stop SKY propagation)
- [ ] 3.3: Edge case handling
  - Chunks with no active neighbors (defer lighting)
  - Chunks activating in sequence (incremental updates)
  - Underground chunks (no SKY blocks)
- [ ] 3.4: Testing checkpoint
  - Verify SKY blocks have outdoor light=15
  - Confirm edge blocks marked dirty correctly
  - Test glowstone initialization

**Technical Notes**:
- ONE global dirty lighting queue for all chunks
- Only non-opaque blocks can be dirty
- Initial state minimizes dirty queue size

---

### Stage 4: Light Propagation Algorithm (**CRITICAL**)
**Goal**: Implement the core lighting propagation system

**Professor's Algorithm** (from Light Propogation.txt):
```
ProcessDirtyLighting():
    while (dirtyQueue not empty):
        block = pop front of queue
        clear block's dirty flag

        correctIndoor = compute correct indoor light
        correctOutdoor = compute correct outdoor light

        if (indoor OR outdoor is incorrect):
            update both light values
            mark chunk meshes dirty (this + 6 neighbors' chunks)
            mark 6 neighbors as dirty (if non-opaque)

Computing Correct Light:
    if (block is SKY):
        outdoor = 15
    if (block emits light):
        indoor = at least emission level
    if (block is non-opaque):
        indoor = at least (max neighbor indoor - 1)
        outdoor = at least (max neighbor outdoor - 1)
```

**Tasks**:
- [ ] 4.1: Implement ProcessDirtyLighting()
  - Pop front block from queue
  - Clear BLOCK_BIT_IS_LIGHT_DIRTY flag
  - Compute correct light influences
  - Update if incorrect, mark neighbors dirty
  - Run until queue empty (every frame)
- [ ] 4.2: Implement light influence calculation
  - SKY blocks: outdoor=15
  - Light-emitting blocks: indoor=emission level
  - Non-opaque blocks: max(neighbors)-1 for each channel
  - Opaque blocks: always 0 (don't process)
- [ ] 4.3: Neighbor dirtying logic
  - Only mark non-opaque neighbors as dirty
  - Check dirty flag before adding to queue (no duplicates)
  - Mark 6 neighbor chunks' meshes as dirty
- [ ] 4.4: Handle light removal (critical edge case)
  - When light source removed, value stays initially
  - Propagation reduces values: 9→7→6→5→...→0
  - "Leapfrog effect": alternating reductions until stable
- [ ] 4.5: Testing checkpoint
  - Place glowstone, verify 15→14→13→...→0 propagation
  - Remove glowstone, verify light drains correctly
  - Test light around corners (non-line-of-sight)
  - Multiple light sources: verify max() logic

**Technical Notes**:
- Light does NOT add (10+10≠20), uses max(A,B)
- Opaque blocks refuse to be dirty (light doesn't enter)
- Propagation stops when block calculates to same value (correct)
- Use std::deque for efficient push_back/pop_front

---

### Stage 5: Vertex Coloring & World Shader (**HIGH PRIORITY**)
**Goal**: Apply lighting to rendered geometry

**Tasks**:
- [ ] 5.1: Update vertex color during mesh rebuild
  - For each block face, query neighboring block's light
  - Red channel: normalize outdoor light (0-15 → 0-1)
  - Green channel: normalize indoor light (0-15 → 0-1)
  - Blue channel: directional greyscale (existing)
- [ ] 5.2: Create WorldConstants constant buffer (b4)
  ```hlsl
  cbuffer WorldConstants : register(b4) {
      float4 CameraPosition;
      float4 IndoorLightColor;  // (255, 230, 204) default
      float4 OutdoorLightColor; // (255, 255, 255) default
      float4 SkyColor;
      float FogNearDistance;
      float FogFarDistance;
      float2 Padding;
  };
  ```
- [ ] 5.3: Update world pixel shader
  - Pass world position from vertex shader
  - Compute diffuse light: DiminishingAdd(outdoor × outdoorColor, indoor × indoorColor)
  - DiminishingAdd(a, b) = 1 - (1-a)(1-b)
  - Multiply texel by diffuse light × vertexColor.b
  - Apply distance fog: lerp(color, skyColor, fogFraction)
- [ ] 5.4: Testing checkpoint
  - Verify glowstone casts reddish/greenish tint
  - Confirm outdoor light appears white
  - Test fog transitions at activation range
  - Check lighting is face-specific (not volumetric on faces)

**Technical Notes**:
- Face lighting: east face lit by east neighbor's light volume
- Opaque blocks' internal light=0, but faces lit by neighbors
- Fog range: far=activationRange - (2×chunkSize), near=far/2

---

### Stage 6: Digging and Placing Blocks (**HIGH PRIORITY**)
**Goal**: Update lighting dynamically during gameplay

**Tasks**:
- [ ] 6.1: Implement digging block effects
  - Set new block type (→ AIR)
  - Mark block lighting dirty
  - DON'T manually update light (algorithm handles it)
  - If block above is SKY, descend downward marking SKY blocks
  - Example: Break cave ceiling, sunlight streams to floor
- [ ] 6.2: Implement placing block effects
  - Set new block type (← glowstone/cobblestone)
  - Mark block lighting dirty
  - DON'T manually update light (algorithm handles it)
  - If replacing SKY block with opaque, descend clearing SKY flags
  - Example: Plug mineshaft, plunge cave into darkness
- [ ] 6.3: Fast Voxel Raycast integration
  - Modify algorithm to use BlockIterator (not tile coords)
  - Ensures cross-chunk raycasting
  - Highlight impacted block face
  - LMB digs, RMB places on impacted face
  - R key toggles raycast lock (debug)
- [ ] 6.4: Testing checkpoint
  - Dig block, verify lighting updates smoothly
  - Place glowstone, verify 15→14→13→... spread
  - Remove glowstone, verify light drains
  - Test digging/placing at chunk boundaries

**Technical Notes**:
- Skylight propagation can affect many blocks (entire vertical column)
- Light propagation is fully resolved every frame
- Mesh rebuilding triggered by light changes (mark chunks dirty)

---

### Stage 7: Day/Night Cycle & Outdoor Effects (**MEDIUM PRIORITY**)
**Goal**: Dynamic lighting atmosphere

**Tasks**:
- [ ] 7.1: Implement world time system
  - Use Clock class for world time in days
  - Time of day = fractional part [0, 1): 0=midnight, 0.25=dawn, 0.5=noon, 0.75=dusk
  - Base ratio: 500:1 (world:real time)
  - Y key held: 100× faster (50000:1 effective ratio)
  - worldTimeScale = acceleration × baseScale / (60×60×24)
- [ ] 7.2: Sky and fog color modulation
  - Night (18:00-06:00): dark blue (20, 20, 40)
  - Day (peak noon): light blue (200, 230, 255)
  - Smooth lerp transitions at dawn/dusk
  - Sky color = clear color (not drawn geometry)
- [ ] 7.3: Variable outdoor light color
  - Midnight: dark blue-grey
  - Daytime: full white (255, 255, 255)
  - Pass via constant buffer to pixel shader
- [ ] 7.4: Testing checkpoint
  - Verify smooth day/night transitions
  - Confirm Y key acceleration works
  - Test sky color matches fog color

**Technical Notes**:
- Light INFLUENCES don't change (still 0-15)
- Only constant buffer outdoor color changes
- No mesh rebuilds needed for day/night

---

### Stage 8: Lightning & Glowstone Flicker (**LOW PRIORITY**)
**Goal**: Atmospheric lighting effects

**Tasks**:
- [ ] 8.1: Lightning strikes
  - 1D Perlin noise based on world time
  - Params: 9 octaves, scale 200
  - Range-map [0.6, 0.9] → lightningStrength [0, 1]
  - Lerp sky + outdoor color toward white (255, 255, 255)
- [ ] 8.2: Glowstone flicker
  - 1D Perlin noise based on world time
  - Params: 9 octaves, scale 500
  - Range-map [-1, 1] → glowStrength [0.8, 1.0]
  - Multiply base indoor color by glowStrength
- [ ] 8.3: Testing checkpoint
  - Lightning flashes visible in sky
  - Glowstone subtly flickers
  - Effects don't interfere with each other

**Technical Notes**:
- Both effects purely constant buffer changes
- No block data modified
- Frame-by-frame noise evaluation

---

## Implementation Timeline

| Phase | Duration | Priority | Notes |
|-------|----------|----------|-------|
| **Stage 1: Foundation** | 4-6 hours | HIGH | Block expansion critical path |
| **Stage 2: Iterators & HSR** | 3-5 hours | HIGH | Cross-chunk foundation |
| **Stage 3: Light Init** | 3-4 hours | HIGH | Efficient setup key for performance |
| **Stage 4: Propagation** | 8-12 hours | **CRITICAL** | Core algorithm, most complex |
| **Stage 5: Vertex Coloring** | 4-6 hours | HIGH | Visual results |
| **Stage 6: Digging/Placing** | 3-4 hours | HIGH | Gameplay integration |
| **Stage 7: Day/Night** | 2-3 hours | MEDIUM | Polish feature |
| **Stage 8: Effects** | 1-2 hours | LOW | Nice-to-have |
| **Total (All)** | 28-42 hours | - | - |
| **Total (Required)** | 25-37 hours | - | Stages 1-6 |

---

## Testing Strategy

### Per-Stage Testing
- [ ] Stage 1: Block data integrity, bit operations
- [ ] Stage 2: Chunk boundary seamlessness
- [ ] Stage 3: Initial lighting correctness
- [ ] Stage 4: Propagation accuracy, removal handling
- [ ] Stage 5: Visual lighting quality
- [ ] Stage 6: Interactive lighting updates
- [ ] Stage 7: Day/night smooth transitions
- [ ] Stage 8: Lightning and flicker visibility

### Integration Testing
- [ ] Place glowstone → verify full propagation → remove → verify drain
- [ ] Dig through cave ceiling → verify sunlight beam
- [ ] Build enclosed room → place torch → verify indoor lighting only
- [ ] Cross-chunk lighting (light source near edge)
- [ ] Performance: 60 FPS maintained during chunk activation

### Visual Quality Checks
- [ ] No flickering or jittering lights
- [ ] Smooth gradients (no banding)
- [ ] Appropriate brightness falloff
- [ ] Natural color tinting (warm indoor, cool outdoor)

---

## Common Pitfalls

### Block Iterator Issues
- **Problem**: Crashes when iterating across chunk boundaries
- **Solution**: Always check neighbor chunk exists before dereferencing

### Light Removal Artifacts
- **Problem**: Light persists after source removed
- **Solution**: Ensure propagation reduces values correctly (leapfrog effect)

### Performance Degradation
- **Problem**: Frame drops when processing large dirty queues
- **Solution**: Profile, optimize hot loops, consider chunked processing

### Mesh Rebuild Cascades
- **Problem**: Placing one block rebuilds dozens of chunk meshes
- **Solution**: Only rebuild when light actually changes, batch rebuilds

---

## Rubric Alignment (100 points)

| Category | Points | Key Requirements |
|----------|--------|------------------|
| **Block Iterators** | 5 | Cross-chunk neighbor lookup |
| **Hidden Surface Removal** | 5 | Boundary HSR, deferred meshes |
| **Mipmapping** | 5 | Auto-gen, correct level counts |
| **Data Structures** | 15 | 3-byte blocks, bit flags, queue |
| **Light Propagation** | 20 | Algorithm correctness, SKY handling |
| **World Shader** | 15 | Vertex colors, constant buffer, fog |
| **Digging/Placing** | 20 | Dynamic updates, SKY beams |
| **Outdoor Effects** | 15 | Day/night, lightning, flicker |

**Deductions**:
- Unable to build: -100 points
- Compile warnings: -5 points
- Unnecessary files: -5 points

---

## File Locations

### Primary Implementation
- `Code/Game/Framework/Block.hpp/cpp` - Expand to 3 bytes, add methods
- `Code/Game/Gameplay/World.hpp/cpp` - Dirty lighting queue, ProcessDirtyLighting()
- `Code/Game/Framework/Chunk.cpp` - Mesh vertex coloring, initialization
- `Run/Data/Shaders/World.hlsl` - Updated pixel shader

### New Files
- `Code/Game/Framework/BlockIterator.hpp/cpp` - Cross-chunk iteration

---

## Final Notes

**Focus on Core Algorithm First**:
The light propagation algorithm (Stage 4) is the heart of this assignment. Get it working with simple test cases (single glowstone) before tackling complex scenarios.

**Professor's Wisdom** (from transcript):
- "Spreading light is relatively simple. Removing light is very strange in an influence map."
- "Light values count down: 15→14→13→...→0. When it hits zero, propagation stops."
- "Blocks can be wrong intentionally to provoke the chain reaction."
- "Processing all dirty lighting every frame is fast enough - don't amortize unless profiling shows issues."

**Debug Visualization Recommendations**:
- Draw yellow cubes for blocks in dirty queue
- Color blocks by light value (red=indoor, green=outdoor)
- Step-by-step propagation (L key to advance one pass)

**Match Reference Build**:
Compare visual output, lighting patterns, and performance to provided reference executable.
