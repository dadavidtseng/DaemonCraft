# Assignment 7-AI: AI Agent Integration (KADI WebSocket, Agent Framework)

**Due Date:** December 12, 2025 (Week 4)
**Estimated Duration:** 1 week
**Complexity:** High (Network integration, multi-agent system)
**Implementation Standard:** ⭐ **Follow Minecraft Implementation Patterns** ⭐
**Dependencies:** ✅ Requires A7-Core completion (Registry, Inventory systems)

---

## Executive Summary

Assignment 7-AI implements the AI agent framework and KADI WebSocket integration, enabling Claude Desktop/Code to control autonomous agents in SimpleMiner:

1. **Agent Entity Class** - Autonomous entity with inventory and command queue
2. **Agent Command System** - MOVE, MINE, PLACE, CRAFT, WAIT commands
3. **Agent Vision System** - Query nearby blocks and entities
4. **KADI WebSocket Integration** - 7 MCP tools for agent control (using KADIWebSocketSubsystem)
5. **TypeScript Agent Framework** - Example agent behaviors (deployed on DigitalOcean)

This assignment completes Assignment 7 by enabling AI-game integration, allowing Claude to spawn and control 3-10 autonomous agents that can mine, build, and interact with the world.

---

## Project Context

### Current State (Post-A7-Core)

**Existing Systems from A7-Core:**
- ✅ Registry<T> template for type-safe ID management
- ✅ ItemRegistry, BlockRegistry, RecipeRegistry (JSON-based)
- ✅ ItemStack data structure (itemID, quantity, durability)
- ✅ Player inventory (36 slots: 27 main + 9 hotbar)
- ✅ ItemEntity for dropped items (magnetic pickup, world physics)
- ✅ Mining mechanics (progressive break, block → item drop)
- ✅ Placement mechanics (consume inventory, place block)
- ✅ Entity system (base class with physics)

**Available Infrastructure:**
- Engine KADIWebSocketSubsystem: WebSocket communication with kadi-broker
- Existing KADI prototype: ProtogameJS3D (reference implementation)
- Deployed Agent_TypeScript on DigitalOcean droplet
- Entity base class: Physics-enabled entities with position, velocity

### Target State (Post-A7-AI)

**New Systems:**
- ✅ Agent entity class (extends Entity, has Inventory + CommandQueue)
- ✅ Agent command system (MOVE, MINE, PLACE, CRAFT, WAIT)
- ✅ Agent vision system (nearby blocks/entities, 3D sphere query)
- ✅ KADI WebSocket integration (7 MCP tools with lowercase_with_underscores naming)
- ✅ TypeScript agent framework (MinerAgent, BuilderAgent)

---

## Goals and Objectives

### Primary Goals

1. **AI Agent Framework** - Enable 3-10 autonomous agents with command execution
2. **KADI WebSocket Integration** - Expose SimpleMiner to Claude Desktop via MCP tools
3. **Agent Behaviors** - Create example TypeScript agents (MinerAgent, BuilderAgent)
4. **Multi-Agent Support** - Handle 3-10 concurrent agents without frame drops

### Secondary Goals

1. **Performance** - Maintain 60 FPS with 3-10 active agents
2. **Extensibility** - Design for future goal-based AI (defer to future assignment)
3. **Debugging** - Visualize agent state (name tags, command queue, vision range)

### Non-Goals (Explicitly Out of Scope for A7-AI)

- ❌ Goal-based AI (command execution only for A7)
- ❌ Pathfinding (simple movement commands only)
- ❌ Agent-agent cooperation (agents work independently)
- ❌ Combat AI (no damage/health system yet)
- ❌ Advanced behaviors (building planning, resource optimization)
- ❌ Menu system (deferred to A7-Polish)
- ❌ Sound effects (deferred to A7-Polish)

---

## Functional Requirements

### FR-1: Agent Entity Class

#### FR-1.1: Agent Class Design
**Description**: Entity subclass for AI-controlled agents.

**Requirements**:
- **Extend Entity Base Class**:
  - Inherit physics (position, velocity, gravity, collision)
  - Inherit rendering (cube model with distinct color)
- **Agent Properties**:
  - `std::string m_agentName` - Unique identifier (e.g., "MinerBot")
  - `uint64_t m_agentID` - Numeric ID for KADI tracking
  - `std::queue<AgentCommand> m_commandQueue` - Queued commands
  - `Inventory m_inventory` - 36 slots (same as player, from A7-Core)
  - `float m_visionRange` - Blocks agent can "see" (default: 10.0f)
  - `AgentCommandState m_currentCommand` - Currently executing command
