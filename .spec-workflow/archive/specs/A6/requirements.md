# Requirements Document - Assignment 6: Player, Camera, and Physics

## Introduction

Assignment 6 transforms SimpleMiner from a basic free-fly navigation system into a fully-featured voxel game with realistic player physics, multiple camera modes, and comprehensive collision detection. This assignment implements Newtonian physics, entity-based architecture, and sophisticated camera control systems to create a Minecraft-like gameplay experience.

### Current State Analysis

**Existing Foundation (A5 Completed):**
- ✅ Entity base class with position/velocity fields
- ✅ Player class deriving from Entity
- ✅ Engine Camera class with full transform support
- ✅ World raycast system (Amanatides & Woo algorithm)
- ✅ WASD movement with mouse look
- ✅ Block digging/placing with raycasts
- ✅ Engine math utilities (AABB3, Vec3, EulerAngles, Mat44)

**Missing Components (A6 Requirements):**
- ❌ No physics system (gravity, collision, acceleration)
- ❌ No player collision bounds (AABB3 physics body)
- ❌ No physics modes (Walking, Flying, NoClip)
- ❌ No camera modes (FirstPerson, OverShoulder, Spectator variants)
- ❌ No ground detection or jumping
- ❌ No friction or velocity damping
- ❌ No entity-world collision resolution

### Purpose and Value

This assignment delivers:
1. **Realistic Physics** - Gravity, acceleration, friction creating believable movement
2. **Solid Collisions** - Players cannot enter blocks, preventing frustrating glitches
3. **Flexible Camera** - Multiple viewpoints for gameplay, debugging, and content creation
4. **Foundation for NPCs** - Entity-based physics supports future AI entities
5. **Professional Feel** - Smooth movement and responsive controls matching AAA standards

## Alignment with Product Vision

SimpleMiner aims to be a educational Minecraft-like voxel engine demonstrating modern game development practices. Assignment 6 directly supports this vision by:

- **Professional Physics** - Implementing industry-standard collision detection and response
- **Modular Architecture** - Entity system enables future expansion (NPCs, projectiles, vehicles)
- **Debug Capabilities** - Multiple camera/physics modes facilitate development and testing
- **Performance Focus** - Maintaining 60 FPS despite complex physics calculations
- **Code Quality** - Clean separation of physics, camera, and input systems

---

## Requirements

### Requirement 1: Entity Physics Foundation

**User Story:** As a game developer, I want Entity to be the physics foundation for all moving objects, so that Player, NPCs, and projectiles share consistent physics behavior.

#### Acceptance Criteria

1. WHEN Entity is constructed THEN it SHALL initialize physics fields (position, velocity, acceleration, bounds)
2. WHEN Entity::Update() is called THEN it SHALL apply physics integration (gravity, friction, collision)
3. IF Entity has m_physicsEnabled = false THEN physics SHALL be skipped (static objects)
4. WHEN Entity collides with solid blocks THEN velocity SHALL be zeroed along collision axes
5. IF Entity is in WALKING mode AND not grounded THEN gravity SHALL be applied
6. WHEN Entity is in FLYING mode THEN gravity SHALL NOT be applied
7. IF Entity is in NOCLIP mode THEN collision detection SHALL be skipped

**Technical Details:**
- Add fields: `AABB3 m_physicsAABB`, `Vec3 m_acceleration`, `bool m_isOnGround`, `PhysicsMode m_physicsMode`
- Physics constants in GameCommon.hpp: GRAVITY_ACCELERATION = -20.0f, FRICTION_GROUND = 10.0f, FRICTION_AIR = 1.0f
- Update order: UpdateInput() → UpdatePhysics() → UpdateCamera()

---

### Requirement 2: Player Derives from Entity

**User Story:** As a player, I want my character to exhibit realistic physics, so that movement feels solid and predictable.

#### Acceptance Criteria

