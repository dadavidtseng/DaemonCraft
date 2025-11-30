# Requirements Document - Assignment 5: Rendering and Lighting

## Introduction

Assignment 5 (SDST-A05) introduces volumetric voxel lighting, advanced rendering optimizations, and interactive block manipulation to SimpleMiner, a Minecraft-inspired procedurally-generated 3D voxel world. This assignment transforms the visual experience from a basic textured terrain into a dynamically-lit, immersive environment with day/night cycles, atmospheric effects, and light-emitting blocks.

### Purpose
Enable realistic volumetric lighting that propagates through the voxel world, providing visual feedback for underground exploration, surface/cave contrast, and player-placed light sources.

### Value to Users
- **Visual Clarity**: Light influences help players navigate underground caves and understand spatial relationships
- **Atmosphere**: Day/night cycles and weather effects create a more immersive gameplay experience
- **Player Agency**: Glowstone placement allows players to customize their environment and solve lighting challenges
- **Performance**: Optimized rendering ensures smooth 60 FPS gameplay even during chunk activation

## Alignment with Product Vision

SimpleMiner aims to be a technically sophisticated voxel game engine demonstrating professional game architecture patterns. Assignment 5 advances this vision by:

- **Technical Excellence**: Implementing industry-standard influence map algorithms (Minecraft-style lighting)
- **Performance First**: Maintaining 60 FPS through multi-threaded processing and optimized data structures
- **Player-Centric Design**: Precise raycasting and visual feedback improve block interaction UX
- **Educational Value**: Demonstrating advanced graphics programming (shaders, fog, mipmapping)

## Requirements

### Requirement 1: Block Data Structure Expansion

**User Story:** As a game engine, I need to store lighting data per-block so that each voxel can display correct light values without excessive memory overhead.

#### Acceptance Criteria

1. WHEN the Block class is defined THEN it SHALL contain exactly 3 bytes total
   - 1 byte: `m_typeIndex` (block type 0-255)
   - 1 byte: `m_lightingData` with outdoor light (high nibble 0-15) and indoor light (low nibble 0-15)
   - 1 byte: `m_bitFlags` for up to 8 boolean properties

2. WHEN accessing light values THEN Block SHALL provide inline getter/setter methods:
   - `uint8_t GetOutdoorLight() const` - extract high nibble (bits 4-7)
   - `uint8_t GetIndoorLight() const` - extract low nibble (bits 0-3)
   - `void SetOutdoorLight(uint8_t value)` - set high nibble, preserve low
   - `void SetIndoorLight(uint8_t value)` - set low nibble, preserve high

3. IF `sizeof(Block)` is queried THEN it SHALL return exactly 3 bytes

4. WHEN bit flags are accessed THEN Block SHALL provide methods for each flag:
   - `bool IsSky() const` / `void SetIsSky(bool)`
   - `bool IsLightDirty() const` / `void SetIsLightDirty(bool)`
   - `bool IsFullOpaque() const` / `void SetIsFullOpaque(bool)`
   - `bool IsSolid() const` / `void SetIsSolid(bool)`
   - `bool IsVisible() const` / `void SetIsVisible(bool)`

### Requirement 2: Dual Light Influence Maps

**User Story:** As a player exploring caves, I want to see both outdoor skylight and indoor glowstone light so that I can distinguish between surface-adjacent areas and deep underground.

#### Acceptance Criteria

1. WHEN a block stores lighting THEN it SHALL maintain two independent 0-15 values:
   - **Outdoor Light Influence**: Represents skylight penetration (blue-tinted by default)
   - **Indoor Light Influence**: Represents artificial light from blocks (warm-tinted by default)

2. WHEN light propagates THEN each influence type SHALL spread independently
   - Outdoor light SHALL decrease by 1 per block distance from sky
   - Indoor light SHALL decrease by 1 per block distance from emitting block
   - Opaque blocks SHALL refuse to accept either light type (remain at 0)

3. WHEN multiple lights influence a block THEN it SHALL take the highest value
   - Example: If outdoor=12 from sky AND indoor=8 from glowstone, outdoor remains 12 (not 20)
   - Light values are NOT additive, only the strongest source applies

4. IF a block emits light THEN it SHALL set its indoor light to its emission value
   - Glowstone: indoor light emission = 15
   - Lava: indoor light emission = 10 (example, configurable)
   - Most blocks: indoor light emission = 0

### Requirement 3: Light Propagation Algorithm

**User Story:** As a game engine, I need to efficiently calculate correct light values for all blocks so that lighting updates complete within one frame without blocking gameplay.