- **Agent Lifecycle**:
  - Spawn at specified coordinates (via KADI tool or debug command)
  - Update every frame (execute command queue)
  - Despawn on death or KADI command
- **Rendering**:
  - Distinct color from player (e.g., blue cube for agents, player is green)
  - Name tag above agent (using debug text system)
  - Optional: Velocity vectors in debug mode (F3 toggle)

**Acceptance Criteria**:
```cpp
Agent* agent = world->SpawnAgent("MinerBot", Vec3(100, 100, 70));
agent->QueueCommand(AgentCommand::MOVE, {direction: Vec3(1,0,0), duration: 2.0f});
agent->QueueCommand(AgentCommand::MINE, {coords: IntVec3(101, 100, 70)});
// Agent executes commands sequentially over multiple frames
```

#### FR-1.2: Command Queue Processing
**Description**: Execute queued commands frame-by-frame.

**Requirements**:
- **Command Queue**:
  - `std::queue<AgentCommand>` - First-in, first-out
  - Pop one command per frame when previous completes
  - Continue until queue empty
- **Command Execution**:
  - Each command has `Execute()` and `IsComplete()` methods
  - MOVE: Apply velocity over duration
  - MINE: Raycast to block, apply mining progress (reuse player mining logic)
  - PLACE: Raycast for placement position, place block (reuse player placement logic)
  - CRAFT: Match recipe in inventory crafting grid, craft item
  - WAIT: Idle for duration
- **Command State Machine**:
  - QUEUED → IN_PROGRESS → COMPLETED/FAILED
  - Completed commands removed from queue
  - Failed commands log error, continue to next command
- **Completion Notification**:
  - Send WebSocket message to KADI when command completes
  - Include success/failure status, error message (if failed)

**Acceptance Criteria**:
- Agent executes commands in order (FIFO queue)
- Commands complete or fail gracefully (no crashes)
- Agent idles when queue empty
- KADI receives completion notifications

---

### FR-2: Agent Command System

#### FR-2.1: AgentCommand Enum and Data
**Description**: Define agent action primitives (command execution, not goal-based).

**Requirements**:
- **AgentCommand Enum**:
  ```cpp
  enum class eAgentCommandType
  {
      MOVE,      // Move in direction for duration
      MINE,      // Break block at coordinates
      PLACE,     // Place block from inventory
      CRAFT,     // Execute crafting recipe
      WAIT       // Idle for duration
  };
  ```
- **Command Parameters** (variant/union struct):
  ```cpp
  struct AgentCommandParams
  {
      // MOVE parameters
      Vec3 direction;       // Normalized direction vector
      float duration;       // Seconds to move

      // MINE/PLACE parameters
      IntVec3 blockCoords;  // World block coordinates
      uint16_t blockType;   // Block type to place (PLACE only)

      // CRAFT parameters
      uint16_t recipeID;    // Recipe index in RecipeRegistry
  };
  ```
- **AgentCommand Class**:
  ```cpp
  class AgentCommand
  {
  public:
      eAgentCommandType GetType() const;
      void Execute(Agent* agent, float deltaSeconds);
      bool IsComplete() const;
      bool IsFailed() const;
      std::string GetErrorMessage() const;
  };
  ```

**Acceptance Criteria**:
- All 5 command types defined and functional
- Commands can be created with parameters
- Commands execute over multiple frames (incremental progress)
- Commands report completion/failure status

#### FR-2.2: Command Implementation Details
**Description**: Detailed behavior for each command type.

**MOVE Command**:
- **Parameters**: `Vec3 direction`, `float duration`
- **Behavior**:
  - Normalize direction vector
  - Set agent velocity = direction * AGENT_MOVE_SPEED
  - Continue for `duration` seconds
  - Stop movement when complete (velocity = 0)
- **Failure Conditions**: None (always completes after duration)

**MINE Command**:
- **Parameters**: `IntVec3 blockCoords`
- **Behavior**:
  - Raycast from agent position to block (check if within reach, max 6 blocks)
  - Apply mining progress over time (reuse player mining logic from A7-Core)
  - When progress reaches 1.0, break block:
    - Remove block from world (set to AIR)
    - Spawn ItemEntity with block drop
    - Add item to agent inventory (if space available)
  - Tool durability decreases if agent holding tool in selected hotbar slot
