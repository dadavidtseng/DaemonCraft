//----------------------------------------------------------------------------------------------------
// World.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <mutex>

#include "Game/Framework/GameCommon.hpp"  // For DebugVisualizationMode

struct IntVec2;
struct IntVec3;
struct Vec3;
class Camera;
class Chunk;
class ChunkGenerateJob;
class ChunkLoadJob;
class ChunkMeshJob;
class ChunkSaveJob;

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
constexpr int CHUNK_ACTIVATION_RANGE    = 480;
constexpr int CHUNK_DEACTIVATION_RANGE  = CHUNK_ACTIVATION_RANGE + 16 + 16; // CHUNK_SIZE_X + CHUNK_SIZE_Y
constexpr int CHUNK_ACTIVATION_RADIUS_X = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_X
constexpr int CHUNK_ACTIVATION_RADIUS_Y = 1 + (CHUNK_ACTIVATION_RANGE / 16); // CHUNK_SIZE_Y
// NOTE: MAX_ACTIVE_CHUNKS removed - deactivation at CHUNK_DEACTIVATION_RANGE provides natural memory limit

//----------------------------------------------------------------------------------------------------
// Chunk Preloading Constants (Phase 0, Task 0.7) - Smart directional preloading
//----------------------------------------------------------------------------------------------------
constexpr float PRELOAD_VELOCITY_THRESHOLD = 1.0f;    // Minimum velocity (m/s) to trigger directional preloading
constexpr int   PRELOAD_LOOKAHEAD_CHUNKS   = 3;       // Number of chunks to preload ahead of movement direction
constexpr float PRELOAD_PRIORITY_BOOST     = 100.0f;  // Priority boost for chunks in movement direction

//----------------------------------------------------------------------------------------------------
// Fixed World Bounds (Phase 0, Task 0.6) - For predictable testing and experimentation
//----------------------------------------------------------------------------------------------------
// World extends from MIN_CHUNK_COORD to MAX_CHUNK_COORD (inclusive) in both X and Y
// Example: [-8, 7] creates a 16x16 chunk world (256x256 blocks with CHUNK_SIZE_X=16)
// With CHUNK_SIZE_X/Y=32, this creates 512x512 blocks (16x16 chunks)
constexpr int WORLD_MIN_CHUNK_X = -8;   // Minimum chunk X coordinate (inclusive)
constexpr int WORLD_MAX_CHUNK_X = 7;    // Maximum chunk X coordinate (inclusive)
constexpr int WORLD_MIN_CHUNK_Y = -8;   // Minimum chunk Y coordinate (inclusive)
constexpr int WORLD_MAX_CHUNK_Y = 7;    // Maximum chunk Y coordinate (inclusive)

// Calculated world dimensions
constexpr int WORLD_SIZE_CHUNKS_X = (WORLD_MAX_CHUNK_X - WORLD_MIN_CHUNK_X + 1); // 16 chunks wide
constexpr int WORLD_SIZE_CHUNKS_Y = (WORLD_MAX_CHUNK_Y - WORLD_MIN_CHUNK_Y + 1); // 16 chunks deep
constexpr int TOTAL_WORLD_CHUNKS  = WORLD_SIZE_CHUNKS_X * WORLD_SIZE_CHUNKS_Y;   // 256 chunks total

