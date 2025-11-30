# Assignment 7-AI: Design Document
# AI Agent Integration (KADI WebSocket, Agent Framework)

**Version:** 2.0 (REVISED)
**Date:** 2025-11-29
**Status:** Design Phase - Revision 1
**Dependencies:** ✅ Requires A7-Core completion (Registry, Inventory systems)

---

## Revision History

**v2.0 (2025-11-29)** - Architecture revision based on ProtogameJS3D framework:

**v2.1 (2025-11-29)** - Ed25519 key management revision:
- ✅ Changed from file-based key storage to runtime key generation
- ✅ Updated InitializeKADI() to use KADIAuthenticationUtility::GenerateKeyPair()
- ✅ Removed kadi_public.key and kadi_private.key from file checklist
- ✅ Aligned with ProtogameJS3D pattern: keys generated on startup, not loaded from disk
- ✅ Renamed AIAgent → Agent throughout entire document
- ✅ Changed MCP tool naming: `SimpleMiner_Spawn_Agent` → `simpleminer_spawn_agent` (lowercase_with_underscores)
- ✅ Revised KADI integration to match Engine KADIWebSocketSubsystem API
- ✅ Added 7-state connection flow (DISCONNECTED → READY)
- ✅ Updated agent architecture to follow broker-centralized pattern
- ✅ Added Ed25519 authentication details
- ✅ Aligned TypeScript agent examples with template-agent-typescript structure

**v1.0 (2025-11-27)** - Initial design

---

## Executive Summary

This design document provides the technical architecture for A7-AI's implementation of AI agent framework and KADI WebSocket integration. The design covers:

1. **Agent Entity Class** - Autonomous entity with inventory and command queue (extends Entity)
2. **Agent Command System** - MOVE, MINE, PLACE, CRAFT, WAIT commands
3. **Agent Vision System** - Query nearby blocks and entities
4. **KADI WebSocket Integration** - 7 MCP tools using Engine's KADIWebSocketSubsystem
5. **TypeScript Agent Framework** - Example agent behaviors (MinerAgent, BuilderAgent)

**Design Philosophy:**
- **Command Execution Only** - No goal-based AI (defer to future assignment)
- **Reuse Player Logic** - Share mining/placement code with Player class
- **Non-Blocking Network** - KADI communication doesn't freeze game
- **Broker-Centralized Architecture** - No MCP spawning in SimpleMiner code
- **MCP Tool Naming** - lowercase_with_underscores (e.g., `simpleminer_spawn_agent`)

---

## Table of Contents

