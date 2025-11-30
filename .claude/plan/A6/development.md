# SimpleMiner - Assignment 06: Player, Camera, and Physics Development Plan

## Overview

Assignment 6 implements character physics, multiple camera modes, and proper entity-world collision detection. This transforms SimpleMiner from a spectator experience into a playable game with walking, jumping, flying, and no-clip movement modes.

**Official Assignment Specification:** See `.claude/docs/A6/Assignment 06 - Player, Camera, and Physics.txt`

**Key Features**:
- **Newtonian Physics**: Velocity-based movement with acceleration/deceleration
- **3 Physics Modes**: Walking (gravity), Flying (no gravity), NoClip (no collision)
- **6 Camera Modes**: First-person, over-shoulder, fixed tracking, spectator, independent
- **World Collision**: AABB vs voxel grid with no tunneling, jitter, or snagging

---

## Architecture Changes

### Entity-Player Hierarchy
```
Entity (base class)
├── m_position: Vec3
├── m_velocity: Vec3
├── m_orientation: EulerAngles
├── m_bounds: AABB3 (physics body)
├── m_physicsMode: PhysicsMode enum
├── UpdatePhysics(float deltaSeconds)
└── RenderDebug()

Player (derived from Entity)
├── m_eyeHeightOffset: float (1.65m)
├── ProcessInput()
└── GetEyePosition()
```

### GameCamera Architecture
```
GameCamera (game-side, NOT engine Camera)
├── m_cameraMode: CameraMode enum
├── m_attachedEntity: Entity* (nullable)
├── m_position: Vec3
├── m_orientation: EulerAngles
├── UpdateFromEntity() // attached modes
├── UpdateFromInput() // detached modes
└── KeepOutOfWalls() // raycast collision
```

---

## Pipeline Stages

### Stage 1: Foundation - Entity & Player Classes (**HIGH PRIORITY**)
**Goal**: Set up entity hierarchy and physics data structures

**Tasks**:
- [ ] 1.1: Create Entity base class
  - Position, velocity, orientation members
  - AABB3 m_bounds for physics collision
  - PhysicsMode enum: WALKING, FLYING, NOCLIP
  - Virtual UpdatePhysics(float deltaSeconds)
  - Virtual RenderDebug() for cyan wireframe
  - Helper: IsOnGround() checks for solid blocks below
- [ ] 1.2: Implement Player derived class
  - Constructor: call Entity constructor
  - Physical dimensions: 1.80m tall, 0.60m wide
  - Eye position: 1.65m above bottom center
  - m_eyeHeightOffset constant (1.65f)
  - GetEyePosition() returns position + Vec3(0, 0, m_eyeHeightOffset)
- [ ] 1.3: Add player to World/Game
  - World owns Player instance
  - Initialize at spawn position (-50, -50, 150)
  - Player updates every frame
- [ ] 1.4: Testing checkpoint
  - Player exists with correct dimensions
  - Debug wireframe renders (cyan AABB)
  - Eye position calculated correctly

**Technical Notes**:
- AABB3 centered on entity position: `AABB3(pos - halfDims, pos + halfDims)`
- Entity bounds should NOT include vertical eye offset
- Physics logic belongs in Entity, not Player

---

### Stage 2: GameCamera & Camera Modes (**HIGH PRIORITY**)
**Goal**: Implement 6 distinct camera modes with attachment system

**Camera Modes**:
1. **FIRST_PERSON** (attached): Camera at entity eye position, entity orientation
2. **OVER_SHOULDER** (attached): Camera 4m back from eye position, entity orientation
3. **FIXED_ANGLE_TRACKING** (attached): Camera faces entity from 10m away, yaw=40°, pitch=30°
4. **INDEPENDENT** (detached): Controls move player, camera free-view (existing behavior)
5. **SPECTATOR** (detached): Controls move camera, WASD relative to camera orientation
6. **SPECTATOR_XY** (detached): Controls move camera, WASD limited to XY-plane

