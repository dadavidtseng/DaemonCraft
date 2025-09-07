//----------------------------------------------------------------------------------------------------
// World.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/GameCommon.hpp"

World::World()
{
    // 1.	Initially (for Assignment 1 only) only 4 chunks are activated: at chunk coordinates (0,0), (2,0), (2,1) and (2,-1).
    Chunk* chunk  = new Chunk(IntVec2(0,0));
    Chunk* chunk2 = new Chunk(IntVec2(2,0));
    Chunk* chunk3 = new Chunk(IntVec2(2,1));
    Chunk* chunk4 = new Chunk(IntVec2(2,-1));
    m_activeChunks.push_back(chunk);
    m_activeChunks.push_back(chunk2);
    m_activeChunks.push_back(chunk3);
    m_activeChunks.push_back(chunk4);
}

World::~World()
{
    for (Chunk* chunk:m_activeChunks)
    {
        GAME_SAFE_RELEASE(chunk);
    }
    m_activeChunks.clear();
}

void World::Update(float deltaSeconds)
{
    if (g_game==nullptr)return;
    for (Chunk* chunk:m_activeChunks)
    {
        if (chunk==nullptr)continue;
        chunk->Update(deltaSeconds);
    }
}

void World::Render() const
{
    for (Chunk* chunk:m_activeChunks)
    {
        if (chunk==nullptr)continue;
        chunk->Render();
    }
}

void World::ActivateChunk(IntVec2 const& chunkCoords)
{
    UNUSED(chunkCoords)
}

Chunk* World::GetChunk(IntVec2 const& chunkCoords) const
{
    UNUSED(chunkCoords)
    return nullptr;
}