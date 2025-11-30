# Assignment 7: Registry System, Inventory, UI, and AI Agent Integration

**Due Date:** December 12, 2025
**Estimated Duration:** 3-4 weeks
**Complexity:** High (Multi-system integration)
**Implementation Standard:** ⭐ **Follow Minecraft Implementation Patterns** ⭐

---

## Executive Summary

Assignment 7 implements a comprehensive gameplay and AI integration system for SimpleMiner, **following Minecraft's implementation patterns** for all systems:

1. **Minecraft-style Registry System** - Data-driven architecture for blocks, items, and entities (JSON-based)
2. **Inventory System** - Player item storage with hotbar and full inventory management (Minecraft-style)
3. **Mining & Placement Mechanics** - Progressive block breaking and item-based placement (Minecraft mechanics)
4. **Minecraft-authentic UI** - HUD with hotbar, full inventory screen, and crafting interface (pixel-perfect Minecraft style)
5. **AI Agent Framework** - KADI WebSocket-integrated agents that can autonomously play the game
6. **Menu System** - Save/Load/Create game menu (enhanced AttractMode)
7. **Sound Effects** - Minecraft-style audio for mining, placing, and crafting (AudioSystem integration)

This assignment transforms SimpleMiner from a terrain exploration sandbox into a playable Minecraft-like voxel game with intelligent AI agents that can mine, build, and interact with the world through Claude Desktop/Code commands.

---

## Project Context

### Current State (Post-A6)

**Existing Systems:**
- ✅ Newtonian physics with gravity, friction, collision detection (Entity.cpp)
- ✅ 5 camera modes (FIRST_PERSON, OVER_SHOULDER, SPECTATOR, etc.)
- ✅ 3 physics modes (WALKING, FLYING, NOCLIP)
- ✅ BlockDefinition system with XML loading (will migrate to JSON)
- ✅ Chunk-based world with async generation/loading
- ✅ WidgetSubsystem for DirectX 11 UI rendering (Engine) - **needs refinement**
- ✅ KADIWebSocketSubsystem for network communication (Engine/Network)
- ✅ JSON library (`C:\p4\Personal\SD\Engine\Code\ThirdParty\json\json.hpp`)
- ✅ V8 JavaScript integration (currently unused)

**Available Infrastructure:**
- Engine WidgetSubsystem: DX11-based UI rendering (needs redesign/refinement for production use)
- Engine KADIWebSocketSubsystem: WebSocket communication with kadi-broker
- BlockDefinition: JSON-driven block properties (migrating from XML)
- Entity system: Physics-enabled base class for dynamic objects
- Player: Entity with camera and input handling
- Existing KADI prototype: ProtogameJS3D (reference implementation)
- Deployed Agent_TypeScript on DigitalOcean droplet

### Target State (Post-A7)

**New Systems:**
- ✅ Registry<T> template for type-safe ID management
- ✅ ItemRegistry, enhanced BlockRegistry, RecipeRegistry (JSON-based)
- ✅ ItemStack data structure with quantity and type
- ✅ Player inventory (36 slots: 27 main + 9 hotbar)
- ✅ Mining mechanics (progressive break, block → item drop, Minecraft-style)
- ✅ Placement mechanics (consume inventory, place block, Minecraft-style)
- ✅ Minecraft-authentic HUD (hotbar, crosshair, item counts)
- ✅ Minecraft-authentic inventory screen with mouse interaction
- ✅ 2×2 crafting system with 10 Minecraft-style recipes
- ✅ Tool durability system (if feasible)
- ✅ AIAgent entity class with command queue (3 agents for MVP, 10 for later)
- ✅ KADI WebSocket integration (using KADIWebSocketSubsystem)
- ✅ Menu system for save/load/create game (enhanced AttractMode)
- ✅ Item stacking in world (Minecraft behavior)

---

## Goals and Objectives

### Primary Goals

1. **Registry Architecture** - Establish Minecraft-inspired JSON-driven foundation
2. **Core Gameplay Loop** - Enable mine → collect → craft → build cycle (Minecraft mechanics)
3. **UI Polish** - Create pixel-perfect Minecraft-style interface (may require WidgetSubsystem refinement)
4. **AI Integration** - Allow Claude Desktop to control 3-10 agents via KADI WebSocket
5. **Menu System** - Implement save/load/create game functionality

### Secondary Goals