**Tasks**:
- [ ] 2.1: Create GameCamera class
  - CameraMode enum with 6 values
  - Entity* m_attachedEntity (nullable)
  - Position, orientation members
  - C key cycles through modes
  - Mode displayed on-screen
- [ ] 2.2: Implement attached modes (FirstPerson, OverShoulder, FixedAngle)
  - FirstPerson: camera position = entity.GetEyePosition(), orientation = entity.orientation
  - OverShoulder: offset 4m backward along entity forward vector
  - FixedAngle: position 10m from entity at fixed angles, always face entity
  - Entity NOT rendered in FirstPerson mode
- [ ] 2.3: Implement detached modes (Spectator, SpectatorXY, Independent)
  - Spectator: WASD moves camera, player still falls/collides (frozen)
  - SpectatorXY: Like spectator, but movement restricted to XY-plane
  - Independent: WASD moves player directly, camera spectates
- [ ] 2.4: Camera wall collision (attached non-NoClip modes)
  - Raycast from entity eye position to intended camera position
  - If hit solid block, move camera to just before impact
  - Prevents camera inside walls in tight spaces
- [ ] 2.5: Testing checkpoint
  - All 6 modes cycle correctly with C key
  - Camera positions match specifications
  - FirstPerson hides player entity
  - Wall collision prevents clipping

**Technical Notes**:
- GameCamera is game-side, separate from engine Camera class
- Engine Camera updated from GameCamera position/orientation
- Attached modes update AFTER entity physics runs
- Detached modes still run entity physics (player falls in Spectator mode)

---

### Stage 3: Physics Modes (**HIGH PRIORITY**)
**Goal**: Implement 3 distinct physics behaviors

**Physics Modes**:
1. **WALKING**: Full physics, gravity, collision, jumping
2. **FLYING**: Full physics, collision, no gravity, fly up/down
3. **NOCLIP**: No collision, no gravity, pass through blocks

**Tasks**:
- [ ] 3.1: Implement physics mode enum and switching
  - PhysicsMode enum: WALKING, FLYING, NOCLIP
  - V key cycles through modes
  - Current mode displayed on-screen
- [ ] 3.2: Walking physics mode
  - Apply gravity acceleration: velocity.z -= 9.8f × deltaSeconds
  - Ground detection: check for solid blocks below all 4 bottom corners
  - Horizontal friction: major on ground (0.9), minor in air (0.1)
  - Jump: Space key gives instant +Z velocity when on ground
  - Collision: full AABB vs world
- [ ] 3.3: Flying physics mode
  - No gravity applied
  - Q/E keys: move up/down in world Z
  - Horizontal friction: same as Walking
  - Collision: full AABB vs world
- [ ] 3.4: NoClip physics mode
  - No gravity, no collision
  - Q/E keys: move up/down in world Z
  - Entity passes through solid blocks
  - Useful for debugging and level editing
- [ ] 3.5: Testing checkpoint
  - V key cycles modes correctly
  - Walking: player falls, jumps, collides
  - Flying: player moves up/down, collides
  - NoClip: player phases through walls

**Technical Notes**:
- Gravity constant: ~9.8 m/s² (can tune for feel)
- Friction applied as: velocity *= (1.0f - frictionFactor)
- Jump impulse: typically 5-6 m/s upward velocity

---

### Stage 4: Newtonian Physics Implementation (**CRITICAL**)
**Goal**: Velocity-based movement with proper acceleration/deceleration

**Physics Concepts**:
- Velocity: 3D vector (m/s), modified by forces/inputs
- Acceleration: Player input applies forces to velocity
- Friction: Decelerates entity when no input (stronger on ground)
- Speed Limit: Horizontal XY speed cap from voluntary input
- Gravity: Constant downward acceleration (Walking mode only)

**Tasks**:
- [ ] 4.1: Implement velocity-based movement
  - Entity has Vec3 m_velocity
  - Position updated: position += velocity × deltaSeconds
  - Velocity modified by inputs, gravity, friction
