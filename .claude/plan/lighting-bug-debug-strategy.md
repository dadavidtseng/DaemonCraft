# Lighting Bug Debug Strategy

**Date:** 2025-11-16
**Status:** Debug logging implemented, awaiting test results

---

## Bug Summary

**Symptom:** Air blocks show `isSkyVisible=0, outdoor=0, indoor=0, m_bitFlags=0x00` when they should show `isSkyVisible=1, outdoor=15` after InitializeLighting().

**Evidence:**
```
INIT LIGHTING DEBUG: Chunk(-4,-2) InitializeLighting() called (call #3)
  -> Set 171842 air blocks to outdoor=15, isSkyVisible=true
MESH BUILD DEBUG: Chunk(-4,-2) SurfaceBlock(21,0,81) surfaceZ=83 block.outdoor=0 block.indoor=0
  -> TopNeighbor(21,0,82) outdoor=0 indoor=0 isSkyVisible=0 m_bitFlags=0x00
```

---

## Root Cause Hypothesis

### Confirmed Issue #1: Save/Load System is Broken
- **SaveToDisk()** (Chunk.cpp:3021) - Only saves `m_typeIndex` (1 byte)
- **LoadFromDisk()** (Chunk.cpp:2989) - Only restores `m_typeIndex` (1 byte)
- **Impact:** Lighting bytes (`m_lightingData`, `m_bitFlags`) remain at default zero after load

### Unknown Issue #2: What's Clearing the Data?
Something is clearing lighting data AFTER InitializeLighting() sets it but BEFORE mesh building reads it.

**Suspects:**
1. **RecalculateBlockLighting()** - May be clearing values during propagation
2. **Memory corruption** - Race condition between threads
3. **Timing issue** - Values cleared during chunk state transitions

---

## Debug Logging Strategy (Non-Spammy)

### 🎯 Strategic Logging Points

All logging is **conditional** - only logs when values are CLEARED (non-zero → zero):

#### 1. Block.hpp - Track ALL Calls That Clear Lighting
```cpp
SetOutdoorLight(value):
  - Only log when: oldValue > 0 && value == 0
  - Output: "[BUG HUNT] SetOutdoorLight: CLEARED outdoor light 15->0 at Block@0xXXXX"

SetIsSkyVisible(visible):
  - Only log when: oldValue == true && visible == false
  - Output: "[BUG HUNT] SetIsSkyVisible: CLEARED flag true->false at Block@0xXXXX"
```

**Why this works:**
- Catches ANY code path that clears the lighting values
- Block pointer address allows correlation with other logs
- Only fires when bug actually happens (no spam)

#### 2. World.cpp - Track RecalculateBlockLighting Clearing Values
```cpp
RecalculateBlockLighting():
  - Only log when: oldOutdoor > 0 && newOutdoor == 0
  - Output: "[BUG HUNT] RecalculateBlockLighting: About to CLEAR outdoor 15->0
             at Chunk(-4,-2) Block(21,0,82) isSkyVisible=0 isOpaque=0"
```

**Why this works:**
- Identifies if RecalculateBlockLighting is the culprit
- Shows block coordinates and state at time of clearing
- Reveals if isSkyVisible flag is already cleared (clue!)

#### 3. Chunk.cpp - Track Chunk Loads
```cpp
LoadFromDisk():
  - Always log (but only once per chunk load)
  - Output: "[BUG HUNT] LoadFromDisk: Chunk(-4,-2) loaded from disk -
             lighting bytes remain at DEFAULT ZERO"
```

**Why this works:**
- Confirms which chunks were loaded from disk (vs generated)
- Establishes timeline: Load → InitLighting → ??? → MeshBuild

---

## Expected Debug Output Sequence

