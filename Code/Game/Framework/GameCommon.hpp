//----------------------------------------------------------------------------------------------------
// GameCommon.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once

#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"

//----------------------------------------------------------------------------------------------------
#define GAME_DEBUG_MODE

//----------------------------------------------------------------------------------------------------
// Terrain Generation Constants (Perlin Noise Based)
//----------------------------------------------------------------------------------------------------

// World Generation Seed
constexpr unsigned int GAME_SEED = 0u;

// Noise Base Parameters
constexpr float DEFAULT_OCTAVE_PERSISTANCE = 0.5f;
constexpr float DEFAULT_NOISE_OCTAVE_SCALE = 2.0f;

// Terrain Height Configuration
constexpr float DEFAULT_TERRAIN_HEIGHT = 64.0f;
constexpr float RIVER_DEPTH = 8.0f;
constexpr float TERRAIN_NOISE_SCALE = 200.0f;
constexpr unsigned int TERRAIN_NOISE_OCTAVES = 5u;

// Humidity (Biome) Parameters
constexpr float HUMIDITY_NOISE_SCALE = 800.0f;
constexpr unsigned int HUMIDITY_NOISE_OCTAVES = 4u;

// Temperature Parameters
constexpr float TEMPERATURE_RAW_NOISE_SCALE = 0.0075f;
constexpr float TEMPERATURE_NOISE_SCALE = 400.0f;
constexpr unsigned int TEMPERATURE_NOISE_OCTAVES = 4u;

// Hill/Mountain Parameters
constexpr float HILLINESS_NOISE_SCALE = 250.0f;
constexpr unsigned int HILLINESS_NOISE_OCTAVES = 4u;

// Ocean Generation
constexpr float OCEAN_START_THRESHOLD = 0.0f;
constexpr float OCEAN_END_THRESHOLD = 0.5f;
constexpr float OCEAN_DEPTH = 30.0f;
constexpr float OCEANESS_NOISE_SCALE = 600.0f;
constexpr unsigned int OCEANESS_NOISE_OCTAVES = 3u;

// Soil Layer Configuration
constexpr int MIN_DIRT_OFFSET_Z = 3;
constexpr int MAX_DIRT_OFFSET_Z = 4;
constexpr float MIN_SAND_HUMIDITY = 0.4f;
constexpr float MAX_SAND_HUMIDITY = 0.7f;
constexpr int SEA_LEVEL_Z = 64; // CHUNK_SIZE_Z / 2 = 128 / 2

// Ice Formation Parameters
constexpr float ICE_TEMPERATURE_MAX = 0.37f;
constexpr float ICE_TEMPERATURE_MIN = 0.0f;
constexpr float ICE_DEPTH_MIN = 0.0f;
constexpr float ICE_DEPTH_MAX = 8.0f;

// Sand Layer Configuration
constexpr float MIN_SAND_DEPTH_HUMIDITY = 0.4f;
constexpr float MAX_SAND_DEPTH_HUMIDITY = 0.0f;
constexpr float SAND_DEPTH_MIN = 0.0f;
constexpr float SAND_DEPTH_MAX = 6.0f;

// Ore Generation Probabilities
constexpr float COAL_CHANCE = 0.05f;
constexpr float IRON_CHANCE = 0.02f;
constexpr float GOLD_CHANCE = 0.005f;
constexpr float DIAMOND_CHANCE = 0.0001f;

// Special Depth Layers
constexpr int OBSIDIAN_Z = 1;
constexpr int LAVA_Z = 0;

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
using ChunkRLEEntry = RLEEntry<uint8_t>;

//-Forward-Declaration--------------------------------------------------------------------------------
struct Rgba8;
struct Vec2;
class App;
class AudioSystem;
class BitmapFont;
class Game;
class LightSubsystem;
class Renderer;
class RandomNumberGenerator;
class ResourceSubsystem;

//----------------------------------------------------------------------------------------------------
// one-time declaration
extern App*                   g_app;
extern AudioSystem*           g_audio;
extern BitmapFont*            g_bitmapFont;
extern Game*                  g_game;
extern Renderer*              g_renderer;
extern RandomNumberGenerator* g_rng;
extern LightSubsystem*        g_lightSubsystem;
extern ResourceSubsystem*     g_resourceSubsystem;

//----------------------------------------------------------------------------------------------------
void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);
void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color);


//----------------------------------------------------------------------------------------------------
template <typename T>
void GAME_SAFE_RELEASE(T*& pointer)
{
    if (pointer == nullptr) return;
    delete pointer;
    pointer = nullptr;
}