- **Failure Conditions**:
  - Block out of reach (> 6 blocks distance)
  - Block already AIR (invalid target)
  - Agent moved too far during mining (> 7 blocks)

**PLACE Command**:
- **Parameters**: `IntVec3 blockCoords`, `uint16_t blockType`
- **Behavior**:
  - Check if block item in agent inventory (selected hotbar slot)
  - Raycast for placement position (adjacent to target, reuse player placement logic)
  - Check placement validity (not occupied, within reach, not inside agent)
  - Place block in world
  - Consume 1 item from agent inventory
  - Update chunk mesh
- **Failure Conditions**:
  - No block item in selected hotbar slot
  - Position occupied by solid block
  - Out of reach (> 6 blocks distance)
  - Agent intersects placed block position

**CRAFT Command**:
- **Parameters**: `uint16_t recipeID`
- **Behavior**:
  - Look up recipe in RecipeRegistry
  - Check if agent inventory has required ingredients
  - Place ingredients in agent's internal crafting grid (2×2)
  - Match recipe (reuse A7-UI crafting logic, if available, or implement simplified version)
  - Craft item, consume ingredients, add output to inventory
- **Failure Conditions**:
  - Invalid recipe ID
  - Missing ingredients in inventory
  - Inventory full (cannot add output)

**WAIT Command**:
- **Parameters**: `float duration`
- **Behavior**:
  - Idle for `duration` seconds
  - No movement, no actions
- **Failure Conditions**: None (always completes after duration)

**Acceptance Criteria**:
- All 5 commands work as specified
- Failure conditions handled gracefully (log error, return failure status)
- Commands integrate with existing A7-Core systems (mining, placement, inventory)

---

### FR-3: Agent Vision System

#### FR-3.1: AgentVision Class
**Description**: Agents can query nearby blocks and entities.

**Requirements**:
- **Vision Range**: Default 10 blocks (configurable per agent)
- **Block Vision**:
  ```cpp
  struct BlockInfo
  {
      IntVec3 coords;        // World block coordinates
      uint8_t blockType;     // Block type index
      float distance;        // Distance from agent
  };

  std::vector<BlockInfo> GetNearbyBlocks(float radius);
  ```
  - Returns all blocks within sphere around agent
  - Sorted by distance (nearest first)
  - Includes block type, coordinates, distance
- **Entity Vision**:
  ```cpp
  struct EntityInfo
  {
      uint64_t entityID;     // Entity unique ID
      eEntityType type;      // PLAYER, AI_AGENT, ITEM_ENTITY
      Vec3 position;         // World position
      float distance;        // Distance from agent
  };

  std::vector<EntityInfo> GetNearbyEntities(float radius);
  ```
  - Returns all entities within sphere around agent
  - Sorted by distance (nearest first)
  - Includes entity type, position, distance
- **Vision Caching**:
  - Update vision data every 0.5 seconds (not every frame)
  - Cache results in `m_cachedBlocks`, `m_cachedEntities`
  - Improves performance with many agents

**Acceptance Criteria**:
```cpp
std::vector<BlockInfo> blocks = agent->GetNearbyBlocks(10.0f);
// Returns all blocks within 10-block sphere
// Example: [{coords: (100,100,70), type: STONE, distance: 2.3}, ...]

std::vector<EntityInfo> entities = agent->GetNearbyEntities(10.0f);
// Returns all entities within 10-block sphere
// Example: [{id: 42, type: PLAYER, pos: (98,102,71), distance: 3.1}, ...]
```

#### FR-3.2: Vision Data Exposure to KADI
**Description**: Expose agent vision to KADI broker via WebSocket.

**Requirements**:
- **JSON Serialization**:
  - BlockInfo → JSON: `{"coords": [100,100,70], "blockType": "stone", "distance": 2.3}`
  - EntityInfo → JSON: `{"id": 42, "type": "player", "position": [98,102,71], "distance": 3.1}`
- **KADI Tool**: `simpleminer_get_agent_vision(agentID)`
  - Returns vision data as JSON
  - Includes both blocks and entities
  - Updated via vision cache (0.5s intervals)

**Acceptance Criteria**:
- KADI tool returns vision data in JSON format
- Vision data updates periodically (not stale)
- Multiple agents have separate vision data

---

### FR-4: KADI WebSocket Integration (KADIWebSocketSubsystem)