If RecalculateBlockLighting is the culprit:
```
[BUG HUNT] LoadFromDisk: Chunk(-4,-2) loaded from disk - lighting bytes remain at DEFAULT ZERO
INIT LIGHTING DEBUG: Chunk(-4,-2) InitializeLighting() called (call #3)
  -> Set 171842 air blocks to outdoor=15, isSkyVisible=true
[BUG HUNT] RecalculateBlockLighting: About to CLEAR outdoor 15->0 at Chunk(-4,-2) Block(21,0,82) isSkyVisible=0 isOpaque=0
[BUG HUNT] SetOutdoorLight: CLEARED outdoor light 15->0 at Block@0x12345678
MESH BUILD DEBUG: ... shows outdoor=0 indoor=0
```

**Key Insight:** If `isSkyVisible=0` in the RecalculateBlockLighting log, it means the flag was ALREADY cleared before recalculation!

---

## What The Logs Will Reveal

### Scenario A: RecalculateBlockLighting is the culprit
- We'll see `[BUG HUNT] RecalculateBlockLighting: About to CLEAR...`
- This confirms the propagation algorithm is broken
- Fix: Modify RecalculateBlockLighting to preserve sky-visible blocks

### Scenario B: Something else clears the flag first
- We'll see `[BUG HUNT] SetIsSkyVisible: CLEARED flag true->false...`
- Block address correlates to a different code path
- Stack trace reveals the actual culprit function

### Scenario C: Memory corruption
- Logs may show inconsistent block addresses
- Values cleared without any SetXXX() calls being logged
- Indicates race condition or buffer overflow

---

## Next Steps After Running

1. **Compile and run** in Debug mode
2. **Delete existing save files** to force fresh chunk loads
3. **Check console output** for `[BUG HUNT]` messages
4. **Report findings** - which scenario matched?
5. **Implement fix** based on identified culprit

---

## Files Modified

- `Code/Game/Framework/Block.hpp` - Added conditional logging to SetOutdoorLight(), SetIsSkyVisible()
- `Code/Game/Gameplay/World.cpp` - Added conditional logging to RecalculateBlockLighting()
- `Code/Game/Framework/Chunk.cpp` - Added load tracking to LoadFromDisk()
- `Code/Game/Framework/Chunk.cpp` - **NEW: Added loop execution tracking to InitializeLighting()**

**All logging is `#ifdef _DEBUG` only** - zero impact on Release builds.

---

## NEW DEBUG LOGGING: Loop Execution Tracking

**Problem**: InitializeLighting() logs entry but NOT the summary, suggesting loops aren't executing.

**Added Logging** (Chunk.cpp lines 3249-3307):

1. **Before loops start** (line 3249):
   ```cpp
   DebuggerPrintf("  [DEBUG] CHUNK_SIZE_X=%d CHUNK_SIZE_Y=%d CHUNK_SIZE_Z=%d\n", ...);
   DebuggerPrintf("  [DEBUG] About to enter outer loop (x loop)\n");
   ```

2. **On entering outer loop** (line 3265, x=0):
   ```cpp
   DebuggerPrintf("  [DEBUG] Entered outer loop (x=0), about to enter middle loop (y loop)\n");
   ```

3. **On entering middle loop** (line 3273, x=0, y=0):
   ```cpp
   DebuggerPrintf("  [DEBUG] Entered middle loop (x=0,y=0), about to enter inner loop (z loop, CHUNK_SIZE_Z-1=%d)\n", CHUNK_SIZE_Z - 1);
   ```

4. **On entering inner loop** (line 3284, x=0, y=0, z=255):
   ```cpp
   DebuggerPrintf("  [DEBUG] Entered inner loop (x=0,y=0,z=%d), processing first block\n", z);
   ```

5. **If GetBlock() returns nullptr** (line 3292):
   ```cpp
   DebuggerPrintf("  [DEBUG] ERROR: GetBlock(0,0,%d) returned nullptr!\n", z);
   ```

6. **If GetDefinitionByIndex() returns nullptr** (line 3305):
   ```cpp
   DebuggerPrintf("  [DEBUG] ERROR: GetDefinitionByIndex(%d) returned nullptr!\n", block->m_typeIndex);
   ```

**What This Will Reveal:**

