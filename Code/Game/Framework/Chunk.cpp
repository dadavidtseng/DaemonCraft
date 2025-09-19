//----------------------------------------------------------------------------------------------------
// Chunk.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Chunk.hpp"

#include "Engine/Core/EngineCommon.hpp"
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
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Framework/BlockIterator.hpp"
#include "ThirdParty/Noise/RawNoise.hpp"
#include "ThirdParty/Noise/SmoothNoise.hpp"

//----------------------------------------------------------------------------------------------------
// Block Type Constants (add these to GameCommon.hpp if not already there)
const uint8_t BLOCK_AIR     = 0;
const uint8_t BLOCK_GRASS   = 1;
const uint8_t BLOCK_DIRT    = 2;
const uint8_t BLOCK_STONE   = 3;
const uint8_t BLOCK_COAL    = 4;
const uint8_t BLOCK_IRON    = 5;
const uint8_t BLOCK_GOLD    = 6;
const uint8_t BLOCK_DIAMOND = 7;
const uint8_t BLOCK_WATER   = 8;
const uint8_t BLOCK_ICE     = 9;
const uint8_t BLOCK_SAND    = 10;
const uint8_t BLOCK_OBSIDIAN = 11;
const uint8_t BLOCK_LAVA    = 12;
const uint8_t BLOCK_GLOWSTONE = 13;
const uint8_t BLOCK_COBBLESTONE = 14;
const uint8_t BLOCK_CHISELED_BRICK = 15;

//----------------------------------------------------------------------------------------------------
Chunk::Chunk(IntVec2 const& chunkCoords)
    : m_chunkCoords(chunkCoords)
{
    // Calculate world bounds
    Vec3 worldMins((float)(chunkCoords.x) * CHUNK_SIZE_X, (float)(chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 worldMaxs = worldMins + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);
    m_worldBounds  = AABB3(worldMins, worldMaxs);

    GenerateTerrain();
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
        g_renderer->BindTexture(g_renderer->CreateOrGetTextureFromFile("Data/Images/BlockSpriteSheet_32px.png"));

        // Use m_indices.size() instead of m_indexBuffer->GetSize()
        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, (int)m_indices.size());
    }

    if (!m_drawDebug || !m_debugVertexBuffer) return;

    g_renderer->BindTexture(nullptr);
    // Use m_debugVertices.size() instead of m_debugVertexBuffer->GetSize()
    g_renderer->DrawVertexBuffer(m_debugVertexBuffer, (int)m_debugVertices.size());
}

//----------------------------------------------------------------------------------------------------
void Chunk::GenerateTerrain()
{
    // Establish world-space position and bounds of this chunk
    Vec3 chunkPosition((float)(m_chunkCoords.x) * CHUNK_SIZE_X, (float)(m_chunkCoords.y) * CHUNK_SIZE_Y, 0.f);
    Vec3 chunkBounds = chunkPosition + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);
    
    // Derive deterministic seeds for each noise channel
    unsigned int terrainSeed = GAME_SEED;
    unsigned int humiditySeed = GAME_SEED + 1;
    unsigned int temperatureSeed = humiditySeed + 1;
    unsigned int hillSeed = temperatureSeed + 1;
    unsigned int oceanSeed = hillSeed + 1;
    unsigned int dirtSeed = oceanSeed + 1;
    
    // Allocate per-(x,y) maps
    int heightMapXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
    int dirtDepthXY[CHUNK_SIZE_X * CHUNK_SIZE_Y];
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
            temperature = temperature + 0.5f + 0.5f * Compute2dPerlinNoise(
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
                terrainHeightF = terrainHeightF - Interpolate(0.f, OCEAN_DEPTH, oceanBlend);
            }
            
            // Dirt layer thickness driven by noise
            float dirtDepthPct = Get2dNoiseZeroToOne(globalX, globalY, dirtSeed);
            int dirtDepth = MIN_DIRT_OFFSET_Z + (int)roundf(dirtDepthPct * (float)(MAX_DIRT_OFFSET_Z - MIN_DIRT_OFFSET_Z));
            
            int idxXY = y * CHUNK_SIZE_X + x;
            humidityMapXY[idxXY] = humidity;
            temperatureMapXY[idxXY] = temperature;
            heightMapXY[idxXY] = (int)floorf(terrainHeightF);
            dirtDepthXY[idxXY] = dirtDepth;
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
                int idx = LocalCoordsToIndex(localCoords);
                int idxXY = y * CHUNK_SIZE_X + x;
                
                int terrainHeight = heightMapXY[idxXY];
                int dirtDepth = dirtDepthXY[idxXY];
                float humidity = humidityMapXY[idxXY];
                float temperature = temperatureMapXY[idxXY];
                
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
        Block& block = m_blocks[blockIndex];
        sBlockDefinition* def = sBlockDefinition::GetDefinitionByIndex(block.m_typeIndex);

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
int Chunk::GetBlockIndexFromLocalCoords(int const localX,
                                        int const localY,
                                        int const localZ) const
{
    return localX + (localY << CHUNK_BITS_X) + (localZ << (CHUNK_BITS_X + CHUNK_BITS_Y));
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetLocalCoordsFromIndex(int const blockIndex) const
{
    int const x = blockIndex & CHUNK_MASK_X;
    int const y = (blockIndex & CHUNK_MASK_Y) >> CHUNK_BITS_X;
    int const z = (blockIndex & CHUNK_MASK_Z) >> (CHUNK_BITS_X + CHUNK_BITS_Y);
    return IntVec3(x, y, z);
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
    int index = GetBlockIndexFromLocalCoords(localBlockIndexX, localBlockIndexY, localBlockIndexZ);
    
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
    m_eastNeighbor = east;
    m_westNeighbor = west;
}

//----------------------------------------------------------------------------------------------------
void Chunk::ClearNeighborPointers()
{
    m_northNeighbor = nullptr;
    m_southNeighbor = nullptr;
    m_eastNeighbor = nullptr;
    m_westNeighbor = nullptr;
}

//----------------------------------------------------------------------------------------------------
// Advanced Mesh Generation with Hidden Surface Removal
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesWithHiddenSurfaceRemoval(BlockIterator const& blockIter, sBlockDefinition* def)
{
    IntVec3 localCoords = blockIter.GetLocalCoords();
    Vec3 blockCenter = Vec3((float)localCoords.x + 0.5f, (float)localCoords.y + 0.5f, (float)localCoords.z + 0.5f) + 
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
            if (faceIndex == 0)       uvs = def->GetTopUVs();     // Top
            else if (faceIndex == 1)  uvs = def->GetBottomUVs();  // Bottom  
            else                      uvs = def->GetSideUVs();    // Sides
            
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
