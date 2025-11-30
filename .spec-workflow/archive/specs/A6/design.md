# Design Document - Assignment 6: Player, Camera, and Physics

## Overview

Assignment 6 transforms SimpleMiner from a basic free-fly navigation system into a fully-featured voxel game with realistic player physics, multiple camera modes, and comprehensive collision detection. This design implements a Newtonian physics system at the Entity base class level, enabling all future game objects (Player, NPCs, projectiles) to benefit from consistent physics behavior.

**Key Design Goals:**
- **Entity-Based Physics**: Physics logic resides in Entity base class, not Player
- **Modular Architecture**: Clean separation of physics, input, camera, and rendering systems
- **Performance**: Maintain 60 FPS with efficient collision detection (raycasts, not brute-force)
- **Extensibility**: Design supports future NPCs, vehicles, projectiles without modification
- **Debug Support**: Multiple camera/physics modes facilitate development and testing

## Technical Debt and Bug Fixes

In addition to the primary A6 implementation, this design addresses two critical technical debt items that can be resolved either before or after the main physics system implementation:

### Issue 1: OpenSSL Include Path and Library Linking Configuration

**Problem:**
The Engine contains OpenSSL 3.6.0 prebuilt libraries in `Engine/Code/ThirdParty/openssl/` (manually added October 12, 2025 via commit 3bb2a60), but the project settings are not configured to use them. This causes two types of build errors:

**Compiler Errors (Engine project):**
```
error C1083: Cannot open include file: 'openssl/macros.h': No such file or directory
(compiling source file 'Network/KADIAuthenticationUtility.cpp')
```

The OpenSSL headers use self-referential includes like `#include <openssl/macros.h>`, but the compiler cannot find them because `ThirdParty/openssl/include` is not in the include path.

**Linker Errors (SimpleMiner project):**
```
error LNK2001: unresolved external symbol EVP_DigestVerifyInit
error LNK2001: unresolved external symbol BIO_new_mem_buf
... (26 total unresolved symbols)
fatal error LNK1120: 26 unresolved externals
```

The Engine.lib references OpenSSL functions, but SimpleMiner doesn't link against `libcrypto.lib` and `libssl.lib`.

**Current OpenSSL Setup:**
- **Location:** `C:\p4\Personal\SD\Engine\Code\ThirdParty\openssl\`
- **Version:** OpenSSL 3.6.0
- **Platform:** x64 only (both Debug and Release)
- **Type:** Prebuilt static libraries
- **Added:** October 12, 2025 (commit 3bb2a60)
- **Source:** Manually downloaded (NOT from vcpkg)

**Impact:**
- All builds fail (both Debug and Release)
- Blocks all development work
- Must be fixed before any code can run

**Solution: Fix Project Configuration (Two-Part Fix)**

**Fix A: Add Include Path to Engine.vcxproj** ⭐ **REQUIRED**

Add OpenSSL include directory to all 4 configurations:

```xml
<AdditionalIncludeDirectories>
  $(SolutionDir)Code/;
  $(SolutionDir)../Engine/Code/;
  $(SolutionDir)../Engine/Code/ThirdParty/imgui;
  $(SolutionDir)../Engine/Code/ThirdParty/openssl/include;  ← ADD THIS LINE
  %(AdditionalIncludeDirectories)
</AdditionalIncludeDirectories>
```

This allows OpenSSL headers to resolve internal includes like `<openssl/macros.h>`.

**Fix C: Add Library Linking to Game.vcxproj** ⭐ **REQUIRED**

Add OpenSSL library path and dependencies to all 4 configurations:

```xml
<Link>
  <AdditionalLibraryDirectories>
    $(SolutionDir)Code/;
    $(SolutionDir)../Engine/Code/;
    $(V8LibPath);
    $(SolutionDir)../Engine/Code/ThirdParty/openssl/lib/x64/$(Configuration);  ← ADD THIS
    %(AdditionalLibraryDirectories)
  </AdditionalLibraryDirectories>
  <AdditionalDependencies>
    winmm.lib;
    dbghelp.lib;
    shlwapi.lib;
    libcrypto.lib;  ← ADD THIS
    libssl.lib;     ← ADD THIS
    %(AdditionalDependencies)
  </AdditionalDependencies>
</Link>
```

**Note:** `$(Configuration)` automatically selects the Debug or Release folder, so this works for both build types.

**Implementation Steps:**
1. Edit `Engine/Code/Engine/Engine.vcxproj` - Add include path (4 configurations)
2. Edit `Code/Game/Game.vcxproj` - Add library path and dependencies (4 configurations)
3. Rebuild Engine project (should succeed)
4. Rebuild SimpleMiner project (should link successfully)
5. Run SimpleMiner_Release_x64.exe to verify no runtime errors

**Platform Notes:**
- ✅ **x64 configurations:** Fully supported (Debug + Release)
- ❌ **Win32 configurations:** Will fail (no Win32 libraries available)
- If Win32 support needed, must download/build Win32 OpenSSL libraries separately

**Timing:** Must be fixed immediately - all builds currently fail

---

### Issue 2: Missing Hidden Surface Removal (A5 Requirement)

**Problem:**
Assignment 5 required full hidden surface removal for chunk mesh optimization, but the current implementation may not be correctly eliminating all hidden faces between adjacent solid blocks.

**Expected Behavior:**
When building chunk meshes, faces between two solid blocks should NOT be added to the vertex/index buffers:
```
Example: Two adjacent stone blocks
Block A (1,2,3) - STONE
Block B (2,2,3) - STONE

