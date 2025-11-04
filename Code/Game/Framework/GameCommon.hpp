//----------------------------------------------------------------------------------------------------
// GameCommon.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once

#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/JobSystem.hpp"

//----------------------------------------------------------------------------------------------------
#define GAME_DEBUG_MODE

//----------------------------------------------------------------------------------------------------
// Debug Visualization Modes (Phase 0, Task 0.4)
//----------------------------------------------------------------------------------------------------
enum class DebugVisualizationMode : uint8_t
{
    NORMAL_TERRAIN,     // Default terrain rendering
    TEMPERATURE,        // Visualize temperature noise layer (hot=red, cold=blue)
    HUMIDITY,           // Visualize humidity noise layer (wet=blue, dry=yellow)
    CONTINENTALNESS,    // Visualize continentalness (ocean=blue, inland=green)
    EROSION,            // Visualize erosion (flat=green, mountainous=brown)
    WEIRDNESS,          // Visualize weirdness/ridges (normal=gray, weird=purple)
    PEAKS_VALLEYS,      // Visualize peaks and valleys (valleys=dark, peaks=white)
    BIOME_TYPE,         // Visualize final biome type (Phase 1, Task 1.4)

    COUNT               // Total number of modes
};

//----------------------------------------------------------------------------------------------------
// Biome Types (Phase 1, Task 1.2 - Assignment 4)
//----------------------------------------------------------------------------------------------------
enum class BiomeType : uint8_t
{
    OCEAN,           // Non-inland, T1-4
    DEEP_OCEAN,      // Non-inland, low continentalness
    FROZEN_OCEAN,    // Non-inland, T0
    BEACH,           // Coast/inland valleys, T1-3
    SNOWY_BEACH,     // Coast/inland valleys, T0
    DESERT,          // Badland biomes, H0-2
    SAVANNA,         // Badland biomes, H3-4
    PLAINS,          // Middle biomes
    SNOWY_PLAINS,    // Middle biomes, T0, H0
    FOREST,          // Middle biomes, T1, H2-3
    JUNGLE,          // Middle biomes, T3-4, H3-4
    TAIGA,           // Middle biomes, T0, H3-4
    SNOWY_TAIGA,     // Middle biomes, T0, H1-2
    STONY_PEAKS,     // High/Peaks with T>2
    SNOWY_PEAKS,     // High/Peaks with T<=2

    COUNT            // Total number of biomes
};

//----------------------------------------------------------------------------------------------------
// Terrain Generation Constants (Perlin Noise Based)
//----------------------------------------------------------------------------------------------------

// World Generation Seed
unsigned int constexpr GAME_SEED = 0u;

// Noise Base Parameters
float constexpr DEFAULT_OCTAVE_PERSISTANCE = 0.5f;
float constexpr DEFAULT_NOISE_OCTAVE_SCALE = 2.f;

// Terrain Height Configuration
// Phase 0, Task 0.5: Updated for larger chunks (256 blocks tall)
// Assignment 4 official spec: Sea Level = Y = 80 (NOT 64!)
float constexpr        DEFAULT_TERRAIN_HEIGHT = 80.f;  // Align with sea level for Assignment 4
float constexpr        RIVER_DEPTH            = 8.f;
float constexpr        TERRAIN_NOISE_SCALE    = 200.f;
unsigned int constexpr TERRAIN_NOISE_OCTAVES  = 5u;

// Humidity (Biome) Parameters
// Assignment 4: Updated to match official Minecraft frequencies (FREQ_H = 1.0 / 8192.0)
float constexpr        HUMIDITY_NOISE_SCALE   = 8192.f;
unsigned int constexpr HUMIDITY_NOISE_OCTAVES = 4u;

// Temperature Parameters
// Assignment 4: Updated to match official Minecraft frequencies (FREQ_T = 1.0 / 8192.0)
float constexpr        TEMPERATURE_RAW_NOISE_SCALE = 0.0075f;
float constexpr        TEMPERATURE_NOISE_SCALE     = 8192.f;
unsigned int constexpr TEMPERATURE_NOISE_OCTAVES   = 4u;

// Hill/Mountain Parameters
float constexpr        HILLINESS_NOISE_SCALE   = 250.f;
unsigned int constexpr HILLINESS_NOISE_OCTAVES = 4u;