#### Acceptance Criteria

1. WHEN a chunk activates THEN lighting SHALL be initialized as follows:
   - All blocks default to 0 outdoor and 0 indoor light
   - Non-opaque boundary blocks touching existing neighbors SHALL be marked dirty
   - Descend each (x,y) column from top, setting IsSky() flag until first opaque block
   - Descend each column again, setting outdoor=15 for IsSky blocks
   - Mark non-sky horizontal neighbors of sky blocks as dirty
   - Mark all light-emitting blocks as dirty

2. WHEN processing dirty lighting THEN the system SHALL:
   - Pop the front block iterator from the dirty queue
   - Clear its IS_LIGHT_DIRTY flag
   - Compute correct outdoor and indoor light values:
     - IF IsSky() THEN outdoor = 15
     - IF block emits light THEN indoor >= emission value
     - IF non-opaque THEN light >= max(all 6 neighbors) - 1
   - IF computed value differs from current value THEN:
     - Update the block's light values
     - Mark the chunk's mesh as dirty
     - Mark all 6 non-opaque neighbors as dirty (if not already dirty)

3. WHEN all dirty blocks are processed THEN the queue SHALL be empty
   - Light SHALL propagate to 15 blocks from source (15→14→13...→1→0)
   - Propagation SHALL stop when a block calculates its correct value
   - No block SHALL appear in dirty queue more than once simultaneously

4. IF lighting is fully resolved THEN performance SHALL meet target:
   - Process all dirty lighting every frame (no amortization required for now)
   - Target: <16ms total frame time including lighting updates

### Requirement 4: Cross-Chunk Light Propagation

**User Story:** As a player placing glowstone near chunk boundaries, I want light to spread naturally across chunks so that lighting appears seamless.

#### Acceptance Criteria

1. WHEN a block iterator moves across chunk boundaries THEN it SHALL:
   - Detect when x, y, or z reaches chunk limits (0 or MAX)
   - Select the appropriate neighbor chunk (north/south/east/west/up/down)
   - Return a valid block iterator in the neighbor chunk at the correct local coordinates
   - Return null/invalid if no neighbor chunk exists

2. WHEN light propagates to chunk edges THEN it SHALL:
   - Mark corresponding edge blocks in neighbor chunks as dirty
   - Use the same dirty queue for all chunks (global, not per-chunk)
   - Ensure light values are correct across all chunk boundaries

3. IF chunk A is deactivated WHILE chunk B references its blocks THEN:
   - System SHALL remove all chunk A blocks from the global dirty queue
   - Use `UndirtyAllBlocksInChunk()` to prevent dangling references

### Requirement 5: Hidden Surface Removal at Chunk Boundaries

**User Story:** As a player, I want smooth performance and correct visuals so that chunk boundaries are invisible and hidden faces don't waste GPU resources.

#### Acceptance Criteria

1. WHEN building a chunk mesh THEN faces SHALL only be added IF:
   - The neighboring block in that direction is non-opaque OR
   - The neighbor is in an inactive chunk (boundary case)

2. WHEN a chunk's mesh is being rebuilt THEN it SHALL:
   - Only rebuild IF all 4 horizontal neighbor chunks are active
   - Wait for neighbor activation before attempting mesh construction
   - Allow chunks to be active without meshes (mesh pending state)

3. WHEN mesh rebuilding is scheduled THEN performance constraints SHALL apply:
   - Build closest 2 chunks to camera per frame maximum
   - Use dirty chunk priority queue (nearest first)

4. IF a chunk deactivates THEN its mesh buffers SHALL be released
   - Prevent DirectX 11 memory leaks
   - Null-check vertex/index buffers before rendering

### Requirement 6: Texture Mipmapping

**User Story:** As a player viewing distant terrain, I want textures to look crisp without aliasing artifacts so that far-away blocks remain visually pleasing.

#### Acceptance Criteria

1. WHEN textures are loaded THEN the Renderer SHALL:
   - Generate 5 mip levels for 32-pixel sprite sheets
   - Generate 6 mip levels for 64-pixel sprite sheets
   - Use auto-gen mipmapping (D3D11_RESOURCE_MISC_GENERATE_MIPS flag)

2. WHEN rendering chunks THEN the sampler SHALL:
   - Use trilinear filtering for mipmap interpolation
   - Select appropriate mip level based on screen-space derivative

3. IF mipmapping is disabled THEN performance MAY degrade
   - Note: This requirement is optional per assignment specification

### Requirement 7: Vertex Color Lighting

