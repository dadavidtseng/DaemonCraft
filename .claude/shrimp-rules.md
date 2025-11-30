# SimpleMiner Development Standards (AI Agent Guide)

> **Purpose:** This document provides SimpleMiner-specific rules for AI Agents. It contains project-specific constraints, file coordination requirements, and architectural boundaries that are NOT general programming knowledge.

---

## 1. Project Architecture

### Technology Stack
- **Language:** C++20 (`/std:c++20`)
- **Rendering:** DirectX 11 with HLSL shaders
- **Scripting:** V8 JavaScript engine
- **Audio:** FMOD
- **Build:** Visual Studio 2022, x64 only

### Repository Structure
```
C:\p4\Personal\SD\
├── SimpleMiner\          (Game project - THIS repository)
│   ├── Code\Game\
│   ├── Run\             (Executables, data, configs)
│   └── .claude\         (Planning docs, shrimp data)
└── Engine\              (Engine project - SEPARATE repository)
    └── Code\Engine\
```

**CRITICAL:** SimpleMiner and Engine are **SEPARATE Git repositories**. Changes affecting both require **TWO separate commits**.

---

## 2. Architectural Boundaries

### Engine vs Game Separation

**MUST ENFORCE:**
- **Engine MUST be game-agnostic** (NO SimpleMiner-specific logic in Engine code)
- **Game CAN include Engine headers** (SimpleMiner includes Engine)
- **Engine CANNOT include Game headers** (Engine knows nothing about SimpleMiner)

**Examples:**

✅ **CORRECT:**
```cpp
// SimpleMiner/Code/Game/Framework/WorldGenConfig.cpp
static PiecewiseCurve1D CreateDefaultContinentalnessCurve() {
    // SimpleMiner-specific terrain curve presets belong in SimpleMiner
}
```

❌ **WRONG:**
```cpp
// Engine/Code/Engine/Math/Curve1D.cpp
static PiecewiseCurve1D CreateDefaultContinentalnessCurve() {
    // Game-specific terrain logic does NOT belong in Engine
}
```

**Decision Rule:**
- Is this feature specific to SimpleMiner gameplay? → SimpleMiner code
- Is this a generic utility usable by any game? → Engine code
- **When in doubt:** Put in SimpleMiner, NOT Engine

---

## 3. File Coordination Rules

### When Adding New Block Types

**MUST update ALL three files simultaneously:**

1. **GameCommon.hpp** (lines ~30-70) - Add enum value:
```cpp
enum BlockType : uint8_t {
    BLOCK_AIR = 0,
    BLOCK_GRASS,
    // ... existing blocks
    BLOCK_YOUR_NEW_BLOCK,  // ← Add here
};
```

2. **Run/Data/Definitions/BlockDefinitions.xml** - Add block definition:
```xml
<BlockDefinition name="YourNewBlock" isSolid="true" isOpaque="true"
                 topTexCoords="7,0" sideTexCoords="7,1" bottomTexCoords="7,2"/>
```

3. **Sprite Sheet Reference** - Ensure texture coordinates point to `Run/Data/Images/terrain_8x8.png`

❌ **WRONG:** Updating only GameCommon.hpp without XML definition

---

### When Modifying Terrain Generation

**MUST coordinate these files:**

| File | Purpose | Lines |
|------|---------|-------|
| `Chunk.cpp` | `GenerateTerrain()` implementation | 800-2500 |
| `GameCommon.hpp` | Terrain constants and biome parameters | 150-250 |
| `WorldGenConfig.hpp/cpp` | Runtime parameter tuning | All |
| `.claude/plan/A4/development.md` | Check assignment plan before changes | All |

**Example Workflow:**
1. Read `.claude/plan/A4/development.md` to understand phase context
2. Modify `Chunk.cpp::GenerateTerrain()` terrain logic
3. Add/update constants in `GameCommon.hpp`
4. Update `WorldGenConfig` if adding tunable parameters
5. Test in-game before considering complete

---

### When Modifying Chunk Lifecycle