1. WHEN Player is created THEN it SHALL be 1.80m tall and 0.60m wide (AABB3 bounds)
2. WHEN Player updates THEN Entity::Update() SHALL handle physics automatically
3. WHEN Player is constructed THEN eye position SHALL be 1.65m above bottom center
4. IF Player presses V key THEN physics mode SHALL cycle (WALKING → FLYING → NOCLIP)
5. WHEN Player is in any physics mode THEN Entity physics logic SHALL still run
6. IF Player moves THEN physics SHALL be Newtonian (velocity-based with acceleration/deceleration)

**Technical Details:**
- Player AABB3: center at (0, 0, 0.90m), half-extents (0.30m, 0.30m, 0.90m)
- Eye offset: Vec3(0, 0, 1.65f) from m_position (feet)
- Player inherits Entity::Update(), adds UpdateFromInput() before physics

---

### Requirement 3: Newtonian Physics System

**User Story:** As a player, I want physics-based movement with gravity and momentum, so that jumping and falling feel natural.

#### Acceptance Criteria

1. WHEN Entity is not grounded THEN gravity SHALL apply -20.0 m/s² acceleration to Z-axis
2. WHEN Entity has velocity THEN horizontal friction SHALL decelerate movement
3. IF Entity is on ground THEN friction SHALL be 10.0 (major damping)
4. IF Entity is airborne THEN friction SHALL be 1.0 (minor damping, allows air control)
5. WHEN Entity accelerates THEN it SHALL reach full speed quickly (responsive controls)
6. WHEN Entity stops moving THEN it SHALL decelerate smoothly to zero velocity
7. IF Entity is in WALKING mode THEN physics SHALL include gravity and ground friction
8. IF Entity is in FLYING mode THEN physics SHALL exclude gravity but keep collision
9. WHEN Entity velocity changes THEN it SHALL integrate using Euler method (v += a * dt)

**Technical Details:**
- Gravity applied only if: (m_physicsMode == WALKING) && (!m_isOnGround)
- Horizontal friction: dragForce = -horizontalVelocity * frictionCoefficient
- Reset acceleration to zero at end of UpdatePhysics() to avoid runaway accumulation
- Maximum horizontal speed: 20.0 m/s (enforced after velocity integration)

---

### Requirement 4: Entity-World Collision Detection

**User Story:** As a player, I want my character to collide with blocks realistically, so that I cannot walk through walls or get stuck in terrain.

#### Acceptance Criteria

1. WHEN Entity moves THEN it SHALL NOT enter solid blocks
2. WHEN Entity contacts a solid block THEN velocity SHALL be zeroed along collision normal
3. IF Entity is moving THEN it SHALL NOT jitter, snap, or teleport when colliding
4. WHEN Entity walks on ground THEN it SHALL NOT snag or slow erratically
5. IF Entity falls from height THEN it SHALL NOT tunnel through blocks (up to 50m fall)
6. WHEN Entity is in NOCLIP mode THEN collision SHALL be completely disabled
7. IF Entity AABB overlaps blocks THEN it SHALL be pushed out along shortest axis

**Technical Details:**
- Use corner raycast method (12 corners: 4 feet, 4 mid-body, 4 head)
- For each corner: raycast in deltaPosition direction with length |deltaPosition| + epsilon
- Collect all impacts, find closest per axis (X, Y, Z)
- Zero velocity and deltaPosition on blocked axes
- Apply remaining deltaPosition to position
- Prevent tunneling using fixed physics timestep (120 Hz minimum)

**Algorithm (World::PushEntityOutOfBlocks):**
```
1. Get entity AABB3 from m_physicsAABB + m_position
2. Query all block coords overlapping AABB (IntVec3 grid)
3. For each solid block:
   a. Calculate penetration depth on all 6 faces
   b. Find axis with minimum penetration (X, Y, or Z)
   c. Push entity out along that axis
4. Repeat until no overlaps (max 3 iterations)
5. Update m_position with corrected position
```

