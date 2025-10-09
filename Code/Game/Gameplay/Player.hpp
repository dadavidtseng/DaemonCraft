//----------------------------------------------------------------------------------------------------
// Player.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Entity.hpp"
//----------------------------------------------------------------------------------------------------
#include <cstdint>

//-Forward-Declaration--------------------------------------------------------------------------------
class Camera;

//----------------------------------------------------------------------------------------------------
enum class ePlayerCameraMode : int8_t
{
    FREE_FLY,
    SPECTATOR_XY
};

//----------------------------------------------------------------------------------------------------
class Player : public Entity
{
public:
    explicit Player(Game* owner);
    ~Player() override;

    void Update(float deltaSeconds) override;
    void Render() const override;
    void UpdateFromInput(float deltaSeconds);
    void UpdateFromKeyBoard(float deltaSeconds);
    void UpdateFromController(float deltaSeconds);

    Camera* GetCamera() const;

private:
    Camera*           m_worldCamera   = nullptr;
    float             m_moveSpeed     = 4.f;
    ePlayerCameraMode m_cameraMode    = ePlayerCameraMode::FREE_FLY;
    bool              m_wasCKeyPressed = false;
};
