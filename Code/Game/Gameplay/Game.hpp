//----------------------------------------------------------------------------------------------------
// Game.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/World.hpp"

//-Forward-Declaration--------------------------------------------------------------------------------
class Camera;
class Clock;
class Player;
class Prop;
struct Vec3;

//----------------------------------------------------------------------------------------------------
enum class eGameState : uint8_t
{
    ATTRACT,
    GAME
};

//----------------------------------------------------------------------------------------------------
class Game
{
public:
    Game();
    ~Game();

    void Update();
    void Render() const;
    bool IsAttractMode() const;
    Vec3 GetPlayerCameraPosition() const;
    void ShowSimpleDemoWindow();
    void ShowInspectorWindow();
    void ShowDebugLogWindow();

private:
    void UpdateFromInput();
    void UpdateFromKeyBoard();
    void UpdateFromController();
    void UpdateEntities(float gameDeltaSeconds, float systemDeltaSeconds) const;
    void UpdateWorld(float gameDeltaSeconds);

    void RenderAttractMode() const;
    void RenderEntities() const;
    void RenderPlayerBasis() const;

    void SpawnPlayer();

    Camera*    m_screenCamera = nullptr;
    Player*    m_player       = nullptr;
    World*     m_world        = nullptr;
    Clock*     m_gameClock    = nullptr;
    eGameState m_gameState    = eGameState::GAME;

    // Block placing system
    uint8_t m_currentBlockType = 9; // Start with BLOCK_GLOWSTONE (index 9 in XML)

    // Debug display toggle
    bool m_showDebugInfo = true; // F3 toggleable debug info display (default visible)

    // ImGui window visibility toggles
    bool m_showDemoWindow = false;
    bool m_showInspectorWindow = false;
    bool m_showConsoleWindow = false;
};
