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
class BlockIterator;

//----------------------------------------------------------------------------------------------------
int constexpr CHUNK_BITS_X     = 4;                                                 // X coordinates need 4 bits (can represent 0-15)
int constexpr CHUNK_BITS_Y     = 4;                                                 // Y coordinates need 4 bits (can represent 0-15)
int constexpr CHUNK_BITS_Z     = 7;                                                 // Z coordinates need 7 bits (can represent 0-127)
int constexpr CHUNK_SIZE_X     = 1 << CHUNK_BITS_X;                                 // 1 shifted left 4 positions = 2^4 = 16 blocks wide (east-west)
int constexpr CHUNK_SIZE_Y     = 1 << CHUNK_BITS_Y;                                 // 1 shifted left 4 positions = 2^4 = 16 blocks deep (north-south)
int constexpr CHUNK_SIZE_Z     = 1 << CHUNK_BITS_Z;                                 // 1 shifted left 7 positions = 2^7 = 128 blocks tall (vertical)
int constexpr CHUNK_MAX_X      = CHUNK_SIZE_X - 1;                                  // Maximum valid X index (15) for bounds checking
int constexpr CHUNK_MAX_Y      = CHUNK_SIZE_Y - 1;                                  // Maximum valid Y index (15) for bounds checking
int constexpr CHUNK_MAX_Z      = CHUNK_SIZE_Z - 1;                                  // Maximum valid Z index (127) for bounds checking
int constexpr CHUNK_MASK_X     = CHUNK_MAX_X;                                       // Bit mask (0x000F) to extract X bits from block index
int constexpr CHUNK_MASK_Y     = CHUNK_MAX_Y << CHUNK_BITS_X;                       // Bit mask (0x00F0) to extract Y bits from block index
int constexpr CHUNK_MASK_Z     = CHUNK_MAX_Z << (CHUNK_BITS_X + CHUNK_BITS_Y);      // Bit mask (0x7F00) to extract Z bits from block index
int constexpr BLOCKS_PER_CHUNK = CHUNK_SIZE_X * CHUNK_SIZE_Y * CHUNK_SIZE_Z;        // Total blocks per chunk (16×16×128 = 32,768)

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

    // Debug information getters
    int GetVertexCount() const { return (int)m_vertices.size(); }
    int GetIndexCount() const { return (int)m_indices.size(); }

    // Core methods
    void GenerateTerrain();
    void RebuildMesh();

    Block* GetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ);
    void   SetBlock(int localBlockIndexX, int localBlockIndexY, int localBlockIndexZ, uint8_t blockTypeIndex);

    // Static utility functions for chunk coordinate management
    static int     LocalCoordsToIndex(IntVec3 const& localCoords);
    static int     LocalCoordsToIndex(int x, int y, int z);
    static int     IndexToLocalX(int index);
    static int     IndexToLocalY(int index);
    static int     IndexToLocalZ(int index);
    static IntVec3 IndexToLocalCoords(int index);
    static int     GlobalCoordsToIndex(IntVec3 const& globalCoords);
    static int     GlobalCoordsToIndex(int x, int y, int z);
    static IntVec2 GetChunkCoords(IntVec3 const& globalCoords);
    static IntVec2 GetChunkCenter(IntVec2 const& chunkCoords);
    static IntVec3 GlobalCoordsToLocalCoords(IntVec3 const& globalCoords);
    static IntVec3 GetGlobalCoords(IntVec2 const& chunkCoords, int blockIndex);
    static IntVec3 GetGlobalCoords(IntVec2 const& chunkCoords, IntVec3 const& localCoords);
    static IntVec3 GetGlobalCoords(Vec3 const& position);

    // Chunk management methods for persistent world
    bool GetNeedsSaving() const { return m_needsSaving; }
    void SetNeedsSaving(bool const needsSaving) { m_needsSaving = needsSaving; }
    bool GetIsMeshDirty() const { return m_isMeshDirty; }
    void SetIsMeshDirty(bool const isDirty) { m_isMeshDirty = isDirty; }

    // Debug rendering control
    bool GetDebugDraw() const { return m_drawDebug; }
    void SetDebugDraw(bool const drawDebug) { m_drawDebug = drawDebug; }

    // Neighbor chunk management
    void   SetNeighborChunks(Chunk* north, Chunk* south, Chunk* east, Chunk* west);
    void   ClearNeighborPointers();
    Chunk* GetNorthNeighbor() const { return m_northNeighbor; }
    Chunk* GetSouthNeighbor() const { return m_southNeighbor; }
    Chunk* GetEastNeighbor() const { return m_eastNeighbor; }
    Chunk* GetWestNeighbor() const { return m_westNeighbor; }

private:
    /// @brief 6. Chunk coordinates: 2D, IntVec2 (int x,y), with x and y axes aligned with world axes (above).
    ///           Adjacent chunks have adjacent chunk coordinates; for example, chunk (4,7) is the immediate eastern neighbor of chunk (3,7), and chunk (3,7)'s easternmost edge lines up exactly with chunk (4,7)'s westernmost edge.
    ///           There is no chunk Z coordinate since each chunk extends fully from the bottom of the world to the top of the world (i.e. Chunks do not stack vertically).
    IntVec2 m_chunkCoords = IntVec2::ZERO;
    /// @brief
    AABB3 m_worldBounds = AABB3::ZERO;
    Block m_blocks[BLOCKS_PER_CHUNK]; // 1D array for cache efficiency

    // Rendering
    VertexList_PCU m_vertices;
    IndexList      m_indices;
    VertexBuffer*  m_vertexBuffer = nullptr;
    IndexBuffer*   m_indexBuffer  = nullptr;
    VertexList_PCU m_debugVertices;
    IndexList      m_debugIndices;
    VertexBuffer*  m_debugVertexBuffer = nullptr;
    IndexBuffer*   m_debugBuffer       = nullptr;
    bool           m_drawDebug         = false;

    // Chunk management flags for persistent world
    bool m_needsSaving = false;        // true if chunk has been modified and needs to be saved to disk
    bool m_isMeshDirty = true;         // true if mesh needs regeneration

    // Neighbor chunk pointers for efficient access
    Chunk* m_northNeighbor = nullptr;   // +Y direction
    Chunk* m_southNeighbor = nullptr;   // -Y direction
    Chunk* m_eastNeighbor  = nullptr;   // +X direction
    Chunk* m_westNeighbor  = nullptr;   // -X direction

    // Helper methods
    void AddBlockFacesIfVisible(Vec3 const& blockCenter, sBlockDefinition* def, IntVec3 const& coords);
    void AddBlockFace(Vec3 const& blockCenter, Vec3 const& faceNormal, Vec2 const& uvs, Rgba8 const& tint);
    void UpdateVertexBuffer();

    // Advanced mesh generation with hidden surface removal
    void AddBlockFacesWithHiddenSurfaceRemoval(BlockIterator const& blockIter, sBlockDefinition* def);
    bool IsFaceVisible(BlockIterator const& blockIter, IntVec3 const& faceDirection) const;
};
