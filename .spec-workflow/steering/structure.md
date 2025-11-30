# Assignment 7: Project Structure

## Directory Organization

```
SimpleMiner/
├── Code/
│   └── Game/
│       ├── Main_Windows.cpp                    # Entry point, WinMain
│       ├── Game.vcxproj                        # Visual Studio project
│       │
│       ├── Framework/                          # Core game systems (A4-A6 primary location)
│       │   ├── App.hpp/cpp                    # Game lifecycle, subsystem management
│       │   ├── Block.hpp/cpp                  # Ultra-lightweight 1-byte block (m_typeIndex only)
│       │   ├── Chunk.hpp/cpp                  # 16×16×128 block storage, mesh generation, terrain
│       │   ├── GameCommon.hpp/cpp             # Global constants, utility functions
│       │   ├── WorldGenConfig.hpp/cpp         # World generation parameter management
│       │   └── CLAUDE.md                      # Module documentation
│       │
│       ├── Gameplay/                          # Game-specific logic (A7 primary location)
│       │   ├── World.hpp/cpp                  # Chunk lifecycle, lighting, entity management
│       │   ├── Player.hpp/cpp                 # Player entity, camera, input
│       │   ├── Entity.hpp/cpp                 # Base class for dynamic objects (physics)
│       │   ├── Prop.hpp/cpp                   # Static props
│       │   ├── Game.hpp/cpp                   # High-level game state machine
│       │   │
│       │   ├── [A7 NEW] AIAgent.hpp/cpp       # AI-controlled entity with command queue
│       │   ├── [A7 NEW] ItemEntity.hpp/cpp    # Dropped items in world (physics-enabled)
│       │   ├── [A7 NEW] Inventory.hpp/cpp     # 36-slot inventory container
│       │   ├── [A7 NEW] ItemStack.hpp/cpp     # Item quantity + type + durability
│       │   └── CLAUDE.md                      # Module documentation
│       │
│       ├── Definition/                        # Data-driven configuration (A7 heavy changes)
│       │   ├── BlockDefinition.hpp/cpp        # Block properties (migrating to JSON)
│       │   │
│       │   ├── [A7 NEW] Registry.hpp          # Generic Registry<T> template
│       │   ├── [A7 NEW] ItemDefinition.hpp/cpp # Item properties (tools, resources)
│       │   ├── [A7 NEW] Recipe.hpp/cpp        # Crafting recipe definitions
│       │   ├── [A7 NEW] ToolDefinition.hpp/cpp # Tool effectiveness, durability
│       │   └── CLAUDE.md                      # Module documentation
│       │
│       ├── UI/                                # [A7 NEW] User interface widgets
│       │   ├── HotbarWidget.hpp/cpp           # Bottom-center hotbar (9 slots)
│       │   ├── CrosshairWidget.hpp/cpp        # Center-screen crosshair
│       │   ├── InventoryWidget.hpp/cpp        # Full inventory screen (36 slots + crafting)
│       │   ├── CraftingWidget.hpp/cpp         # 2×2 crafting grid component
│       │   ├── DebugOverlayWidget.hpp/cpp     # F3 debug info (Minecraft style)
│       │   ├── MenuWidget.hpp/cpp             # Save/Load/Create game menu
│       │   └── NameTagWidget.hpp/cpp          # Agent name tags (world-space UI)
│       │
│       ├── KADI/                              # [A7 NEW] KADI integration layer
│       │   ├── KADIToolHandler.hpp/cpp        # MCP tool registration & callbacks
│       │   ├── AgentCommandExecutor.hpp/cpp   # Command execution logic (MOVE, MINE, etc.)
│       │   └── AgentVisionSystem.hpp/cpp      # Nearby block/entity queries
│       │
│       └── CLAUDE.md                          # Game module overview
│
├── Run/                                       # Runtime assets and executables
│   ├── SimpleMiner_x64.exe                   # Compiled executable (Debug/Release)
│   │
│   ├── Data/
│   │   ├── Definitions/                      # [A7 MIGRATION] XML → JSON
│   │   │   ├── BlockDefinitions.json         # [A7 NEW] Replaces .xml
│   │   │   ├── ItemDefinitions.json          # [A7 NEW]
│   │   │   ├── Recipes.json                  # [A7 NEW]
│   │   │   └── GameConfig.xml                # [LEGACY] Will migrate post-A7
│   │   │
│   │   ├── Scripts/                          # [A7 NEW] V8 JavaScript game logic
│   │   │   ├── main.js                       # Entry point for JavaScript engine
│   │   │   ├── JSEngine.js                   # System registration framework
│   │   │   ├── JSGame.js                     # Game logic coordinator
│   │   │   ├── InputSystem.js                # Input handling system
│   │   │   ├── Core/                         # Core utilities
│   │   │   ├── Component/                    # Component systems
│   │   │   ├── Interface/                    # C++ bridge interfaces
│   │   │   ├── KADI/                         # KADI agent integration scripts
│   │   │   └── Event/                        # Event handling
│   │   │
│   │   ├── Shaders/                          # HLSL compiled shaders
│   │   │   ├── World.hlsl                    # Voxel lighting shader (A5)
│   │   │   ├── Default.hlsl                  # Standard mesh rendering
│   │   │   └── UI.hlsl                       # [A7 NEW] Widget rendering shader
│   │   │
│   │   ├── Audio/                            # [A7 NEW] Sound effects
│   │   │   ├── Mining/
│   │   │   │   ├── stone_dig.wav
│   │   │   │   ├── stone_break.wav
│   │   │   │   ├── dirt_dig.wav
│   │   │   │   └── wood_dig.wav
│   │   │   ├── Placement/
│   │   │   │   └── block_place.wav
│   │   │   ├── Crafting/
│   │   │   │   └── craft_success.wav
│   │   │   └── UI/
│   │   │       ├── button_click.wav
│   │   │       └── inventory_open.wav
│   │   │
│   │   ├── Images/                           # Textures and sprites
│   │   │   ├── Terrain_8x8.png              # Block textures (existing)
│   │   │   ├── Items_16x16.png              # [A7 NEW] Item sprite sheet
│   │   │   └── UI_Minecraft.png             # [A7 NEW] Minecraft UI sprites
│   │   │
│   │   └── Models/                           # 3D models (if any)
│   │
│   ├── Saves/                                # [A7 NEW] World save files
│   │   └── <WorldName>/
│   │       ├── chunks/                       # Chunk files (.chunk binary)
│   │       └── player.json                   # [A7 NEW] Player state + inventory
│   │
│   └── CLAUDE.md                             # Runtime assets documentation
│
├── Docs/                                     # Development documentation
│   ├── Henrik_Kniberg_Analysis.md           # Minecraft world generation research
│   ├── WorldGenerationDevelopmentBlog/      # Professor's reference blog
│   └── CLAUDE.md
│
├── .claude/plan/                             # [LEGACY] Assignment 4 planning
│   ├── development.md                        # A4 world generation plan
│   └── task-pointer.md                       # A4 task index
│
├── .spec-workflow/                           # Assignment workflow system
│   ├── steering/                            # [PROJECT-WIDE] Steering documents
│   │   ├── product.md                       # ✅ Product vision and principles
│   │   ├── tech.md                          # ✅ Technology stack and architecture
│   │   └── structure.md                     # ✅ Project structure and organization
│   │
│   ├── specs/
│   │   └── A7/                              # [CURRENT ASSIGNMENT]
│   │       ├── requirements.md              # ✅ Approved v3
│   │       ├── design.md                    # ⏳ Pending (includes P-1 rendering fix)
│   │       └── tasks.md                     # ⏳ Pending (after design approval)
│   │
│   ├── archive/specs/                        # Completed assignments
│   │   ├── A5/                              # Lighting system (archived)
│   │   └── A6/                              # Physics & camera (archived)
│   │
│   └── templates/                            # Document templates
│
├── Engine/                                   # [GIT SUBMODULE] Shared engine code
│   └── Code/Engine/
│       ├── Widget/                           # [A7 CRITICAL] UI rendering subsystem
│       │   ├── WidgetSubsystem.hpp/cpp      # Widget lifecycle management
│       │   ├── IWidget.hpp                  # Widget base interface
│       │   └── CLAUDE.md                    # ✅ Created (P-2)
│       │
│       ├── Network/                          # [A7 CRITICAL] KADI integration
│       │   ├── KADIWebSocketSubsystem.hpp/cpp # WebSocket KADI broker client
│       │   ├── IKADIProtocolAdapter.hpp     # KADI protocol abstraction
│       │   ├── KADIProtocolV1Adapter.hpp/cpp # Version 1 implementation
│       │   └── CLAUDE.md                    # ✅ Updated (P-2)
│       │
│       ├── Audio/                            # [A7 USED] FMOD-based sound
│       │   ├── AudioSystem.hpp/cpp          # Sound loading and playback
│       │   └── CLAUDE.md                    # ✅ Updated (P-2)
│       │
│       ├── ThirdParty/json/                 # [A7 CRITICAL] JSON parsing
│       │   ├── json.hpp                     # nlohmann/json library
│       │   └── CLAUDE.md                    # ✅ Created (P-2)
│       │
│       └── [Other modules: Core, Math, Renderer, Input, Platform, etc.]
│
├── CLAUDE.md                                 # ✅ Updated (P-2) - Root project overview
├── SimpleMiner.sln                           # Visual Studio solution
└── .gitignore                                # Git exclusions
```

