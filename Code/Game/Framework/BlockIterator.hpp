//----------------------------------------------------------------------------------------------------
// BlockIterator.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once

#include "Engine/Math/IntVec3.hpp"

//-Forward-Declaration--------------------------------------------------------------------------------
class Chunk;
class Block;

//----------------------------------------------------------------------------------------------------
// BlockIterator - Efficient block iteration with directional movement using bit operations
//----------------------------------------------------------------------------------------------------
class BlockIterator
{
public:
    // Constructor
    explicit BlockIterator(Chunk* chunk, int blockIndex = 0);

    // Basic access
    Block*  GetBlock() const;
    Chunk*  GetChunk() const { return m_chunk; }
    int     GetBlockIndex() const { return m_blockIndex; }
    IntVec3 GetLocalCoords() const;
    bool    IsValid() const;

    // Directional movement (returns true if movement was successful)
    bool MoveNorth();   // +Y direction
    bool MoveSouth();   // -Y direction
    bool MoveEast();    // +X direction
    bool MoveWest();    // -X direction
    bool MoveUp();      // +Z direction
    bool MoveDown();    // -Z direction

    // Movement by offset
    bool MoveByOffset(IntVec3 const& offset);

    // Create iterator at neighboring position (returns invalid iterator if out of bounds)
    BlockIterator GetNeighbor(IntVec3 const& offset) const;
    BlockIterator GetNorthNeighbor() const;
    BlockIterator GetSouthNeighbor() const;
    BlockIterator GetEastNeighbor() const;
    BlockIterator GetWestNeighbor() const;
    BlockIterator GetUpNeighbor() const;
    BlockIterator GetDownNeighbor() const;

private:
    Chunk* m_chunk      = nullptr;
    int    m_blockIndex = 0;

    // Helper methods for bit operations
    bool IsIndexValid(int index) const;
    int  CalculateIndexFromOffset(IntVec3 const& offset) const;
};
