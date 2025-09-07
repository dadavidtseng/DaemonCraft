//----------------------------------------------------------------------------------------------------
// Game.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include <cstdint>

#include "World.hpp"

//----------------------------------------------------------------------------------------------------
class Camera;
class Clock;
class Player;
class Prop;

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
    void SpawnProp();

    Camera* m_screenCamera = nullptr;
    Player* m_player       = nullptr;
    World*  m_world        = nullptr;

    Prop*      m_grid       = nullptr;
    Clock*     m_gameClock  = nullptr;
    eGameState m_gameState  = eGameState::GAME;
};
