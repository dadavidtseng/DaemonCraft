//----------------------------------------------------------------------------------------------------
// Block.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <cstdint>

//----------------------------------------------------------------------------------------------------
//1. An ultra-flyweight voxel (volumetric element); one unit (1x1x1) of world-stuff.
//2. Each block knows its type, which is an index (stored as an unsigned char / uint8_t) into a global table of block definitions (see below).
//3. sizeof(Block) == 1 byte
//4. In later assignments, each block will also store any additional information critical to its function on a per-block basis;
//   all other information (e.g., per-type) is stored on its block definition or elsewhere.
class Block
{
public:
    uint8_t m_typeIndex = 0; // Ultra-flyweight - 1 byte only

    // All other data lives in BlockDefinition
};
