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
// Assignment 6: Camera mode determines view perspective and camera behavior
enum class eCameraMode : uint8_t
{
    FIRST_PERSON,   // Camera at player eye height, player not rendered
    OVER_SHOULDER,  // Camera 4m behind player, player rendered
    SPECTATOR,      // Free-flying camera detached from player, full 3D movement
    SPECTATOR_XY,   // Free-flying camera detached from player, XY-plane only (no Z movement)
    INDEPENDENT     // Camera stays in place while player moves (developer debug mode)
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
    void UpdateCamera();  // Assignment 6: Position camera based on camera mode

    Camera* GetCamera() const;
    Vec3 const& GetVelocity();
    Vec3 GetEyePosition() const;  // Assignment 6: Helper to get player eye position
    eCameraMode GetCameraMode() const;  // Get current camera mode

private:
    Camera*      m_worldCamera   = nullptr;
    float        m_moveSpeed     = 4.f;

    // Assignment 6: Camera system
    eCameraMode     m_cameraMode               = eCameraMode::FIRST_PERSON;  // Current camera view mode
    Vec3            m_spectatorPosition        = Vec3(-50.f, -50.f, 150.f);  // Spectator camera position (when detached)
    EulerAngles     m_spectatorOrientation     = EulerAngles(45.f, 45.f, 0.f); // Spectator camera orientation (when detached)
    bool            m_wasCKeyPressed           = false;                      // C key debounce for camera mode cycling
};