#### FR-4.1: KADIWebSocketSubsystem Integration
**Description**: Expose SimpleMiner game state to KADI broker via WebSocket.

**Requirements**:
- **Use Existing KADIWebSocketSubsystem**: `C:\p4\Personal\SD\Engine\Code\Engine\Network\KADIWebSocketSubsystem.hpp`
- **Reference ProtogameJS3D**: `C:\p4\Personal\SD\ProtogameJS3D` (implementation example)
- **WebSocket Server**:
  - Run on localhost:port (KADI broker connects)
  - Non-blocking communication (don't freeze game)
  - JSON-RPC 2.0 protocol
- **Tool Registration**:
  - Register 7 MCP tools with KADI broker on startup
  - Tools use lowercase_with_underscores naming (per KADI framework convention)
  - Each tool has name, description, parameters, return type

**MCP Tools** (lowercase_with_underscores):
1. `simpleminer_spawn_agent(name: string, x: float, y: float, z: float) → agentID: uint64`
2. `simpleminer_queue_command(agentID: uint64, command_type: string, params: object)`
3. `simpleminer_get_nearby_blocks(agentID: uint64, radius: float) → {blocks: BlockInfo[]}`
4. `simpleminer_get_agent_inventory(agentID: uint64) → {slots: ItemStack[]}`
5. `simpleminer_get_agent_status(agentID: uint64) → {position, current_command, queue_size}`
6. `simpleminer_list_agents() → {agents: AgentInfo[]}`
7. `simpleminer_despawn_agent(agentID: uint64)`

**Protocol Example**:
```json
{
  "jsonrpc": "2.0",
  "method": "tools/call",
  "params": {
    "name": "simpleminer_spawn_agent",
    "arguments": {
      "name": "MinerBot",
      "x": 100,
      "y": 100,
      "z": 70
    }
  },
  "id": 1
}

// Response:
{
  "jsonrpc": "2.0",
  "result": {
    "agentID": 1234567890
  },
  "id": 1
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

#### FR-4.2: Tool Implementation Details
**Description**: Detailed behavior for each KADI MCP tool.

**simpleminer_spawn_agent**:
- Create new Agent entity at specified coordinates
- Assign unique agentID
- Add to world entity list
- Return agentID in JSON response

**simpleminer_queue_command** (with command_type="MOVE"):
- Find agent by agentID
- Create MOVE command with parameters
- Add to agent command queue
- Return success/failure status

**simpleminer_queue_command** (with command_type="MINE"):
- Find agent by agentID
- Create MINE command with block coordinates
- Add to agent command queue
- Return success/failure status

**simpleminer_queue_command** (with command_type="PLACE"):
- Find agent by agentID
- Resolve blockType string to BlockRegistry ID
- Create PLACE command with parameters
- Add to agent command queue
- Return success/failure status

**simpleminer_get_nearby_blocks**:
- Find agent by agentID
- Retrieve cached vision data (blocks, entities)
- Serialize to JSON
- Return vision data

**simpleminer_get_agent_inventory**:
- Find agent by agentID
- Retrieve agent inventory (36 slots)
- Serialize to JSON (include itemID, quantity, durability)
- Return inventory data

**simpleminer_queue_command** (with command_type="CRAFT"):
- Find agent by agentID
- Create CRAFT command with recipeID
- Add to agent command queue
- Return success/failure status

**Error Handling**:
- Invalid agentID → Return error: "Agent not found"
- Invalid blockType → Return error: "Unknown block type"
- Invalid recipeID → Return error: "Unknown recipe"
- Agent out of reach → Return error: "Target out of reach"

**Acceptance Criteria**:
- All 7 tools work as specified
- Error conditions return clear error messages
- Tools integrate with agent command system
- JSON serialization works correctly

---

### FR-5: TypeScript Agent Framework

#### FR-5.1: MinerAgent Example
**Description**: Create example agent behavior in Agent_TypeScript project (deployed on DigitalOcean).

**Requirements**:
- **MinerAgent.ts**:
  - Spawn at specified coordinates
  - Call `simpleminer_get_nearby_blocks` to scan for stone blocks
  - Find nearest stone block within vision range
  - Call `simpleminer_queue_command` with MOVE command to navigate toward stone
  - Call `simpleminer_queue_command` with MINE command to mine stone
  - Wait for mining completion (poll or callback)
  - Call `simpleminer_get_agent_inventory` to verify item collected
  - Return to start position (call `simpleminer_queue_command` with MOVE command back)
  - Report completion to Claude Desktop
- **Agent Utilities**:
  - `pathfinding.ts`: Simple greedy pathfinding (move toward target, avoid obstacles)
  - `inventory.ts`: Query inventory, find item by type
  - `vision.ts`: Query vision, find block by type, sort by distance
- **Deployed on DigitalOcean** (user already has infrastructure)

**Acceptance Criteria**:
```typescript
// From Claude Desktop:
"Spawn a miner agent at (100, 100, 70) and mine 10 stone blocks"

// MinerAgent.ts executes autonomously:
const agentID = await SimpleMiner_Spawn_Agent("MinerBot", 100, 100, 70);
for (let i = 0; i < 10; i++) {
    const vision = await SimpleMiner_Get_Agent_Vision(agentID);
    const stoneBlock = vision.blocks.find(b => b.blockType === "stone");
    await SimpleMiner_Move_Agent(agentID, ...calculateDirection(stoneBlock));
    await SimpleMiner_Mine_Block(agentID, stoneBlock.coords);
}
// Claude receives: "MinerBot mined 10 stone blocks successfully"
```

#### FR-5.2: BuilderAgent Example
**Description**: Create building agent behavior in Agent_TypeScript project.

**Requirements**:
- **BuilderAgent.ts**:
  - Given list of block coordinates and types (e.g., "build a wall")
  - For each block in list:
    - Call `SimpleMiner_Get_Agent_Inventory` to check for block item
    - If missing, report error (or mine resources first, stretch goal)
    - Call `SimpleMiner_Move_Agent` to navigate near placement position
    - Call `SimpleMiner_Place_Block` to place block
    - Wait for placement completion
  - Report completion when all blocks placed

**Acceptance Criteria**:
```typescript
// From Claude Desktop:
"Spawn a builder agent and build a 5×5 platform at (110, 110, 70)"

// BuilderAgent.ts executes autonomously:
const agentID = await SimpleMiner_Spawn_Agent("BuilderBot", 110, 110, 70);
const platform = generatePlatformBlocks(5, 5, 110, 110, 70, "stone");
for (const block of platform) {
    await SimpleMiner_Move_Agent(agentID, ...calculateDirection(block));
    await SimpleMiner_Place_Block(agentID, block.type, ...block.coords);
}
// Claude receives: "BuilderBot built 25-block platform successfully"
```

---

## Non-Functional Requirements

### NFR-1: Performance
- **Target FPS**: 60 FPS sustained with 3-10 active agents
- **Agent Update**: < 0.5ms per agent per frame (total < 5ms for 10 agents)
- **Vision Caching**: Update every 0.5s (reduce CPU load)
- **WebSocket**: Non-blocking (don't freeze game on KADI calls)

### NFR-2: Memory
- **AIAgent**: < 1KB per agent (Inventory = 216 bytes, queue = variable)
- **Vision Cache**: < 10KB per agent (100 blocks × 20 bytes + 10 entities × 40 bytes)

### NFR-3: Usability (Minecraft Standard)
- **Agent Rendering**: Distinct color, name tag visible (debug friendly)
- **Command Feedback**: Clear error messages for failed commands
- **Claude Integration**: Tools discoverable via `/mcp`, well-documented

### NFR-4: Extensibility
- **Command System**: Easy to add new commands (extend enum, implement Execute/IsComplete)
- **Vision System**: Easy to extend with new queries (e.g., GetBlocksOfType)
- **KADI Tools**: Easy to add new MCP tools (register with KADIWebSocketSubsystem)

### NFR-5: Code Quality
- **SOLID Principles**: AIAgent has single responsibility (command execution)
- **DRY**: Reuse player mining/placement logic (no duplication)
- **KISS**: Simple command queue (no complex behavior trees for A7)
- **YAGNI**: Command execution only (defer goal-based AI to future)

---

## Constraints and Assumptions

### Technical Constraints
1. **Windows Only**: No cross-platform support required
2. **Single-threaded Gameplay**: Agent updates on main thread
3. **KADI Broker**: Must connect to existing `C:\p4\Personal\SD\kadi\kadi-broker` instance
4. **KADIWebSocketSubsystem**: Must use existing Engine module

### Assumptions
1. **A7-Core Complete**: Inventory, mining, placement systems functional
2. **KADI Broker Running**: Assumes `kadi-broker` started separately (not embedded)
3. **Claude Desktop Available**: User has Claude Desktop or Claude Code with KADI MCP server configured
4. **DigitalOcean Deployment**: Agent_TypeScript already deployed and accessible

---

## Success Criteria

### Minimum Viable Product (MVP)
**These features MUST work for A7-AI completion**:

1. ✅ AIAgent entity spawns at coordinates (3 agents minimum)
2. ✅ Agent command system works (MOVE, MINE, PLACE, WAIT)
3. ✅ Agent vision system returns nearby blocks and entities
4. ✅ KADI WebSocket integration (7 MCP tools registered)
5. ✅ Claude Desktop can discover and call tools
6. ✅ Agents execute commands autonomously (command queue processing)
7. ✅ TypeScript MinerAgent example works (mine 10 stone blocks)

**Demo Scenario**:
```
1. Start SimpleMiner, connect to kadi-broker
2. From Claude Desktop: "/mcp" (discover SimpleMiner tools)
3. From Claude Desktop: "Spawn a miner agent at (100, 100, 70)"
4. Agent appears in game (blue cube, name tag "MinerBot")
5. From Claude Desktop: "Mine 5 stone blocks"
6. Agent scans for stone, moves toward it, mines block
7. Agent inventory fills with stone items (visible if agent had UI)
8. Claude Desktop receives: "Mined 5 stone blocks successfully"
9. From Claude Desktop: "Spawn builder agent and place 10 stone blocks in a line"
10. BuilderAgent spawns, places blocks sequentially
```

### Stretch Goals (Nice-to-Have)
**These features are bonus, not required**:

- ⭐ Pathfinding (A* algorithm for MOVE command)
- ⭐ 10 concurrent agents working together
- ⭐ Agent-agent cooperation (shared task queue)
- ⭐ Complex TypeScript behaviors (resource gathering, base building)
- ⭐ Agent debug visualization (command queue, vision sphere, velocity vectors)
- ⭐ CRAFT command fully implemented (may defer if too complex)

---

## Dependencies

**A7-Core Systems** (REQUIRED):
1. **Registry<T>**: Block and Item registries for tool parameter resolution
2. **ItemStack**: Data structure for inventory representation
3. **Inventory**: 36-slot inventory system (agents have same inventory as player)
4. **Mining Mechanics**: Reuse for MINE command
5. **Placement Mechanics**: Reuse for PLACE command
6. **Entity System**: Extend for AIAgent physics

**Engine Systems**:
1. **KADIWebSocketSubsystem**: WebSocket communication with kadi-broker
2. **JSON Library**: Serialize/deserialize KADI messages (nlohmann/json)
3. **Entity**: Base class for physics-enabled objects

**External Dependencies**:
1. **KADI Broker**: Must connect to `C:\p4\Personal\SD\kadi\kadi-broker`
2. **Agent_TypeScript**: Must work with deployed agents on DigitalOcean
3. **ProtogameJS3D**: Reference for implementation patterns

**A7 Spec Sequence**:
- **A7-Core** (complete) → **A7-UI** (optional, not dependency) → **A7-AI** (this spec)
- A7-AI depends on A7-Core (inventory, mining, placement)
- A7-AI does NOT depend on A7-UI (agents use programmatic API, not UI)

---

## References

### KADI Integration
- **KADI Broker**: `C:\p4\Personal\SD\kadi\kadi-broker`
- **ProtogameJS3D**: `C:\p4\Personal\SD\ProtogameJS3D` (reference implementation)
- **Agent_TypeScript**: `C:\p4\Personal\SD\Agent_TypeScript` (DigitalOcean deployment)

### SimpleMiner Codebase
- **Engine**: `C:\p4\Personal\SD\Engine\Code\Engine\`
  - `Network\KADIWebSocketSubsystem.hpp` - KADI integration
  - `ThirdParty\json\json.hpp` - JSON library
- **SimpleMiner**: `C:\p4\Personal\SD\SimpleMiner\Code\Game\`
  - `Gameplay\Player.cpp` (mining/placement logic to reuse)
  - `Gameplay\Entity.hpp` (base class for AIAgent)

---

**Next Steps**:
1. Review and approve this requirements.md via spec-workflow
2. Create design.md for A7-AI with detailed agent architecture
3. After design approval, create tasks.md and begin implementation
4. After A7-AI complete, Assignment 7 is COMPLETE (all 3 specs done)
