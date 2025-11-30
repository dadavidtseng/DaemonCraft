# Assignment 7: Design Document
# Registry System, Inventory, UI, and AI Agent Integration

**Version:** 1.0
**Date:** 2025-11-27
**Status:** Design Phase
**Prerequisites:** ✅ P-2 Complete, ✅ P-3 Complete, 🔄 P-1 Included Below

---

## Executive Summary

This design document provides the complete technical architecture for Assignment 7's implementation of a Minecraft-authentic gameplay system with AI agent integration. The design covers:

1. **P-1: Rendering Bug Fix** - Fix hidden surface removal at chunk boundaries
2. **Registry System** - JSON-based data-driven architecture
3. **Inventory System** - 36-slot Minecraft-style item storage
4. **Mining & Placement** - Progressive block breaking with tool durability
5. **UI System** - HUD, inventory screen, 2×2 crafting interface
6. **AI Agent Framework** - KADI WebSocket-integrated autonomous agents
7. **Menu System** - Save/Load/Create game functionality
8. **Sound Effects** - Mining, placing, crafting audio

**Design Philosophy:**
- **Minecraft Implementation Fidelity** - Follow Minecraft patterns exactly
- **KISS Over YAGNI** - Simple, working implementations for tight deadline (12/12)
- **JSON-First** - All data uses nlohmann/json library
- **SOLID Principles** - Clean separation, dependency injection, const correctness

---

## Table of Contents

