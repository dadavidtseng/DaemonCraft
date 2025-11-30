# Assignment 7-Core: Foundation Systems (Registry, Inventory, Mining, Placement)

**Due Date:** December 12, 2025 (Week 1-2)
**Estimated Duration:** 1-2 weeks
**Complexity:** High (Multi-system foundation)
**Implementation Standard:** ⭐ **Follow Minecraft Implementation Patterns** ⭐

---

## Executive Summary

Assignment 7-Core implements the foundational gameplay systems for SimpleMiner, establishing the data-driven architecture and core mechanics that will support all future features:

1. **Rendering Bug Fix** - Fix incorrect hidden surface removal at chunk boundaries
2. **Minecraft-style Registry System** - Data-driven architecture for blocks, items, and recipes (JSON-based)
3. **Inventory System** - Player item storage with hotbar and full inventory management (Minecraft-style)
4. **Mining Mechanics** - Progressive block breaking and item drops (Minecraft mechanics)
5. **Block Placement Mechanics** - Item-based block placement (Minecraft mechanics)
6. **Basic HUD** - Hotbar widget only (bottom-center display, Minecraft-style)

This assignment establishes the foundation for SimpleMiner's gameplay loop (mine → collect → craft → build) and prepares the codebase for UI and AI integration in subsequent specs.

---

## Project Context

### Current State (Post-A6)

**Existing Systems:**
- ✅ Newtonian physics with gravity, friction, collision detection (Entity.cpp)
- ✅ 5 camera modes (FIRST_PERSON, OVER_SHOULDER, SPECTATOR, etc.)
- ✅ 3 physics modes (WALKING, FLYING, NOCLIP)
- ✅ BlockDefinition system with XML loading (will migrate to JSON)
- ✅ Chunk-based world with async generation/loading
- ✅ JSON library (`C:\p4\Personal\SD\Engine\Code\ThirdParty\json\json.hpp`)

**Known Issues:**
- ❌ **Rendering Bug**: Some block faces incorrectly hidden at chunk boundaries (MUST FIX)

### Target State (Post-A7-Core)

**New Systems:**
- ✅ Rendering bug fixed (chunk boundary face visibility)
- ✅ Registry<T> template for type-safe ID management
- ✅ ItemRegistry, enhanced BlockRegistry, RecipeRegistry (JSON-based)
- ✅ ItemStack data structure with quantity and type
- ✅ Player inventory (36 slots: 27 main + 9 hotbar)
- ✅ ItemEntity for dropped items (magnetic pickup, world physics)
- ✅ Mining mechanics (progressive break, block → item drop, Minecraft-style)
- ✅ Placement mechanics (consume inventory, place block, Minecraft-style)
- ✅ Basic HUD (hotbar widget only, Minecraft-style)

---

## Goals and Objectives

### Primary Goals

1. **Fix Rendering Bug** - Eliminate hidden surface removal artifacts at chunk boundaries
2. **Registry Architecture** - Establish Minecraft-inspired JSON-driven foundation
3. **Core Gameplay Loop** - Enable mine → collect → place cycle (Minecraft mechanics)
4. **Basic UI** - Display hotbar with item counts (Minecraft-style)

### Secondary Goals

1. **Performance** - Maintain 60 FPS with hotbar rendering and item entities
2. **Extensibility** - Design for future expansion (more items, recipes, full UI)
3. **Code Quality** - Clean separation between data (Registry) and logic (Game systems)

### Non-Goals (Explicitly Out of Scope for A7-Core)

- ❌ Full inventory screen (deferred to A7-UI)
- ❌ Crafting interface (deferred to A7-UI)
- ❌ AI agents (deferred to A7-AI)
- ❌ Menu system (deferred to A7-Polish)
- ❌ Sound effects (deferred to A7-Polish)
- ❌ Combat/damage system
- ❌ Health/hunger bars
- ❌ Armor system
- ❌ Particle effects

---

## Functional Requirements

### FR-0: Rendering Bug Fix (BLOCKING)

