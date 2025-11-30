# Assignment 7: Product Overview

## Product Purpose

Assignment 7 transforms SimpleMiner from a terrain exploration sandbox into a **playable Minecraft-like voxel game with AI agent integration**. This assignment introduces core gameplay mechanics (mining, crafting, inventory) combined with a unique AI agent system that allows Claude Desktop/Code to control autonomous agents within the game world.

**Core Problem Solved:** Creating a functional gameplay loop where both human players and AI agents can interact with a voxel world through intuitive Minecraft-style mechanics, while establishing a robust AI-to-game integration framework for future autonomous gameplay research.

## Target Users

### Primary User: Course Instructor/Grader
- **Needs**: Evaluate Assignment 7 implementation against Minecraft implementation patterns
- **Pain Points**: Need clear demonstration of all 7 systems working together in MVP demo
- **Success Criteria**: 11 MVP features functional, Minecraft-authentic UI, 3+ AI agents operational

### Secondary User: Developer (Student)
- **Needs**: Build gameplay systems following Minecraft patterns, integrate AI agents via KADI
- **Pain Points**: Complex multi-system integration, UI refinement requirements, tight deadline (12/12)
- **Success Criteria**: All systems integrate cleanly, code follows SOLID principles, extensible architecture

### Tertiary User: AI Researchers (Future)
- **Needs**: Framework for autonomous agent gameplay research and experimentation
- **Pain Points**: Difficulty integrating AI with game engines, lack of standardized interfaces
- **Success Criteria**: MCP tool registration working, agents can perceive and act in game world

## Key Features

### 1. **Minecraft-Style Registry System (JSON-Based)**
- Data-driven architecture for blocks, items, and recipes
- Persistent IDs for save/load compatibility
- JSON format using Engine's json.hpp library
- Migration from XML to JSON for consistency

### 2. **Inventory & Crafting System**
- 36-slot player inventory (27 main + 9 hotbar) following Minecraft structure
- ItemStack pattern with quantity and durability
- 2×2 crafting grid with 10 Minecraft-style recipes
- Item pickup with magnetic attraction and stacking

### 3. **Mining & Placement Mechanics**
- Progressive block breaking with visual crack overlay (Minecraft system)
- Tool effectiveness and durability (Wood=60, Stone=132 uses)
- Right-click block placement with validity checks
- Item drops spawn as entities in world

### 4. **Minecraft-Authentic UI**
- Pixel-perfect HUD (hotbar, crosshair, item counts)
- Full inventory screen with mouse drag-and-drop
- F3 debug overlay (Minecraft style)
- Uses Engine WidgetSubsystem (requires refinement)

### 5. **AI Agent Framework (KADI Integration)**
- 3-10 autonomous AI agents controllable via Claude Desktop/Code
- Command queue system (MOVE, MINE, PLACE, CRAFT, WAIT)
- Vision system for nearby blocks and entities
- MCP tool registration via KADIWebSocketSubsystem

### 6. **Menu System**
- Save/Load/Create game functionality
- Enhanced AttractMode with Minecraft-style buttons
- JSON serialization for player state and inventory
- World management (create, continue, load saved games)

### 7. **Sound Effects System**
- Minecraft-style audio for mining, placing, crafting
- FMOD-based using Engine AudioSystem
- Block-type-specific sounds (stone, dirt, wood)
- UI sounds (button clicks, inventory open/close)

## Business Objectives

1. **Academic Credit**: Successfully complete Assignment 7 requirements by 12/12 deadline
2. **Minecraft Fidelity**: Demonstrate authentic Minecraft implementation patterns throughout
3. **AI Integration Pioneer**: Establish KADI-based AI agent framework as foundation for future research
4. **Code Quality**: Maintain SOLID/DRY/YAGNI/KISS principles for maintainable codebase
5. **Extensibility**: Design systems that support future assignments and feature additions

## Success Metrics

### MVP Completion (Required for Credit)
- **Registry System**: ✅ All blocks, items, recipes load from JSON
- **Inventory**: ✅ 36 slots functional with pickup and transfer
- **Mining**: ✅ Progressive breaking with tool effectiveness
- **Placement**: ✅ Right-click placement consuming inventory
- **HUD**: ✅ Hotbar with item icons and quantities visible
- **Inventory Screen**: ✅ E key opens Minecraft-style UI
- **Crafting**: ✅ 2×2 grid with 10 working recipes
- **AI Agents**: ✅ 3+ agents spawn, move, mine via Claude commands
- **KADI Integration**: ✅ 3+ MCP tools registered and callable
- **Menu System**: ✅ Save/load/create game working
- **Sound Effects**: ✅ Mining, placing, crafting sounds play

### Stretch Goals (Bonus)
- ⭐ Tool durability fully implemented
- ⭐ 10 agents coordinating simultaneously
- ⭐ Pixel-perfect Minecraft UI recreation
- ⭐ Agent pathfinding (A* algorithm)
- ⭐ Shift-click quick transfer