1. [P-1: Rendering Bug Fix](#p-1-rendering-bug-fix)
2. [System Architecture Overview](#system-architecture-overview)
3. [Registry System Design](#registry-system-design)
4. [Inventory System Design](#inventory-system-design)
5. [Mining & Placement Mechanics](#mining--placement-mechanics)
6. [UI System Design](#ui-system-design)
7. [AI Agent Framework Design](#ai-agent-framework-design)
8. [KADI WebSocket Integration](#kadi-websocket-integration)
9. [Menu System Design](#menu-system-design)
10. [Sound Effects Integration](#sound-effects-integration)
11. [Data Flow Diagrams](#data-flow-diagrams)
12. [Class Hierarchies](#class-hierarchies)
13. [JSON Schema Definitions](#json-schema-definitions)
14. [Implementation Strategy](#implementation-strategy)

---

## P-1: Rendering Bug Fix

### Problem Analysis

**Issue:** Block faces are incorrectly hidden at chunk boundaries when neighboring chunks are not loaded.

**Root Cause Location:** `Chunk.cpp:3616-3633` in `IsFaceVisible()` function

```cpp
// CURRENT (BUGGY) CODE:
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
            return true;  // At vertical world boundaries
        }

        return false;  // ❌ BUG: Assumes unloaded chunk = opaque
    }

    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock) return false;

    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef) return false;

    return !neighborDef->IsOpaque();
}
```

**Problem:** When `neighborIter.IsValid()` returns `false` (horizontal chunk boundary with unloaded neighbor), the function returns `false`, hiding the face. This is incorrect because:
1. Unloaded chunks at horizontal boundaries are usually **AIR** (especially near surface)
2. This causes visible block faces to disappear incorrectly
3. Players see "holes" in terrain at chunk edges

**Impact:**
- ❌ Surface blocks near chunk boundaries show missing faces
- ❌ Underground caves show missing walls when spanning chunk edges
- ❌ Building structures across chunks have invisible walls

### Solution Design

**Strategy:** Conservative rendering with intelligent defaults

**Key Insight:** When neighbor chunk is not loaded, we should assume it's **TRANSPARENT** (air) rather than opaque, then hide the face later when the chunk loads if it's actually solid.

**Rationale:**
- **Visual Quality**: Better to show extra faces temporarily than hide visible ones
- **Minecraft Behavior**: Minecraft renders chunk boundaries optimistically
- **Performance**: Extra faces only rendered at chunk edges (small overhead)
- **Correctness**: Face will be properly hidden once neighbor chunk activates

### Implementation

**File:** `SimpleMiner\Code\Game\Framework\Chunk.cpp`
**Function:** `Chunk::IsFaceVisible()` (lines 3607-3656)

```cpp
// FIXED CODE:
bool Chunk::IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const
{
    BlockIterator neighborIter = blockIter.GetNeighbor(faceDirection);

    // ✅ FIX: If neighbor is invalid (unloaded chunk), assume TRANSPARENT (air)
    // This prevents incorrectly hiding faces at chunk boundaries
    if (!neighborIter.IsValid())
    {
        IntVec3 currentCoords = blockIter.GetLocalCoords();
        IntVec3 neighborCoords = currentCoords + faceDirection;
        int worldZ = neighborCoords.z;

        // Vertical world boundaries (top/bottom of world)
        if (worldZ < 0 || worldZ >= CHUNK_SIZE_Z)
        {
            return true;  // At vertical boundaries, render face
        }

        // ✅ FIX: Horizontal chunk boundary - assume neighbor is AIR (transparent)
        // Rationale: Better to render extra faces temporarily than hide visible ones
        // When neighbor chunk loads, mesh will rebuild and hide face if needed
        return true;  // Changed from false to true
    }

    // Rest of function unchanged
    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock) return false;  // No block data = assume opaque (safety)

    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef) return false;  // Invalid definition = assume opaque

    // Face is VISIBLE if neighbor is NOT opaque
    return !neighborDef->IsOpaque();
}
```

**Change Summary:**
- **Line 3632**: Changed `return false;` to `return true;`
- **Added comments** explaining the fix rationale

### Verification Strategy

**Test Cases:**

1. **Surface Chunk Boundaries**
   - **Setup**: Stand at edge of loaded chunks (e.g., chunk boundary between (0,0) and (1,0))
   - **Expected**: Surface blocks show all visible faces (no missing top/side faces)
   - **Verification**: Walk around chunk edges, verify no "holes" in terrain

2. **Underground Cave Boundaries**
   - **Setup**: Dig cave spanning multiple chunks
   - **Expected**: Cave walls are continuous across chunk boundaries
   - **Verification**: Walk through caves, verify no missing wall faces

3. **Building Across Chunks**
   - **Setup**: Place blocks in structures spanning chunk boundaries
   - **Expected**: Walls are solid with no missing faces
   - **Verification**: Build multi-chunk structures, inspect for gaps

4. **Chunk Load/Unload Cycle**
   - **Setup**: Move far from chunk, return to reload it
   - **Expected**: After reload, faces are correctly hidden where appropriate
   - **Verification**: Move >16 chunks away, return, verify mesh rebuilt correctly

**Performance Impact:**
- **Extra Faces Rendered**: ~16-32 extra quads per chunk edge (4 edges × 4-8 exposed faces per edge)
- **Memory**: Negligible (extra vertices in VBO)
- **GPU**: <1% performance impact (modern GPUs handle thousands of extra quads easily)

**Edge Cases Handled:**
- ✅ Vertical world boundaries (z < 0, z >= 128) still render correctly
- ✅ Completely unloaded chunks (neighboring chunks never loaded) show faces
- ✅ Partially loaded chunks (some neighbors loaded) correctly hide solid neighbors
- ✅ Chunk activation/deactivation triggers mesh rebuild, hiding faces as needed

### Deployment Notes

**Before Fix:**
- Backup `Chunk.cpp` (for rollback if needed)
- Verify current behavior with test world save

**After Fix:**
- Rebuild SimpleMiner (Debug and Release)
- Test all verification scenarios
- Profile performance (should be identical to before)
- Verify no new visual artifacts

**Rollback Plan:**
- If fix causes new issues, revert `return true;` back to `return false;`
- Investigate alternative solutions (e.g., neighbor chunk preloading)

---

## System Architecture Overview

### Layer Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    KADI Integration Layer                    │
│  - KADIToolHandler: Maps JSON-RPC to game functions          │
│  - AgentCommandExecutor: Queues and executes agent commands  │
└───────────────────┬─────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────────────────────┐
│                      UI Layer (NEW)                          │
│  - HotbarWidget, InventoryWidget, CraftingWidget             │
│  - Uses Engine WidgetSubsystem for DirectX 11 rendering      │
└───────────────────┬─────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────────────────────┐
│                   Gameplay Layer (ENHANCED)                  │
│  - World: Manages chunks, entities, agents                   │
│  - Player: Entity with Inventory + input handling            │
│  - AIAgent: Entity with Inventory + command queue            │
│  - ItemEntity: Drops from mining, magnetic pickup            │
└───────────────────┬─────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────────────────────┐
│                  Core Systems (FOUNDATION)                   │
│  - Registry<T>: Generic type-safe ID management              │
│  - BlockRegistry, ItemRegistry, RecipeRegistry (JSON-based)  │
│  - BlockDefinition, ItemDefinition, Recipe                   │
└───────────────────┬─────────────────────────────────────────┘
                    │
┌─────────────────────────────────────────────────────────────┐
│                   Engine Layer (EXISTING)                    │
│  - Renderer (DirectX 11), AudioSystem (FMOD)                 │
│  - KADIWebSocketSubsystem (WebSocket client)                 │
│  - WidgetSubsystem (UI rendering, needs refinement)          │
└─────────────────────────────────────────────────────────────┘
```

### Component Dependency Graph

```
┌──────────────┐
│   Game.cpp   │  (Main entry point)
└──────┬───────┘
       │
       ├──> App ──────────────┐
       │     │                │
       │     └──> World       │
       │          │           │
       │          ├──> Player (has Inventory)
       │          │
       │          ├──> AIAgent (has Inventory + CommandQueue)
       │          │
       │          └──> ItemEntity (drops from mining)
       │
       ├──> WidgetSubsystem   │
       │     │                │
       │     ├──> HotbarWidget
       │     ├──> InventoryWidget
       │     └──> CraftingWidget
       │
       ├──> KADIWebSocketSubsystem
       │     │
       │     └──> KADIToolHandler ──> AgentCommandExecutor
       │
       └──> Registries
            ├──> BlockRegistry (JSON)
            ├──> ItemRegistry (JSON)
            └──> RecipeRegistry (JSON)
```

### Module Boundaries

**Engine → Game (One-Way Dependency)**
- ✅ Game can `#include <Engine/...>`
- ❌ Engine **CANNOT** `#include "Game/..."`
- **Enforcement**: Compile-time errors if violated

**Data Flow:**
1. **Startup**: Load JSON registries (blocks, items, recipes)
2. **Input**: Player input → Game logic → Entity updates
3. **KADI**: WebSocket message → Tool handler → Agent command → World update
4. **Rendering**: World → Chunk meshes → DirectX 11 → Screen
5. **Audio**: Game events → AudioSystem → FMOD → Speakers

---

## Registry System Design

### Overview

Generic, type-safe registry pattern for managing game objects with persistent IDs. Replaces static vectors with JSON-driven, extensible architecture.

### Class Diagram

```cpp
template <typename T>
class Registry
{
public:
    // Registration
    void Register(std::string const& name, T* object);
    void Unregister(std::string const& name);

    // Lookup
    uint16_t GetID(std::string const& name) const;
    T* Get(uint16_t id) const;
    T* Get(std::string const& name) const;

    // Iteration
    size_t GetCount() const;
    T* GetByIndex(size_t index) const;

    // Validation
    bool Exists(std::string const& name) const;
    bool IsValidID(uint16_t id) const;

private:
    std::vector<T*> m_objects;                   // ID → Object (dense array)
    std::map<std::string, uint16_t> m_nameToID;  // Name → ID (case-insensitive)
    mutable std::mutex m_mutex;                  // Thread-safe read access
};
```

**Design Decisions:**

1. **Template Class**: Reusable for `BlockDefinition`, `ItemDefinition`, `Recipe`
2. **uint16_t IDs**: Supports up to 65,535 entries (sufficient for A7)
3. **String → ID Map**: Case-insensitive name lookup for JSON parsing
4. **Dense Array Storage**: ID → Object is O(1) lookup
5. **Thread-Safe Reads**: Mutex protects concurrent access (writes only at startup)
6. **Header-Only**: Template implementation in `.hpp` for compiler optimization

### Block Registry Migration

**Before (XML-based):**
```cpp
// BlockDefinition.hpp
class sBlockDefinition
{
    static std::vector<sBlockDefinition*> s_definitions;  // Static vector

    static void CreateDefinitions();  // Hardcoded in C++
    static sBlockDefinition* GetDefinitionByIndex(uint8_t index);
};
```

**After (JSON-based):**
```cpp
// BlockDefinition.hpp
class sBlockDefinition
{
    // ... properties unchanged ...

    static void LoadDefinitionsFromJSON(std::string const& filepath);
    static Registry<sBlockDefinition>& GetRegistry();
};

// BlockDefinition.cpp
static Registry<sBlockDefinition> g_blockRegistry;

void sBlockDefinition::LoadDefinitionsFromJSON(std::string const& filepath)
{
    std::ifstream file(filepath);
    nlohmann::json data = nlohmann::json::parse(file);

    for (auto& blockJson : data["blocks"])
    {
        sBlockDefinition* def = new sBlockDefinition();
        def->m_name = blockJson["name"];
        def->m_isVisible = blockJson["visible"];
        def->m_isSolid = blockJson["solid"];
        def->m_isOpaque = blockJson["opaque"];
        // ... parse other fields ...

        g_blockRegistry.Register(def->m_name, def);
    }
}

Registry<sBlockDefinition>& sBlockDefinition::GetRegistry()
{
    return g_blockRegistry;
}
```

**JSON Format** (`Run/Data/Definitions/BlockDefinitions.json`):
```json
{
  "blocks": [
    {
      "name": "air",
      "visible": false,
      "solid": false,
      "opaque": false,
      "topSprite": [0, 0],
      "bottomSprite": [0, 0],
      "sideSprite": [0, 0],
      "indoorLighting": 0.0,
      "hardness": 0.0
    },
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

### Item Registry Design

**Class Structure:**
```cpp
// ItemDefinition.hpp
enum class eItemType : uint8_t
{
    RESOURCE,     // Stackable materials (stone, wood, coal)
    TOOL,         // Non-stackable tools (pickaxe, axe, shovel)
    BLOCK,        // Placeable blocks (stone_block, dirt_block)
    CONSUMABLE    // Food, potions (future expansion)
};

class ItemDefinition
{
public:
    std::string   m_name;              // "wooden_pickaxe"
    std::string   m_displayName;       // "Wooden Pickaxe"
    eItemType     m_type;              // TOOL
    Vec2          m_spriteCoords;      // Texture atlas coords
    uint8_t       m_maxStackSize;      // 1 for tools, 64 for resources

    // Tool-specific properties (only for eItemType::TOOL)
    float         m_miningSpeed;       // 2.0x multiplier
    uint16_t      m_durability;        // 60 uses for wooden tools

    // Block-specific properties (only for eItemType::BLOCK)
    uint16_t      m_blockTypeID;       // Points to BlockRegistry ID

    // JSON loading
    static void LoadDefinitionsFromJSON(std::string const& filepath);
    static Registry<ItemDefinition>& GetRegistry();
};
```

**JSON Format** (`Run/Data/Definitions/ItemDefinitions.json`):
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
    },
    {
      "name": "coal",
      "displayName": "Coal",
      "type": "resource",
      "maxStackSize": 64,
      "spriteCoords": [2, 1]
    }
  ]
}
```

### Recipe Registry Design

**Class Structure:**
```cpp
// Recipe.hpp
enum class eRecipeType : uint8_t
{
    SHAPED,      // Pattern matters (e.g., 4 planks in 2×2 = crafting table)
    SHAPELESS    // Pattern doesn't matter (e.g., 1 log = 4 planks)
};

struct RecipeSlot
{
    uint16_t itemID;   // ItemRegistry ID (0 = empty)
    uint8_t  quantity; // Required quantity (usually 1)
};

class Recipe
{
public:
    std::string     m_name;            // "crafting_table"
    eRecipeType     m_type;            // SHAPED or SHAPELESS
    RecipeSlot      m_input[4];        // 2×2 grid (row-major order)
    RecipeSlot      m_output;          // Result item + quantity

    // Matching
    bool Matches(RecipeSlot const input[4]) const;

    // JSON loading
    static void LoadDefinitionsFromJSON(std::string const& filepath);
    static Registry<Recipe>& GetRegistry();
};
```

**JSON Format** (`Run/Data/Definitions/Recipes.json`):
```json
{
  "recipes": [
    {
      "name": "planks_from_log",
      "type": "shapeless",
      "ingredients": ["log"],
      "result": {
        "item": "planks",
        "count": 4
      }
    },
    {
      "name": "crafting_table",
      "type": "shaped",
      "pattern": [
        ["planks", "planks"],
        ["planks", "planks"]
      ],
      "result": {
        "item": "crafting_table",
        "count": 1
      }
    },
    {
      "name": "sticks",
      "type": "shaped",
      "pattern": [
        ["planks", ""],
        ["planks", ""]
      ],
      "result": {
        "item": "stick",
        "count": 4
      }
    }
  ]
}
```

**Recipe Matching Algorithm:**
```cpp
bool Recipe::Matches(RecipeSlot const input[4]) const
{
    if (m_type == eRecipeType::SHAPELESS)
    {
        // Count quantities of each item type
        std::map<uint16_t, uint8_t> inputCounts, recipeCounts;

        for (int i = 0; i < 4; i++)
        {
            if (input[i].itemID != 0)
                inputCounts[input[i].itemID] += input[i].quantity;
            if (m_input[i].itemID != 0)
                recipeCounts[m_input[i].itemID] += m_input[i].quantity;
        }

        return inputCounts == recipeCounts;
    }
    else  // SHAPED
    {
        // Exact pattern match (position matters)
        for (int i = 0; i < 4; i++)
        {
            if (input[i].itemID != m_input[i].itemID ||
                input[i].quantity < m_input[i].quantity)
            {
                return false;
            }
        }
        return true;
    }
}
```

---

## Inventory System Design

### ItemStack Structure

**Design:** Lightweight struct for slot-based storage

```cpp
// ItemStack.hpp
struct ItemStack
{
    uint16_t itemID;      // ItemRegistry ID (0 = empty slot)
    uint8_t  quantity;    // 1-255 (Minecraft uses 1-64 typically)
    uint16_t durability;  // For tools (0 = broken, max varies by tool)

    // Constructors
    ItemStack();  // Empty stack (itemID = 0)
    ItemStack(uint16_t id, uint8_t qty, uint16_t dur = 0);

    // Stack operations
    bool IsEmpty() const { return itemID == 0 || quantity == 0; }
    bool IsFull() const;  // quantity >= maxStackSize
    bool CanMergeWith(ItemStack const& other) const;

    // Merging
    uint8_t AddQuantity(uint8_t amount);  // Returns overflow
    uint8_t TakeQuantity(uint8_t amount); // Returns amount taken

    // Serialization
    void WriteToJSON(nlohmann::json& out) const;
    void ReadFromJSON(nlohmann::json const& in);
};
```

**Memory Layout:**
```
struct ItemStack  // 6 bytes total
{
    uint16_t itemID;      // 2 bytes (0-65535)
    uint8_t  quantity;    // 1 byte (0-255)
    uint16_t durability;  // 2 bytes (0-65535)
    uint8_t  padding;     // 1 byte (alignment)
};
```

**Design Rationale:**
- **Small Size**: 6 bytes → 36 slots = 216 bytes per inventory (cache-friendly)
- **Pass by Value**: Cheap to copy for drag-and-drop operations
- **Durability Optional**: Only used for tools, 0 for other items

### Inventory Class

**Design:** Fixed-size array with Minecraft layout

```cpp
// Inventory.hpp
constexpr int INVENTORY_MAIN_SIZE = 27;     // 3 rows × 9 columns
constexpr int INVENTORY_HOTBAR_SIZE = 9;    // Bottom row
constexpr int INVENTORY_TOTAL_SIZE = 36;    // 27 + 9

class Inventory
{
public:
    Inventory();
    ~Inventory();

    // Slot access
    ItemStack& GetSlot(int slotIndex);
    ItemStack const& GetSlot(int slotIndex) const;

    // Hotbar access (convenience)
    ItemStack& GetHotbarSlot(int hotbarIndex);  // 0-8
    int GetSelectedHotbarSlot() const { return m_selectedHotbarSlot; }
    void SetSelectedHotbarSlot(int index);

    // Add/Remove items
    bool AddItem(uint16_t itemID, uint8_t quantity);  // Auto-find slot
    bool RemoveItem(uint16_t itemID, uint8_t quantity);
    bool HasItem(uint16_t itemID, uint8_t quantity) const;

    // Slot operations
    void SwapSlots(int slotA, int slotB);
    void MergeSlots(int sourceSlot, int destSlot);
    ItemStack TakeSplitStack(int slotIndex, uint8_t takeAmount);

    // Serialization
    void WriteToJSON(nlohmann::json& out) const;
    void ReadFromJSON(nlohmann::json const& in);

private:
    ItemStack m_slots[INVENTORY_TOTAL_SIZE];  // 36 slots (216 bytes)
    int       m_selectedHotbarSlot;           // 0-8 (current hotbar selection)
};
```

**Slot Index Layout (Minecraft Standard):**
```
Main Inventory (Slots 0-26):
┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
│ 0 │ 1 │ 2 │ 3 │ 4 │ 5 │ 6 │ 7 │ 8 │  Row 1
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│ 9 │10 │11 │12 │13 │14 │15 │16 │17 │  Row 2
├───┼───┼───┼───┼───┼───┼───┼───┼───┤
│18 │19 │20 │21 │22 │23 │24 │25 │26 │  Row 3
└───┴───┴───┴───┴───┴───┴───┴───┴───┘

Hotbar (Slots 27-35):
┌───┬───┬───┬───┬───┬───┬───┬───┬───┐
│27 │28 │29 │30 │31 │32 │33 │34 │35 │  Keys 1-9
└───┴───┴───┴───┴───┴───┴───┴───┴───┘
```

### ItemEntity (World Item Drops)

**Design:** Physics-enabled entity for dropped items

```cpp
// ItemEntity.hpp
class ItemEntity : public Entity
{
public:
    ItemEntity(Vec3 const& position, ItemStack const& stack);
    ~ItemEntity();

    // Entity overrides
    virtual void Update(float deltaSeconds) override;
    virtual void Render() const override;

    // Item-specific behavior
    void SetPickupCooldown(float seconds);  // 0.5s after drop (Minecraft)
    bool CanBePickedUp() const;

    ItemStack const& GetItemStack() const { return m_itemStack; }

    // Magnetic attraction (Minecraft mechanic)
    void AttractToPlayer(Vec3 const& playerPos, float deltaSeconds);

private:
    ItemStack m_itemStack;          // What item(s) this entity holds
    float     m_pickupCooldownTimer; // Seconds until can be picked up
    float     m_despawnTimer;        // 5 minutes (Minecraft standard)
    float     m_attractionRange;     // 3 blocks (Minecraft)
};
```

**Behavior:**
- **Spawning**: Drop from center of broken block with random velocity
- **Physics**: Affected by gravity, bounces off terrain
- **Attraction**: Flies toward nearby player within 3 blocks
- **Pickup**: On collision with player, add to inventory
- **Stacking**: Multiple drops of same item merge into one entity
- **Despawn**: After 5 minutes (300 seconds)

**Update Loop:**
```cpp
void ItemEntity::Update(float deltaSeconds)
{
    Entity::Update(deltaSeconds);  // Physics (gravity, collision)

    // Cooldown timer
    m_pickupCooldownTimer -= deltaSeconds;

    // Despawn timer
    m_despawnTimer -= deltaSeconds;
    if (m_despawnTimer <= 0.0f)
    {
        m_isGarbage = true;  // Mark for deletion
        return;
    }

    // Magnetic attraction to player
    Player* player = g_theGame->GetNearestPlayer(m_position, m_attractionRange);
    if (player && CanBePickedUp())
    {
        AttractToPlayer(player->GetPosition(), deltaSeconds);
    }
}

void ItemEntity::AttractToPlayer(Vec3 const& playerPos, float deltaSeconds)
{
    Vec3 toPlayer = playerPos - m_position;
    float distance = toPlayer.GetLength();

    if (distance < 0.5f)  // Close enough to pick up
    {
        // Attempt to add to player inventory
        Player* player = g_theGame->GetPlayer();
        if (player->GetInventory().AddItem(m_itemStack.itemID, m_itemStack.quantity))
        {
            m_isGarbage = true;  // Successfully picked up
            // Play pickup sound
            g_audioSystem->PlaySound(g_theGame->m_soundItemPickup);
        }
    }
    else
    {
        // Apply magnetic force (accelerate toward player)
        Vec3 direction = toPlayer / distance;
        float attractionSpeed = 8.0f;  // Blocks per second (Minecraft-like)
        m_velocity += direction * attractionSpeed * deltaSeconds;
    }
}
```

---

This is getting quite long. Let me continue with the remaining sections. Should I proceed with the rest of the design document (Mining & Placement, UI, AI Agents, KADI, Menu, Sound, Data Flow, etc.)?