1. **Debug UI Enhancement** - Transform F3 debug overlay to Minecraft style
2. **Performance** - Maintain 60 FPS with inventory UI and 3-10 AI agents
3. **Extensibility** - Design for future expansion (more items, recipes, agent behaviors)
4. **Code Quality** - Clean separation between data (Registry) and logic (Game systems)
5. **Tool Durability** - Implement if not too complex

### Non-Goals (Explicitly Out of Scope)

- ❌ Mob AI system (deferred to A8)
- ❌ Combat/damage system
- ❌ Advanced crafting (3×3 grid, workbench) - only 2×2 for now
- ❌ Health/hunger bars (may add later)
- ❌ Armor system
- ❌ Particle effects
- ❌ Multiplayer networking
- ❌ Goal-based AI (command execution only for A7)

---

## Functional Requirements

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
      "indoorLighting": 0.0
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
- **10 total recipes** (not just 3 starters)

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

### FR-4: User Interface (Minecraft-Authentic Style)

**⚠️ Note**: Current WidgetSubsystem may need redesign/refinement as it's "still very rough" per user feedback.

#### FR-4.1: HUD (Minecraft-Style)
**Description**: Always-visible UI elements during gameplay (pixel-perfect Minecraft style).

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
- **Debug Text** (F3 toggle, Minecraft F3 style):
  - Minecraft F3 debug overlay (left side)
  - Position, chunk coords, FPS, biome, light level (Minecraft info)
  - Black semi-transparent background with white/yellow text (Minecraft style)
  - Rendered using existing debug text system

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
- F3 toggles debug overlay (Minecraft F3 functionality)

#### FR-4.2: Inventory Screen (Minecraft-Authentic)
**Description**: Full inventory UI opened with 'E' key (pixel-perfect Minecraft layout).

**Requirements**:
- **Main Inventory Grid** (center, Minecraft layout):
  - 27 slots (3 rows × 9 columns, Minecraft standard)
  - Slot size: 16×16 pixels for items + padding (Minecraft size)
  - Gray GUI background (Minecraft GUI texture)
- **Hotbar Section** (bottom, Minecraft layout):
  - 9 slots mirroring HUD hotbar
  - Visual connection showing it's the same inventory (Minecraft style)
- **Crafting Grid** (left side, Minecraft layout):
  - 2×2 input grid (4 slots, Minecraft 2×2 crafting)
  - 1 output slot (right of grid with arrow, Minecraft style)
  - Arrow icon between input and output (Minecraft arrow graphic)
- **Mouse Interaction** (Minecraft controls):
  - Left-click slot: Pick up entire stack or place held stack (Minecraft behavior)
  - Right-click slot: Pick up/place single item (Minecraft behavior)
  - Shift-left-click: Quick transfer to/from hotbar (Minecraft convenience)
- **Cursor Item** (held item, Minecraft style):
  - Render item icon following mouse cursor
  - Stack quantity displayed
- **Pause Gameplay** (Minecraft behavior):
  - World updates freeze
  - Player cannot move
  - Mouse cursor visible

**UI Assets**:
- **Find Minecraft GUI textures online** (creative commons/fan resources) or **recreate programmatically**
- Must achieve Minecraft visual authenticity

**WidgetSubsystem Integration**:
- May require **WidgetSubsystem improvements** for modal dialogs and mouse capture
- Create `InventoryWidget` class (extends `IWidget`)
- Register with owner ID = player entity ID
- Modal widget (blocks input to world)

**Acceptance Criteria**:
- Inventory screen looks like Minecraft (visual authenticity)
- 'E' key toggles inventory screen (Minecraft control)
- Can drag-and-drop items between slots (Minecraft behavior)
- Shift-click moves items quickly (Minecraft convenience)
- Crafting grid functional (see FR-4.3)
- Inventory screen pauses gameplay (Minecraft behavior)

#### FR-4.3: Crafting Interface (Minecraft 2×2 Crafting)
**Description**: 2×2 crafting grid with recipe matching (Minecraft mechanics).

**Requirements**:
- 2×2 input grid (part of inventory screen, Minecraft layout)
- Output slot shows craftable item (if valid recipe, Minecraft behavior)
- Recipe matching (Minecraft algorithm):
  - Check all registered recipes
  - Match shaped recipes (pattern exact, Minecraft shaped crafting)
  - Match shapeless recipes (any arrangement, Minecraft shapeless crafting)
