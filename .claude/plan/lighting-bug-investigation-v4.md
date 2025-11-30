# Lighting Bug Investigation V4 - Complete Analysis

**Date:** 2025-11-16
**Status:** 🔍 CRITICAL FINDING - Shader implementation does NOT match A5 specification

---

## Executive Summary

After thorough investigation, I've identified that the current World.hlsl shader implementation **deviates from Assignment 5 specification requirements**. While this explains some visual differences, it doesn't fully explain why some chunks are bright while others are dark at the same time.

---

## A5 Specification Requirements vs Current Implementation

### 1. ❌ Lighting Calculation Formula

**A5 Spec (lines 192-196):**
```hlsl
// DiminishingAdd(a, b) = 1 – (1 - a)(1 - b)
float3 diffuseLight = DiminishingAdd(
    input.color.r * OutdoorLightColor,  // Outdoor light contribution
    input.color.g * IndoorLightColor    // Indoor light contribution
);
finalColor = texColor * diffuseLight * input.color.b;  // directional shading
```

**Current Implementation (World.hlsl:106-110):**
```hlsl
float outdoorModulated = outdoor * c_outdoorBrightness;
float finalBrightness = max(outdoorModulated, indoor) * directional;  // WRONG!
litColor.rgb *= finalBrightness;
```

**Impact:** Using `max()` instead of `DiminishingAdd()` produces different visual results, especially when both outdoor and indoor light are present.

### 2. ❌ Missing Constant Buffer Data

**A5 Spec (lines 202-211):**
```hlsl
cbuffer WorldConstants : register(b4) {
    float4 CameraPosition;        // For fog distance calculation
    float4 IndoorLightColor;      // Default: (255, 230, 204)
    float4 OutdoorLightColor;     // Default: (255, 255, 255)
    float4 SkyColor;              // For fog blending
    float FogNearDistance;        // activation_range - 2*chunk_size
    float FogFarDistance;         // half of FogNearDistance
    float2 Padding;
};
```

**Current Implementation:**
```hlsl
cbuffer WorldConstants : register(b8) {
    float c_outdoorBrightness;    // Only a scalar!
    float c_gameTime;
    float2 padding;
};
```

**Impact:** Cannot implement proper day/night color variation or distance fog.

### 3. ❌ Missing Distance Fog

**A5 Spec (lines 197-200):**
```
- Compute world distance from pixel to camera
- Fog fraction is distance into fog [near, far] range-mapped and clamped to [0, 1]
- Blends final color toward the fog color based on (fog fraction * fog max alpha)
```

**Current Implementation:** No fog blending at all.

### 4. ❌ Missing Vertex World Position

**A5 Spec (line 191a):**
```
Vertex shader passes the world position of the vertex to the pixel shader.
```

**Current Implementation:** Only passes clip position, color, and UV coordinates.

---

## The Remaining Mystery: Why Some Chunks Bright, Others Dark?

Even with these specification mismatches, ALL chunks should have the same behavior - either all bright or all dark. The fact that some chunks are bright while others are dark in the same frame suggests a different issue:

### Theory 1: Constant Buffer Not Being Applied
If the constant buffer with `c_outdoorBrightness` is not being bound properly, the shader might use uninitialized memory (garbage values) which could be different per chunk.

**How to Verify:** Add debug logging in World.hlsl to output c_outdoorBrightness value as color.

### Theory 2: Some Chunks Using Different Shader
If some chunks are accidentally using Default.hlsl instead of World.hlsl, they would be full bright.

**How to Verify:** Check if g_renderer->BindShader() is being called between chunk renders.

### Theory 3: Vertex Color Data Corruption
If some chunks have corrupted vertex color data (all 255s), they would appear bright.

**How to Verify:** Add debug logging in mesh building to dump vertex color values.

### Theory 4: Mesh Built Before Lighting Initialization
If a chunk's mesh is built BEFORE InitializeLighting() runs, it would have garbage lighting data.

**How to Verify:** Check chunk state machine transitions and timing.

---

## Recommended Immediate Actions

### 1. Verify Shader is Bound
Add this at the START of every Chunk::Render():
```cpp
static Shader* lastBoundShader = nullptr;
Shader* currentShader = g_renderer->GetCurrentlyBoundShader();
if (currentShader != lastBoundShader)
{
    DebuggerPrintf("[SHADER CHANGE] Shader changed to 0x%p\n", currentShader);
    lastBoundShader = currentShader;
}
```

