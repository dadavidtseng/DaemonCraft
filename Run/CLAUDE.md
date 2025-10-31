[🏠 Root](../../CLAUDE.md) > **[📦 Run]**

**Navigation:** [Back to Root](../../CLAUDE.md) | [Game Module](../../Code/Game/CLAUDE.md) | [Framework](../../Code/Game/Framework/CLAUDE.md) | [Gameplay](../../Code/Game/Gameplay/CLAUDE.md)

---

# Run Module - Runtime Assets and Executables

## Quick Navigation
- **[Game Module](../../Code/Game/CLAUDE.md)** - Core game logic and entry points
- **[Framework Module](../../Code/Game/Framework/CLAUDE.md)** - Core systems (**Assignment 4: Primary implementation**)
- **[Definition Module](../../Code/Game/Definition/CLAUDE.md)** - BlockDefinition configuration
- **[Development Plan](../../.claude/plan/development.md)** - Assignment 4: World Generation
- **[Task Pointer](../../.claude/plan/task-pointer.md)** - Quick task reference

---

## Assignment 4: World Generation - Asset Requirements

**Status:** Phase 0 - Prerequisites (Upcoming)

**CRITICAL ASSET UPDATES REQUIRED:** Assignment 4 requires new sprite sheets and block definitions to support biome-specific blocks (trees, leaves, surface variants).

### Files to Replace/Add

**REQUIRED (Phase 1, Task 1.1):**
1. **BlockSpriteSheet_BlockDefinitions.xml**
   - Location: `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`
   - Action: **REPLACE** with new file from Canvas assignment zip
   - Contents: New block types for trees (wood, leaves), biome-specific surfaces
   - Must be done FIRST before any code changes

2. **New Sprite Sheets**
   - Location: `Data/Images/`
   - Action: **ADD** new sprite sheet files from Canvas assignment zip
   - Contents: Biome-specific block textures (oak/pine/palm trees, varied leaves, etc.)
   - Resolution: Likely 32px and/or 128px versions

**Verification Steps:**
- [ ] Download assignment zip from Canvas
- [ ] Replace `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`
- [ ] Add new sprite sheets to `Data/Images/`
- [ ] Launch game to verify assets load (may have missing textures initially - expected)
- [ ] Document new block type indices for use in code

### New Block Types Expected

Based on Assignment 4 requirements, the new sprite sheet should include:
- **Wood Types**: Oak, Pine, Palm (for tree trunks)
- **Leaf Types**: Oak leaves, Pine needles, Palm fronds
- **Surface Variants**: Biome-specific grass, sand, snow textures
- **Underground**: Cave-specific blocks (stalactites/stalagmites if supported)

**Block Index Documentation:** After loading new definitions, update [Framework/GameCommon.hpp](../../Code/Game/Framework/CLAUDE.md) with new block type constants.

**Resources:**
- [Development Plan](../../.claude/plan/development.md) - Phase 1, Task 1.1: Asset Integration
- [Task Pointer](../../.claude/plan/task-pointer.md) - Quick reference for asset setup

---

## Module Responsibilities

The Run module contains all runtime assets, executables, and configuration files required to execute SimpleMiner. This directory serves as the working directory for the game application and contains data-driven configurations, 3D assets, shaders, audio files, and all necessary runtime libraries. The module follows a structured asset organization that supports the game's rendering pipeline and gameplay systems.

## Entry and Startup

### Executable Files
- **SimpleMiner_Debug_x64.exe**: Debug build with full debugging symbols and logging
- **SimpleMiner_Release_x64.exe**: Optimized release build for production
- **DaemonCraft_Debug_x64.exe**: Legacy executable name (being transitioned)
- **DaemonCraft_Release_x64.exe**: Legacy release executable

### Runtime Libraries
- **V8 JavaScript Engine**: v8.dll, v8_libbase.dll, v8_libplatform.dll
- **ICU Internationalization**: icuuc.dll, third_party_icu_icui18n.dll
- **FMOD Audio**: fmod.dll, fmod64.dll for professional audio support
- **Compression**: third_party_zlib.dll, third_party_abseil-cpp_absl.dll

## External Interfaces

### Configuration System
- **GameConfig.xml**: Primary game configuration
  - Window properties (size: 1600x800, center position)
  - Display settings and window behavior
  - Runtime toggles and system preferences

### Block Definition System
- **BlockSpriteSheet_BlockDefinitions.xml**: Data-driven block properties
  - 27 defined block types (Air, Grass, Stone, ores, wood types, etc.)
  - Visual properties: visibility, opacity, texture coordinates
  - Physical properties: solidity, collision detection
  - Lighting properties: emission values for light sources (Glowstone)

### Asset Loading Pipeline
- **Texture Atlas**: BlockSpriteSheet_128px.png and BlockSpriteSheet_32px.png
- **3D Models**: Cube variants, Tutorial_Box_Phong, Woman character model
- **Audio Assets**: TestSound.mp3 for audio system validation
- **Fonts**: DaemonFont.png, SquirrelFixedFont.png for UI text rendering

## Key Dependencies and Configuration