**User Story:** As a game engine, I need to encode light values in vertex colors so that the pixel shader can calculate per-pixel lighting without additional lookups.

#### Acceptance Criteria

1. WHEN adding a block face to the mesh THEN each vertex SHALL:
   - Set `color.r` = neighboring block's outdoor light / 15.0 (normalized 0-1)
   - Set `color.g` = neighboring block's indoor light / 15.0 (normalized 0-1)
   - Set `color.b` = directional greyscale shading (existing, based on face normal)
   - Use the light value from the block the face is pointing toward

2. WHEN a block's light value changes THEN affected chunks SHALL:
   - Mark their mesh as dirty
   - Rebuild mesh with updated vertex colors

3. IF a block has outdoor=15 and indoor=0 THEN its south-facing neighbor's north face SHALL:
   - Have vertex `color.r = 1.0` (full outdoor light)
   - Have vertex `color.g = 0.0` (no indoor light)

### Requirement 8: World Shader with Lighting and Fog

**User Story:** As a player, I want lighting and fog to create atmosphere so that caves feel dark, outdoors feels bright, and distant terrain fades into fog.

#### Acceptance Criteria

1. WHEN the world shader is compiled THEN it SHALL define a constant buffer (register b4):
   ```hlsl
   cbuffer WorldConstants : register(b4) {
       float4 CameraPosition;
       float4 IndoorLightColor;    // Default (255, 230, 204) warm
       float4 OutdoorLightColor;   // Default (255, 255, 255) white
       float4 SkyColor;            // Default (0, 0, 0), updated at runtime
       float FogNearDistance;       // Default activation_range - (2 * chunk_size)
       float FogFarDistance;        // Default FogNearDistance / 2
       float2 Padding;
   };
   ```

2. WHEN the vertex shader executes THEN it SHALL:
   - Pass world position to pixel shader
   - Pass vertex color (r=outdoor, g=indoor, b=directional) unchanged

3. WHEN the pixel shader executes THEN it SHALL:
   - Calculate `diffuseLightColor = DiminishingAdd(outdoor * OutdoorLightColor, indoor * IndoorLightColor)`
     - Where `DiminishingAdd(a, b) = 1 - (1-a)(1-b)` (assuming a,b in [0,1])
   - Multiply sampled texel color by diffuseLightColor and vertex.color.b
   - Calculate `fogFraction = smoothstep(FogNearDistance, FogFarDistance, distance(pixelWorldPos, CameraPosition))`
   - Blend final color toward SkyColor based on `fogFraction * fogMaxAlpha`

4. IF camera is far from chunk THEN fog SHALL:
   - Reach 100% opacity at activation range boundary
   - Use sky color for fog tint (matches clear color)

### Requirement 9: Day and Night Cycle

**User Story:** As a player, I want dynamic time-of-day so that the world feels alive and lighting conditions change naturally.

#### Acceptance Criteria

1. WHEN world time advances THEN it SHALL:
   - Use Clock class to store world time in days (float)
   - Calculate time of day as fractional part [0,1):
     - 0.0 = midnight
     - 0.25 = dawn (6am)
     - 0.5 = noon
     - 0.75 = dusk (6pm)
   - Base time ratio: 500:1 (world time : real time)
   - WHEN Y key held THEN accelerate time by 50x (total 25000:1 ratio)

2. WHEN time of day changes THEN sky and fog color SHALL:
   - Hold steady at dark blue (20, 20, 40) during night (6pm-6am)
   - Lerp toward light blue (200, 230, 255) at high noon
   - Use smooth interpolation curve (not linear)

3. WHEN time of day changes THEN outdoor light color SHALL:
   - Vary from dark blue-grey at midnight to full white during day
   - Blend smoothly to avoid sudden brightness changes

4. IF day/night cycle updates THEN NO block light values SHALL change
   - Only constant buffer values update (no mesh rebuilds required)
   - Performance: <0.1ms per frame for time calculations

### Requirement 10: Lightning Strikes

**User Story:** As a player, I want occasional lightning flashes so that the world feels dynamic and weather effects add excitement.

#### Acceptance Criteria

1. WHEN calculating lightning each frame THEN the system SHALL:
   - Sample 1D Perlin noise at current world time
   - Use 9 octaves and scale of 200
   - Range-map noise from [0.6, 0.9] to lightningStrength [0, 1]

2. WHEN lightningStrength > 0 THEN:
   - Lerp sky color toward white (255, 255, 255) by lightningStrength
   - Lerp outdoor light color toward white by lightningStrength
   - Update constant buffer, no mesh changes