The +X face of Block A should NOT be rendered (hidden by Block B)
The -X face of Block B should NOT be rendered (hidden by Block A)
```

**Current Implementation Location:**
`Code/Game/Framework/Chunk.cpp` - `AddBlockFacesWithHiddenSurfaceRemoval()` method

**Verification Steps:**
1. Check if the method queries adjacent block solidity before adding faces
2. Ensure all 6 face directions check neighbors:
   - +X face: Check if block at (x+1, y, z) is solid
   - -X face: Check if block at (x-1, y, z) is solid
   - +Y face: Check if block at (x, y+1, z) is solid
   - -Y face: Check if block at (x, y-1, z) is solid
   - +Z face: Check if block at (x, y, z+1) is solid
   - -Z face: Check if block at (x, y, z-1) is solid
3. Solid blocks should use `BlockDefinition::IsOpaque()` check
4. Cross-chunk boundaries must query neighbor chunks correctly

**Correct Implementation Pattern:**
```cpp
void Chunk::AddBlockFacesWithHiddenSurfaceRemoval(/* ... */) {
    // For each block in chunk
    for (int z = 0; z < CHUNK_SIZE_Z; z++) {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) {
            for (int x = 0; x < CHUNK_SIZE_X; x++) {
                Block block = GetBlock(x, y, z);
                if (block.m_type == 0) continue;  // Skip air

                // Check each face direction
                // +X face
                IntVec3 neighborCoords = IntVec3(x+1, y, z);
                Block neighborBlock = GetBlockAtLocalCoords(neighborCoords);  // Handles cross-chunk
                if (!IsBlockOpaque(neighborBlock)) {
                    AddQuadToMesh(/* +X face vertices */);
                }

                // Repeat for -X, +Y, -Y, +Z, -Z faces...
            }
        }
    }
}
```

**Cross-Chunk Handling:**
```cpp
Block Chunk::GetBlockAtLocalCoords(IntVec3 const& localCoords) const {
    // If coords within chunk bounds, return directly
    if (IsWithinBounds(localCoords)) {
        return GetBlock(localCoords.x, localCoords.y, localCoords.z);
    }

    // If outside chunk, query neighbor chunk via World
    IntVec3 worldCoords = LocalToWorldCoords(localCoords);
    return m_world->GetBlockAtCoords(worldCoords);
}
```

**Performance Impact:**
- Properly culled chunks reduce vertex count by 40-60%
- Improves GPU performance (fewer triangles to rasterize)
- Critical for maintaining 60 FPS with large view distances

**Testing:**
1. Visual: Dig tunnel through solid blocks, ensure no "internal faces" visible
2. Performance: Measure vertex count before/after fix (should decrease significantly)
3. Profiling: GPU frame time should improve with fewer triangles

**Timing:** Should be fixed before A6 if rendering performance is poor, or after A6 if physics is higher priority

---

## Steering Document Alignment

### Technical Standards (tech.md)

**C++ Best Practices:**
- RAII for Camera resource management
- Virtual functions for polymorphic Entity behavior
- Const correctness for collision query methods
- Enum class for type-safe mode selections

**Engine Separation:**
- Entity/Player in `Code/Game/Gameplay/` (game-specific)
- Camera uses Engine class (no game code in Engine)
- Physics uses Engine math utilities (AABB3, Vec3, raycasts)

**Performance Standards:**
- Fixed physics timestep prevents tunneling (120 Hz)
- Corner raycasts O(12 * rayLength) vs brute AABB O(n³)
- Ground detection limited to 0.02m rays (not full height)

### Project Structure (structure.md)

**Module Organization:**
```
Code/Game/
├── Framework/
│   ├── GameCommon.hpp         # Physics constants (GRAVITY, FRICTION, JUMP_VELOCITY)
│   └── App.cpp                # Game loop calls Player::Update()
├── Gameplay/
│   ├── Entity.hpp/cpp         # Physics foundation (collision, gravity, friction)
│   ├── Player.hpp/cpp         # Input handling, camera control
│   └── World.hpp/cpp          # Collision queries (IsBlockSolid, PushEntityOutOfBlocks)
└── Definition/
    └── BlockDefinition.hpp    # Block solidity data
```

**Separation of Concerns:**
- `Entity.cpp`: Physics integration and collision (no input handling)
- `Player.cpp`: Input handling and camera control (no direct physics)
- `World.cpp`: Block queries and collision services (no entity logic)

## Code Reuse Analysis

### Existing Components to Leverage

**Engine Math Utilities:**
- **AABB3**: Entity physics bounds, collision queries
- **Vec3**: Position, velocity, acceleration vectors
- **EulerAngles**: Camera orientation (pitch, yaw, roll)
- **Mat44**: Transform matrices for camera view

**Engine Systems:**
- **Camera**: Full-featured camera class with FOV, transforms, projection
- **RaycastUtils**: 3D raycast functions for collision detection
- **DebugRenderSystem**: Wireframe visualization for AABB, rays, vectors

**Game Systems:**
- **World::RaycastVoxel()**: Amanatides & Woo algorithm (A5 implementation)
- **World::GetBlockAtCoords()**: Block type queries for solidity checks
- **Chunk::GetBlock()**: Direct block access for collision detection
- **BlockDefinition::IsOpaque()**: Determines solid vs non-solid blocks

### Integration Points

**Entity → World Interface:**
```cpp
// World provides collision services to Entity
bool World::IsBlockSolid(IntVec3 const& coords) const;
void World::PushEntityOutOfBlocks(Entity* entity);
bool World::IsEntityOnGround(Entity const* entity) const;
RaycastResult3D World::RaycastVoxel(Vec3 const& start, Vec3 const& direction, float maxDist) const;
```

**Player → Entity Inheritance:**
```cpp
// Player overrides Entity virtuals
void Player::Update(float deltaSeconds) override;    // Calls UpdateFromInput(), then Entity::Update()
void Player::Render() const override;                // Renders AABB debug, calls DebugRender
```

**Player → Camera Integration:**
```cpp
// Player owns and controls Camera position/orientation
Camera* Player::GetCamera() const;                   // Returns m_worldCamera
void Player::UpdateCamera();                          // Sets camera transform based on mode
```

## Architecture

### Modular Design Principles

**Single Responsibility:**
- **Entity**: Physics simulation only (gravity, friction, collision resolution)
- **Player**: Input interpretation and camera control only
- **World**: Voxel data and collision queries only
- **GameCommon.hpp**: Physics constants centralized (no magic numbers)

**Open/Closed Principle:**
- Entity physics system is closed for modification (final methods)
- Player extends Entity behavior through virtual Update() override
- Future NPCs extend Entity without modifying Entity.cpp

**Dependency Inversion:**
- Entity depends on World interface (not Chunk implementation)
- Player depends on Camera abstraction (not rendering details)
- World provides abstract collision queries (not internal structures)

### System Interaction Diagram

```mermaid
graph TD
    A[App::Update] --> B[Player::Update]
    B --> C[Player::UpdateFromInput]
    C --> D[Build Acceleration from WASD]
    B --> E[Entity::Update]
    E --> F[Entity::UpdatePhysics]
    F --> G[Apply Gravity]
    F --> H[Apply Friction]
    F --> I[Integrate Velocity]
    F --> J[World::PushEntityOutOfBlocks]
    J --> K[Corner Raycast Collision]
    K --> L[Zero Blocked Velocities]
    E --> M[Entity::UpdateIsGrounded]
    M --> N[World::IsEntityOnGround]
    N --> O[4-Corner Downward Raycasts]
    B --> P[Player::UpdateCamera]
    P --> Q[Calculate Camera Position by Mode]
    P --> R[Set Camera Transform]
