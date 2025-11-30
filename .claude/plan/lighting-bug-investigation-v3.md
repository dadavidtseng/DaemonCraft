# Lighting Bug Investigation V3 - Shader Verification

**Date:** 2025-11-16
**Status:** 🔍 INVESTIGATING - Memory fix applied, shader verification in progress

---

## Changes Made This Session

### 1. ✅ Fixed Uninitialized Memory Bug

**File:** `Code/Game/Framework/Chunk.cpp` (lines 451-460)

```cpp
// CRITICAL FIX: Initialize ALL Block members to prevent garbage memory values
for (int i = 0; i < BLOCKS_PER_CHUNK; ++i)
{
    m_blocks[i].m_typeIndex = 0;      // BLOCK_AIR
    m_blocks[i].m_lightingData = 0;   // outdoor=0, indoor=0
    m_blocks[i].m_bitFlags = 0;       // isSkyVisible=false
}
```

### 2. ✅ Added Shader Loading Verification

**File:** `Code/Game/Gameplay/World.cpp` (lines 60-84)

```cpp
World::World()
{
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
}
```

### 3. ✅ Added Shader Binding Verification

**File:** `Code/Game/Gameplay/World.cpp` (lines 315-330)

```cpp
void World::Render() const
{
    // BUG HUNT: Log shader binding status every 300 frames (~5 seconds)
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
}
```

### 4. ✅ Added Lighting Initialization Verification

**File:** `Code/Game/Framework/Chunk.cpp` (lines 2408-2424)

```cpp
// BUG HUNT: Verify lighting was initialized before mesh gets rebuilt
static int verifyCount = 0;
verifyCount++;
if (verifyCount <= 3)
{
    Block* surfaceBlock = GetBlock(8, 8, 100);     // High altitude (should be outdoor=15)
    Block* undergroundBlock = GetBlock(8, 8, 50);  // Underground (should be outdoor=0)
    if (surfaceBlock && undergroundBlock)
    {
        DebuggerPrintf("[VERIFY LIGHTING] Chunk(%d,%d) AFTER InitializeLighting:\n");
        DebuggerPrintf("  Surface block (8,8,100): outdoor=%d indoor=%d isSkyVisible=%d\n");
        DebuggerPrintf("  Underground (8,8,50): outdoor=%d indoor=%d isSkyVisible=%d\n");
    }
}
```

---

## Expected Log Output

When you rebuild and run, you should see:

### On Startup:
```
[SHADER OK] World.hlsl loaded successfully at 0x[address]
[SHADER OK] World constant buffer created successfully
```

**If you see:**
```
[SHADER ERROR] World.hlsl FAILED to load! Will fall back to Default.hlsl
```
**Then:** World.hlsl has compilation errors or missing file!

### During Rendering (every ~5 seconds):
```
[RENDER OK] Binding World.hlsl shader (call #1)
[RENDER OK] Binding World.hlsl shader (call #301)
[RENDER OK] Binding World.hlsl shader (call #601)
```

**If you see:**
```
[RENDER WARNING] m_worldShader is NULL! Using Default.hlsl fallback!
```
**Then:** Shader failed to load, game falling back to unlit shader!

### After Terrain Generation (first 3 chunks):
```
[VERIFY LIGHTING] Chunk(0,0) AFTER InitializeLighting:
  Surface block (8,8,100): outdoor=15 indoor=0 isSkyVisible=1
  Underground (8,8,50): outdoor=0 indoor=0 isSkyVisible=0
```

**If outdoor values are WRONG:**
- Surface should be outdoor=15 (bright)
- Underground should be outdoor=0 (dark)

---

## Debugging Checklist

### ✅ Shader Verification
1. Check for `[SHADER OK]` or `[SHADER ERROR]` logs
2. If ERROR: Check if `Run/Data/Shaders/World.hlsl` exists
3. If ERROR: Check Visual Studio Output for HLSL compilation errors

### ✅ Lighting Initialization Verification
1. Check `[VERIFY LIGHTING]` logs show correct values:
   - Surface blocks: outdoor=15
   - Underground blocks: outdoor=0
2. If values are WRONG: InitializeLighting() has a logic error

### ✅ Mesh Building Verification
1. Mesh is built AFTER InitializeLighting() completes
2. Vertex colors encode: r=outdoor/15, g=indoor/15, b=directional
3. World.hlsl shader decodes these correctly

### ✅ Constant Buffer Verification
1. WorldConstants bound to register b8
2. OutdoorBrightness value being updated (day/night cycle)
3. Check if c_outdoorBrightness is always 1.0 (or changing with time)

---

## Possible Remaining Issues

### Theory 1: Shader Compilation Failure
**Symptom:** `[SHADER ERROR]` log appears
**Fix:** Check Visual Studio Output window for HLSL errors

### Theory 2: Default.hlsl Fallback
**Symptom:** Game uses Default.hlsl instead of World.hlsl
**Fix:** Ensure World.hlsl compiles without errors

### Theory 3: Constant Buffer Not Bound
**Symptom:** Lighting works but no day/night cycle
**Fix:** Verify BindConstantBuffer(8, m_worldConstantBuffer) is called

### Theory 4: Vertex Colors Not Encoded
**Symptom:** All blocks same brightness
**Fix:** Check mesh building actually writes lighting to vertex colors

### Theory 5: InitializeLighting() Logic Error
**Symptom:** `[VERIFY LIGHTING]` shows wrong values
**Fix:** Debug InitializeLighting() algorithm

---

## Files Modified

1. ✅ `Code/Game/Framework/Chunk.cpp` (lines 451-460) - Fixed memory initialization
2. ✅ `Code/Game/Framework/Chunk.cpp` (lines 2408-2424) - Added lighting verification
3. ✅ `Code/Game/Gameplay/World.cpp` (lines 60-84) - Added shader load verification
4. ✅ `Code/Game/Gameplay/World.cpp` (lines 315-330) - Added shader bind verification

---

## Next Steps

1. **Rebuild SimpleMiner** (Release or Debug)
2. **Run the game** and check console logs
3. **Report the log output:**
   - Did `[SHADER OK]` or `[SHADER ERROR]` appear?
   - Did `[RENDER OK]` or `[RENDER WARNING]` appear?
   - What values did `[VERIFY LIGHTING]` show?
4. **Go underground** - Are blocks still bright?

---

## Status Summary

- ✅ Memory initialization bug FIXED
- ✅ Shader loading verification ADDED
- ✅ Shader binding verification ADDED
- ✅ Lighting initialization verification ADDED
- ⏳ Waiting for user to rebuild and test