1. [System Architecture Overview](#1-system-architecture-overview)
2. [Agent Class Design](#2-agent-class-design)
3. [Agent Command System](#3-agent-command-system)
4. [Agent Vision System](#4-agent-vision-system)
5. [KADI WebSocket Integration](#5-kadi-websocket-integration)
6. [TypeScript Agent Framework](#6-typescript-agent-framework)
7. [Data Flow Diagrams](#7-data-flow-diagrams)
8. [Implementation Strategy](#8-implementation-strategy)

---

## 1. System Architecture Overview

### Component Architecture

```
┌─────────────────────────────────────────────┐
│      TypeScript Agents (DigitalOcean)       │
│   MinerAgent.ts, BuilderAgent.ts            │
│   Uses @kadi.build/core (KadiClient)        │
└─────────────────────────────────────────────┘
                    ↓↑ WebSocket (KADI Protocol)
┌─────────────────────────────────────────────┐
│         KADI Broker (localhost:8080)        │
│   - Tool discovery & routing                │
│   - Ed25519 authentication                  │
│   - Network isolation (global, simpleminer) │
└─────────────────────────────────────────────┘
                    ↓↑ WebSocket
┌─────────────────────────────────────────────┐
│    KADIWebSocketSubsystem (Engine)          │
│   - RegisterTools() - 7 MCP tools           │
│   - SetToolInvokeCallback()                 │
│   - SendToolResult() / SendToolError()      │
└─────────────────────────────────────────────┘
                    ↓↑
┌─────────────────────────────────────────────┐
│         SimpleMiner Game Logic              │
│   Agent, AgentCommand, AgentVision          │
│   World::OnKADIToolInvoke()                 │
└─────────────────────────────────────────────┘
```

### Entity Hierarchy

```
Entity (base class)
├── Player
├── ItemEntity
└── Agent (new) - Autonomous AI-controlled entity
```

### KADI Connection States (7-State Flow)

Based on Engine's KADIWebSocketSubsystem implementation:

```
DISCONNECTED ──Connect()──→ CONNECTING
                               ↓ (WebSocket open)
                          CONNECTED
                               ↓ (send hello)
                        AUTHENTICATING
                               ↓ (Ed25519 signature)
                        AUTHENTICATED
                               ↓ (RegisterTools())
                     REGISTERING_TOOLS
                               ↓ (tools registered)
                            READY ✅
```

**State Transitions:**
- `DISCONNECTED`: Not connected to broker
- `CONNECTING`: WebSocket connection in progress
- `CONNECTED`: WebSocket open, not authenticated
- `AUTHENTICATING`: Sending Ed25519 authentication
- `AUTHENTICATED`: Authentication complete
- `REGISTERING_TOOLS`: Sending tool registration
- `READY`: Fully operational, can receive tool calls

### Data Flow: Spawning Agent via KADI

```
Claude Desktop: "Spawn a miner agent at (100, 100, 70)"
     ↓
TypeScript Agent (MinerAgent.ts):
  const simpleminer = await client.load('simpleminer', 'broker');
  const result = await simpleminer.simpleminer_spawn_agent({
    name: "MinerBot",
    x: 100, y: 100, z: 70
  });
     ↓
KADI Broker: Route to SimpleMiner (network: simpleminer)
     ↓
KADIWebSocketSubsystem: Receive JSON-RPC request
     ↓
Game::OnKADIToolInvoke(requestId, "simpleminer_spawn_agent", {...})
     ↓
World::SpawnAgent("MinerBot", Vec3(100, 100, 70))
     ↓
Agent* agent = new Agent("MinerBot", agentID, position);
     ↓
g_kadiWebSocket->SendToolResult(requestId, {{"agent_id", agentID}});
     ↓
TypeScript Agent receives result: { agent_id: 1234567890 }
```

---

## 2. Agent Class Design

### Class Definition

**File:** `Code/Game/Gameplay/Agent.hpp`

```cpp
#pragma once
#include "Entity.hpp"
#include "Game/Framework/Inventory.hpp"
#include "Game/Framework/AgentCommand.hpp"
#include <string>
#include <queue>
#include <vector>

//-----------------------------------------------------------------------------------------------
// Agent - AI-controlled entity with inventory and command queue
//
// Extends Entity to provide:
// - 36-slot inventory (same as Player)
// - Command queue for autonomous actions (MOVE, MINE, PLACE, CRAFT, WAIT)
// - Vision system for querying nearby blocks
// - Command execution state machine
//
// Design Notes:
// - Shares mining/placement logic with Player (via World methods)
// - No goal-based AI (commands come from TypeScript agents via KADI)
// - Command queue processed one command per Update() call
// - Non-blocking: Failed commands are removed, agent continues
//
class Agent : public Entity
{
public:
    // Constructor
    Agent(std::string const& name, uint64_t agentID, Vec3 const& position);
    virtual ~Agent();

    // Entity Interface
    virtual void Update(float deltaSeconds) override;
    virtual void Render() const override;
    virtual EntityType GetEntityType() const override { return EntityType::AGENT; }

    // Agent Identity
    std::string const& GetName() const { return m_agentName; }
    uint64_t GetAgentID() const { return m_agentID; }

    // Command Queue
    void QueueCommand(AgentCommand* command);
    bool HasPendingCommands() const { return !m_commandQueue.empty(); }
    int GetCommandQueueSize() const { return static_cast<int>(m_commandQueue.size()); }
    void ClearCommandQueue(); // For emergency stops

    // Inventory Access
    Inventory& GetInventory() { return m_inventory; }
    Inventory const& GetInventory() const { return m_inventory; }

    // Vision System
    struct BlockInfo {
        IntVec3 blockCoords;  // World coordinates
        uint16_t blockID;     // BlockRegistry ID
        std::string blockName; // Human-readable name (e.g., "Stone", "Oak_Log")
    };

    std::vector<BlockInfo> GetNearbyBlocks(float radius) const;
    std::vector<Entity*> GetNearbyEntities(float radius) const;

    // Command Execution State
    bool IsExecutingCommand() const { return m_currentCommand != nullptr; }
    std::string GetCurrentCommandType() const;

private:
    // Agent Identity
    std::string m_agentName;          // Unique name (e.g., "MinerBot")
    uint64_t m_agentID;               // Unique ID from KADI

    // Inventory
    Inventory m_inventory;            // 36 slots (matches Player)

    // Command System
    std::queue<AgentCommand*> m_commandQueue;
    AgentCommand* m_currentCommand = nullptr;

    // Command Execution
    void ProcessCommandQueue(float deltaSeconds);
    void ExecuteCurrentCommand(float deltaSeconds);
    void CompleteCurrentCommand();
};
```

### Implementation Details

**File:** `Code/Game/Gameplay/Agent.cpp`

**Key Methods:**

#### Constructor
```cpp
Agent::Agent(std::string const& name, uint64_t agentID, Vec3 const& position)
    : Entity(position)
    , m_agentName(name)
    , m_agentID(agentID)
    , m_inventory(36) // 36 slots, matches Player
{
    // Agent rendering: Green wireframe cube (2x2x2 blocks tall)
    // (Reuse Entity rendering system)
}
```

#### Update()
```cpp
void Agent::Update(float deltaSeconds)
{
    Entity::Update(deltaSeconds);  // Physics, collision, etc.
    ProcessCommandQueue(deltaSeconds);
}

void Agent::ProcessCommandQueue(float deltaSeconds)
{
    // If no current command, dequeue next command
    if (m_currentCommand == nullptr && !m_commandQueue.empty()) {
        m_currentCommand = m_commandQueue.front();
        m_commandQueue.pop();
        m_currentCommand->Start(); // Initialize command state
    }

    // Execute current command
    if (m_currentCommand != nullptr) {
        ExecuteCurrentCommand(deltaSeconds);
    }
}

void Agent::ExecuteCurrentCommand(float deltaSeconds)
{
    eCommandStatus status = m_currentCommand->Execute(deltaSeconds, this);

    if (status == eCommandStatus::COMPLETED) {
        CompleteCurrentCommand();
    } else if (status == eCommandStatus::FAILED) {
        // Log failure and move to next command
        g_theConsole->PrintString(Rgba8::RED, Stringf("Agent '%s': Command failed: %s",
            m_agentName.c_str(), m_currentCommand->GetFailureReason().c_str()));
        CompleteCurrentCommand();
    }
    // Status IN_PROGRESS: Continue executing next frame
}

void Agent::CompleteCurrentCommand()
{
    delete m_currentCommand;
    m_currentCommand = nullptr;
}
```

#### Vision System
```cpp
std::vector<Agent::BlockInfo> Agent::GetNearbyBlocks(float radius) const
{
    std::vector<BlockInfo> blocks;

    // Query all blocks within radius
    int radiusBlocks = static_cast<int>(radius) + 1;
    IntVec3 centerCoords = IntVec3(m_position);

    for (int x = -radiusBlocks; x <= radiusBlocks; ++x) {
        for (int y = -radiusBlocks; y <= radiusBlocks; ++y) {
            for (int z = -radiusBlocks; z <= radiusBlocks; ++z) {
                IntVec3 coords = centerCoords + IntVec3(x, y, z);

                // Check distance
                Vec3 blockCenter = Vec3(coords) + Vec3(0.5f, 0.5f, 0.5f);
                float dist = GetDistance3D(m_position, blockCenter);
                if (dist > radius) continue;

                // Get block from World
                Block block = m_world->GetBlock(coords);
                if (block.IsAir()) continue;

                BlockInfo info;
                info.blockCoords = coords;
                info.blockID = block.GetBlockID();
                info.blockName = g_blockRegistry->GetBlockName(block.GetBlockID());
                blocks.push_back(info);
            }
        }
    }

    return blocks;
}

std::vector<Entity*> Agent::GetNearbyEntities(float radius) const
{
    std::vector<Entity*> entities;

    for (Entity* entity : m_world->GetAllEntities()) {
        if (entity == this) continue; // Skip self

        float dist = GetDistance3D(m_position, entity->GetPosition());
        if (dist <= radius) {
            entities.push_back(entity);
        }
    }

    return entities;
}
```

---

## 3. Agent Command System

### Command Base Class

**File:** `Code/Game/Framework/AgentCommand.hpp`

```cpp
#pragma once
#include "Engine/Core/Vec3.hpp"
#include <string>

// Forward declarations
class Agent;
class World;

//-----------------------------------------------------------------------------------------------
// Command Status
enum class eCommandStatus : uint8_t
{
    NOT_STARTED,  // Command not started yet
    IN_PROGRESS,  // Command actively executing
    COMPLETED,    // Command successfully finished
    FAILED        // Command failed (invalid target, unreachable, etc.)
};

//-----------------------------------------------------------------------------------------------
// AgentCommand - Abstract base class for agent actions
//
// All commands implement:
// - Start() - Initialize command state
// - Execute() - Perform one frame of work, return status
// - GetType() - Return command type as string
// - GetFailureReason() - Return reason if status == FAILED
//
class AgentCommand
{
public:
    virtual ~AgentCommand() = default;

    // Command lifecycle
    virtual void Start() = 0;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) = 0;

    // Command metadata
    virtual std::string GetType() const = 0;
    std::string GetFailureReason() const { return m_failureReason; }

protected:
    eCommandStatus m_status = eCommandStatus::NOT_STARTED;
    std::string m_failureReason;
};
```

### Command Implementations

#### 1. MoveCommand

```cpp
class MoveCommand : public AgentCommand
{
public:
    MoveCommand(Vec3 const& targetPosition, float moveSpeed = 4.0f);

    virtual void Start() override;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
    virtual std::string GetType() const override { return "MOVE"; }

private:
    Vec3 m_targetPosition;
    float m_moveSpeed; // Blocks per second (Player uses 4.0f)

    static constexpr float ARRIVAL_THRESHOLD = 0.5f; // Within 0.5 blocks
};

// Implementation
eCommandStatus MoveCommand::Execute(float deltaSeconds, Agent* agent)
{
    Vec3 currentPos = agent->GetPosition();
    Vec3 toTarget = m_targetPosition - currentPos;
    float distanceRemaining = toTarget.GetLength();

    // Check arrival
    if (distanceRemaining < ARRIVAL_THRESHOLD) {
        return eCommandStatus::COMPLETED;
    }

    // Move towards target
    Vec3 moveDirection = toTarget.GetNormalized();
    float moveDistance = m_moveSpeed * deltaSeconds;

    if (moveDistance > distanceRemaining) {
        moveDistance = distanceRemaining; // Don't overshoot
    }

    Vec3 newPos = currentPos + (moveDirection * moveDistance);
    agent->SetPosition(newPos);

    // Check collision (reuse Player physics)
    if (agent->IsCollidingWithWorld()) {
        m_failureReason = "Collision with terrain";
        return eCommandStatus::FAILED;
    }

    return eCommandStatus::IN_PROGRESS;
}
```

#### 2. MineCommand

```cpp
class MineCommand : public AgentCommand
{
public:
    MineCommand(IntVec3 const& blockCoords);

    virtual void Start() override;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
    virtual std::string GetType() const override { return "MINE"; }

private:
    IntVec3 m_blockCoords;
    float m_miningProgress = 0.0f;  // 0.0 to 1.0
    float m_miningDuration = 0.0f;  // Seconds (from block definition)

    static constexpr float MAX_MINING_DISTANCE = 5.0f; // Same as Player
};

// Implementation
void MineCommand::Start()
{
    m_status = eCommandStatus::IN_PROGRESS;
    m_miningProgress = 0.0f;
}

eCommandStatus MineCommand::Execute(float deltaSeconds, Agent* agent)
{
    // Check distance
    Vec3 blockCenter = Vec3(m_blockCoords) + Vec3(0.5f, 0.5f, 0.5f);
    float distance = GetDistance3D(agent->GetPosition(), blockCenter);
    if (distance > MAX_MINING_DISTANCE) {
        m_failureReason = "Block out of range";
        return eCommandStatus::FAILED;
    }

    // Get block
    World* world = agent->GetWorld();
    Block block = world->GetBlock(m_blockCoords);
    if (block.IsAir()) {
        m_failureReason = "Block already mined";
        return eCommandStatus::COMPLETED; // Treat as success
    }

    // Calculate mining duration (if not calculated yet)
    if (m_miningDuration == 0.0f) {
        BlockDefinition const& blockDef = g_blockRegistry->GetBlockDefinition(block.GetBlockID());
        m_miningDuration = blockDef.GetMiningDuration();
    }

    // Update progress
    m_miningProgress += deltaSeconds / m_miningDuration;

    if (m_miningProgress >= 1.0f) {
        // Break block and spawn ItemEntity
        world->BreakBlock(m_blockCoords, agent->GetPosition());
        return eCommandStatus::COMPLETED;
    }

    return eCommandStatus::IN_PROGRESS;
}
```

#### 3. PlaceCommand

```cpp
class PlaceCommand : public AgentCommand
{
public:
    PlaceCommand(IntVec3 const& blockCoords, uint16_t itemID);

    virtual void Start() override;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
    virtual std::string GetType() const override { return "PLACE"; }

private:
    IntVec3 m_blockCoords;
    uint16_t m_itemID; // ItemRegistry ID (must be a block item)

    static constexpr float MAX_PLACEMENT_DISTANCE = 5.0f;
};

// Implementation
eCommandStatus PlaceCommand::Execute(float deltaSeconds, Agent* agent)
{
    UNUSED(deltaSeconds);

    // Check distance
    Vec3 blockCenter = Vec3(m_blockCoords) + Vec3(0.5f, 0.5f, 0.5f);
    float distance = GetDistance3D(agent->GetPosition(), blockCenter);
    if (distance > MAX_PLACEMENT_DISTANCE) {
        m_failureReason = "Block position out of range";
        return eCommandStatus::FAILED;
    }

    // Check agent has the item
    Inventory& inv = agent->GetInventory();
    int slotIndex = inv.FindItemSlot(m_itemID);
    if (slotIndex == -1) {
        m_failureReason = "Item not in inventory";
        return eCommandStatus::FAILED;
    }

    // Get corresponding block ID from ItemRegistry
    ItemDefinition const& itemDef = g_itemRegistry->GetItemDefinition(m_itemID);
    if (itemDef.GetBlockID() == 0) {
        m_failureReason = "Item is not a placeable block";
        return eCommandStatus::FAILED;
    }

    // Attempt placement (reuse Player placement logic)
    World* world = agent->GetWorld();
    bool placed = world->PlaceBlock(m_blockCoords, itemDef.GetBlockID());

    if (placed) {
        // Remove item from inventory
        inv.RemoveItemFromSlot(slotIndex, 1);
        return eCommandStatus::COMPLETED;
    } else {
        m_failureReason = "Block placement failed (occupied or invalid)";
        return eCommandStatus::FAILED;
    }
}
```

#### 4. CraftCommand

```cpp
class CraftCommand : public AgentCommand
{
public:
    CraftCommand(uint16_t recipeID);

    virtual void Start() override;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
    virtual std::string GetType() const override { return "CRAFT"; }

private:
    uint16_t m_recipeID; // RecipeRegistry ID
};

// Implementation (instant crafting, no duration)
eCommandStatus CraftCommand::Execute(float deltaSeconds, Agent* agent)
{
    UNUSED(deltaSeconds);

    // Get recipe
    RecipeDefinition const& recipe = g_recipeRegistry->GetRecipeDefinition(m_recipeID);

    // Check ingredients
    Inventory& inv = agent->GetInventory();
    if (!inv.HasIngredients(recipe.GetIngredients())) {
        m_failureReason = "Missing ingredients";
        return eCommandStatus::FAILED;
    }

    // Remove ingredients
    inv.RemoveIngredients(recipe.GetIngredients());

    // Add result
    ItemStack result = recipe.GetResult();
    bool added = inv.AddItem(result);

    if (!added) {
        m_failureReason = "Inventory full (cannot add result)";
        // TODO: Should we return ingredients? (Edge case handling)
        return eCommandStatus::FAILED;
    }

    return eCommandStatus::COMPLETED;
}
```

#### 5. WaitCommand

```cpp
class WaitCommand : public AgentCommand
{
public:
    WaitCommand(float duration); // Duration in seconds

    virtual void Start() override;
    virtual eCommandStatus Execute(float deltaSeconds, Agent* agent) override;
    virtual std::string GetType() const override { return "WAIT"; }

private:
    float m_duration;
    float m_elapsedTime = 0.0f;
};

// Implementation
void WaitCommand::Start()
{
    m_status = eCommandStatus::IN_PROGRESS;
    m_elapsedTime = 0.0f;
}

eCommandStatus WaitCommand::Execute(float deltaSeconds, Agent* agent)
{
    UNUSED(agent);

    m_elapsedTime += deltaSeconds;

    if (m_elapsedTime >= m_duration) {
        return eCommandStatus::COMPLETED;
    }

    return eCommandStatus::IN_PROGRESS;
}
```

---

## 4. Agent Vision System

### Vision System Design

The vision system allows agents to query their environment without direct access to World's internal data structures.

**Key Methods:**

```cpp
// In Agent class
std::vector<BlockInfo> GetNearbyBlocks(float radius) const;
std::vector<Entity*> GetNearbyEntities(float radius) const;
```

**BlockInfo Structure:**

```cpp
struct BlockInfo {
    IntVec3 blockCoords;    // World coordinates
    uint16_t blockID;       // BlockRegistry ID
    std::string blockName;  // Human-readable name (e.g., "Stone", "Diamond_Ore")
};
```

**Use Case Example:**

```typescript
// TypeScript Agent: Find nearest diamond ore
const visionResult = await simpleminer.simpleminer_get_nearby_blocks({
  agent_id: agentID,
  radius: 10.0
});

const diamondOre = visionResult.blocks.find(b => b.block_name === "Diamond_Ore");
if (diamondOre) {
  // Queue MOVE command to diamond ore
  await simpleminer.simpleminer_queue_command({
    agent_id: agentID,
    command_type: "MOVE",
    params: diamondOre.block_coords
  });

  // Queue MINE command
  await simpleminer.simpleminer_queue_command({
    agent_id: agentID,
    command_type: "MINE",
    params: { block_coords: diamondOre.block_coords }
  });
}
```

---

## 5. KADI WebSocket Integration

### KADIWebSocketSubsystem API

Based on Engine's `KADIWebSocketSubsystem` (see `Engine\Code\Engine\Network\CLAUDE.md`):

**Key Methods:**

```cpp
// Lifecycle
void Startup();
void Shutdown();
void BeginFrame();
void EndFrame();

// Connection Management
void Connect(std::string const& brokerUrl, std::string const& publicKey, std::string const& privateKey);
void Disconnect();
bool IsConnected() const;
eKADIConnectionState GetConnectionState() const;

// Tool Registration (MCP Tools)
void RegisterTools(nlohmann::json const& tools);
void SendToolResult(int requestId, nlohmann::json const& result);
void SendToolError(int requestId, std::string const& errorMessage);

// Event System
void SubscribeToEvents(std::vector<std::string> const& channels);
void PublishEvent(std::string const& channel, nlohmann::json const& data);

// Callback Registration
void SetToolInvokeCallback(KADIToolInvokeCallback callback);
void SetEventDeliveryCallback(KADIEventDeliveryCallback callback);
void SetConnectionStateCallback(KADIConnectionStateCallback callback);
```

**Callback Types:**

```cpp
using KADIToolInvokeCallback = std::function<void(int requestId, std::string const& toolName, nlohmann::json const& arguments)>;
using KADIEventDeliveryCallback = std::function<void(std::string const& channel, nlohmann::json const& data)>;
using KADIConnectionStateCallback = std::function<void(eKADIConnectionState oldState, eKADIConnectionState newState)>;
```

### MCP Tool Registration

**File:** `Code/Game/Gameplay/Game.cpp` (Startup)

**Tool Naming Convention:** `lowercase_with_underscores` (e.g., `simpleminer_spawn_agent`)

```cpp
void Game::InitializeKADI()
{
    // Get KADIWebSocketSubsystem from Engine
    g_kadiWebSocket = g_theEngine->GetKADIWebSocketSubsystem();
    if (!g_kadiWebSocket) {
        g_theConsole->PrintString(Rgba8::RED, "KADI WebSocket subsystem not available");
        return;
    }

    // Set tool invoke callback
    g_kadiWebSocket->SetToolInvokeCallback([this](int requestId, std::string const& toolName, nlohmann::json const& arguments) {
        this->OnKADIToolInvoke(requestId, toolName, arguments);
    });

    // Register 7 MCP tools
    nlohmann::json tools = nlohmann::json::array();

    // Tool 1: simpleminer_spawn_agent
    tools.push_back({
        {"name", "simpleminer_spawn_agent"},
        {"description", "Spawn a new AI agent at specified position"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"name", {{"type", "string"}, {"description", "Agent name (e.g., 'MinerBot')"}}},
                {"x", {{"type", "number"}, {"description", "X world coordinate"}}},
                {"y", {{"type", "number"}, {"description", "Y world coordinate"}}},
                {"z", {{"type", "number"}, {"description", "Z world coordinate"}}}
            }},
            {"required", nlohmann::json::array({"name", "x", "y", "z"})}
        }}
    });

    // Tool 2: simpleminer_queue_command
    tools.push_back({
        {"name", "simpleminer_queue_command"},
        {"description", "Queue a command for an agent (MOVE, MINE, PLACE, CRAFT, WAIT)"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"agent_id", {{"type", "number"}, {"description", "Agent ID"}}},
                {"command_type", {{"type", "string"}, {"enum", nlohmann::json::array({"MOVE", "MINE", "PLACE", "CRAFT", "WAIT"})}}},
                {"params", {{"type", "object"}, {"description", "Command-specific parameters"}}}
            }},
            {"required", nlohmann::json::array({"agent_id", "command_type", "params"})}
        }}
    });

    // Tool 3: simpleminer_get_nearby_blocks
    tools.push_back({
        {"name", "simpleminer_get_nearby_blocks"},
        {"description", "Query blocks near an agent"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"agent_id", {{"type", "number"}}},
                {"radius", {{"type", "number"}, {"description", "Search radius in blocks"}}}
            }},
            {"required", nlohmann::json::array({"agent_id", "radius"})}
        }}
    });

    // Tool 4: simpleminer_get_agent_inventory
    tools.push_back({
        {"name", "simpleminer_get_agent_inventory"},
        {"description", "Get agent's inventory contents"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"agent_id", {{"type", "number"}}}
            }},
            {"required", nlohmann::json::array({"agent_id"})}
        }}
    });

    // Tool 5: simpleminer_get_agent_status
    tools.push_back({
        {"name", "simpleminer_get_agent_status"},
        {"description", "Get agent position, current command, queue size"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"agent_id", {{"type", "number"}}}
            }},
            {"required", nlohmann::json::array({"agent_id"})}
        }}
    });

    // Tool 6: simpleminer_list_agents
    tools.push_back({
        {"name", "simpleminer_list_agents"},
        {"description", "List all active agents in the world"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {}}
        }}
    });

    // Tool 7: simpleminer_despawn_agent
    tools.push_back({
        {"name", "simpleminer_despawn_agent"},
        {"description", "Remove an agent from the world"},
        {"inputSchema", {
            {"type", "object"},
            {"properties", {
                {"agent_id", {{"type", "number"}}}
            }},
            {"required", nlohmann::json::array({"agent_id"})}
        }}
    });

    // Register tools with broker
    g_kadiWebSocket->RegisterTools(tools);

    // Generate Ed25519 key pair at runtime (following ProtogameJS3D pattern)
    sEd25519KeyPair keyPair;
    if (!KADIAuthenticationUtility::GenerateKeyPair(keyPair)) {
        g_theConsole->PrintString(Rgba8::RED, "KADI: Failed to generate Ed25519 key pair");
        return;
    }

    // Convert keys to base64 strings for broker authentication
    std::string publicKey = keyPair.GetPublicKeyBase64();
    std::string privateKey = keyPair.GetPrivateKeyBase64();

    g_theConsole->PrintString(Rgba8::CYAN, "KADI: Ed25519 key pair generated successfully");

    // Connect to KADI broker
    std::string brokerUrl = "ws://localhost:8080/ws";
    g_kadiWebSocket->Connect(brokerUrl, publicKey, privateKey);

    g_theConsole->PrintString(Rgba8::GREEN, "KADI WebSocket initialized, connecting to broker...");
}
```

### Tool Invocation Handler

**File:** `Code/Game/Gameplay/Game.cpp`

```cpp
void Game::OnKADIToolInvoke(int requestId, std::string const& toolName, nlohmann::json const& arguments)
{
    // Dispatch to appropriate handler
    if (toolName == "simpleminer_spawn_agent") {
        HandleSpawnAgent(requestId, arguments);
    }
    else if (toolName == "simpleminer_queue_command") {
        HandleQueueCommand(requestId, arguments);
    }
    else if (toolName == "simpleminer_get_nearby_blocks") {
        HandleGetNearbyBlocks(requestId, arguments);
    }
    else if (toolName == "simpleminer_get_agent_inventory") {
        HandleGetAgentInventory(requestId, arguments);
    }
    else if (toolName == "simpleminer_get_agent_status") {
        HandleGetAgentStatus(requestId, arguments);
    }
    else if (toolName == "simpleminer_list_agents") {
        HandleListAgents(requestId, arguments);
    }
    else if (toolName == "simpleminer_despawn_agent") {
        HandleDespawnAgent(requestId, arguments);
    }
    else {
        // Unknown tool
        g_kadiWebSocket->SendToolError(requestId, "Unknown tool: " + toolName);
    }
}

void Game::HandleSpawnAgent(int requestId, nlohmann::json const& arguments)
{
    try {
        // Parse arguments
        std::string name = arguments["name"];
        float x = arguments["x"];
        float y = arguments["y"];
        float z = arguments["z"];

        // Spawn agent
        uint64_t agentID = m_world->SpawnAgent(name, Vec3(x, y, z));

        // Send result
        nlohmann::json result = {
            {"agent_id", agentID},
            {"position", {{"x", x}, {"y", y}, {"z", z}}},
            {"name", name}
        };
        g_kadiWebSocket->SendToolResult(requestId, result);
    }
    catch (std::exception const& e) {
        g_kadiWebSocket->SendToolError(requestId, std::string("Failed to spawn agent: ") + e.what());
    }
}

void Game::HandleQueueCommand(int requestId, nlohmann::json const& arguments)
{
    try {
        uint64_t agentID = arguments["agent_id"];
        std::string commandType = arguments["command_type"];
        nlohmann::json params = arguments["params"];

        // Find agent
        Agent* agent = m_world->FindAgentByID(agentID);
        if (!agent) {
            g_kadiWebSocket->SendToolError(requestId, "Agent not found");
            return;
        }

        // Create command based on type
        AgentCommand* command = nullptr;

        if (commandType == "MOVE") {
            Vec3 target(params["x"], params["y"], params["z"]);
            command = new MoveCommand(target);
        }
        else if (commandType == "MINE") {
            IntVec3 coords(params["x"], params["y"], params["z"]);
            command = new MineCommand(coords);
        }
        else if (commandType == "PLACE") {
            IntVec3 coords(params["x"], params["y"], params["z"]);
            uint16_t itemID = params["item_id"];
            command = new PlaceCommand(coords, itemID);
        }
        else if (commandType == "CRAFT") {
            uint16_t recipeID = params["recipe_id"];
            command = new CraftCommand(recipeID);
        }
        else if (commandType == "WAIT") {
            float duration = params["duration"];
            command = new WaitCommand(duration);
        }
        else {
            g_kadiWebSocket->SendToolError(requestId, "Unknown command type: " + commandType);
            return;
        }

        // Queue command
        agent->QueueCommand(command);

        // Send result
        nlohmann::json result = {
            {"success", true},
            {"queue_size", agent->GetCommandQueueSize()}
        };
        g_kadiWebSocket->SendToolResult(requestId, result);
    }
    catch (std::exception const& e) {
        g_kadiWebSocket->SendToolError(requestId, std::string("Failed to queue command: ") + e.what());
    }
}

void Game::HandleGetNearbyBlocks(int requestId, nlohmann::json const& arguments)
{
    try {
        uint64_t agentID = arguments["agent_id"];
        float radius = arguments["radius"];

        Agent* agent = m_world->FindAgentByID(agentID);
        if (!agent) {
            g_kadiWebSocket->SendToolError(requestId, "Agent not found");
            return;
        }

        // Get nearby blocks
        std::vector<Agent::BlockInfo> blocks = agent->GetNearbyBlocks(radius);

        // Convert to JSON
        nlohmann::json blocksJson = nlohmann::json::array();
        for (auto const& block : blocks) {
            blocksJson.push_back({
                {"block_coords", {{"x", block.blockCoords.x}, {"y", block.blockCoords.y}, {"z", block.blockCoords.z}}},
                {"block_id", block.blockID},
                {"block_name", block.blockName}
            });
        }

        nlohmann::json result = {
            {"blocks", blocksJson},
            {"count", blocks.size()}
        };
        g_kadiWebSocket->SendToolResult(requestId, result);
    }
    catch (std::exception const& e) {
        g_kadiWebSocket->SendToolError(requestId, std::string("Failed to get nearby blocks: ") + e.what());
    }
}

// Similar implementations for HandleGetAgentInventory, HandleGetAgentStatus, HandleListAgents, HandleDespawnAgent
```

---

## 6. TypeScript Agent Framework

### Example: MinerAgent

**File:** `agents/simpleminer/MinerAgent.ts`

Based on `template-agent-typescript` structure:

```typescript
/**
 * MinerAgent - Autonomous mining agent for SimpleMiner
 *
 * Capabilities:
 * - Find nearest diamond ore using vision system
 * - Navigate to ore location
 * - Mine ore and collect drops
 * - Return to home position
 */

import 'dotenv/config';
import { KadiClient } from '@kadi.build/core';

// Configuration
const BROKER_URL = process.env.KADI_BROKER_URL || 'ws://localhost:8080';
const AGENT_NAME = 'miner-agent';
const NETWORKS = ['global', 'simpleminer'];

// Mining constants
const VISION_RADIUS = 20.0;
const HOME_POSITION = { x: 0, y: 0, z: 70 };
const MINING_TARGETS = ['Diamond_Ore', 'Iron_Ore', 'Coal_Ore'];

async function main() {
    // Create KADI client
    const client = new KadiClient({
        name: AGENT_NAME,
        broker: BROKER_URL,
        networks: NETWORKS
    });

    // Connect to broker
    await client.connect();
    console.log(`✅ Connected to KADI broker at ${BROKER_URL}`);

    // Load SimpleMiner tools
    const simpleminer = await client.load('simpleminer', 'broker');
    console.log(`✅ Loaded SimpleMiner tools`);

    // Spawn agent in game
    const spawnResult = await simpleminer.simpleminer_spawn_agent({
        name: 'MinerBot',
        x: HOME_POSITION.x,
        y: HOME_POSITION.y,
        z: HOME_POSITION.z
    });

    const agentID = spawnResult.agent_id;
    console.log(`✅ Spawned agent with ID: ${agentID}`);

    // Main mining loop
    while (true) {
        try {
            // Get nearby blocks
            const visionResult = await simpleminer.simpleminer_get_nearby_blocks({
                agent_id: agentID,
                radius: VISION_RADIUS
            });

            // Find nearest valuable ore
            const targetOre = visionResult.blocks.find((block: any) =>
                MINING_TARGETS.includes(block.block_name)
            );

            if (targetOre) {
                console.log(`🎯 Found ${targetOre.block_name} at (${targetOre.block_coords.x}, ${targetOre.block_coords.y}, ${targetOre.block_coords.z})`);

                // Move to ore
                await simpleminer.simpleminer_queue_command({
                    agent_id: agentID,
                    command_type: 'MOVE',
                    params: {
                        x: targetOre.block_coords.x + 0.5,
                        y: targetOre.block_coords.y + 0.5,
                        z: targetOre.block_coords.z
                    }
                });

                // Mine ore
                await simpleminer.simpleminer_queue_command({
                    agent_id: agentID,
                    command_type: 'MINE',
                    params: {
                        x: targetOre.block_coords.x,
                        y: targetOre.block_coords.y,
                        z: targetOre.block_coords.z
                    }
                });

                console.log(`⛏️ Queued mining commands`);
            } else {
                console.log(`🔍 No ores found nearby, waiting...`);

                // Wait 5 seconds before next scan
                await simpleminer.simpleminer_queue_command({
                    agent_id: agentID,
                    command_type: 'WAIT',
                    params: { duration: 5.0 }
                });
            }

            // Check agent status
            const status = await simpleminer.simpleminer_get_agent_status({
                agent_id: agentID
            });

            console.log(`📊 Agent status: Position (${status.position.x}, ${status.position.y}, ${status.position.z}), Queue: ${status.queue_size} commands`);

            // Wait for commands to execute
            await new Promise(resolve => setTimeout(resolve, 3000));

        } catch (error) {
            console.error(`❌ Error: ${error}`);
            await new Promise(resolve => setTimeout(resolve, 5000));
        }
    }
}

// Graceful shutdown
process.on('SIGINT', async () => {
    console.log('\n🛑 Shutting down MinerAgent...');
    process.exit(0);
});

main().catch(console.error);
```

### Example: BuilderAgent

**File:** `agents/simpleminer/BuilderAgent.ts`

```typescript
/**
 * BuilderAgent - Automated construction agent
 *
 * Capabilities:
 * - Build simple structures (walls, floors, pillars)
 * - Manage inventory for construction materials
 * - Follow blueprint commands from Claude Desktop
 */

import 'dotenv/config';
import { KadiClient } from '@kadi.build/core';

const BROKER_URL = process.env.KADI_BROKER_URL || 'ws://localhost:8080';
const AGENT_NAME = 'builder-agent';

interface BuildTask {
    type: 'wall' | 'floor' | 'pillar';
    material: string; // e.g., "Stone", "Oak_Planks"
    start: { x: number; y: number; z: number };
    end: { x: number; y: number; z: number };
}

async function buildWall(simpleminer: any, agentID: number, task: BuildTask) {
    const { start, end, material } = task;

    // Get material item ID (simplified - would query ItemRegistry in production)
    const materialID = getMaterialID(material);

    // Build wall column by column
    for (let x = start.x; x <= end.x; x++) {
        for (let z = start.z; z <= end.z; z++) {
            // Move to position
            await simpleminer.simpleminer_queue_command({
                agent_id: agentID,
                command_type: 'MOVE',
                params: { x: x + 0.5, y: start.y, z: z }
            });

            // Place block
            await simpleminer.simpleminer_queue_command({
                agent_id: agentID,
                command_type: 'PLACE',
                params: { x, y: start.y, z, item_id: materialID }
            });
        }
    }

    console.log(`✅ Wall built from (${start.x}, ${start.y}, ${start.z}) to (${end.x}, ${end.y}, ${end.z})`);
}

function getMaterialID(materialName: string): number {
    // Map material names to item IDs
    // In production, this would query SimpleMiner's ItemRegistry via MCP tool
    const materials: { [key: string]: number } = {
        'Stone': 1,
        'Dirt': 2,
        'Oak_Planks': 10,
        'Cobblestone': 15
    };
    return materials[materialName] || 1;
}

// Main function would set up similar pattern to MinerAgent
async function main() {
    const client = new KadiClient({
        name: AGENT_NAME,
        broker: BROKER_URL,
        networks: ['global', 'simpleminer']
    });

    await client.connect();
    console.log(`✅ BuilderAgent connected to KADI broker`);

    const simpleminer = await client.load('simpleminer', 'broker');

    // Spawn builder agent
    const spawnResult = await simpleminer.simpleminer_spawn_agent({
        name: 'BuilderBot',
        x: 0, y: 0, z: 70
    });

    const agentID = spawnResult.agent_id;
    console.log(`✅ Spawned BuilderBot with ID: ${agentID}`);

    // Example: Build a simple wall
    await buildWall(simpleminer, agentID, {
        type: 'wall',
        material: 'Stone',
        start: { x: 10, y: 70, z: 10 },
        end: { x: 20, y: 70, z: 10 }
    });
}

main().catch(console.error);
```

---

## 7. Data Flow Diagrams

### Spawning Agent Flow

```
┌────────────────────┐
│  Claude Desktop    │ User: "Spawn MinerBot at (100, 100, 70)"
└─────────┬──────────┘
          ↓ (Claude API processes request)
┌─────────┴──────────┐
│ TypeScript Agent   │ MinerAgent.ts calls:
│  (MinerAgent.ts)   │ simpleminer.simpleminer_spawn_agent({name, x, y, z})
└─────────┬──────────┘
          ↓ (KadiClient sends JSON-RPC request)
┌─────────┴──────────┐
│   KADI Broker      │ Routes to SimpleMiner (network: simpleminer)
└─────────┬──────────┘
          ↓ (WebSocket JSON-RPC message)
┌─────────┴──────────┐
│ KADIWebSocketSub   │ Receives request, parses JSON
│     (Engine)       │ Invokes SetToolInvokeCallback
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::OnKADI       │ OnKADIToolInvoke(requestId, "simpleminer_spawn_agent", args)
│   ToolInvoke       │ Dispatches to HandleSpawnAgent()
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│  World::           │ SpawnAgent(name, position)
│  SpawnAgent        │ Creates new Agent* entity
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│  Agent*            │ new Agent(name, agentID, position)
│  Constructor       │ Initialize inventory, command queue
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::             │ SendToolResult(requestId, {agent_id: 1234})
│ SendToolResult     │
└─────────┬──────────┘
          ↓ (WebSocket response)
┌─────────┴──────────┐
│   KADI Broker      │ Routes response to TypeScript Agent
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ TypeScript Agent   │ Receives { agent_id: 1234 }
│  (MinerAgent.ts)   │ Stores agentID for future commands
└────────────────────┘
```

### Command Execution Flow

```
┌────────────────────┐
│ TypeScript Agent   │ Calls: simpleminer.simpleminer_queue_command({
│  (MinerAgent.ts)   │   agent_id: 1234,
└─────────┬──────────┘   command_type: "MINE",
          ↓              params: {x: 105, y: 100, z: 70}
          ↓            })
┌─────────┴──────────┐
│   KADI Broker      │ Routes to SimpleMiner
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::             │ HandleQueueCommand(requestId, args)
│ OnKADIToolInvoke   │ Finds agent by ID
└─────────┬──────────┘ Creates MineCommand(IntVec3(105, 100, 70))
          ↓
┌─────────┴──────────┐
│  Agent::           │ QueueCommand(command)
│  QueueCommand      │ Adds to m_commandQueue
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::             │ SendToolResult(requestId, {success: true})
│ SendToolResult     │
└────────────────────┘

(Later, during Update loop...)

┌────────────────────┐
│  Agent::Update     │ Calls ProcessCommandQueue(deltaSeconds)
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│  Agent::           │ If no current command, dequeue next
│  ProcessCommand    │ m_currentCommand = m_commandQueue.front()
│  Queue             │ m_currentCommand->Start()
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│  MineCommand::     │ Execute(deltaSeconds, agent)
│  Execute           │ Increment m_miningProgress
└─────────┬──────────┘ If progress >= 1.0:
          ↓              - World::BreakBlock()
          ↓              - return COMPLETED
┌─────────┴──────────┐
│  Agent::           │ If status == COMPLETED:
│  ExecuteCommand    │   CompleteCurrentCommand()
└─────────┬──────────┘   delete m_currentCommand
          ↓              m_currentCommand = nullptr
┌────────────────────┐
│  Next Update()     │ Dequeue next command from queue
└────────────────────┘
```

### Vision System Flow

```
┌────────────────────┐
│ TypeScript Agent   │ Calls: simpleminer.simpleminer_get_nearby_blocks({
│  (MinerAgent.ts)   │   agent_id: 1234,
└─────────┬──────────┘   radius: 20.0
          ↓            })
┌─────────┴──────────┐
│   KADI Broker      │ Routes to SimpleMiner
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::             │ HandleGetNearbyBlocks(requestId, args)
│ OnKADIToolInvoke   │ Finds agent by ID
└─────────┬──────────┘ Calls agent->GetNearbyBlocks(radius)
          ↓
┌─────────┴──────────┐
│  Agent::           │ GetNearbyBlocks(20.0)
│  GetNearbyBlocks   │ Iterate all blocks within 20 block radius
└─────────┬──────────┘ For each block:
          ↓              - Check distance
          ↓              - Get Block from World
          ↓              - Query BlockRegistry for name
┌─────────┴──────────┐
│  Return            │ std::vector<BlockInfo>
│  BlockInfo[]       │ Each entry: {coords, blockID, blockName}
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ Game::             │ Convert to JSON array
│ Handle...Blocks    │ SendToolResult(requestId, {blocks: [...], count: 42})
└─────────┬──────────┘
          ↓
┌─────────┴──────────┐
│ TypeScript Agent   │ Receives { blocks: [{block_name: "Diamond_Ore", ...}], count: 42 }
│  (MinerAgent.ts)   │ Find nearest Diamond_Ore
└────────────────────┘ Queue MOVE and MINE commands
```

---

## 8. Implementation Strategy

### Phase 1: Agent Entity and Command System (Week 4, Days 1-2)

**Tasks:**

1. **Create Agent class** (`Agent.hpp`, `Agent.cpp`)
   - Extend Entity base class
   - Add inventory (36 slots)
   - Add command queue (std::queue<AgentCommand*>)
   - Implement Update() with command processing
   - Implement Render() (green wireframe cube)

2. **Create AgentCommand base class and implementations**
   - `AgentCommand.hpp` - Base class with virtual Execute()
   - `MoveCommand` - Navigate to position
   - `MineCommand` - Progressive block breaking
   - `PlaceCommand` - Block placement with inventory check
   - `CraftCommand` - Recipe execution
   - `WaitCommand` - Timer-based delay

3. **Add Agent vision system**
   - `GetNearbyBlocks(radius)` - Query blocks within radius
   - `GetNearbyEntities(radius)` - Query entities within radius

4. **Update World class**
   - `SpawnAgent(name, position)` → `uint64_t agentID`
   - `FindAgentByID(agentID)` → `Agent*`
   - `DespawnAgent(agentID)` - Remove from entity list
   - Track agents in `std::map<uint64_t, Agent*> m_agents`

**Testing:**
- Spawn agent via console command: `/spawn_agent MinerBot 100 100 70`
- Queue commands via console: `/queue_command 1234 MOVE 105 100 70`
- Verify command execution (agent moves to target)

---

### Phase 2: KADI WebSocket Integration (Week 4, Days 3-4)

**Tasks:**

1. **Initialize KADIWebSocketSubsystem in Game::Startup()**
   - Get subsystem from Engine: `g_theEngine->GetKADIWebSocketSubsystem()`
   - Set tool invoke callback: `SetToolInvokeCallback(...)`
   - Generate Ed25519 key pair at runtime: `KADIAuthenticationUtility::GenerateKeyPair()`
   - Connect to broker: `Connect("ws://localhost:8080/ws", publicKey, privateKey)`

2. **Register 7 MCP tools**
   - Create nlohmann::json array with tool schemas
   - Call `RegisterTools(tools)`
   - Tools:
     1. `simpleminer_spawn_agent`
     2. `simpleminer_queue_command`
     3. `simpleminer_get_nearby_blocks`
     4. `simpleminer_get_agent_inventory`
     5. `simpleminer_get_agent_status`
     6. `simpleminer_list_agents`
     7. `simpleminer_despawn_agent`

3. **Implement tool handlers**
   - `Game::OnKADIToolInvoke(requestId, toolName, arguments)`
   - Dispatch to 7 handler functions:
     - `HandleSpawnAgent()` - World::SpawnAgent(), send result
     - `HandleQueueCommand()` - Create command, queue, send result
     - `HandleGetNearbyBlocks()` - agent->GetNearbyBlocks(), convert to JSON
     - `HandleGetAgentInventory()` - Serialize inventory to JSON
     - `HandleGetAgentStatus()` - Return position, current command, queue size
     - `HandleListAgents()` - List all active agents
     - `HandleDespawnAgent()` - World::DespawnAgent(), send confirmation

4. **Handle connection states**
   - Monitor eKADIConnectionState transitions
   - Print status messages (CONNECTED, AUTHENTICATED, READY)
   - Handle disconnections gracefully (don't crash game)

**Testing:**
- Manually send JSON-RPC request via WebSocket client
- Verify tool registration appears in KADI broker dashboard
- Test spawning agent via KADI tool call
- Test queuing commands via KADI tool call

---

### Phase 3: TypeScript Agent Framework (Week 4, Days 5-7)

**Tasks:**

1. **Set up TypeScript agent project**
   - Copy `template-agent-typescript` structure
   - Create `agents/simpleminer/` directory
   - Install dependencies: `@kadi.build/core`, `dotenv`
   - Configure `.env` file with `KADI_BROKER_URL` and `KADI_NETWORK`

2. **Implement MinerAgent.ts**
   - Connect to KADI broker using KadiClient
   - Load SimpleMiner tools: `client.load('simpleminer', 'broker')`
   - Spawn agent: `simpleminer.simpleminer_spawn_agent({...})`
   - Implement mining loop:
     - Get nearby blocks
     - Find diamond ore
     - Queue MOVE command
     - Queue MINE command
     - Wait for execution
   - Add error handling and logging

3. **Implement BuilderAgent.ts**
   - Similar setup to MinerAgent
   - Implement `buildWall()` function
   - Queue PLACE commands in sequence
   - Manage inventory (check material availability)

4. **Create agent deployment script**
   - Deploy to DigitalOcean droplet (or run locally for testing)
   - Set up systemd service for auto-restart
   - Configure environment variables

5. **Test end-to-end flow**
   - Claude Desktop → TypeScript Agent → KADI Broker → SimpleMiner → Agent execution
   - Verify agent spawns, moves, mines blocks
   - Verify vision system returns nearby blocks
   - Verify inventory updates after mining

**Testing:**
- Run MinerAgent.ts locally: `npm run dev`
- Ask Claude Desktop: "Spawn a mining agent and find diamond ore"
- Verify agent appears in SimpleMiner world
- Verify agent moves to ore and mines it
- Verify TypeScript logs show successful execution

---

### Phase 4: Integration and Polish (Week 4, Day 7)

**Tasks:**

1. **Error handling improvements**
   - Handle agent not found errors gracefully
   - Handle invalid command parameters (out of range, missing items)
   - Add timeout handling for stuck commands

2. **Performance optimization**
   - Limit vision system queries (cache results for 1 second)
   - Limit command queue size (max 100 commands)
   - Throttle KADI message rate (max 10 requests/second)

3. **Documentation**
   - Write `agents/simpleminer/README.md` with setup instructions
   - Document MCP tool schemas
   - Add JSDoc comments to TypeScript agents

4. **Final testing**
   - Test all 7 MCP tools
   - Test all 5 command types
   - Test error cases (invalid agent ID, out of range, etc.)
   - Test concurrent TypeScript agents (MinerAgent + BuilderAgent)

---

### Implementation Files Checklist

**New Files:**

```
Code/Game/Gameplay/Agent.hpp                     # Agent entity class
Code/Game/Gameplay/Agent.cpp                     # Agent implementation
Code/Game/Framework/AgentCommand.hpp             # Command base class + implementations
Code/Game/Framework/AgentCommand.cpp             # Command implementations
agents/simpleminer/MinerAgent.ts                 # TypeScript mining agent
agents/simpleminer/BuilderAgent.ts               # TypeScript building agent
agents/simpleminer/package.json                  # TypeScript dependencies
agents/simpleminer/.env.template                 # Environment config template
agents/simpleminer/README.md                     # Setup documentation
```

**Modified Files:**

```
Code/Game/Gameplay/Game.hpp                      # Add KADI initialization
Code/Game/Gameplay/Game.cpp                      # Implement tool handlers
Code/Game/Gameplay/World.hpp                     # Add SpawnAgent, FindAgentByID
Code/Game/Gameplay/World.cpp                     # Agent management
Code/Game/Framework/GameCommon.hpp               # Add Agent-related constants
.gitignore                                       # Ignore kadi_private.key
```

---

### Dependencies and Prerequisites

**Prerequisites from A7-Core:**
- ✅ Registry<T> template (BlockRegistry, ItemRegistry, RecipeRegistry)
- ✅ Inventory class with 36-slot storage
- ✅ ItemStack structure (itemID, quantity, durability)
- ✅ ItemEntity with magnetic pickup
- ✅ World::BreakBlock() - Progressive mining logic
- ✅ World::PlaceBlock() - Block placement validation
- ✅ RecipeDefinition with GetIngredients() and GetResult()

**External Dependencies:**
- ✅ Engine KADIWebSocketSubsystem (already implemented in Engine)
- ✅ KADI Broker running at `ws://localhost:8080` (separate process)
- ✅ nlohmann/json library (already in Engine/Code/ThirdParty/json/)
- ⏳ @kadi.build/core npm package (for TypeScript agents)
- ⏳ template-agent-typescript repository (reference implementation)

---

## Conclusion

This revised design document provides a complete technical architecture for A7-AI's AI agent framework, aligned with the ProtogameJS3D reference implementation and Engine's KADIWebSocketSubsystem. Key revisions include:

1. ✅ **Naming Corrections:**
   - Renamed `AIAgent` → `Agent` throughout
   - Changed MCP tool naming from `SimpleMiner_Spawn_Agent` → `simpleminer_spawn_agent`

2. ✅ **Architecture Alignment:**
   - Broker-centralized pattern (no MCP spawning in game code)
   - Ed25519 authentication with public/private keys
   - 7-state connection flow (DISCONNECTED → READY)
   - Tool registration via nlohmann::json schemas

3. ✅ **TypeScript Integration:**
   - KadiClient usage pattern from template-agent-typescript
   - Zod schema validation (handled by broker)
   - Event-driven architecture support
   - Cross-language tool loading (`client.load()`)

The implementation follows SOLID principles, reuses existing Player logic for mining/placement, and provides a clean separation between game logic and AI control. The design is ready for approval and implementation in Week 4 of Assignment 7.

**Estimated Implementation Time:** 7 days (Week 4)
**Lines of Code:** ~2,000 lines (C++ Agent framework) + ~500 lines (TypeScript agents)
**Testing Time:** 2 days (integrated with A7-Core and A7-UI testing)