3. IF lightningStrength = 1.0 THEN:
   - Sky SHALL be pure white (flash peak)
   - Outdoor-lit faces SHALL appear fully illuminated

### Requirement 11: Glowstone Flickering

**User Story:** As a player, I want glowstone to flicker subtly so that artificial lighting feels organic and torch-like.

#### Acceptance Criteria

1. WHEN calculating glowstone flicker each frame THEN the system SHALL:
   - Sample 1D Perlin noise at current world time
   - Use 9 octaves and scale of 500
   - Range-map noise from [-1, 1] to glowStrength [0.8, 1.0]

2. WHEN glowStrength is calculated THEN:
   - Multiply base indoor light color (255, 230, 204) by glowStrength
   - Update constant buffer only, no mesh rebuilds

3. IF glowStrength = 0.8 THEN indoor light SHALL:
   - Be 80% of base brightness (subtle darkening)
   - Remain warm-tinted (color hue preserved)

### Requirement 12: Fast Voxel Raycast

**User Story:** As a player, I want precise block selection so that I can dig and place blocks exactly where I intend.

#### Acceptance Criteria

1. WHEN performing raycast THEN the system SHALL:
   - Start at camera world position
   - Extend camera-forward for 8.0 meters
   - Use Fast Voxel Raycast algorithm (modified to use BlockIterator instead of coordinates)
   - Return GameRaycastResult3D with:
     - Block hit (if any)
     - Face normal (which face was struck)
     - Impact position (world coordinates)

2. WHEN raycast hits a block THEN the system SHALL:
   - Highlight the hit face visually (debug draw, not mesh modification)
   - Draw highlight separately from chunk meshes

3. WHEN R key is pressed THEN raycast SHALL:
   - Toggle locked/unlocked mode
   - IF locked THEN preserve raycast origin and direction regardless of camera movement
   - IF unlocked THEN update raycast each frame with camera

4. IF raycast is locked THEN visual feedback SHALL:
   - Draw 3D debug line showing ray path
   - Indicate hit (green) or miss (red)
   - Show impact point if hit

### Requirement 13: Improved Block Digging

**User Story:** As a player, I want to dig blocks precisely so that I can mine resources and shape terrain effectively.

#### Acceptance Criteria

1. WHEN left mouse button is pressed AND raycast hits a block THEN system SHALL:
   - Convert hit block to AIR
   - Mark the block's light as dirty
   - NOT immediately update light values (let propagation algorithm handle it)

2. IF the block above the dug block has IsSky() flag THEN:
   - Descend downward from dug block position
   - Set IsSky() flag on all non-opaque blocks until first opaque
   - Mark each newly-flagged sky block as dirty
   - Example: Breaking cave roof allows sunlight beam to stream down

3. WHEN digging completes THEN chunk SHALL:
   - Mark mesh as dirty
   - Rebuild mesh on next frame (or within 2-frame budget)

4. IF block cannot be dug (out of range, no hit) THEN:
   - No changes occur
   - No error messages displayed

### Requirement 14: Improved Block Placement

**User Story:** As a player, I want to place blocks precisely so that I can build structures and light dark areas.

#### Acceptance Criteria

1. WHEN right mouse button is pressed AND raycast hits a block THEN system SHALL:
   - Place selected block type on the struck face (adjacent to hit block)
   - Mark the new block's light as dirty
   - NOT immediately update light values (let propagation algorithm handle it)

2. IF the placed block is opaque AND replaced block had IsSky() flag THEN:
   - Clear IsSky() flag on placed block
   - Descend downward, clearing IsSky() flags until first opaque
   - Mark all affected blocks as dirty
   - Example: Plugging vertical shaft cuts off sunlight

3. WHEN placing block THEN selected type SHALL be:
   - Displayed in UI (top center of screen)
   - Changeable with number keys:
     - 1 = Glowstone (emits light 15)
     - 2 = Cobblestone (opaque, no emission)
     - 3 = Chiseled Brick (opaque, no emission)

4. IF placement location is occupied by opaque block THEN:
   - No placement occurs
   - No error feedback required

### Requirement 15: Glowstone and Cobblestone Blocks

**User Story:** As a game engine, I need to define light-emitting and opaque blocks so that players can manipulate lighting dynamically.

#### Acceptance Criteria

1. WHEN BlockDefinition is queried for Glowstone THEN it SHALL return:
   - `m_isOpaque = true` (blocks light propagation)
   - `m_isSolid = true` (player collision enabled)
   - `m_indoorLightEmission = 15` (maximum light output)
   - Sprite coordinates for glowstone texture

