# Lighting Bug Debug Strategy V2

**Date:** 2025-11-16
**Status:** Investigating race condition / temporal brightening bug

---

## Bug Summary V2

**NEW SYMPTOM (from screenshot):**
- Some chunks START with correct dark lighting (underwater caves)
- Then chunks BECOME incorrectly bright (bright blue water areas)
- **Temporal bug**: Lighting changes from correct → incorrect over time
- **Spatial pattern**: Some chunks bright, adjacent chunks dark

**Evidence from screenshot:**
- Bright blue water areas → outdoor=15 (incorrect)
- Dark underwater caves → outdoor=0 (correct)
- **Hypothesis**: Race condition or propagation issue

---

## Root Cause Analysis V2

### Confirmed Facts

1. ✅ `InitializeLighting()` works correctly
   - Sets 150,000-180,000 blocks per chunk
   - Top-to-bottom scan, sets air blocks above terrain to outdoor=15

2. ✅ Save/load system is correct
   - Only saves m_typeIndex (good compression)
   - Lighting regenerated procedurally

3. ❌ **Issue:** Chunks become bright AFTER initial correct state

### Suspected Causes

#### Theory 1: OnActivate() Race Condition
**Problem:**
- `OnActivate()` called after chunk added to m_activeChunks
- Adds edge blocks to dirty light queue
- Propagation might be happening in wrong order
- Neighbor chunks might not be initialized yet

**Timeline:**
1. Worker thread: LoadFromDisk() + InitializeLighting() ✅
2. Main thread: SetState(COMPLETE)
3. Main thread: Add to m_activeChunks
4. Main thread: UpdateNeighborPointers() ← **Neighbors can see this chunk now!**
5. Main thread: OnActivate() ← **Adds blocks to dirty queue**
6. Later: ProcessDirtyLightQueue() ← **Might process in wrong order?**

#### Theory 2: InitializeLighting() Sets Wrong Blocks to Sky-Visible
**Problem:**
- Top-to-bottom scan sets ALL air blocks above first opaque to outdoor=15
- But what about air in CAVES that happen to be above the cave floor?
- Example: Underwater cave with air pocket above water level

**Code Analysis (Chunk.cpp lines 3324-3329):**
```cpp
if (!foundSolid)
{
    // Air blocks above terrain: Full outdoor light, sky visible
    block->SetOutdoorLight(15);
    block->SetIsSkyVisible(true);
    airBlocksSetToSky++;
}
```

**Issue:** This sets `isSkyVisible=true` for ANY non-opaque block above the first opaque block!
- Air in caves ABOVE the cave floor → marked as sky-visible ❌
- Water above ocean floor → might be marked as sky-visible ❌

#### Theory 3: RecalculateBlockLighting() Algorithm Issue
**Original algorithm:**
```cpp
uint8_t newOutdoor = 0;

if (block->IsSkyVisible())
    newOutdoor = 15;
else if (!IsOpaque())
    newOutdoor = max(neighbor outdoor - 1);
```

**Problem:**
- If `InitializeLighting()` wrongly sets `isSkyVisible=true` on cave air
- `RecalculateBlockLighting()` preserves that and sets outdoor=15 forever
- Propagates to neighbors → entire cave becomes bright

---

## Debugging Strategy

### Step 1: Add Targeted Debug Logging

**Location:** `Chunk.cpp InitializeLighting()`

**Add logging for blocks that get isSkyVisible=true:**
```cpp
if (!foundSolid)
{
    block->SetOutdoorLight(15);
    block->SetIsSkyVisible(true);

    #ifdef _DEBUG
    // Log blocks that are set to sky-visible
    // ONLY log blocks below z=100 (these might be cave air)
    if (z < 100 && callCount <= 3)
    {
        DebuggerPrintf("  [SKY VISIBLE] Block(%d,%d,%d) marked sky-visible, typeIndex=%d\\n",
                      x, y, z, block->m_typeIndex);
    }
    #endif

    airBlocksSetToSky++;
}
```

**Purpose:** See if caves/underwater areas are wrongly marked as sky-visible

### Step 2: Check Water Block Definitions

**Question:** Is BLOCK_WATER opaque or transparent?

**Check:** `Run/Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml`

**Expected:**
- Water should be `isOpaque="false"` (you can see through it)
- BUT it should NOT be sky-visible!

### Step 3: Verify Propagation Order

**Add logging in RecalculateBlockLighting():**
```cpp
#ifdef _DEBUG
// Log when underwater blocks get brightened
if (block->m_typeIndex == BLOCK_WATER && newOutdoor > 5)
{
    DebuggerPrintf("[WATER BRIGHT] Block outdoor=%d->%d, isSkyVisible=%d\\n",
                  oldOutdoor, newOutdoor, block->IsSkyVisible() ? 1 : 0);
}
#endif
```

---

## Expected Findings

### If Theory 1 is correct:
- Logs will show blocks being processed before neighbors are ready
- Timing-dependent behavior (some chunks bright, others not)

### If Theory 2 is correct:
- Logs will show cave air blocks marked as isSkyVisible=true
- Pattern: blocks below z=100 with isSkyVisible=true