- Clicking output slot (Minecraft behavior):
  - Consume input items (1 of each)
  - Add output item to cursor
  - If shift-clicked, add directly to inventory (Minecraft convenience)
- Recipe preview (optional nice-to-have):
  - Hover over output shows recipe in tooltip (Minecraft recipe book style)

**10 Minecraft-Style Recipes** (see FR-1.4 for full list):
1. LOG → 4 PLANKS
2. 4 PLANKS → CRAFTING_TABLE
3. 2 PLANKS → 4 STICKS
... (7 more recipes)

**Acceptance Criteria**:
- Crafting works like Minecraft (behavior match)
- Placing log in any slot shows 4 planks as output
- Placing 4 planks in 2×2 shows crafting table
- Clicking output consumes inputs, gives output (Minecraft behavior)
- Invalid arrangements show no output

---

### FR-5: AI Agent Framework (KADI WebSocket Integration)

#### FR-5.1: AIAgent Entity Class
**Description**: Entity subclass for AI-controlled agents.

**Requirements**:
- Extend `Entity` base class
- Properties:
  - `std::string m_agentName` (unique identifier)
  - `uint64_t m_agentID` (for KADI tracking)
  - `std::queue<AgentCommand> m_commandQueue`
  - `Inventory m_inventory` (36 slots, same as player)
  - `float m_visionRange` (blocks agent can "see")
- Command queue processing:
  - Pop one command per frame
  - Execute command logic
  - Send completion status to KADI
- Rendering:
  - Distinct color/appearance from player
  - Name tag above agent (using debug text)
  - Optional: Velocity vectors (debug mode)
- **Support 3 agents for MVP, 10 for later** (user has DigitalOcean deployment)

**Acceptance Criteria**:
```cpp
AIAgent* agent = world->SpawnAgent("MinerBot", Vec3(100, 100, 70));
agent->QueueCommand(AgentCommand::MOVE, {direction: Vec3(1,0,0), duration: 2.0f});
agent->QueueCommand(AgentCommand::MINE, {x: 101, y: 100, z: 70});
// Agent executes commands sequentially
```

#### FR-5.2: Agent Command System
**Description**: Define agent action primitives (command execution, not goal-based).

**Requirements**:
- `AgentCommand` enum (command execution only for A7):
  - `MOVE` - Move in direction for duration
  - `MINE` - Break block at coordinates
  - `PLACE` - Place block from inventory
  - `CRAFT` - Execute crafting recipe
  - `WAIT` - Idle for duration
- Command parameters (variant/union struct):
  - Vec3 direction, float duration (MOVE)
  - IntVec3 blockCoords (MINE, PLACE)
  - uint16_t recipeID (CRAFT)
  - float duration (WAIT)
- Command execution state machine:
  - QUEUED → IN_PROGRESS → COMPLETED/FAILED
- Failure conditions:
  - Out of reach
  - Invalid target
  - Inventory full (MINE)
  - Missing items (PLACE, CRAFT)
- **Not goal-based AI** (defer to future assignment)

**Acceptance Criteria**:
- Agent can execute all 5 command types
- Commands report completion status
- Failed commands don't crash, return error code

#### FR-5.3: Agent Vision System
**Description**: Agents can query nearby blocks and entities.

**Requirements**:
- `AgentVision::GetNearbyBlocks(radius)`:
  - Returns vector of block types in sphere around agent
  - Includes block coordinates
  - Sorted by distance (nearest first)
- `AgentVision::GetNearbyEntities(radius)`:
  - Returns other agents, player, item entities
  - Includes entity position, type
- Vision caching (update every 0.5 seconds, not every frame)
- Expose vision data to KADI broker

**Acceptance Criteria**:
```cpp
std::vector<BlockInfo> blocks = agent->GetNearbyBlocks(10.0f);
// Returns all blocks within 10-block sphere
// BlockInfo: {IntVec3 coords, uint8_t blockType, float distance}
```

#### FR-5.4: KADI WebSocket Integration (KADIWebSocketSubsystem)
**Description**: Expose SimpleMiner game state to KADI broker via WebSocket.

