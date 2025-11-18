//----------------------------------------------------------------------------------------------------
// World.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"

#include <algorithm>
#include <chrono>      // For std::chrono::milliseconds
#include <cmath>       // For cosf, fmod in day/night cycle (Assignment 5 Phase 9)
#include <filesystem>
#include <thread>      // For std::this_thread::sleep_for

#include "Engine/Core/Clock.hpp"  // Assignment 5 Phase 4: For GetTotalSeconds()
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Core/FileUtils.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/IntVec3.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Math/Vec3.hpp"
#include "Engine/Renderer/Renderer.hpp"  // Assignment 5 Phase 8: For shader and constant buffer
#include "Engine/Renderer/Shader.hpp"    // Assignment 5 Phase 8: For Shader class
#include "Engine/Renderer/ConstantBuffer.hpp"  // Assignment 5 Phase 8: For ConstantBuffer class
#include "Game/Framework/App.hpp"
#include "Game/Framework/Block.hpp"  // Assignment 5 Phase 4: For BlockIterator
#include "Game/Framework/BlockIterator.hpp"  // Assignment 5 Phase 4: For dirty light queue
#include "Game/Framework/Chunk.hpp"
#include "Game/Definition/BlockDefinition.hpp"  // Assignment 5 Phase 5: For IsOpaque(), IsEmissive()
#include "Game/Framework/ChunkGenerateJob.hpp"
#include "Game/Framework/ChunkLoadJob.hpp"
#include "Game/Framework/ChunkMeshJob.hpp"
#include "Game/Framework/ChunkSaveJob.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "ThirdParty/Noise/SmoothNoise.hpp"

//----------------------------------------------------------------------------------------------------
// Hash function implementation for IntVec2
//----------------------------------------------------------------------------------------------------
size_t std::hash<IntVec2>::operator()(IntVec2 const& vec) const noexcept
{
    return std::hash<int>()(vec.x) ^ (std::hash<int>()(vec.y) << 1);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 8: World shader constant buffer structure
// Must match layout in World.hlsl cbuffer WorldConstants : register(b8)
// 16-byte aligned structure
// Assignment 5: Updated to match A5 specification requirements
//----------------------------------------------------------------------------------------------------
struct WorldConstants
{
    float cameraPosition[4];    // Camera world position (for fog distance calculation)
    float indoorLightColor[4];  // Indoor light color (default: 255, 230, 204)
    float outdoorLightColor[4]; // Outdoor light color (varies with day/night)
    float skyColor[4];          // Sky/fog color (for distance fog)
    float fogNearDistance;      // Fog near distance
    float fogFarDistance;       // Fog far distance
    float gameTime;             // Current game time in seconds
    float padding;              // 16-byte alignment
};

//----------------------------------------------------------------------------------------------------
World::World()
{
    // Assignment 5 Phase 8: Load World shader and create constant buffer
    m_worldShader = g_renderer->CreateOrGetShaderFromFile("Data/Shaders/World");
    m_worldConstantBuffer = g_renderer->CreateConstantBuffer(sizeof(WorldConstants));

    // BUG HUNT: Verify shader was loaded successfully
    if (m_worldShader == nullptr)
    {
        DebuggerPrintf("[SHADER ERROR] World.hlsl FAILED to load! Will fall back to Default.hlsl\n");
    }
    else
    {
        DebuggerPrintf("[SHADER OK] World.hlsl loaded successfully at 0x%p\n", m_worldShader);
    }

    if (m_worldConstantBuffer == nullptr)
    {
        DebuggerPrintf("[SHADER ERROR] World constant buffer FAILED to create!\n");
    }
    else
    {
        DebuggerPrintf("[SHADER OK] World constant buffer created successfully\n");
    }
}

//----------------------------------------------------------------------------------------------------
World::~World()
{
    // CRITICAL: Cancel all pending save jobs BEFORE deactivating chunks
    // During shutdown, async save jobs will never complete, causing memory leaks
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        for (ChunkSaveJob* saveJob : m_chunkSaveJobs)
        {
            if (saveJob != nullptr)
            {
                Chunk* chunk = saveJob->GetChunk();
                if (chunk != nullptr)
                {
                    // Force synchronous save before deleting job
                    chunk->SaveToDisk();
                    delete chunk;
                }
                delete saveJob;
            }
        }
        m_chunkSaveJobs.clear();
    }

    // Clean up orphaned chunks in m_nonActiveChunks (being processed by workers)
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        for (Chunk* chunk : m_nonActiveChunks)
        {
            if (chunk != nullptr)
            {
                // Save if needed, then delete
                if (chunk->GetNeedsSaving())
                {
                    chunk->SaveToDisk();
                }
                delete chunk;  // Releases DirectX resources in ~Chunk() destructor
            }
        }
        m_nonActiveChunks.clear();
    }

    // Assignment 5 Phase 10: Clear mesh rebuild tracking set to prevent dangling pointers
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        m_chunksNeedingMeshRebuild.clear();
    }

    // Deactivate all active chunks (saves modified chunks synchronously during shutdown)
    DeactivateAllChunks(true);  // forceSynchronousSave=true prevents async jobs during shutdown

    // Assignment 5 Phase 8: Clean up shader resources
    // Note: Shader is owned by Renderer (CreateOrGetShaderFromFile uses a cache), don't delete it
    // Only delete the constant buffer we own
    if (m_worldConstantBuffer != nullptr)
    {
        delete m_worldConstantBuffer;
        m_worldConstantBuffer = nullptr;
    }
}

//----------------------------------------------------------------------------------------------------
void World::Update(float const deltaSeconds)
{
    if (g_game == nullptr) return;

    // Assignment 5 Phase 9: Update day/night cycle
    // KEYCODE_Y accelerates time by 60x for testing day/night transitions
    float timeMultiplier = 1.0f;
    if (g_input && g_input->IsKeyDown(KEYCODE_Y))
    {
        timeMultiplier = 60.0f;  // Hold Y to fast-forward time (60x speed = 4 seconds per full cycle)
    }
    m_gameTime += deltaSeconds * timeMultiplier;

    constexpr float DAY_NIGHT_CYCLE_DURATION = 240.0f;  // 4 minutes per full day/night cycle
    constexpr float LOCAL_PI = 3.14159265359f;
    float cyclePosition = fmod(m_gameTime, DAY_NIGHT_CYCLE_DURATION) / DAY_NIGHT_CYCLE_DURATION;

    // Outdoor brightness: 1.0 at noon (cyclePosition=0.5), 0.2 at midnight (cyclePosition=0.0 or 1.0)
    // Use cosine wave: cos(0) = 1 (midnight), cos(PI) = -1 (noon)
    float cosValue = cosf(cyclePosition * 2.0f * LOCAL_PI);  // Range [-1, 1], peaks at midnight
    m_outdoorBrightness = RangeMap(cosValue, 1.0f, -1.0f, 0.2f, 1.0f);  // Map to [0.2, 1.0]

    // Lightning strikes (Perlin noise based on time)
    // Very rare: only trigger when noise > 0.95 (approximately 5% of time during storms)
    float lightningNoise = Compute2dPerlinNoise(m_gameTime * 10.0f, 0.0f, 2.0f, 3);
    if (lightningNoise > 0.95f)
    {
        m_outdoorBrightness = 1.5f;  // Bright flash (exceeds normal max for dramatic effect)
    }

    // Update all active chunks first
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& chunkPair : m_activeChunks)
        {
            if (chunkPair.second != nullptr)
            {
                chunkPair.second->Update(deltaSeconds);
            }
        }
    }

    // Process asynchronous job operations
    ProcessCompletedJobs();  // Process all completed jobs (generation, load, save)

    // Assignment 5 Phase 4: Process dirty light queue with 16ms budget
    // CRITICAL: Process lighting BEFORE mesh rebuilds to prevent progressive brightening
    // Meshes read lighting values, so lighting must propagate first
    ProcessDirtyLighting(0.016f);

    // Process dirty meshes AFTER lighting propagation
    ProcessDirtyChunkMeshes();

    // Get camera position for chunk management decisions
    Vec3 const cameraPos = GetCameraPosition();

    // At startup or when very few chunks exist, activate multiple chunks per frame
    // to quickly populate the world around the player
    size_t activeChunkCount;
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        activeChunkCount = m_activeChunks.size();
    }

    // DEBUG MODE: Fixed world generation (Task 5B.4)
    // Generate exactly 256 chunks (16×16 grid) centered at origin, then stop
    if (DEBUG_FIXED_WORLD_GEN)
    {
        // Generate all 256 chunks at startup (aggressive burst)
        int chunksToActivateThisFrame = (activeChunkCount < 256) ? 50 : 0;

        // 1. Check if initial world generation is complete (all 256 chunks activated)
        // CRITICAL FIX: Defer ALL mesh rebuilding until world gen is complete
        // This prevents progressive brightening from cross-chunk lighting propagation
        if (!m_initialWorldGenComplete && activeChunkCount >= 256)
        {
            m_initialWorldGenComplete = true;
            DebuggerPrintf("[WORLD GEN COMPLETE] All 256 chunks activated, enabling mesh rebuilding\n");
        }

        // 2. Only rebuild meshes after initial world gen is complete AND light queue is empty
        if (m_initialWorldGenComplete && m_dirtyLightQueue.empty())
        {
            // Assignment 5 Phase 10 FIX: Mark chunks with changed lighting as mesh-dirty FIRST
            {
                std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
                if (!m_chunksNeedingMeshRebuild.empty())
                {
                    for (Chunk* chunk : m_chunksNeedingMeshRebuild)
                    {
                        if (chunk)
                        {
                            chunk->SetIsMeshDirty(true);
                        }
                    }
                    m_chunksNeedingMeshRebuild.clear();
                }
            }

            Chunk* nearestDirtyChunk = FindNearestDirtyChunk(cameraPos);
            if (nearestDirtyChunk != nullptr)
            {
                // DEBUG: Log WHY mesh is being rebuilt
                static int rebuildCount = 0;
                rebuildCount++;
                if (rebuildCount <= 20)  // Log first 20 rebuilds
                {
                    IntVec2 coords = nearestDirtyChunk->GetChunkCoords();
                    DebuggerPrintf("[MESH REBUILD #%d] Chunk(%d,%d) rebuilding mesh\n", rebuildCount, coords.x, coords.y);
                }
                nearestDirtyChunk->RebuildMesh();
                nearestDirtyChunk->SetIsMeshDirty(false);
            }
        }
        else if (!m_initialWorldGenComplete)
        {
            static int deferCount = 0;
            deferCount++;
            if (deferCount <= 10)  // Log first 10 deferrals
            {
                DebuggerPrintf("[MESH DEFER] Waiting for world gen complete (%zu/256 chunks), queue size=%zu\n",
                              activeChunkCount, m_dirtyLightQueue.size());
            }
        }

        // 3. Generate fixed 16×16 chunk grid only (never generate beyond this)
        for (int i = 0; i < chunksToActivateThisFrame; i++)
        {
            // Find next chunk in fixed grid that hasn't been generated yet
            bool foundChunk = false;
            for (int chunkX = -DEBUG_FIXED_WORLD_HALF_SIZE; chunkX < DEBUG_FIXED_WORLD_HALF_SIZE && !foundChunk; chunkX++)
            {
                for (int chunkY = -DEBUG_FIXED_WORLD_HALF_SIZE; chunkY < DEBUG_FIXED_WORLD_HALF_SIZE && !foundChunk; chunkY++)
                {
                    IntVec2 chunkCoords(chunkX, chunkY);

                    // Check if this chunk already exists
                    bool exists = false;
                    {
                        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
                        exists = (m_activeChunks.find(chunkCoords) != m_activeChunks.end());
                    }
                    if (!exists)
                    {
                        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                        for (Chunk* chunk : m_nonActiveChunks)
                        {
                            if (chunk->GetChunkCoords() == chunkCoords)
                            {
                                exists = true;
                                break;
                            }
                        }
                    }

                    if (!exists)
                    {
                        ActivateChunk(chunkCoords);
                        foundChunk = true;
                    }
                }
            }
            if (!foundChunk) break; // All 256 chunks generated
        }

        // 3. NEVER deactivate chunks in debug mode (keep all 256 active)
        return; // Skip normal chunk management
    }

    // NORMAL MODE: Dynamic world generation
    // Tiered burst mode activation thresholds for responsive world generation
    // Adjusted for CHUNK_ACTIVATION_RANGE = 480
    int chunksToActivateThisFrame = 1; // Default: steady state (minimal activation)

    if (activeChunkCount < 400)
    {
        chunksToActivateThisFrame = 50; // AGGRESSIVE burst: fill initial activation range quickly
    }
    else if (activeChunkCount < 1200)
    {
        chunksToActivateThisFrame = 30; // HIGH burst: continue populating surrounding areas
    }
    else if (activeChunkCount < 2500)
    {
        chunksToActivateThisFrame = 15; // MEDIUM burst: approaching full coverage
    }
    else if (activeChunkCount < 5000)
    {
        chunksToActivateThisFrame = 5; // LOW burst: taper off to steady state
    }
    // else: steady state (1 per frame) - world is well-populated

    // Execute chunk management actions
    // Priority: 1) Regenerate dirty chunk, 2) Activate missing chunks, 3) Deactivate distant chunk

    // 1. Check for dirty chunks and regenerate the single nearest dirty chunk
    // CRITICAL FIX: Only rebuild if light queue is empty to prevent progressive brightening
    if (m_dirtyLightQueue.empty())
    {
        // Assignment 5 Phase 10 FIX: Mark chunks with changed lighting as mesh-dirty FIRST
        {
            std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
            if (!m_chunksNeedingMeshRebuild.empty())
            {
                for (Chunk* chunk : m_chunksNeedingMeshRebuild)
                {
                    if (chunk)
                    {
                        chunk->SetIsMeshDirty(true);
                    }
                }
                m_chunksNeedingMeshRebuild.clear();
            }
        }

        Chunk* nearestDirtyChunk = FindNearestDirtyChunk(cameraPos);

        if (nearestDirtyChunk != nullptr)
        {
            static int rebuildCount = 0;
            rebuildCount++;
            if (rebuildCount <= 20)  // Log first 20 rebuilds
            {
                IntVec2 coords = nearestDirtyChunk->GetChunkCoords();
                DebuggerPrintf("[MESH REBUILD #%d] Chunk(%d,%d) rebuilding mesh\n", rebuildCount, coords.x, coords.y);
            }
            nearestDirtyChunk->RebuildMesh();
            nearestDirtyChunk->SetIsMeshDirty(false);
            // Continue to other operations (removed early return)
        }
    }
    else
    {
        static int deferCount = 0;
        deferCount++;
        if (deferCount <= 10)  // Log first 10 deferrals
        {
            DebuggerPrintf("[MESH DEFER] Skipping mesh rebuild, dirty light queue size=%zu\n", m_dirtyLightQueue.size());
        }
    }

    // 2. Activate missing chunks within activation range
    for (int i = 0; i < chunksToActivateThisFrame; i++)
    {
        IntVec2 const nearestMissingChunk = FindNearestMissingChunkInRange(cameraPos);

        if (nearestMissingChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
        {
            ActivateChunk(nearestMissingChunk);
        }
        else
        {
            break; // No more chunks to activate in range
        }
    }

    // 3. Deactivate the farthest active chunk if outside deactivation range
    IntVec2 const farthestChunk = FindFarthestActiveChunkOutsideDeactivationRange(cameraPos);

    if (farthestChunk != IntVec2(INT_MAX, INT_MAX)) // Valid chunk found
    {
        DeactivateChunk(farthestChunk);
    }
}