---

### Requirement 5: Ground Detection System

**User Story:** As a player, I want the game to accurately detect when I'm on the ground, so that jumping and gravity work correctly.

#### Acceptance Criteria

1. WHEN Entity has solid blocks directly below any bottom corner THEN m_isOnGround SHALL be true
2. IF no solid blocks are below Entity THEN m_isOnGround SHALL be false
3. WHEN Entity is in FLYING or NOCLIP modes THEN m_isOnGround SHALL always be false
4. IF Entity steps off ledge THEN m_isOnGround SHALL immediately become false
5. WHEN Entity lands on ground THEN m_isOnGround SHALL immediately become true

**Technical Details:**
- Cast 4 downward rays from AABB bottom corners
- Ray length: 2 * raycastOffset (e.g., 0.02m)
- Ray starts: corner + Vec3(0, 0, raycastOffset) to avoid floor-plane collision
- Any ray hit → m_isOnGround = true
- Multiple corners ensure grounded on edges and slopes

---

### Requirement 6: Player Controls and Movement

**User Story:** As a player, I want responsive WASD controls with jumping and sprinting, so that movement feels smooth and intuitive.

#### Acceptance Criteria

1. WHEN W/A/S/D keys are held THEN Player SHALL accelerate in corresponding direction
2. IF Shift is held THEN movement speed SHALL increase by 20x factor
3. WHEN Space is pressed AND Player is grounded AND in WALKING mode THEN Player SHALL jump
4. IF Player jumps THEN instant velocity boost of +8.5 m/s on Z-axis SHALL be applied
5. WHEN Q/E are pressed AND in FLYING/NOCLIP modes THEN Player SHALL move up/down
6. IF Player is in WALKING mode THEN Q/E SHALL NOT provide vertical movement
7. WHEN Player stops input THEN deceleration SHALL bring them to smooth stop

**Technical Details:**
- Build localMovement from input: Vec3(forward, strafe, vertical)
- Normalize before transforming to avoid faster diagonal movement
- Transform to world space using camera orientation matrix
- Walking mode: force worldMovement.z = 0 (no vertical from WASD)
- Jump impulse: m_velocity.z += PLAYER_JUMP_VELOCITY (8.5 m/s)
- Acceleration: m_acceleration += worldMovement * accelerationRate * sprintModifier

---

### Requirement 7: GameCamera with Multiple Modes

**User Story:** As a player and developer, I want multiple camera modes for gameplay and debugging, so that I can switch between first-person, spectator, and free-camera views.

#### Acceptance Criteria

1. WHEN C key is pressed THEN camera mode SHALL cycle through all 5 modes
2. IF camera is in FIRST_PERSON mode THEN it SHALL be at Player eye position with Player orientation
3. IF camera is in OVER_SHOULDER mode THEN it SHALL be 4m behind Player eye position
4. WHEN camera is in SPECTATOR mode THEN WASD SHALL move camera directly (not Player)
5. IF camera is in SPECTATOR_XY mode THEN WASD movement SHALL be restricted to XY-plane
6. WHEN camera is in INDEPENDENT mode THEN Player moves but camera stays detached
7. IF camera is attached (FIRST_PERSON or OVER_SHOULDER) THEN raycasts SHALL prevent wall clipping

**Camera Modes Defined:**
- **FIRST_PERSON**: Camera at eye position, rotates with Player, Player entity invisible
- **OVER_SHOULDER**: Camera 4m back from eye position along -forward, Player visible
- **SPECTATOR**: Player frozen, camera moves freely in 3D space (full WASD control)
- **SPECTATOR_XY**: Player frozen, camera moves only in XY-plane (Z locked)
- **INDEPENDENT**: Player moves normally, camera stays at last spectator position

