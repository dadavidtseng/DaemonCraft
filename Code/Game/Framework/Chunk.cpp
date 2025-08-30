//----------------------------------------------------------------------------------------------------
// Chunk.cpp - Complete Implementation
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/Chunk.hpp"

#include "GameCommon.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Game/Definition/BlockDefinition.hpp"
#include "Engine/Core/Rgba8.hpp"
#include "Engine/Math/Vec2.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/RandomNumberGenerator.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Vertex_PCUTBN.hpp"
#include "Engine/Renderer/VertexUtils.hpp"

//----------------------------------------------------------------------------------------------------
// Block Type Constants (add these to GameCommon.hpp if not already there)
const uint8_t BLOCK_AIR = 0;
const uint8_t BLOCK_GRASS = 1;
const uint8_t BLOCK_DIRT = 2;
const uint8_t BLOCK_STONE = 3;
const uint8_t BLOCK_COAL = 4;
const uint8_t BLOCK_IRON = 5;
const uint8_t BLOCK_GOLD = 6;
const uint8_t BLOCK_DIAMOND = 7;
const uint8_t BLOCK_WATER = 8;

//----------------------------------------------------------------------------------------------------
Chunk::Chunk(IntVec2 const& chunkCoords)
    : m_chunkCoords(chunkCoords)
    , m_vertexBuffer(nullptr)
    , m_indexBuffer(nullptr)
    , m_debugVBO(nullptr)
{
    // Calculate world bounds
    Vec3 worldMins(
        (float)(chunkCoords.x * CHUNK_SIZE_X),
        (float)(chunkCoords.y * CHUNK_SIZE_Y),
        0.f
    );
    Vec3 worldMaxs = worldMins + Vec3((float)CHUNK_SIZE_X, (float)CHUNK_SIZE_Y, (float)CHUNK_SIZE_Z);
    m_worldBounds = AABB3(worldMins, worldMaxs);

    GenerateTerrain();
}

//----------------------------------------------------------------------------------------------------
Chunk::~Chunk()
{
    delete m_vertexBuffer;
    delete m_indexBuffer;
    delete m_debugVBO;
}

//----------------------------------------------------------------------------------------------------
void Chunk::Update(float deltaSeconds)
{
    // UNUSED(deltaSeconds);
    if (m_needsRebuild)
    {
        RebuildMesh();
        m_needsRebuild = false;
    }
}

//----------------------------------------------------------------------------------------------------
void Chunk::Render() const
{
    if (m_vertexBuffer && m_indexBuffer && !m_vertices.empty())
    {
        // g_renderer->BindTexture(nullptr);
        // g_renderer->SetModelConstants(GetModelToWorldTransform(), m_color);
        g_renderer->SetBlendMode(eBlendMode::OPAQUE); //AL
        g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);  //SOLID_CULL_NONE
        g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
        g_renderer->SetDepthMode(eDepthMode::READ_WRITE_LESS_EQUAL);  //DISABLE
        g_renderer->BindTexture(g_renderer->CreateOrGetTextureFromFile("Data/Images/BlockSpriteSheet_32px.png"));
        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, (unsigned int)m_indices.size());
    }
}

//----------------------------------------------------------------------------------------------------
void Chunk::RenderDebug() const
{
    if (m_debugVBO && !m_debugVertices.empty())
    {
        g_renderer->BindTexture(nullptr);
        g_renderer->DrawVertexBuffer(m_debugVBO, (unsigned int)m_debugVertices.size());
    }
}

//----------------------------------------------------------------------------------------------------
void Chunk::GenerateTerrain()
{
    for (int x = 0; x < CHUNK_SIZE_X; x++) 
    {
        for (int y = 0; y < CHUNK_SIZE_Y; y++) 
        {
            // Convert to global coordinates
            int globalX = m_chunkCoords.x * CHUNK_SIZE_X + x;
            int globalY = m_chunkCoords.y * CHUNK_SIZE_Y + y;

            // Terrain height formula from assignment
            int terrainHeight = 50 + (globalX / 3) + (globalY / 5) + g_rng->RollRandomIntInRange(0, 1);

            // Dirt layer thickness is random per column (3-4 blocks)
            int dirtLayerThickness = g_rng->RollRandomIntInRange(3, 4);

            for (int z = 0; z < CHUNK_SIZE_Z; z++) 
            {
                uint8_t blockType = BLOCK_AIR; // Air by default

                // Water rule first
                if (z <= CHUNK_SIZE_Z/2) blockType = BLOCK_WATER;

                // Then terrain rules (can override water)
                if (z < terrainHeight - dirtLayerThickness) blockType = BLOCK_STONE;
                if (z >= terrainHeight - dirtLayerThickness && z < terrainHeight) blockType = BLOCK_DIRT;
                if (z == terrainHeight) blockType = BLOCK_GRASS;

                // Ore generation (only in stone blocks)
                if (blockType == BLOCK_STONE) 
                {
                    float roll = g_rng->RollRandomFloatInRange(0.f, 100.f);
                    if (roll < 5.f) blockType = BLOCK_COAL;         // 5% coal
                    else if (roll < 7.f) blockType = BLOCK_IRON;    // 2% iron
                    else if (roll < 7.5f) blockType = BLOCK_GOLD;   // 0.5% gold
                    else if (roll < 7.6f) blockType = BLOCK_DIAMOND; // 0.1% diamond
                }

                GetBlock(x, y, z)->m_typeIndex = blockType;
            }
        }
    }
    m_needsRebuild = true;
}