//----------------------------------------------------------------------------------------------------
void World::Render() const
{
    // BUG HUNT: Log shader binding status every 300 frames (every ~5 seconds at 60fps)
    static int renderCallCount = 0;
    renderCallCount++;
    if (renderCallCount % 300 == 1)
    {
        if (m_worldShader == nullptr)
        {
            DebuggerPrintf("[RENDER WARNING] m_worldShader is NULL! Using Default.hlsl fallback!\n");
        }
        else
        {
            DebuggerPrintf("[RENDER OK] Binding World.hlsl shader (call #%d)\n", renderCallCount);
        }
    }

    // Assignment 5 Phase 8: Bind World shader and update lighting constants
    if (m_worldShader != nullptr && m_worldConstantBuffer != nullptr)
    {
        g_renderer->BindShader(m_worldShader);

        // DEBUG: Verify shader is actually being used
        static int verifyCount = 0;
        if (++verifyCount == 1)
        {
            DebuggerPrintf("[SHADER VERIFY] m_worldShader=%p, m_worldConstantBuffer=%p\n", m_worldShader, m_worldConstantBuffer);
        }
    }
    else
    {
        // BUG HUNT: Log when shader binding FAILS (World shader/CB is null)
        static int failCount = 0;
        if (failCount < 10)
        {
            DebuggerPrintf("[SHADER BIND FAIL #%d] m_worldShader=%p, m_worldConstantBuffer=%p - chunks will use Default.hlsl!\n",
                          failCount, m_worldShader, m_worldConstantBuffer);
            failCount++;
        }
        // Chunks will render with whatever shader was previously bound (likely Default.hlsl)
        // This causes "bright chunks" bug because Default.hlsl doesn't read WorldConstants CB
    }

    // Continue with constant buffer update ONLY if shader was bound successfully
    if (m_worldShader != nullptr && m_worldConstantBuffer != nullptr)
    {

        // Update world constants for day/night cycle (A5 specification compliant)
        WorldConstants worldConstants;

        // Camera position (for fog distance calculation)
        Vec3 cameraPos3 = GetCameraPosition();
        worldConstants.cameraPosition[0] = cameraPos3.x;
        worldConstants.cameraPosition[1] = cameraPos3.y;
        worldConstants.cameraPosition[2] = cameraPos3.z;
        worldConstants.cameraPosition[3] = 1.0f;

        // Indoor light color: warm white (255, 230, 204) normalized to [0, 1]
        worldConstants.indoorLightColor[0] = 1.0f;
        worldConstants.indoorLightColor[1] = 0.902f;
        worldConstants.indoorLightColor[2] = 0.8f;
        worldConstants.indoorLightColor[3] = 1.0f;

        // Outdoor light color: varies with day/night cycle
        // At noon (brightness=1.0): pure white (255, 255, 255)
        // At midnight (brightness=0.2): dim blue (51, 51, 76) = 0.2 * white blended with blue tint
        // Interpolate between dim blue-ish color at night and white at day
        float dayFactor = (m_outdoorBrightness - 0.2f) / 0.8f;  // 0.0 at midnight, 1.0 at noon
        dayFactor = fmaxf(0.0f, fminf(1.0f, dayFactor));  // Clamp to [0, 1]

        // Night color: VERY DARK blue - matches reference (only glowstone visible at night)
        // Reference has almost black outdoor lighting, only indoor lights visible
        // Day color: pure white (1.0, 1.0, 1.0)
        Vec3 nightColor(0.02f, 0.02f, 0.04f);  // Changed from (0.2, 0.2, 0.3) - was too bright
        Vec3 dayColor(1.0f, 1.0f, 1.0f);
        Vec3 outdoorRGB = nightColor + (dayColor - nightColor) * dayFactor;
        worldConstants.outdoorLightColor[0] = outdoorRGB.x;
        worldConstants.outdoorLightColor[1] = outdoorRGB.y;
        worldConstants.outdoorLightColor[2] = outdoorRGB.z;
        worldConstants.outdoorLightColor[3] = 1.0f;

        // Sky/fog color: light blue during day, dark blue at night
        Vec3 daySkyColor(0.6f, 0.75f, 0.95f);   // Light blue sky
        Vec3 nightSkyColor(0.15f, 0.15f, 0.25f); // Brightened: was (0.05, 0.05, 0.15), now more visible at night
        Vec3 skyRGB = nightSkyColor + (daySkyColor - nightSkyColor) * dayFactor;
        // Store fog max alpha in skyColor.a (0.8 = 80% fog blending at max distance)
        worldConstants.skyColor[0] = skyRGB.x;
        worldConstants.skyColor[1] = skyRGB.y;
        worldConstants.skyColor[2] = skyRGB.z;
        worldConstants.skyColor[3] = 0.8f;

        // Fog distances based on chunk activation range
        // Near distance: start fog at 60% of activation range (gives clear vision before fading)
        // Far distance: full fog at activation range boundary
        float activationRange = static_cast<float>(CHUNK_ACTIVATION_RANGE);
        worldConstants.fogNearDistance = activationRange * 0.6f;  // 480 * 0.6 = 288 blocks
        worldConstants.fogFarDistance = activationRange;          // 480 blocks

        worldConstants.gameTime = m_gameTime;
        worldConstants.padding = 0.0f;

        g_renderer->CopyCPUToGPU(&worldConstants, sizeof(WorldConstants), m_worldConstantBuffer);
        g_renderer->BindConstantBuffer(8, m_worldConstantBuffer);  // Register b8 for WorldConstants

        // DEBUG: Verify constant buffer operations every 60 frames (~1 second at 60fps)
        static int cbVerifyCount = 0;
        if (++cbVerifyCount % 60 == 0)
        {
            DebuggerPrintf("[CB UPDATE #%d] Time=%.2f CyclePos=%.3f DayFactor=%.3f OutdoorBright=%.3f OutdoorColor=(%.2f,%.2f,%.2f) → CB=%p\n",
                          cbVerifyCount,
                          m_gameTime,
                          fmod(m_gameTime, 240.0f) / 240.0f,
                          dayFactor,
                          m_outdoorBrightness,
                          worldConstants.outdoorLightColor[0],
                          worldConstants.outdoorLightColor[1],
                          worldConstants.outdoorLightColor[2],
                          m_worldConstantBuffer);
        }

        // DEBUG: Log current outdoor brightness value every 2 seconds
        static float lastLogTime = 0.0f;
        if (m_gameTime - lastLogTime >= 2.0f)
        {
            DebuggerPrintf("RENDER DEBUG: Time=%.2f, OutdoorColor=(%.2f,%.2f,%.2f), DayFactor=%.3f, CyclePos=%.3f\n",
                          m_gameTime,
                          worldConstants.outdoorLightColor[0],
                          worldConstants.outdoorLightColor[1],
                          worldConstants.outdoorLightColor[2],
                          dayFactor,
                          fmodf(m_gameTime, 240.0f) / 240.0f);
            lastLogTime = m_gameTime;
        }
    }

    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    for (std::pair<IntVec2 const, Chunk*> const& chunkPair : m_activeChunks)
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
    // Check if chunk is already active (with mutex protection)
    {
        std::lock_guard lock(m_activeChunksMutex);
        if (m_activeChunks.contains(chunkCoords))
        {
            return; // Already active
        }
    }

    // Check if chunk is already queued for generation (with mutex protection)
    {
        std::lock_guard lock(m_queuedChunksMutex);
        if (m_queuedGenerateChunks.find(chunkCoords) != m_queuedGenerateChunks.end())
        {
            return; // Already queued for generation
        }
    }

    // Create new chunk
    Chunk* newChunk = new Chunk(chunkCoords);

    // Set initial state
    newChunk->SetState(ChunkState::ACTIVATING);

    // Try to load from disk first using asynchronous I/O job
    if (ChunkExistsOnDisk(chunkCoords))
    {
        // Submit LoadChunkJob to I/O worker thread
        SubmitChunkForLoading(newChunk);
    }
    else
    {
        // No save file exists, submit for asynchronous generation
        SubmitChunkForGeneration(newChunk);
    }
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateChunk(IntVec2 const& chunkCoords, bool forceSynchronousSave)
{
    // CRITICAL FIX: Copy chunkCoords to local variable BEFORE erasing from map
    // Problem: chunkCoords is a reference to map key (it->first)
    // When we erase(it), the reference becomes invalid → crash when used later
    IntVec2 localChunkCoords = chunkCoords;

    Chunk* chunk = nullptr;
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        auto const                  it = m_activeChunks.find(localChunkCoords);

        if (it == m_activeChunks.end())
        {
            return; // Chunk not active
        }

        chunk = it->second;
        if (chunk == nullptr)
        {
            m_activeChunks.erase(it);
            return;
        }

        // Remove from active chunks map
        m_activeChunks.erase(it);
    }

    // Clear neighbor pointers (outside mutex - only affects this chunk's members)
    chunk->ClearNeighborPointers();

    // Assignment 5 Phase 10: Remove chunk from mesh rebuild tracking set
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        m_chunksNeedingMeshRebuild.erase(chunk);
    }

    // Update neighbors to remove references to this chunk
    ClearNeighborReferences(localChunkCoords);

    // Save to disk if needed
    if (chunk->GetNeedsSaving())
    {
        if (forceSynchronousSave)
        {
            // Synchronous save during shutdown - don't use async jobs
            chunk->SaveToDisk();
            delete chunk;
        }
        else
        {
            // Normal async save using I/O worker thread
            SubmitChunkForSaving(chunk);
        }
    }
    else
    {
        // No need to save, delete immediately
        delete chunk;
    }
}

