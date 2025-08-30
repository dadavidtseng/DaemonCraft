//----------------------------------------------------------------------------------------------------
// Chunk.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>

#include "Block.hpp"
#include "Engine/Math/AABB3.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"

struct Rgba8;
struct Vec2;
struct Vec3;
struct sBlockDefinition;
class IndexBuffer;
class VertexBuffer;
struct Vertex_PCUTBN;
//----------------------------------------------------------------------------------------------------
constexpr int CHUNK_SIZE_X_BITS = 4;  // 16 blocks wide
constexpr int CHUNK_SIZE_Y_BITS = 4;  // 16 blocks deep
constexpr int CHUNK_SIZE_Z_BITS = 7;  // 128 blocks tall

constexpr int CHUNK_SIZE_X = 1 << CHUNK_SIZE_X_BITS;
constexpr int CHUNK_SIZE_Y = 1 << CHUNK_SIZE_Y_BITS;
constexpr int CHUNK_SIZE_Z = 1 << CHUNK_SIZE_Z_BITS;
constexpr int CHUNK_TOTAL_BLOCKS = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;

//----------------------------------------------------------------------------------------------------
class Chunk
{
public:  // <- Add this line
    explicit Chunk(IntVec2 const& chunkCoords);
    ~Chunk();

    void Update(float deltaSeconds);
    void Render() const;
    void RenderDebug() const;

    IntVec2 GetChunkCoords() const { return m_chunkCoords; }
    AABB3 GetWorldBounds() const { return m_worldBounds; }

    // Core methods
    void GenerateTerrain();
    void RebuildMesh();
    int GetBlockIndex(int localX, int localY, int localZ) const;
    IntVec3 GetLocalCoordsFromIndex(int blockIndex) const;
    Block* GetBlock(int localX, int localY, int localZ);

private:  // <- Add this line
    IntVec2 m_chunkCoords;
    AABB3 m_worldBounds;
    Block m_blocks[CHUNK_TOTAL_BLOCKS]; // 1D array for cache efficiency

    // Rendering
    std::vector<Vertex_PCUTBN> m_vertices;
    std::vector<unsigned int> m_indices;
    VertexBuffer* m_vertexBuffer;
    IndexBuffer* m_indexBuffer;
    bool m_needsRebuild = true;

    // Debug rendering
    std::vector<Vertex_PCUTBN> m_debugVertices;
    VertexBuffer* m_debugVBO;

    // Helper methods
    void AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords);
    void AddBlockFace(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint);
    void UpdateVertexBuffer();
};

// Fast coordinate conversions using bit operations
inline int GetBlockIndex(int x, int y, int z) {
    return x + (y << CHUNK_SIZE_X_BITS) + (z << (CHUNK_SIZE_X_BITS + CHUNK_SIZE_Y_BITS));
}

inline IntVec3 GetCoordsFromIndex(int index) {
    int x = index & ((1 << CHUNK_SIZE_X_BITS) - 1);
    int y = (index >> CHUNK_SIZE_X_BITS) & ((1 << CHUNK_SIZE_Y_BITS) - 1);
    int z = index >> (CHUNK_SIZE_X_BITS + CHUNK_SIZE_Y_BITS);
    return IntVec3(x, y, z);
}