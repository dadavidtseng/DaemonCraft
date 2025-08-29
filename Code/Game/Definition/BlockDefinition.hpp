//----------------------------------------------------------------------------------------------------
// BlockDefinition.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <vector>

#include "Engine/Core/StringUtils.hpp"
#include "Engine/Core/XmlUtils.hpp"
#include "Engine/Math/IntVec2.hpp"

//----------------------------------------------------------------------------------------------------
struct sBlockDefinition
{
    // Constructor for initialization
    sBlockDefinition() = default;
    ~sBlockDefinition();

    bool LoadFromXmlElement(XmlElement const* element);

    static void                          InitializeDefinitionFromFile(char const* path);
    static sBlockDefinition*             GetDefinitionByIndex(uint8_t typeIndex);
    static std::vector<sBlockDefinition*> s_definitions;

private:
    String m_name               = "DEFAULT";
    bool   m_isVisible          = false;
    bool   m_isSolid            = false;
    bool   m_isOpaque           = false;
    IntVec2   m_topSpriteCoords    = IntVec2::ZERO;
    IntVec2  m_bottomSpriteCoords = IntVec2::ZERO;
    IntVec2  m_sideSpriteCoords   = IntVec2::ZERO;
    float  m_indoorLighting     = 0.f;
};