//----------------------------------------------------------------------------------------------------
void World::DeactivateAllChunks(bool forceSynchronousSave)
{
    // Save all modified chunks before deactivating
    while (!m_activeChunks.empty())
    {
        auto const it = m_activeChunks.begin();

        DeactivateChunk(it->first, forceSynchronousSave);
    }
}

//----------------------------------------------------------------------------------------------------
void World::RegenerateAllChunks()
{
    // Purpose: Force all active chunks to regenerate with fresh procedural terrain
    // This is critical for rapid iteration when tuning world generation parameters via ImGui
    //
    // Strategy:
    // 1. Cancel all pending mesh jobs (prevents dangling pointers when chunks are deleted)
    // 2. Mark all active chunks as NOT needing save (prevents old terrain from being written to disk)
    // 3. Deactivate all chunks (which will skip saving since needsSaving=false)
    // 4. Natural chunk activation system will regenerate chunks with current generation parameters

    // Clear the queued chunks tracking set FIRST to stop new jobs from being submitted
    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        m_queuedGenerateChunks.clear();
    }

    // CRITICAL FIX: Wait for ALL currently executing jobs to complete BEFORE deleting any jobs
    // Problem: Deleting jobs/chunks while worker threads are still executing causes crashes
    // - Worker threads access freed chunk memory → 0x01010101 crash pattern
    // - SetBlock() writes to freed memory → corrupted chunks saved to disk
    //
    // Solution: Poll until all workers finish their current Execute() calls
    // This ensures no worker thread is accessing chunk memory before we delete it
    //
    // Why this works:
    // 1. We cleared m_queuedGenerateChunks - no new jobs will be submitted
    // 2. Workers will finish their current job and find no more queued jobs
    // 3. GetExecutingJobCount() will drop to 0 when all jobs complete
    // 4. Only THEN is it safe to delete job objects and chunks
    //
    // IMPORTANT: Must wait BEFORE deleting any jobs (including mesh jobs)
    // Deleting a job while it's executing causes orphaned threads and infinite hang
    while (g_jobSystem->GetExecutingJobCount() > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
        // Give worker threads time to finish GenerateTerrain(), LoadFromDisk(), RebuildMesh(), etc.
        // This prevents accessing freed memory (chunk coordinates, block array)
    }

    // CRITICAL FIX: Retrieve ALL completed jobs from JobSystem BEFORE deleting chunks
    // Problem: Completed jobs are in JobSystem's completedJobs queue, NOT in our tracking lists
    // If we delete chunks first, ProcessCompletedJobs() will try to access deleted chunks
    //
    // Solution: Retrieve completed jobs, remove from tracking lists, then delete ALL jobs together
    std::vector<Job*> completedJobs = g_jobSystem->RetrieveAllCompletedJobs();

    // Remove completed jobs from tracking lists to prevent double-delete
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);

        // Remove completed mesh jobs from tracking list
        for (Job* completedJob : completedJobs)
        {
            // Try to find and remove from mesh jobs list
            auto meshIt = std::find(m_chunkMeshJobs.begin(), m_chunkMeshJobs.end(),
                                   static_cast<ChunkMeshJob*>(completedJob));
            if (meshIt != m_chunkMeshJobs.end())
            {
                m_chunkMeshJobs.erase(meshIt);
                continue;  // Found in mesh jobs, skip other lists
            }

            // Try to find and remove from generation jobs list
            auto genIt = std::find(m_chunkGenerationJobs.begin(), m_chunkGenerationJobs.end(),
                                  static_cast<ChunkGenerateJob*>(completedJob));
            if (genIt != m_chunkGenerationJobs.end())
            {
                m_chunkGenerationJobs.erase(genIt);
                continue;  // Found in generation jobs, skip other lists
            }

            // Try to find and remove from load jobs list
            auto loadIt = std::find(m_chunkLoadJobs.begin(), m_chunkLoadJobs.end(),
                                   static_cast<ChunkLoadJob*>(completedJob));
            if (loadIt != m_chunkLoadJobs.end())
            {
                m_chunkLoadJobs.erase(loadIt);
            }
        }

        // NOW delete all remaining tracked jobs (not yet completed)
        for (ChunkMeshJob* meshJob : m_chunkMeshJobs)
        {
            if (meshJob != nullptr)
            {
                delete meshJob;
            }
        }
        m_chunkMeshJobs.clear();

        for (ChunkGenerateJob* genJob : m_chunkGenerationJobs)
        {
            if (genJob != nullptr)
            {
                delete genJob;
            }
        }
        m_chunkGenerationJobs.clear();

        for (ChunkLoadJob* loadJob : m_chunkLoadJobs)
        {
            if (loadJob != nullptr)
            {
                delete loadJob;
            }
        }
        m_chunkLoadJobs.clear();
    }

    // Finally, delete all completed jobs (removed from tracking lists above)
    for (Job* job : completedJobs)
    {
        delete job;
    }

    // NOW it's safe to delete orphaned chunks in m_nonActiveChunks
    // No worker thread is accessing this memory anymore
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        for (Chunk* chunk : m_nonActiveChunks)
        {
            if (chunk != nullptr)
            {
                delete chunk;  // Safe now - releases DirectX resources in ~Chunk() destructor
            }
        }
        m_nonActiveChunks.clear();
    }

    // Mark all chunks as not needing save
    // CRITICAL: This prevents RegenerateAllChunks() from writing old terrain to disk
    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto& chunkPair : m_activeChunks)
        {
            if (chunkPair.second != nullptr)
            {
                chunkPair.second->SetNeedsSaving(false);
            }
        }
    }

    // CRITICAL FIX: Delete ALL saved chunk files to force fresh terrain generation
    // This ensures chunks regenerate with NEW terrain instead of loading old saves
    // Bug: Without this, ActivateChunk() finds saved files and loads old terrain!
    try
    {
        std::string saveDir = "Saves/";
        if (std::filesystem::exists(saveDir))
        {
            // Iterate through all .chunk files in Saves/ directory
            for (auto const& entry : std::filesystem::directory_iterator(saveDir))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".chunk")
                {
                    try
                    {
                        std::filesystem::remove(entry.path());
                        DebuggerPrintf("Deleted chunk file: %s\n", entry.path().string().c_str());
                    }
                    catch (std::filesystem::filesystem_error const& e)
                    {
                        DebuggerPrintf("Failed to delete chunk file %s: %s\n",
                                       entry.path().string().c_str(), e.what());
                    }
                }
            }
        }
    }
    catch (std::filesystem::filesystem_error const& e)
    {
        DebuggerPrintf("Failed to iterate Saves/ directory: %s\n", e.what());
    }

    // Deactivate all chunks (they won't be saved due to SetNeedsSaving(false))
    DeactivateAllChunks();

    // Note: Chunks will automatically reactivate and regenerate during the next Update()
    // when the activation system detects missing chunks around the player
}

//----------------------------------------------------------------------------------------------------
void World::ToggleGlobalChunkDebugDraw()
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    m_globalChunkDebugDraw = !m_globalChunkDebugDraw;
    for (auto& chunkPair : m_activeChunks)
    {
        if (chunkPair.second != nullptr)
        {
            chunkPair.second->SetDebugDraw(m_globalChunkDebugDraw);
        }
    }
}

//----------------------------------------------------------------------------------------------------
void World::SetDebugVisualizationMode(DebugVisualizationMode mode)
{
    // Only regenerate if mode actually changed
    if (m_debugVisualizationMode == mode)
    {
        return;
    }

    m_debugVisualizationMode = mode;

    // Regenerate all chunks to apply new visualization
    // This ensures chunks display the selected noise layer
    RegenerateAllChunks();
}

