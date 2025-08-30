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
class World
{
    World()  = default;
    ~World() = default;

    std::vector<Chunk*> m_activeChunks;
    Camera*             m_worldCamera = nullptr;

    void   Update(float deltaSeconds);
    void   Render() const;
    void   ActivateChunk(IntVec2 const& chunkCoords);
    Chunk* GetChunk(IntVec2 const& chunkCoords) const;

    // Initially activate chunks (0,0), (2,0), (2,1), (2,-1)
    void InitializeStartingChunks();
};