- **Scenario A**: Loops don't execute at all
  - We'll see "About to enter outer loop" but NOT "Entered outer loop"
  - Indicates CHUNK_SIZE_X is zero or negative

- **Scenario B**: GetBlock() returns nullptr for all blocks
  - We'll see "ERROR: GetBlock(0,0,255) returned nullptr!" repeated
  - Indicates m_blocks array is not initialized

- **Scenario C**: GetDefinitionByIndex() returns nullptr for all blocks
  - We'll see "ERROR: GetDefinitionByIndex(X) returned nullptr!" repeated
  - Indicates block type indices are invalid or definition table is empty

- **Scenario D**: Loops execute but `continue` statements skip all blocks
  - We'll see all "Entered" messages but NO summary at end
  - Indicates all blocks are being skipped by nullptr checks

---

## CRITICAL FINDING (2025-11-16)

**Console Output Anomaly Detected:**
```
INIT LIGHTING DEBUG: Chunk(-3,-3) InitializeLighting() called (call #1)
INIT LIGHTING DEBUG: Chunk(-2,-1) InitializeLighting() called (call #2)
INIT LIGHTING DEBUG: Chunk(-3,-1) InitializeLighting() called (call #3)
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
```

**Problem:** CHUNK_SIZE logs appear SEPARATELY from the call headers, but they're in the SAME `if (callCount <= 3)` block (Chunk.cpp lines 3245-3251).

**Code Structure (lines 3245-3251):**
```cpp
if (callCount <= 3)  // Only log first 3 calls to avoid spam
{
    DebuggerPrintf("INIT LIGHTING DEBUG: Chunk(%d,%d) InitializeLighting() called (call #%d)\n",
                  m_chunkCoords.x, m_chunkCoords.y, callCount);
    DebuggerPrintf("  [DEBUG] CHUNK_SIZE_X=%d CHUNK_SIZE_Y=%d CHUNK_SIZE_Z=%d\n",
                  CHUNK_SIZE_X, CHUNK_SIZE_Y, CHUNK_SIZE_Z);
}
```

**Expected Output:**
```
INIT LIGHTING DEBUG: Chunk(-3,-3) InitializeLighting() called (call #1)
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
INIT LIGHTING DEBUG: Chunk(-2,-1) InitializeLighting() called (call #2)
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
INIT LIGHTING DEBUG: Chunk(-3,-1) InitializeLighting() called (call #3)
  [DEBUG] CHUNK_SIZE_X=32 CHUNK_SIZE_Y=32 CHUNK_SIZE_Z=256
```

**Actual Output:**
The two log lines appear in separate groups, suggesting they're NOT in the same if-block in the compiled code.

**Hypothesis: COMPILATION ISSUE**
- The executable is running OLD code from a previous build
- Source code changes (added debug logs) have NOT been compiled
- Need to verify: Clean build or check exe modification timestamp

**STATUS: RESOLVED** - User performed clean rebuild in Debug mode

---

## NEXT DEBUG ATTEMPT (2025-11-16 - After Clean Rebuild)

**New Aggressive Logging Added** (Chunk.cpp):

1. **Line 3256** - CRITICAL log BEFORE if-statement:
   ```cpp
   DebuggerPrintf("  [DEBUG CRITICAL] Reached line 3255, callCount=%d, about to check if callCount <= 3\n", callCount);
   ```

2. **Line 3265** - Added else branch to detect if callCount > 3:
   ```cpp
   else
   {
       DebuggerPrintf("  [DEBUG] Skipping outer loop log because callCount=%d > 3\n", callCount);
   }
   ```

3. **Line 3364** - CRITICAL log BEFORE function returns:
   ```cpp
   DebuggerPrintf("  [DEBUG CRITICAL] About to return from InitializeLighting(), callCount=%d, airBlocksSetToSky=%d\n", callCount, airBlocksSetToSky);
   ```

**What This Will Reveal:**

- **If line 3256 log appears**: Execution reaches past CHUNK_SIZE log
- **If line 3265 else branch triggers**: callCount is > 3 (static variable issue?)
- **If line 3364 log appears**: Function completes successfully
- **If NONE of these appear**: Function is crashing or being killed silently