2. WHEN BlockDefinition is queried for Cobblestone THEN it SHALL return:
   - `m_isOpaque = true`
   - `m_isSolid = true`
   - `m_indoorLightEmission = 0` (no light output)
   - Sprite coordinates for cobblestone texture

3. IF player places glowstone THEN:
   - Light SHALL propagate 15 blocks from source (15→14→...→1→0)
   - Nearby chunks SHALL be marked mesh-dirty if lighting changes

4. IF player places cobblestone THEN:
   - Block SHALL stop light propagation (acts as light blocker)
   - Opposite side of cobblestone SHALL be darker than front side

### Requirement 16: BlockIterator Cross-Chunk Boundaries

**User Story:** As a game engine, I need block iterators to work across chunk boundaries so that light propagation and raycasting don't break at edges.

#### Acceptance Criteria

1. WHEN BlockIterator moves north AND y == CHUNK_MAX_Y THEN it SHALL:
   - Return `BlockIterator(northNeighborChunk, blockIndex & ~CHUNK_MASK_Y)`
   - Preserve x and z coordinates, reset y to 0 in new chunk

2. WHEN BlockIterator moves in any direction THEN it SHALL:
   - Detect boundary crossing (coordinate == 0 or MAX)
   - Select appropriate neighbor chunk pointer
   - Calculate new local block index using bitwise operations
   - Return valid iterator or null if no neighbor exists

3. WHEN BlockIterator is used in raycast THEN:
   - Algorithm SHALL traverse multiple chunks seamlessly
   - No special-case logic needed for chunk boundaries in raycast loop

4. IF neighbor chunk is inactive THEN:
   - Return null/invalid iterator
   - Caller SHALL handle null gracefully

### Requirement 17: Tree Height and Radius Variation

**User Story:** As a player exploring forests, I want trees to have natural size variation so that the environment feels more organic and less repetitive.

#### Acceptance Criteria

1. WHEN generating trees for a biome THEN the system SHALL:
   - Use Perlin noise based on world position (x, y) to generate height variation
   - Apply biome-specific height ranges:
     - Oak: 4-7 blocks trunk height, 2-4 blocks canopy radius
     - Spruce: 6-10 blocks trunk height, 1-3 blocks canopy radius
     - Jungle: 8-14 blocks trunk height, 3-5 blocks canopy radius
     - Acacia: 4-6 blocks trunk height, 2-4 blocks canopy radius
     - Birch: 5-8 blocks trunk height, 2-3 blocks canopy radius
   - Use separate noise channels for height and radius variation

2. WHEN placing a tree at position (x, y) THEN it SHALL:
   - Sample height noise at (x, y) with scale 50, octaves 2
   - Sample radius noise at (x, y) with scale 30, octaves 2
   - Range-map noise values to biome-specific min/max ranges
   - Round to nearest integer for discrete block placement

3. WHEN building tree canopy THEN the system SHALL:
   - Use generated radius value for leaf sphere/cone dimensions
   - Maintain tree shape (Oak=sphere, Spruce=cone, Jungle=irregular)
   - Ensure leaves don't extend beyond reasonable bounds (max 8 blocks from trunk)

4. IF tree generation creates invalid structures THEN:
   - Height SHALL be clamped to prevent trees exceeding chunk height (max z=250)
   - Radius SHALL be clamped to prevent excessive cross-chunk tree data (max 8 blocks)
   - Trees SHALL NOT generate if insufficient vertical space available

### Requirement 18: Fix Mesh Flashing and Jittering

**User Story:** As a player digging blocks and exploring, I want smooth visual rendering so that blocks don't flash or jitter when I interact with them or when chunks activate.

#### Acceptance Criteria

1. WHEN digging or placing a block THEN the system SHALL:
   - Mark affected chunk meshes as dirty BEFORE updating block data
   - Defer mesh rebuild to the next frame (not immediate)
   - Ensure old mesh remains visible until new mesh is ready
   - Use double-buffered vertex/index buffers to prevent read-during-write conflicts

2. WHEN a chunk mesh is being rebuilt THEN it SHALL:
   - Lock the mesh buffer during construction
   - Upload complete mesh in a single operation (not incrementally)
   - Atomically swap old and new buffers to prevent partial rendering
   - Release old buffer AFTER new buffer is bound for rendering

