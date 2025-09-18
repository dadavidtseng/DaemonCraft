//----------------------------------------------------------------------------------------------------
// World.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"

#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/GameCommon.hpp"
#include <algorithm>
#include <filesystem>

#include "Game.hpp"

//----------------------------------------------------------------------------------------------------
// Hash function implementation for IntVec2
//----------------------------------------------------------------------------------------------------
size_t std::hash<IntVec2>::operator()(const IntVec2& vec) const
{
    return std::hash<int>()(vec.x) ^ (std::hash<int>()(vec.y) << 1);
}

//----------------------------------------------------------------------------------------------------
World::World()
{
    // No longer hardcode chunks - they will be dynamically loaded based on camera position
}

//----------------------------------------------------------------------------------------------------
World::~World()
{
    // Deactivate all chunks when world is destroyed (saves modified chunks)
    DeactivateAllChunks();
}

//----------------------------------------------------------------------------------------------------
void World::Update(float deltaSeconds)
{
    if (g_game == nullptr) return;
    
    // Update all active chunks first
    for (auto& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->Update(deltaSeconds);
        }
    }
    
    // Get camera position for chunk management decisions
    Vec3 cameraPos = GetCameraPosition();
    
    // Execute exactly one chunk management action per frame
    // Priority: 1) Regenerate dirty chunk, 2) Activate missing chunk, 3) Deactivate distant chunk
    
    // 1. Check for dirty chunks and regenerate the single nearest dirty chunk
    Chunk* nearestDirtyChunk = FindNearestDirtyChunk(cameraPos);
    if (nearestDirtyChunk != nullptr)
    {
        nearestDirtyChunk->RebuildMesh();
        nearestDirtyChunk->SetIsMeshDirty(false);
        return; // Only one action per frame
    }

    // 2. If under max chunks, activate single nearest missing chunk within activation range
    if (m_activeChunks.size() < MAX_ACTIVE_CHUNKS)
    {
        IntVec2 nearestMissingChunk = FindNearestMissingChunkInRange(cameraPos);
        if (nearestMissingChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
        {
            ActivateChunk(nearestMissingChunk);
            return; // Only one action per frame
        }
    }
    
    // 3. Otherwise, deactivate the farthest active chunk if outside deactivation range
    else
    {
        IntVec2 farthestChunk = FindFarthestActiveChunkOutsideDeactivationRange(cameraPos);
        if (farthestChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
        {
            DeactivateChunk(farthestChunk);
            return; // Only one action per frame
        }
    }

    // Debug key F8 - force deactivate all chunks
    if (g_input && g_input->WasKeyJustPressed(KEYCODE_F8))
    {
        DeactivateAllChunks(); // Note: This breaks the one-action-per-frame rule for testing
    }
}

//----------------------------------------------------------------------------------------------------
void World::Render() const
{
    for (const auto& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->Render();
        }
    }
}