//----------------------------------------------------------------------------------------------------
bool World::SetBlockAtGlobalCoords(IntVec3 const& globalCoords,
                                   uint8_t const  blockTypeIndex)
{
    // Get the chunk that contains this global coordinate
    IntVec2 chunkCoords = Chunk::GetChunkCoords(globalCoords);
    Chunk*  chunk       = GetChunk(chunkCoords);

    if (chunk == nullptr)
    {
        return false; // Chunk not active, cannot modify
    }

    // Convert global coordinates to local coordinates within the chunk
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(globalCoords);

    // Validate Z coordinate is within chunk bounds
    if (localCoords.z < 0 || localCoords.z > CHUNK_MAX_Z)
    {
        return false; // Z coordinate out of bounds
    }

    // Set the block using the chunk's SetBlock method (which handles save/mesh dirty flags and lighting)
    chunk->SetBlock(localCoords.x, localCoords.y, localCoords.z, blockTypeIndex, this);

    // Mark neighboring chunks as dirty if the modified block is on a chunk boundary
    // This ensures proper face culling updates across chunk edges
    if (localCoords.x == 0) // West boundary
    {
        IntVec2 westChunkCoords = IntVec2(chunkCoords.x - 1, chunkCoords.y);
        Chunk*  westChunk       = GetChunk(westChunkCoords);
        if (westChunk != nullptr)
        {
            westChunk->SetIsMeshDirty(true);
        }
    }
    else if (localCoords.x == CHUNK_MAX_X) // East boundary
    {
        IntVec2 eastChunkCoords = IntVec2(chunkCoords.x + 1, chunkCoords.y);
        Chunk*  eastChunk       = GetChunk(eastChunkCoords);
        if (eastChunk != nullptr)
        {
            eastChunk->SetIsMeshDirty(true);
        }
    }

    if (localCoords.y == 0) // South boundary
    {
        IntVec2 southChunkCoords = IntVec2(chunkCoords.x, chunkCoords.y - 1);
        Chunk*  southChunk       = GetChunk(southChunkCoords);
        if (southChunk != nullptr)
        {
            southChunk->SetIsMeshDirty(true);
        }
    }
    else if (localCoords.y == CHUNK_MAX_Y) // North boundary
    {
        IntVec2 northChunkCoords = IntVec2(chunkCoords.x, chunkCoords.y + 1);
        Chunk*  northChunk       = GetChunk(northChunkCoords);
        if (northChunk != nullptr)
        {
            northChunk->SetIsMeshDirty(true);
        }
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
uint8_t World::GetBlockTypeAtGlobalCoords(IntVec3 const& globalCoords) const
{
    // Get the chunk that contains this global coordinate
    IntVec2 chunkCoords = Chunk::GetChunkCoords(globalCoords);
    Chunk*  chunk       = GetChunk(chunkCoords);

    if (chunk == nullptr)
    {
        return 0; // Return air block if chunk not active
    }

    // Convert global coordinates to local coordinates within the chunk
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(globalCoords);

    // Validate Z coordinate is within chunk bounds
    if (localCoords.z < 0 || localCoords.z > CHUNK_MAX_Z)
    {
        return 0; // Return air block if out of bounds
    }

    // Get the block using the chunk's GetBlock method
    Block* block = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);

    if (block == nullptr)
    {
        return 0; // Return air block if invalid
    }

    return block->m_typeIndex;
}

//----------------------------------------------------------------------------------------------------
Chunk* World::GetChunk(IntVec2 const& chunkCoords) const
{
    std::lock_guard lock(m_activeChunksMutex);
    auto const      it = m_activeChunks.find(chunkCoords);

    if (it != m_activeChunks.end())
    {
        return it->second;
    }
    return nullptr;
}

//----------------------------------------------------------------------------------------------------
int World::GetActiveChunkCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    return (int)m_activeChunks.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetTotalVertexCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    int             totalVertices = 0;
    for (const auto& pair : m_activeChunks)
    {
        if (pair.second)
        {
            totalVertices += pair.second->GetVertexCount();
        }
    }
    return totalVertices;
}

//----------------------------------------------------------------------------------------------------
int World::GetTotalIndexCount() const
{
    std::lock_guard lock(m_activeChunksMutex);
    int             totalIndices = 0;
    for (const auto& pair : m_activeChunks)
    {
        if (pair.second)
        {
            totalIndices += pair.second->GetIndexCount();
        }
    }
    return totalIndices;
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingGenerateJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkGenerationJobs.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingLoadJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkLoadJobs.size();
}

//----------------------------------------------------------------------------------------------------
int World::GetPendingSaveJobCount() const
{
    std::lock_guard lock(m_jobListsMutex);
    return (int)m_chunkSaveJobs.size();
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
Vec3 World::GetPlayerVelocity() const
{
    if (g_game != nullptr)
    {
        return g_game->GetPlayerVelocity();
    }
    return Vec3::ZERO; // Fallback if no game instance
}

//----------------------------------------------------------------------------------------------------
float World::GetDistanceToChunkCenter(IntVec2 const& chunkCoords, Vec3 const& cameraPos) const
{
    IntVec2 chunkCenter   = Chunk::GetChunkCenter(chunkCoords);
    Vec3    chunkCenter3D = Vec3(static_cast<float>(chunkCenter.x), static_cast<float>(chunkCenter.y), cameraPos.z);

    // Only consider XY distance as specified
    float deltaX = chunkCenter3D.x - cameraPos.x;
    float deltaY = chunkCenter3D.y - cameraPos.y;
    return sqrtf(deltaX * deltaX + deltaY * deltaY);
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindNearestMissingChunkInRange(Vec3 const& cameraPos) const
{
    IntVec2 cameraChunkCoords = Chunk::GetChunkCoords(IntVec3(static_cast<int>(cameraPos.x), static_cast<int>(cameraPos.y), static_cast<int>(cameraPos.z)));

    int maxRadius = (CHUNK_ACTIVATION_RANGE / 16) + 2; // CHUNK_SIZE_X is 16

    // Phase 0, Task 0.7: Get player velocity for directional preloading
    Vec3  playerVelocity        = GetPlayerVelocity();
    float velocityMagnitude     = playerVelocity.GetLength();
    bool  useDirectionalPreload = velocityMagnitude > PRELOAD_VELOCITY_THRESHOLD;

    // Calculate movement direction (normalized)
    Vec3 movementDirection = Vec3::ZERO;
    if (useDirectionalPreload)
    {
        movementDirection = playerVelocity.GetNormalized();
    }

    // OPTIMIZATION: Build lookup sets ONCE with single mutex lock per set
    // This reduces mutex locks from ~1,936 per call to just 2 total
    std::unordered_set<IntVec2> activeSet;
    std::unordered_set<IntVec2> queuedSet;

    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& pair : m_activeChunks)
        {
            activeSet.insert(pair.first);
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        queuedSet = m_queuedGenerateChunks; // Copy the set
    }

    // Phase 0, Task 0.7: Smart directional preloading
    // If player is moving, prioritize chunks ahead of movement direction
    if (useDirectionalPreload)
    {
        // Calculate lookahead position (3 chunks ahead in movement direction)
        float   lookaheadDistance = PRELOAD_LOOKAHEAD_CHUNKS * CHUNK_SIZE_X;
        Vec3    lookaheadPos      = cameraPos + (movementDirection * lookaheadDistance);
        IntVec2 lookaheadChunk    = Chunk::GetChunkCoords(IntVec3(static_cast<int>(lookaheadPos.x), static_cast<int>(lookaheadPos.y), 0));

        // Search in expanding rings around lookahead position
        for (int radius = 0; radius <= 2; radius++) // Only check nearby chunks around lookahead
        {
            for (int dx = -radius; dx <= radius; dx++)
            {
                for (int dy = -radius; dy <= radius; dy++)
                {
                    // Only check perimeter of current ring
                    if (abs(dx) != radius && abs(dy) != radius)
                    {
                        continue;
                    }

                    IntVec2 testChunk(lookaheadChunk.x + dx, lookaheadChunk.y + dy);

                    // Phase 0, Task 0.6: Fixed world bounds DISABLED for infinite world generation
                    // Previously limited to 16x16 chunks (256 total) for testing
                    // Now supports infinite terrain generation
                    /*
                    if (testChunk.x < WORLD_MIN_CHUNK_X || testChunk.x > WORLD_MAX_CHUNK_X ||
                        testChunk.y < WORLD_MIN_CHUNK_Y || testChunk.y > WORLD_MAX_CHUNK_Y)
                    {
                        continue;
                    }
                    */

                    // Fast O(1) lookups without mutex locks
                    if (activeSet.find(testChunk) != activeSet.end())
                    {
                        continue;
                    }
                    if (queuedSet.find(testChunk) != queuedSet.end())
                    {
                        continue;
                    }

                    // Check if within activation range
                    float distance = GetDistanceToChunkCenter(testChunk, cameraPos);
                    if (distance <= CHUNK_ACTIVATION_RANGE)
                    {
                        return testChunk; // Found chunk in movement direction! Prioritize this
                    }
                }
            }
        }
    }

    // Standard spiral search from camera position (fallback or when not moving)
    // OPTIMIZATION: Spiral search outward from camera (nearest-first)
    // Returns immediately when first valid chunk found, avoiding distance sorting
    for (int radius = 0; radius <= maxRadius; radius++)
    {
        // Check ring at 'radius' distance from camera
        for (int dx = -radius; dx <= radius; dx++)
        {
            for (int dy = -radius; dy <= radius; dy++)
            {
                // Only check perimeter of current ring (not interior)
                if (abs(dx) != radius && abs(dy) != radius)
                {
                    continue;
                }

                IntVec2 testChunk(cameraChunkCoords.x + dx, cameraChunkCoords.y + dy);

                // Phase 0, Task 0.6: Fixed world bounds DISABLED for infinite world generation
                // Previously limited to 16x16 chunks (256 total) for testing
                // Now supports infinite terrain generation
                /*
                if (testChunk.x < WORLD_MIN_CHUNK_X || testChunk.x > WORLD_MAX_CHUNK_X ||
                    testChunk.y < WORLD_MIN_CHUNK_Y || testChunk.y > WORLD_MAX_CHUNK_Y)
                {
                    continue;
                }
                */

                // Fast O(1) lookups without mutex locks
                if (activeSet.find(testChunk) != activeSet.end())
                {
                    continue;
                }
                if (queuedSet.find(testChunk) != queuedSet.end())
                {
                    continue;
                }

                // Check if within activation range
                float distance = GetDistanceToChunkCenter(testChunk, cameraPos);
                if (distance <= CHUNK_ACTIVATION_RANGE)
                {
                    return testChunk; // Found nearest! Return immediately
                }
            }
        }
    }

    return IntVec2(INT_MAX, INT_MAX); // No valid chunk found in range
}

//----------------------------------------------------------------------------------------------------
IntVec2 World::FindFarthestActiveChunkOutsideDeactivationRange(Vec3 const& cameraPos) const
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    float                       farthestDistance = 0.0f;
    IntVec2                     farthestChunk    = IntVec2(INT_MAX, INT_MAX);

    for (auto const& chunkPair : m_activeChunks)
    {
        float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);

        // Only consider chunks outside deactivation range
        if (distance > CHUNK_DEACTIVATION_RANGE && distance > farthestDistance)
        {
            farthestDistance = distance;
            farthestChunk    = chunkPair.first;
        }
    }

    return farthestChunk;
}

//----------------------------------------------------------------------------------------------------
Chunk* World::FindNearestDirtyChunk(Vec3 const& cameraPos) const
{
    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
    float                       nearestDistance   = FLT_MAX;
    Chunk*                      nearestDirtyChunk = nullptr;

    for (const auto& chunkPair : m_activeChunks)
    {
        Chunk* chunk = chunkPair.second;
        if (chunk != nullptr && chunk->GetIsMeshDirty())
        {
            float distance = GetDistanceToChunkCenter(chunkPair.first, cameraPos);
            if (distance < nearestDistance)
            {
                nearestDistance   = distance;
                nearestDirtyChunk = chunk;
            }
        }
    }

    return nearestDirtyChunk;
}

//----------------------------------------------------------------------------------------------------
bool World::ChunkExistsOnDisk(IntVec2 const& chunkCoords) const
{
    std::string filename = StringFormat("Saves/Chunk({0},{1}).chunk", chunkCoords.x, chunkCoords.y);
    return std::filesystem::exists(filename);
}