### If Theory 3 is correct:
- Logs will show water blocks getting outdoor=15 from RecalculateBlockLighting()
- isSkyVisible might be true for water blocks

---

## Fix Strategy

### If Theory 1: Disable OnActivate() temporarily
```cpp
// TEMPORARY TEST: Comment out OnActivate() call
// chunk->OnActivate(this);
```

**Expected:** If bug disappears, confirms race condition

### If Theory 2: Fix InitializeLighting() logic
```cpp
// Instead of "first opaque block from top"
// Use "blocks that can see sky" (no opaque blocks ABOVE them in entire Z column)
bool hasOpaqueAbove = false;
for (int checkZ = CHUNK_SIZE_Z - 1; checkZ > z; checkZ--)
{
    Block* aboveBlock = GetBlock(x, y, checkZ);
    if (aboveBlock && GetDefinition(aboveBlock)->IsOpaque())
    {
        hasOpaqueAbove = true;
        break;
    }
}

if (!hasOpaqueAbove && !blockDef->IsOpaque())
{
    block->SetOutdoorLight(15);
    block->SetIsSkyVisible(true);
}
```

### If Theory 3: Fix isSkyVisible check
Ensure water blocks are NEVER marked as sky-visible unless at surface

---

## Next Steps

1. ~~Add debug logging to InitializeLighting()~~ ✅ DONE - No logs (water NOT set to outdoor=15)
2. ~~Add debug logging to RecalculateBlockLighting()~~ ✅ DONE - No logs initially
3. ~~Add debug logging to mesh building~~ ✅ DONE - No logs (mesh NOT reading outdoor > 10 from water)
4. **NEW: Add debug logging to track dirty light queue processing** ✅ DONE
   - Added [QUEUE] logging in AddToDirtyLightQueue() to track water blocks being queued
   - Added [BRIGHTEN BUG] logging in RecalculateBlockLighting() to track water blocks being brightened
5. Run game and check console for new log patterns
6. Report findings to identify root cause
7. Implement targeted fix based on findings

---

## Latest Investigation (2025-11-16)

### Discovery: ProcessDirtyLighting() Runs Every Frame

**Found in World.cpp line 163:**
```cpp
// Assignment 5 Phase 4: Process dirty light queue with 8ms budget
ProcessDirtyLighting(0.008f);
```

This means even with OnActivate() disabled, the dirty light queue is still processed every frame!

### Discovery: RecalculateBlockLighting() Creates Feedback Loop

**Found in World.cpp line 2074:**
```cpp
// Inside RecalculateBlockLighting()
if (neighborDef && !neighborDef->IsOpaque())
{
    AddToDirtyLightQueue(neighborIter);  // Adds neighbors back to queue!
}
```

**Implication:**
- If ANY block gets into the dirty queue (from anywhere)
- RecalculateBlockLighting() processes it
- RecalculateBlockLighting() adds its non-opaque neighbors to the queue
- Those neighbors get processed and add THEIR neighbors
- This creates a cascading propagation through all connected non-opaque blocks

**Question:** What initially adds blocks to the dirty queue if OnActivate() is disabled?

### New Debug Logging Added

**1. AddToDirtyLightQueue() (World.cpp:1861-1875):**
```cpp
#ifdef _DEBUG
static int waterQueueLogCount = 0;
Block* block = blockIter.GetBlock();
if (waterQueueLogCount < 20 && block && block->m_typeIndex == BLOCK_WATER)
{
    DebuggerPrintf("[QUEUE] WATER block added to dirty queue: Chunk(%d,%d) Pos(%d,%d,%d) outdoor=%d queueSize=%d\n",
                  chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                  outdoor, (int)m_dirtyLightQueue.size());
    waterQueueLogCount++;
}
#endif
```

**2. RecalculateBlockLighting() (World.cpp:2012-2025):**
```cpp
#ifdef _DEBUG
static int waterBrightenLogCount = 0;
if (waterBrightenLogCount < 50 && block->m_typeIndex == BLOCK_WATER && newOutdoor > oldOutdoor && newOutdoor > 5)
{
    DebuggerPrintf("[BRIGHTEN BUG] WATER block Chunk(%d,%d) Pos(%d,%d,%d) outdoor %d->%d, isSkyVisible=%d\n",
                  chunkCoords.x, chunkCoords.y, localCoords.x, localCoords.y, localCoords.z,
                  oldOutdoor, newOutdoor, isSkyVisible ? 1 : 0);
    waterBrightenLogCount++;
}
#endif
```

### Expected Patterns

**If dirty queue is empty at startup:**
- No [QUEUE] or [BRIGHTEN BUG] logs should appear
- Bug would NOT occur (chunks stay dark)
- This would indicate OnActivate() was the only source

**If dirty queue has blocks from other sources:**
- [QUEUE] logs will show water blocks being added
- [BRIGHTEN BUG] logs will show water blocks being brightened
- Pattern of outdoor values (0→15? 0→14→13? other?) will reveal the bug mechanism

**If water blocks have isSkyVisible=true:**
- [BRIGHTEN BUG] logs will show isSkyVisible=1
- This indicates InitializeLighting() set the flag but we missed it somehow
- Or something else is setting the flag after initialization