### Shader System (HLSL DirectX 11)
```hlsl
// Default.hlsl - Basic unlit rendering with Vertex_PCU format
// BlinnPhong.hlsl - Phong lighting model for realistic materials  
// Bloom.hlsl - Post-processing effects for enhanced visuals
```

### Data Directory Structure
```
Run/Data/
├── Audio/              # Sound effects and music
├── Definitions/        # XML configuration files
├── Fonts/             # Typography resources
├── Images/            # Textures and UI graphics
├── Models/            # 3D model assets (.obj, .FBX files)
├── Shaders/           # HLSL shader programs
└── GameConfig.xml     # Main configuration
```

### Asset Format Support
- **3D Models**: .obj, .FBX formats with material definitions
- **Textures**: .png, .tga formats with sprite sheet organization
- **Audio**: .mp3 format with FMOD processing
- **Configuration**: .xml for structured data definitions

## Data Models

### Block Definition Schema
```xml
<BlockDefinition name="Grass" 
                isVisible="true" 
                isSolid="true" 
                isOpaque="true" 
                topSpriteCoords="0, 0" 
                bottomSpriteCoords="2, 0" 
                sideSpriteCoords="1, 0"/>
```

### Texture Atlas System
- **Sprite Coordinates**: Grid-based UV mapping (x, y) coordinates
- **Multiple Resolutions**: 32px and 128px versions for different quality levels
- **Face-Specific Textures**: Top, bottom, and side faces can use different sprites

### Model Asset Organization
- **Cube Models**: Basic geometry variants (v, vi, vn, vni formats)
- **Character Models**: Woman model with diffuse and normal maps
- **Environment**: Tutorial_Box with complete material setup (diffuse, normal, specular)

## Testing and Quality

### Asset Validation
- **XML Schema**: Well-formed XML with consistent attribute naming
- **Texture Integrity**: Proper sprite sheet alignment and UV coordinates
- **Model Loading**: Vertex format compatibility with engine expectations
- **Audio Testing**: FMOD integration validation with test audio files

### Performance Considerations
- **Texture Resolution**: Multiple sizes for LOD and performance scaling
- **Model Complexity**: Optimized vertex counts for real-time rendering
- **Asset Streaming**: Efficient loading patterns for large world data
- **Memory Usage**: Conservative texture and model memory management

### Build Integration
- **Post-Build Events**: Automatic copying of executables from Temporary/ to Run/
- **DLL Deployment**: V8 runtime libraries copied based on build configuration
- **Asset Synchronization**: Game data remains in Run/ for consistent access

## FAQ

**Q: Why are there both SimpleMiner and DaemonCraft executables?**
A: This appears to be a naming transition. The project is moving from "DaemonCraft" to "SimpleMiner" branding, with both executables currently present during the transition.

**Q: How does the texture atlas system work?**
A: Block faces reference sprite coordinates (x, y) in the BlockSpriteSheet. Each block type can specify different textures for top, bottom, and side faces using these coordinates.

**Q: What is the purpose of multiple model variants (Cube_v, Cube_vi, etc.)?**
A: Different vertex formats: 'v' = vertices only, 'vi' = vertices+indices, 'vn' = vertices+normals, 'vni' = vertices+normals+indices for different rendering needs.

**Q: How are runtime libraries managed?**
A: V8 and other third-party DLLs are automatically copied by post-build events based on Debug/Release configuration. The game loads these dynamically at runtime.

**Q: Can game configuration be modified without rebuilding?**
A: Yes, GameConfig.xml and BlockSpriteSheet_BlockDefinitions.xml are loaded at runtime, allowing configuration changes without recompilation.

## Related File List

### Configuration Files
- `Data/GameConfig.xml` - Main game settings
- `Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml` - Block type definitions

### Rendering Assets
- `Data/Shaders/Default.hlsl` - Basic rendering shader
- `Data/Shaders/BlinnPhong.hlsl` - Lighting shader
- `Data/Shaders/Bloom.hlsl` - Post-processing shader
- `Data/Images/BlockSpriteSheet_*.png` - Texture atlas files

### 3D Assets
- `Data/Models/Cube/` - Basic geometry variants
- `Data/Models/TutorialBox_Phong/` - Complete material example
- `Data/Models/Woman/` - Character model with textures

### Runtime Files
- `SimpleMiner_*.exe` - Game executables
- `fmod*.dll` - Audio system libraries
- `v8*.dll` - JavaScript engine libraries
- `icu*.dll` - Internationalization libraries

## Changelog

- **2025-10-26**: Updated documentation for Assignment 4: World Generation
  - Added Assignment 4 Asset Requirements section detailing critical file replacements
  - Added navigation breadcrumbs and quick navigation
  - Documented new sprite sheets and block definitions from Canvas assignment zip
  - Added verification steps for asset integration (Phase 1, Task 1.1)
  - Enhanced inline cross-references to Framework and Definition modules
  - Linked to development planning resources
- **2025-09-13**: Initial Run module documentation created
- **Recent**: Added comprehensive block definitions with 27+ block types
- **Recent**: Texture atlas system with multiple resolution support
- **Recent**: V8 JavaScript runtime integration with automatic DLL deployment