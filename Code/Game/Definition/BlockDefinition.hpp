//----------------------------------------------------------------------------------------------------
// BlockDefinition.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>

#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/IntVec2.hpp"
#include "Engine/Math/Vec2.hpp"

//----------------------------------------------------------------------------------------------------
/// @brief 1.	One of several types of blocks used in the game (e.g. grass, stone, dirt).
/// 2.	Block definitions should be read in from the provided definition file.
/// 3.	Each different type of block has one block definition instance created for it, which describes everything we need to know about that type of block.
/// 4.	Each block definition stores bools for whether that type of block is (a) visible, (b) solid (physical), and (c) opaque (light does not pass through), as well as the UV texture coordinates (mins and maxs) to use for that block type’s (d) top, (e) sides, and (f) bottom.
/// 5.	The block definition class itself owns the (static) array of block definition.
/// 6.	Block definitions need not be flyweight; they may hold any and all information about each type of block.  For instance, the block definition for grass may eventually hold UVs (for top, side, and block bottom faces), audio IDs (for breaking, placing, and walking on this type of block), emissive light level (for glowstone, moonstone, etc.), whether it is solid and/or opaque, its toughness (i.e. resistance to digging and/or explosions), and any other special information as later required (e.g. which tool(s) it can be most efficiently dug with, flammability, etc.)..
///
struct sBlockDefinition
{
    // Constructor for initialization
    sBlockDefinition() = default;
    ~sBlockDefinition();

    bool LoadFromXmlElement(XmlElement const* element);

    static void                           InitializeDefinitionFromFile(char const* path);
    static sBlockDefinition*              GetDefinitionByIndex(uint8_t typeIndex);
    static std::vector<sBlockDefinition*> s_definitions;

    Vec2 GetTopUVs() const { return Vec2(m_topSpriteCoords); }
    Vec2 GetBottomUVs() const { return Vec2(m_bottomSpriteCoords); }
    Vec2 GetSideUVs() const { return Vec2(m_sideSpriteCoords); }
    bool IsVisible() const { return m_isVisible; }
    bool IsSolid() const { return m_isSolid; }
    bool IsOpaque() const { return m_isOpaque; }

private:
    String  m_name               = "DEFAULT";
    bool    m_isVisible          = false;
    bool    m_isSolid            = false;
    bool    m_isOpaque           = false;
    IntVec2 m_topSpriteCoords    = IntVec2::ZERO;
    IntVec2 m_bottomSpriteCoords = IntVec2::ZERO;
    IntVec2 m_sideSpriteCoords   = IntVec2::ZERO;
    float   m_indoorLighting     = 0.f;
};