// Ocean Generation
float constexpr        OCEAN_START_THRESHOLD  = 0.0f;
float constexpr        OCEAN_END_THRESHOLD    = 0.5f;
float constexpr        OCEAN_DEPTH            = 30.f;
float constexpr        OCEANESS_NOISE_SCALE   = 600.f;
unsigned int constexpr OCEANESS_NOISE_OCTAVES = 3u;

// Assignment 4: Official Biome Noise Parameters (Phase 1, Task 1.2)
// Based on Minecraft 1.18+ overworld.json frequencies
// Denominators map to Perlin noise scale parameters: FREQ = 1.0 / SCALE
//
// CRITICAL FIX: Original Minecraft scales (2048, 1024) were too large for our world
// At those scales, biomes vary too slowly - entire visible area shows same biome
// Reduced to 300-400 range to match TERRAIN_NOISE_SCALE = 200.f for visible variety

// Continentalness - Ocean to inland gradient (7 categories)
// Original: FREQ_C = 1.0 / 2048.0 (too slow for our world size)
// Fixed: Reduced to 400.f for visible ocean/inland transitions
float constexpr        CONTINENTALNESS_NOISE_SCALE = 400.f;
unsigned int constexpr CONTINENTALNESS_NOISE_OCTAVES = 4u;

// Erosion - Flat to mountainous (7 levels: E0-E6)
// Original: FREQ_E = 1.0 / 1024.0 (too slow for our world size)
// Fixed: Reduced to 300.f for visible erosion patterns
float constexpr        EROSION_NOISE_SCALE = 300.f;
unsigned int constexpr EROSION_NOISE_OCTAVES = 4u;

// Weirdness - Normal to strange terrain (used to calculate PV)
// Original: FREQ_W = 1.0 / 1024.0 (too slow for our world size)
// Fixed: Reduced to 350.f for visible weirdness/peaks & valleys variation
float constexpr        WEIRDNESS_NOISE_SCALE = 350.f;
unsigned int constexpr WEIRDNESS_NOISE_OCTAVES = 3u;

// Peaks and Valleys - Height variation within biome (5 levels: Valleys to Peaks)
// Calculated from Weirdness: PV = 1 - |( 3 * abs(W) ) - 2|
// No separate noise layer needed, derived from Weirdness

//----------------------------------------------------------------------------------------------------
// Assignment 4: 3D Density Terrain Parameters (Phase 2, Task 2.1)
//----------------------------------------------------------------------------------------------------
// Based on professor's simplified density formula (October 15, 2025 blog)
// D(x, y, z) = N(x, y, z, s) + B(z)
// where N = 3D Perlin noise, B = vertical bias based on height from default terrain
// CRITICAL: Negative density = MORE dense (solid blocks), positive = air

// 3D Density Noise Parameters
float constexpr        DENSITY_NOISE_SCALE = 200.f;       // Official: FREQ_3D = 1.0 / 200.0
unsigned int constexpr DENSITY_NOISE_OCTAVES = 3u;        // Balance detail vs performance

// Density Bias Calculation: B(z) = b × (z − t)
// Controls terrain tendency to be solid vs air based on vertical position
// Uses DEFAULT_TERRAIN_HEIGHT = 80.f (defined above at line 69)
// Must be strong enough to overcome noise (which ranges -2.5 to +2.5 after erosion scaling)
float constexpr DENSITY_BIAS_PER_BLOCK = 0.10f;           // Increased from 0.02 to make terrain height dominant

// Top and Bottom Slide Parameters (Phase 2, Task 2.2)
// Slides smooth density transitions at world boundaries for natural terrain
// Top slide: prevents sharp cutoffs near surface (makes terrain taper to air)
// Bottom slide: creates flatter base near bedrock (makes terrain more solid)
int constexpr TOP_SLIDE_START = 100;                      // Z level where top slide begins
int constexpr TOP_SLIDE_END = 120;                        // Z level where top slide ends (world top)
int constexpr BOTTOM_SLIDE_START = 0;                     // Z level where bottom slide starts (world bottom)
int constexpr BOTTOM_SLIDE_END = 20;                      // Z level where bottom slide ends

// Terrain Shaping Curves (Phase 2, Task 2.3)
// Use biome parameters (C, E, PV) to shape terrain height and scale
// Based on professor's October 16 blog findings (squashing factor, height offset)

// Continentalness Curve: Height offset based on ocean/inland distance
// Maps C: [-1.2, 1.0] → Height offset: [-30, +40]
float constexpr CONTINENTALNESS_HEIGHT_MIN = -30.0f;     // Ocean depth offset
float constexpr CONTINENTALNESS_HEIGHT_MAX = 40.0f;      // Inland height boost