```

### Physics Mode State Machine

```mermaid
stateDiagram-v2
    [*] --> WALKING
    WALKING --> FLYING: V key pressed
    FLYING --> NOCLIP: V key pressed
    NOCLIP --> WALKING: V key pressed

    note right of WALKING
        Gravity: ON
        Collision: ON
        Jump: Enabled when grounded
    end note

    note right of FLYING
        Gravity: OFF
        Collision: ON
        Jump: Disabled, Q/E for vertical
    end note

    note right of NOCLIP
        Gravity: OFF
        Collision: OFF
        Jump: Disabled, Q/E for vertical
    end note
```

### Camera Mode State Machine

```mermaid
stateDiagram-v2
    [*] --> FIRST_PERSON
    FIRST_PERSON --> OVER_SHOULDER: C key pressed
    OVER_SHOULDER --> SPECTATOR: C key pressed
    SPECTATOR --> SPECTATOR_XY: C key pressed
    SPECTATOR_XY --> INDEPENDENT: C key pressed
    INDEPENDENT --> FIRST_PERSON: C key pressed

    note right of FIRST_PERSON
        Camera at Player eye (1.65m)
        Player hidden
        Mouse controls Player orientation
    end note

    note right of OVER_SHOULDER
        Camera 4m behind Player
        Player visible
        Raycast prevents wall clipping
    end note

    note right of SPECTATOR
        Player frozen
        Camera moves with WASD (free 3D)
        Mouse controls Camera orientation
    end note

    note right of SPECTATOR_XY
        Player frozen
        Camera XY movement only
        Z position locked
    end note

    note right of INDEPENDENT
        Player moves normally
        Camera stays at spectator position
        Decoupled control
    end note
```

## Components and Interfaces

### Component 1: Entity (Physics Foundation)

**Purpose:** Base class providing Newtonian physics simulation for all moving game objects

**File:** `Code/Game/Gameplay/Entity.hpp`, `Entity.cpp`

**New Fields:**
```cpp
class Entity {
public:
    // Existing fields (A5)
    Vec3        m_position;
    Vec3        m_velocity;
    EulerAngles m_orientation;

    // NEW: Physics fields (A6)
    AABB3       m_physicsAABB;        // Collision bounds (local space)
    Vec3        m_acceleration;       // Accumulated forces this frame
    bool        m_isOnGround;         // Grounded state for jump/gravity
    PhysicsMode m_physicsMode;        // WALKING, FLYING, or NOCLIP

    // NEW: Physics configuration
    bool        m_physicsEnabled;     // Static objects disable physics
    float       m_gravityCoefficient; // Multiplier for gravity (1.0 default)
    float       m_frictionCoefficient;// Multiplier for friction (1.0 default)
};
```

**Public Interface:**
```cpp
// Virtual methods (derived classes override)
virtual void Update(float deltaSeconds) = 0;        // Game loop calls this
virtual void Render() const = 0;                     // Rendering

// Physics methods (final - derived classes cannot override)
void UpdatePhysics(float deltaSeconds);              // Applies gravity, friction, collision
void UpdateIsGrounded();                             // Updates m_isOnGround via raycasts
void AddForce(Vec3 const& force);                    // Accumulates into m_acceleration
void AddImpulse(Vec3 const& impulse);                // Instant velocity change (jump)

// Configuration
void SetPhysicsMode(PhysicsMode mode);
void SetPhysicsEnabled(bool enabled);
AABB3 GetWorldAABB() const;                          // Returns m_physicsAABB + m_position
```

**Dependencies:**
- World (for collision queries: `IsBlockSolid`, `PushEntityOutOfBlocks`, `IsEntityOnGround`)
- AABB3 (for physics bounds)
- Vec3, EulerAngles (for transforms)

**Reuses:**
- Engine::AABB3 for collision bounds
- Engine::RaycastUtils (indirectly through World)
- GameCommon physics constants

### Component 2: Player (Input and Camera Control)

**Purpose:** Handles player-specific input interpretation and camera management

**File:** `Code/Game/Gameplay/Player.hpp`, `Player.cpp`

**New Fields:**
```cpp
class Player : public Entity {
private:
    // Existing fields (A5)
    Camera* m_worldCamera;
    float   m_moveSpeed;

    // NEW: Extended camera system (A6)
    CameraMode  m_cameraMode;         // 5 modes: FIRST_PERSON, OVER_SHOULDER, etc.
    Vec3        m_spectatorPosition;  // Camera position when in spectator modes
    EulerAngles m_spectatorOrientation; // Camera orientation when in spectator modes

    // NEW: Input state tracking
    bool m_wasCKeyPressed;            // Debounce camera mode toggle
    bool m_wasVKeyPressed;            // Debounce physics mode toggle

    // NEW: Player dimensions
    static constexpr float PLAYER_HEIGHT = 1.80f;
    static constexpr float PLAYER_WIDTH = 0.60f;
    static constexpr float PLAYER_EYE_HEIGHT = 1.65f;
};
```

**Public Interface:**
```cpp
// Override Entity virtuals
void Update(float deltaSeconds) override;            // Calls UpdateFromInput, then Entity::Update
void Render() const override;                        // Renders AABB wireframe, debug info

// Player-specific methods
void UpdateFromInput(float deltaSeconds);            // Builds acceleration from WASD
void UpdateCamera();                                 // Updates camera position/orientation
Camera* GetCamera() const;                           // Returns m_worldCamera pointer
Vec3 GetEyePosition() const;                         // Returns m_position + Vec3(0,0,1.65f)

// Mode controls
void CycleCameraMode();                              // C key: cycles through 5 camera modes
void CyclePhysicsMode();                             // V key: cycles through 3 physics modes
```

**Dependencies:**
- Entity (base class)
- Camera (Engine class, owned pointer)
- InputSystem (for key states)
- Game (for world access, block interaction)

**Reuses:**
- Entity physics system (inherits Update, collision, grounded checks)
- Engine Camera class (no custom camera implementation)
- World::RaycastVoxel (for OVER_SHOULDER wall clipping prevention)

### Component 3: World (Collision Services)

**Purpose:** Provides collision detection and block query services to Entity

**File:** `Code/Game/Gameplay/World.hpp`, `World.cpp`

**New Methods:**
```cpp
class World {
public:
    // NEW: Entity collision services (A6)
    bool IsBlockSolid(IntVec3 const& coords) const;
    void PushEntityOutOfBlocks(Entity* entity);
    bool IsEntityOnGround(Entity const* entity) const;

