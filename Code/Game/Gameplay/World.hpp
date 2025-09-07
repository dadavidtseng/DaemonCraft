//----------------------------------------------------------------------------------------------------
// World.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>

struct IntVec2;
class Camera;
class Chunk;

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

    /// @brief World class own all active chunks
    std::vector<Chunk*> m_activeChunks;
    Camera*             m_worldCamera = nullptr;

    void   Update(float deltaSeconds);
    void   Render() const;
    void   ActivateChunk(IntVec2 const& chunkCoords);
    Chunk* GetChunk(IntVec2 const& chunkCoords) const;
};