//----------------------------------------------------------------------------------------------------
void World::ActivateChunk(IntVec2 const& chunkCoords)
{
    // Check if chunk is already active
    if (m_activeChunks.find(chunkCoords) != m_activeChunks.end())
    {
        return; // Already active
    }
    
    // Create new chunk
    Chunk* newChunk = new Chunk(chunkCoords);
    
    // Try to load from disk first
    if (ChunkExistsOnDisk(chunkCoords))
    {
        if (!LoadChunkFromDisk(newChunk))
        {
            // If load fails, generate terrain
            newChunk->GenerateTerrain();
        }
    }
    else
    {
        // No save file exists, generate terrain
        newChunk->GenerateTerrain();
    }
    
    // Set initial chunk state
    newChunk->SetIsMeshDirty(true);  // Mesh needs generation
    newChunk->SetNeedsSaving(false); // Fresh chunk doesn't need saving yet
    newChunk->SetDebugDraw(m_globalChunkDebugDraw); // Inherit current global debug state
    
    // Add to active chunks map
    m_activeChunks[chunkCoords] = newChunk;
    
    // Hook up neighbor pointers
    UpdateNeighborPointers(chunkCoords);
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateChunk(IntVec2 const& chunkCoords)
{
    auto it = m_activeChunks.find(chunkCoords);
    if (it == m_activeChunks.end())
    {
        return; // Chunk not active
    }
    
    Chunk* chunk = it->second;
    if (chunk == nullptr)
    {
        m_activeChunks.erase(it);
        return;
    }
    
    // Clear neighbor pointers
    chunk->ClearNeighborPointers();
    
    // Update neighbors to remove references to this chunk
    ClearNeighborReferences(chunkCoords);
    
    // Remove from active chunks map
    m_activeChunks.erase(it);
    
    // Save to disk if needed
    if (chunk->GetNeedsSaving())
    {
        SaveChunkToDisk(chunk);
    }
    
    // Delete the chunk (this will destroy VBOs and block memory)
    delete chunk;
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateAllChunks()
{
    // Save all modified chunks before deactivating
    while (!m_activeChunks.empty())
    {
        auto it = m_activeChunks.begin();
        DeactivateChunk(it->first);
    }
}

//----------------------------------------------------------------------------------------------------
Chunk* World::GetChunk(IntVec2 const& chunkCoords) const
{
    auto it = m_activeChunks.find(chunkCoords);
    if (it != m_activeChunks.end())
    {
        return it->second;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------
Vec3 World::GetCameraPosition() const
{
    if (g_game != nullptr)
    {
        return g_game->GetPlayerCameraPosition();
    }
    return Vec3::ZERO; // Fallback if no game instance
}

//----------------------------------------------------------------------------------------------------
float World::GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const
{
    IntVec2 chunkCenter = Chunk::GetChunkCenter(chunkCoords);
    Vec3 chunkCenter3D = Vec3(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y), cameraPos.z);
    
    // Only consider XY distance as specified
    float deltaX = chunkCenter3D.x - cameraPos.x;
    float deltaY = chunkCenter3D.y - cameraPos.y;
    return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindNearestMissingChunkInRange(Vec3 const& cameraPos) const
{
    IntVec2 cameraChunkCoords = Chunk::GetChunkCoords(IntVec3(static_cast<int>(cameraPos.x), static_cast<int>(cameraPos.y), static_cast<int>(cameraPos.z)));
    
    float nearestDistance = FLT_MAX;
    IntVec2 nearestMissingChunk = IntVec2(INT_MAX, INT_MAX);
    
    // Search in a square around the camera position
    int searchRadius = (CHUNK_ACTIVATION_RANGE / 16) + 2; // CHUNK_SIZE_X is 16
    
    for (int x = cameraChunkCoords.x - searchRadius; x <= cameraChunkCoords.x + searchRadius; x++)
    {
        for (int y = cameraChunkCoords.y - searchRadius; y <= cameraChunkCoords.y + searchRadius; y++)
        {
            IntVec2 testChunk(x, y);
            
            // Skip if chunk is already active
            if (m_activeChunks.find(testChunk) != m_activeChunks.end())
            {
                continue;
            }
            
            // Check if within activation range
            float distance = GetDistanceToChunkCenter(testChunk, cameraPos);
            if (distance <= CHUNK_ACTIVATION_RANGE && distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestMissingChunk = testChunk;
            }
        }
    }
    
    return nearestMissingChunk;
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const
{
    float farthestDistance = 0.0f;
    IntVec2 farthestChunk = IntVec2(INT_MAX, INT_MAX);
    
    for (const auto& chunkPair : m_activeChunks)
    {
        float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);
        
        // Only consider chunks outside deactivation range
        if (distance > CHUNK_DEACTIVATION_RANGE && distance > farthestDistance)
        {
            farthestDistance = distance;
            farthestChunk = chunkPair.first;
        }
    }
    
    return farthestChunk;
}

//----------------------------------------------------------------------------------------------------
Chunk* World::FindNearestDirtyChunk(Vec3 const& cameraPos) const
{
    float nearestDistance = FLT_MAX;
    Chunk* nearestDirtyChunk = nullptr;
    
    for (const auto& chunkPair : m_activeChunks)
    {
        Chunk* chunk = chunkPair.second;
        if (chunk != nullptr && chunk->GetIsMeshDirty())
        {
            float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);
            if (distance < nearestDistance)
            {
                nearestDistance = distance;
                nearestDirtyChunk = chunk;
            }
        }
    }
    
    return nearestDirtyChunk;
}

//----------------------------------------------------------------------------------------------------
bool World::ChunkExistsOnDisk(IntVec2 const& chunkCoords) const
{
    std::string filename = StringFormat("Run/Data/Saves/DefaultWorld/Chunk_{0}_{1}.chunk", chunkCoords.x, chunkCoords.y);
    return std::filesystem::exists(filename);
}

//----------------------------------------------------------------------------------------------------
bool World::LoadChunkFromDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;
    
    IntVec2 chunkCoords = chunk->GetChunkCoords();
    std::string filename = StringFormat("Run/Data/Saves/DefaultWorld/Chunk_{0}_{1}.chunk", chunkCoords.x, chunkCoords.y);
    
    std::vector<uint8_t> buffer;
    if (!FileReadToBuffer(buffer, filename))
    {
        return false;
    }
    
    // Verify file size (should be exactly BLOCKS_PER_CHUNK bytes for block data)
    if (buffer.size() != BLOCKS_PER_CHUNK)
    {
        return false; // Corrupted file
    }
    
    // Copy block data from file to chunk
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = Chunk::IndexToLocalCoords(i);
        Block* block = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            block->m_typeIndex = buffer[i];
        }
    }
    
    return true;
}