---

## ROOT CAUSE IDENTIFIED (2025-11-16 - FINAL DIAGNOSIS)

**STATUS:** ✅ BUG FOUND - RecalculateBlockLighting() clears correct values!

### Timeline of Events

1. ✅ **LoadFromDisk()** - Restores only m_typeIndex (lighting bytes stay at zero)
2. ✅ **InitializeLighting()** - Sets outdoor=15, isSkyVisible=true for 150,000-180,000 air blocks
3. ❌ **OnActivate()** - Adds edge blocks to dirty light queue
4. ❌ **RecalculateBlockLighting()** - **CLEARS the correct values!**

### The Bug (World.cpp lines 1927-1960)

```cpp
else if (!blockDef->IsOpaque())
{
    // Transparent blocks: Find max neighbor outdoor light - 1 (influence map)
    uint8_t newOutdoor = 0;  // ← BUG: Always starts at ZERO!

    for (int i = 0; i < 6; i++)
    {
        BlockIterator neighborIter = blockIter.GetNeighbor(neighborOffsets[i]);
        if (neighborIter.IsValid())
        {
            Block* neighborBlock = neighborIter.GetBlock();
            if (neighborBlock)
            {
                uint8_t neighborLight = neighborBlock->GetOutdoorLight();
                if (neighborLight > 0)
                {
                    uint8_t propagatedLight = neighborLight - 1;
                    if (propagatedLight > newOutdoor)
                        newOutdoor = propagatedLight;
                }
            }
        }
    }
}
```

**Problem:** When an AIR block with outdoor=15 is added to dirty queue:
- Algorithm starts `newOutdoor = 0`
- Checks all 6 neighbors
- If ALL neighbors also have outdoor=0 (not processed yet)
- Result: `newOutdoor = 0` (clears the correct value!)
- Line 2023: `block->SetOutdoorLight(0)` - **OVERWRITES outdoor=15 → 0**

### The Fix (World.cpp line 1928)

**WRONG (current code):**
```cpp
uint8_t newOutdoor = 0;  // Always starts at zero - clears existing light!
```

**CORRECT (fix):**
```cpp
uint8_t newOutdoor = block->GetOutdoorLight();  // Start with existing value - preserve correct lighting!
```

**Why This Fixes It:**
- InitializeLighting() already set outdoor=15
- RecalculateBlockLighting() should PRESERVE this value
- Only UPDATE if neighbors have HIGHER light (influence map)
- Result: outdoor=15 is maintained until neighbors are processed

### Why Save/Load Is NOT Broken

The save/load system is **working as designed**:
- Only saves `m_typeIndex` (good RLE compression)
- Lighting is regenerated by `InitializeLighting()` (procedural, not persistent)
- This is the CORRECT architecture!

The bug is that RecalculateBlockLighting() **clears** the values that InitializeLighting() correctly set.

---

## FIX IMPLEMENTATION (2025-11-16)

**File:** `C:\p4\Personal\SD\SimpleMiner\Code\Game\Gameplay\World.cpp`

**Line 1920:** Change initialization to preserve existing outdoor light

**BEFORE:**
```cpp
uint8_t newOutdoor = 0;
```

**AFTER:**
```cpp
uint8_t newOutdoor = block->GetOutdoorLight();  // Start with existing value - preserve InitializeLighting() results
```

**Line 1966:** Change initialization to preserve existing indoor light

**BEFORE:**
```cpp
uint8_t newIndoor = 0;
```

**AFTER:**
```cpp
uint8_t newIndoor = block->GetIndoorLight();  // Start with existing value - preserve InitializeLighting() results
```

**Expected Result:**
- InitializeLighting() sets outdoor=15 for air blocks ✅
- RecalculateBlockLighting() preserves outdoor=15 (doesn't clear to 0) ✅
- Mesh building reads outdoor=15 correctly ✅
- Lighting system works!
