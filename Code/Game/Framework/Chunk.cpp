//----------------------------------------------------------------------------------------------------
// Chunk.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Chunk.hpp"

#include <filesystem>

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Framework/BlockIterator.hpp"
#include "Game/Gameplay/Game.hpp"  // For g_game and visualization mode access
#include "ThirdParty/Noise/RawNoise.hpp"
#include "ThirdParty/Noise/SmoothNoise.hpp"

//----------------------------------------------------------------------------------------------------
// Block Type Constants - Must match BlockSpriteSheet_BlockDefinitions.xml exactly (0-indexed)
// Phase 1, Task 1.1: Updated to match professor's new XML (Assignment 4)
const uint8_t BLOCK_AIR                 = 0;   // Air
const uint8_t BLOCK_WATER               = 1;   // Water
const uint8_t BLOCK_SAND                = 2;   // Sand
const uint8_t BLOCK_SNOW                = 3;   // Snow
const uint8_t BLOCK_ICE                 = 4;   // Ice
const uint8_t BLOCK_DIRT                = 5;   // Dirt
const uint8_t BLOCK_STONE               = 6;   // Stone
const uint8_t BLOCK_COAL                = 7;   // Coal
const uint8_t BLOCK_IRON                = 8;   // Iron
const uint8_t BLOCK_GOLD                = 9;   // Gold
const uint8_t BLOCK_DIAMOND             = 10;  // Diamond
const uint8_t BLOCK_OBSIDIAN            = 11;  // Obsidian
const uint8_t BLOCK_LAVA                = 12;  // Lava
const uint8_t BLOCK_GLOWSTONE           = 13;  // Glowstone
const uint8_t BLOCK_COBBLESTONE         = 14;  // Cobblestone
const uint8_t BLOCK_CHISELED_BRICK      = 15;  // ChiseledBrick
const uint8_t BLOCK_GRASS               = 16;  // Grass (standard)
const uint8_t BLOCK_GRASS_LIGHT         = 17;  // GrassLight
const uint8_t BLOCK_GRASS_DARK          = 18;  // GrassDark
const uint8_t BLOCK_GRASS_YELLOW        = 19;  // GrassYellow
const uint8_t BLOCK_ACACIA_LOG          = 20;  // AcaciaLog
const uint8_t BLOCK_ACACIA_PLANKS       = 21;  // AcaciaPlanks
const uint8_t BLOCK_ACACIA_LEAVES       = 22;  // AcaciaLeaves
const uint8_t BLOCK_CACTUS_LOG          = 23;  // CactusLog
const uint8_t BLOCK_OAK_LOG             = 24;  // OakLog
const uint8_t BLOCK_OAK_PLANKS          = 25;  // OakPlanks
const uint8_t BLOCK_OAK_LEAVES          = 26;  // OakLeaves
const uint8_t BLOCK_BIRCH_LOG           = 27;  // BirchLog
const uint8_t BLOCK_BIRCH_PLANKS        = 28;  // BirchPlanks
const uint8_t BLOCK_BIRCH_LEAVES        = 29;  // BirchLeaves
const uint8_t BLOCK_JUNGLE_LOG          = 30;  // JungleLog
const uint8_t BLOCK_JUNGLE_PLANKS       = 31;  // JunglePlanks
const uint8_t BLOCK_JUNGLE_LEAVES       = 32;  // JungleLeaves
const uint8_t BLOCK_SPRUCE_LOG          = 33;  // SpruceLog
const uint8_t BLOCK_SPRUCE_PLANKS       = 34;  // SprucePlanks
const uint8_t BLOCK_SPRUCE_LEAVES       = 35;  // SpruceLeaves
const uint8_t BLOCK_SPRUCE_LEAVES_SNOW  = 36;  // SpruceLeavesSnow