//----------------------------------------------------------------------------------------------------
bool World::LoadChunkFromDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;

    IntVec2     chunkCoords = chunk->GetChunkCoords();
    std::string filename    = StringFormat("Saves/Chunk({0},{1}).chunk", chunkCoords.x, chunkCoords.y);

    std::vector<uint8_t> buffer;
    if (!FileReadToBuffer(buffer, filename))
    {
        return false;
    }

    // Verify minimum file size (header + at least one RLE entry)
    if (buffer.size() < sizeof(ChunkFileHeader) + sizeof(ChunkRLEEntry))
    {
        return false; // File too small
    }

    // Read and validate header
    ChunkFileHeader header;
    memcpy(&header, buffer.data(), sizeof(ChunkFileHeader));

    // Validate header
    if (header.fourCC[0] != 'G' || header.fourCC[1] != 'C' ||
        header.fourCC[2] != 'H' || header.fourCC[3] != 'K')
    {
        return false; // Invalid 4CC
    }

    if (header.version != 1 || header.chunkBitsX != CHUNK_BITS_X ||
        header.chunkBitsY != CHUNK_BITS_Y || header.chunkBitsZ != CHUNK_BITS_Z)
    {
        return false; // Incompatible format
    }

    // Decompress RLE data
    size_t dataOffset = sizeof(ChunkFileHeader);
    int    blockIndex = 0;

    while (dataOffset < buffer.size() && blockIndex < BLOCKS_PER_CHUNK)
    {
        // Read RLE entry
        if (dataOffset + sizeof(ChunkRLEEntry) > buffer.size())
        {
            return false; // Incomplete RLE entry
        }

        ChunkRLEEntry entry;
        memcpy(&entry, buffer.data() + dataOffset, sizeof(ChunkRLEEntry));
        dataOffset += sizeof(ChunkRLEEntry);

        // Apply run to blocks
        for (int i = 0; i < entry.count && blockIndex < BLOCKS_PER_CHUNK; i++)
        {
            IntVec3 localCoords = Chunk::IndexToLocalCoords(blockIndex);
            Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
            if (block != nullptr)
            {
                block->m_typeIndex = entry.value;
            }
            blockIndex++;
        }
    }

    // Verify we loaded exactly the right number of blocks
    if (blockIndex != BLOCKS_PER_CHUNK)
    {
        return false; // RLE data doesn't match expected block count
    }

    return true;
}

//----------------------------------------------------------------------------------------------------
bool World::SaveChunkToDisk(Chunk* chunk) const
{
    if (chunk == nullptr) return false;

    IntVec2 chunkCoords = chunk->GetChunkCoords();

    // Debug output to help diagnose save issues
    DebuggerPrintf("Saving chunk (%d,%d) to disk...\n", chunkCoords.x, chunkCoords.y);

    // Ensure save directory exists (relative to executable in Run/ directory)
    std::string saveDir = "Saves/";
    std::filesystem::create_directories(saveDir);

    std::string filename = StringFormat("{0}Chunk({1},{2}).chunk", saveDir, chunkCoords.x, chunkCoords.y);

    // Collect block data in order for RLE compression
    std::vector<uint8_t> blockData(BLOCKS_PER_CHUNK);
    for (int i = 0; i < BLOCKS_PER_CHUNK; i++)
    {
        IntVec3 localCoords = Chunk::IndexToLocalCoords(i);
        Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        if (block != nullptr)
        {
            blockData[i] = block->m_typeIndex;
        }
        else
        {
            blockData[i] = 0; // Air block if invalid
        }
    }

    // Compress using RLE
    std::vector<ChunkRLEEntry> rleEntries;
    uint8_t                    currentType = blockData[0];
    uint8_t                    runLength   = 1;

    for (int i = 1; i < BLOCKS_PER_CHUNK; i++)
    {
        if (blockData[i] == currentType && runLength < 255)
        {
            runLength++;
        }
        else
        {
            // End current run
            rleEntries.push_back({currentType, runLength});
            currentType = blockData[i];
            runLength   = 1;
        }
    }
    // Don't forget the last run
    rleEntries.push_back({currentType, runLength});

    // Create file header
    ChunkFileHeader header;
    header.fourCC[0]  = 'G';
    header.fourCC[1]  = 'C';
    header.fourCC[2]  = 'H';
    header.fourCC[3]  = 'K';
    header.version    = 1;
    header.chunkBitsX = CHUNK_BITS_X;
    header.chunkBitsY = CHUNK_BITS_Y;
    header.chunkBitsZ = CHUNK_BITS_Z;

    // Calculate total file size
    size_t               fileSize = sizeof(ChunkFileHeader) + rleEntries.size() * sizeof(ChunkRLEEntry);
    std::vector<uint8_t> fileBuffer(fileSize);

    // Write header
    memcpy(fileBuffer.data(), &header, sizeof(ChunkFileHeader));

    // Write RLE entries
    size_t offset = sizeof(ChunkFileHeader);
    for (const ChunkRLEEntry& entry : rleEntries)
    {
        memcpy(fileBuffer.data() + offset, &entry, sizeof(ChunkRLEEntry));
        offset += sizeof(ChunkRLEEntry);
    }

    // Write to file using safe fopen_s
    FILE*   file = nullptr;
    errno_t err  = fopen_s(&file, filename.c_str(), "wb");
    if (err != 0 || file == nullptr) return false;

    size_t written = fwrite(fileBuffer.data(), 1, fileBuffer.size(), file);
    fclose(file);

    return written == fileBuffer.size();
}

//----------------------------------------------------------------------------------------------------
void World::UpdateNeighborPointers(IntVec2 const& chunkCoords)
{
    Chunk* centerChunk = GetChunk(chunkCoords);
    if (centerChunk == nullptr) return;

    // Get neighbor coordinates
    IntVec2 northCoords = chunkCoords + IntVec2(0, 1);
    IntVec2 southCoords = chunkCoords + IntVec2(0, -1);
    IntVec2 eastCoords  = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords  = chunkCoords + IntVec2(-1, 0);

    // Get neighbor chunks (if they exist)
    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk  = GetChunk(eastCoords);
    Chunk* westChunk  = GetChunk(westCoords);

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
    IntVec2 eastCoords  = chunkCoords + IntVec2(1, 0);
    IntVec2 westCoords  = chunkCoords + IntVec2(-1, 0);

    Chunk* northChunk = GetChunk(northCoords);
    Chunk* southChunk = GetChunk(southCoords);
    Chunk* eastChunk  = GetChunk(eastCoords);
    Chunk* westChunk  = GetChunk(westCoords);

    if (northChunk != nullptr) northChunk->SetNeighborChunks(northChunk->GetNorthNeighbor(), nullptr, northChunk->GetEastNeighbor(), northChunk->GetWestNeighbor());
    if (southChunk != nullptr) southChunk->SetNeighborChunks(nullptr, southChunk->GetSouthNeighbor(), southChunk->GetEastNeighbor(), southChunk->GetWestNeighbor());
    if (eastChunk != nullptr) eastChunk->SetNeighborChunks(eastChunk->GetNorthNeighbor(), eastChunk->GetSouthNeighbor(), eastChunk->GetEastNeighbor(), nullptr);
    if (westChunk != nullptr) westChunk->SetNeighborChunks(westChunk->GetNorthNeighbor(), westChunk->GetSouthNeighbor(), nullptr, westChunk->GetWestNeighbor());
}