//----------------------------------------------------------------------------------------------------
bool World::SaveChunkToDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;
    
    IntVec2 chunkCoords = chunk->GetChunkCoords();
    
    // Ensure save directory exists
    std::string saveDir = "Run/Data/Saves/DefaultWorld/";
    std::filesystem::create_directories(saveDir);
    
    std::string filename = StringFormat("{0}Chunk_{1}_{2}.chunk", saveDir, chunkCoords.x, chunkCoords.y);
    
    // Collect block data
    std::vector<uint8_t> buffer(BLOCKS_PER_CHUNK);
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = Chunk::IndexToLocalCoords(i);
        Block* block = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            buffer[i] = block->m_typeIndex;
        }
        else
        {
            buffer[i] = 0; // Air block if invalid
        }
    }
    
    // Write to file using safe fopen_s
    FILE* file = nullptr;
    errno_t err = fopen_s(&file, filename.c_str(), "wb");
    if (err != 0 || file == nullptr) return false;
    
    size_t written = fwrite(buffer.data(), 1, buffer.size(), file);
    fclose(file);
    
    return written == buffer.size();
}

//----------------------------------------------------------------------------------------------------
void World::UpdateNeighborPointers(IntVec2 const& chunkCoords)
{
    Chunk* centerChunk = GetChunk(chunkCoords);
    if (centerChunk == nullptr) return;
    
    // Get neighbor coordinates
    IntVec2 northCoords = chunkCoords + IntVec2(0, 1);
    IntVec2 southCoords = chunkCoords + IntVec2(0, -1);
    IntVec2 eastCoords = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords = chunkCoords + IntVec2(-1, 0);
    
    // Get neighbor chunks (if they exist)
    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk = GetChunk(eastCoords);
    Chunk* westChunk = GetChunk(westCoords);
    
    // Set neighbor pointers on center chunk
    centerChunk->SetNeighborChunks(northChunk, southChunk, eastChunk, westChunk);
    
    // Update neighbor chunks to point back to center chunk
    if (northChunk != nullptr) northChunk->SetNeighborChunks(northChunk->GetNorthNeighbor(), centerChunk, northChunk->GetEastNeighbor(), northChunk->GetWestNeighbor());
    if (southChunk != nullptr) southChunk->SetNeighborChunks(centerChunk, southChunk->GetSouthNeighbor(), southChunk->GetEastNeighbor(), southChunk->GetWestNeighbor());
    if (eastChunk != nullptr) eastChunk->SetNeighborChunks(eastChunk->GetNorthNeighbor(), eastChunk->GetSouthNeighbor(), eastChunk->GetEastNeighbor(), centerChunk);
    if (westChunk != nullptr) westChunk->SetNeighborChunks(westChunk->GetNorthNeighbor(), westChunk->GetSouthNeighbor(), centerChunk, westChunk->GetWestNeighbor());
}

//----------------------------------------------------------------------------------------------------
void World::ClearNeighborReferences(IntVec2 const& chunkCoords)
{
    // Clear references to this chunk from its neighbors
    IntVec2 northCoords = chunkCoords + IntVec2(0, 1);
    IntVec2 southCoords = chunkCoords + IntVec2(0, -1);
    IntVec2 eastCoords = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords = chunkCoords + IntVec2(-1, 0);
    
    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk = GetChunk(eastCoords);
    Chunk* westChunk = GetChunk(westCoords);
    
    if (northChunk != nullptr) northChunk->SetNeighborChunks(northChunk->GetNorthNeighbor(), nullptr, northChunk->GetEastNeighbor(), northChunk->GetWestNeighbor());
    if (southChunk != nullptr) southChunk->SetNeighborChunks(nullptr, southChunk->GetSouthNeighbor(), southChunk->GetEastNeighbor(), southChunk->GetWestNeighbor());
    if (eastChunk != nullptr) eastChunk->SetNeighborChunks(eastChunk->GetNorthNeighbor(), eastChunk->GetSouthNeighbor(), eastChunk->GetEastNeighbor(), nullptr);
    if (westChunk != nullptr) westChunk->SetNeighborChunks(westChunk->GetNorthNeighbor(), westChunk->GetSouthNeighbor(), nullptr, westChunk->GetWestNeighbor());
}

//----------------------------------------------------------------------------------------------------
void World::ToggleGlobalChunkDebugDraw()
{
    m_globalChunkDebugDraw = !m_globalChunkDebugDraw;
    for (auto& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->SetDebugDraw(m_globalChunkDebugDraw);
        }
    }
}