**Technical Details:**
- Camera orientation uses Player m_aim (EulerAngles: yaw, pitch, roll)
- Mouse delta updates m_aim: yaw += mouseDelta.x * 0.075, pitch += mouseDelta.y * 0.075
- Pitch clamped to [-85°, +85°] to prevent gimbal lock
- Camera starts at (-50, -50, 150) looking (45°, 45°, 0°)
- FOV: 60°, near clip: 0.01, far clip: 10000

---

### Requirement 8: Physics Modes (Walking, Flying, NoClip)

**User Story:** As a developer, I want multiple physics modes for testing, so that I can debug issues in creative mode or observe physics in action.

#### Acceptance Criteria

1. WHEN V key is pressed THEN physics mode SHALL cycle (WALKING → FLYING → NOCLIP → WALKING)
2. IF physics mode is WALKING THEN full physics SHALL apply (gravity, collision, grounded checks)
3. IF physics mode is FLYING THEN collision SHALL apply but gravity SHALL NOT
4. WHEN physics mode is NOCLIP THEN both collision and gravity SHALL be disabled
5. IF Player is in WALKING mode THEN jumping SHALL be enabled when grounded
6. WHEN Player is in FLYING/NOCLIP modes THEN Q/E keys SHALL provide vertical movement

**Technical Details:**
- Enum: `enum class PhysicsMode { WALKING, FLYING, NOCLIP }`
- Physics logic checks mode before applying gravity/collision
- All modes still use velocity-based movement (smooth transitions when switching modes)
- Current mode displayed on-screen with "V" key hint

---

### Requirement 9: Entity Rendering and Debug Visualization

**User Story:** As a developer, I want visual debugging for entity bounds and physics state, so that I can diagnose collision and movement issues.

#### Acceptance Criteria

1. WHEN Entity is rendered THEN physics AABB SHALL be drawn as cyan wireframe
2. IF camera is in first-person attached to Entity THEN that Entity SHALL NOT be rendered
3. WHEN Entity is rendered THEN local origin SHALL be shown as cyan X-ray sphere
4. IF block selection raycast hits THEN ray path and impact SHALL be visualized
5. WHEN debug draw is enabled THEN velocity vector SHALL be shown as arrow
6. IF Entity is on ground THEN ground detection rays SHALL be visible

**Technical Details:**
- Use DebugAddWorldWireBox() for AABB3 bounds
- Use DebugAddWorldSphere() for origin (radius 0.1m)
- Raycast visualization: DebugAddWorldLine() with impact sphere at hit point
- X-ray mode (depth test disabled) ensures visibility through walls
- Color coding: Cyan = entity bounds, Green = grounded, Red = airborne

---

### Requirement 10: Interface and On-Screen Display

**User Story:** As a player, I want on-screen controls and status display, so that I always know my current mode and available actions.

#### Acceptance Criteria

1. WHEN game starts THEN all controls SHALL be printed to dev console
2. WHEN game runs THEN it SHALL be fullscreen borderless by default
3. IF screen updates THEN top of screen SHALL show: LMB/RMB controls, current block type, camera mode, physics mode
4. WHEN display updates THEN FPS SHALL be shown in top-right corner
5. IF F2 is pressed THEN chunk debug draw SHALL toggle (current chunk coords, active chunks, bounding boxes)
6. IF F3 is pressed THEN job system debug SHALL toggle (chunk states, job counts)
7. WHEN camera is in spectator modes THEN world-aligned basis SHALL be shown in front of Player

**On-Screen Elements:**
```
LMB: Dig  RMB: Place  [1] Glowstone [2] Cobblestone
Camera: FIRST_PERSON (C)  Physics: WALKING (V)
FPS: 60
```

---

### Requirement 11: Build Configurations

**User Story:** As a developer, I want specialized build configurations for performance tuning, so that I can profile and optimize the physics system.

#### Acceptance Criteria