    // Existing raycast (A5)
    RaycastResult3D RaycastVoxel(Vec3 const& start, Vec3 const& direction, float maxDist) const;

    // Existing block queries (A5)
    Block GetBlockAtCoords(IntVec3 const& coords) const;
    BlockDefinition const* GetBlockDefAtCoords(IntVec3 const& coords) const;
};
```

**IsBlockSolid() Algorithm:**
```
1. Get Block at coords using GetBlockAtCoords()
2. If block.m_type == AIR (0), return false
3. Get BlockDefinition* using GetBlockDefAtCoords()
4. Return blockDef->IsOpaque() (solid blocks are opaque)
```

**PushEntityOutOfBlocks() Algorithm:**
```
1. Get entity world AABB (entity->GetWorldAABB())
2. Calculate IntVec3 min/max coords overlapping AABB
3. For each block coord in AABB range:
   a. If !IsBlockSolid(coord), skip
   b. Calculate block AABB (1x1x1 at coord)
   c. Calculate penetration depth on X, Y, Z axes
   d. Find minimum penetration axis
   e. Push entity position along that axis
4. Repeat until no overlaps (max 3 iterations)
5. Zero entity velocity on pushed axes
```

**IsEntityOnGround() Algorithm:**
```
1. Get entity bottom AABB corners (4 corners at Z_min)
2. For each corner:
   a. Start ray slightly above corner (+ Vec3(0,0,0.01f))
   b. Cast downward ray (direction = -Vec3::Z_AXIS)
   c. Ray length = 2 * RAYCAST_OFFSET (0.02m total)
   d. If RaycastVoxel hits solid block, return true
3. If no rays hit, return false
```

**Dependencies:**
- Chunk (for block data access)
- BlockDefinition (for block properties)
- Entity (parameter type for collision services)
- RaycastUtils (for voxel raycasts)

**Reuses:**
- Existing chunk management system
- Existing RaycastVoxel implementation (A5)
- BlockDefinition::IsOpaque() for solidity checks

### Component 4: GameCamera (Camera Management)

**Purpose:** Manages camera positioning and orientation based on active camera mode

**Implementation:** Embedded in `Player.cpp` (not a separate class)

**Camera Mode Behavior:**

**FIRST_PERSON:**
- Position: `player.m_position + Vec3(0, 0, PLAYER_EYE_HEIGHT)`
- Orientation: `player.m_orientation` (controlled by mouse)
- Player rendering: Disabled (self not visible)

**OVER_SHOULDER:**
- Target position: Eye position - (forward * 4.0f)
- Actual position: Raycast from eye to target, place at impact point
- Orientation: `player.m_orientation`
- Player rendering: Enabled (see self from behind)

**SPECTATOR:**
- Position: `m_spectatorPosition` (independent from player)
- Orientation: `m_spectatorOrientation`
- Movement: WASD moves camera directly (full 3D freedom)
- Player: Frozen (no input applied to player)

**SPECTATOR_XY:**
- Position: `Vec3(m_spectatorPosition.x, m_spectatorPosition.y, lockedZ)`
- Orientation: `m_spectatorOrientation`
- Movement: WASD moves camera in XY plane only
- Player: Frozen

**INDEPENDENT:**
- Position: `m_spectatorPosition` (stays at last spectator position)
- Orientation: `m_spectatorOrientation`
- Player: Moves normally with input
- Camera: Static, does not track player

**Camera Update Flow:**
```cpp
void Player::UpdateCamera() {
    switch (m_cameraMode) {
        case FIRST_PERSON:
            m_worldCamera->SetTransform(GetEyePosition(), m_orientation);
            break;

        case OVER_SHOULDER:
            Vec3 targetPos = GetEyePosition() - (GetForward() * 4.0f);
            RaycastResult3D result = m_game->GetWorld()->RaycastVoxel(
                GetEyePosition(), -GetForward(), 4.0f
            );
            Vec3 cameraPos = result.m_didImpact ? result.m_impactPos : targetPos;
            m_worldCamera->SetTransform(cameraPos, m_orientation);
            break;

        case SPECTATOR:
        case SPECTATOR_XY:
        case INDEPENDENT:
            m_worldCamera->SetTransform(m_spectatorPosition, m_spectatorOrientation);
            break;
    }
}
```

## Data Models

### PhysicsMode Enum

```cpp
enum class PhysicsMode : uint8_t {
    WALKING,    // Full physics: gravity ON, collision ON, jump enabled
    FLYING,     // Creative mode: gravity OFF, collision ON, Q/E vertical
    NOCLIP      // Debug mode: gravity OFF, collision OFF, pass through blocks
};
```

### CameraMode Enum

```cpp
enum class CameraMode : uint8_t {
    FIRST_PERSON,   // Camera at eye, player hidden
    OVER_SHOULDER,  // Camera 4m behind, player visible
    SPECTATOR,      // Player frozen, camera free 3D movement
    SPECTATOR_XY,   // Player frozen, camera XY movement only
    INDEPENDENT     // Player moves, camera stays at spectator position
};
```

### Entity Physics State

```cpp
struct EntityPhysicsState {
    Vec3        position;           // World position (feet)
    Vec3        velocity;           // Current velocity (m/s)
    Vec3        acceleration;       // Accumulated forces (m/s²)
    AABB3       physicsAABB;        // Collision bounds (local space)
    bool        isOnGround;         // Grounded state
    PhysicsMode physicsMode;        // Current physics mode
};
```

### Player Dimensions

```cpp
struct PlayerDimensions {
    static constexpr float HEIGHT = 1.80f;      // Total height (meters)
    static constexpr float WIDTH = 0.60f;       // XY width (meters)
    static constexpr float EYE_HEIGHT = 1.65f;  // Eye offset from feet
    static constexpr float HALF_WIDTH = 0.30f;  // Half-extents for AABB
    static constexpr float HALF_HEIGHT = 0.90f; // Half-extents for AABB
};

// Player AABB construction:
// Center: Vec3(0, 0, 0.90f) relative to m_position
// Half-extents: Vec3(0.30f, 0.30f, 0.90f)
```

## Physics Constants (GameCommon.hpp)

```cpp
//----------------------------------------------------------------------------------------------------
// Physics System Constants (Assignment 6)
//----------------------------------------------------------------------------------------------------

// Gravity
constexpr float GRAVITY_ACCELERATION = -20.0f;  // m/s² (downward)

// Friction coefficients
constexpr float FRICTION_GROUND = 10.0f;        // Strong damping when grounded
constexpr float FRICTION_AIR = 1.0f;            // Weak damping when airborne