//----------------------------------------------------------------------------------------------------
// Digging and Placing System
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
IntVec3 World::FindHighestNonAirBlockAtOrBelow(Vec3 const& position) const
{
    // Start from camera position and search downward
    IntVec3 searchPos = IntVec3(static_cast<int>(floorf(position.x)),
                                static_cast<int>(floorf(position.y)),
                                static_cast<int>(floorf(position.z)));

    // Search downward from camera Z position to find highest non-air block
    for (int z = searchPos.z; z >= 0; z--)
    {
        IntVec3 testPos(searchPos.x, searchPos.y, z);
        uint8_t blockType = GetBlockTypeAtGlobalCoords(testPos);

        // If we found a non-air block, this is our target
        if (blockType != 0) // 0 = BLOCK_AIR
        {
            return testPos;
        }
    }

    // No non-air block found (all air down to bedrock)
    return IntVec3(INT_MAX, INT_MAX, INT_MAX); // Invalid position
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 10: Fast Voxel Raycast using Amanatides & Woo algorithm
//----------------------------------------------------------------------------------------------------
RaycastResult World::RaycastVoxel(Vec3 const& start, Vec3 const& direction, float maxDistance) const
{
    // Validate input: direction must be normalized
    Vec3 normalizedDir = direction.GetNormalized();

    // Convert world position to global block coordinates
    IntVec3 currentBlock = IntVec3(static_cast<int>(floorf(start.x)),
                                   static_cast<int>(floorf(start.y)),
                                   static_cast<int>(floorf(start.z)));

    // Get chunk and check if starting position is valid
    IntVec2 chunkCoords = Chunk::GetChunkCoords(IntVec3(currentBlock.x, currentBlock.y, currentBlock.z));
    Chunk*  chunk       = GetChunk(chunkCoords);

    // If starting position is outside loaded chunks, return no hit
    if (chunk == nullptr)
    {
        return RaycastResult(false);
    }

    // Check if we're starting inside a solid block (shouldn't happen in gameplay, but handle it)
    IntVec3 localCoords = Chunk::GlobalCoordsToLocalCoords(currentBlock);
    Block*  block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
    sBlockDefinition* def = (block != nullptr) ? sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex) : nullptr;
    if (def != nullptr && def->IsOpaque())
    {
        // Starting inside solid block - return immediate hit
        RaycastResult result(true, currentBlock, 0.0f);
        result.m_impactPosition = start;
        result.m_impactNormal   = -normalizedDir;
        return result;
    }

    // ================================================================================
    // Amanatides & Woo Algorithm - Fast Voxel Traversal
    // ================================================================================

    // Calculate step direction for each axis (-1, 0, or +1)
    int stepX = (normalizedDir.x < 0.0f) ? -1 : (normalizedDir.x > 0.0f) ? 1 : 0;
    int stepY = (normalizedDir.y < 0.0f) ? -1 : (normalizedDir.y > 0.0f) ? 1 : 0;
    int stepZ = (normalizedDir.z < 0.0f) ? -1 : (normalizedDir.z > 0.0f) ? 1 : 0;

    // Calculate tDelta: distance along ray to cross 1 voxel in each axis
    // Avoid division by zero for rays parallel to axis
    float tDeltaX = (stepX != 0) ? (1.0f / fabsf(normalizedDir.x)) : FLT_MAX;
    float tDeltaY = (stepY != 0) ? (1.0f / fabsf(normalizedDir.y)) : FLT_MAX;
    float tDeltaZ = (stepZ != 0) ? (1.0f / fabsf(normalizedDir.z)) : FLT_MAX;

    // Calculate tMax: distance along ray to next voxel boundary in each axis
    // This is the distance from start to the first grid boundary crossed
    float tMaxX, tMaxY, tMaxZ;

    // X axis
    if (stepX > 0)
        tMaxX = ((currentBlock.x + 1.0f) - start.x) / normalizedDir.x;
    else if (stepX < 0)
        tMaxX = (currentBlock.x - start.x) / normalizedDir.x;
    else
        tMaxX = FLT_MAX;

    // Y axis
    if (stepY > 0)
        tMaxY = ((currentBlock.y + 1.0f) - start.y) / normalizedDir.y;
    else if (stepY < 0)
        tMaxY = (currentBlock.y - start.y) / normalizedDir.y;
    else
        tMaxY = FLT_MAX;

    // Z axis
    if (stepZ > 0)
        tMaxZ = ((currentBlock.z + 1.0f) - start.z) / normalizedDir.z;
    else if (stepZ < 0)
        tMaxZ = (currentBlock.z - start.z) / normalizedDir.z;
    else
        tMaxZ = FLT_MAX;

    // ================================================================================
    // Main Traversal Loop
    // ================================================================================

    float distanceTraveled = 0.0f;
    Vec3  impactNormal     = Vec3(0.0f, 0.0f, 0.0f);

    while (distanceTraveled < maxDistance)
    {
        // Determine which axis boundary we cross first
        if (tMaxX < tMaxY && tMaxX < tMaxZ)
        {
            // Cross X boundary first
            if (tMaxX > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in X direction
            currentBlock.x += stepX;
            distanceTraveled = tMaxX;
            tMaxX += tDeltaX;
            impactNormal = Vec3(static_cast<float>(-stepX), 0.0f, 0.0f);
        }
        else if (tMaxY < tMaxZ)
        {
            // Cross Y boundary first
            if (tMaxY > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in Y direction
            currentBlock.y += stepY;
            distanceTraveled = tMaxY;
            tMaxY += tDeltaY;
            impactNormal = Vec3(0.0f, static_cast<float>(-stepY), 0.0f);
        }
        else
        {
            // Cross Z boundary first
            if (tMaxZ > maxDistance)
                break; // Exceeded max distance

            // Step to next voxel in Z direction
            currentBlock.z += stepZ;
            distanceTraveled = tMaxZ;
            tMaxZ += tDeltaZ;
            impactNormal = Vec3(0.0f, 0.0f, static_cast<float>(-stepZ));
        }

        // Check if we stepped outside world bounds
        if (currentBlock.z < 0 || currentBlock.z >= CHUNK_SIZE_Z)
        {
            return RaycastResult(false); // Hit world boundary
        }

        // Get the chunk containing the new block
        chunkCoords = Chunk::GetChunkCoords(IntVec3(currentBlock.x, currentBlock.y, currentBlock.z));
        chunk       = GetChunk(chunkCoords);

        // If chunk not loaded, treat as no hit
        if (chunk == nullptr)
        {
            return RaycastResult(false);
        }

        // Check if current block is solid
        localCoords = Chunk::GlobalCoordsToLocalCoords(currentBlock);
        block       = chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
        def         = (block != nullptr) ? sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex) : nullptr;

        if (def != nullptr && def->IsOpaque())
        {
            // Hit a solid block!
            RaycastResult result(true, currentBlock, distanceTraveled);
            result.m_impactPosition = start + (normalizedDir * distanceTraveled);
            result.m_impactNormal   = impactNormal;
            return result;
        }
    }

    // No hit within max distance
    return RaycastResult(false);
}

//----------------------------------------------------------------------------------------------------
bool World::DigBlockAtCameraPosition(Vec3 const& cameraPos)
{
    // Find highest non-air block at or below camera position
    IntVec3 targetBlock = FindHighestNonAirBlockAtOrBelow(cameraPos);

    // Check if we found a valid block to dig
    if (targetBlock.x == INT_MAX || targetBlock.y == INT_MAX || targetBlock.z == INT_MAX)
    {
        return false; // No block to dig
    }

    // Convert the block to air (dig it)
    bool success = SetBlockAtGlobalCoords(targetBlock, 0); // 0 = BLOCK_AIR

    if (success)
    {
        DebuggerPrintf("Dug block at (%d,%d,%d)\n", targetBlock.x, targetBlock.y, targetBlock.z);
    }

    return success;
}

//----------------------------------------------------------------------------------------------------
bool World::PlaceBlockAtCameraPosition(Vec3 const&   cameraPos,
                                       uint8_t const blockType)
{
    // Find highest non-air block at or below camera position
    IntVec3 highestBlock = FindHighestNonAirBlockAtOrBelow(cameraPos);

    // Check if we found a valid foundation block
    if (highestBlock.x == INT_MAX || highestBlock.y == INT_MAX || highestBlock.z == INT_MAX)
    {
        return false; // No foundation block found
    }

    // Place block directly above the highest non-air block
    IntVec3 placePos = IntVec3(highestBlock.x, highestBlock.y, highestBlock.z + 1);

    // Check if the target position is valid (within world bounds)
    if (placePos.z >= CHUNK_SIZE_Z)
    {
        return false; // Would place block above world height limit
    }

    // Check if target position is already occupied by a non-air block
    uint8_t existingBlock = GetBlockTypeAtGlobalCoords(placePos);
    if (existingBlock != 0) // 0 = BLOCK_AIR
    {
        return false; // Position already occupied
    }

    // Place the new block
    bool success = SetBlockAtGlobalCoords(placePos, blockType);

    if (success)
    {
        DebuggerPrintf("Placed block type %d at (%d,%d,%d)\n", blockType, placePos.x, placePos.y, placePos.z);
    }

    return success;
}

//----------------------------------------------------------------------------------------------------
// Asynchronous job processing methods
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
void World::ProcessCompletedJobs()
{
    if (g_jobSystem == nullptr) return;

    // Retrieve all completed jobs from JobSystem ONCE
    std::vector<Job*> completedJobs = g_jobSystem->RetrieveAllCompletedJobs();

    // Process each completed job
    for (Job* completedJob : completedJobs)
    {
        ChunkGenerateJob* job      = nullptr;
        int               jobIndex = -1;

        // Find the corresponding ChunkGenerateJob in our tracking list
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkGenerationJobs.size(); ++i)
            {
                if (m_chunkGenerationJobs[i] == completedJob)
                {
                    job      = m_chunkGenerationJobs[i];
                    jobIndex = i;
                    break;
                }
            }
        }

        // If not a generation job, check if it's a load job
        if (job == nullptr)
        {
            // BUGFIX: Store chunk pointer before releasing mutex to prevent deadlock
            // when calling SubmitChunkForGeneration() which also needs m_jobListsMutex
            Chunk*        chunkToGenerate = nullptr;
            ChunkLoadJob* loadJobToDelete = nullptr;

            {
                std::lock_guard<std::mutex> lock(m_jobListsMutex);
                for (int i = 0; i < (int)m_chunkLoadJobs.size(); ++i)
                {
                    if (m_chunkLoadJobs[i] == completedJob)
                    {
                        ChunkLoadJob* loadJob = m_chunkLoadJobs[i];
                        Chunk*        chunk   = loadJob->GetChunk();

                        if (chunk)
                        {
                            IntVec2 chunkCoords = chunk->GetChunkCoords();

                            if (loadJob->WasSuccessful() && chunk->GetState() == ChunkState::LOAD_COMPLETE)
                            {
                                chunk->SetState(ChunkState::COMPLETE);
                                chunk->SetDebugDraw(m_globalChunkDebugDraw); // Inherit global debug state

                                {
                                    std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                                    m_nonActiveChunks.erase(chunk);
                                }

                                {
                                    std::lock_guard<std::mutex> activeLock(m_activeChunksMutex);
                                    m_activeChunks[chunkCoords] = chunk;
                                }

                                UpdateNeighborPointers(chunkCoords);

                                // Assignment 5 Phase 6: Trigger cross-chunk lighting propagation
                                chunk->OnActivate(this);  // CRITICAL: Re-enabled for loaded chunks

                                // NOTE: Do NOT call SetIsMeshDirty(true) here!
                                // The deferred mesh rebuild system will mark this chunk dirty
                                // AFTER lighting stabilizes via ProcessDirtyChunkMeshes()
                            }
                            else
                            {
                                {
                                    std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                                    m_nonActiveChunks.erase(chunk);
                                }

                                chunk->SetState(ChunkState::ACTIVATING);
                                // Store chunk for generation AFTER releasing mutex
                                chunkToGenerate = chunk;
                            }
                        }

                        m_chunkLoadJobs.erase(m_chunkLoadJobs.begin() + i);
                        loadJobToDelete = loadJob;
                        job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                        break;
                    }
                }
            } // Release m_jobListsMutex here

            // BUGFIX: Call SubmitChunkForGeneration AFTER releasing mutex
            // to prevent deadlock (SubmitChunkForGeneration also locks m_jobListsMutex)
            if (chunkToGenerate != nullptr)
            {
                SubmitChunkForGeneration(chunkToGenerate);
            }

            // Clean up after mutex is released
            if (loadJobToDelete != nullptr)
            {
                delete loadJobToDelete;
            }
        }

        // If not a load job either, check if it's a mesh job
        if (job == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkMeshJobs.size(); ++i)
            {
                if (m_chunkMeshJobs[i] == completedJob)
                {
                    ChunkMeshJob* meshJob = m_chunkMeshJobs[i];
                    Chunk*        chunk   = meshJob->GetChunk();

                    if (meshJob->WasSuccessful())
                    {
                        // Apply mesh data on main thread (CPU data only)
                        meshJob->ApplyMeshDataToChunk();

                        // Now perform DirectX operations on main thread
                        chunk->UpdateVertexBuffer();
                        chunk->SetMeshClean();
                    }

                    m_chunkMeshJobs.erase(m_chunkMeshJobs.begin() + i);
                    delete meshJob;
                    job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                    break;
                }
            }
        }

        // If not a mesh job either, check if it's a save job
        if (job == nullptr)
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            for (int i = 0; i < (int)m_chunkSaveJobs.size(); ++i)
            {
                if (m_chunkSaveJobs[i] == completedJob)
                {
                    ChunkSaveJob* saveJob = m_chunkSaveJobs[i];
                    Chunk*        chunk   = saveJob->GetChunk();

                    if (chunk)
                    {
                        {
                            std::lock_guard<std::mutex> nonActiveLock(m_nonActiveChunksMutex);
                            m_nonActiveChunks.erase(chunk);
                        }

                        delete chunk;
                    }

                    m_chunkSaveJobs.erase(m_chunkSaveJobs.begin() + i);
                    delete saveJob;
                    job = reinterpret_cast<ChunkGenerateJob*>(1); // Mark as processed
                    break;
                }
            }
        }

        if (job == nullptr)
        {
            continue; // Not any of our job types
        }

        // If job is a real ChunkGenerateJob pointer (not marker), process it
        if (job != reinterpret_cast<ChunkGenerateJob*>(1))
        {
            Chunk* chunk = job->GetChunk();
            if (!chunk)
            {
                // Clean up invalid job
                {
                    std::lock_guard<std::mutex> lock(m_jobListsMutex);
                    m_chunkGenerationJobs.erase(m_chunkGenerationJobs.begin() + jobIndex);
                }
                delete job;
                continue;
            }

            IntVec2 chunkCoords = chunk->GetChunkCoords();

            // Verify the chunk is in the expected state
            if (chunk->GetState() == ChunkState::LIGHTING_INITIALIZING)
            {
                // Job completed successfully - handle lighting initialization on main thread
                // For now, just transition to COMPLETE state
                chunk->SetState(ChunkState::COMPLETE);
                chunk->SetDebugDraw(m_globalChunkDebugDraw); // Inherit global debug state

                // Remove from non-active chunks, add to active chunks
                {
                    std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                    m_nonActiveChunks.erase(chunk);
                }

                {
                    std::lock_guard<std::mutex> lock(m_activeChunksMutex);
                    if (m_activeChunks.find(chunkCoords) == m_activeChunks.end())
                    {
                        m_activeChunks[chunkCoords] = chunk;
                    }
                }

                // Update neighbor pointers (needs active chunks mutex, but UpdateNeighborPointers handles it)
                UpdateNeighborPointers(chunkCoords);

                // Remove from queued chunks
                {
                    std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
                    m_queuedGenerateChunks.erase(chunkCoords);
                }

                // Assignment 5 Phase 6: Trigger cross-chunk lighting propagation FIRST
                // This queues edge blocks for lighting propagation
                chunk->OnActivate(this);

                // NOTE: Do NOT call SetIsMeshDirty(true) here!
                // The deferred mesh rebuild system will mark this chunk dirty
                // AFTER lighting stabilizes via ProcessDirtyChunkMeshes()
            }
            else
            {
                // Job completed but chunk is in unexpected state - handle error
                // This could happen if the job failed or chunk was modified externally
                // For now, we'll clean up and let the chunk be retried later

                // Remove from non-active chunks
                {
                    std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
                    m_nonActiveChunks.erase(chunk);
                }

                // Remove from queued chunks
                {
                    std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
                    m_queuedGenerateChunks.erase(chunkCoords);
                }

                // Reset chunk to ACTIVATING state so it can be retried
                chunk->SetState(ChunkState::ACTIVATING);
            }

            // Clean up the job
            {
                std::lock_guard<std::mutex> lock(m_jobListsMutex);
                m_chunkGenerationJobs.erase(m_chunkGenerationJobs.begin() + jobIndex);
            }
            delete job;
        } // End if (job is real ChunkGenerateJob)
    }
}