**Requirements**:
- **Use existing `KADIWebSocketSubsystem.hpp`** from Engine/Network module
- **Reference `ProtogameJS3D` implementation** for integration patterns
- MCP tool registration (SimpleMiner exposes these tools to KADI):
  - `SimpleMiner_Spawn_Agent(name, x, y, z)` → agentID
  - `SimpleMiner_Move_Agent(agentID, dirX, dirY, dirZ, duration)`
  - `SimpleMiner_Mine_Block(agentID, x, y, z)`
  - `SimpleMiner_Place_Block(agentID, blockType, x, y, z)`
  - `SimpleMiner_Get_Agent_Vision(agentID)` → {blocks, entities}
  - `SimpleMiner_Get_Agent_Inventory(agentID)` → {itemStacks}
  - `SimpleMiner_Craft_Item(agentID, recipeID)`
- JSON serialization for all data types
- Non-blocking communication (don't freeze game)
- WebSocket server running on localhost (KADI broker connects)

**Protocol Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "SimpleMiner_Spawn_Agent",
    "arguments": {
      "name": "MinerBot",
      "x": 100,
      "y": 100,
      "z": 70
    }
  }
}
```

**Reference Files**:
- `C:\p4\Personal\SD\Engine\Code\Engine\Network\KADIWebSocketSubsystem.hpp`
- `C:\p4\Personal\SD\ProtogameJS3D` (implementation example)

**Acceptance Criteria**:
- SimpleMiner registers 7 MCP tools with kadi-broker
- Claude Desktop can discover tools via `/mcp` command
- Calling tools from Claude executes in-game actions
- Tool results return to Claude (success/failure, data)

#### FR-5.5: TypeScript Agent Framework
**Description**: Create example agent behaviors in Agent_TypeScript project (deployed on DigitalOcean).

**Requirements**:
- `MinerAgent.ts`:
  - Spawn at specified coordinates
  - Scan for nearest stone blocks (vision system)
  - Navigate to stone block
  - Mine block
  - Collect dropped item
  - Return to start position
- `BuilderAgent.ts`:
  - Given list of block coordinates and types
  - Place blocks sequentially to build structure
  - Handle inventory management (swap items in hotbar)
- Agent utilities:
  - Pathfinding helper (simple A* or greedy)
  - Inventory query helper
  - Vision query helper
- **Deployed on DigitalOcean** (user already has infrastructure)

**Acceptance Criteria**:
- From Claude Desktop: "Spawn a miner agent and mine 10 stone blocks"
- MinerAgent.ts executes autonomously
- Agent appears in SimpleMiner game
- Agent mines blocks and collects items
- Claude receives completion message

---

### FR-6: Menu System (Save/Load/Create Game)

**Description**: Enhance AttractMode to support save/load/create game functionality.

**Requirements**:
- **Menu Screen UI** (Minecraft-style main menu):
  - "Create New Game" button
  - "Load Game" button (shows list of saved games)
  - "Continue" button (resume last game)
  - "Quit" button
- **Save System**:
  - Serialize player inventory to JSON
  - Serialize player position, rotation
  - Serialize world seed, creation date
  - Save to `Saves/<worldName>/player.json`
- **Load System**:
  - Deserialize player data from JSON
  - Load world chunks from disk
  - Restore player state
- **Create Game**:
  - Prompt for world name
  - Generate new world seed
  - Initialize player at spawn point
- **UI Integration**:
  - Use WidgetSubsystem for menu rendering
  - Minecraft-style button graphics
  - Mouse and keyboard navigation

**Acceptance Criteria**:
- Can create new game from menu (Minecraft "Create New World")
- Can save game and load later with inventory preserved
- Can list and select saved games (Minecraft "Select World")
- Menu looks like Minecraft main menu

---

### FR-7: Sound Effects (AudioSystem Integration)

**Description**: Add Minecraft-style sound effects for mining, placing, and crafting.

**Requirements**:
- **Use existing AudioSystem** (`C:\p4\Personal\SD\Engine\Code\Engine\Audio\AudioSystem.hpp`)
- **Mining Sounds**:
  - Different sounds per block type (stone, dirt, wood, etc.) - Minecraft-style
  - Play progressive "dig" sound while mining (tapping sound loop)
  - Play final "break" sound when block breaks
- **Placement Sounds**:
  - Block placement sound (thud/click) - Minecraft-style
  - Different sounds per block type (optional)
- **Crafting Sounds**:
  - Sound when crafting recipe completes - Minecraft-style
  - Item pickup sound when taking crafted item
- **UI Sounds**:
  - Button click sound (menu navigation)
  - Inventory open/close sound
  - Item drag/drop sound
- **Audio Assets**:
  - Find Minecraft-style sound effects online (creative commons) or record/create similar
  - Use AudioSystem to load `.wav` or `.mp3` files
  - Store in `Run/Data/Audio/` directory

**AudioSystem Integration**:
```cpp
// Load sound at startup
SoundID miningSoundStone = g_audioSystem->CreateOrGetSound("Data/Audio/stone_dig.wav");