## Naming Conventions

### Files

#### C++ Source Files
- **Classes**: `PascalCase.hpp` / `PascalCase.cpp` (e.g., `AIAgent.hpp`, `ItemStack.cpp`)
- **Modules**: `PascalCase.hpp` / `PascalCase.cpp` (e.g., `GameCommon.hpp`)
- **Documentation**: `CLAUDE.md` (all caps, per module directory)

#### Data Files (Assignment 7)
- **JSON Definitions**: `PascalCase.json` (e.g., `BlockDefinitions.json`, `Recipes.json`)
- **Shaders**: `PascalCase.hlsl` (e.g., `World.hlsl`, `UI.hlsl`)
- **Audio**: `snake_case.wav` (e.g., `stone_dig.wav`, `button_click.wav`)
- **Textures**: `PascalCase_dimension.png` (e.g., `Terrain_8x8.png`, `Items_16x16.png`)

#### Save Files (Assignment 7)
- **Chunks**: `chunk_<x>_<y>.chunk` (binary format)
- **Player State**: `player.json` (JSON format)
- **World Metadata**: `world.json` (seed, creation date, etc.)

### Code

#### Classes/Types (C++)
- **Classes**: `PascalCase` (e.g., `AIAgent`, `ItemStack`, `Registry<T>`)
- **Structs**: `s` prefix + `PascalCase` (e.g., `sBlockDefinition`, `sItemDefinition`)
- **Enums**: `ePascalCase` (e.g., `eItemType`, `eConnectionState`)
- **Typedefs**: `PascalCase` (e.g., `WidgetPtr`, `SoundID`)