// Player movement
constexpr float PLAYER_WALK_ACCELERATION = 40.0f;    // m/s²
constexpr float PLAYER_SPRINT_MULTIPLIER = 20.0f;    // Shift key multiplier
constexpr float PLAYER_JUMP_VELOCITY = 8.5f;         // Instant Z velocity boost
constexpr float PLAYER_MAX_HORIZONTAL_SPEED = 20.0f; // Speed cap

// Player dimensions
constexpr float PLAYER_HEIGHT = 1.80f;          // Meters
constexpr float PLAYER_WIDTH = 0.60f;           // Meters
constexpr float PLAYER_EYE_HEIGHT = 1.65f;      // Eye offset from feet

// Collision detection
constexpr float RAYCAST_OFFSET = 0.01f;         // Epsilon for corner raycasts
constexpr float GROUND_CHECK_DISTANCE = 0.02f;  // Ground detection ray length
constexpr int MAX_PUSH_ITERATIONS = 3;          // Max iterations for PushEntityOutOfBlocks

// Camera
constexpr float CAMERA_OVER_SHOULDER_DISTANCE = 4.0f;  // Meters behind player
constexpr float CAMERA_MOUSE_SENSITIVITY = 0.075f;     // Degrees per pixel
constexpr float CAMERA_PITCH_CLAMP_MIN = -85.0f;       // Prevent gimbal lock
constexpr float CAMERA_PITCH_CLAMP_MAX = 85.0f;

// Physics timestep
constexpr float PHYSICS_FIXED_TIMESTEP = 1.0f / 120.0f; // 120 Hz (prevents tunneling)
```

## Algorithms

### Algorithm 1: Corner Raycast Collision Detection

**Purpose:** Detect and resolve entity-block collisions using 12-corner raycast method

**Input:**
- Entity AABB (world space)
- Desired delta position (movement this frame)

**Output:**
- Corrected delta position (blocked axes zeroed)
- Updated entity velocity (blocked axes zeroed)

**Steps:**
```
1. Calculate 12 corner positions from entity AABB:
   - 4 bottom corners (Z_min)
   - 4 mid-body corners (Z_center)
   - 4 top corners (Z_max)
   - For XY: (X_min, Y_min), (X_max, Y_min), (X_min, Y_max), (X_max, Y_max)

2. For each corner:
   a. Ray start = corner + (raycastOffset in delta direction)
   b. Ray direction = deltaPosition.GetNormalized()
   c. Ray length = deltaPosition.GetLength() + raycastOffset
   d. Cast ray using World::RaycastVoxel()
   e. If impact with solid block:
      - Store impact distance
      - Store impact normal

3. Analyze impacts per axis:
   X-axis: Find closest impact with normal.x != 0
   Y-axis: Find closest impact with normal.y != 0
   Z-axis: Find closest impact with normal.z != 0

4. For each blocked axis:
   a. Zero deltaPosition component
   b. Zero entity velocity component

5. Apply remaining (unblocked) deltaPosition to entity position
```

**Complexity:** O(12 * rayLength) - Linear in ray distance, constant in block count

**Advantages over AABB Sweep:**
- No tunneling through thin walls (raycasts always detect)
- Cheaper than swept AABB (no continuous collision detection)
- Works with non-axis-aligned movement

### Algorithm 2: Ground Detection (4-Corner Raycast)

**Purpose:** Determine if entity is standing on solid ground

**Input:**
- Entity AABB (world space)

**Output:**
- Boolean: true if grounded, false if airborne

**Steps:**
```
1. Get bottom 4 corners of entity AABB:
   corners = [
       (AABB.min.x, AABB.min.y, AABB.min.z),
       (AABB.max.x, AABB.min.y, AABB.min.z),
       (AABB.min.x, AABB.max.y, AABB.min.z),
       (AABB.max.x, AABB.max.y, AABB.min.z)
   ]

2. For each corner:
   a. Ray start = corner + Vec3(0, 0, RAYCAST_OFFSET)  // Lift slightly above floor
   b. Ray direction = Vec3(0, 0, -1)  // Straight down
   c. Ray length = 2 * RAYCAST_OFFSET  // Very short ray (0.02m total)
   d. Cast ray using World::RaycastVoxel()
   e. If ray hits solid block:
      return true  // Grounded

3. If no rays hit:
   return false  // Airborne
```

**Rationale for 4 Corners:**
- Detects grounded when standing on edge of block
- Detects grounded when straddling 2+ blocks
- Single center point would fail on edges

**Rationale for Short Rays:**
- Performance: Only checks immediate vicinity (not full height)
- Accuracy: Prevents false positives from blocks far below

### Algorithm 3: Newtonian Physics Integration (Euler Method)

**Purpose:** Update entity position and velocity based on forces

**Input:**
- Current position, velocity, acceleration
- Delta time

**Output:**
- Updated position and velocity

**Steps:**
```
1. Apply gravity (if applicable):
   IF (physicsMode == WALKING && !isOnGround):
       acceleration.z += GRAVITY_ACCELERATION

2. Apply friction:
   Vec2 horizontalVelocity = Vec2(velocity.x, velocity.y)
   float frictionCoeff = isOnGround ? FRICTION_GROUND : FRICTION_AIR
   Vec2 frictionForce = -horizontalVelocity * frictionCoeff
   acceleration.x += frictionForce.x
   acceleration.y += frictionForce.y

3. Integrate velocity (Euler method):
   velocity += acceleration * deltaTime

4. Clamp horizontal speed:
   Vec2 horizontalVel = Vec2(velocity.x, velocity.y)
   IF (horizontalVel.GetLength() > PLAYER_MAX_HORIZONTAL_SPEED):
       horizontalVel = horizontalVel.GetNormalized() * PLAYER_MAX_HORIZONTAL_SPEED
       velocity.x = horizontalVel.x
       velocity.y = horizontalVel.y

5. Calculate delta position:
   deltaPosition = velocity * deltaTime

6. Collision detection and resolution:
   IF (physicsMode != NOCLIP):
       PerformCornerRaycastCollision(deltaPosition)  // Modifies deltaPosition and velocity

7. Integrate position:
   position += deltaPosition

8. Reset acceleration for next frame:
   acceleration = Vec3::ZERO
