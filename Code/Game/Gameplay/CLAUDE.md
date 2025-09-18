[Root Directory](../../../CLAUDE.md) > [Code](../../) > [Game](../) > **Gameplay**

# Gameplay Module - Game Logic and World Systems

## Module Responsibilities

The Gameplay module implements the core game mechanics, world management, and player systems for SimpleMiner. It coordinates the high-level game state, manages the voxel world through chunk systems, handles player input and movement, and provides the foundation for entity-based gameplay. This module contains the game-specific logic that sits above the Framework layer.

## Entry and Startup

### Game State Machine
- **Game Class**: Primary game controller with state management
- **States**: `eGameState::ATTRACT` (menu/title) and `eGameState::GAME` (active play)
- **Initialization**: Creates world, spawns player, initializes cameras and props
- **Update Pipeline**: Input handling → Entity updates → World updates

### World Initialization
- **World Class**: Container for all active chunks and spatial management
- **Chunk Activation**: Dynamic loading/unloading based on player position
- **Camera Management**: World camera separate from UI/debug cameras

## External Interfaces

### Player Input System
- **Keyboard Input**: `UpdateFromKeyBoard()` - Movement, actions, debug toggles
- **Controller Support**: `UpdateFromController()` - Gamepad input handling
- **Input Coordination**: `UpdateFromInput()` - Combines all input sources

### Entity Management
- **Base Entity Class**: Foundation for all dynamic game objects
- **Player Entity**: Specialized entity with camera, input, and movement
- **Prop System**: Static and dynamic world objects, environmental elements
- **Update Coordination**: `UpdateEntities()` manages entity lifecycle and interactions

### World-Chunk Interface
- **Chunk Activation**: `World::ActivateChunk()` creates and initializes chunks
- **Spatial Queries**: `World::GetChunk()` retrieves chunks by coordinates
- **Dynamic Loading**: Chunks created/destroyed based on player proximity

## Key Dependencies and Configuration

### World Management System
```cpp
class World {
    std::vector<Chunk*> m_activeChunks;  // Dynamic chunk collection
    Camera* m_worldCamera;               // Primary gameplay camera
};
```

### Player System Architecture
- **Movement**: 3D character controller with collision detection
- **Camera Control**: First-person perspective with mouse/keyboard input
- **World Interaction**: Block placement/removal, item interaction
- **State Management**: Health, inventory, position tracking

### Entity-Component Architecture
- **Entity Base**: Position, rotation, scale, update/render virtuals
- **Prop Specialization**: Static world objects, decorative elements
- **Extensibility**: Foundation for future entity types (NPCs, animals, items)

## Data Models

### Spatial Coordinate Systems
```cpp
// World coordinates: Float-based 3D positions (Vec3)
// Chunk coordinates: Integer 2D grid positions (IntVec2)
// Local coordinates: Block positions within chunk (IntVec3)
```

### Game State Data
- **Player State**: Position, camera orientation, movement velocity
- **World State**: Active chunk list, loaded chunk boundaries
- **Game Mode**: Current state (attract/game), timing, progression

### Chunk Management
- **Dynamic Activation**: Chunks loaded based on player proximity
- **Memory Management**: Inactive chunks unloaded to conserve memory  
- **Seamless World**: Adjacent chunks align perfectly for continuous world

## Testing and Quality

### Performance Optimization
- **Chunk LOD**: Distance-based level of detail for chunk updates
- **Selective Updates**: Only update active chunks within player range
- **Entity Culling**: Entities outside view frustum skip expensive operations
- **Frame Rate**: Consistent 60fps target with deltaTime-based updates

### Game Loop Architecture
```cpp
void Game::Update() {
    UpdateFromInput();           // Process player input
    UpdateEntities(gameDelta, systemDelta);  // Update all entities
    UpdateWorld(gameDelta);      // Update world systems
}
```

### Debug and Development Tools
- **Attract Mode**: Debug/development state separate from gameplay
- **Player Basis Rendering**: Debug visualization of player coordinate system
- **Entity Debug Info**: Position, velocity, state visualization
- **World Debug**: Chunk boundaries, loading zones, performance metrics

## FAQ

**Q: How does the world coordinate system work?**
A: World uses floating-point Vec3 coordinates where each unit equals 1 meter. Players move continuously in this space while chunks provide discrete 16x16x128 block regions.

**Q: When are chunks activated/deactivated?**
A: Chunks are dynamically loaded based on player position. The exact distance algorithm needs investigation, but typically uses a radius around the player.

**Q: How does player movement relate to chunk boundaries?**
A: Player movement is continuous across chunk boundaries. The World class handles seamless transitions and ensures appropriate chunks are loaded.

**Q: What is the difference between Entity and Prop?**
A: Entity is the base class for all dynamic objects. Prop is a specialized Entity for static/decorative world objects. Player is another specialized Entity.

**Q: How does the camera system work?**
A: Multiple cameras exist: m_screenCamera for UI, m_worldCamera for 3D gameplay. The player controls the world camera through input.

## Related File List

### Core Gameplay Files
- `Game.hpp/cpp` - Main game controller and state machine
- `World.hpp/cpp` - World management and chunk coordination
- `Player.hpp/cpp` - Player character controller and input
- `Entity.hpp/cpp` - Base class for dynamic game objects
- `Prop.hpp/cpp` - Static and dynamic world objects

### Framework Dependencies
- `../Framework/Chunk.hpp` - Individual chunk management
- `../Framework/Block.hpp` - Block data structure
- `../Framework/App.hpp` - Application lifecycle coordination

### Engine Dependencies
- Engine math libraries for Vec3, camera, transformations
- Engine input system for keyboard/controller handling
- Engine rendering for entity and world visualization

## Changelog

- **2025-09-13**: Initial Gameplay module documentation created
- **Recent**: World class now properly manages active chunks for dynamic loading
- **Recent**: Player system integration with input handling and camera control
- **Recent**: Entity system foundation established for future gameplay expansion