1. WHEN build configurations are created THEN DebugInline SHALL be a clone of Debug
2. IF DebugInline is selected THEN inline function expansion SHALL be "Any Suitable"
3. IF DebugInline is selected THEN intrinsic functions SHALL be enabled
4. WHEN FastBreak configuration exists THEN it SHALL be a clone of Release
5. IF FastBreak is selected THEN optimization SHALL be disabled
6. WHEN any configuration builds THEN it SHALL produce no warnings

**Technical Details:**
- Configuration Manager → New from Debug/Release
- DebugInline: C/C++ → Optimization → Inline Function Expansion = Any Suitable
- DebugInline: C/C++ → Optimization → Enable Intrinsic Functions = Yes
- FastBreak: C/C++ → Optimization → Optimization = Disabled
- All configurations must build and run correctly

---

### Requirement 12: Performance Target

**User Story:** As a player, I want smooth 60 FPS gameplay, so that physics and camera movement feel responsive even during chunk loading.

#### Acceptance Criteria

1. WHEN game runs in Release mode THEN framerate SHALL maintain at least 60 FPS
2. IF chunks are activating/deactivating THEN framerate SHALL NOT drop below 60 FPS
3. WHEN physics calculations occur THEN they SHALL complete within 1ms per frame
4. IF collision detection runs THEN it SHALL use efficient raycasting (not brute-force AABB checks)
5. WHEN ground detection occurs THEN it SHALL use short raycasts (not full-height checks)

**Technical Details:**
- Use Clock system with delta time clamping (max 1/15s, min 1/120s)
- Physics substeps if needed (fixed 120 Hz timestep for tunneling prevention)
- Limit ground detection rays to 2 * raycastOffset length (~0.02m)
- Use 12-corner raycast instead of full AABB sweep for efficiency

---

## Non-Functional Requirements

### Code Architecture and Modularity

#### Single Responsibility Principle
- **Entity.cpp**: Physics integration and collision only
- **Player.cpp**: Input handling and camera control only
- **World.cpp**: Block queries and collision queries only
- **GameCommon.hpp**: Physics constants centralized

#### Modular Design
- Physics logic in Entity base class works for any derived type (Player, NPC, Projectile)
- Camera system decoupled from Player (GameCamera can attach to any Entity)
- Collision detection uses World interface (no direct Chunk access from Entity)
- Input system converts to acceleration (not direct position changes)