**MUST coordinate App, World, and Chunk:**

| File | Responsibility | Key Functions |
|------|---------------|---------------|
| `App.cpp` | Shutdown sequence | `Shutdown()` - Three-stage: workers→chunks→renderer |
| `World.cpp` | Chunk activation/deactivation | `ActivateNearestChunks()`, `DeactivateChunk()` |
| `Chunk.cpp` | State transitions | Use `std::atomic<ChunkState>` for thread safety |

**CRITICAL Shutdown Pattern (App.cpp):**
```cpp
// Stage 1: Stop worker threads
m_theJobSystem->StopWorkers();

// Stage 2: Delete all chunks (before destroying renderer!)
delete m_theWorld;
m_theWorld = nullptr;

// Stage 3: Destroy DirectX resources
Renderer::DestroyInstance();
```

❌ **WRONG:** Deleting chunks after destroying renderer (causes crashes)

---

### When Updating Documentation

**MUST update CLAUDE.md files in this order:**

1. **If change affects SimpleMiner only:**
   - Update: `SimpleMiner/CLAUDE.md` (root)
   - Update: Relevant module CLAUDE.md (e.g., `Code/Game/Framework/CLAUDE.md`)

2. **If change affects Engine only:**
   - Update: `Engine/CLAUDE.md` (root)
   - Update: Relevant module CLAUDE.md (e.g., `Code/Engine/Core/CLAUDE.md`)

3. **If change affects BOTH repositories:**
   - Update ALL 4 files above
   - Create separate commits for SimpleMiner and Engine

**Example:** Moving terrain curves from Engine to SimpleMiner
- Updated 2 SimpleMiner CLAUDE.md files (root + Framework)
- Updated 2 Engine CLAUDE.md files (root + Core)
- Created 2 separate commits (one per repo)

---

## 4. Assignment-Based Development

### Current Status
- **A4 (World Generation):** 98% complete, Phase 5B.4 remaining
- **A5 (Lighting System):** NOT started
- **A6 (Physics & Camera):** NOT started

### Development Plan Locations
```
.claude/plan/
├── A4/development.md        (World generation phases)
├── A5/development.md        (Lighting stages)
├── A6/development.md        (Physics/camera stages)
└── task-pointer.md          (Quick task index)
```

### Assignment-Specific Rules

#### A4 (World Generation) - MODIFY ONLY:
- `Chunk.cpp::GenerateTerrain()` (lines 800-2500)
- `GameCommon.hpp` (biome constants, terrain parameters)
- `WorldGenConfig.hpp/cpp` (tunable parameters)
- `GameConfig.xml` (persistent parameter storage)

**DO NOT modify:**
- Block class structure (wait for A5)
- Entity/Player classes (wait for A6)
- Rendering pipeline (wait for A5)

#### A5 (Lighting System) - WILL MODIFY:
- `Block.hpp/cpp` - Expand from 1 byte to 3 bytes (add light data)
- `Chunk.cpp` - Add light propagation algorithm
- `World.cpp` - Add dirty lighting queue
- `Shaders/World.hlsl` - Add vertex coloring

**WARNING:** A5 changes Block class size, will break A4 chunk saves

#### A6 (Physics & Camera) - WILL ADD:
- NEW: `Entity.hpp/cpp`, `Player.hpp/cpp`, `GameCamera.hpp/cpp`
- MODIFY: `Game.cpp` (camera modes, physics integration)
- MODIFY: `World.cpp` (entity-voxel collision)

---

## 5. Thread Safety Patterns

### Multi-threaded Architecture

**Thread Types:**
- **Main Thread:** Rendering, ImGui, mesh building
- **I/O Worker (1 thread):** Disk loading/saving (`LoadChunkJob`, `SaveChunkJob`)
- **Generic Workers (N-2 threads):** Chunk generation (`ChunkGenerateJob`)

### Chunk State Machine

