//----------------------------------------------------------------------------------------------------
// Chunk.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>

#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Renderer/VertexUtils.hpp"
#include "Game/Framework/Block.hpp"

//-Forward-Declaration--------------------------------------------------------------------------------
struct Rgba8;
struct Vec2;
struct Vec3;
struct sBlockDefinition;
class IndexBuffer;
class VertexBuffer;
struct Vertex_PCUTBN;

//----------------------------------------------------------------------------------------------------
int constexpr CHUNK_SIZE_X_BITS  = 4;        // X coordinates need 4 bits (can represent 0-15)
int constexpr CHUNK_SIZE_Y_BITS  = 4;        // Y coordinates need 4 bits (can represent 0-15)
int constexpr CHUNK_SIZE_Z_BITS  = 7;        // Z coordinates need 7 bits (can represent 0-127)
int constexpr CHUNK_SIZE_X       = 1 << CHUNK_SIZE_X_BITS;      // 1 shifted left 4 positions = 2^4 = (16 blocks wide)
int constexpr CHUNK_SIZE_Y       = 1 << CHUNK_SIZE_Y_BITS;      // 1 shifted left 4 positions = 2^4 = (16 blocks deep)
int constexpr CHUNK_SIZE_Z       = 1 << CHUNK_SIZE_Z_BITS;      // 1 shifted left 7 positions = 2^7 = (128 blocks tall)
int constexpr CHUNK_TOTAL_BLOCKS = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

//----------------------------------------------------------------------------------------------------
class Chunk
{
public:
    explicit Chunk(IntVec2 const& chunkCoords);
    ~Chunk();

    void Update(float deltaSeconds);
    void Render();


    IntVec2 GetChunkCoords() const { return m_chunkCoords; }
    AABB3   GetWorldBounds() const { return m_worldBounds; }

    // Core methods
    void    GenerateTerrain();
    void    RebuildMesh();
    int     GetBlockIndexFromLocalCoords(int localX, int localY, int localZ) const;
    IntVec3 GetLocalCoordsFromIndex(int blockIndex) const;
    Block*  GetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ);

private:
    /// @brief 6. Chunk coordinates: 2D, IntVec2 (int x,y), with x and y axes aligned with world axes (above).
    ///           Adjacent chunks have adjacent chunk coordinates; for example, chunk (4,7) is the immediate eastern neighbor of chunk (3,7), and chunk (3,7)’s easternmost edge lines up exactly with chunk (4,7)’s westernmost edge.
    ///           There is no chunk Z coordinate since each chunk extends fully from the bottom of the world to the top of the world (i.e. Chunks do not stack vertically).
    IntVec2 m_chunkCoords = IntVec2::ZERO;
    /// @brief
    AABB3 m_worldBounds = AABB3::ZERO;
    Block m_blocks[CHUNK_TOTAL_BLOCKS]; // 1D array for cache efficiency

    // Rendering
    VertexList_PCU m_vertices;
    IndexList      m_indices;
    VertexBuffer*  m_vertexBuffer = nullptr;
    IndexBuffer*   m_indexBuffer  = nullptr;
    VertexList_PCU m_debugVertices;
    IndexList      m_debugIndices;
    VertexBuffer*  m_debugVertexBuffer = nullptr;
    IndexBuffer*   m_debugBuffer       = nullptr;
    bool           m_needsRebuild      = true;
    bool           m_drawDebug         = false;

    // Helper methods
    void AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords);
    void AddBlockFace(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint);
    void UpdateVertexBuffer();
};