#### Dependency Management
- Entity depends on: World (for collision queries), AABB3 (for bounds)
- Player depends on: Entity (base class), Camera (engine class), InputSystem
- World provides: IsBlockSolid(), RaycastWorld(), GetBlocksInAABB()
- No circular dependencies (Entity doesn't know about Player)

#### Clear Interfaces
```cpp
// World provides collision interface for Entity
bool World::IsBlockSolid(IntVec3 const& coords) const;
void World::PushEntityOutOfBlocks(Entity* entity);
bool World::IsEntityOnGround(Entity const* entity) const;

// Entity provides physics interface for derived classes
virtual void Entity::Update(float deltaSeconds);
virtual void Entity::UpdatePhysics(float deltaSeconds);  // final
virtual void Entity::UpdateIsGrounded();  // final

// Player implements input interface
virtual void Player::UpdateFromInput(float deltaSeconds);
```

---

### Performance

1. **60 FPS Minimum**: Release mode maintains 60 FPS during gameplay and chunk activation
2. **Physics Efficiency**:
   - Collision detection uses raycasts (O(n) in ray length), not AABB overlap tests (O(n³) in block count)
   - Ground detection limited to 0.02m ray length (not full player height)
   - Maximum 3 push-out iterations per frame
3. **Memory Footprint**: Entity physics adds ~100 bytes per instance (acceptable for 100s of entities)
4. **Timestep Stability**: Fixed 120 Hz physics substep prevents tunneling with <1ms overhead

---

### Reliability

1. **No Tunneling**: Falling from any height (tested up to 100m) does not clip through blocks
2. **No Jittering**: Standing still or walking produces stable, smooth motion
3. **No Getting Stuck**: Entity never becomes permanently stuck in blocks (push-out always succeeds)
4. **Consistent Physics**: Switching physics modes doesn't cause teleporting or velocity spikes
5. **Graceful Chunk Loading**: Physics gracefully handles missing chunks (no gravity when chunk below doesn't exist)

---

### Usability

1. **Responsive Controls**: Input latency <16ms (1 frame), acceleration reaches max speed in <0.2s
2. **Intuitive Keys**:
   - WASD movement (industry standard)
   - Space for jump (universal game convention)
   - C for camera cycle (mnemonic: "C" = Camera)
   - V for physics cycle (mnemonic: "V" = Variant)
3. **Visual Feedback**:
   - On-screen display always shows current mode
   - Debug visualization clearly shows collision bounds
   - FPS counter confirms performance
4. **Smooth Transitions**: Mode switching has no jarring snaps or velocity resets

---

### Security

Not applicable for this single-player game engine project.

---

## Edge Cases and Special Considerations

### Chunk Loading Transitions
- **Issue**: Entity falling while chunks below are loading
- **Solution**: Disable gravity when chunk below doesn't exist, freeze vertical velocity
- **Detection**: `World::GetChunkAtCoords()` returns nullptr check

### Camera Wall Clipping (OVER_SHOULDER mode)
- **Issue**: Camera clips into walls when backing up
- **Solution**: Raycast from Player eye to desired camera position, place camera at impact point
- **Implementation**: Already shown in assignment guide (commented-out code section)

### High-Speed Collisions
- **Issue**: Moving >20 m/s might tunnel through 1-block thick walls
- **Solution**: Use fixed 120 Hz physics substep (dt = 1/120 = 0.0083s, max distance per step = 0.166m)
- **Alternative**: Swept AABB collision (not required for A6)

### Diagonal Movement Speed
- **Issue**: WASD diagonal input gives √2 faster speed
- **Solution**: Normalize localMovement vector before applying acceleration
- **Code**: `localMovement = localMovement.GetNormalized()`

### Vertical Camera Flip
- **Issue**: Looking straight up/down causes camera forward to align with world Z
- **Solution**: Clamp pitch to [-85°, +85°] (prevents 90° singularity)
- **Implementation**: `m_aim.m_pitchDegrees = Clamp(pitch, -85.f, 85.f)`

### Swimming in Water Blocks
- **Assignment Note**: Water handling not required (instructor quote: "I honestly have no plan for how to handle water")
- **Options**:
  1. Make water solid (easiest, avoids complexity)
  2. Add underwater physics (friction, no jump, screen overlay) - optional extra credit

---

## Success Metrics

### Functional Completeness
- ✅ All 5 camera modes implemented and cycle with C key
- ✅ All 3 physics modes implemented and cycle with V key
- ✅ Jump works only when grounded in WALKING mode
- ✅ Gravity applies correctly based on mode and grounded state
- ✅ Collision prevents entering blocks in WALKING and FLYING modes
- ✅ NoClip mode allows passing through blocks
- ✅ FPS maintains 60+ in Release mode

### Code Quality
- ✅ Zero compiler warnings
- ✅ Clean Entity→Player inheritance
- ✅ Physics logic in Entity base class (works for NPCs)
- ✅ No code duplication between camera modes
- ✅ Constants in GameCommon.hpp (not magic numbers)

### User Experience
- ✅ Movement feels responsive and smooth
- ✅ Jumping height is 1.1-1.6 meters (achievable)
- ✅ No visible jitter when standing or walking
- ✅ Camera doesn't clip through walls in attached modes
- ✅ On-screen display clearly shows current mode
- ✅ Debug visualization helps diagnose issues

### Submission Requirements
- ✅ Depot paths submitted correctly to Canvas
- ✅ Both Engine and SimpleMiner folders submitted
- ✅ All 4 build configurations (Debug, Release, DebugInline, FastBreak) build without errors
- ✅ Buddy build performed and confirmed working
