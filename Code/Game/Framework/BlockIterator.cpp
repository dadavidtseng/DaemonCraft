//----------------------------------------------------------------------------------------------------
// BlockIterator.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Framework/BlockIterator.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/Block.hpp"

//----------------------------------------------------------------------------------------------------
BlockIterator::BlockIterator(Chunk* chunk, int blockIndex)
    : m_chunk(chunk)
    , m_blockIndex(blockIndex)
{
}

//----------------------------------------------------------------------------------------------------
Block* BlockIterator::GetBlock() const
{
    if (!IsValid()) return nullptr;
    
    IntVec3 localCoords = GetLocalCoords();
    return m_chunk->GetBlock(localCoords.x, localCoords.y, localCoords.z);
}

//----------------------------------------------------------------------------------------------------
IntVec3 BlockIterator::GetLocalCoords() const
{
    return Chunk::IndexToLocalCoords(m_blockIndex);
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::IsValid() const
{
    return m_chunk != nullptr && IsIndexValid(m_blockIndex);
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveNorth()
{
    return MoveByOffset(IntVec3(0, 1, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveSouth()
{
    return MoveByOffset(IntVec3(0, -1, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveEast()
{
    return MoveByOffset(IntVec3(1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveWest()
{
    return MoveByOffset(IntVec3(-1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveUp()
{
    return MoveByOffset(IntVec3(0, 0, 1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveDown()
{
    return MoveByOffset(IntVec3(0, 0, -1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::MoveByOffset(IntVec3 const& offset)
{
    if (!IsValid()) return false;
    
    int newIndex = CalculateIndexFromOffset(offset);
    if (IsIndexValid(newIndex))
    {
        m_blockIndex = newIndex;
        return true;
    }
    
    return false;
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetNeighbor(IntVec3 const& offset) const
{
    if (!IsValid()) return BlockIterator(nullptr, -1);
    
    int newIndex = CalculateIndexFromOffset(offset);
    if (IsIndexValid(newIndex))
    {
        return BlockIterator(m_chunk, newIndex);
    }
    
    return BlockIterator(nullptr, -1);  // Invalid iterator
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetNorthNeighbor() const
{
    return GetNeighbor(IntVec3(0, 1, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetSouthNeighbor() const
{
    return GetNeighbor(IntVec3(0, -1, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetEastNeighbor() const
{
    return GetNeighbor(IntVec3(1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetWestNeighbor() const
{
    return GetNeighbor(IntVec3(-1, 0, 0));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetUpNeighbor() const
{
    return GetNeighbor(IntVec3(0, 0, 1));
}

//----------------------------------------------------------------------------------------------------
BlockIterator BlockIterator::GetDownNeighbor() const
{
    return GetNeighbor(IntVec3(0, 0, -1));
}

//----------------------------------------------------------------------------------------------------
bool BlockIterator::IsIndexValid(int index) const
{
    return index >= 0 && index < BLOCKS_PER_CHUNK;
}

//----------------------------------------------------------------------------------------------------
int BlockIterator::CalculateIndexFromOffset(IntVec3 const& offset) const
{
    IntVec3 currentCoords = GetLocalCoords();
    IntVec3 newCoords = currentCoords + offset;
    
    // Check bounds using bit operations for efficiency
    if (newCoords.x < 0 || newCoords.x >= CHUNK_SIZE_X ||
        newCoords.y < 0 || newCoords.y >= CHUNK_SIZE_Y ||
        newCoords.z < 0 || newCoords.z >= CHUNK_SIZE_Z)
    {
        return -1;  // Out of bounds
    }
    
    // Use bit operations for fast index calculation
    return Chunk::LocalCoordsToIndex(newCoords);
}