#### Functions/Methods
- **Public Methods**: `PascalCase` (e.g., `GetNearbyBlocks()`, `QueueCommand()`)
- **Private/Protected**: `PascalCase` (same as public, no prefix distinction)
- **Static Functions**: `PascalCase` (e.g., `BlockRegistry::Get()`)

#### Variables
- **Member Variables**: `m_` prefix + `camelCase` (e.g., `m_itemID`, `m_commandQueue`)
- **Local Variables**: `camelCase` (e.g., `itemStack`, `blockDef`)
- **Function Parameters**: `camelCase` (e.g., `agentID`, `recipeID`)
- **Global Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_STACK_SIZE`, `CHUNK_SIZE_X`)

#### Constants (GameCommon.hpp)
- **World Dimensions**: `CHUNK_SIZE_X`, `CHUNK_SIZE_Y`, `CHUNK_SIZE_Z` (16, 16, 128)
- **Gameplay**: `DEFAULT_TERRAIN_HEIGHT` (70), `MAX_AGENTS` (10)
- **Inventory**: `INVENTORY_SIZE` (36), `HOTBAR_SIZE` (9)

## Import Patterns

### Include Order (C++ Files)
```cpp
// 1. Corresponding header (for .cpp files)
#include "Game/Gameplay/AIAgent.hpp"

// 2. Engine headers (external to Game module)
#include "Engine/Core/StringUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Network/KADIWebSocketSubsystem.hpp"

// 3. Game module headers (internal)
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/World.hpp"
#include "Game/Definition/ItemDefinition.hpp"

// 4. Third-party headers
#include "ThirdParty/json/json.hpp"

// 5. Standard library
#include <vector>
#include <string>
#include <memory>
```

### Forward Declarations (Headers)
- Use forward declarations to minimize #include dependencies in .hpp files
- Only #include when inheritance, composition, or template instantiation required

```cpp
// Forward declarations (in .hpp)
class World;
class ItemDefinition;
class KADIWebSocketSubsystem;

// Full includes (in .cpp)
#include "Game/Gameplay/World.hpp"
#include "Game/Definition/ItemDefinition.hpp"
```

### Absolute Paths
- All includes use absolute paths from project root
- **Game includes**: `#include "Game/..."`
- **Engine includes**: `#include "Engine/..."`
- **No relative includes**: Avoid `#include "../Framework/GameCommon.hpp"`