// Play when mining stone
g_audioSystem->PlaySound(miningSoundStone);
```

**Acceptance Criteria**:
- Mining stone plays stone dig sound (Minecraft-like)
- Breaking block plays break sound
- Placing block plays placement sound
- Crafting item plays crafting sound
- All sounds match Minecraft audio style and quality

---

## Non-Functional Requirements

### NFR-1: Performance
- **Target FPS**: 60 FPS sustained with inventory UI open
- **UI Rendering**: < 2ms per frame for all widgets
- **Agent Update**: < 0.5ms per agent per frame
- **Max Concurrent Agents**: Support 3-10 agents without frame drops

### NFR-2: Memory
- **Registry Overhead**: < 1MB for all registries combined
- **Inventory Storage**: 36 slots × 64 bytes = 2.3KB per player/agent
- **UI Texture Memory**: < 10MB for all UI sprites

### NFR-3: Usability (Minecraft Standard)
- **Input Responsiveness**: < 16ms (one frame) from input to action
- **Visual Feedback**: All interactions show immediate feedback (Minecraft polish)
- **Error Messages**: Clear error messages for KADI tool failures
- **F3 Debug**: Comprehensive debug info for troubleshooting

### NFR-4: Extensibility
- **Registry Design**: Easy to add new blocks, items, recipes via JSON
- **Agent Commands**: New commands added with minimal code changes
- **UI Widgets**: New widgets inherit from IWidget base (may need WidgetSubsystem refinement)
- **KADI Tools**: New tools registered with simple function mapping

### NFR-5: Code Quality
- **SOLID Principles**: Registry pattern demonstrates Single Responsibility
- **DRY**: No duplicate inventory logic between Player and AIAgent
- **YAGNI**: Only implement 2×2 crafting, not 3×3 yet
- **KISS**: Simple command queue for agents, not complex behavior trees
- **Minecraft Fidelity**: All systems follow Minecraft implementation patterns

---

## Constraints and Assumptions

### Technical Constraints
1. **DirectX 11**: All UI must use existing WidgetSubsystem (may need refinement)
2. **Windows Only**: No cross-platform support required
3. **Single-threaded Gameplay**: Game logic runs on main thread (rendering on main thread, chunk gen on workers)
4. **KADIWebSocketSubsystem**: Must use existing Engine network module
5. **JSON Library**: Must use `C:\p4\Personal\SD\Engine\Code\ThirdParty\json\json.hpp`

### Assumptions
1. **KADI Broker Running**: Assumes `kadi-broker` is started separately (not embedded in SimpleMiner)
2. **Claude Desktop Available**: Assumes user has Claude Desktop or Claude Code with KADI MCP server configured
3. **Minecraft Assets**: Assumes UI sprite sheets can be found online or recreated programmatically (creative commons/fan resources)
4. **Save System**: Inventory serialization piggybacks on existing chunk save system
5. **DigitalOcean Deployment**: Agent_TypeScript already deployed and accessible

### Dependencies
1. **Engine WidgetSubsystem**: Must refine/redesign for production use
2. **Engine KADIWebSocketSubsystem**: Must integrate for KADI communication
3. **Engine JSON Library**: Must use for all JSON parsing
4. **KADI Broker**: Must connect to existing `C:\p4\Personal\SD\kadi\kadi-broker` instance
5. **Agent_TypeScript**: Must work with deployed agents on DigitalOcean
6. **ProtogameJS3D**: Must reference for implementation patterns

---

## Prerequisites (MUST Complete Before A7 Implementation)

### P-1: Fix Hidden Surface Removal Bug (BLOCKING)

**Issue**: Some block faces are hidden incorrectly after hidden surface removal implementation.

**Requirements**:
- Investigate face culling logic in Chunk mesh generation
- Identify cases where visible faces are incorrectly culled
- Fix the culling algorithm to only hide truly hidden faces
- Test with various block configurations (overhangs, caves, corners)
- Verify no visual artifacts or missing faces

**Acceptance Criteria**:
- All visible block faces render correctly
- No missing faces in normal gameplay
- Underground caves show correct wall faces
- Building overhangs and structures render properly

### P-2: Update All CLAUDE.md Documentation (REQUIRED)

**Scope**: Update all CLAUDE.md files in both Engine and SimpleMiner codebases to reflect current state.

**Requirements**:
- **Engine Module**: `C:\p4\Personal\SD\Engine\**\CLAUDE.md`
  - Update WidgetSubsystem documentation with current API and status
  - Update KADIWebSocketSubsystem documentation with integration patterns
  - Update AudioSystem documentation
  - Add JSON library documentation
  - Update any outdated system descriptions
- **SimpleMiner Module**: `C:\p4\Personal\SD\SimpleMiner\**\CLAUDE.md`
  - Update Framework module with A6 completion status
  - Update Gameplay module with current Entity/Player state
  - Update Definition module (prepare for JSON migration)
  - Update root CLAUDE.md with Assignment 6 completion and Assignment 7 scope

**Acceptance Criteria**:
- All CLAUDE.md files accurately reflect current codebase state
- New AI conversations can understand current architecture
- Documentation includes A6 completion and A7 prerequisites

### P-3: Create Steering Document (REQUIRED)

**Description**: Create project steering document before design.md per spec-workflow process.

**Requirements**:
- Use `mcp__spec-workflow-SimpleMiner__steering-guide` tool to get template
- Create `product.md` - Product vision for A7 gameplay systems
- Create `tech.md` - Technical architecture and constraints for A7
- Create `structure.md` - File organization and module structure for A7
- Request approval via spec-workflow dashboard

**Acceptance Criteria**:
- Steering documents approved via dashboard
- Clear product vision and technical direction established
- File structure planned before implementation begins

---

## Success Criteria

### Minimum Viable Product (MVP)
**These features MUST work for assignment credit**:

1. ✅ Registry system (Block, Item, Recipe) functional with JSON loading
2. ✅ Player inventory with 36 slots
3. ✅ Mining mechanics (progressive break, item drop, pickup) - Minecraft-style
4. ✅ Placement mechanics (consume inventory, place block) - Minecraft-style
5. ✅ Minecraft-authentic HUD with hotbar and item display
6. ✅ Minecraft-authentic inventory screen with mouse interaction
7. ✅ 2×2 crafting with 10 Minecraft-style recipes
8. ✅ AIAgent spawning and basic commands (MOVE, MINE, PLACE) - 3 agents minimum
9. ✅ KADI WebSocket integration (at least 3 tools: spawn, move, mine)
10. ✅ Menu system for save/load/create game
11. ✅ Sound effects for mining, placing, crafting (AudioSystem integration)

**Demo Scenario**:
```
1. Start game from menu, create new world
2. Player mines 4 logs (break blocks, collect items)
3. Opens inventory (E key) - looks like Minecraft
4. Crafts planks from logs (2×2 grid, Minecraft recipe)
5. Places planks to build structure (right-click)
6. From Claude Desktop: "Spawn miner agent and mine 5 stone"
7. Agent appears in game, executes command autonomously
8. Claude Desktop receives completion confirmation
9. Save game from menu, quit
10. Load game, inventory and agent state restored
```

### Stretch Goals (Nice-to-Have)
**These features are bonus, not required**:

- ⭐ Tool durability fully implemented (if not too hard)
- ⭐ Shift-click quick transfer
- ⭐ Item tooltips on hover
- ⭐ Ghost block preview before placement
- ⭐ Agent pathfinding (A* algorithm)
- ⭐ Multiple agent coordination (10 agents working together)
- ⭐ Recipe preview in crafting grid
- ⭐ Item entity stacking animation (Minecraft item pickup animation)
- ⭐ Pixel-perfect Minecraft UI recreation
- ⭐ WidgetSubsystem full production-ready redesign

---

## Revision Summary

### v3 (Current) - Prerequisites and Sound Effects Added

**Changes from v2 based on user feedback (3 comments):**

1. ✅ **Prerequisites Section Added**: Three blocking/required prerequisites before A7 implementation:
   - P-1: Fix hidden surface removal bug (BLOCKING)
   - P-2: Update all CLAUDE.md files in Engine and SimpleMiner (REQUIRED)
   - P-3: Create steering document before design.md (REQUIRED)
2. ✅ **Sound Effects Added**: Moved from Non-Goals to FR-7 with full AudioSystem integration
   - Mining, placing, crafting sounds (Minecraft-style)
   - Using `C:\p4\Personal\SD\Engine\Code\Engine\Audio\AudioSystem.hpp`
   - Added to MVP as item 11
3. ✅ **Executive Summary**: Updated to reflect 7 major systems (added sound effects)

### v2 - Minecraft Fidelity and System Refinements

**Changes from v1 based on user feedback (14 comments):**

1. ✅ **JSON Migration**: Replaced ALL XML references with JSON (blocks, items, recipes)
2. ✅ **KADI WebSocket**: Use KADIWebSocketSubsystem, reference ProtogameJS3D
3. ✅ **Minecraft Fidelity**: Added "follow Minecraft implementation" throughout
4. ✅ **Menu System**: Added FR-6 for save/load/create game (enhanced AttractMode)
5. ✅ **Agent Count**: 3 for MVP, 10 for later (DigitalOcean deployment noted)
6. ✅ **Recipe Count**: 10 total Minecraft-style recipes (not just 3)
7. ✅ **Tool Durability**: Included in scope (if feasible)
8. ✅ **Item Stacking**: Items stack in world (Minecraft behavior)
9. ✅ **WidgetSubsystem**: Noted needs refinement/redesign
10. ✅ **UI Authenticity**: Emphasized pixel-perfect Minecraft style
11. ✅ **Command-Based AI**: Clarified command execution (not goal-based yet)

---

## References

### Minecraft Systems (Implementation Reference)
- [Minecraft Wiki: Inventory](https://minecraft.wiki/w/Inventory)
- [Minecraft Wiki: Crafting](https://minecraft.wiki/w/Crafting)
- [Minecraft Wiki: Mining](https://minecraft.wiki/w/Mining)
- [Minecraft Wiki: Debug Screen (F3)](https://minecraft.wiki/w/Debug_screen)
- [Minecraft Wiki: Items](https://minecraft.wiki/w/Item)
- [Minecraft Wiki: Blocks](https://minecraft.wiki/w/Block)

### SimpleMiner Codebase
- **Engine**: `C:\p4\Personal\SD\Engine\Code\Engine\`
  - `Widget\` - WidgetSubsystem (needs refinement)
  - `Network\KADIWebSocketSubsystem.hpp` - KADI integration
  - `ThirdParty\json\json.hpp` - JSON library
- **SimpleMiner**: `C:\p4\Personal\SD\SimpleMiner\Code\Game\`
- **KADI**: `C:\p4\Personal\SD\kadi\kadi-broker\`
- **Agent**: `C:\p4\Personal\SD\Agent_TypeScript\` (DigitalOcean deployment)
- **Prototype**: `C:\p4\Personal\SD\ProtogameJS3D\` (reference implementation)

### Documentation
- SimpleMiner CLAUDE.md files (Root, Framework, Gameplay, Definition modules)
- Assignment 6 (Physics & Camera) - archived in `.spec-workflow/archive/specs/A6`
- WidgetSubsystem.hpp - Engine UI rendering system (needs refinement)
- KADIWebSocketSubsystem.hpp - Engine network module
- BlockDefinition system - existing registry pattern (migrating to JSON)

---

## Revision History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-11-24 | Claude Code | Initial requirements document |
| 2.0 | 2025-11-24 | Claude Code | Revised based on user feedback (14 comments): JSON migration, KADI WebSocket, Minecraft fidelity, menu system, agent count (3-10), 10 recipes, tool durability, item stacking, WidgetSubsystem refinement note, UI authenticity, command-based AI |
| 3.0 | 2025-11-24 | Claude Code | Added prerequisites section (3 items: rendering bug fix, CLAUDE.md updates, steering document), added FR-7 sound effects (AudioSystem integration), updated MVP to include 11 items, updated executive summary to 7 systems |

---

**Next Steps**:
1. **Complete Prerequisites (P-1, P-2, P-3)** before starting A7 implementation
2. After prerequisites complete and user approval, proceed to create **steering documents** (product.md, tech.md, structure.md)
3. After steering approval, create **design.md** with detailed architecture, class diagrams, UML, and implementation strategy following Minecraft patterns