**MUST use atomic transitions:**
```cpp
// Chunk.hpp
std::atomic<ChunkState> m_state;

// State transitions (atomic)
CONSTRUCTING → ACTIVATING → GENERATING → ACTIVE
           ↓               ↓
     DEACTIVATING ← SAVING
```

**DO NOT:**
- Directly assign to `m_state` without using `.store()` or `.compare_exchange_strong()`
- Access chunk block data without checking state is ACTIVE
- Modify chunk from worker thread after state is DEACTIVATING

### Mutex Protection

**MUST lock mutex before:**
- Accessing `World::m_activeChunksByCoords` map
- Modifying chunk activation/deactivation lists
- Adding jobs to JobSystem queues

**Example (World.cpp):**
```cpp
std::lock_guard<std::mutex> lock(m_chunkMapMutex);
auto iter = m_activeChunksByCoords.find(chunkCoords);
```

---

## 6. Code Modification Locations

### Adding Terrain Features
- **Location:** `Chunk.cpp::GenerateTerrain()` lines 800-2500
- **Pattern:** Follow phases: Biome Noise → Density → Surface → Features → Caves → Carvers
- **Reference:** `.claude/plan/A4/development.md` for phase structure

### Adding World Parameters
- **Location:** `GameCommon.hpp` lines 150-250
- **Naming:** Use `g_worldGenConfig->category.parameterName` pattern
- **Persistence:** Add to `WorldGenConfig::SaveToXML()` and `LoadFromXML()`

### Adding ImGui Controls
- **Location:** `Game.cpp` lines 531-1305
- **Pattern:** Use tabs for categories, sliders for parameters
- **Visual Feedback:** Add hover/active state color changes (lines 848-864)

### Modifying Chunk Rendering
- **Location:** `Chunk.cpp::RebuildMesh()` lines 400-700
- **Vertex Type:** MUST use `VertexBuffer_PCT` (Position, Color, TexCoords)
- **Hidden Surface Removal:** Check all 6 neighbors before adding face

---

## 7. AI Decision-Making Guidelines

### When to Ask User vs. Proceed

**PROCEED without asking if:**
- Following established patterns in existing code
- Implementing tasks from `.claude/plan/A4|A5|A6/development.md`
- Fixing compiler errors or warnings
- Updating documentation to match code changes

**ASK USER before:**
- Changing Block class structure (affects save files)
- Modifying chunk state machine transitions
- Adding new dependencies or libraries
- Making architectural decisions not covered in development.md

### Decision Trees

#### New Terrain Feature?
1. Check `.claude/plan/A4/development.md` for phase context
2. Identify which phase (0, 1, 2, 3, 4, 5A, 5B)
3. Modify `Chunk.cpp::GenerateTerrain()` in appropriate phase section
4. Add constants to `GameCommon.hpp` if needed
5. Add tunable parameters to `WorldGenConfig` if desired
6. Test in-game

#### New Block Type?
1. Add enum value to `GameCommon.hpp::BlockType`
2. Add definition to `Run/Data/Definitions/BlockDefinitions.xml`
3. Verify sprite sheet texture coordinates in `terrain_8x8.png`
4. Rebuild and test

#### Modifying Engine Code?
1. Verify change is game-agnostic (NOT SimpleMiner-specific)
2. Test that SimpleMiner still compiles after change
3. Create SEPARATE commit for Engine repository
4. Update both SimpleMiner and Engine CLAUDE.md files

---

## 8. Prohibited Actions

### ❌ NEVER DO THESE:

1. **Auto-build without permission**
   - User builds manually via Visual Studio
   - ONLY build if user explicitly requests: "build", "compile", "run build"

2. **Auto-commit without permission**
   - User commits manually or explicitly requests split commits
   - EXCEPTION: User says "commit" or "split commit for me"

3. **Add SimpleMiner logic to Engine**
   - Engine MUST remain game-agnostic
   - Move game-specific code from Engine to SimpleMiner if found

4. **Modify chunk state without atomic operations**
   - MUST use `std::atomic` for state transitions
   - MUST lock mutex before accessing shared data structures