- [ ] 4.2: Player control acceleration
  - WASD applies acceleration in desired direction
  - Walking: relative to camera forward (project to XY-plane)
  - Acceleration rate: reach max speed in ~0.2 seconds
  - Max XY speed: 4 m/s default, 80 m/s with Shift
- [ ] 4.3: Gravity implementation (Walking mode only)
  - Constant: gravity = Vec3(0, 0, -9.8) m/s²
  - Apply: velocity += gravity × deltaSeconds
  - Terminal velocity: naturally achieved by air friction
- [ ] 4.4: Friction system
  - On ground: strong friction (drag coefficient ~0.9)
  - In air: minimal friction (drag coefficient ~0.1)
  - Apply: velocity *= (1.0f - friction) each frame
  - Deceleration: player stops ~0.5 seconds after releasing keys
- [ ] 4.5: IsOnGround() implementation
  - Check 4 bottom corners of AABB
  - Extend slightly below (0.01m) to detect support
  - Return true if ANY corner has solid block below
  - Used for: jump permission, friction selection
- [ ] 4.6: Jump mechanics
  - Space pressed + IsOnGround() + Walking mode
  - Instant velocity boost: velocity.z += jumpSpeed (5-6 m/s)
  - Jump height: ~1.3-1.5 meters achievable
  - No double-jump (must be on ground)
- [ ] 4.7: Testing checkpoint
  - Player accelerates smoothly to max speed
  - Deceleration feels responsive
  - Jump height within spec (1.1-1.6 meters)
  - Shift speed boost works (20× multiplier)

**Technical Notes**:
- Separate horizontal (XY) and vertical (Z) movement logic
- Speed limit only applies to voluntary movement (not falling)
- Friction formula: `velocity *= pow(1.0f - friction, deltaSeconds)`
- Jump height calculation: h = v²/(2g), so v = sqrt(2gh)

---

### Stage 5: Entity-World Collision (**CRITICAL**)
**Goal**: Robust AABB vs voxel collision with no artifacts

**Requirements**:
- No jitter when standing still
- No snagging on edges/corners
- No visible teleporting/snapping
- No tunneling through blocks
- No getting stuck in geometry

**Tasks**:
- [ ] 5.1: AABB vs block grid collision detection
  - Entity AABB vs world voxel grid
  - Check all blocks entity overlaps (use AABB extents)
  - Filter: only solid blocks cause collision
  - Return: penetration depth and normal for each contact
- [ ] 5.2: Collision response & depenetration
  - Move entity out of solid blocks along shortest path
  - Zero velocity component along collision normal
  - Order: resolve Z (vertical) first, then XY (horizontal)
  - Iterative resolution if multiple simultaneous contacts
- [ ] 5.3: Prevent tunneling
  - Option A: Fixed timestep physics (e.g., 120 Hz)
  - Option B: Swept AABB (continuous collision detection)
  - Option C: Multi-step integration (subdivide large dt)
  - Requirement: survive 50m fall without tunneling
- [ ] 5.4: Edge case handling
  - Walking up 1-block steps: automatic (collision pushes up)
  - Sliding along walls: friction only in movement direction
  - Corners: resolve both axes independently
  - Ceiling collision: zero upward velocity, fall back down
- [ ] 5.5: Optimize collision performance
  - Only check blocks in entity's expanded AABB
  - Early-out if no solid blocks nearby
  - Cache chunk references to avoid lookups
- [ ] 5.6: Testing checkpoint
  - Walk into wall: no jitter, smooth stop
  - Run along wall: smooth sliding, no snagging
  - Jump into ceiling: clean stop, fall back
  - Fall 50+ meters: no tunneling
  - Walk on uneven terrain: no bouncing

**Technical Notes**:
- Collision resolution order matters: Z-axis first prevents "stairs are walls"
- Small epsilon values (0.001m) prevent floating-point edge cases
- Test with single-block-wide gaps and 1-block-high steps
- Use BlockIterator for efficient voxel access