### 2. Verify Constant Buffer Values
Add this in World::Render() after BindConstantBuffer():
```cpp
DebuggerPrintf("CBO VERIFY: outdoorBrightness=%.3f bound to slot 8\n", worldConstants.outdoorBrightness);
```

### 3. Debug Shader Output
Temporarily modify World.hlsl to output c_outdoorBrightness as a color:
```hlsl
// DEBUG: Visualize outdoor brightness
return float4(c_outdoorBrightness, c_outdoorBrightness, c_outdoorBrightness, 1.0);
```
If all chunks are the same color, the constant buffer is working correctly.

### 4. Check Vertex Color Values
Add logging in AddBlockFacesWithHiddenSurfaceRemoval():
```cpp
static int vertexColorLog = 0;
if (vertexColorLog < 100)
{
    DebuggerPrintf("VERTEX: Block(%d,%d,%d) Face#%d r=%d g=%d b=%d\n",
                  x, y, z, faceIndex, vertexColor.r, vertexColor.g, vertexColor.b);
    vertexColorLog++;
}
```

---

## Long-Term Fix: Implement A5 Specification Correctly

### 1. Update WorldConstants Structure
```cpp
struct WorldConstants
{
    Vec4 cameraPosition;         // 16 bytes
    Vec4 indoorLightColor;       // 16 bytes
    Vec4 outdoorLightColor;      // 16 bytes
    Vec4 skyColor;               // 16 bytes
    float fogNearDistance;       // 4 bytes
    float fogFarDistance;        // 4 bytes
    float gameTime;              // 4 bytes
    float padding;               // 4 bytes (total 80 bytes, 16-byte aligned)
};
```

### 2. Update World.hlsl Shader
```hlsl
cbuffer WorldConstants : register(b8)
{
    float4 c_cameraPosition;
    float4 c_indoorLightColor;
    float4 c_outdoorLightColor;
    float4 c_skyColor;
    float c_fogNear;
    float c_fogFar;
    float c_gameTime;
    float c_padding;
};

// DiminishingAdd implementation
float3 DiminishingAdd(float3 a, float3 b)
{
    return 1.0 - (1.0 - a) * (1.0 - b);
}

float4 PixelMain(VertexOutPixelIn input) : SV_Target0
{
    float4 texColor = t_diffuseTexture.Sample(s_diffuseSampler, input.uv);

    // Compute light contributions
    float3 outdoorContrib = input.color.r * c_outdoorLightColor.rgb;
    float3 indoorContrib = input.color.g * c_indoorLightColor.rgb;

    // Combine using DiminishingAdd
    float3 diffuseLight = DiminishingAdd(outdoorContrib, indoorContrib);

    // Apply directional shading
    float3 finalColor = texColor.rgb * diffuseLight * input.color.b;

    // Apply distance fog (if vertex position is passed through)
    // ... fog blending code ...

    return float4(finalColor, texColor.a);
}
```

### 3. Update Vertex Shader to Pass World Position
```hlsl
struct VertexOutPixelIn
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 uv : TEXCOORD0;
    float3 worldPos : TEXCOORD1;  // NEW: World position for fog
};
```

---

## Files to Modify

1. **World.hpp** - Update WorldConstants struct
2. **World.cpp** - Update constant buffer population and day/night cycle
3. **World.hlsl** - Implement DiminishingAdd and fog blending
4. **Chunk.cpp (Vertex Shader)** - Pass world position to pixel shader

---

## Next Steps

1. **Immediate:** Add debug logging to identify why some chunks are bright while others are dark
2. **Short-term:** Fix the lighting calculation to use DiminishingAdd instead of max()
3. **Long-term:** Implement full A5 specification with fog, light colors, and world position

---

## Status

- ✅ Identified shader specification mismatches
- ✅ Documented correct A5 implementation requirements
- ✅ **FIXED: Updated World.hlsl to use DiminishingAdd formula**
- ✅ **FIXED: Updated WorldConstants struct with all required fields**
- ✅ **FIXED: Added indoor/outdoor light colors (not just brightness)**
- ✅ **FIXED: Implemented distance fog blending**
- ✅ **FIXED: Added world position pass-through for fog calculation**
- ⏳ Need to rebuild and test to verify fixes resolve bright/dark chunk issue