#### FR-0.1: Fix Hidden Surface Removal at Chunk Boundaries
**Description**: Fix incorrect face culling when neighboring chunks are not loaded.

**Problem**:
- File: `Chunk.cpp:3632` in `IsFaceVisible()` function
- Bug: Returns `false` when `neighborIter.IsValid()` is false
- Effect: Assumes unloaded chunks are opaque, hides visible surface faces

**Solution**:
```cpp
// File: Chunk.cpp, line ~3632
bool Chunk::IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const
{
    BlockIterator neighborIter = blockIter.GetNeighbor(faceDirection);

    if (!neighborIter.IsValid())
    {
        IntVec3 currentCoords = blockIter.GetLocalCoords();
        IntVec3 neighborCoords = currentCoords + faceDirection;
        int worldZ = neighborCoords.z;

        if (worldZ < 0 || worldZ >= CHUNK_SIZE_Z)
        {
            return true;  // At vertical boundaries, render face
        }

        // ✅ FIX: Horizontal chunk boundary - assume neighbor is AIR (transparent)
        return true;  // Changed from false to true
    }

    // Rest unchanged...
}
```

**Rationale**:
- Better to render extra faces temporarily than hide visible ones
- Minecraft renders chunk boundaries optimistically
- Face will be hidden when neighbor chunk loads (mesh rebuild)
- Small performance impact (~16-32 extra quads per chunk edge)

**Acceptance Criteria**:
- ✅ Surface chunk boundaries show all visible faces
- ✅ Underground cave boundaries are continuous
- ✅ Building across chunks has no missing faces
- ✅ Chunk load/unload cycle correctly rebuilds meshes

---

### FR-1: Registry System (JSON-Based)

#### FR-1.1: Generic Registry Template
**Description**: Implement type-safe registry pattern for managing game objects with persistent IDs.

**Requirements**:
- Template class `Registry<T>` supporting any type
- Numeric ID assignment (uint16_t for 65,535 entries)
- String name → ID lookup (case-insensitive)
- ID → Object pointer lookup
- Registration order preservation
- Thread-safe read access (write happens during startup only)
- **JSON loading** (not XML) using Engine's json.hpp

**Acceptance Criteria**:
```cpp
Registry<BlockDefinition> blockRegistry;
blockRegistry.Register("stone", stoneDefPtr);
uint16_t stoneID = blockRegistry.GetID("stone");  // Returns stable ID
BlockDefinition* def = blockRegistry.Get(stoneID); // O(1) lookup
```

#### FR-1.2: Block Registry (JSON Migration)
**Description**: Migrate existing BlockDefinition system to Registry pattern with JSON.

**Requirements**:
- Replace static `s_definitions` vector with `BlockRegistry` singleton
- **Migrate from XML to JSON** format (BlockDefinitions.json)
- Use `C:\p4\Personal\SD\Engine\Code\ThirdParty\json\json.hpp` for parsing
- Preserve AIR as ID 0
- Maintain loading order for compatibility with existing chunks
- Support by-name and by-ID lookup
- Follow Minecraft's block definition structure

**JSON Format Example**:
```json
{
  "blocks": [
    {
      "name": "stone",
      "visible": true,
      "solid": true,
      "opaque": true,
      "topSprite": [1, 0],
      "bottomSprite": [1, 0],
      "sideSprite": [1, 0],
      "indoorLighting": 0.0,
      "hardness": 1.5
    }
  ]
}
```

**Acceptance Criteria**:
- All existing block types load from BlockDefinitions.json
- `Block::GetDefinition()` still works via registry lookup
- JSON definitions load in same order as before
- No breaking changes to existing Chunk code

#### FR-1.3: Item Registry (JSON-Based, Minecraft-Style)
**Description**: Create item system with JSON registry following Minecraft patterns.

**Requirements**:
- `ItemDefinition` class with Minecraft-style properties:
  - String name, description
  - Texture/sprite coordinates
  - ItemType enum (RESOURCE, TOOL, BLOCK, CONSUMABLE)
  - BlockType reference (for placeable items)
  - Tool properties (mining speed, durability)
  - Max stack size (Minecraft standard: 64 for blocks, 1 for tools, 16 for some items)