```

**Why Euler (not Verlet/RK4):**
- Simple and fast (good for 60 FPS target)
- Sufficient accuracy for gameplay (not scientific simulation)
- Industry standard for game physics

### Algorithm 4: Push Entity Out of Blocks

**Purpose:** Resolve entity stuck in blocks (e.g., after teleport, chunk load)

**Input:**
- Entity with world AABB

**Output:**
- Entity position adjusted to nearest non-overlapping position

**Steps:**
```
1. Get entity world AABB bounds
2. Calculate IntVec3 min/max block coords overlapping AABB
3. iterations = 0
4. WHILE (overlaps exist && iterations < MAX_PUSH_ITERATIONS):
   a. FOR each block coord in [min, max] range:
      - IF !IsBlockSolid(coord), continue
      - Calculate block AABB (1x1x1 cube at coord)
      - IF entity AABB intersects block AABB:
         * Calculate penetration on X, Y, Z axes
         * Find minimum penetration axis
         * Push entity position along that axis to resolve
         * Break (only fix one overlap per iteration)
   b. Zero entity velocity on pushed axes
   c. iterations++
5. IF (iterations >= MAX_PUSH_ITERATIONS):
   DebugLog("Entity stuck in blocks after max iterations")
```

**Why Iterative:**
- Single push may create new overlaps
- Max 3 iterations prevents infinite loops
- Each iteration resolves shortest penetration (optimal)

## Error Handling

### Error Scenario 1: Entity Tunneling Through Blocks

**Description:** Fast-moving entity skips over thin walls (deltaPosition > block thickness)

**Handling:**
- Use fixed physics timestep (120 Hz) to limit max deltaPosition per step
- At 120 Hz with max speed 20 m/s: max distance = 20 / 120 = 0.166m per step
- This prevents skipping 1-block (1m) thick walls
- Corner raycasts detect all intermediate blocks along path

**User Impact:**
- None (prevented by design)
- Worst case: entity slows down slightly when approaching max speed

### Error Scenario 2: Camera Clipping Through Walls (OVER_SHOULDER)

**Description:** Camera placed 4m behind player ends up inside blocks

**Handling:**
```cpp
// Raycast from eye to desired camera position
Vec3 eyePos = GetEyePosition();
Vec3 desiredCameraPos = eyePos - (GetForward() * 4.0f);
RaycastResult3D result = World::RaycastVoxel(eyePos, -GetForward(), 4.0f);

if (result.m_didImpact) {
    // Wall detected - place camera at impact point (slightly offset)
    Vec3 actualCameraPos = result.m_impactPos + (result.m_impactNormal * 0.1f);
    m_worldCamera->SetTransform(actualCameraPos, m_orientation);
} else {
    // No wall - use full 4m distance
    m_worldCamera->SetTransform(desiredCameraPos, m_orientation);
}
```

**User Impact:**
- Camera automatically pulls forward when backing into walls
- Prevents seeing "inside" of blocks (immersion maintained)

### Error Scenario 3: Entity Falls While Chunk Below is Loading

**Description:** Player walks off edge of loaded chunks, chunk below not yet activated

**Handling:**
```cpp
void Entity::UpdatePhysics(float deltaSeconds) {
    // Check if chunk below exists
    IntVec3 chunkCoordsBelow = World::GetChunkCoordsForWorldPos(m_position + Vec3(0,0,-5));
    Chunk* chunkBelow = m_game->GetWorld()->GetChunkAtCoords(chunkCoordsBelow);

    if (chunkBelow == nullptr || chunkBelow->GetState() != ChunkState::ACTIVE) {
        // Chunk not loaded - freeze vertical velocity
        m_velocity.z = 0.0f;
        m_acceleration.z = 0.0f;
        return;  // Skip physics this frame
    }

    // Normal physics continues...
}
```

**User Impact:**
- Player hovers at chunk boundary until chunk loads
- Better than falling into void or crashing
- Visual feedback: "Loading chunks..." message

### Error Scenario 4: Getting Stuck in Blocks After Mode Switch

**Description:** Switching from NOCLIP to WALKING/FLYING while inside blocks

**Handling:**
```cpp
void Player::CyclePhysicsMode() {
    // Old mode
    PhysicsMode oldMode = m_physicsMode;

    // Cycle to next mode
    m_physicsMode = static_cast<PhysicsMode>((static_cast<int>(m_physicsMode) + 1) % 3);

    // If switching from NOCLIP to collision-enabled mode
    if (oldMode == PhysicsMode::NOCLIP && m_physicsMode != PhysicsMode::NOCLIP) {
        // Attempt to push out of blocks
        m_game->GetWorld()->PushEntityOutOfBlocks(this);

        // If still stuck after push, revert to NOCLIP
        if (IsStuckInBlocks()) {
            m_physicsMode = PhysicsMode::NOCLIP;
            DebugLog("Cannot exit NOCLIP - stuck in blocks");
        }
    }
}
```

**User Impact:**
- Safe mode switching (no instant death)
- Clear feedback when stuck
- Must navigate to free space before exiting NOCLIP

### Error Scenario 5: Diagonal Movement Speed Exploit

**Description:** Pressing W+D simultaneously gives √2 faster movement (vector addition)

**Handling:**
```cpp
void Player::UpdateFromInput(float deltaSeconds) {
    Vec3 localMovement = Vec3::ZERO;

    if (IsKeyDown('W')) localMovement.x += 1.0f;  // Forward
    if (IsKeyDown('S')) localMovement.x -= 1.0f;  // Backward
    if (IsKeyDown('D')) localMovement.y += 1.0f;  // Right
    if (IsKeyDown('A')) localMovement.y -= 1.0f;  // Left

    // FIX: Normalize before applying speed
    if (localMovement.GetLengthSquared() > 0.0f) {
        localMovement = localMovement.GetNormalized();  // Length = 1.0
    }

    // Transform to world space and apply acceleration
    Vec3 worldMovement = TransformToWorldSpace(localMovement);
    m_acceleration += worldMovement * PLAYER_WALK_ACCELERATION;
}
```

**User Impact:**
- Fair movement speed in all directions
- No exploits or advantages from diagonal movement

## Testing Strategy

### Unit Testing

**Entity Physics Tests:**
```cpp
// Test 1: Gravity applies only when not grounded in WALKING mode
Entity* entity = new Entity(game);
entity->SetPhysicsMode(PhysicsMode::WALKING);
entity->m_isOnGround = false;
entity->UpdatePhysics(0.1f);  // 100ms
EXPECT_EQ(entity->m_velocity.z, GRAVITY_ACCELERATION * 0.1f);

// Test 2: Friction decelerates horizontal velocity
entity->m_velocity = Vec3(10.0f, 0.0f, 0.0f);
entity->m_isOnGround = true;
entity->UpdatePhysics(0.1f);
EXPECT_LT(entity->m_velocity.x, 10.0f);  // Should slow down