## Code Structure Patterns

### Class Organization (Standard Pattern)

```cpp
//----------------------------------------------------------------------------------------------------
// AIAgent.hpp
//----------------------------------------------------------------------------------------------------
#pragma once

#include "Game/Gameplay/Entity.hpp"
#include "ThirdParty/json/json.hpp"
#include <queue>
#include <string>

//----------------------------------------------------------------------------------------------------
// Forward Declarations
//----------------------------------------------------------------------------------------------------
class World;
class Inventory;
struct AgentCommand;

//----------------------------------------------------------------------------------------------------
// AIAgent - Autonomous agent entity controlled via KADI commands
//----------------------------------------------------------------------------------------------------
class AIAgent : public Entity
{
public:
    // Construction/Destruction
    AIAgent(Vec3 const& position, std::string const& name);
    ~AIAgent();

    // Entity Interface (override)
    void Update(float deltaSeconds) override;
    void Render() const override;

    // Command Queue
    void QueueCommand(AgentCommand const& command);
    bool HasPendingCommands() const;

    // Vision System
    std::vector<BlockInfo> GetNearbyBlocks(float radius) const;

    // Accessors
    std::string const& GetAgentName() const { return m_agentName; }
    uint64_t GetAgentID() const { return m_agentID; }
    Inventory* GetInventory() { return m_inventory; }

private:
    // Command Execution
    void ProcessNextCommand(float deltaSeconds);
    void ExecuteMoveCommand(AgentCommand const& cmd);
    void ExecuteMineCommand(AgentCommand const& cmd);

    // Member Variables
    std::string              m_agentName;
    uint64_t                 m_agentID;
    std::queue<AgentCommand> m_commandQueue;
    Inventory*               m_inventory = nullptr;
    float                    m_visionRange = 10.0f;
};
```

### Function Organization Principles
1. **Public interface first**: Most commonly used functions at top
2. **Lifecycle methods**: Constructor, destructor, Update(), Render()
3. **Accessors/Mutators**: Getters and setters grouped together
4. **Private helpers**: Implementation details at bottom
5. **Member variables last**: Clear separation from methods

## Code Organization Principles

### 1. Single Responsibility Principle (SOLID)
- Each class/file has one clear purpose
- **Example**: `ItemStack` only manages item quantity/type, doesn't handle rendering
- **Counter-Example**: Don't put inventory logic in `Player` class, use separate `Inventory` class

### 2. Dependency Injection Over Singletons
- Pass dependencies via constructor or method parameters
- **Example**: `AIAgent(World* world, KADIWebSocketSubsystem* kadi)`
- **Avoid**: Global `g_theWorld`, `g_theKADI` singletons (exception: `g_theApp` for legacy reasons)

### 3. Composition Over Inheritance
- **Example**: `Player` *has-a* `Inventory`, not *is-a* `InventoryOwner` base class
- Entity is acceptable base class (provides physics, rendering common to all dynamic objects)

### 4. Const Correctness
- Methods that don't modify state should be `const`
- Pass large objects by `const&` to avoid copies
- Use `const` member variables for immutable properties

## Module Boundaries

### Engine → Game Dependency (One-Way Only)
- **RULE**: Game code can #include Engine headers, Engine code CANNOT #include Game headers
- **Rationale**: Engine is a reusable library, Game is application-specific

```cpp
// ✅ ALLOWED: Game includes Engine
#include "Engine/Widget/WidgetSubsystem.hpp"  // In Game/UI/HotbarWidget.cpp

// ❌ FORBIDDEN: Engine includes Game
#include "Game/Gameplay/AIAgent.hpp"          // In Engine/Network/KADIWebSocketSubsystem.cpp
```

### Framework vs Gameplay Separation
- **Framework**: Core systems used by all games (Chunk, Block, App)
- **Gameplay**: Game-specific logic (World, Player, AIAgent, Inventory)
- **Definition**: Data-driven configuration (BlockDefinition, ItemDefinition, Recipe)
- **UI**: User interface widgets (HotbarWidget, InventoryWidget)

### Cross-Module Communication
- **World → Chunk**: Direct access (World owns chunks)
- **Player → World**: Through public World API (e.g., `World::GetBlock()`)
- **AIAgent → World**: Same as Player (consistent interface)
- **Widget → Game**: Via callbacks or polling (no direct Game manipulation from widgets)

## Code Size Guidelines

### File Size Limits
- **Target**: 500-1000 lines per .cpp file
- **Maximum**: 1500 lines (beyond this, consider splitting into multiple files)
- **Exception**: `Chunk.cpp` currently ~3500 lines due to complex terrain generation (A4 legacy, will refactor post-A7)