5. **Delete chunks before stopping workers**
   - MUST follow three-stage shutdown: workers→chunks→renderer
   - See `App.cpp::Shutdown()` for correct pattern

6. **Update only one CLAUDE.md when changes affect multiple modules**
   - MUST update root + module-specific CLAUDE.md files
   - MUST update both SimpleMiner and Engine CLAUDE.md if affecting both

7. **Modify Block class size without user confirmation**
   - A5 will change Block from 1 byte to 3 bytes
   - This BREAKS existing chunk save files
   - MUST get explicit confirmation before proceeding

8. **Skip updating BlockDefinitions.xml when adding block types**
   - MUST update all three: GameCommon.hpp enum + XML definition + sprite sheet

9. **Use vague terminology mixing**
   - A4 uses **"Phase"** (professor's terminology)
   - A5/A6 use **"Stage"** (professor's terminology)
   - MUST maintain consistency within each assignment

10. **Implement features not in development.md without approval**
    - Follow assignment plans in `.claude/plan/A4|A5|A6/development.md`
    - ASK before adding features outside planned scope

---

## 9. Common Scenarios Reference

### Scenario: User reports compilation error

**Action:**
1. Read build log output
2. Identify error type (syntax, linker, missing include)
3. Fix error in relevant file
4. **DO NOT build** unless user requests it
5. Report what was fixed

### Scenario: User asks to update documentation

**Action:**
1. Identify which repositories are affected (SimpleMiner only, Engine only, or both)
2. Update root CLAUDE.md files
3. Update module-specific CLAUDE.md files
4. If both repos: Create summary of changes for each

### Scenario: User requests new terrain feature

**Action:**
1. Read `.claude/plan/A4/development.md` to find correct phase
2. Locate implementation area in `Chunk.cpp::GenerateTerrain()`
3. Add constants to `GameCommon.hpp` if needed
4. Implement feature following existing phase patterns
5. Ask about tunable parameters for `WorldGenConfig`

### Scenario: User reports DirectX 11 memory leak

**Action:**
1. Check `App.cpp::Shutdown()` for three-stage pattern
2. Verify chunks deleted before `Renderer::DestroyInstance()`
3. Check `Chunk.cpp::RebuildMesh()` for buffer cleanup
4. Add proper cleanup in destructors if missing

---

## 10. File Path Quick Reference

### Core Gameplay Files
```
Code/Game/
├── Framework/
│   ├── App.hpp/cpp              (Application lifecycle, shutdown)
│   ├── Block.hpp/cpp            (1-byte block type, will expand in A5)
│   ├── Chunk.hpp/cpp            (16x16x128 voxel storage, generation)
│   ├── GameCommon.hpp/cpp       (Constants, enums, global config)
│   └── WorldGenConfig.hpp/cpp   (Runtime parameter tuning, XML)
├── Gameplay/
│   ├── Game.hpp/cpp             (Game logic, ImGui interface)
│   ├── World.hpp/cpp            (Chunk management, job queuing)
│   └── Player.hpp/cpp           (Will be created in A6)
└── Definition/
    └── BlockDefinition.hpp/cpp  (XML-driven block properties)
```

### Runtime Assets
```
Run/
├── Data/
│   ├── Definitions/
│   │   └── BlockDefinitions.xml      (Block type definitions)
│   ├── Images/
│   │   └── terrain_8x8.png           (64x64 sprite sheet)
│   └── Shaders/
│       └── World.hlsl                (Chunk rendering shader)
└── GameConfig.xml                    (WorldGenConfig persistence)
```

### Planning Documents
```
.claude/
├── plan/
│   ├── A4/development.md             (World generation phases)
│   ├── A5/development.md             (Lighting stages)
│   ├── A6/development.md             (Physics/camera stages)
│   └── task-pointer.md               (Quick task index)
└── shrimp_data/                      (Task manager data)
```

---

## End of Document

**Remember:** This document is for AI Agents. Focus on SimpleMiner-specific rules, not general C++ knowledge. When in doubt, check existing code patterns or ask the user.