// Test 3: NOCLIP mode skips collision
entity->SetPhysicsMode(PhysicsMode::NOCLIP);
entity->m_position = Vec3(0, 0, 0);  // Inside ground
entity->UpdatePhysics(0.1f);
EXPECT_EQ(entity->m_position.z, entity->m_velocity.z * 0.1f);  // Moved without collision
```

**Ground Detection Tests:**
```cpp
// Test 4: Grounded when standing on block
World* world = CreateTestWorld();  // World with flat ground at Z=0
Entity* entity = new Entity(game);
entity->m_position = Vec3(0, 0, 1.0f);  // Standing on ground
entity->UpdateIsGrounded();
EXPECT_TRUE(entity->m_isOnGround);

// Test 5: Airborne when no blocks below
entity->m_position = Vec3(0, 0, 10.0f);  // High in air
entity->UpdateIsGrounded();
EXPECT_FALSE(entity->m_isOnGround);

// Test 6: Grounded when on edge of block
entity->m_position = Vec3(0.95f, 0.95f, 1.0f);  // Corner of block
entity->UpdateIsGrounded();
EXPECT_TRUE(entity->m_isOnGround);  // 4-corner detection catches this
```

**Collision Detection Tests:**
```cpp
// Test 7: Entity stops when hitting wall
entity->m_position = Vec3(5.5f, 0, 1);  // Next to wall at X=6
entity->m_velocity = Vec3(2.0f, 0, 0);  // Moving toward wall
entity->UpdatePhysics(1.0f);
EXPECT_LT(entity->m_position.x, 6.0f);  // Stopped before wall
EXPECT_EQ(entity->m_velocity.x, 0.0f);  // Velocity zeroed

// Test 8: Entity slides along walls
entity->m_velocity = Vec3(1.0f, 1.0f, 0);  // Diagonal into wall
entity->UpdatePhysics(1.0f);
EXPECT_LT(entity->m_velocity.x, 1.0f);  // X blocked
EXPECT_EQ(entity->m_velocity.y, 1.0f);  // Y unblocked, continues
```

### Integration Testing

**Player Movement Integration:**
```cpp
// Test 9: Jump only works when grounded in WALKING mode
Player* player = new Player(game);
player->SetPhysicsMode(PhysicsMode::WALKING);
player->m_position = Vec3(0, 0, 1);
player->UpdateIsGrounded();  // Should be grounded on flat world

// Simulate spacebar press
player->m_isJumpKeyPressed = true;
player->UpdateFromInput(0.016f);  // 60 FPS frame
EXPECT_EQ(player->m_velocity.z, PLAYER_JUMP_VELOCITY);

// Test 10: Jump doesn't work when airborne
player->m_velocity.z = 0.0f;
player->m_isOnGround = false;
player->UpdateFromInput(0.016f);
EXPECT_EQ(player->m_velocity.z, 0.0f);  // No double-jump
```

**Camera Mode Integration:**
```cpp
// Test 11: FIRST_PERSON camera at eye position
player->SetCameraMode(CameraMode::FIRST_PERSON);
player->m_position = Vec3(0, 0, 0);
player->UpdateCamera();
Vec3 expectedCameraPos = Vec3(0, 0, PLAYER_EYE_HEIGHT);
EXPECT_VEC3_NEAR(player->GetCamera()->GetPosition(), expectedCameraPos, 0.01f);

// Test 12: OVER_SHOULDER camera 4m behind (no walls)
player->SetCameraMode(CameraMode::OVER_SHOULDER);
player->m_orientation = EulerAngles(0, 0, 0);  // Facing +X
player->UpdateCamera();
Vec3 expectedCameraPos = Vec3(-4.0f, 0, PLAYER_EYE_HEIGHT);
EXPECT_VEC3_NEAR(player->GetCamera()->GetPosition(), expectedCameraPos, 0.01f);
```

**World Collision Service Integration:**
```cpp
// Test 13: PushEntityOutOfBlocks resolves overlaps
Entity* entity = new Entity(game);
entity->m_physicsAABB = AABB3(Vec3(-0.3f, -0.3f, 0), Vec3(0.3f, 0.3f, 1.8f));
entity->m_position = Vec3(5.5f, 5.5f, 0.5f);  // Overlapping block at (5,5,0)

world->PushEntityOutOfBlocks(entity);
AABB3 worldAABB = entity->GetWorldAABB();
// Entity should be pushed out - no longer overlapping solid blocks
EXPECT_FALSE(WorldAABBOverlapsSolidBlocks(world, worldAABB));
```

### End-to-End Testing

**E2E Test 1: Player Navigation**
```
1. Start game in WALKING mode, FIRST_PERSON camera
2. Press W key for 2 seconds → Player moves forward smoothly
3. Press Space → Player jumps ~1.2m high, lands smoothly
4. Walk off cliff → Player falls with gravity, lands with impact (velocity zeroed)
5. Expected: Smooth movement, no jitter, no tunneling through blocks
```

**E2E Test 2: Camera Mode Cycling**
```
1. Press C key → Camera switches to OVER_SHOULDER
   - Verify: See player model from behind
2. Press C key → Camera switches to SPECTATOR
   - Verify: Player frozen, WASD moves camera freely
3. Press C key → Camera switches to SPECTATOR_XY
   - Verify: WASD moves camera in XY only, Z locked
4. Press C key → Camera switches to INDEPENDENT
   - Verify: Player moves, camera stays at spectator position
5. Press C key → Camera switches back to FIRST_PERSON
Expected: All 5 modes work correctly, smooth transitions
```

**E2E Test 3: Physics Mode Cycling**
```
1. Start in WALKING mode → Walk on ground, affected by gravity
2. Press V → Switch to FLYING mode
   - Verify: No gravity, can press Q/E to move up/down
   - Verify: Still collides with blocks (cannot enter walls)
3. Press V → Switch to NOCLIP mode
   - Verify: Can fly through walls and blocks
   - Verify: Q/E still work for vertical movement
4. Press V → Switch back to WALKING mode
   - Verify: Gravity resumes, falls if airborne
Expected: All 3 modes work correctly, no crashes or stuck states
```

**E2E Test 4: Stress Test (Performance)**
```
1. Generate world with many chunks active
2. Jump off tall mountain (100+ meter fall)
3. Hold Sprint (Shift) and run diagonally across terrain
4. Rapidly toggle between camera modes (mash C key)
5. Rapidly toggle between physics modes (mash V key)
Expected: 60 FPS maintained, no crashes, no tunneling
```

**E2E Test 5: Edge Case Coverage**
```
1. Enable NOCLIP, fly into solid blocks
2. Switch to WALKING while inside blocks
   - Expected: Either pushed out, or mode switch prevented