3. WHEN chunks are activating near player THEN the system SHALL:
   - Wait for all 4 horizontal neighbor chunks to be ACTIVE before building mesh
   - Render chunks without meshes as invisible (not empty/flashing)
   - Build meshes in priority order (nearest to camera first)
   - Limit mesh builds to 2 chunks per frame to prevent frame time spikes

4. IF flashing persists after mesh rebuild fixes THEN investigate:
   - Camera frustum culling correctness (ensure chunks aren't flickering in/out of view)
   - Depth buffer precision issues (z-fighting on coincident faces)
   - Shader constant buffer updates (ensure transforms are synchronized)
   - DirectX 11 device context state changes (ensure proper render state)

5. WHEN debugging mesh issues THEN provide visual feedback:
   - F4 key: Toggle "mesh pending" debug visualization (wireframe bounding box)
   - Console logging: Mesh rebuild events with chunk coordinates and frame time
   - Performance metric: Track mesh rebuild count and total time per frame

## Non-Functional Requirements

### Code Architecture and Modularity

**Single Responsibility Principle:**
- Block class: Store type, lighting, and flags only (3 bytes, no logic)
- Chunk class: Manage block array, lighting initialization, mesh building
- World class: Own global dirty queue, coordinate chunks, process lighting
- BlockIterator class: Provide cross-chunk navigation, neighbor queries

**Modular Design:**
- Lighting system isolated in World::ProcessDirtyLighting() and helper methods
- Shader constants managed by Renderer, updated by Game
- Raycast algorithm encapsulated in World::RaycastVsBlocks()

**Dependency Management:**
- Block depends on BlockDefinition (flyweight pattern)
- Chunk depends on World (for neighbor access, dirty queue)
- World depends on JobSystem (for async operations)
- Minimize circular dependencies through forward declarations

**Clear Interfaces:**
- Block: Public inline getters/setters for light and flags
- Chunk: Public methods for mesh rebuild, lighting init
- World: Public methods for marking dirty, processing lighting, raycast
- BlockIterator: Consistent API for moving in 6 directions

### Performance

**Frame Rate:**
- Maintain 60 FPS in Release build during normal gameplay
- Maintain 60 FPS during chunk activation/deactivation within range
- Target: 16.67ms total frame time (including all subsystems)

**Lighting Performance:**
- Process all dirty lighting within 8ms per frame (budgeted)
- Dirty queue processing: O(n) where n = dirty blocks (typically <1000)
- Chunk activation lighting init: O(blocks_per_chunk) = O(262,144) ~5ms

**Mesh Rebuild Performance:**
- Build maximum 2 chunks per frame (budget: 10ms total)
- Prioritize nearest chunks to camera
- Defer distant chunk meshes without blocking activation

**Memory Constraints:**
- Block size: 3 bytes (previously 1 byte, 3x increase acceptable)
- Dirty queue: ~1000 BlockIterators typical, 8 bytes each = 8 KB
- Avoid allocating memory during lighting propagation (use reserved queue)

### Reliability

**Thread Safety:**
- All DirectX 11 operations SHALL execute on main thread only
- Lighting propagation SHALL execute on main thread only (atomics too slow)
- Chunk generation/loading SHALL remain on worker threads (no change from A4)

**Error Handling:**
- Null-check neighbor chunk pointers before dereferencing
- Validate block indices within chunk bounds (assertions in debug)
- Handle raycast misses gracefully (no crash if no hit)

**Data Integrity:**
- Dirty queue SHALL never contain duplicate BlockIterators
- Chunk deactivation SHALL remove all its blocks from dirty queue
- Light values SHALL remain in [0, 15] range (clamped if needed)

### Usability

**Visual Clarity:**
- Highlight selected block face clearly (bright overlay or outline)
- Display current selected block type in UI (top center)
- Show FPS counter (top right) for performance monitoring

**Controls:**
- LMB: Dig block (intuitive, standard in genre)
- RMB: Place block (intuitive, standard in genre)
- 1/2/3 keys: Select block type (quick access to glowstone/cobblestone/brick)
- R key: Toggle raycast lock (useful for debugging)
- Y key: Accelerate time (useful for testing day/night)

**Feedback:**
- Block highlight: Immediate visual response (<16ms)
- Light changes: Visible within 1 frame after dirty processing
- Fog transitions: Smooth (no popping or abrupt changes)

**Debugging:**
- F2: Toggle chunk debug draw (bounding boxes, coords, counts)
- F3: Toggle job system debug draw (job counts, states)
- Step-by-step lighting: Implement L key to process one dirty queue iteration (for development)

### Compatibility

**Platform:**
- Windows 10/11 x64 only
- DirectX 11 (no fallback to DX10 or DX9)
- Visual Studio 2022 MSVC v143 compiler

**Third-Party Dependencies:**
- Engine project (shared codebase, C++20 features)
- FMOD audio (already integrated in A1-A4)
- TinyXML2 for XML parsing (BlockDefinitions.xml)
- Perlin noise implementation (provided on Canvas)

**Backward Compatibility:**
- All Assignment 4 features SHALL continue to work
- World save files MAY be incompatible (acceptable for assignment)
- Existing chunk files MAY require regeneration due to Block structure change

## Success Criteria

### Functional Completeness
- All 18 requirements implemented and tested
- Rubric: 100/100 points achievable
  - Block iterators: 5 pts
  - Hidden surface removal: 5 pts
  - Mipmapping: 5 pts (optional)
  - Data structures: 15 pts
  - Light propagation: 20 pts
  - World shader: 15 pts
  - Digging/placing: 20 pts
  - Outdoor effects: 15 pts
- Additional quality improvements (not graded, but enhance experience):
  - Req 17: Tree variation creates more natural-looking forests
  - Req 18: Smooth rendering eliminates visual distractions

### Performance Targets
- 60 FPS sustained in Release build
- <16ms frame time during chunk activation
- Lighting updates complete within same frame

### Visual Quality
- No visible artifacts at chunk boundaries
- Smooth fog transitions
- Realistic light falloff (15 blocks range)
- Atmospheric day/night cycle

### Code Quality
- Zero compile warnings
- No unnecessary files submitted to Perforce
- Proper Perforce depot path submission
- Buddy build successful (compiles on teammate's machine)

## Assumptions and Constraints

### Assumptions
- User has completed Assignments 1-4 successfully
- Block structure can be expanded from 1 byte to 3 bytes (memory acceptable)
- Frame budget allows for full lighting propagation per frame (no amortization)
- Professor's reference build demonstrates expected behavior

### Constraints
- Must use provided sprite sheets and BlockDefinitions.xml (no modifications)
- Must use provided Perlin noise functions (from Canvas)
- All DirectX 11 activity on main thread only (threading constraint)
- Maximum light value = 15 (4-bit storage per light type)
- Chunk size fixed at 32×32×256 blocks (inherited from A4)

## Risks and Mitigations

| Risk | Impact | Likelihood | Mitigation |
|------|---------|-----------|------------|
| Lighting propagation too slow (>60 FPS drop) | High | Medium | Optimize dirty queue processing, profile with actual data, consider amortization if needed |
| Chunk boundary lighting artifacts | Medium | Medium | Thorough testing of BlockIterator edge cases, visual inspection |
| Memory bloat from 3-byte blocks | Medium | Low | Monitor chunk memory usage, acceptable 3x increase per assignment |
| DirectX 11 memory leaks | High | Low | Use existing three-stage shutdown (fixed in A4 Phase 5B), track buffer creation/deletion |
| Dirty queue infinite loops | High | Low | Add iteration limit failsafe, debug step-through with L key |
| Raycast precision errors | Medium | Low | Use Professor's Fast Voxel Raycast reference implementation, test edge cases |
| Mesh flashing/jittering persists after fixes | Medium | Medium | Implement debug visualization (F4), profile mesh rebuild timing, use performance counters to identify root cause |
| Tree variation creates cross-chunk issues | Low | Low | Clamp max radius to 8 blocks, ensure TreeStamp placement respects chunk boundaries, test at chunk edges |

## Dependencies

### Internal Dependencies
- Assignment 4 completion (world generation, chunk system, threading)
- Engine project (Renderer, JobSystem, Camera, Clock, Input)
- CLAUDE.md documentation (current SimpleMiner state)

### External Dependencies
- DirectX 11 SDK (included with Windows SDK)
- Visual Studio 2022 with C++20 support
- FMOD audio library
- TinyXML2 library
- Perlin noise implementation (provided)

### Data Dependencies
- BlockDefinitions.xml (provided, must use unchanged)
- Sprite sheets (64x faithful pack, provided)
- Default.hlsl shader (existing, requires modification)

## Timeline and Milestones

**Total Duration**: 20 days (3 weeks)
**Key Change from Original**: Added Phase 10 (Quality Improvements) for tree variation and mesh fixes

### Phase 1: Foundation (Days 1-3)
- Expand Block class to 3 bytes
- Implement bit flag methods
- Add lighting data getters/setters
- **Milestone**: `sizeof(Block) == 3`, all tests pass

### Phase 2: Block Iterators (Days 2-3)
- Implement cross-chunk boundary navigation
- Test all 6 directions at chunk edges
- **Milestone**: Raycast works across chunk boundaries

### Phase 3: Lighting Data Structures (Days 3-5)
- Create global dirty queue in World
- Implement dirty flag management
- Add chunk activation lighting initialization
- **Milestone**: Chunks activate with correct sky light (no propagation yet)

### Phase 4: Light Propagation Algorithm (Days 5-8)
- Implement ProcessDirtyLighting() loop
- Calculate correct light values (sky, emission, neighbor propagation)
- Test with single glowstone, verify 15→0 falloff
- **Milestone**: Light propagates correctly within single chunk

### Phase 5: Cross-Chunk Lighting (Days 8-10)
- Extend propagation to chunk boundaries
- Test glowstone near chunk edge
- Verify seamless lighting across chunks
- **Milestone**: No lighting artifacts at chunk boundaries

### Phase 6: Vertex Lighting and Shaders (Days 10-12)
- Modify mesh building to use neighbor light values
- Create World shader constant buffer
- Implement pixel shader lighting calculations
- Add fog distance-based blending
- **Milestone**: Chunks render with correct lighting colors

### Phase 7: Raycasting and Block Manipulation (Days 12-14)
- Implement Fast Voxel Raycast with BlockIterator
- Add block digging with sky propagation
- Add block placement with sky clearing
- Implement block highlight rendering
- **Milestone**: Can dig/place blocks with correct lighting updates

### Phase 8: Outdoor Effects (Days 14-15)
- Implement day/night cycle time system
- Add sky color transitions
- Add lightning strikes (Perlin noise)
- Add glowstone flickering
- **Milestone**: Atmospheric effects working, smooth transitions

### Phase 9: Optimization and Polish (Days 15-17)
- Profile lighting performance
- Optimize dirty queue processing if needed
- Add hidden surface removal at boundaries
- Implement mipmapping (optional)
- **Milestone**: 60 FPS sustained

### Phase 10: Quality Improvements (Days 17-18)
- Implement tree height and radius variation (Req 17)
  - Add Perlin noise for height and radius
  - Update TreeStamp generation with biome-specific ranges
  - Test in all 5 tree biomes (Oak, Spruce, Jungle, Acacia, Birch)
- Fix mesh flashing and jittering (Req 18)
  - Implement double-buffered mesh updates
  - Fix chunk activation mesh timing
  - Add F4 debug visualization
- **Milestone**: Natural tree variation, smooth rendering

### Phase 11: Testing and Submission (Days 18-20)
- Comprehensive testing of all features
- Buddy build verification
- Fix compile warnings
- Clean Perforce submission
- **Milestone**: Assignment complete, ready to submit

## Glossary

| Term | Definition |
|------|------------|
| **Influence Map** | A grid where each cell stores a value counting down from a source (15→14→...→0), unlike distance fields which count up |
| **Dirty Queue** | A global list of BlockIterators marking blocks with potentially incorrect light values that need recalculation |
| **Sky Block** | A non-opaque block with no opaque blocks above it (IsSky flag set), receives full outdoor light (15) |
| **Nibble** | 4 bits, storing values 0-15 (half a byte) |
| **Opaque Block** | A block that prevents light propagation (stone, glowstone, cobblestone) |
| **Diminishing Add** | Formula `1 - (1-a)(1-b)` for combining lights without simple addition (avoids unrealistic brightness) |
| **Flyweight Pattern** | Design pattern where Block (1-3 bytes) references shared BlockDefinition data to save memory |
| **Fast Voxel Raycast** | Algorithm by Amanatides & Woo for efficiently traversing a voxel grid along a ray |
| **Chunk Boundary** | Edge where x=0/31, y=0/31 where blocks must check neighbor chunks for data |
| **Mipmapping** | Pre-generating smaller texture versions to reduce aliasing when viewed at distance |
| **Activation Range** | Distance from camera where chunks activate (480 blocks = 15 chunk radius per A4) |

## References

- **Assignment Document**: `.claude/docs/A5/Assignment 05 - Rendering and Lighting.txt`
- **Light Propagation Lecture**: `.claude/docs/A5/Light Propogation.txt` (Professor Forseth's transcript)
- **SimpleMiner Documentation**: `CLAUDE.md` (root level, current status)
- **Codebase Analysis**: Comprehensive exploration completed prior to requirements creation
- **Minecraft Lighting Reference**: Java Edition lighting system (industry standard for voxel lighting)
