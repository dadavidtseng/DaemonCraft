//----------------------------------------------------------------------------------------------------
// World.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>
#include <unordered_map>

struct IntVec2;
struct IntVec3;
struct Vec3;
class Camera;
class Chunk;

//----------------------------------------------------------------------------------------------------
// Hash function for IntVec2 to enable std::unordered_map usage
//----------------------------------------------------------------------------------------------------
namespace std
{
    template <>
    struct hash<IntVec2>
    {
        size_t operator()(IntVec2 const& vec) const noexcept;
    };
}

//----------------------------------------------------------------------------------------------------
// Chunk Management Constants - Reduced for Intel graphics compatibility
//----------------------------------------------------------------------------------------------------
constexpr int CHUNK_ACTIVATION_RANGE    = 320;   // Reduced from 320 to 80
constexpr int CHUNK_DEACTIVATION_RANGE  = CHUNK_ACTIVATION_RANGE + 16 + 16; // CHUNK_SIZE_X + CHUNK_SIZE_Y
constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_X
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_Y  
constexpr int MAX_ACTIVE_CHUNKS         = (2 * CHUNK_ACTIVATION_RADIUS_X) * (2 * CHUNK_ACTIVATION_RADIUS_Y);

//----------------------------------------------------------------------------------------------------
// 4. World units: Each world unit is 1 meter.  Each block is 1.0 x 1.0 x 1.0 world units (meters) in size.
//    This assumption (that blocks are each 1m x 1m x 1m) is fundamental and should be hard-coded (for purposes of speed and numerical precision).
// 5. World positions: 3D, Vec3 (float x,y,z), free-floating positions in world space.
//    The world’s bounds extend infinitely in +/- X (east/west) and +/- Y (north/south) directions, but are finite vertically (from Z=0.0 at world bottom to Z=128.0 at world top, if chunks are 128 blocks tall).
class World
{
public:
    World();
    ~World();

    void    Update(float deltaSeconds);
    void    Render() const;
    void    ActivateChunk(IntVec2 const& chunkCoords);
    void    DeactivateChunk(IntVec2 const& chunkCoords);
    void    DeactivateAllChunks(); // For debug F8 and shutdown
    void    ToggleGlobalChunkDebugDraw(); // For debug F2 key
    bool    SetBlockAtGlobalCoords(IntVec3 const& globalCoords, uint8_t blockTypeIndex); // Set block at world position
    uint8_t GetBlockTypeAtGlobalCoords(IntVec3 const& globalCoords) const; // Get block type at world position
    Chunk*  GetChunk(IntVec2 const& chunkCoords) const;
    
    // Debug information getters
    int     GetActiveChunkCount() const;
    int     GetTotalVertexCount() const;
    int     GetTotalIndexCount() const;

    // Digging and placing methods
    bool    DigBlockAtCameraPosition(Vec3 const& cameraPos); // LMB - dig highest non-air block at or below camera
    bool    PlaceBlockAtCameraPosition(Vec3 const& cameraPos, uint8_t blockType); // RMB - place block above highest non-air block
    IntVec3 FindHighestNonAirBlockAtOrBelow(Vec3 const& position) const; // Helper to find highest solid block

    // Chunk management helper methods
    Vec3    GetCameraPosition() const;
    float   GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const;
    IntVec2 FindNearestMissingChunkInRange(Vec3 const& cameraPos) const;
    IntVec2 FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const;
    Chunk*  FindNearestDirtyChunk(Vec3 const& cameraPos) const;

private:
    /// @brief Active chunks stored in hash map for O(1) lookup
    std::unordered_map<IntVec2, Chunk*> m_activeChunks;

    // Global debug state for all chunks
    bool m_globalChunkDebugDraw = false;

    // Chunk management helper methods
    bool ChunkExistsOnDisk(IntVec2 const& chunkCoords) const;
    bool LoadChunkFromDisk(Chunk* chunk) const;
    bool SaveChunkToDisk(Chunk* chunk) const;
    void UpdateNeighborPointers(IntVec2 const& chunkCoords);
    void ClearNeighborReferences(IntVec2 const& chunkCoords);
};
