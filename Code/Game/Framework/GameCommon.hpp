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

    COUNT               // Total number of modes
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
float constexpr        HUMIDITY_NOISE_SCALE   = 800.f;
unsigned int constexpr HUMIDITY_NOISE_OCTAVES = 4u;

// Temperature Parameters
float constexpr        TEMPERATURE_RAW_NOISE_SCALE = 0.0075f;
float constexpr        TEMPERATURE_NOISE_SCALE     = 400.f;
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

// Debug Visualization Noise Parameters (Phase 0, Task 0.4)
// These mirror the Assignment 4 parameters but are simplified for debug visualization
float constexpr        CONTINENTALNESS_NOISE_SCALE = 600.f;
unsigned int constexpr CONTINENTALNESS_NOISE_OCTAVES = 4u;
float constexpr        EROSION_NOISE_SCALE = 250.f;
unsigned int constexpr EROSION_NOISE_OCTAVES = 4u;
float constexpr        WEIRDNESS_NOISE_SCALE = 300.f;
unsigned int constexpr WEIRDNESS_NOISE_OCTAVES = 3u;
float constexpr        PEAKS_VALLEYS_NOISE_SCALE = 400.f;
unsigned int constexpr PEAKS_VALLEYS_NOISE_OCTAVES = 4u;

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