---

### Stage 6: Entity Rendering & Debug Visualization (**MEDIUM PRIORITY**)
**Goal**: Clear visual feedback for entity state

**Tasks**:
- [ ] 6.1: Entity debug rendering
  - Draw AABB in cyan wireframe (x-ray, always visible)
  - Draw local origin (small axis gizmo)
  - Draw velocity vector (line from center, scaled)
  - Draw ground detection rays (4 corners, downward)
- [ ] 6.2: FirstPerson mode entity hiding
  - Check: if (camera.mode == FIRST_PERSON && camera.attachedEntity == this)
  - Skip rendering this entity
  - Still render other entities normally
- [ ] 6.3: Raycast visualization
  - Draw selection raycast: green if hit, red if miss
  - Draw hit point as small sphere
  - Draw hit normal as short line from impact point
  - R key locks/unlocks raycast for debugging
- [ ] 6.4: Testing checkpoint
  - Entity debug draw visible in all modes except FirstPerson
  - Velocity vector scales correctly with speed
  - Ground rays turn green when on ground
  - Raycast visualization accurate

**Technical Notes**:
- Use `Renderer::SetDepthTest(false)` for x-ray wireframes
- Cyan color: RGB(0, 255, 255)
- Velocity vector scale: length = velocity.magnitude × 0.1 meters

---

### Stage 7: Build Configurations (**HIGH PRIORITY**)
**Goal**: Create specialized build targets for debugging and testing

**New Configurations**:
1. **DebugInline**: Debug with optimization hints
   - Clone of Debug configuration
   - C/C++ → Optimization → Inline Function Expansion = Any Suitable
   - C/C++ → Optimization → Enable Intrinsic Functions = Yes
   - Purpose: Debug symbols + some performance boost
2. **FastBreak**: Release with no optimization
   - Clone of Release configuration
   - C/C++ → Optimization → Optimization = Disabled
   - Purpose: Fast iteration with release features

**Tasks**:
- [ ] 7.1: Create DebugInline configuration
  - Configuration Manager → New → Copy Debug
  - Set inline/intrinsic settings for Engine + Game projects
  - Test: builds successfully, breakpoints work
- [ ] 7.2: Create FastBreak configuration
  - Configuration Manager → New → Copy Release
  - Disable optimization for Engine + Game projects
  - Test: builds quickly, runs release features
- [ ] 7.3: Update project settings
  - All 4 configs: Debug, Release, DebugInline, FastBreak
  - Multi-processor Compilation enabled for all
  - Output directories correct for all configs
- [ ] 7.4: Buddy build validation
  - Test all 4 configurations build cleanly
  - Test all 4 configurations run without crashing
  - Confirm no warnings in any config

**Technical Notes**:
- Configuration Manager: Build → Configuration Manager → New
- Apply settings to BOTH Engine and SimpleMiner projects
- Buddy build BEFORE submission to catch linker issues

---

## Implementation Timeline

| Phase | Duration | Priority | Notes |
|-------|----------|----------|-------|
| **Stage 1: Entity/Player** | 3-4 hours | HIGH | Foundation |
| **Stage 2: Camera Modes** | 4-6 hours | HIGH | 6 modes + transitions |
| **Stage 3: Physics Modes** | 3-4 hours | HIGH | 3 modes + switching |
| **Stage 4: Newtonian Physics** | 6-8 hours | **CRITICAL** | Core feel |
| **Stage 5: Collision** | 8-12 hours | **CRITICAL** | Most complex |
| **Stage 6: Rendering** | 2-3 hours | MEDIUM | Polish |
| **Stage 7: Build Configs** | 1-2 hours | HIGH | Submission requirement |
| **Total (All)** | 27-39 hours | - | - |

---

## Testing Strategy