### Function/Method Size
- **Target**: 10-30 lines per function
- **Maximum**: 100 lines (beyond this, extract helper functions)
- **Exception**: Large switch statements (e.g., command execution) acceptable if well-commented

### Class Complexity
- **Target**: 10-20 public methods per class
- **Maximum**: 30 methods (beyond this, consider composition or strategy pattern)
- **Nesting Depth**: Maximum 3 levels (if/for/while nesting)

## Assignment 7 Specific Structure

### New Files Created (A7)

#### Gameplay Module
- `AIAgent.hpp/cpp` (~300 lines) - AI-controlled entity
- `ItemEntity.hpp/cpp` (~200 lines) - Dropped items with physics
- `Inventory.hpp/cpp` (~250 lines) - 36-slot container
- `ItemStack.hpp` (~50 lines) - Header-only struct

#### Definition Module
- `Registry.hpp` (~150 lines) - Template registry pattern (header-only)
- `ItemDefinition.hpp/cpp` (~200 lines) - Item properties
- `Recipe.hpp/cpp` (~150 lines) - Crafting recipes
- `ToolDefinition.hpp` (~100 lines) - Tool properties (may merge with ItemDefinition)

#### UI Module (New Directory)
- `HotbarWidget.hpp/cpp` (~250 lines)
- `InventoryWidget.hpp/cpp` (~400 lines) - Most complex UI
- `CraftingWidget.hpp/cpp` (~200 lines)
- `CrosshairWidget.hpp/cpp` (~50 lines)
- `DebugOverlayWidget.hpp/cpp` (~150 lines)
- `MenuWidget.hpp/cpp` (~300 lines)
- `NameTagWidget.hpp/cpp` (~100 lines)

#### KADI Module (New Directory)
- `KADIToolHandler.hpp/cpp` (~300 lines) - MCP tool registration
- `AgentCommandExecutor.hpp/cpp` (~250 lines) - Command execution logic
- `AgentVisionSystem.hpp/cpp` (~150 lines) - Block/entity queries

### Modified Files (A7)

#### Framework Module
- `Block.hpp/cpp` - Update `GetDefinition()` to use BlockRegistry instead of static vector
- `GameCommon.hpp` - Add inventory/item/agent constants

#### Gameplay Module
- `World.hpp/cpp` - Add agent spawning, item entity management
- `Player.hpp/cpp` - Add Inventory member, mining/placement logic
- `Game.hpp/cpp` - Initialize KADI, registries, audio

#### Definition Module
- `BlockDefinition.hpp/cpp` - Migrate from XML to JSON, integrate with Registry<T>

### JSON Data Files (A7)
- `BlockDefinitions.json` (~200 lines) - Migrate from .xml
- `ItemDefinitions.json` (~300 lines) - New
- `Recipes.json` (~150 lines) - 10 recipes

## Documentation Standards

### CLAUDE.md Files
- **Purpose**: Per-module AI context documentation
- **Location**: Root of each module directory
- **Contents**: Module responsibilities, entry points, external interfaces, key dependencies, data models, testing, FAQ, changelog
- **Update**: Keep in sync with code changes (P-2 prerequisite for A7)

### Code Comments
- **Public APIs**: Doxygen-style comments (///-style or //-----------------------------------------------------------------------------------------------------)
- **Complex Logic**: Inline comments explaining "why", not "what"
- **TODOs**: `// TODO(username): Description of future work`
- **Hacks**: `// HACK: Explanation of workaround` (mark technical debt)

### Inline Documentation Example
```cpp
//----------------------------------------------------------------------------------------------------
// QueueCommand - Add command to agent's execution queue
//
// Commands are executed sequentially (FIFO order). If the queue is full, the oldest
// command is dropped (tail-drop policy). This prevents agent freeze when commands
// accumulate faster than execution.
//
// Thread-Safety: This method is NOT thread-safe. Only call from main thread.
//----------------------------------------------------------------------------------------------------
void AIAgent::QueueCommand(AgentCommand const& command)
{
    if (m_commandQueue.size() >= MAX_COMMAND_QUEUE_SIZE)
    {
        m_commandQueue.pop();  // Drop oldest command
    }

    m_commandQueue.push(command);
}
```

---

**Document Version:** 1.0
**Created:** 2025-11-24
**Assignment:** A7 - Registry, Inventory, UI, AI Agents
**Due Date:** December 12, 2025