- **JSON loading** from `ItemDefinitions.json`
- Item ID range separate from block IDs
- Support for block items (e.g., "Stone Block" item places STONE block)
- Follow Minecraft's item definition structure

**JSON Format Example**:
```json
{
  "items": [
    {
      "name": "stone_block",
      "displayName": "Stone",
      "type": "block",
      "blockType": "stone",
      "maxStackSize": 64,
      "spriteCoords": [1, 0]
    },
    {
      "name": "wooden_pickaxe",
      "displayName": "Wooden Pickaxe",
      "type": "tool",
      "miningSpeed": 2.0,
      "durability": 60,
      "maxStackSize": 1,
      "spriteCoords": [0, 3]
    }
  ]
}
```

**Acceptance Criteria**:
- Items load from JSON
- Block items correctly reference block types
- Tool items have durability and mining speed
- Stack sizes follow Minecraft conventions

#### FR-1.4: Recipe Registry (JSON-Based, Minecraft Recipes)
**Description**: Define 10 Minecraft-style crafting recipes for 2×2 crafting grid.

**Requirements**:
- `Recipe` class with input/output patterns
- 2×2 grid support (4 input slots)
- Shaped recipes (pattern matters)
- Shapeless recipes (pattern doesn't matter)
- Recipe validation (check if inventory matches)
- **JSON-based recipe definitions** following Minecraft format
- **10 total recipes**

**JSON Format Example**:
```json
{
  "recipes": [
    {
      "type": "shaped",
      "pattern": [
        ["log", "log"],
        ["log", "log"]
      ],
      "result": {
        "item": "crafting_table",
        "count": 1
      }
    },
    {
      "type": "shapeless",
      "ingredients": ["log"],
      "result": {
        "item": "planks",
        "count": 4
      }
    }
  ]
}
```

**10 Required Recipes** (Minecraft-based):
1. LOG → 4 PLANKS (shapeless)
2. 4 PLANKS (2×2) → CRAFTING_TABLE (shaped)
3. 2 PLANKS (vertical) → 4 STICKS (shaped)
4. 3 PLANKS + 2 STICKS → WOODEN_PICKAXE (shaped)
5. 3 PLANKS + 2 STICKS → WOODEN_AXE (shaped)
6. 1 PLANK + 2 STICKS → WOODEN_SHOVEL (shaped)
7. 4 COBBLESTONE (2×2) → FURNACE (shaped)
8. 2 COBBLESTONE (vertical) → STONE_STAIRS (shaped)
9. 3 COBBLESTONE (horizontal) → STONE_SLAB (shaped)
10. 1 COAL + 1 STICK → TORCH (shapeless)

**Note**: Recipes will be used in A7-UI crafting interface, but registry is defined in A7-Core for data completeness.

---

### FR-2: Inventory System (Minecraft-Style)

#### FR-2.1: ItemStack Data Structure
**Description**: Represent item quantity and type in a single slot (Minecraft pattern).

**Requirements**:
- `ItemStack` struct:
  - `uint16_t itemID` (ItemRegistry ID)
  - `uint8_t quantity` (1-255, Minecraft uses 1-64 typically)
  - `uint16_t durability` (for tools, optional)
- Empty stack representation (itemID = 0)
- Stack merging logic (same item, below max stack size)
- Stack splitting (take N from stack)
- Follow Minecraft stacking rules

**Acceptance Criteria**:
```cpp
ItemStack stack(STONE_BLOCK_ITEM, 32);
bool canMerge = stack.CanMergeWith(otherStack);
ItemStack split = stack.TakeQuantity(16); // Now has 16, split has 16
```

#### FR-2.2: Player Inventory (Minecraft Layout)
**Description**: Player storage for collected items following Minecraft structure.

**Requirements**:
- 36 total slots (Minecraft standard):
  - Slots 0-26: Main inventory (27 slots, 3 rows × 9 columns)
  - Slots 27-35: Hotbar (9 slots, bottom row)
- Current selected hotbar slot (0-8)
- Add item (find empty/matching slot, Minecraft behavior)
- Remove item by slot
- Transfer items between slots
- **Serialize/deserialize for save system** (JSON format)

**Acceptance Criteria**:
- Picking up items fills inventory automatically (Minecraft behavior)
- Hotbar slots 1-9 map to keyboard keys 1-9
- Full inventory prevents further pickups
- Inventory persists across save/load

#### FR-2.3: Item Pickup System (Minecraft Mechanics)
**Description**: World items can be collected into inventory (Minecraft behavior).

**Requirements**:
- `ItemEntity` class (extends Entity):
  - Position, velocity (affected by gravity, Minecraft physics)
  - ItemStack contents
  - Pickup cooldown (0.5 seconds after drop, Minecraft standard)
  - Magnetic attraction to nearby player (3 block radius, Minecraft mechanic)
  - **Item stacking in world** (multiple same items merge into one entity with higher quantity)
- Collision detection with player
- Inventory space check before pickup
- Visual feedback (item entity disappears)
- Item entity despawn timer (5 minutes, Minecraft standard)

**Acceptance Criteria**:
- Breaking a block spawns ItemEntity at block center
- Player walks near ItemEntity, it flies toward player (Minecraft magnetic pull)
- On collision, item added to inventory, entity destroyed
- If inventory full, item remains in world
- Multiple dropped items of same type stack together (Minecraft behavior)

---

### FR-3: Mining & Placement Mechanics (Minecraft-Style)

#### FR-3.1: Progressive Block Breaking (Minecraft Mechanics)
**Description**: Blocks take time to break with visual progress (Minecraft system).

**Requirements**:
- Raycast from player camera to detect targeted block (6 block range, Minecraft standard)
- Hold left mouse button to mine
- Mining progress (0.0 to 1.0) based on Minecraft formula:
  - Block hardness (BlockDefinition property)
  - Tool effectiveness (ToolDefinition property)
  - Time held
- Visual crack overlay (10 stages, 0-9, Minecraft style)
- Cancel mining if:
  - Player releases mouse
  - Player looks away from block
  - Player moves too far (7 block range)
- On completion:
  - Block removed from world (set to AIR)
  - ItemEntity spawned with block drop
  - Tool durability decreases (if tool used)

**Break Time Formula** (Minecraft-based):
```cpp
float breakTime = blockHardness / (toolEffectiveness * globalMultiplier);
// Example: Stone (hardness 1.5) with wooden pickaxe (effectiveness 2.0)
// breakTime = 1.5 / 2.0 = 0.75 seconds
```

**Acceptance Criteria**:
- Holding left mouse on stone block shows progressive cracks (Minecraft style)
- After ~0.75s (with pickaxe), block breaks and drops item
- Without tool, takes 3x longer (Minecraft penalty)
- Breaking GRASS drops DIRT_BLOCK item (Minecraft behavior)
- Tool durability decreases on each block broken

#### FR-3.2: Block Placement (Minecraft Mechanics)
**Description**: Place blocks from hotbar into world (Minecraft placement rules).

**Requirements**:
- Raycast to find placement position (adjacent to targeted block face, Minecraft rule)
- Right mouse button to place
- Consume 1 item from selected hotbar slot
- Check placement validity (Minecraft rules):
  - Target position not occupied by solid block
  - Player not intersecting placed block position
  - Within reach (6 block range, Minecraft standard)
- Update chunk mesh after placement
- Visual preview (ghost block) before placement (optional, nice-to-have)

**Acceptance Criteria**:
- With STONE_BLOCK in hotbar, right-click places stone block (Minecraft behavior)
- Item quantity decreases by 1
- Cannot place block inside player (Minecraft rule)
- Can place blocks on ground, walls, ceilings (Minecraft placement)

#### FR-3.3: Tool Effectiveness System (Minecraft-Based)
**Description**: Different tools break blocks at different speeds (Minecraft system).

**Requirements**:
- `ToolDefinition` properties (Minecraft-style):
  - Tool tier (WOOD, STONE, IRON, DIAMOND)
  - Effectiveness multiplier per block type
  - **Durability** (uses before breaking, Minecraft standard: Wood=60, Stone=132, Iron=251, Diamond=1562)
- Preferred tool for each block (Minecraft rules):
  - STONE → Pickaxe (faster)
  - DIRT/GRASS → Shovel (faster)
  - WOOD → Axe (faster)
- Wrong tool or no tool: 0.5x speed (Minecraft penalty)
- **Tool durability decreases on block break**
- Tool breaks when durability reaches 0 (disappears from inventory, Minecraft behavior)

**Example** (Minecraft-based):
```cpp
// Breaking stone with wooden pickaxe: 0.75s
// Breaking stone with stone pickaxe: 0.5s
// Breaking stone with hand (no tool): 2.25s
// Wooden pickaxe breaks after 60 uses
```

---

### FR-4: Basic HUD (Minecraft-Style Hotbar Only)

#### FR-4.1: Hotbar Widget (Minecraft-Style)
**Description**: Always-visible hotbar UI element during gameplay (pixel-perfect Minecraft style).

**Requirements**:
- **Hotbar Widget** (bottom-center, Minecraft style):
  - 9 slots displayed horizontally
  - Selected slot highlighted (white border, Minecraft style)
  - Item icons rendered in each slot (16×16 pixel items, Minecraft size)
  - Item quantity displayed (bottom-right corner, if > 1, Minecraft position)
  - Semi-transparent gray background (Minecraft GUI texture)
- **Crosshair Widget** (center, Minecraft style):
  - Simple white cross (Minecraft crosshair design)
  - Always visible, no alpha fade

**UI Assets**:
- **Find online** or **draw programmatically** using Engine renderer
- Must match Minecraft visual style (pixel-perfect if possible)
- Reference Minecraft UI screenshots for accuracy

**WidgetSubsystem Integration**:
- May need to **refine/redesign WidgetSubsystem** for production quality
- Create custom `HotbarWidget` class (extends `IWidget`)
- Use `WidgetSubsystem::AddWidget()` with zOrder=100 (above world)
- Render with orthographic projection (screen-space coordinates)

**Acceptance Criteria**:
- Hotbar looks like Minecraft (visual fidelity)
- Selecting different slots (1-9 keys) updates highlight (Minecraft behavior)
- Item icons and quantities render correctly (Minecraft style)
- Crosshair centered on screen

**Note**: Full inventory screen and crafting interface deferred to A7-UI.

---

## Non-Functional Requirements

### NFR-1: Performance
- **Target FPS**: 60 FPS sustained with hotbar rendering
- **UI Rendering**: < 1ms per frame for hotbar widget
- **JSON Loading**: < 100ms for all registries at startup

### NFR-2: Memory
- **Registry Overhead**: < 1MB for all registries combined
- **Inventory Storage**: 36 slots × 6 bytes = 216 bytes per player
- **ItemEntity**: < 100 bytes per entity

### NFR-3: Usability (Minecraft Standard)
- **Input Responsiveness**: < 16ms (one frame) from input to action
- **Visual Feedback**: All interactions show immediate feedback (Minecraft polish)
- **Error Messages**: Clear error messages for invalid placements/mining

### NFR-4: Extensibility
- **Registry Design**: Easy to add new blocks, items, recipes via JSON
- **Inventory Logic**: Reusable for AI agents (A7-AI) and other entities

### NFR-5: Code Quality
- **SOLID Principles**: Registry pattern demonstrates Single Responsibility
- **DRY**: No duplicate inventory logic across classes
- **YAGNI**: Only implement 2×2 crafting registry, not UI yet
- **KISS**: Simple ItemStack struct, straightforward mining mechanics
- **Minecraft Fidelity**: All systems follow Minecraft implementation patterns

---

## Constraints and Assumptions

### Technical Constraints
1. **DirectX 11**: Hotbar UI must use existing WidgetSubsystem (may need refinement)
2. **Windows Only**: No cross-platform support required
3. **Single-threaded Gameplay**: Game logic runs on main thread
4. **JSON Library**: Must use `C:\p4\Personal\SD\Engine\Code\ThirdParty\json\json.hpp`

### Assumptions
1. **Rendering Bug Fix**: Can be completed quickly (1-2 hours)
2. **WidgetSubsystem**: Functional enough for basic hotbar rendering (refinement can happen later)
3. **Minecraft Assets**: Hotbar sprite sheet can be found online or recreated programmatically
4. **Save System**: Inventory serialization piggybacks on existing chunk save system

---

## Success Criteria

### Minimum Viable Product (MVP)
**These features MUST work for A7-Core completion**:

1. ✅ Rendering bug fixed (no missing faces at chunk boundaries)
2. ✅ Registry system (Block, Item, Recipe) functional with JSON loading
3. ✅ Player inventory with 36 slots
4. ✅ Mining mechanics (progressive break, item drop, pickup) - Minecraft-style
5. ✅ Placement mechanics (consume inventory, place block) - Minecraft-style
6. ✅ Hotbar HUD with item display and selection (Minecraft-style)
7. ✅ Tool durability system (if feasible)

**Demo Scenario**:
```
1. Start game in existing world
2. Player mines 4 logs (break blocks, collect items)
3. Hotbar shows log items (bottom-center, Minecraft style)
4. Player uses wooden pickaxe to mine stone (tool durability decreases)
5. Pickaxe breaks after 60 uses (disappears from hotbar)
6. Player places logs to build structure (right-click)
7. Item quantity in hotbar decreases with each placement
```

### Stretch Goals (Nice-to-Have)
**These features are bonus, not required**:

- ⭐ Tool durability fully implemented (if not too hard)
- ⭐ Ghost block preview before placement
- ⭐ Pixel-perfect Minecraft hotbar recreation
- ⭐ Item entity stacking animation (Minecraft item pickup animation)
- ⭐ F3 debug overlay enhancement (Minecraft F3 style) - currently basic

---

## Dependencies

**Engine Systems**:
1. **WidgetSubsystem**: Must refine for hotbar rendering (basic functionality sufficient)
2. **JSON Library**: Must use for all JSON parsing (`ThirdParty\json\json.hpp`)
3. **Entity System**: Extend for ItemEntity physics
4. **Chunk System**: Integrate with block placement/removal mesh rebuilding

**A7 Spec Sequence**:
- **A7-Core** (this spec) → **A7-UI** (inventory screen, crafting) → **A7-AI** (agents, KADI)
- A7-UI depends on A7-Core registries and inventory system
- A7-AI depends on A7-Core inventory system (agents have inventory)

---

## References

### Minecraft Systems (Implementation Reference)
- [Minecraft Wiki: Inventory](https://minecraft.wiki/w/Inventory)
- [Minecraft Wiki: Mining](https://minecraft.wiki/w/Mining)
- [Minecraft Wiki: Items](https://minecraft.wiki/w/Item)
- [Minecraft Wiki: Blocks](https://minecraft.wiki/w/Block)

### SimpleMiner Codebase
- **Engine**: `C:\p4\Personal\SD\Engine\Code\Engine\`
  - `Widget\` - WidgetSubsystem (needs refinement)
  - `ThirdParty\json\json.hpp` - JSON library
- **SimpleMiner**: `C:\p4\Personal\SD\SimpleMiner\Code\Game\`
  - `Framework\Chunk.cpp` (line 3632: rendering bug fix location)
  - `Gameplay\Player.cpp` (extend with inventory)

---

**Next Steps**:
1. Review and approve this requirements.md via spec-workflow
2. Create design.md for A7-Core with detailed architecture
3. After design approval, create tasks.md and begin implementation
4. After A7-Core complete, proceed to A7-UI spec