### Per-Stage Testing
- [ ] Stage 1: Entity exists, debug draws correctly
- [ ] Stage 2: All camera modes functional, smooth transitions
- [ ] Stage 3: All physics modes behave distinctly
- [ ] Stage 4: Movement feels smooth and responsive
- [ ] Stage 5: Collision is bug-free (no jitter/snag/tunnel)
- [ ] Stage 6: Debug visualizations helpful and clear
- [ ] Stage 7: All build configs work

### Integration Testing
- [ ] Walk around, jump, switch modes seamlessly
- [ ] Camera transitions don't cause jarring jumps
- [ ] Collision works in all physics modes (except NoClip)
- [ ] Performance: maintain 60 FPS with entity physics

### Feel Testing (Critical for Physics)
- [ ] Movement acceleration feels responsive (not sluggish)
- [ ] Jump height feels appropriate (not floaty or stubby)
- [ ] Friction stops player quickly enough (not ice skating)
- [ ] Collision feels solid (not bouncy or mushy)

---

## Common Pitfalls

### Collision Jitter
- **Problem**: Player vibrates when standing on ground
- **Solution**: Don't apply gravity if on ground and velocity.z < 0

### Stair Climbing Issues
- **Problem**: Player can't walk up 1-block steps
- **Solution**: Resolve vertical (Z) collisions before horizontal (XY)

### Camera Clipping Through Walls
- **Problem**: In OverShoulder mode, camera in wall
- **Solution**: Implement wall raycast pull-in (Stage 2.4)

### Tunneling Through Floors
- **Problem**: Fall through floor when moving fast
- **Solution**: Implement fixed timestep or swept collision (Stage 5.3)

### Poor Movement Feel
- **Problem**: Player feels unresponsive or slippery
- **Solution**: Tune acceleration/friction values, get playtester feedback

---

## Rubric Alignment (100 points)

| Category | Points | Key Requirements |
|----------|--------|------------------|
| **Player & Camera** | 10 | Entity hierarchy, GameCamera architecture |
| **Physics Modes** | 15 | 3 modes distinct, V key cycling |
| **Camera Modes** | 15 | 6 modes functional, C key cycling |
| **Newtonian Physics** | 20 | Velocity, acceleration, friction, gravity |
| **Player Controls** | 10 | WASD, Space jump, responsive |
| **Entity-World Collision** | 15 | No jitter/snag/tunnel, smooth |
| **Entity Rendering** | 5 | Debug wireframes, FirstPerson hiding |
| **Build Configurations** | 10 | DebugInline & FastBreak functional |

**Deductions**:
- Unable to build: -100 points
- Compile warnings: -5 points
- Unnecessary files: -5 points

---

## File Locations

### New Files
- `Code/Game/Gameplay/Entity.hpp/cpp` - Base entity class
- `Code/Game/Gameplay/Player.hpp/cpp` - Player derived class
- `Code/Game/Gameplay/GameCamera.hpp/cpp` - Camera controller

### Modified Files
- `Code/Game/Gameplay/Game.hpp/cpp` - Entity and camera management
- `Code/Game/Gameplay/World.hpp/cpp` - Entity-world collision

---

## Final Notes

**Focus on Feel First**:
Physics-based games live or die by how the movement feels. Once collision works, spend time tuning acceleration, friction, jump height, and speed until it feels good.

**Test Collision Extensively**:
The collision system (Stage 5) is the most complex part. Test every edge case:
- Corners, edges, ceilings, floors
- Fast movement, slow movement
- Jumping into walls, falling from height
- Walking up stairs, sliding down slopes

**Buddy Build All 4 Configs**:
Don't discover on submission day that FastBreak doesn't link. Test all configurations early and often.

**Match Reference Build**:
Compare movement feel, camera behavior, and collision response to the reference executable. If something feels off, it probably is.

**Camera Modes Are Gameplay**:
Different camera modes serve different purposes:
- FirstPerson: Immersive gameplay
- OverShoulder: Third-person action
- FixedAngle: Cinematic view
- Spectator: Level exploration
- Independent: Debugging tool
Make sure each mode is useful and polished.