### Performance Targets
- **60 FPS**: Sustained with inventory UI open and 3-10 agents active
- **UI Rendering**: < 2ms per frame for all widgets
- **Agent Update**: < 0.5ms per agent per frame
- **JSON Loading**: < 100ms for all registries at startup

## Product Principles

### 1. **Minecraft Implementation Fidelity**
Every system must follow Minecraft patterns, not generic voxel game patterns. This includes:
- Inventory slot layout (27+9 structure)
- Crafting mechanics (shaped vs shapeless)
- Mining progression (crack overlay, tool effectiveness)
- Item stacking rules (64 for blocks, 1 for tools)
- UI visual style (pixel-perfect when feasible)

**Rationale:** Assignment requirements explicitly specify Minecraft patterns throughout. Deviating risks implementation rejection.

### 2. **JSON-First Data Architecture**
All game data (blocks, items, recipes) must use JSON format via Engine's json.hpp library, replacing legacy XML systems.

**Rationale:** User explicitly requested JSON migration (3 separate comments). Consistency across all A7 data files.

### 3. **KISS Over YAGNI for MVP**
Prioritize simple, working implementations over future-proofing. Only implement 2×2 crafting (not 3×3), command-based AI (not goal-based), 3 agents minimum (not full swarm logic).

**Rationale:** Tight deadline (12/12) and 11 MVP features. Over-engineering risks incomplete submission.

### 4. **KADI as Primary AI Interface**
All AI agent control must route through KADIWebSocketSubsystem with MCP tool registration, not direct API calls.

**Rationale:** Establishes standard protocol for AI-game integration. User has existing DigitalOcean deployment (Agent_TypeScript) and KADI broker infrastructure.

### 5. **WidgetSubsystem Pragmatism**
Use existing WidgetSubsystem despite "very rough" status. Refine only what's blocking MVP, defer comprehensive redesign to post-A7.

**Rationale:** User acknowledged system needs work but wants to use it for A7. Time constraint prevents full UI framework rewrite.

## Monitoring & Visibility

### Demo Scenario Dashboard
During MVP demonstration, the following should be clearly visible:

- **HUD**: Hotbar with 9 slots, selected slot highlighted, item counts displayed
- **Inventory Screen**: 27+9 grid with mouse cursor item, crafting 2×2 grid visible
- **Agent UI**: Name tags above agents, optional velocity vectors (debug mode)
- **F3 Overlay**: Position, FPS, chunk coords, biome, light level (Minecraft info)
- **Claude Desktop**: MCP tool list showing SimpleMiner tools, command execution visible

### Real-Time Updates
- **WebSocket**: KADI connection state (DISCONNECTED → AUTHENTICATED → READY)
- **Agent Status**: Command queue progress, inventory state, vision updates (0.5s refresh)
- **Game State**: Block placement/breaking, item pickup, crafting success

### Key Metrics Displayed
- **Performance**: FPS counter (target: 60 FPS sustained)
- **Agent Count**: Active agents (3 for MVP, up to 10 supported)
- **Inventory**: Slot occupancy, item quantities
- **Tool Durability**: Remaining uses before tool breaks

## Future Vision

### Potential Enhancements (Post-A7)

#### Phase 1: Enhanced Gameplay (Assignment 8?)
- **Mob System**: Hostile mobs (zombies, skeletons) with AI behavior
- **Combat System**: Player damage, health bars, armor
- **3×3 Crafting**: Workbench with full Minecraft recipe support
- **Advanced Items**: Redstone, potions, enchantments

#### Phase 2: Advanced AI (Research Focus)
- **Goal-Based AI**: Agents use planning algorithms instead of command queues
- **Multi-Agent Coordination**: Agents communicate and collaborate on complex tasks
- **Learning System**: Agents learn from player demonstrations
- **Vision Improvements**: Full scene understanding, pathfinding optimization

#### Phase 3: Multiplayer & Social
- **Networked Multiplayer**: Multiple human players + AI agents
- **Spectator Mode**: Watch AI agents play autonomously
- **Replay System**: Record and replay agent behaviors
- **Competition Framework**: AI vs AI or AI vs human challenges

#### Phase 4: Production Polish
- **WidgetSubsystem Overhaul**: Modal dialogs, layout system, event routing
- **Particle Effects**: Block breaking particles, item pickup animations
- **Advanced Audio**: 3D positioned sounds, music system, ambient audio
- **Visual Effects**: Day/night sky, weather, animated water

### Research Applications
SimpleMiner's AI agent framework positions it as a platform for:
- **Autonomous Gameplay Research**: Testing AI decision-making in complex environments
- **Human-AI Collaboration**: Studying how humans and AI agents work together in games
- **Natural Language Control**: Expanding from commands to conversational game control
- **Embodied AI**: Grounding AI agents in physical (voxel) worlds

---

**Document Version:** 1.0
**Created:** 2025-11-24
**Assignment:** A7 - Registry, Inventory, UI, AI Agents
**Due Date:** December 12, 2025
