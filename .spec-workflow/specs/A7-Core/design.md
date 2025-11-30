# Assignment 7-Core: Design Document
# Foundation Systems (Registry, Inventory, Mining, Placement)

**Version:** 1.0
**Date:** 2025-11-27
**Status:** Design Phase
**Dependencies:** None (first spec in A7 sequence)

---

## Executive Summary

This design document provides the technical architecture for A7-Core's implementation of foundational gameplay systems. The design covers:

1. **Rendering Bug Fix** - Fix hidden surface removal at chunk boundaries
2. **Registry System** - Generic Registry<T> template with JSON loading
3. **Inventory System** - 36-slot Minecraft-style item storage
4. **Mining & Placement** - Progressive block breaking with tool durability
5. **Basic HUD** - Hotbar widget (bottom-center, Minecraft-style)

**Design Philosophy:**
- **Minecraft Implementation Fidelity** - Follow Minecraft patterns exactly
- **KISS Over YAGNI** - Simple, working implementations for tight deadline (12/12)
- **JSON-First** - All data uses nlohmann/json library
- **SOLID Principles** - Clean separation, dependency injection, const correctness

---

## Table of Contents

1. [Rendering Bug Fix](#1-rendering-bug-fix)
2. [System Architecture Overview](#2-system-architecture-overview)
3. [Registry System Design](#3-registry-system-design)
4. [Inventory System Design](#4-inventory-system-design)
5. [Mining & Placement Mechanics](#5-mining--placement-mechanics)
6. [Basic HUD Design](#6-basic-hud-design)
7. [Data Flow Diagrams](#7-data-flow-diagrams)
8. [Class Hierarchies](#8-class-hierarchies)
9. [JSON Schema Definitions](#9-json-schema-definitions)
10. [Implementation Strategy](#10-implementation-strategy)

---

## 1. Rendering Bug Fix

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

**Problem:** When `neighborIter.IsValid()` returns `false` (horizontal chunk boundary with unloaded neighbor), the function returns `false`, hiding the face. This is incorrect because unloaded chunks at horizontal boundaries are usually **AIR** (especially near surface).

### Solution Design

**Strategy:** Conservative rendering with intelligent defaults

**Key Insight:** When neighbor chunk is not loaded, assume it's **TRANSPARENT** (air) rather than opaque, then hide the face later when the chunk loads if it's actually solid.

**Rationale:**
- **Visual Quality**: Better to show extra faces temporarily than hide visible ones
- **Minecraft Behavior**: Minecraft renders chunk boundaries optimistically
- **Performance**: Extra faces only rendered at chunk edges (small overhead ~16-32 quads per edge)
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
        return true;  // Changed from false to true
    }

    // Rest of function unchanged
    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock) return false;

    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef) return false;

    return !neighborDef->IsOpaque();
}
```

**Change Summary:**
- **Line 3632**: Changed `return false;` to `return true;`
- **Added comments** explaining the fix rationale

### Verification Strategy

**Test Cases:**
1. **Surface Chunk Boundaries**: Stand at edge of loaded chunks, verify surface blocks show all visible faces
2. **Underground Cave Boundaries**: Dig cave spanning chunks, verify cave walls are continuous
3. **Building Across Chunks**: Place blocks spanning chunk boundaries, verify no missing faces
4. **Chunk Load/Unload Cycle**: Move far from chunk, return, verify mesh rebuilds correctly

---

## 2. System Architecture Overview

### Layer Architecture

```
┌─────────────────────────────────────────────────┐
│           UI Layer (Basic HUD)                  │
│     HotbarWidget, Crosshair (WidgetSubsystem)   │
└─────────────────────────────────────────────────┘
                     ↓↑
┌─────────────────────────────────────────────────┐
│         Gameplay Layer (Player, World)          │
│   Player.hpp, World.hpp, ItemEntity.hpp         │
└─────────────────────────────────────────────────┘
                     ↓↑
┌─────────────────────────────────────────────────┐
│        Core Systems (Registry, Inventory)       │
│  Registry<T>, Inventory, ItemStack, Mining      │
└─────────────────────────────────────────────────┘
                     ↓↑
┌─────────────────────────────────────────────────┐
│      Engine Layer (Renderer, Input, Audio)      │
│   WidgetSubsystem, Renderer, InputSystem        │
└─────────────────────────────────────────────────┘
```

### Component Dependency Graph

```
App → World → Player → Inventory → ItemStack
                  ↓
              ItemEntity → ItemStack
                  ↓
              Chunk → Block → BlockDefinition ← BlockRegistry
                                                         ↑
                                                  Registry<T>
                                                         ↑
ItemRegistry ← Registry<T>
RecipeRegistry ← Registry<T>

WidgetSubsystem → HotbarWidget → Player (inventory query)
```

### Module Boundaries

- **Engine → Game (One-Way Dependency)**:
  - **RULE**: Game code can `#include` Engine headers
  - **RULE**: Engine code **CANNOT** `#include` Game headers
  - **RATIONALE**: Engine is reusable, Game is project-specific

---

## 3. Registry System Design

### Generic Registry Template

**File:** `SimpleMiner\Code\Game\Definition\Registry.hpp`

```cpp
template <typename T>
class Registry
{
public:
    // Registration
    void Register(std::string const& name, T* object);

    // Lookup
    uint16_t GetID(std::string const& name) const;
    T* Get(uint16_t id) const;
    T* Get(std::string const& name) const;

    // Iteration
    size_t GetCount() const;
    std::vector<T*> const& GetAll() const;

private:
    std::vector<T*> m_objects;                           // ID → Object (index = ID)
    std::map<std::string, uint16_t> m_nameToID;          // Name → ID (case-insensitive)
    mutable std::shared_mutex m_mutex;                   // Thread-safe read access
};
```

**Key Design Decisions:**

1. **Header-Only Template**: All implementation in `.hpp` for template instantiation
2. **ID Assignment**: Sequential (index in vector = ID)
3. **Thread Safety**: `std::shared_mutex` for concurrent reads, exclusive writes
4. **Case-Insensitive**: Convert names to lowercase for lookup
5. **O(1) Lookup**: Vector index for ID→Object, map for Name→ID

### Block Registry Migration

**Old System** (XML, static vector):
```cpp
// OLD BlockDefinition.cpp:
static std::vector<sBlockDefinition*> s_definitions;
```

**New System** (JSON, Registry):
```cpp
// NEW BlockRegistry.hpp:
class BlockRegistry : public Registry<BlockDefinition>
{
public:
    static BlockRegistry& GetInstance();  // Singleton
    void LoadFromJSON(std::string const& filePath);
};
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
      "hardness": 0.0
    },
    {
      "name": "stone",
      "visible": true,
      "solid": true,
      "opaque": true,
      "topSprite": [1, 0],
      "sideSprite": [1, 0],
      "bottomSprite": [1, 0],
      "indoorLighting": 0.0,
      "hardness": 1.5
    }
  ]
}
```

**Migration Strategy:**
1. Keep BlockDefinition class unchanged (same properties)
2. Replace `s_definitions` with `BlockRegistry::GetInstance()`
3. `Block::GetDefinition()` calls `BlockRegistry::GetInstance().Get(m_typeIndex)`
4. Load JSON at startup, preserve loading order for chunk compatibility

### Item Registry

**File:** `SimpleMiner\Code\Game\Definition\ItemDefinition.hpp`

```cpp
enum class eItemType
{
    RESOURCE,    // Crafting materials (e.g., stick, coal)
    TOOL,        // Tools with durability (e.g., pickaxe, axe)
    BLOCK,       // Placeable blocks (e.g., stone_block, wood_block)
    CONSUMABLE   // Food, potions (future)
};

class ItemDefinition
{
public:
    std::string m_name;             // "stone_block"
    std::string m_displayName;      // "Stone"
    eItemType m_type;               // BLOCK, TOOL, RESOURCE

    // Visual
    IntVec2 m_spriteCoords;         // Texture atlas coordinates (16×16 grid)

    // Stacking
    uint8_t m_maxStackSize;         // 64 for blocks, 1 for tools, 16 for some items

    // Block Items
    uint16_t m_blockTypeID;         // BlockRegistry ID (if type == BLOCK)

    // Tool Items
    float m_miningSpeed;            // Multiplier for mining (e.g., 2.0 for wooden pickaxe)
    uint16_t m_maxDurability;       // Uses before breaking (e.g., 60 for wooden tools)
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
      "maxDurability": 60,
      "maxStackSize": 1,
      "spriteCoords": [0, 3]
    }
  ]
}
```

### Recipe Registry

**File:** `SimpleMiner\Code\Game\Definition\Recipe.hpp`

```cpp
enum class eRecipeType
{
    SHAPED,      // Pattern matters (e.g., 4 planks in 2×2 → crafting table)
    SHAPELESS    // Pattern doesn't matter (e.g., 1 log → 4 planks)
};

class Recipe
{
public:
    uint16_t m_recipeID;                    // Unique ID in RecipeRegistry
    eRecipeType m_type;                     // SHAPED or SHAPELESS

    // Shaped Recipe
    std::array<uint16_t, 4> m_pattern;      // 2×2 grid (itemID, 0 = empty)

    // Shapeless Recipe
    std::vector<uint16_t> m_ingredients;    // List of itemIDs (unordered)

    // Output
    uint16_t m_outputItemID;                // ItemRegistry ID
    uint8_t m_outputQuantity;               // Crafted item count

    // Matching
    bool Matches(std::array<uint16_t, 4> const& craftingGrid) const;
};
```

**JSON Format** (`Run/Data/Definitions/Recipes.json`):
```json
{
  "recipes": [
    {
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

---

## 4. Inventory System Design

### ItemStack Data Structure

**File:** `SimpleMiner\Code\Game\Gameplay\ItemStack.hpp`

```cpp
struct ItemStack
{
    uint16_t itemID;       // ItemRegistry ID (0 = empty)
    uint8_t quantity;      // 1-255 (Minecraft uses 1-64 typically)
    uint16_t durability;   // For tools (0 = broken)

    // Methods
    bool IsEmpty() const { return itemID == 0 || quantity == 0; }
    bool CanMergeWith(ItemStack const& other) const;
    bool IsFull() const;  // quantity >= maxStackSize

    // Stack manipulation
    void Add(uint8_t amount);
    uint8_t Take(uint8_t amount);  // Returns amount taken
    void Clear() { itemID = 0; quantity = 0; durability = 0; }
};
```

**Size:** 6 bytes total (pass by value is efficient)

**Design Decisions:**
- **Small struct**: Pass by value for function parameters
- **Empty representation**: itemID = 0 or quantity = 0
- **Durability**: Only used for tools (ignored for blocks/resources)

### Inventory Class

**File:** `SimpleMiner\Code\Game\Gameplay\Inventory.hpp`

```cpp
class Inventory
{
public:
    static constexpr int MAIN_SLOT_COUNT = 27;     // 3 rows × 9 columns
    static constexpr int HOTBAR_SLOT_COUNT = 9;    // Bottom row
    static constexpr int TOTAL_SLOT_COUNT = 36;    // Main + Hotbar

    // Access
    ItemStack& GetSlot(int slotIndex);             // 0-35
    ItemStack& GetMainSlot(int index);             // 0-26
    ItemStack& GetHotbarSlot(int index);           // 0-8

    // Hotbar
    int GetSelectedHotbarSlot() const;             // 0-8
    void SetSelectedHotbarSlot(int slot);          // 1-9 keys
    ItemStack& GetSelectedItemStack();

    // Operations
    bool AddItem(ItemStack const& item);           // Find empty/matching slot
    bool RemoveItem(uint16_t itemID, uint8_t quantity);
    void SwapSlots(int slot1, int slot2);
    bool MergeSlots(int sourceSlot, int destSlot);

    // Serialization
    void SaveToJSON(Json::Value& root) const;
    void LoadFromJSON(Json::Value const& root);

private:
    std::array<ItemStack, TOTAL_SLOT_COUNT> m_slots;  // 0-26 main, 27-35 hotbar
    int m_selectedHotbarSlot = 0;                     // 0-8
};
```

**Layout:**
```
Slots 0-26: Main Inventory (3 rows × 9 columns)
┌─┬─┬─┬─┬─┬─┬─┬─┬─┐
│0│1│2│3│4│5│6│7│8│
├─┼─┼─┼─┼─┼─┼─┼─┼─┤
│9│...      ...│17│
├─┼─┼─┼─┼─┼─┼─┼─┼─┤
│18│...     ...│26│
└─┴─┴─┴─┴─┴─┴─┴─┴─┘

Slots 27-35: Hotbar (bottom row, visible in HUD)
┌─┬─┬─┬─┬─┬─┬─┬─┬─┐
│27│28│29│30│31│32│33│34│35│
└─┴─┴─┴─┴─┴─┴─┴─┴─┘
```

**AddItem Algorithm** (Minecraft behavior):
1. Find existing slot with same itemID (not full)
2. Merge with existing stack (up to maxStackSize)
3. If excess, find empty slot and create new stack
4. Repeat until all added or inventory full
5. Return true if all added, false if partial/failed

### ItemEntity Class

**File:** `SimpleMiner\Code\Game\Gameplay\ItemEntity.hpp`

```cpp
class ItemEntity : public Entity
{
public:
    // Constructor
    ItemEntity(Vec3 const& position, ItemStack const& item);

    // Entity Overrides
    virtual void Update(float deltaSeconds) override;
    virtual void Render() const override;

    // Item Properties
    ItemStack const& GetItemStack() const { return m_item; }
    void SetItemStack(ItemStack const& item) { m_item = item; }

    // Pickup
    bool CanBePickedUp() const;               // Checks cooldown
    void OnPickedUp();                        // Called when added to inventory

    // Stacking
    bool CanStackWith(ItemEntity const& other) const;
    void MergeWith(ItemEntity& other);        // Combine quantities, destroy other

private:
    ItemStack m_item;                         // What item this represents
    float m_pickupCooldown = 0.5f;            // Time after spawn before can pickup
    float m_magnetRadius = 3.0f;              // Blocks distance for magnetic pull
    float m_despawnTimer = 300.0f;            // 5 minutes (Minecraft standard)

    void ApplyMagneticPull(float deltaSeconds);  // Pull toward nearby player
};
```

**Magnetic Pickup** (Minecraft mechanic):
```cpp
void ItemEntity::ApplyMagneticPull(float deltaSeconds)
{
    Player* player = World::GetInstance()->GetPlayer();
    Vec3 toPlayer = player->GetPosition() - m_position;
    float distance = toPlayer.GetLength();

    if (distance < m_magnetRadius && CanBePickedUp())
    {
        Vec3 pullDir = toPlayer.GetNormalized();
        float pullSpeed = 5.0f;  // Blocks/second
        m_velocity += pullDir * pullSpeed * deltaSeconds;
    }
}
```

**Item Stacking in World** (Minecraft behavior):
- When ItemEntity spawns, check for nearby ItemEntity of same type
- If found within 1 block radius, merge quantities, destroy duplicate entity
- Prevents clutter from mass block breaking

---

## 5. Mining & Placement Mechanics

### Progressive Block Breaking

**File:** `SimpleMiner\Code\Game\Gameplay\Player.cpp`

**Mining State Machine:**
```cpp
enum class eMiningState
{
    IDLE,               // Not mining
    MINING,             // Holding left mouse, progress increasing
    BROKEN              // Block just broke, spawn ItemEntity this frame
};

class Player
{
private:
    eMiningState m_miningState = eMiningState::IDLE;
    IntVec3 m_targetBlockCoords;        // Block being mined
    float m_miningProgress = 0.0f;      // 0.0 to 1.0
    float m_breakTime = 0.0f;           // Seconds to break (depends on hardness, tool)
};
```

**Break Time Formula** (Minecraft-based):
```cpp
float Player::CalculateBreakTime(Block const& block, ItemStack const& tool)
{
    BlockDefinition* blockDef = BlockRegistry::GetInstance().Get(block.m_typeIndex);
    float hardness = blockDef->m_hardness;  // e.g., 1.5 for stone

    float toolEffectiveness = 1.0f;  // Hand mining
    if (!tool.IsEmpty())
    {
        ItemDefinition* toolDef = ItemRegistry::GetInstance().Get(tool.itemID);
        if (toolDef->m_type == eItemType::TOOL)
        {
            toolEffectiveness = toolDef->m_miningSpeed;  // e.g., 2.0 for wooden pickaxe
        }
    }

    float breakTime = hardness / toolEffectiveness;
    return breakTime;
}
```

**Mining Update Loop:**
```cpp
void Player::UpdateMining(float deltaSeconds)
{
    if (m_miningState == eMiningState::IDLE)
    {
        if (g_inputSystem->IsKeyDown(MOUSE_LEFT))
        {
            RaycastResult result = RaycastToBlock(6.0f);  // 6 block range
            if (result.hit)
            {
                m_targetBlockCoords = result.blockCoords;
                m_miningProgress = 0.0f;
                m_breakTime = CalculateBreakTime(result.block, GetSelectedItemStack());
                m_miningState = eMiningState::MINING;
            }
        }
    }
    else if (m_miningState == eMiningState::MINING)
    {
        // Check cancel conditions
        if (!g_inputSystem->IsKeyDown(MOUSE_LEFT) ||
            !IsBlockInView(m_targetBlockCoords, 6.0f))
        {
            m_miningState = eMiningState::IDLE;
            m_miningProgress = 0.0f;
            return;
        }

        // Increment progress
        m_miningProgress += deltaSeconds / m_breakTime;

        // Check completion
        if (m_miningProgress >= 1.0f)
        {
            BreakBlock(m_targetBlockCoords);
            m_miningState = eMiningState::BROKEN;
        }
    }
    else if (m_miningState == eMiningState::BROKEN)
    {
        // Reset next frame
        m_miningState = eMiningState::IDLE;
        m_miningProgress = 0.0f;
    }
}
```

**Visual Crack Overlay** (10 stages, 0-9):
```cpp
void Player::RenderMiningProgress() const
{
    if (m_miningState != eMiningState::MINING) return;

    int crackStage = (int)(m_miningProgress * 10.0f);  // 0-9
    crackStage = Clamp(crackStage, 0, 9);

    // Render crack texture overlay at target block position
    Texture* crackTexture = g_renderer->GetTexture("Data/Images/Cracks.png");
    IntVec2 spriteCoords(crackStage, 0);  // Crack stages in horizontal strip

    // Draw crack on all 6 faces of target block
    DrawBlockOverlay(m_targetBlockCoords, crackTexture, spriteCoords);
}
```

**Tool Durability:**
```cpp
void Player::BreakBlock(IntVec3 const& coords)
{
    // Remove block from world
    m_world->SetBlock(coords, BLOCK_AIR);

    // Spawn ItemEntity with block drop
    Vec3 spawnPos = Vec3((float)coords.x + 0.5f, (float)coords.y + 0.5f, (float)coords.z + 0.5f);
    ItemStack droppedItem = GetBlockDropItem(coords);  // e.g., GRASS → DIRT
    m_world->SpawnItemEntity(spawnPos, droppedItem);

    // Decrease tool durability
    ItemStack& tool = GetSelectedItemStack();
    if (!tool.IsEmpty() && tool.durability > 0)
    {
        tool.durability--;
        if (tool.durability == 0)
        {
            tool.Clear();  // Tool broke, remove from inventory
        }
    }

    // Rebuild chunk mesh
    Chunk* chunk = m_world->GetChunkAtWorldCoords(coords);
    chunk->SetDirty();
}
```

### Block Placement

**Placement Raycast:**
```cpp
RaycastResult Player::RaycastForPlacement(float maxDistance)
{
    Ray ray(m_cameraPosition, m_cameraForward);
    RaycastResult result = m_world->Raycast(ray, maxDistance);

    if (result.hit)
    {
        // Calculate placement position (adjacent to hit face)
        result.placementCoords = result.blockCoords + result.hitNormal;
    }

    return result;
}
```

**Placement Validation:**
```cpp
bool Player::CanPlaceBlock(IntVec3 const& coords)
{
    // Check 1: Position not occupied by solid block
    Block* existingBlock = m_world->GetBlock(coords);
    if (existingBlock && existingBlock->IsSolid()) return false;

    // Check 2: Player not intersecting placed block position
    AABB3 blockAABB = AABB3::MakeFromMinMaxInts(coords, coords + IntVec3(1,1,1));
    if (blockAABB.IsPointInside(m_position)) return false;

    // Check 3: Within reach (6 blocks)
    float distance = (Vec3(coords) - m_position).GetLength();
    if (distance > 6.0f) return false;

    return true;
}
```

**Placement Logic:**
```cpp
void Player::UpdatePlacement()
{
    if (g_inputSystem->WasKeyJustPressed(MOUSE_RIGHT))
    {
        RaycastResult result = RaycastForPlacement(6.0f);
        if (!result.hit) return;

        IntVec3 placeCoords = result.placementCoords;
        if (!CanPlaceBlock(placeCoords)) return;

        // Get selected item from hotbar
        ItemStack& item = GetSelectedItemStack();
        if (item.IsEmpty()) return;

        ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(item.itemID);
        if (itemDef->m_type != eItemType::BLOCK) return;

        // Place block
        uint16_t blockType = itemDef->m_blockTypeID;
        m_world->SetBlock(placeCoords, blockType);

        // Consume item
        item.Take(1);

        // Rebuild chunk mesh
        Chunk* chunk = m_world->GetChunkAtWorldCoords(placeCoords);
        chunk->SetDirty();
    }
}
```

---

## 6. Basic HUD Design

### HotbarWidget Class

**File:** `SimpleMiner\Code\Game\UI\HotbarWidget.hpp`

```cpp
class HotbarWidget : public IWidget
{
public:
    HotbarWidget();

    // IWidget Interface
    virtual void Update(float deltaSeconds) override;
    virtual void Render() const override;
    virtual bool OnMouseEvent(MouseEvent const& event) override;
    virtual bool OnKeyEvent(KeyEvent const& event) override;

    // Configuration
    void SetPlayer(Player* player) { m_player = player; }

private:
    Player* m_player = nullptr;

    // Layout
    Vec2 m_position;          // Bottom-center of screen
    Vec2 m_slotSize;          // 18×18 pixels per slot (16px item + 1px padding)
    Vec2 m_backgroundSize;    // 182×22 pixels (Minecraft hotbar)

    // Textures
    Texture* m_backgroundTexture = nullptr;  // Gray GUI background
    Texture* m_selectionTexture = nullptr;   // White border for selected slot
    Texture* m_itemSpriteSheet = nullptr;    // Item icons (16×16 grid)

    // Rendering
    void RenderBackground() const;
    void RenderSlots() const;
    void RenderSelection() const;
    void RenderItemIcon(int slotIndex, ItemStack const& item) const;
    void RenderItemQuantity(int slotIndex, uint8_t quantity) const;
};
```

**Hotbar Layout (Minecraft style):**
```
┌────────────────────────────────────────────────┐
│  [0] [1] [2] [3] [4] [5] [6] [7] [8]          │
│   1   2   3   4   5   6   7   8   9  (keys)   │
└────────────────────────────────────────────────┘
Bottom-center of screen (centered horizontally)
```

**Rendering Pipeline:**
```cpp
void HotbarWidget::Render() const
{
    if (!m_player) return;

    // 1. Render background (gray GUI texture)
    RenderBackground();

    // 2. Render selection highlight (white border around selected slot)
    RenderSelection();

    // 3. Render item icons and quantities for each slot
    Inventory& inventory = m_player->GetInventory();
    for (int i = 0; i < 9; i++)
    {
        ItemStack const& item = inventory.GetHotbarSlot(i);
        if (!item.IsEmpty())
        {
            RenderItemIcon(i, item);
            if (item.quantity > 1)
            {
                RenderItemQuantity(i, item.quantity);
            }
        }
    }
}
```

**Item Icon Rendering:**
```cpp
void HotbarWidget::RenderItemIcon(int slotIndex, ItemStack const& item) const
{
    ItemDefinition* itemDef = ItemRegistry::GetInstance().Get(item.itemID);
    if (!itemDef) return;

    // Calculate slot position
    Vec2 slotPos = m_position + Vec2(slotIndex * 20.0f, 0.0f);  // 20px spacing

    // Get sprite coordinates from item definition
    IntVec2 spriteCoords = itemDef->m_spriteCoords;  // e.g., (1, 0) for stone

    // Render 16×16 item icon from sprite sheet
    AABB2 uvs = GetUVsForSpriteCoords(spriteCoords, IntVec2(16, 16));  // 16×16 grid
    g_renderer->DrawTexturedQuad(slotPos, Vec2(16.0f, 16.0f), m_itemSpriteSheet, uvs);
}
```

**Item Quantity Rendering:**
```cpp
void HotbarWidget::RenderItemQuantity(int slotIndex, uint8_t quantity) const
{
    // Render quantity text in bottom-right corner of slot (Minecraft style)
    Vec2 slotPos = m_position + Vec2(slotIndex * 20.0f, 0.0f);
    Vec2 textPos = slotPos + Vec2(12.0f, 2.0f);  // Bottom-right offset

    std::string quantityText = std::to_string(quantity);
    g_renderer->DrawText2D(textPos, quantityText, 8.0f, Rgba8::WHITE);
}
```

### Crosshair Widget

**Simple Rendering:**
```cpp
void RenderCrosshair()
{
    // Center of screen
    Vec2 screenCenter = g_window->GetDimensions() * 0.5f;

    // Draw white cross (5px lines)
    g_renderer->DrawLine2D(screenCenter + Vec2(-5, 0), screenCenter + Vec2(5, 0), Rgba8::WHITE, 2.0f);
    g_renderer->DrawLine2D(screenCenter + Vec2(0, -5), screenCenter + Vec2(0, 5), Rgba8::WHITE, 2.0f);
}
```

---

## 7. Data Flow Diagrams

### Mining Flow
```
Player (left-click) → Raycast(6 blocks) → Hit Block
       ↓
   Update Mining Progress (deltaTime / breakTime)
       ↓
   Render Crack Overlay (progress * 10 stages)
       ↓
   Progress >= 1.0? → BreakBlock()
       ↓
   World::SetBlock(coords, AIR)
       ↓
   SpawnItemEntity(block drop)
       ↓
   Chunk::SetDirty() → Rebuild Mesh
       ↓
   Decrease Tool Durability
```

### Placement Flow
```
Player (right-click) → Raycast(6 blocks) → Hit Block
       ↓
   Calculate Placement Position (adjacent to hit face)
       ↓
   Validate Placement (not occupied, in reach, not inside player)
       ↓
   Get Selected ItemStack → Check if BLOCK type
       ↓
   World::SetBlock(coords, blockType)
       ↓
   ItemStack::Take(1) → Decrease quantity
       ↓
   Chunk::SetDirty() → Rebuild Mesh
```

### Item Pickup Flow
```
ItemEntity (world) → Update() → ApplyMagneticPull()
       ↓
   Distance < 3 blocks? → Apply pull velocity toward player
       ↓
   Collision with Player? → Player::OnItemEntityPickup()
       ↓
   Inventory::AddItem(itemStack)
       ↓
   Success? → ItemEntity::OnPickedUp() → Destroy entity
   Failed? → Item remains in world
```

---

## 8. Class Hierarchies

### Entity Inheritance
```
Entity (abstract)
├── Player
├── ItemEntity
└── Prop (existing)
```

### Widget Inheritance
```
IWidget (Engine interface)
└── HotbarWidget
```

### Definition Classes
```
[NO INHERITANCE]

BlockDefinition (standalone)
ItemDefinition (standalone)
Recipe (standalone)
```

---

## 9. JSON Schema Definitions

### BlockDefinitions.json
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "blocks": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "visible": { "type": "boolean" },
          "solid": { "type": "boolean" },
          "opaque": { "type": "boolean" },
          "topSprite": { "type": "array", "items": { "type": "integer" }, "minItems": 2, "maxItems": 2 },
          "sideSprite": { "type": "array", "items": { "type": "integer" }, "minItems": 2, "maxItems": 2 },
          "bottomSprite": { "type": "array", "items": { "type": "integer" }, "minItems": 2, "maxItems": 2 },
          "indoorLighting": { "type": "number" },
          "hardness": { "type": "number" }
        },
        "required": ["name", "visible", "solid", "opaque", "hardness"]
      }
    }
  },
  "required": ["blocks"]
}
```

### ItemDefinitions.json
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "items": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "name": { "type": "string" },
          "displayName": { "type": "string" },
          "type": { "enum": ["resource", "tool", "block", "consumable"] },
          "blockType": { "type": "string" },
          "maxStackSize": { "type": "integer", "minimum": 1, "maximum": 255 },
          "spriteCoords": { "type": "array", "items": { "type": "integer" }, "minItems": 2, "maxItems": 2 },
          "miningSpeed": { "type": "number" },
          "maxDurability": { "type": "integer" }
        },
        "required": ["name", "displayName", "type", "maxStackSize", "spriteCoords"]
      }
    }
  },
  "required": ["items"]
}
```

### Recipes.json
```json
{
  "$schema": "http://json-schema.org/draft-07/schema#",
  "type": "object",
  "properties": {
    "recipes": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "type": { "enum": ["shaped", "shapeless"] },
          "pattern": {
            "type": "array",
            "items": {
              "type": "array",
              "items": { "type": "string" },
              "minItems": 2,
              "maxItems": 2
            },
            "minItems": 2,
            "maxItems": 2
          },
          "ingredients": {
            "type": "array",
            "items": { "type": "string" }
          },
          "result": {
            "type": "object",
            "properties": {
              "item": { "type": "string" },
              "count": { "type": "integer", "minimum": 1 }
            },
            "required": ["item", "count"]
          }
        },
        "required": ["type", "result"]
      }
    }
  },
  "required": ["recipes"]
}
```

---

## 10. Implementation Strategy

### Phase 1: Rendering Bug Fix (1-2 hours)
1. Open `Chunk.cpp:3632`
2. Change `return false;` to `return true;`
3. Add comment explaining fix
4. Test with chunk boundaries
5. Verify no visual artifacts

### Phase 2: Registry System (4-6 hours)
1. Create `Registry<T>` template in `Definition/Registry.hpp`
2. Migrate BlockDefinition to JSON (keep class unchanged)
3. Implement `BlockRegistry::LoadFromJSON()`
4. Create `ItemDefinition` class
5. Create `ItemRegistry::LoadFromJSON()`
6. Create `Recipe` class
7. Create `RecipeRegistry::LoadFromJSON()`
8. Test registry lookups (name→ID, ID→object)

### Phase 3: Inventory System (6-8 hours)
1. Create `ItemStack` struct
2. Create `Inventory` class with 36 slots
3. Add inventory to `Player` class
4. Implement `Inventory::AddItem()` (Minecraft behavior)
5. Create `ItemEntity` class (extends Entity)
6. Implement magnetic pickup
7. Test item pickup and inventory filling

### Phase 4: Mining & Placement (8-10 hours)
1. Implement progressive mining in `Player::UpdateMining()`
2. Add crack overlay rendering
3. Implement `BreakBlock()` with ItemEntity spawning
4. Add tool durability system
5. Implement placement raycast
6. Implement `CanPlaceBlock()` validation
7. Implement `UpdatePlacement()` logic
8. Test mining and placement mechanics

### Phase 5: Basic HUD (4-6 hours)
1. Create `HotbarWidget` class (extends IWidget)
2. Load hotbar background texture
3. Implement slot rendering
4. Implement item icon rendering
5. Implement quantity text rendering
6. Implement selection highlight
7. Add crosshair rendering
8. Test hotbar with inventory changes

### Total Estimated Time: 23-32 hours (1-2 weeks)

---

**Next Steps:**
1. Approve this design.md via spec-workflow
2. Create tasks.md breaking down implementation into specific tasks
3. Begin Phase 1 (Rendering Bug Fix)
4. Proceed sequentially through phases