// Erosion Curve: Terrain wildness/scale based on flat/mountainous
// Maps E: [-1, 1] → Scale multiplier: [0.3, 2.5]
float constexpr EROSION_SCALE_MIN = 0.3f;                 // Flat terrain (low wildness)
float constexpr EROSION_SCALE_MAX = 2.5f;                 // Mountainous (high wildness)

// Peaks & Valleys Curve: Additional height variation
// Maps PV: [-1, 1] → Height modifier: [-15, +25]
float constexpr PV_HEIGHT_MIN = -15.0f;                   // Valley depression
float constexpr PV_HEIGHT_MAX = 25.0f;                    // Peak elevation

// Note: Cave carving parameters (cheese/spaghetti) will be added in Phase 4

//----------------------------------------------------------------------------------------------------
// Assignment 4: Tree Placement Parameters (Phase 3, Task 3B.2)
//----------------------------------------------------------------------------------------------------
// Trees are placed using noise-based sampling after surface generation.
// Each biome has specific tree types and placement density.

// Tree Noise Parameters
float constexpr TREE_NOISE_SCALE = 10.f;              // Small scale for local variation
unsigned int constexpr TREE_NOISE_OCTAVES = 2u;       // Low octaves for performance
float constexpr TREE_PLACEMENT_THRESHOLD = 0.8f;      // Higher threshold = sparser trees

// Tree Spacing Parameters
int constexpr MIN_TREE_SPACING = 3;                   // Minimum blocks between tree trunks
int constexpr TREE_EDGE_SAFETY_MARGIN = 4;            // Blocks from chunk edge to skip trees

//----------------------------------------------------------------------------------------------------
// Soil Layer Configuration
int constexpr   MIN_DIRT_OFFSET_Z = 3;
int constexpr   MAX_DIRT_OFFSET_Z = 4;
float constexpr MIN_SAND_HUMIDITY = 0.4f;
float constexpr MAX_SAND_HUMIDITY = 0.7f;
// Phase 0, Task 0.5: Updated for Assignment 4 (Official spec: Sea Level = Y = 80, NOT CHUNK_SIZE_Z/2)
int constexpr   SEA_LEVEL_Z       = 80; // Official Assignment 4 specification

// Ice Formation Parameters
float constexpr ICE_TEMPERATURE_MAX = 0.37f;
float constexpr ICE_TEMPERATURE_MIN = 0.f;
float constexpr ICE_DEPTH_MIN       = 0.f;
float constexpr ICE_DEPTH_MAX       = 8.f;

// Sand Layer Configuration
float constexpr MIN_SAND_DEPTH_HUMIDITY = 0.4f;
float constexpr MAX_SAND_DEPTH_HUMIDITY = 0.f;
float constexpr SAND_DEPTH_MIN          = 0.f;
float constexpr SAND_DEPTH_MAX          = 6.f;

// Ore Generation Probabilities
float constexpr COAL_CHANCE    = 0.05f;
float constexpr IRON_CHANCE    = 0.02f;
float constexpr GOLD_CHANCE    = 0.005f;
float constexpr DIAMOND_CHANCE = 0.0001f;

// Special Depth Layers
int constexpr OBSIDIAN_Z = 1;
int constexpr LAVA_Z     = 0;

//----------------------------------------------------------------------------------------------------
// Chunk File Format Structures
//----------------------------------------------------------------------------------------------------

// Chunk file header structure (8 bytes total)
struct ChunkFileHeader
{
    char    fourCC[4];      // "GCHK" - Guildhall Chunk identifier
    uint8_t version;        // File format version (1)
    uint8_t chunkBitsX;     // Will be set to CHUNK_BITS_X (4)
    uint8_t chunkBitsY;     // Will be set to CHUNK_BITS_Y (4) 
    uint8_t chunkBitsZ;     // Will be set to CHUNK_BITS_Z (7)
};

// Chunk-specific RLE entry type alias for Engine's generic RLE system
using ChunkRLEEntry = sRLEEntry<uint8_t>;

//-Forward-Declaration--------------------------------------------------------------------------------
class App;
class Game;

//----------------------------------------------------------------------------------------------------
// one-time declaration
extern App*  g_app;
extern Game* g_game;

//----------------------------------------------------------------------------------------------------
template <typename T>
void GAME_SAFE_RELEASE(T*& pointer)
{
    if (pointer == nullptr) return;
    delete pointer;
    pointer = nullptr;
}