3. Stand on 1-block-wide pillar edge
   - Expected: Grounded detection works (4-corner method)
4. Back into wall in OVER_SHOULDER mode
   - Expected: Camera pulls forward, doesn't clip through wall
5. Walk off chunk boundary (unloaded chunks)
   - Expected: Player freezes or falls gracefully (no crash)
```

## Performance Optimizations

### Optimization 1: Fixed Physics Timestep

**Problem:** Variable delta time causes inconsistent physics, tunneling at high framerates

**Solution:**
```cpp
void Player::Update(float deltaSeconds) {
    // Clamp delta time to prevent physics instability
    float clampedDelta = Clamp(deltaSeconds, 1.0f/120.0f, 1.0f/15.0f);

    UpdateFromInput(clampedDelta);
    Entity::Update(clampedDelta);  // Calls UpdatePhysics internally
    UpdateCamera();
}
```

**Benefit:**
- Prevents tunneling (max deltaPosition = 0.166m at 20 m/s)
- Consistent physics regardless of framerate
- Overhead: <1ms per frame

### Optimization 2: Ground Detection Ray Length

**Problem:** Full-height raycasts (1.8m) every frame wasteful

**Solution:**
- Use 0.02m rays (2 * RAYCAST_OFFSET)
- Only detect immediate ground contact
- 90x shorter rays = 90x faster raycasts

**Benefit:**
- Reduces ground check from ~2ms to <0.05ms
- Still accurate (detects all grounded cases)

### Optimization 3: Corner Raycast vs AABB Overlap

**Problem:** Checking every block in AABB range is O(n³)

**Solution:**
- 12 corner raycasts = O(12 * rayLength)
- Typical rayLength ~0.3m at 60 FPS → ~4 blocks checked per ray
- Total: 48 block checks vs 1000+ for brute AABB

**Benefit:**
- 20x faster collision detection
- No false negatives (raycasts always detect)

### Optimization 4: Early Exit for NOCLIP

**Problem:** Running full physics logic when collision disabled

**Solution:**
```cpp
void Entity::UpdatePhysics(float deltaSeconds) {
    if (m_physicsMode == PhysicsMode::NOCLIP) {
        // Skip gravity, friction, collision
        m_position += m_velocity * deltaSeconds;
        m_acceleration = Vec3::ZERO;
        return;
    }

    // Full physics for WALKING/FLYING modes...
}
```

**Benefit:**
- NOCLIP mode runs at <0.1ms (vs 1-2ms for full physics)
- Useful for spectator cameras, debug flying

### Optimization 5: Camera Update Only When Active

**Problem:** Updating camera transform every frame wasteful if not rendering

**Solution:**
```cpp
void Player::UpdateCamera() {
    // Only update if camera is the active view camera
    if (m_worldCamera != m_game->GetActiveCamera()) {
        return;
    }

    // Calculate and set camera transform...
}
```

**Benefit:**
- Saves ~0.2ms when player camera not active
- Useful for splitscreen or cinematic cameras

## Migration and Deployment

### Phase 1: Entity Physics Foundation (Week 1)

**Tasks:**
1. Add physics fields to Entity.hpp (AABB3, acceleration, isOnGround, physicsMode)
2. Implement Entity::UpdatePhysics() (gravity, friction, Euler integration)
3. Implement Entity::UpdateIsGrounded() (4-corner raycast)
4. Add physics constants to GameCommon.hpp
5. Test: Entity falls with gravity, friction decelerates

**Deliverable:** Entity class with working physics simulation

### Phase 2: World Collision Services (Week 1)

**Tasks:**
1. Implement World::IsBlockSolid() (block type + opacity check)
2. Implement World::PushEntityOutOfBlocks() (iterative push algorithm)
3. Implement World::IsEntityOnGround() (4 downward raycasts)
4. Test: Entity stops at walls, doesn't tunnel, detects ground correctly

**Deliverable:** World provides collision services to Entity

### Phase 3: Player Input and Movement (Week 2)

**Tasks:**
1. Update Player::UpdateFromInput() (build acceleration from WASD)
2. Add jump logic (Space key, only when grounded)
3. Add sprint modifier (Shift key, 20x multiplier)
4. Implement physics mode cycling (V key)
5. Test: Player moves smoothly, jumps work, modes switch correctly

**Deliverable:** Player with full input-driven movement

### Phase 4: Camera System (Week 2)

**Tasks:**
1. Add CameraMode enum and m_cameraMode field
2. Implement 5 camera modes in UpdateCamera()
3. Add camera mode cycling (C key)
4. Implement OVER_SHOULDER wall clipping prevention
5. Test: All 5 camera modes work, smooth transitions

**Deliverable:** Complete camera system with all modes

### Phase 5: Debug Visualization (Week 3)

**Tasks:**
1. Render Entity AABB wireframe (cyan)
2. Render ground detection rays (green/red)
3. Add on-screen mode display (camera, physics)
4. Add FPS counter
5. Test: Debug visuals help diagnose issues

**Deliverable:** Full debug visualization suite

### Phase 6: Polish and Performance (Week 3)

**Tasks:**
1. Add DebugInline and FastBreak build configs
2. Profile physics system (target <2ms per frame)
3. Fix any jitter or tunneling issues
4. Validate 60 FPS in Release mode
5. Final testing and bug fixes

**Deliverable:** Production-ready A6 implementation

## Acceptance Criteria Summary

### Functional Requirements
- ✅ Entity base class provides physics for all moving objects
- ✅ Player derives from Entity, inherits physics behavior
- ✅ Newtonian physics (gravity, friction, acceleration)
- ✅ Entity-world collision detection prevents entering blocks
- ✅ Ground detection enables jumping and gravity
- ✅ WASD controls with sprint (Shift) and jump (Space)
- ✅ 5 camera modes cycle with C key
- ✅ 3 physics modes cycle with V key
- ✅ Debug visualization (AABB, rays, on-screen info)

### Non-Functional Requirements
- ✅ 60 FPS in Release mode during gameplay
- ✅ No compiler warnings
- ✅ Clean Entity→Player inheritance (physics in base class)
- ✅ All 4 build configs build successfully
- ✅ No tunneling through blocks (up to 50m fall)
- ✅ No jitter when standing or walking

### Code Quality
- ✅ Physics constants in GameCommon.hpp (no magic numbers)
- ✅ Single responsibility (Entity=physics, Player=input, World=collision)
- ✅ Clean interfaces (World provides collision services)
- ✅ Extensible design (NPCs use same physics system)
- ✅ Comprehensive debug visualization for development