//----------------------------------------------------------------------------------------------------
Chunk::Chunk(IntVec2 const& chunkCoords)
    : m_chunkCoords(chunkCoords)
{
    // Calculate world bounds
    Vec3 worldMins((float)(chunkCoords.x) * CHUNK_SIZE_X, (float)(chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 worldMaxs = worldMins + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);
    m_worldBounds  = AABB3(worldMins, worldMaxs);

    // Initialize all blocks to air (terrain generation happens asynchronously)
    for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
    {
        m_blocks[i].m_typeIndex = 0; // BLOCK_AIR
    }
}

//----------------------------------------------------------------------------------------------------
Chunk::~Chunk()
{
    GAME_SAFE_RELEASE(m_vertexBuffer);
    GAME_SAFE_RELEASE(m_indexBuffer);
    GAME_SAFE_RELEASE(m_debugVertexBuffer);
    // GAME_SAFE_RELEASE(m_debugBuffer);
}

//----------------------------------------------------------------------------------------------------
void Chunk::Update(float const deltaSeconds)
{
    UNUSED(deltaSeconds)

    // NOTE: Mesh rebuilding is now managed by World class to ensure only one chunk per frame
    // The World::Update() method calls RebuildMesh() directly on the nearest dirty chunk
    // This ensures the assignment requirement of "up to one active chunk at most may be built or rebuilt per frame"

    // NOTE: F2 debug key handling is now managed by World class to ensure consistent behavior
    // across all chunks including newly activated ones
}

//----------------------------------------------------------------------------------------------------
void Chunk::Render()
{
    // CRITICAL FIX: Don't render dirty chunks - they have stale buffer data
    // When a neighbor chunk is modified, this chunk gets marked dirty but buffers
    // still contain old mesh data, causing a flash when rendered with stale data
    if (m_isMeshDirty)
    {
        return;  // Skip rendering - mesh is being rebuilt or needs rebuild
    }

    // CRITICAL FIX: Only check buffers, not m_vertices
    // m_vertices can be inconsistent during SetMeshData() causing false negatives
    // Buffers are the source of truth for whether chunk can be rendered
    if (m_vertexBuffer && m_indexBuffer)
    {
        g_renderer->SetBlendMode(eBlendMode::OPAQUE);
        g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
        g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
        g_renderer->SetDepthMode(eDepthMode::READ_WRITE_LESS_EQUAL);

        // Phase 1, Task 1.1: Use Assignment 4 Dokucraft High 32px sprite sheet (matches new XML layout)
        g_renderer->BindTexture(g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/SpriteSheet_Dokucraft_High_32px.png"));

        // CRITICAL FIX: Use buffer's internal size to avoid race condition during RebuildMesh()
        // RebuildMesh() clears m_indices at line 579, causing m_indices.size() to return 0
        // This results in bright flashing when rendering with 0 indices
        // Buffer's size is stable and represents the actual GPU data
        int indexCount = m_indexBuffer->GetSize() / m_indexBuffer->GetStride();

        // DEBUG: Log rendering info when chunk is dirty (being rebuilt)
        if (m_isMeshDirty)
        {
            DebuggerPrintf("RENDER DURING DIRTY: Chunk(%d,%d) - indexCount=%d, m_indices.size()=%zu, bufferSize=%u, bufferStride=%u\n",
                m_chunkCoords.x, m_chunkCoords.y,
                indexCount, m_indices.size(),
                m_indexBuffer->GetSize(), m_indexBuffer->GetStride());
        }

        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, indexCount);
    }

    if (!m_drawDebug || !m_debugVertexBuffer) return;

    g_renderer->BindTexture(nullptr);
    // Use buffer's internal size for consistency (same fix as main rendering)
    int debugVertexCount = m_debugVertexBuffer->GetSize() / m_debugVertexBuffer->GetStride();
    g_renderer->DrawVertexBuffer(m_debugVertexBuffer, debugVertexCount);
}

//----------------------------------------------------------------------------------------------------
// Helper function for Phase 0, Task 0.4: Debug Visualization
// Converts noise value [-1, 1] to a colored block type for visualization
//----------------------------------------------------------------------------------------------------
static uint8_t GetDebugVisualizationBlock(float noiseValue, DebugVisualizationMode mode)
{
    // Clamp noise value to [-1, 1] range using Engine's GetClamped() template function
    float clampedValue = GetClamped(noiseValue, -1.f, 1.f);

    // Map to [0, 1] range for easier color mapping
    float normalizedValue = (clampedValue + 1.f) * 0.5f;

    // Different color schemes for different noise layers
    switch (mode)
    {
        case DebugVisualizationMode::TEMPERATURE:
            // Hot (red/yellow) to Cold (blue/white)
            if (normalizedValue > 0.75f)      return BLOCK_LAVA;          // Very hot - red
            if (normalizedValue > 0.5f)       return BLOCK_GOLD;          // Hot - yellow/gold
            if (normalizedValue > 0.25f)      return BLOCK_STONE;         // Cool - gray
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Cold - dark gray
            return BLOCK_ICE;  // Very cold - blue/white

        case DebugVisualizationMode::HUMIDITY:
            // Wet (blue) to Dry (yellow/brown)
            if (normalizedValue > 0.75f)      return BLOCK_ICE;           // Very wet - blue
            if (normalizedValue > 0.5f)       return BLOCK_COBBLESTONE;   // Wet - dark
            if (normalizedValue > 0.25f)      return BLOCK_DIRT;          // Medium - brown
            if (normalizedValue > 0.1f)       return BLOCK_SAND;          // Dry - tan
            return BLOCK_GOLD;  // Very dry - yellow

        case DebugVisualizationMode::CONTINENTALNESS:
            // Ocean (blue) to Inland (green/brown)
            if (normalizedValue > 0.75f)      return BLOCK_GRASS;         // Far inland - green
            if (normalizedValue > 0.5f)       return BLOCK_DIRT;          // Mid inland - brown
            if (normalizedValue > 0.25f)      return BLOCK_SAND;          // Coast - tan
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Near ocean - gray
            return BLOCK_ICE;  // Ocean - blue

        case DebugVisualizationMode::EROSION:
            // Flat (green) to Mountainous (brown/gray)
            if (normalizedValue > 0.75f)      return BLOCK_STONE;         // Very eroded/mountainous - gray
            if (normalizedValue > 0.5f)       return BLOCK_COBBLESTONE;   // Eroded - dark gray
            if (normalizedValue > 0.25f)      return BLOCK_DIRT;          // Medium - brown
            if (normalizedValue > 0.1f)       return BLOCK_GRASS;         // Flat - green
            return BLOCK_SAND;  // Very flat - smooth tan

        case DebugVisualizationMode::WEIRDNESS:
            // Normal (gray) to Weird (colorful)
            if (normalizedValue > 0.75f)      return BLOCK_DIAMOND;       // Very weird - cyan
            if (normalizedValue > 0.5f)       return BLOCK_GOLD;          // Weird - yellow
            if (normalizedValue > 0.25f)      return BLOCK_STONE;         // Normal - gray
            if (normalizedValue > 0.1f)       return BLOCK_COBBLESTONE;   // Slightly weird - dark gray
            return BLOCK_IRON;  // Normal - iron gray

        case DebugVisualizationMode::PEAKS_VALLEYS:
            // Valleys (dark) to Peaks (white/bright)
            if (normalizedValue > 0.75f)      return BLOCK_ICE;           // Peaks - white/bright
            if (normalizedValue > 0.5f)       return BLOCK_STONE;         // High - light gray
            if (normalizedValue > 0.25f)      return BLOCK_COBBLESTONE;   // Mid - gray
            if (normalizedValue > 0.1f)       return BLOCK_DIRT;          // Low - brown
            return BLOCK_COAL;  // Valleys - black/dark

        case DebugVisualizationMode::BIOME_TYPE:
        {
            // Special case: noiseValue is actually BiomeType enum cast to float
            // No need to normalize - directly cast back to BiomeType
            BiomeType biome = static_cast<BiomeType>(static_cast<int>(noiseValue));

            // Map each biome to a distinctive colored block
            // CRITICAL FIX: Use different colored blocks for each biome to show variety
            // Previous issue: PLAINS/FOREST/JUNGLE/TAIGA all mapped to BLOCK_GRASS (green)
            switch (biome)
            {
                case BiomeType::OCEAN:          return BLOCK_DIAMOND;       // Cyan (water)
                case BiomeType::DEEP_OCEAN:     return BLOCK_COBBLESTONE;   // Dark blue/gray
                case BiomeType::FROZEN_OCEAN:   return BLOCK_ICE;           // White/blue (ice)
                case BiomeType::BEACH:          return BLOCK_SAND;          // Tan (sand)
                case BiomeType::SNOWY_BEACH:    return BLOCK_SNOW;          // White (snow)
                case BiomeType::DESERT:         return BLOCK_GOLD;          // Yellow/gold (sand)
                case BiomeType::SAVANNA:        return BLOCK_DIRT;          // Brown (dirt)
                case BiomeType::PLAINS:         return BLOCK_GRASS;         // Light green (grass)
                case BiomeType::SNOWY_PLAINS:   return BLOCK_SNOW;          // White (snow)
                case BiomeType::FOREST:         return BLOCK_OAK_LEAVES;    // Green (distinct from plains)
                case BiomeType::JUNGLE:         return BLOCK_JUNGLE_LEAVES; // Dark green (dense forest)
                case BiomeType::TAIGA:          return BLOCK_SPRUCE_LEAVES; // Blue-green (conifer forest)
                case BiomeType::SNOWY_TAIGA:    return BLOCK_SPRUCE_LEAVES_SNOW; // White/green (snow + trees)
                case BiomeType::STONY_PEAKS:    return BLOCK_STONE;         // Gray (rocky mountains)
                case BiomeType::SNOWY_PEAKS:    return BLOCK_ICE;           // White (snowy mountains)
                default:                        return BLOCK_STONE;         // Fallback
            }
        }

        default:
            return BLOCK_STONE;  // Should never reach here
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 4: Biome Selection Helper Function (Phase 1, Task 1.3)
//----------------------------------------------------------------------------------------------------
// Determines biome type based on 6 noise parameters using lookup table approach
// Based on Minecraft 1.18+ biome selection system
// Reference: Docs/Biomes.txt for complete lookup tables
//----------------------------------------------------------------------------------------------------
static BiomeType SelectBiome(float temperature, float humidity, float continentalness,
                             float erosion, float peaksValleys)
{
    // Discretize temperature into 5 levels (T0-T4)
    // T0: [-1.00, -0.45) Frozen, T1: [-0.45, -0.15) Cool, T2: [-0.15, 0.20) Neutral
    // T3: [0.20, 0.55) Warm, T4: [0.55, 1.00] Hot
    int tempLevel = 2;  // Default: T2 (Neutral)
    if (temperature < -0.45f)      tempLevel = 0;  // T0 (Frozen)
    else if (temperature < -0.15f) tempLevel = 1;  // T1 (Cool)
    else if (temperature < 0.20f)  tempLevel = 2;  // T2 (Neutral)
    else if (temperature < 0.55f)  tempLevel = 3;  // T3 (Warm)
    else                           tempLevel = 4;  // T4 (Hot)

    // Discretize humidity into 5 levels (H0-H4)
    // H0: [-1.00, -0.35) Very Dry, H1: [-0.35, -0.10) Dry, H2: [-0.10, 0.10) Medium
    // H3: [0.10, 0.30) Wet, H4: [0.30, 1.00] Very Wet
    int humidLevel = 2;  // Default: H2 (Medium)
    if (humidity < -0.35f)      humidLevel = 0;  // H0 (Very Dry)
    else if (humidity < -0.10f) humidLevel = 1;  // H1 (Dry)
    else if (humidity < 0.10f)  humidLevel = 2;  // H2 (Medium)
    else if (humidity < 0.30f)  humidLevel = 3;  // H3 (Wet)
    else                        humidLevel = 4;  // H4 (Very Wet)

    // Hierarchical biome selection (simplified from Minecraft's lookup tables)

    // Step 1: Check continentalness for ocean biomes
    if (continentalness < -0.19f)  // Ocean/Deep Ocean range
    {
        if (tempLevel == 0)  // T0 = Frozen
            return BiomeType::FROZEN_OCEAN;
        else if (continentalness < -1.05f)  // Deep ocean threshold
            return BiomeType::DEEP_OCEAN;
        else
            return BiomeType::OCEAN;
    }

    // Step 2: Beach biomes (coast areas)
    if (continentalness < -0.11f)  // Coast range
    {
        if (tempLevel == 0)  // T0 = Frozen
            return BiomeType::SNOWY_BEACH;
        else
            return BiomeType::BEACH;
    }

    // Step 3: Inland biomes - Use Peaks & Valleys and Erosion for categorization

    // Peaks biomes (high PV values)
    if (peaksValleys > 0.7f)
    {
        if (tempLevel <= 2)  // T0-T2 = Cold/neutral
            return BiomeType::SNOWY_PEAKS;
        else
            return BiomeType::STONY_PEAKS;
    }

    // Badland biomes (low erosion + dry/medium humidity)
    // CRITICAL FIX: Outer condition checks humidLevel <= 2, so inner condition was redundant
    // Now properly distinguishes between DESERT (very dry) and SAVANNA (dry to medium)
    if (erosion < -0.2225f && humidLevel <= 2)
    {
        if (humidLevel <= 1)  // H0-H1 = Very Dry to Dry
            return BiomeType::DESERT;
        else  // H2 = Medium
            return BiomeType::SAVANNA;
    }

    // Middle biomes (most common) - Use temperature and humidity
    if (tempLevel == 0)  // T0 = Frozen
    {
        if (humidLevel <= 1)
            return BiomeType::SNOWY_PLAINS;
        else if (humidLevel <= 2)
            return BiomeType::SNOWY_TAIGA;
        else
            return BiomeType::TAIGA;
    }
    else if (tempLevel == 1)  // T1 = Cool
    {
        if (humidLevel >= 2)
            return BiomeType::FOREST;
        else
            return BiomeType::PLAINS;
    }
    else if (tempLevel >= 3)  // T3-T4 = Warm/Hot
    {
        if (humidLevel >= 3)
            return BiomeType::JUNGLE;
        else if (humidLevel <= 2)
            return BiomeType::SAVANNA;
        else
            return BiomeType::PLAINS;
    }

    // Default: Plains (T2 Neutral temperature, medium conditions)
    return BiomeType::PLAINS;
}

//----------------------------------------------------------------------------------------------------
void Chunk::GenerateTerrain()
{
    // Establish world-space position and bounds of this chunk
    Vec3 chunkPosition((float)(m_chunkCoords.x) * CHUNK_SIZE_X, (float)(m_chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 chunkBounds = chunkPosition + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);

    // Phase 0, Task 0.4: Check for debug visualization mode
    // If in visualization mode, generate simple flat terrain showing noise values as colored blocks
    DebugVisualizationMode vizMode = DebugVisualizationMode::NORMAL_TERRAIN;
    if (g_game && g_game->GetWorld())
    {
        vizMode = g_game->GetWorld()->GetDebugVisualizationMode();
    }

    if (vizMode != DebugVisualizationMode::NORMAL_TERRAIN)
    {
        // Generate debug visualization terrain
        // Create a flat layer at Y=80 (sea level) showing the selected noise layer as colored blocks

        // Derive seed for the noise layer being visualized
        unsigned int visualizationSeed = GAME_SEED;
        switch (vizMode)
        {
            case DebugVisualizationMode::TEMPERATURE:    visualizationSeed = GAME_SEED + 2; break;
            case DebugVisualizationMode::HUMIDITY:       visualizationSeed = GAME_SEED + 1; break;
            case DebugVisualizationMode::CONTINENTALNESS: visualizationSeed = GAME_SEED + 5; break;
            case DebugVisualizationMode::EROSION:        visualizationSeed = GAME_SEED + 6; break;
            case DebugVisualizationMode::WEIRDNESS:      visualizationSeed = GAME_SEED + 7; break;
            case DebugVisualizationMode::PEAKS_VALLEYS:  visualizationSeed = GAME_SEED + 8; break;
            case DebugVisualizationMode::BIOME_TYPE:     visualizationSeed = GAME_SEED + 9; break;  // Phase 1, Task 1.4
            default: break;
        }

        // Fill blocks with visualization data
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int x = 0; x < CHUNK_SIZE_X; x++)
            {
                int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
                int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

                // Sample the appropriate noise layer based on visualization mode
                float noiseValue = 0.f;
                switch (vizMode)
                {
                    case DebugVisualizationMode::TEMPERATURE:
                    {
                        float rawTemp = Get2dNoiseNegOneToOne(globalX, globalY, visualizationSeed) * TEMPERATURE_RAW_NOISE_SCALE;
                        noiseValue = rawTemp + 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            TEMPERATURE_NOISE_SCALE,
                            TEMPERATURE_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        // Remap from [0,1] to [-1,1]
                        noiseValue = (noiseValue * 2.f) - 1.f;
                        break;
                    }

                    case DebugVisualizationMode::HUMIDITY:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            HUMIDITY_NOISE_SCALE,
                            HUMIDITY_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::CONTINENTALNESS:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            CONTINENTALNESS_NOISE_SCALE,
                            CONTINENTALNESS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::EROSION:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            EROSION_NOISE_SCALE,
                            EROSION_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::WEIRDNESS:
                        noiseValue = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            WEIRDNESS_NOISE_SCALE,
                            WEIRDNESS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        break;

                    case DebugVisualizationMode::PEAKS_VALLEYS:
                    {
                        // PV is derived from weirdness: PV = 1 - |( 3 * abs(W) ) - 2|
                        float weirdness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            WEIRDNESS_NOISE_SCALE,
                            WEIRDNESS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        noiseValue = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);
                        break;
                    }

                    case DebugVisualizationMode::BIOME_TYPE:
                    {
                        // Phase 1, Task 1.4: Sample all 6 biome noise layers and determine biome type
                        // CRITICAL FIX: Use the SAME seeds as normal terrain generation (lines 599-610)
                        // Otherwise visualization shows different biomes than actual terrain!

                        // Derive seeds matching normal terrain generation
                        unsigned int humiditySeed = GAME_SEED + 1;
                        unsigned int temperatureSeed = GAME_SEED + 2;
                        unsigned int continentalnessSeed = GAME_SEED + 6;
                        unsigned int erosionSeed = GAME_SEED + 7;
                        unsigned int weirdnessSeed = GAME_SEED + 8;

                        // Sample Temperature (matches Pass 1 lines 638-647)
                        float rawTemp = Get2dNoiseNegOneToOne(globalX, globalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
                        float temperature = rawTemp + 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            TEMPERATURE_NOISE_SCALE,
                            TEMPERATURE_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            temperatureSeed
                        );
                        // Normalize temperature from [0,1] to [-1,1] for biome selection
                        float temperatureNormalized = RangeMap(temperature, 0.f, 1.f, -1.f, 1.f);

                        // Sample Humidity (matches Pass 1 lines 627-635)
                        float humidity = 0.5f + 0.5f * Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            HUMIDITY_NOISE_SCALE,
                            HUMIDITY_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            humiditySeed
                        );
                        // Normalize humidity from [0,1] to [-1,1] for biome selection
                        float humidityNormalized = RangeMap(humidity, 0.f, 1.f, -1.f, 1.f);

                        // Sample Continentalness (matches Pass 1 lines 653-661)
                        float continentalness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            CONTINENTALNESS_NOISE_SCALE,
                            CONTINENTALNESS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            continentalnessSeed
                        );

                        // Sample Erosion (matches Pass 1 lines 664-672)
                        float erosion = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            EROSION_NOISE_SCALE,
                            EROSION_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            erosionSeed
                        );

                        // Sample Weirdness (matches Pass 1 lines 675-683)
                        float weirdness = Compute2dPerlinNoise(
                            (float)globalX, (float)globalY,
                            WEIRDNESS_NOISE_SCALE,
                            WEIRDNESS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            weirdnessSeed
                        );

                        // Calculate Peaks & Valleys from Weirdness (matches Pass 1 line 687)
                        float peaksValleys = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);

                        // Determine biome type using lookup table (from Task 1.3)
                        BiomeType biome = SelectBiome(temperatureNormalized, humidityNormalized,
                                                       continentalness, erosion, peaksValleys);

                        // Cast biome enum to float for color mapping
                        noiseValue = static_cast<float>(static_cast<int>(biome));
                        break;
                    }

                    default: break;
                }

                // Get the appropriate colored block for this noise value
                uint8_t visualizationBlock = GetDebugVisualizationBlock(noiseValue, vizMode);

                // Create a flat visualization surface at Y=80 (sea level)
                // Phase 0, Task 0.5: Updated to match new sea level (was Y=64 for 128-block chunks)
                constexpr int VISUALIZATION_HEIGHT = 80;

                // Fill bottom with stone (foundation)
                for (int z = 0; z < VISUALIZATION_HEIGHT; z++)
                {
                    SetBlock(x, y, z, BLOCK_STONE);
                }

                // Place the visualization block on top
                SetBlock(x, y, VISUALIZATION_HEIGHT, visualizationBlock);

                // Leave everything above as air
                for (int z = VISUALIZATION_HEIGHT + 1; z < CHUNK_SIZE_Z; z++)
                {
                    SetBlock(x, y, z, BLOCK_AIR);
                }
            }
        }

        // Mark chunk as needing mesh rebuild and dirty save
        SetIsMeshDirty(true);
        SetNeedsSaving(true);
        return;  // Skip normal terrain generation
    }

    // Normal terrain generation continues below...

    // Derive deterministic seeds for each noise channel
    unsigned int terrainSeed     = GAME_SEED;
    unsigned int humiditySeed    = GAME_SEED + 1;
    unsigned int temperatureSeed = humiditySeed + 1;
    unsigned int hillSeed        = temperatureSeed + 1;
    unsigned int oceanSeed       = hillSeed + 1;
    unsigned int dirtSeed        = oceanSeed + 1;

    // Assignment 4: Biome noise seeds (Phase 1, Task 1.3)
    unsigned int continentalnessSeed = dirtSeed + 1;
    unsigned int erosionSeed         = continentalnessSeed + 1;
    unsigned int weirdnessSeed       = erosionSeed + 1;

    // Allocate per-(x,y) maps
    int   heightMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    int   dirtDepthXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    float humidityMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    float temperatureMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];

    // --- Pass 1: compute surface & biome fields per (x,y) pillar ---
    for (int y = 0; y < CHUNK_SIZE_Y; y++)
    {
        for (int x = 0; x < CHUNK_SIZE_X; x++)
        {
            int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
            int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

            // Humidity calculation (0.5 + 0.5 * Perlin2D(...))
            float humidity = 0.5f + 0.5f * Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                HUMIDITY_NOISE_SCALE,
                HUMIDITY_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                humiditySeed
            );

            // Temperature calculation (raw noise + Perlin)
            float temperature = Get2dNoiseNegOneToOne(globalX, globalY, temperatureSeed) * TEMPERATURE_RAW_NOISE_SCALE;
            temperature       = temperature + 0.5f + 0.5f * Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                TEMPERATURE_NOISE_SCALE,
                TEMPERATURE_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                temperatureSeed
            );

            // Assignment 4: Biome noise sampling (Phase 1, Task 1.3)
            // Sample 4 additional noise layers for biome determination

            // Continentalness - Ocean to inland distance (C: [-1.2, 1.0])
            float continentalness = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                CONTINENTALNESS_NOISE_SCALE,
                CONTINENTALNESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                continentalnessSeed
            );

            // Erosion - Flat to mountainous (E: [-1, 1])
            float erosion = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                EROSION_NOISE_SCALE,
                EROSION_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                erosionSeed
            );

            // Weirdness - Terrain variation (W: [-1, 1])
            float weirdness = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                WEIRDNESS_NOISE_SCALE,
                WEIRDNESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize to [-1, 1]
                weirdnessSeed
            );

            // Peaks & Valleys - Calculated from Weirdness (PV: [-1, 1])
            // Formula: PV = 1 - |(3 * abs(W)) - 2|
            float peaksValleys = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);

            // Hilliness calculation
            float rawHill = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                HILLINESS_NOISE_SCALE,
                HILLINESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                hillSeed
            );
            float hill = SmoothStep3(RangeMap(rawHill, -1.f, 1.f, 0.f, 1.f));

            // Ocean calculation
            float ocean = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                OCEANESS_NOISE_SCALE,
                OCEANESS_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                oceanSeed
            );

            // Terrain height calculation
            float rawTerrain = Compute2dPerlinNoise(
                (float)globalX, (float)globalY,
                TERRAIN_NOISE_SCALE,
                TERRAIN_NOISE_OCTAVES,
                DEFAULT_OCTAVE_PERSISTANCE,
                DEFAULT_NOISE_OCTAVE_SCALE,
                true,  // renormalize
                terrainSeed
            );

            // Base terrain height with river/hill shaping
            float terrainHeightF = DEFAULT_TERRAIN_HEIGHT + hill * RangeMap(fabsf(rawTerrain), 0.f, 1.f, -RIVER_DEPTH, DEFAULT_TERRAIN_HEIGHT);

            // Ocean depressions
            if (ocean > OCEAN_START_THRESHOLD)
            {
                float oceanBlend = RangeMapClamped(ocean, OCEAN_START_THRESHOLD, OCEAN_END_THRESHOLD, 0.f, 1.f);
                terrainHeightF   = terrainHeightF - Interpolate(0.f, OCEAN_DEPTH, oceanBlend);
            }

            // Dirt layer thickness driven by noise
            float dirtDepthPct = Get2dNoiseZeroToOne(globalX, globalY, dirtSeed);
            int   dirtDepth    = MIN_DIRT_OFFSET_Z + (int)roundf(dirtDepthPct * (float)(MAX_DIRT_OFFSET_Z - MIN_DIRT_OFFSET_Z));

            int idxXY               = y * CHUNK_SIZE_X + x;
            humidityMapXY[idxXY]    = humidity;
            temperatureMapXY[idxXY] = temperature;
            heightMapXY[idxXY]      = (int)floorf(terrainHeightF);
            dirtDepthXY[idxXY]      = dirtDepth;

            // Assignment 4: Store biome data for this column (Phase 1, Task 1.3)
            // Note: Temperature and Humidity ranges need adjustment from [0,1] to [-1,1]
            // Current calculation produces [0,1], but biome selection expects [-1,1]
            float temperatureNormalized = RangeMap(temperature, 0.f, 1.f, -1.f, 1.f);
            float humidityNormalized    = RangeMap(humidity, 0.f, 1.f, -1.f, 1.f);

            // Select biome type using lookup table approach
            BiomeType biomeType = SelectBiome(temperatureNormalized, humidityNormalized,
                                             continentalness, erosion, peaksValleys);

            // Store all biome data
            m_biomeData[idxXY].temperature     = temperatureNormalized;
            m_biomeData[idxXY].humidity        = humidityNormalized;
            m_biomeData[idxXY].continentalness = continentalness;
            m_biomeData[idxXY].erosion         = erosion;
            m_biomeData[idxXY].weirdness       = weirdness;
            m_biomeData[idxXY].peaksValleys    = peaksValleys;
            m_biomeData[idxXY].biomeType       = biomeType;
        }
    }

    // --- Pass 2: assign block types for every (x,y,z) using 3D density formula ---
    // Assignment 4: Phase 2, Task 2.1 - Replace heightmap with 3D density terrain
    // Formula: D(x,y,z) = N(x,y,z,s) + B(z)
    // where N = 3D Perlin noise, B = vertical bias
    // CRITICAL: Negative density = MORE dense (solid blocks), positive = air

    for (int z = 0; z < CHUNK_SIZE_Z; z++)
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++)
        {
            for (int x = 0; x < CHUNK_SIZE_X; x++)
            {
                IntVec3 localCoords(x, y, z);
                IntVec3 globalCoords = GetGlobalCoords(m_chunkCoords, localCoords);
                int     idx          = LocalCoordsToIndex(localCoords);
                int     idxXY        = y * CHUNK_SIZE_X + x;

                // Retrieve cached per-column data from Pass 1
                int   dirtDepth     = dirtDepthXY[idxXY];
                float humidity      = humidityMapXY[idxXY];
                float temperature   = temperatureMapXY[idxXY];

                // --- Assignment 4: 3D Density Formula (Phase 2, Task 2.1) ---

                // Sample 3D noise at this global position
                // CRITICAL: Use global coordinates for cross-chunk consistency
                // Use Compute3dPerlinNoise for smooth continuous interpolated noise
                // Returns [-1, 1], scale controls frequency, octaves add detail
                unsigned int densitySeed = GAME_SEED + 10; // Separate seed for density noise
                float noise = Compute3dPerlinNoise(
                    (float)globalCoords.x,
                    (float)globalCoords.y,
                    (float)globalCoords.z,
                    DENSITY_NOISE_SCALE,           // Scale: 200.0 (lower freq = smoother terrain)
                    DENSITY_NOISE_OCTAVES,         // Octaves: 3 (adds fractal detail)
                    DEFAULT_OCTAVE_PERSISTANCE,    // Persistence: 0.5 (amplitude falloff)
                    DEFAULT_NOISE_OCTAVE_SCALE,    // Octave scale: 2.0 (frequency multiplier)
                    true,                          // Renormalize to [-1, 1]
                    densitySeed
                ); // Returns N(x,y,z,s) in [-1, 1]

                // Calculate vertical bias: B(z) = b × (z − t)
                // Higher blocks (z > DEFAULT_TERRAIN_HEIGHT) get positive bias (more air)
                // Lower blocks (z < DEFAULT_TERRAIN_HEIGHT) get negative bias (more solid)
                float bias = DENSITY_BIAS_PER_BLOCK * (float)(globalCoords.z - (int)DEFAULT_TERRAIN_HEIGHT);

                // --- Assignment 4: Top and Bottom Slides (Phase 2, Task 2.2) ---

                // Top slide: Smooth transition near surface (z=100-120)
                // Forces density toward positive (air) near world top to prevent sharp cutoffs
                float topSlide = 0.0f;
                if (globalCoords.z >= TOP_SLIDE_START && globalCoords.z <= TOP_SLIDE_END)
                {
                    // Calculate slide progress: 0.0 at start, 1.0 at end
                    float slideProgress = (float)(globalCoords.z - TOP_SLIDE_START) / (float)(TOP_SLIDE_END - TOP_SLIDE_START);

                    // Use SmoothStep3 for smooth transition curve
                    float smoothedProgress = SmoothStep3(slideProgress);

                    // Apply positive bias to force terrain toward air
                    // Strength increases as we approach world top
                    topSlide = smoothedProgress * 2.0f; // Range: 0.0 to 2.0 (positive = more air)
                }

                // Bottom slide: Flatten terrain near bedrock (z=0-20)
                // Forces density toward negative (solid) near world bottom for stable base
                float bottomSlide = 0.0f;
                if (globalCoords.z >= BOTTOM_SLIDE_START && globalCoords.z <= BOTTOM_SLIDE_END)
                {
                    // Calculate slide progress: 1.0 at bottom, 0.0 at end
                    float slideProgress = 1.0f - ((float)(globalCoords.z - BOTTOM_SLIDE_START) / (float)(BOTTOM_SLIDE_END - BOTTOM_SLIDE_START));

                    // Use SmoothStep3 for smooth transition curve
                    float smoothedProgress = SmoothStep3(slideProgress);

                    // Apply negative bias to force terrain toward solid
                    // Strength increases as we approach bedrock
                    bottomSlide = -smoothedProgress * 3.0f; // Range: -3.0 to 0.0 (negative = more solid)
                }

                // --- Assignment 4: Terrain Shaping Curves (Phase 2, Task 2.3) ---

                // Retrieve biome data for this column (from Pass 1)
                BiomeData& biomeData = m_biomeData[idxXY];

                // Continentalness Curve: Height offset based on ocean/inland distance
                // Maps C: [-1.2, 1.0] → Height offset: [-30, +40]
                // Ocean areas get negative offset (deeper), inland gets positive (higher)
                float continentalnessOffset = RangeMap(biomeData.continentalness,
                                                       -1.2f, 1.0f,
                                                       CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);

                // Erosion Curve: Terrain wildness/scale based on flat/mountainous
                // Maps E: [-1, 1] → Scale multiplier: [0.3, 2.5]
                // Flat terrain (low E) gets less noise amplification
                // Mountainous (high E) gets more noise amplification
                float erosionScale = RangeMap(biomeData.erosion,
                                              -1.0f, 1.0f,
                                              EROSION_SCALE_MIN, EROSION_SCALE_MAX);

                // Peaks & Valleys Curve: Additional height variation
                // Maps PV: [-1, 1] → Height modifier: [-15, +25]
                // Valleys (low PV) get negative modifier, peaks (high PV) get positive
                float pvOffset = RangeMap(biomeData.peaksValleys,
                                         -1.0f, 1.0f,
                                         PV_HEIGHT_MIN, PV_HEIGHT_MAX);

                // Apply terrain shaping:
                // 1. Calculate total height offset from biome parameters
                float heightOffset = continentalnessOffset + pvOffset;

                // 2. Calculate effective terrain height for this location
                //    - Start with DEFAULT_TERRAIN_HEIGHT (80)
                //    - Add continentalness and PV height offsets
                float effectiveTerrainHeight = (float)DEFAULT_TERRAIN_HEIGHT + heightOffset;

                // 3. Calculate bias relative to the SHAPED terrain height (not default height)
                //    - This makes terrain follow the biome-specific height curves
                float shapedBias = DENSITY_BIAS_PER_BLOCK * (float)((float)globalCoords.z - effectiveTerrainHeight);

                // 4. Scale noise by erosion factor (controls terrain wildness)
                float shapedNoise = noise * erosionScale;

                // Combine noise, shaped bias, and slides to get final density
                // D(x,y,z) = N(x,y,z,s)*erosionScale + B_shaped(z) + topSlide(z) + bottomSlide(z)
                float density = shapedNoise + shapedBias + topSlide + bottomSlide;

                // Density threshold: negative = solid, positive = air
                bool isSolid = (density < 0.0f);

                // --- Block Type Assignment ---

                uint8_t blockType = BLOCK_AIR; // Default to air

                if (isSolid)
                {
                    // --- Solid terrain blocks ---

                    // Special bottom layers (always placed regardless of density)
                    if (globalCoords.z == OBSIDIAN_Z)
                    {
                        blockType = BLOCK_OBSIDIAN;
                    }
                    else if (globalCoords.z == LAVA_Z)
                    {
                        blockType = BLOCK_LAVA;
                    }
                    else
                    {
                        // Default solid block is stone, with ore generation
                        float oreNoise = Get3dNoiseZeroToOne(globalCoords.x, globalCoords.y, globalCoords.z, GAME_SEED);
                        if (oreNoise < DIAMOND_CHANCE)
                        {
                            blockType = BLOCK_DIAMOND;
                        }
                        else if (oreNoise < GOLD_CHANCE)
                        {
                            blockType = BLOCK_GOLD;
                        }
                        else if (oreNoise < IRON_CHANCE)
                        {
                            blockType = BLOCK_IRON;
                        }
                        else if (oreNoise < COAL_CHANCE)
                        {
                            blockType = BLOCK_COAL;
                        }
                        else
                        {
                            blockType = BLOCK_STONE;

                            // --- Assignment 4: Phase 2, Task 2.4 - Surface Block Replacement ---
                            // Check if this block is at the surface (has air above it)
                            bool isSurface = false;
                            if (globalCoords.z < CHUNK_SIZE_Z - 1) // Not at world top
                            {
                                // Calculate density of block above this one
                                IntVec3 aboveCoords(globalCoords.x, globalCoords.y, globalCoords.z + 1);
                                unsigned int aboveDensitySeed = GAME_SEED + 10;
                                float aboveNoise = Compute3dPerlinNoise(
                                    (float)aboveCoords.x,
                                    (float)aboveCoords.y,
                                    (float)aboveCoords.z,
                                    DENSITY_NOISE_SCALE,
                                    DENSITY_NOISE_OCTAVES,
                                    DEFAULT_OCTAVE_PERSISTANCE,
                                    DEFAULT_NOISE_OCTAVE_SCALE,
                                    true,
                                    aboveDensitySeed
                                );

                                // Get biome data for this column (same as current block)
                                BiomeData& aboveBiomeData = m_biomeData[idxXY];

                                // Apply terrain shaping to above block
                                float aboveContinentalnessOffset = RangeMap(aboveBiomeData.continentalness,
                                                                           -1.2f, 1.0f,
                                                                           CONTINENTALNESS_HEIGHT_MIN, CONTINENTALNESS_HEIGHT_MAX);
                                float aboveErosionScale = RangeMap(aboveBiomeData.erosion,
                                                                   -1.0f, 1.0f,
                                                                   EROSION_SCALE_MIN, EROSION_SCALE_MAX);
                                float abovePvOffset = RangeMap(aboveBiomeData.peaksValleys,
                                                               -1.0f, 1.0f,
                                                               PV_HEIGHT_MIN, PV_HEIGHT_MAX);

                                // Calculate effective terrain height for above block (same as current block)
                                float aboveHeightOffset = aboveContinentalnessOffset + abovePvOffset;
                                float aboveEffectiveTerrainHeight = (float)DEFAULT_TERRAIN_HEIGHT + aboveHeightOffset;

                                // Calculate shaped bias relative to effective terrain height
                                float aboveShapedBias = DENSITY_BIAS_PER_BLOCK * (float)((float)aboveCoords.z - aboveEffectiveTerrainHeight);

                                // Scale noise by erosion factor
                                float aboveShapedNoise = aboveNoise * aboveErosionScale;

                                // Apply slides to above block
                                float aboveTopSlide = 0.0f;
                                if (aboveCoords.z >= TOP_SLIDE_START && aboveCoords.z <= TOP_SLIDE_END)
                                {
                                    float slideProgress = (float)(aboveCoords.z - TOP_SLIDE_START) / (float)(TOP_SLIDE_END - TOP_SLIDE_START);
                                    float smoothedProgress = SmoothStep3(slideProgress);
                                    aboveTopSlide = smoothedProgress * 2.0f;
                                }

                                float aboveBottomSlide = 0.0f;
                                if (aboveCoords.z >= BOTTOM_SLIDE_START && aboveCoords.z <= BOTTOM_SLIDE_END)
                                {
                                    float slideProgress = 1.0f - ((float)(aboveCoords.z - BOTTOM_SLIDE_START) / (float)(BOTTOM_SLIDE_END - BOTTOM_SLIDE_START));
                                    float smoothedProgress = SmoothStep3(slideProgress);
                                    aboveBottomSlide = -smoothedProgress * 3.0f;
                                }

                                // Combine components for above block density
                                float aboveDensity = aboveShapedNoise + aboveShapedBias + aboveTopSlide + aboveBottomSlide;
                                isSurface = (aboveDensity >= 0.0f); // Above block is air/water
                            }

                            // Apply biome-specific surface blocks
                            if (isSurface)
                            {
                                BiomeType biome = m_biomeData[idxXY].biomeType;

                                switch (biome)
                                {
                                    // Ocean biomes - sandy ocean floor or gravel
                                    case BiomeType::OCEAN:
                                    case BiomeType::DEEP_OCEAN:
                                    case BiomeType::FROZEN_OCEAN:
                                        blockType = BLOCK_SAND; // Sandy ocean floor
                                        break;

                                    // Beach biomes - sand
                                    case BiomeType::BEACH:
                                    case BiomeType::SNOWY_BEACH:
                                        blockType = BLOCK_SAND;
                                        break;

                                    // Desert biome - sand with possible dirt underneath
                                    case BiomeType::DESERT:
                                        blockType = BLOCK_SAND;
                                        break;

                                    // Savanna biome - dirt with grassy surface
                                    case BiomeType::SAVANNA:
                                        blockType = BLOCK_GRASS_YELLOW; // Dry yellow grass
                                        break;

                                    // Plains biomes - standard grass
                                    case BiomeType::PLAINS:
                                        blockType = BLOCK_GRASS;
                                        break;

                                    // Snowy biomes - snow or ice blocks
                                    case BiomeType::SNOWY_PLAINS:
                                        blockType = BLOCK_SNOW; // Snow surface
                                        break;

                                    // Forest biomes - grass with different shades
                                    case BiomeType::FOREST:
                                        blockType = BLOCK_GRASS_DARK; // Dark green grass
                                        break;

                                    case BiomeType::JUNGLE:
                                        blockType = BLOCK_GRASS_LIGHT; // Light tropical grass
                                        break;

                                    // Taiga biomes - grass with snow possibilities
                                    case BiomeType::TAIGA:
                                        blockType = BLOCK_GRASS; // Standard grass
                                        break;

                                    case BiomeType::SNOWY_TAIGA:
                                        blockType = BLOCK_SNOW; // Snow-covered grass
                                        break;

                                    // Mountain peaks - stone or snow
                                    case BiomeType::STONY_PEAKS:
                                        blockType = BLOCK_COBBLESTONE; // Rocky mountain surface
                                        break;

                                    case BiomeType::SNOWY_PEAKS:
                                        blockType = BLOCK_SNOW; // Snowy mountain peaks
                                        break;

                                    default:
                                        blockType = BLOCK_GRASS; // Default to grass
                                        break;
                                }

                                // Special temperature-based surface modifications
                                float surfaceTemp = m_biomeData[idxXY].temperature;
                                if (surfaceTemp <= ICE_TEMPERATURE_MAX && globalCoords.z <= ICE_DEPTH_MAX)
                                {
                                    // Very cold areas get ice formation near surface
                                    if (blockType == BLOCK_WATER || (blockType == BLOCK_SAND && globalCoords.z < SEA_LEVEL_Z))
                                    {
                                        blockType = BLOCK_ICE;
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    // --- Air/fluid blocks ---

                    // Fill below sea level with water
                    if (globalCoords.z < SEA_LEVEL_Z)
                    {
                        blockType = BLOCK_WATER;

                        // Temperature-driven ice ceiling
                        float iceDepth = DEFAULT_TERRAIN_HEIGHT - floorf(RangeMapClamped(temperature,
                                                                                         ICE_TEMPERATURE_MAX, ICE_TEMPERATURE_MIN,
                                                                                         ICE_DEPTH_MIN, ICE_DEPTH_MAX));
                        if (temperature < 0.38f && globalCoords.z > iceDepth)
                        {
                            blockType = BLOCK_ICE;
                        }
                    }
                    // else: remains BLOCK_AIR
                }

                m_blocks[idx].m_typeIndex = blockType;
            }
        }
    }

    // Mark mesh as needing regeneration
    m_isMeshDirty = true;
}

//----------------------------------------------------------------------------------------------------
void Chunk::RebuildMesh()
{
    // Clear existing mesh data
    m_vertices.clear();
    m_indices.clear();

    // Cache-coherent iteration: iterate blocks in memory order for optimal cache performance
    // Memory layout is: index = x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y))
    for (int blockIndex = 0; blockIndex < BLOCKS_PER_CHUNK; blockIndex++)
    {
        Block&            block = m_blocks[blockIndex];
        sBlockDefinition* def   = sBlockDefinition::GetDefinitionByIndex(block.m_typeIndex);

        // Skip invisible blocks (air, transparent blocks)
        if (!def || !def->IsVisible()) continue;

        // Create block iterator for efficient neighbor access
        BlockIterator blockIter(this, blockIndex);

        // Use hidden surface removal for only visible faces
        AddBlockFacesWithHiddenSurfaceRemoval(blockIter, def);
    }

    // Add debug wireframe for chunk bounds
    AddVertsForWireframeAABB3D(m_debugVertices, m_worldBounds, 0.1f);

    // Update GPU buffers from CPU arrays
    UpdateVertexBuffer();

    // Mark mesh as no longer dirty
    m_isMeshDirty = false;
}

//----------------------------------------------------------------------------------------------------
Block* Chunk::GetBlock(int const localBlockIndexX,
                       int const localBlockIndexY,
                       int const localBlockIndexZ)
{
    if (localBlockIndexX < 0 || localBlockIndexX > CHUNK_MAX_X ||
        localBlockIndexY < 0 || localBlockIndexY > CHUNK_MAX_Y ||
        localBlockIndexZ < 0 || localBlockIndexZ > CHUNK_MAX_Z)
    {
        return nullptr;
    }

    int const index = LocalCoordsToIndex(localBlockIndexX, localBlockIndexY, localBlockIndexZ);

    return &m_blocks[index];
}

//----------------------------------------------------------------------------------------------------
void Chunk::SetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ, uint8_t blockTypeIndex)
{
    // SAFETY CHECK: Validate chunk is in a valid state before modification
    // This prevents writing to chunks being deleted/deactivated during RegenerateAllChunks()
    ChunkState currentState = m_state.load();
    if (currentState != ChunkState::COMPLETE &&
        currentState != ChunkState::TERRAIN_GENERATING)
    {
        // Chunk is being deleted or not fully initialized - do not modify
        return;
    }

    // Validate coordinates
    if (localBlockIndexX < 0 || localBlockIndexX > CHUNK_MAX_X ||
        localBlockIndexY < 0 || localBlockIndexY > CHUNK_MAX_Y ||
        localBlockIndexZ < 0 || localBlockIndexZ > CHUNK_MAX_Z)
    {
        return; // Invalid coordinates
    }

    // Calculate block index
    int index = LocalCoordsToIndex(localBlockIndexX, localBlockIndexY, localBlockIndexZ);

    // Check if block type is actually changing
    if (m_blocks[index].m_typeIndex != blockTypeIndex)
    {
        // Set the new block type
        m_blocks[index].m_typeIndex = blockTypeIndex;

        // Mark chunk as modified - needs saving and mesh regeneration
        SetNeedsSaving(true);
        SetIsMeshDirty(true);
    }
}

//----------------------------------------------------------------------------------------------------
// Private helper methods
//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords)
{
    UNUSED(coords)
    // For now, add all 6 faces - you can optimize with face culling later
    AddBlockFace(blockCenter, Vec3::Z_BASIS, def->GetTopUVs(), Rgba8::WHITE);        // Top
    AddBlockFace(blockCenter, -Vec3::Z_BASIS, def->GetBottomUVs(), Rgba8::WHITE);       // Bottom
    AddBlockFace(blockCenter, Vec3::X_BASIS, def->GetSideUVs(), Rgba8(230, 230, 230)); // East
    AddBlockFace(blockCenter, -Vec3::X_BASIS, def->GetSideUVs(), Rgba8(230, 230, 230)); // West
    AddBlockFace(blockCenter, Vec3::Y_BASIS, def->GetSideUVs(), Rgba8(200, 200, 200)); // North
    AddBlockFace(blockCenter, -Vec3::Y_BASIS, def->GetSideUVs(), Rgba8(200, 200, 200)); // South
}

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFace(Vec3 const&  blockCenter,
                         Vec3 const&  faceNormal,
                         Vec2 const&  uvs,
                         Rgba8 const& tint)
{
    Vec3 right, up;
    faceNormal.GetOrthonormalBasis(faceNormal, &right, &up);

    Vec3 faceCenter = blockCenter + faceNormal * 0.5f;

    // Convert sprite coordinates to UV coordinates
    // Assuming 8x8 sprite atlas (64 total sprites in an 8x8 grid)
    constexpr float ATLAS_SIZE  = 8.f;  // 8x8 grid of sprites
    constexpr float SPRITE_SIZE = 1.f / ATLAS_SIZE;  // Each sprite is 1/8 of the texture

    Vec2 const  uvMins    = uvs;
    Vec2 const  uvMaxs    = uvs + Vec2::ONE;
    Vec2 const  realMins  = Vec2(uvMins.x, uvMaxs.y) * SPRITE_SIZE;
    Vec2 const  realMaxs  = Vec2(uvMaxs.x, uvMins.y) * SPRITE_SIZE;
    AABB2 const spriteUVs = AABB2(Vec2(realMins.x, 1.f - realMins.y), Vec2(realMaxs.x, 1.f - realMaxs.y));

    // Add vertices using the corrected UV coordinates
    AddVertsForQuad3D(m_vertices, m_indices,
                      faceCenter - right * 0.5f - up * 0.5f,    // Bottom Left
                      faceCenter + right * 0.5f - up * 0.5f,    // Bottom Right
                      faceCenter - right * 0.5f + up * 0.5f,    // Top Left
                      faceCenter + right * 0.5f + up * 0.5f,    // Top Right
                      tint, spriteUVs);
}

//----------------------------------------------------------------------------------------------------
void Chunk::UpdateVertexBuffer()
{
    if (m_vertices.empty()) return;

    // CRITICAL FIX: Use atomic buffer swapping to prevent rendering race condition
    // Create new buffers FIRST, then swap atomically to prevent flashing during buffer updates

    // Create new main vertex buffer
    VertexBuffer* newVertexBuffer = g_renderer->CreateVertexBuffer(
        (int)m_vertices.size() * sizeof(Vertex_PCU),  // Total buffer size in bytes
        sizeof(Vertex_PCU)                       // Size of each vertex
    );
    g_renderer->CopyCPUToGPU(m_vertices.data(),
                             (unsigned int)(m_vertices.size() * sizeof(Vertex_PCU)),
                             newVertexBuffer);

    // Create new index buffer
    IndexBuffer* newIndexBuffer = g_renderer->CreateIndexBuffer(
        (int)m_indices.size() * sizeof(unsigned int),  // Total buffer size in bytes
        sizeof(unsigned int)                      // Size of each index
    );
    g_renderer->CopyCPUToGPU(m_indices.data(),
                             (unsigned int)(m_indices.size() * sizeof(unsigned int)),
                             newIndexBuffer);

    // Create new debug buffer if needed
    VertexBuffer* newDebugVertexBuffer = nullptr;
    if (!m_debugVertices.empty())
    {
        newDebugVertexBuffer = g_renderer->CreateVertexBuffer(
            (int)m_debugVertices.size() * sizeof(Vertex_PCU),  // Total buffer size
            sizeof(Vertex_PCU)                            // Size of each vertex
        );
        g_renderer->CopyCPUToGPU(m_debugVertices.data(),
                                 (unsigned int)(m_debugVertices.size() * sizeof(Vertex_PCU)),
                                 newDebugVertexBuffer);
    }

    // Store old buffers for deletion
    VertexBuffer* oldVertexBuffer = m_vertexBuffer;
    IndexBuffer*  oldIndexBuffer  = m_indexBuffer;
    VertexBuffer* oldDebugBuffer  = m_debugVertexBuffer;

    // ATOMIC SWAP: Replace all buffer pointers simultaneously
    // This prevents Render() from seeing inconsistent buffer state
    m_vertexBuffer      = newVertexBuffer;
    m_indexBuffer       = newIndexBuffer;
    m_debugVertexBuffer = newDebugVertexBuffer;

    // Delete old buffers AFTER the swap
    // Render() now uses new buffers, safe to delete old ones
    GAME_SAFE_RELEASE(oldVertexBuffer);
    GAME_SAFE_RELEASE(oldIndexBuffer);
    GAME_SAFE_RELEASE(oldDebugBuffer);
}

void Chunk::SetMeshClean()
{
    m_isMeshDirty = false;
}

//----------------------------------------------------------------------------------------------------
void Chunk::SetMeshData(VertexList_PCU const& vertices, IndexList const& indices,
                        VertexList_PCU const& debugVertices, IndexList const& debugIndices)
{
    // This method is called by ChunkMeshJob on the main thread to apply
    // mesh data that was generated on worker threads
    m_vertices = vertices;
    m_indices = indices;
    m_debugVertices = debugVertices;
    m_debugIndices = debugIndices;

    // The chunk mesh is now dirty and needs GPU buffer updates
    // This will be handled by the main thread calling UpdateVertexBuffer()
    m_isMeshDirty = true;
}

//----------------------------------------------------------------------------------------------------
// Static Utility Functions
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
int Chunk::LocalCoordsToIndex(const IntVec3& localCoords)
{
    return LocalCoordsToIndex(localCoords.x, localCoords.y, localCoords.z);
}

//----------------------------------------------------------------------------------------------------
int Chunk::LocalCoordsToIndex(int x, int y, int z)
{
    return x + (y << CHUNK_BITS_X) + (z << (CHUNK_BITS_X + CHUNK_BITS_Y));
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalX(int index)
{
    return index & CHUNK_MASK_X;
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalY(int index)
{
    return (index & CHUNK_MASK_Y) >> CHUNK_BITS_X;
}

//----------------------------------------------------------------------------------------------------
int Chunk::IndexToLocalZ(int index)
{
    return (index & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::IndexToLocalCoords(int index)
{
    return IntVec3(IndexToLocalX(index), IndexToLocalY(index), IndexToLocalZ(index));
}

//----------------------------------------------------------------------------------------------------
int Chunk::GlobalCoordsToIndex(const IntVec3& globalCoords)
{
    return GlobalCoordsToIndex(globalCoords.x, globalCoords.y, globalCoords.z);
}

//----------------------------------------------------------------------------------------------------
int Chunk::GlobalCoordsToIndex(int x, int y, int z)
{
    IntVec3 localCoords = GlobalCoordsToLocalCoords(IntVec3(x, y, z));
    return LocalCoordsToIndex(localCoords);
}

//----------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCoords(const IntVec3& globalCoords)
{
    // Use proper integer division with floor behavior for negative coordinates
    int chunkX = static_cast<int>(floorf(static_cast<float>(globalCoords.x) / CHUNK_SIZE_X));
    int chunkY = static_cast<int>(floorf(static_cast<float>(globalCoords.y) / CHUNK_SIZE_Y));

    return IntVec2(chunkX, chunkY);
}

//----------------------------------------------------------------------------------------------------
IntVec2 Chunk::GetChunkCenter(const IntVec2& chunkCoords)
{
    return IntVec2(
        chunkCoords.x * CHUNK_SIZE_X + CHUNK_SIZE_X / 2,
        chunkCoords.y * CHUNK_SIZE_Y + CHUNK_SIZE_Y / 2
    );
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GlobalCoordsToLocalCoords(const IntVec3& globalCoords)
{
    // Use modulo operation for local coordinates with proper handling of negative numbers
    int localX = globalCoords.x % CHUNK_SIZE_X;
    int localY = globalCoords.y % CHUNK_SIZE_Y;
    int localZ = globalCoords.z;

    // Handle negative modulo results (C++ modulo can return negative values)
    if (localX < 0) localX += CHUNK_SIZE_X;
    if (localY < 0) localY += CHUNK_SIZE_Y;

    return IntVec3(localX, localY, localZ);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const IntVec2& chunkCoords, int blockIndex)
{
    IntVec3 localCoords = IndexToLocalCoords(blockIndex);
    return GetGlobalCoords(chunkCoords, localCoords);
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const IntVec2& chunkCoords, const IntVec3& localCoords)
{
    return IntVec3(
        chunkCoords.x * CHUNK_SIZE_X + localCoords.x,
        chunkCoords.y * CHUNK_SIZE_Y + localCoords.y,
        localCoords.z
    );
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetGlobalCoords(const Vec3& position)
{
    return IntVec3(
        static_cast<int>(position.x),
        static_cast<int>(position.y),
        static_cast<int>(position.z)
    );
}

//----------------------------------------------------------------------------------------------------
// Neighbor Chunk Management
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void Chunk::SetNeighborChunks(Chunk* north, Chunk* south, Chunk* east, Chunk* west)
{
    m_northNeighbor = north;
    m_southNeighbor = south;
    m_eastNeighbor  = east;
    m_westNeighbor  = west;
}

//----------------------------------------------------------------------------------------------------
void Chunk::ClearNeighborPointers()
{
    m_northNeighbor = nullptr;
    m_southNeighbor = nullptr;
    m_eastNeighbor  = nullptr;
    m_westNeighbor  = nullptr;
}

//----------------------------------------------------------------------------------------------------
// Advanced Mesh Generation with Hidden Surface Removal
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesWithHiddenSurfaceRemoval(BlockIterator const& blockIter,
                                                  sBlockDefinition*    def)
{
    IntVec3 localCoords = blockIter.GetLocalCoords();
    Vec3    blockCenter = Vec3((float)localCoords.x + 0.5f, (float)localCoords.y + 0.5f, (float)localCoords.z + 0.5f) +
    Vec3(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y, 0);

    // Check each face direction and only add visible faces
    // Face directions as offset vectors
    IntVec3 faceDirections[6] = {
        IntVec3(0, 0, 1),   // Top face (+Z)
        IntVec3(0, 0, -1),  // Bottom face (-Z)
        IntVec3(1, 0, 0),   // East face (+X)
        IntVec3(-1, 0, 0),  // West face (-X)
        IntVec3(0, 1, 0),   // North face (+Y)
        IntVec3(0, -1, 0)   // South face (-Y)
    };

    Vec3 faceNormals[6] = {
        Vec3::Z_BASIS,      // Top
        -Vec3::Z_BASIS,     // Bottom
        Vec3::X_BASIS,      // East
        -Vec3::X_BASIS,     // West
        Vec3::Y_BASIS,      // North
        -Vec3::Y_BASIS      // South
    };

    Rgba8 faceTints[6] = {
        Rgba8::WHITE,               // Top
        Rgba8::WHITE,               // Bottom
        Rgba8(230, 230, 230),       // East
        Rgba8(230, 230, 230),       // West
        Rgba8(200, 200, 200),       // North
        Rgba8(200, 200, 200)        // South
    };

    // For each face, check if it's visible before adding it
    for (int faceIndex = 0; faceIndex < 6; faceIndex++)
    {
        if (IsFaceVisible(blockIter, faceDirections[faceIndex]))
        {
            Vec2 uvs;
            if (faceIndex == 0) uvs = def->GetTopUVs();     // Top
            else if (faceIndex == 1) uvs = def->GetBottomUVs();  // Bottom
            else uvs                     = def->GetSideUVs();    // Sides

            AddBlockFace(blockCenter, faceNormals[faceIndex], uvs, faceTints[faceIndex]);
        }
    }
}

//----------------------------------------------------------------------------------------------------
bool Chunk::IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const
{
    // Get neighboring block in the face direction
    BlockIterator neighborIter = blockIter.GetNeighbor(faceDirection);

    // If neighbor is outside this chunk, always render face (assignment requirement)
    if (!neighborIter.IsValid())
    {
        return true;  // Face against neighboring blocks in other chunks are always rendered
    }

    // Get the neighboring block
    Block* neighborBlock = neighborIter.GetBlock();
    if (!neighborBlock)
    {
        return true;  // No block = air, so face is visible
    }

    // Get neighbor block definition
    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
    if (!neighborDef)
    {
        return true;  // No definition = air, so face is visible
    }

    // Face is hidden if neighbor is opaque (not visible = air, visible = solid)
    // Hidden surface removal: skip faces whose neighboring block is opaque
    return !neighborDef->IsVisible() || !neighborDef->IsOpaque();
}

//----------------------------------------------------------------------------------------------------
// Thread-safe chunk state management methods
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
bool Chunk::SetState(ChunkState newState)
{
    m_state.store(newState);
    return true;
}

//----------------------------------------------------------------------------------------------------
bool Chunk::CompareAndSetState(ChunkState expected, ChunkState desired)
{
    return m_state.compare_exchange_strong(expected, desired);
}

//----------------------------------------------------------------------------------------------------
bool Chunk::IsStateOneOf(ChunkState state1, ChunkState state2, ChunkState state3, ChunkState state4) const
{
    ChunkState currentState = m_state.load();
    return (currentState == state1) || (currentState == state2) || (currentState == state3) || (currentState == state4);
}

//----------------------------------------------------------------------------------------------------
// Disk I/O operations - Thread-safe, called by I/O worker thread
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
bool Chunk::LoadFromDisk()
{
    std::string filename = StringFormat("Saves/Chunk({0},{1}).chunk", m_chunkCoords.x, m_chunkCoords.y);

    std::vector<uint8_t> buffer;
    if (!FileReadToBuffer(buffer, filename))
    {
        return false;
    }

    // Verify minimum file size (header + at least one RLE entry)
    if (buffer.size() < sizeof(ChunkFileHeader) + sizeof(ChunkRLEEntry))
    {
        return false; // File too small
    }

    // Read and validate header
    ChunkFileHeader header;
    memcpy(&header, buffer.data(), sizeof(ChunkFileHeader));

    // Validate header
    if (header.fourCC[0] != 'G' || header.fourCC[1] != 'C' ||
        header.fourCC[2] != 'H' || header.fourCC[3] != 'K')
    {
        return false; // Invalid 4CC
    }

    if (header.version != 1 || header.chunkBitsX != CHUNK_BITS_X ||
        header.chunkBitsY != CHUNK_BITS_Y || header.chunkBitsZ != CHUNK_BITS_Z)
    {
        return false; // Incompatible format
    }

    // Decompress RLE data
    size_t dataOffset = sizeof(ChunkFileHeader);
    int    blockIndex = 0;

    while (dataOffset < buffer.size() && blockIndex < BLOCKS_PER_CHUNK)
    {
        // Read RLE entry
        if (dataOffset + sizeof(ChunkRLEEntry) > buffer.size())
        {
            return false; // Incomplete RLE entry
        }

        ChunkRLEEntry entry;
        memcpy(&entry, buffer.data() + dataOffset, sizeof(ChunkRLEEntry));
        dataOffset += sizeof(ChunkRLEEntry);

        // Apply run to blocks
        for (int i = 0; i < entry.count && blockIndex < BLOCKS_PER_CHUNK; i++)
        {
            IntVec3 localCoords = IndexToLocalCoords(blockIndex);
            Block*  block       = GetBlock(localCoords.x, localCoords.y, localCoords.z);
            if (block != nullptr)
            {
                block->m_typeIndex = entry.value;
            }
            blockIndex++;
        }
    }

    // Verify we loaded exactly the right number of blocks
    if (blockIndex != BLOCKS_PER_CHUNK)
    {
        return false; // RLE data doesn't match expected block count
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
bool Chunk::SaveToDisk() const
{
    // Ensure save directory exists (relative to executable in Run/ directory)
    std::string saveDir = "Saves/";
    std::filesystem::create_directories(saveDir);

    std::string filename = StringFormat("{0}Chunk({1},{2}).chunk", saveDir, m_chunkCoords.x, m_chunkCoords.y);

    // Collect block data in order for RLE compression
    std::vector<uint8_t> blockData(BLOCKS_PER_CHUNK);
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = IndexToLocalCoords(i);
        Block*  block       = const_cast<Chunk*>(this)->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            blockData[i] = block->m_typeIndex;
        }
        else
        {
            blockData[i] = 0; // Air block if invalid
        }
    }

    // Compress using RLE
    std::vector<ChunkRLEEntry> rleEntries;
    uint8_t                    currentType = blockData[0];
    uint8_t                    runLength   = 1;

    for (int i = 1; i < BLOCKS_PER_CHUNK; i++)
    {
        if (blockData[i] == currentType && runLength < 255)
        {
            runLength++;
        }
        else
        {
            // End current run
            rleEntries.push_back({currentType, runLength});
            currentType = blockData[i];
            runLength   = 1;
        }
    }
    // Don't forget the last run
    rleEntries.push_back({currentType, runLength});

    // Create file header
    ChunkFileHeader header;
    header.fourCC[0]  = 'G';
    header.fourCC[1]  = 'C';
    header.fourCC[2]  = 'H';
    header.fourCC[3]  = 'K';
    header.version    = 1;
    header.chunkBitsX = CHUNK_BITS_X;
    header.chunkBitsY = CHUNK_BITS_Y;
    header.chunkBitsZ = CHUNK_BITS_Z;

    // Calculate total file size
    size_t               fileSize = sizeof(ChunkFileHeader) + rleEntries.size() * sizeof(ChunkRLEEntry);
    std::vector<uint8_t> fileBuffer(fileSize);

    // Write header
    memcpy(fileBuffer.data(), &header, sizeof(ChunkFileHeader));

    // Write RLE entries
    size_t offset = sizeof(ChunkFileHeader);
    for (const ChunkRLEEntry& entry : rleEntries)
    {
        memcpy(fileBuffer.data() + offset, &entry, sizeof(ChunkRLEEntry));
        offset += sizeof(ChunkRLEEntry);
    }

    // Write to file using safe fopen_s
    FILE*   file = nullptr;
    errno_t err  = fopen_s(&file, filename.c_str(), "wb");
    if (err != 0 || file == nullptr) return false;

    size_t written = fwrite(fileBuffer.data(), 1, fileBuffer.size(), file);
    fclose(file);

    return written == fileBuffer.size();
}
