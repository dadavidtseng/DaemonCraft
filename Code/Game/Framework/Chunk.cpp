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
#include "Engine/Renderer/IndexBuffer.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Renderer/VertexBuffer.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Framework/GameCommon.hpp"

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

    // 1.	Chunk meshes (CPU and GPU) are only rebuilt when dirty (i.e., not every frame).
    if (m_needsRebuild)
    {
        RebuildMesh();
        m_needsRebuild = false;
    }

    if (g_input->WasKeyJustPressed(KEYCODE_F2))
    {
        m_drawDebug = !m_drawDebug;
    }
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
        g_renderer->DrawIndexedVertexBuffer(m_vertexBuffer, m_indexBuffer, m_indices.size());
    }

    if (!m_drawDebug || !m_debugVertexBuffer) return;

    g_renderer->BindTexture(nullptr);
    // Use m_debugVertices.size() instead of m_debugVertexBuffer->GetSize()
    g_renderer->DrawVertexBuffer(m_debugVertexBuffer, m_debugVertices.size());
}

//----------------------------------------------------------------------------------------------------
void Chunk::GenerateTerrain()
{
    // Use three for-loops for every local block index in a chunk
    for (int localBlockIndexX = 0; localBlockIndexX < CHUNK_SIZE_X; localBlockIndexX++)
    {
        for (int localBlockIndexY = 0; localBlockIndexY < CHUNK_SIZE_Y; localBlockIndexY++)
        {
            // Convert to global coordinates
            int const globalX = m_chunkCoords.x * CHUNK_SIZE_X + localBlockIndexX;
            int const globalY = m_chunkCoords.y * CHUNK_SIZE_Y + localBlockIndexY;

            // 4.   Chunk block types are initialized on chunk creation/activation based on a simple random generation scheme;
            //      the terrain height of each block column is based on the global block coordinates of that block’s column with a slight random variation as follows (all numbers are integers)
            //      terrainHeightZ ( globalX, globalY ) = 50 + ( globalX / 3 ) + ( globally / 5) + random( 0 or 1 )
            int const terrainHeight = 50 + globalX / 3 + globalY / 5 + g_rng->RollRandomIntInRange(0, 1);

            // Dirt layer thickness is random per column (3-4 blocks)
            int const dirtLayerThickness = g_rng->RollRandomIntInRange(3, 4);

            for (int localBlockIndexZ = 0; localBlockIndexZ < CHUNK_SIZE_Z; localBlockIndexZ++)
            {
                uint8_t blockType = BLOCK_AIR; // Air by default

                // Water rule first
                if (localBlockIndexZ <= CHUNK_SIZE_Z / 2) blockType = BLOCK_WATER;

                // Then terrain rules (can override water)
                if (localBlockIndexZ < terrainHeight - dirtLayerThickness) blockType = BLOCK_STONE;
                if (localBlockIndexZ >= terrainHeight - dirtLayerThickness && localBlockIndexZ < terrainHeight) blockType = BLOCK_DIRT;
                if (localBlockIndexZ == terrainHeight) blockType = BLOCK_GRASS;

                // Ore generation (only in stone blocks)
                if (blockType == BLOCK_STONE)
                {
                    float const percentage = g_rng->RollRandomFloatInRange(0.f, 100.f);
                    if (percentage < 5.f) blockType = BLOCK_COAL;         // 5% coal
                    else if (percentage < 7.f) blockType = BLOCK_IRON;    // 2% iron
                    else if (percentage < 7.5f) blockType = BLOCK_GOLD;   // 0.5% gold
                    else if (percentage < 7.6f) blockType = BLOCK_DIAMOND; // 0.1% diamond
                }

                GetBlock(localBlockIndexX, localBlockIndexY, localBlockIndexZ)->m_typeIndex = blockType;
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
        Block&            block = m_blocks[blockIndex];
        sBlockDefinition* def   = sBlockDefinition::GetDefinitionByIndex(block.m_typeIndex);

        if (!def || !def->IsVisible()) continue;

        IntVec3 localCoords = GetLocalCoordsFromIndex(blockIndex);
        Vec3    blockCenter = Vec3((float)localCoords.x + 0.5f, (float)localCoords.y + 0.5f, (float)localCoords.z + 0.5f) + Vec3(m_chunkCoords.x * CHUNK_SIZE_X, m_chunkCoords.y * CHUNK_SIZE_Y, 0);

        // Add cube faces
        AddBlockFacesIfVisible(blockCenter, def, localCoords);
    }
    AddVertsForWireframeAABB3D(m_debugVertices, m_worldBounds, 0.1f);

    // Update GPU buffers
    UpdateVertexBuffer();
}

//----------------------------------------------------------------------------------------------------
int Chunk::GetBlockIndexFromLocalCoords(int const localX,
                                        int const localY,
                                        int const localZ) const
{
    return localX + (localY << CHUNK_SIZE_X_BITS) + (localZ << (CHUNK_SIZE_X_BITS + CHUNK_SIZE_Y_BITS));
}

//----------------------------------------------------------------------------------------------------
IntVec3 Chunk::GetLocalCoordsFromIndex(int blockIndex) const
{
    int x = blockIndex & (1 << CHUNK_SIZE_X_BITS) - 1;
    int y = blockIndex >> CHUNK_SIZE_X_BITS & (1 << CHUNK_SIZE_Y_BITS) - 1;
    int z = blockIndex >> (CHUNK_SIZE_X_BITS + CHUNK_SIZE_Y_BITS);
    return IntVec3(x, y, z);
}

//----------------------------------------------------------------------------------------------------
Block* Chunk::GetBlock(int const localBlockIndexX,
                       int const localBlockIndexY,
                       int const localBlockIndexZ)
{
    if (localBlockIndexX < 0 || localBlockIndexX >= CHUNK_SIZE_X ||
        localBlockIndexY < 0 || localBlockIndexY >= CHUNK_SIZE_Y ||
        localBlockIndexZ < 0 || localBlockIndexZ >= CHUNK_SIZE_Z)
    {
        return nullptr;
    }

    int const index = GetBlockIndexFromLocalCoords(localBlockIndexX, localBlockIndexY, localBlockIndexZ);

    return &m_blocks[index];
}

//----------------------------------------------------------------------------------------------------
// Private helper methods
//----------------------------------------------------------------------------------------------------
void Chunk::AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords)
{
    UNUSED(coords);
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
        m_vertices.size() * sizeof(Vertex_PCU),  // Total buffer size in bytes
        sizeof(Vertex_PCU)                       // Size of each vertex
    );
    g_renderer->CopyCPUToGPU(m_vertices.data(),
        (unsigned int)(m_vertices.size() * sizeof(Vertex_PCU)),
        m_vertexBuffer);

    // Create index buffer with correct total size
    m_indexBuffer = g_renderer->CreateIndexBuffer(
        m_indices.size() * sizeof(unsigned int),  // Total buffer size in bytes
        sizeof(unsigned int)                      // Size of each index
    );
    g_renderer->CopyCPUToGPU(m_indices.data(),
        (unsigned int)(m_indices.size() * sizeof(unsigned int)),
        m_indexBuffer);

    // Only create debug buffer if we have debug vertices
    if (!m_debugVertices.empty()) {
        m_debugVertexBuffer = g_renderer->CreateVertexBuffer(
            m_debugVertices.size() * sizeof(Vertex_PCU),  // Total buffer size
            sizeof(Vertex_PCU)                            // Size of each vertex
        );
        g_renderer->CopyCPUToGPU(m_debugVertices.data(),
            (unsigned int)(m_debugVertices.size() * sizeof(Vertex_PCU)),
            m_debugVertexBuffer);
    }
}
