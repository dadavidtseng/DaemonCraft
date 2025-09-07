//----------------------------------------------------------------------------------------------------
// Player.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Player.hpp"

#include "Game.hpp"
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Game/Framework/GameCommon.hpp"

//----------------------------------------------------------------------------------------------------
Player::Player(Game* owner)
    : Entity(owner)
{
    m_worldCamera = new Camera();
    m_worldCamera->SetPerspectiveGraphicView(2.f, 60.f, 0.1f, 1000.f);
    m_worldCamera->SetNormalizedViewport(AABB2::ZERO_TO_ONE);

    Mat44 c2r;

    c2r.m_values[Mat44::Ix] = 0.f;
    c2r.m_values[Mat44::Iz] = 1.f;
    c2r.m_values[Mat44::Jx] = -1.f;
    c2r.m_values[Mat44::Jy] = 0.f;
    c2r.m_values[Mat44::Ky] = 1.f;
    c2r.m_values[Mat44::Kz] = 0.f;

    m_worldCamera->SetCameraToRenderTransform(c2r);

    m_position    = Vec3(-50, -50, 150);
    m_orientation = EulerAngles(45, 45, 0);
}

//----------------------------------------------------------------------------------------------------
Player::~Player()
{
    GAME_SAFE_RELEASE(m_worldCamera);
}

//----------------------------------------------------------------------------------------------------
void Player::Update(float const deltaSeconds)
{
    UpdateFromInput(deltaSeconds);
}

//----------------------------------------------------------------------------------------------------
void Player::Render() const
{
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromInput(float const deltaSeconds)
{
    UpdateFromKeyBoard(deltaSeconds);
    UpdateFromController(deltaSeconds);
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromKeyBoard(float deltaSeconds)
{
    if (g_input->WasKeyJustPressed(KEYCODE_H))
    {
        if (m_game->IsAttractMode() == false)
        {
            m_position    = Vec3::ZERO;
            m_orientation = EulerAngles::ZERO;
        }
    }

    Vec3 forward, left, up = Vec3::ZERO;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    m_velocity = Vec3::ZERO;

    if (g_input->IsKeyDown(KEYCODE_W)) m_velocity += forward * m_moveSpeed;
    if (g_input->IsKeyDown(KEYCODE_S)) m_velocity -= forward * m_moveSpeed;
    if (g_input->IsKeyDown(KEYCODE_A)) m_velocity += left * m_moveSpeed;
    if (g_input->IsKeyDown(KEYCODE_D)) m_velocity -= left * m_moveSpeed;
    if (g_input->IsKeyDown(KEYCODE_Z)) m_velocity -= Vec3(0.f, 0.f, 1.f) * m_moveSpeed;
    if (g_input->IsKeyDown(KEYCODE_C)) m_velocity += Vec3(0.f, 0.f, 1.f) * m_moveSpeed;

    if (g_input->IsKeyDown(KEYCODE_SHIFT)) deltaSeconds *= 20.f;

    m_position += m_velocity * deltaSeconds;


    m_orientation.m_yawDegrees -= g_input->GetCursorClientDelta().x * 0.075f;
    m_orientation.m_pitchDegrees += g_input->GetCursorClientDelta().y * 0.075f;
    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);

    m_angularVelocity.m_rollDegrees = 0.f;

    if (g_input->IsKeyDown(KEYCODE_Q)) m_angularVelocity.m_rollDegrees = 90.f;
    if (g_input->IsKeyDown(KEYCODE_E)) m_angularVelocity.m_rollDegrees = -90.f;

    m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
    m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

    m_worldCamera->SetPositionAndOrientation(m_position, m_orientation);
}

//----------------------------------------------------------------------------------------------------
void Player::UpdateFromController(float deltaSeconds)
{
    XboxController const& controller = g_input->GetController(0);

    if (controller.WasButtonJustPressed(XBOX_BUTTON_START))
    {
        if (m_game->IsAttractMode() == false)
        {
            m_position    = Vec3::ZERO;
            m_orientation = EulerAngles::ZERO;
        }
    }

    Vec2 const leftStickInput = controller.GetLeftStick().GetPosition();
    m_velocity += Vec3(leftStickInput.y, -leftStickInput.x, 0.f) * m_moveSpeed;

    if (controller.IsButtonDown(XBOX_BUTTON_LSHOULDER)) m_velocity -= Vec3(0.f, 0.f, 1.f) * m_moveSpeed;
    if (controller.IsButtonDown(XBOX_BUTTON_RSHOULDER)) m_velocity += Vec3(0.f, 0.f, 1.f) * m_moveSpeed;

    if (controller.IsButtonDown(XBOX_BUTTON_A)) deltaSeconds *= 20.f;

    m_position += m_velocity * deltaSeconds;

    Vec2 const rightStickInput = controller.GetRightStick().GetPosition();
    m_orientation.m_yawDegrees -= rightStickInput.x * 0.125f;
    m_orientation.m_pitchDegrees -= rightStickInput.y * 0.125f;

    m_angularVelocity.m_rollDegrees = 0.f;

    float const leftTriggerInput  = controller.GetLeftTrigger();
    float const rightTriggerInput = controller.GetRightTrigger();

    if (leftTriggerInput != 0.f)
    {
        m_angularVelocity.m_rollDegrees -= 90.f;
    }

    if (rightTriggerInput != 0.f)
    {
        m_angularVelocity.m_rollDegrees += 90.f;
    }

    m_orientation.m_rollDegrees += m_angularVelocity.m_rollDegrees * deltaSeconds;
    m_orientation.m_rollDegrees = GetClamped(m_orientation.m_rollDegrees, -45.f, 45.f);

    m_worldCamera->SetPositionAndOrientation(m_position, m_orientation);
}

//----------------------------------------------------------------------------------------------------
Camera* Player::GetCamera() const
{
    return m_worldCamera;
}