//----------------------------------------------------------------------------------------------------
// Job Queue Limiting - Prevent overwhelming the worker threads
//----------------------------------------------------------------------------------------------------
constexpr int MAX_PENDING_GENERATE_JOBS = 128;  // Maximum chunk generation jobs in flight (increased from 16)
constexpr int MAX_PENDING_LOAD_JOBS     = 16;   // Maximum chunk load jobs in flight (increased from 4)
constexpr int MAX_PENDING_MESH_JOBS     = 16;   // Maximum chunk mesh jobs in flight
constexpr int MAX_PENDING_SAVE_JOBS     = 4;    // Maximum chunk save jobs in flight

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

    void Update(float deltaSeconds);
    void Render() const;

    void    ActivateChunk(IntVec2 const& chunkCoords);
    void    DeactivateChunk(IntVec2 const& chunkCoords, bool forceSynchronousSave = false);
    void    DeactivateAllChunks(bool forceSynchronousSave = false); // For debug F8 and shutdown
    void    RegenerateAllChunks(); // For ImGui "Regenerate Chunks" - forces fresh terrain generation
    void    ToggleGlobalChunkDebugDraw(); // For debug F2 key

    // Debug Visualization (Phase 0, Task 0.4)
    void    SetDebugVisualizationMode(DebugVisualizationMode mode);
    DebugVisualizationMode GetDebugVisualizationMode() const { return m_debugVisualizationMode; }
    bool    SetBlockAtGlobalCoords(IntVec3 const& globalCoords, uint8_t blockTypeIndex); // Set block at world position
    uint8_t GetBlockTypeAtGlobalCoords(IntVec3 const& globalCoords) const; // Get block type at world position
    Chunk*  GetChunk(IntVec2 const& chunkCoords) const;

    // Debug information getters
    int GetActiveChunkCount() const;
    int GetTotalVertexCount() const;
    int GetTotalIndexCount() const;
    int GetPendingGenerateJobCount() const;
    int GetPendingLoadJobCount() const;
    int GetPendingSaveJobCount() const;

    // Digging and placing methods
    bool    DigBlockAtCameraPosition(Vec3 const& cameraPos); // LMB - dig highest non-air block at or below camera
    bool    PlaceBlockAtCameraPosition(Vec3 const& cameraPos, uint8_t blockType); // RMB - place block above highest non-air block
    IntVec3 FindHighestNonAirBlockAtOrBelow(Vec3 const& position) const; // Helper to find the highest solid block

    // Chunk management helper methods
    Vec3    GetCameraPosition() const;
    Vec3    GetPlayerVelocity() const;  // For directional preloading (Task 0.7)
    float   GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const;
    IntVec2 FindNearestMissingChunkInRange(Vec3 const& cameraPos) const;
    IntVec2 FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const;
    Chunk*  FindNearestDirtyChunk(Vec3 const& cameraPos) const;

    // Asynchronous job processing
    void ProcessCompletedJobs();  // Consolidated processor for all job types
    void ProcessDirtyChunkMeshes();
    void SubmitChunkForGeneration(Chunk* chunk);
    void SubmitChunkForMeshGeneration(Chunk* chunk);
    void SubmitChunkForLoading(Chunk* chunk);
    void SubmitChunkForSaving(Chunk* chunk);

private:
    //----------------------------------------------------------------------------------------------------
    // Active Chunks - Fully loaded and renderable chunks (main thread only)
    //----------------------------------------------------------------------------------------------------
    /// @brief Active chunks stored in hash map for O(1) lookup
    std::unordered_map<IntVec2, Chunk*> m_activeChunks;
    mutable std::mutex m_activeChunksMutex;  // Protects m_activeChunks from concurrent access

    //----------------------------------------------------------------------------------------------------
    // Non-Active Chunks - Chunks being processed by worker threads (REQUIRED by assignment)
    //----------------------------------------------------------------------------------------------------
    // This separate container holds chunks that are:
    // - Being generated by generic worker threads (TERRAIN_GENERATING state)
    // - Being loaded by I/O worker thread (LOADING state)
    // - Being saved by I/O worker thread (SAVING state)
    //
    // Assignment Requirement: "World owns and maintains a separate std::set of chunks which have been
    // created and queued for generation or loading or saving but are not currently active."
    std::set<Chunk*> m_nonActiveChunks;
    mutable std::mutex m_nonActiveChunksMutex;  // Protects m_nonActiveChunks from concurrent access

    //----------------------------------------------------------------------------------------------------
    // Job Management - Track pending/executing jobs for asynchronous chunk operations
    //----------------------------------------------------------------------------------------------------
    std::vector<ChunkGenerateJob*> m_chunkGenerationJobs;
    std::vector<ChunkLoadJob*>     m_chunkLoadJobs;
    std::vector<ChunkMeshJob*>     m_chunkMeshJobs;
    std::vector<ChunkSaveJob*>     m_chunkSaveJobs;
    mutable std::mutex m_jobListsMutex;  // Protects all job vectors from concurrent access

    std::unordered_set<IntVec2> m_queuedGenerateChunks;  // Track which chunks are queued for generation
    mutable std::mutex m_queuedChunksMutex;  // Protects m_queuedGenerateChunks from concurrent access

    std::vector<Chunk*> m_dirtyChunks;  // Chunks needing mesh rebuild (accessed only on main thread)

    //----------------------------------------------------------------------------------------------------
    // Game State
    //----------------------------------------------------------------------------------------------------
    // Global debug state for all chunks
    bool m_globalChunkDebugDraw = false;

    // Debug visualization mode (Phase 0, Task 0.4)
    DebugVisualizationMode m_debugVisualizationMode = DebugVisualizationMode::NORMAL_TERRAIN;

    // Chunk management helper methods
    bool ChunkExistsOnDisk(IntVec2 const& chunkCoords) const;
    bool LoadChunkFromDisk(Chunk* chunk) const;
    bool SaveChunkToDisk(Chunk* chunk) const;
    void UpdateNeighborPointers(IntVec2 const& chunkCoords);
    void ClearNeighborReferences(IntVec2 const& chunkCoords);
};
