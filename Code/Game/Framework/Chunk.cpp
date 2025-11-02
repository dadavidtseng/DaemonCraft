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
#include "Engine/Math/RandomNumberGenerator.hpp"
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
const uint8_t BLOCK_AIR            = 0;   // Air
const uint8_t BLOCK_GRASS          = 1;   // Grass
const uint8_t BLOCK_DIRT           = 2;   // Dirt
const uint8_t BLOCK_STONE          = 3;   // Stone
const uint8_t BLOCK_COAL           = 4;   // Coal
const uint8_t BLOCK_IRON           = 5;   // Iron
const uint8_t BLOCK_GOLD           = 6;   // Gold
const uint8_t BLOCK_DIAMOND        = 7;   // Diamond
const uint8_t BLOCK_WATER          = 8;   // Water
const uint8_t BLOCK_GLOWSTONE      = 9;   // Glowstone (index 9 in XML)
const uint8_t BLOCK_COBBLESTONE    = 10;  // Cobblestone (index 10 in XML)
const uint8_t BLOCK_CHISELED_BRICK = 11;  // ChiseledBrick (index 11 in XML)
const uint8_t BLOCK_SAND           = 12;  // Sand (index 12 in XML)
const uint8_t BLOCK_SNOW           = 13;  // Snow (index 13 in XML)
const uint8_t BLOCK_ICE            = 14;  // Ice (index 14 in XML)
const uint8_t BLOCK_OBSIDIAN       = 15;  // Obsidian (index 15 in XML)
const uint8_t BLOCK_LAVA           = 26;  // Lava (index 26 in XML)

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
    if (m_vertexBuffer && m_indexBuffer && !m_vertices.empty())
    {
        g_renderer->SetBlendMode(eBlendMode::OPAQUE);
        g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
        g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
        g_renderer->SetDepthMode(eDepthMode::READ_WRITE_LESS_EQUAL);
        // g_renderer->BindTexture(g_renderer->CreateOrGetTextureFromFile("Data/Images/BlockSpriteSheet_Dokucraft_32px.png"));
        g_renderer->BindTexture(g_resourceSubsystem->CreateOrGetTextureFromFile("Data/Images/BlockSpriteSheet_Dokucraft_32px.png"));

        // Use m_indices.size() instead of m_indexBuffer->GetSize()
        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, (int)m_indices.size());
    }

    if (!m_drawDebug || !m_debugVertexBuffer) return;

    g_renderer->BindTexture(nullptr);
    // Use m_debugVertices.size() instead of m_debugVertexBuffer->GetSize()
    g_renderer->DrawVertexBuffer(m_debugVertexBuffer, (int)m_debugVertices.size());
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

        default:
            return BLOCK_STONE;  // Should never reach here
    }
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
                            PEAKS_VALLEYS_NOISE_SCALE,
                            PEAKS_VALLEYS_NOISE_OCTAVES,
                            DEFAULT_OCTAVE_PERSISTANCE,
                            DEFAULT_NOISE_OCTAVE_SCALE,
                            true,
                            visualizationSeed
                        );
                        noiseValue = 1.f - fabsf((3.f * fabsf(weirdness)) - 2.f);
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
        }
    }

    // --- Pass 2: assign block types for every (x,y,z) using cache-coherent iteration ---
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

                int   terrainHeight = heightMapXY[idxXY];
                int   dirtDepth     = dirtDepthXY[idxXY];
                float humidity      = humidityMapXY[idxXY];
                float temperature   = temperatureMapXY[idxXY];

                // Temperature-driven ice ceiling depth
                float iceDepth = DEFAULT_TERRAIN_HEIGHT - floorf(RangeMapClamped(temperature,
                                                                                 ICE_TEMPERATURE_MAX, ICE_TEMPERATURE_MIN, ICE_DEPTH_MIN, ICE_DEPTH_MAX));

                uint8_t blockType = BLOCK_AIR; // Default to air

                // Water and ice between surface and sea level
                if (globalCoords.z > terrainHeight && globalCoords.z < SEA_LEVEL_Z)
                {
                    blockType = BLOCK_WATER;
                    if (temperature < 0.38f && globalCoords.z > iceDepth)
                    {
                        blockType = BLOCK_ICE;
                    }
                }

                // Surface block (grass vs sand by humidity and elevation)
                if (globalCoords.z == terrainHeight)
                {
                    blockType = BLOCK_GRASS;
                    if (humidity < MIN_SAND_HUMIDITY)
                    {
                        blockType = BLOCK_SAND;
                    }
                    if (humidity < MAX_SAND_HUMIDITY && terrainHeight <= (int)DEFAULT_TERRAIN_HEIGHT)
                    {
                        blockType = BLOCK_SAND;
                    }
                }

                // Subsurface: dirt or sand cap above stone/ores
                int dirtTopZ = terrainHeight - dirtDepth;
                int sandTopZ = terrainHeight - (int)floorf(RangeMapClamped(humidity,
                                                                           MIN_SAND_DEPTH_HUMIDITY, MAX_SAND_DEPTH_HUMIDITY, SAND_DEPTH_MIN, SAND_DEPTH_MAX));

                if (globalCoords.z < terrainHeight && globalCoords.z >= dirtTopZ)
                {
                    blockType = BLOCK_DIRT;
                    if (globalCoords.z >= sandTopZ)
                    {
                        blockType = BLOCK_SAND;
                    }
                }

                // Deep underground: special layers, lava/obsidian, ores, stone
                if (globalCoords.z < dirtTopZ)
                {
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
                        }
                    }
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
        // Debug output to help diagnose modification
        DebuggerPrintf("Block modified at (%d,%d,%d) in chunk (%d,%d): %d -> %d\n",
                       localBlockIndexX, localBlockIndexY, localBlockIndexZ,
                       m_chunkCoords.x, m_chunkCoords.y,
                       m_blocks[index].m_typeIndex, blockTypeIndex);

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

    // Delete old buffers
    GAME_SAFE_RELEASE(m_vertexBuffer);
    GAME_SAFE_RELEASE(m_indexBuffer);
    GAME_SAFE_RELEASE(m_debugVertexBuffer);

    // Create main vertex buffer with correct total size
    m_vertexBuffer = g_renderer->CreateVertexBuffer(
        (int)m_vertices.size() * sizeof(Vertex_PCU),  // Total buffer size in bytes
        sizeof(Vertex_PCU)                       // Size of each vertex
    );
    g_renderer->CopyCPUToGPU(m_vertices.data(),
                             (unsigned int)(m_vertices.size() * sizeof(Vertex_PCU)),
                             m_vertexBuffer);

    // Create index buffer with correct total size
    m_indexBuffer = g_renderer->CreateIndexBuffer(
        (int)m_indices.size() * sizeof(unsigned int),  // Total buffer size in bytes
        sizeof(unsigned int)                      // Size of each index
    );
    g_renderer->CopyCPUToGPU(m_indices.data(),
                             (unsigned int)(m_indices.size() * sizeof(unsigned int)),
                             m_indexBuffer);

    // Only create debug buffer if we have debug vertices
    if (!m_debugVertices.empty())
    {
        m_debugVertexBuffer = g_renderer->CreateVertexBuffer(
            (int)m_debugVertices.size() * sizeof(Vertex_PCU),  // Total buffer size
            sizeof(Vertex_PCU)                            // Size of each vertex
        );
        g_renderer->CopyCPUToGPU(m_debugVertices.data(),
                                 (unsigned int)(m_debugVertices.size() * sizeof(Vertex_PCU)),
                                 m_debugVertexBuffer);
    }
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