//----------------------------------------------------------------------------------------------------
void World::ProcessDirtyChunkMeshes()
{
    // CRITICAL FIX: Don't rebuild meshes while light propagation is in progress
    // This prevents "progressive brightening" bug where chunks appear dark and gradually brighten
    // as light propagates from neighboring chunks
    if (!m_dirtyLightQueue.empty())
    {
        static int skipCount = 0;
        skipCount++;
        if (skipCount <= 10)  // Log first 10 skips
        {
            DebuggerPrintf("[MESH DEFER] Skipping mesh rebuild, dirty light queue size=%zu\n", m_dirtyLightQueue.size());
        }
        return;  // Don't rebuild any meshes until light queue is empty
    }

    // Assignment 5 Phase 10 FIX: Lighting queue is now empty (lighting has stabilized)
    // Mark all chunks with changed lighting as mesh-dirty so they rebuild with correct lighting
    // This fixes the "inconsistent nighttime lighting" bug where some chunks stay bright
    {
        std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
        if (!m_chunksNeedingMeshRebuild.empty())
        {
            // Log how many chunks are being marked for rebuild (first occurrence only)
            static bool firstMarkLogged = false;
            if (!firstMarkLogged)
            {
                DebuggerPrintf("[MESH REBUILD FIX] Lighting stabilized, marking %zu chunks as mesh-dirty\n",
                              m_chunksNeedingMeshRebuild.size());
                firstMarkLogged = true;
            }

            for (Chunk* chunk : m_chunksNeedingMeshRebuild)
            {
                if (chunk)
                {
                    chunk->SetIsMeshDirty(true);
                }
            }
            m_chunksNeedingMeshRebuild.clear();
        }
    }

    Vec3 cameraPos = GetCameraPosition();

    // Find up to 2 dirty chunks closest to camera
    std::vector<std::pair<float, Chunk*>> dirtyChunksWithDistance;

    {
        std::lock_guard<std::mutex> lock(m_activeChunksMutex);
        for (auto const& chunkPair : m_activeChunks)
        {
            Chunk* chunk = chunkPair.second;
            if (chunk && chunk->GetIsMeshDirty() && chunk->GetState() == ChunkState::COMPLETE)
            {
                float distance = GetDistanceToChunkCenter(chunk->GetChunkCoords(), cameraPos);
                dirtyChunksWithDistance.emplace_back(distance, chunk);
            }
        }
    }

    // Sort by distance (closest first)
    std::sort(dirtyChunksWithDistance.begin(), dirtyChunksWithDistance.end());

    // Rebuild mesh for up to 2 closest dirty chunks
    int meshesRebuilt = 0;
    for (auto const& pair : dirtyChunksWithDistance)
    {
        if (meshesRebuilt >= 2)
        {
            break;
        }

        Chunk* chunk = pair.second;

        // Submit mesh generation job instead of rebuilding on main thread
        SubmitChunkForMeshGeneration(chunk);
        ++meshesRebuilt;
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForGeneration(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    IntVec2 chunkCoords = chunk->GetChunkCoords();

    // Check if we've reached the maximum number of pending generation jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkGenerationJobs.size() >= MAX_PENDING_GENERATE_JOBS)
        {
            return; // Too many jobs in flight, try again next frame
        }
    }

    // Check if chunk is already queued for generation (with mutex protection)
    {
        std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
        if (m_queuedGenerateChunks.find(chunkCoords) != m_queuedGenerateChunks.end())
        {
            return; // Already queued
        }
    }

    // Set chunk state and submit job
    if (chunk->CompareAndSetState(ChunkState::ACTIVATING, ChunkState::TERRAIN_GENERATING))
    {
        ChunkGenerateJob* job = new ChunkGenerateJob(chunk);

        // Add chunk to non-active chunks (being processed by worker thread)
        {
            std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
            m_nonActiveChunks.insert(chunk);
        }

        // Add job to tracking lists
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            m_chunkGenerationJobs.push_back(job);
        }

        // Mark as queued
        {
            std::lock_guard<std::mutex> lock(m_queuedChunksMutex);
            m_queuedGenerateChunks.insert(chunkCoords);
        }

        g_jobSystem->SubmitJob(job);
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForLoading(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    // Check if we've reached the maximum number of pending load jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkLoadJobs.size() >= MAX_PENDING_LOAD_JOBS)
        {
            return; // Too many load jobs in flight, try again next frame
        }
    }

    // Set chunk state and submit I/O job
    if (chunk->CompareAndSetState(ChunkState::ACTIVATING, ChunkState::LOADING))
    {
        ChunkLoadJob* job = new ChunkLoadJob(chunk);

        // Add chunk to non-active chunks (being processed by I/O worker thread)
        {
            std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
            m_nonActiveChunks.insert(chunk);
        }

        // Add job to tracking lists
        {
            std::lock_guard<std::mutex> lock(m_jobListsMutex);
            m_chunkLoadJobs.push_back(job);
        }

        g_jobSystem->SubmitJob(job);
    }
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForSaving(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr)
    {
        // Can't save asynchronously, delete chunk
        delete chunk;
        return;
    }

    // Check if we've reached the maximum number of pending save jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkSaveJobs.size() >= MAX_PENDING_SAVE_JOBS)
        {
            // Too many save jobs in flight, save synchronously (fallback)
            chunk->SaveToDisk();
            delete chunk;
            return;
        }
    }

    // Set chunk state and submit I/O job
    chunk->SetState(ChunkState::SAVING);

    ChunkSaveJob* job = new ChunkSaveJob(chunk);

    // Add chunk to non-active chunks (being processed by I/O worker thread)
    {
        std::lock_guard<std::mutex> lock(m_nonActiveChunksMutex);
        m_nonActiveChunks.insert(chunk);
    }

    // Add job to tracking lists
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        m_chunkSaveJobs.push_back(job);
    }

    g_jobSystem->SubmitJob(job);
    chunk->SetIsMeshDirty(false);
}

//----------------------------------------------------------------------------------------------------
void World::SubmitChunkForMeshGeneration(Chunk* chunk)
{
    if (!chunk)
    {
        return;
    }

    if (g_jobSystem == nullptr) return;

    // Check if we've reached the maximum number of pending mesh jobs
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        if ((int)m_chunkMeshJobs.size() >= MAX_PENDING_MESH_JOBS)
        {
            return; // Too many jobs in flight, try again next frame
        }
    }

    // Create and submit mesh generation job
    ChunkMeshJob* job = new ChunkMeshJob(chunk);

    // Add job to tracking lists
    {
        std::lock_guard<std::mutex> lock(m_jobListsMutex);
        m_chunkMeshJobs.push_back(job);
    }

    // CRITICAL FIX: Mark chunk as clean IMMEDIATELY to prevent re-queuing while job is in flight
    // This fixes the oscillation bug where chunks alternate between bright/dark
    chunk->SetIsMeshDirty(false);

    g_jobSystem->SubmitJob(job);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 4: Add block to dirty light queue for recalculation