//----------------------------------------------------------------------------------------------------
void Chunk::RebuildMesh()
{
    m_vertices.clear();
    m_indices.clear();

    for (int blockIndex = 0; blockIndex < CHUNK_TOTAL_BLOCKS; blockIndex++)
    {
        Block& block = m_blocks[blockIndex];
        sBlockDefinition* def = sBlockDefinition::GetDefinitionByIndex(block.m_typeIndex);

        if (!def || !def->IsVisible()) continue;

        IntVec3 localCoords = GetLocalCoordsFromIndex(blockIndex);
        Vec3 blockCenter = Vec3((float)localCoords.x + 0.5f, (float)localCoords.y + 0.5f, (float)localCoords.z + 0.5f);

        // Add cube faces
        AddBlockFacesIfVisible(blockCenter, def, localCoords);
    }

    // Update GPU buffers
    UpdateVertexBuffer();
}

//----------------------------------------------------------------------------------------------------
int Chunk::GetBlockIndex(int localX, int localY, int localZ) const
{
    return ::GetBlockIndex(localX, localY, localZ); // Use the global inline function
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetLocalCoordsFromIndex(int blockIndex) const
{
    return ::GetCoordsFromIndex(blockIndex); // Use the global inline function
}

//----------------------------------------------------------------------------------------------------
Block* Chunk::GetBlock(int localX, int localY, int localZ)
{
    if (localX < 0 || localX >= CHUNK_SIZE_X ||
        localY < 0 || localY >= CHUNK_SIZE_Y ||
        localZ < 0 || localZ >= CHUNK_SIZE_Z)
    {
        return nullptr;
    }

    int index = GetBlockIndex(localX, localY, localZ);
    return &m_blocks[index];
}

//----------------------------------------------------------------------------------------------------
// Private helper methods
//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords)
{
    UNUSED(coords);
    // For now, add all 6 faces - you can optimize with face culling later
    AddBlockFace(blockCenter, Vec3::Z_BASIS,  def->GetTopUVs(),    Rgba8::WHITE);        // Top
    AddBlockFace(blockCenter, -Vec3::Z_BASIS, def->GetBottomUVs(), Rgba8::WHITE);       // Bottom
    AddBlockFace(blockCenter, Vec3::X_BASIS,  def->GetSideUVs(),   Rgba8(230,230,230)); // East
    AddBlockFace(blockCenter, -Vec3::X_BASIS, def->GetSideUVs(),   Rgba8(230,230,230)); // West
    AddBlockFace(blockCenter, Vec3::Y_BASIS,  def->GetSideUVs(),   Rgba8(200,200,200)); // North
    AddBlockFace(blockCenter, -Vec3::Y_BASIS, def->GetSideUVs(),   Rgba8(200,200,200)); // South
}

//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFace(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint)
{
    UNUSED(uvs);
    Vec3 right, up;
    if (faceNormal == Vec3::Z_BASIS || faceNormal == -Vec3::Z_BASIS)
    {
        right = Vec3::X_BASIS;
        up = Vec3::Y_BASIS;
    }
    else if (faceNormal == Vec3::X_BASIS || faceNormal == -Vec3::X_BASIS)
    {
        right = Vec3::Y_BASIS;
        up = Vec3::Z_BASIS;
    }
    else
    {
        right = Vec3::X_BASIS;
        up = Vec3::Z_BASIS;
    }

    Vec3 faceCenter = blockCenter + faceNormal * 0.5f;
    Vec3 tangent = right;
    Vec3 bitangent = up;
    Vec3 normal = faceNormal;

    // Add 4 vertices for quad with proper Vertex_PCUTBN constructor


    int startVertIndex = (int)m_vertices.size();

    // Create vertices with proper constructor (position, color, UVs, tangent, bitangent, normal)
    m_vertices.emplace_back(faceCenter - right*0.5f - up*0.5f, tint, Vec2(0,1), tangent, bitangent, normal); // BL
    m_vertices.emplace_back(faceCenter + right*0.5f - up*0.5f, tint, Vec2(1,1), tangent, bitangent, normal); // BR
    m_vertices.emplace_back(faceCenter + right*0.5f + up*0.5f, tint, Vec2(1,0), tangent, bitangent, normal); // TR
    m_vertices.emplace_back(faceCenter - right*0.5f + up*0.5f, tint, Vec2(0,0), tangent, bitangent, normal); // TL

    // Add 6 indices for 2 triangles
    m_indices.push_back(startVertIndex + 0);
    m_indices.push_back(startVertIndex + 1);
    m_indices.push_back(startVertIndex + 2);

    m_indices.push_back(startVertIndex + 0);
    m_indices.push_back(startVertIndex + 2);
    m_indices.push_back(startVertIndex + 3);
}

//----------------------------------------------------------------------------------------------------
void Chunk::UpdateVertexBuffer()
{
    if (m_vertices.empty()) return;

    // Delete old buffers
    delete m_vertexBuffer;
    delete m_indexBuffer;
    m_vertexBuffer = nullptr;
    m_indexBuffer = nullptr;

    // Create new buffers with correct API
    m_vertexBuffer = g_renderer->CreateVertexBuffer((unsigned int)(m_vertices.size() * sizeof(Vertex_PCUTBN)), sizeof(Vertex_PCUTBN));
    g_renderer->CopyCPUToGPU(m_vertices.data(), (unsigned int)(m_vertices.size() * sizeof(Vertex_PCUTBN)), m_vertexBuffer);

    m_indexBuffer = g_renderer->CreateIndexBuffer((unsigned int)(m_indices.size() * sizeof(unsigned int)), sizeof(unsigned int));
    g_renderer->CopyCPUToGPU(m_indices.data(), (unsigned int)(m_indices.size() * sizeof(unsigned int)), m_indexBuffer);
}