//----------------------------------------------------------------------------------------------------
void World::AddToDirtyLightQueue(BlockIterator const& blockIter)
{
    if (!blockIter.IsValid()) return;

    // Assignment 5 Phase 7: O(1) duplicate detection using std::unordered_set
    // Check if block is already in the set - prevents infinite propagation loops
    if (m_dirtyLightSet.find(blockIter) != m_dirtyLightSet.end())
    {
        return;  // Already queued, skip
    }

    // Add to tracking set for O(1) duplicate detection
    m_dirtyLightSet.insert(blockIter);

    // BUG HUNT: Log when ANY blocks are added to dirty light queue
    // Works in both Debug and Release builds for performance testing
    static int queueLogCount = 0;
    Block* block = blockIter.GetBlock();
    if (queueLogCount < 50 && block)
    {
        IntVec3 localCoords = blockIter.GetLocalCoords();
        IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        uint8_t outdoor = block->GetOutdoorLight();

        // Only log underground blocks (z < 100) to reduce spam
        if (localCoords.z < 100)
        {
            DebuggerPrintf("[QUEUE] Block type=%d added to dirty queue: Chunk(%d,%d) Pos(%d,%d,%d) outdoor=%d queueSize=%d setSize=%d\n",
                          block->m_typeIndex, chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                          outdoor, (int)m_dirtyLightQueue.size(), (int)m_dirtyLightSet.size());
            queueLogCount++;
        }
    }

    // Add to back of queue (FIFO order)
    m_dirtyLightQueue.push_back(blockIter);
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 4: Process dirty light queue with time budget
//----------------------------------------------------------------------------------------------------
void World::ProcessDirtyLighting(float const maxTimeSeconds)
{
    if (m_dirtyLightQueue.empty()) return;

    // Start timer
    double const startTime = Clock::GetSystemClock().GetTotalSeconds();

    // Process blocks until time budget exhausted or queue empty
    while (!m_dirtyLightQueue.empty())
    {
        // Check time budget
        double const currentTime = Clock::GetSystemClock().GetTotalSeconds();
        double const elapsedTime = currentTime - startTime;

        if (elapsedTime >= maxTimeSeconds)
        {
            break;  // Time budget exhausted
        }

        // Pop block from front of queue (FIFO order)
        BlockIterator blockIter = m_dirtyLightQueue.front();
        m_dirtyLightQueue.pop_front();

        // Assignment 5 Phase 7: Remove from tracking set (block is now being processed)
        m_dirtyLightSet.erase(blockIter);

        // Assignment 5 Phase 5: Recalculate lighting for this block
        RecalculateBlockLighting(blockIter);
    }

    // Assignment 5 Phase 7: Safety check - if queue is empty, set should also be empty
    if (m_dirtyLightQueue.empty() && !m_dirtyLightSet.empty())
    {
        DebuggerPrintf("[WARNING] Lighting queue empty but set has %d items - clearing set\n", (int)m_dirtyLightSet.size());
        m_dirtyLightSet.clear();
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 5: Influence map light propagation algorithm
//----------------------------------------------------------------------------------------------------
void World::RecalculateBlockLighting(BlockIterator const& blockIter)
{
    if (!blockIter.IsValid())
        return;

    Block* block = blockIter.GetBlock();
    if (!block)
        return;

    // Get block definition to check properties
    sBlockDefinition* blockDef = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
    if (!blockDef)
        return;

    // Store old values to detect changes
    uint8_t const oldOutdoor = block->GetOutdoorLight();
    uint8_t const oldIndoor  = block->GetIndoorLight();

    //----------------------------------------------------------------------------------------------------
    // Calculate new outdoor light (skylight propagation)
    //----------------------------------------------------------------------------------------------------
    uint8_t newOutdoor = 0;

    if (block->IsSkyVisible())
    {
        // Direct skylight - always full brightness
        newOutdoor = 15;

        // DIAGNOSTIC: Verify sky-visible blocks maintain outdoor=15
        static int skyVisibleCheckCount = 0;
        if (skyVisibleCheckCount < 10 && oldOutdoor != 15)
        {
            IntVec3 localCoords = blockIter.GetLocalCoords();
            IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
            DebuggerPrintf("[SKY-VIS BUG] Block type=%d Chunk(%d,%d) Pos(%d,%d,%d) isSkyVisible=TRUE but oldOutdoor=%d (fixing to 15)\n",
                          block->m_typeIndex, chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z, oldOutdoor);
            skyVisibleCheckCount++;
        }
    }
    else if (!blockDef->IsOpaque())
    {
        // A5 SPEC COMPLIANT: Only NON-OPAQUE blocks receive propagated outdoor light
        // Opaque blocks (including water/ice/leaves) ALWAYS get outdoor=0
        // This matches reference implementation World.cpp lines 798-803
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    // A5 SPEC: Take light FROM neighbor if neighbor is NOT opaque OR neighbor is emissive
                    // This allows emissive blocks (glowstone) to provide light even though they're opaque
                    // Reference implementation World.cpp line 762
                    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
                    bool canProvideLight = neighborDef && (!neighborDef->IsOpaque() || neighborDef->IsEmissive());

                    if (canProvideLight)
                    {
                        uint8_t neighborLight = neighborBlock->GetOutdoorLight();
                        if (neighborLight > 0)
                        {
                            // Influence map: Take max of (neighbor - 1)
                            uint8_t propagatedLight = neighborLight - 1;
                            if (propagatedLight > newOutdoor)
                                newOutdoor = propagatedLight;
                        }
                    }
                }
            }
        }
    }
    // else: Opaque blocks (water/ice/leaves/stone/etc) ALWAYS get outdoor=0

    //----------------------------------------------------------------------------------------------------
    // Calculate new indoor light (emissive + propagation)
    //----------------------------------------------------------------------------------------------------
    // CRITICAL FIX: Start at 0, not existing value!
    // InitializeLighting() sets the INITIAL values correctly
    // But propagation must recalculate from scratch based on neighbors
    // Preserving old values prevents correct propagation
    uint8_t newIndoor = 0;

    if (blockDef->IsEmissive())
    {
        // Emissive blocks are light sources
        newIndoor = blockDef->GetEmissiveValue();
    }
    else if (!blockDef->IsOpaque())
    {
        // Transparent blocks: Find max neighbor indoor light - 1 (influence map)
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        // BUG HUNT: Track which neighbor is providing indoor light
        static int indoorPropagationLogCount = 0;
        Block* block = blockIter.GetBlock();
        IntVec3 localCoords = blockIter.GetLocalCoords();
        IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    uint8_t neighborLight = neighborBlock->GetIndoorLight();
                    if (neighborLight > 0)
                    {
                        // BUG HUNT: Log FIRST 20 cases of indoor light propagation from neighbors
                        if (indoorPropagationLogCount < 20)
                        {
                            IntVec3 neighborCoords = neighborIter.GetLocalCoords();
                            IntVec2 neighborChunkCoords = neighborIter.GetChunk() ? neighborIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
                            sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
                            bool neighborIsEmissive = neighborDef ? neighborDef->IsEmissive() : false;

                            DebuggerPrintf("[INDOOR PROPAGATION #%d] Block Chunk(%d,%d) Pos(%d,%d,%d) type=%d receiving indoor=%d from neighbor Chunk(%d,%d) Pos(%d,%d,%d) type=%d isEmissive=%d\n",
                                          indoorPropagationLogCount, chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z, block->m_typeIndex,
                                          neighborLight - 1, neighborChunkCoords.x, neighborChunkCoords.y, neighborCoords.x, neighborCoords.y, neighborCoords.z,
                                          neighborBlock->m_typeIndex, neighborIsEmissive ? 1 : 0);
                            indoorPropagationLogCount++;
                        }

                        // Influence map: Take max of (neighbor - 1)
                        uint8_t propagatedLight = neighborLight - 1;
                        if (propagatedLight > newIndoor)
                            newIndoor = propagatedLight;
                    }
                }
            }
        }
    }
    // else: Opaque blocks stay at indoor light = 0

    //----------------------------------------------------------------------------------------------------
    // Update block lighting values
    //----------------------------------------------------------------------------------------------------
    // BUG HUNT: Log when ANY blocks get BRIGHTENED unexpectedly
    // CRITICAL: Log ALL brightening events to catch the propagation cascade
    static int brightenLogCount = 0;
    if (brightenLogCount < 100 && newOutdoor > oldOutdoor)
    {
        IntVec3 localCoords = blockIter.GetLocalCoords();
        IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        bool isSkyVisible = block->IsSkyVisible();
        bool isOpaque = blockDef ? blockDef->IsOpaque() : false;

        // Log ALL brightening events, not just underground
        DebuggerPrintf("[BRIGHTEN] Block type=%d Chunk(%d,%d) Pos(%d,%d,%d) outdoor %d->%d, isSkyVisible=%d, isOpaque=%d\n",
                      block->m_typeIndex, chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                      oldOutdoor, newOutdoor, isSkyVisible ? 1 : 0, isOpaque ? 1 : 0);
        brightenLogCount++;
    }

    // BUG HUNT: Log when outdoor light is DIMMED (15 -> lower) - this should NOT happen for sky-visible blocks
    static int dimLogCount = 0;
    if (dimLogCount < 10 && oldOutdoor > newOutdoor && oldOutdoor == 15)
    {
        IntVec3 localCoords = blockIter.GetLocalCoords();
        IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        bool isSkyVisible = block->IsSkyVisible();
        bool isOpaque = blockDef ? blockDef->IsOpaque() : false;
        DebuggerPrintf("[DIM BUG] Block type=%d Chunk(%d,%d) Pos(%d,%d,%d) outdoor 15->%d, isSkyVisible=%d, isOpaque=%d\n",
                      block->m_typeIndex, chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                      newOutdoor, isSkyVisible ? 1 : 0, isOpaque ? 1 : 0);
        dimLogCount++;
    }

    block->SetOutdoorLight(newOutdoor);
    block->SetIndoorLight(newIndoor);

    // BUG HUNT: Log when indoor light is SET to non-zero value
    static int indoorSetLogCount = 0;
    if (indoorSetLogCount < 50 && newIndoor > 0 && oldIndoor == 0)
    {
        IntVec3 localCoords = blockIter.GetLocalCoords();
        IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        sBlockDefinition* def = sBlockDefinition::GetDefinitionByIndex(block->m_typeIndex);
        bool isEmissive = def ? def->IsEmissive() : false;

        DebuggerPrintf("[INDOOR SET] Chunk(%d,%d) Block(%d,%d,%d) type=%d indoor 0->%d, isEmissive=%d, outdoor=%d\n",
                      chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                      block->m_typeIndex, newIndoor, isEmissive ? 1 : 0, newOutdoor);
        indoorSetLogCount++;
    }

    //----------------------------------------------------------------------------------------------------
    // If lighting changed, propagate to neighbors by adding them to dirty queue
    //----------------------------------------------------------------------------------------------------
    if (newOutdoor != oldOutdoor || newIndoor != oldIndoor)
    {
        // TEMPORARY DEBUG: Log lighting changes to verify propagation algorithm
        // Phase 6 VALIDATION: ✅ PASSED - Commented out after successful validation
        // IntVec3 localCoords = blockIter.GetLocalCoords();
        // IntVec2 chunkCoords = blockIter.GetChunk() ? blockIter.GetChunk()->GetChunkCoords() : IntVec2(0, 0);
        // DebuggerPrintf("LIGHT: Chunk(%d,%d) Block(%d,%d,%d) outdoor %d->%d, indoor %d->%d\n",
        //                chunkCoords.x, chunkCoords.y,
        //                localCoords.x, localCoords.y, localCoords.z,
        //                oldOutdoor, newOutdoor, oldIndoor, newIndoor);

        // Assignment 5 Phase 10 FIX: Track chunks with changed lighting for DEFERRED mesh rebuild
        // DON'T mark mesh dirty immediately - causes progressive brightening during light propagation
        // Instead, add chunk to tracking set. ProcessDirtyChunkMeshes() will mark them dirty
        // AFTER the lighting queue empties (lighting has stabilized).
        // This prevents mesh rebuild starvation while preserving "wait for stable lighting" behavior.
        Chunk* chunk = blockIter.GetChunk();
        if (chunk)
        {
            std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
            m_chunksNeedingMeshRebuild.insert(chunk);
        }

        // Add only NON-OPAQUE neighbors to dirty queue for propagation
        // Assignment 5 Phase 7: Mesh rendering reads neighbor light values for each face
        // Opaque blocks stay at light=0 but their faces use neighbor light during rendering
        // This prevents infinite propagation loops through solid blocks
        IntVec3 neighborOffsets[6] = {
            IntVec3(1, 0, 0),   // East
            IntVec3(-1, 0, 0),  // West
            IntVec3(0, 1, 0),   // North
            IntVec3(0, -1, 0),  // South
            IntVec3(0, 0, 1),   // Up
            IntVec3(0, 0, -1)   // Down
        };

        for (int i = 0; i < 6; i++)
        {
            BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
            if (neighborIter.IsValid())
            {
                // Only add neighbor if it's non-opaque (can receive light)
                Block* neighborBlock = neighborIter.GetBlock();
                if (neighborBlock)
                {
                    sBlockDefinition* neighborDef = sBlockDefinition::GetDefinitionByIndex(neighborBlock->m_typeIndex);
                    if (neighborDef && !neighborDef->IsOpaque())
                    {
                        AddToDirtyLightQueue(neighborIter);
                    }
                }
            }
        }
    }
}

//----------------------------------------------------------------------------------------------------
// Assignment 5 Phase 10: Mark chunk for DEFERRED mesh rebuild
// Called by OnActivate() to ensure chunks get mesh rebuild AFTER lighting stabilizes
// CRITICAL FIX: Don't mark mesh dirty immediately - wait for lighting to propagate first!
// The deferred system will mark chunks dirty only after lighting queue empties.
//----------------------------------------------------------------------------------------------------
void World::MarkChunkForMeshRebuild(Chunk* chunk)
{
    if (!chunk)
        return;

    // DEFERRED mesh rebuild - add to tracking set, DON'T mark dirty yet
    // ProcessDirtyChunkMeshes() will mark chunks dirty after lighting stabilizes
    // This ensures meshes are built with FINAL lighting values, not initial values
    std::lock_guard<std::mutex> lock(m_meshRebuildSetMutex);
    m_chunksNeedingMeshRebuild.insert(chunk);

    // DO NOT call chunk->SetIsMeshDirty(true) here!
    // Let ProcessDirtyChunkMeshes() handle it after lighting queue empties
}
