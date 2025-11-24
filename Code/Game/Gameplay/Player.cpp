//----------------------------------------------------------------------------------------------------
// Player.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Player.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Game.hpp"
#include "Game/Gameplay/World.hpp"  // Assignment 6: For PushEntityOutOfBlocks() in physics mode cycling
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Math/MathUtils.hpp"
#include "Engine/Renderer/Camera.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"  // Assignment 6: For AABB debug wireframe rendering

//----------------------------------------------------------------------------------------------------
Player::Player(Game* owner)
    : Entity(owner)
{
    m_worldCamera = new Camera();
    m_worldCamera->SetPerspectiveGraphicView(2.f, 60.f, 0.1f, 10000.f);
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

    // Assignment 6: Initialize Player physics AABB
    // Player is 1.80m tall, 0.60m wide (0.30m radius)
    // m_position is feet position, AABB is local space relative to position
    // Box extends from feet (z=0.0) to head (z=1.80), centered at (0, 0, 0.90)
    m_physicsAABB = AABB3(Vec3(-0.30f, -0.30f, 0.0f), Vec3(0.30f, 0.30f, 1.80f));

    // Assignment 6: Enable physics and set default mode to WALKING
    m_physicsMode = ePhysicsMode::WALKING;
    m_physicsEnabled = true;
}

//----------------------------------------------------------------------------------------------------
Player::~Player()
{
    GAME_SAFE_RELEASE(m_worldCamera);
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Override Entity::Update() to ensure correct call order
// Critical order: UpdateFromInput() → Entity::Update() (physics) → UpdateCamera()
// This wiring enables:
// 1. Input builds acceleration (UpdateFromInput)
// 2. Physics integrates acceleration → velocity → position (Entity::Update)
// 3. Camera follows updated player state (UpdateCamera)
void Player::Update(float const deltaSeconds)
{
    // Step 1: Process all input and build acceleration for this frame
    // In spectator modes, this updates m_spectatorPosition/Orientation instead
    UpdateFromInput(deltaSeconds);

    // Step 2: Run Newtonian physics simulation (gravity, friction, collision)
    // This integrates m_acceleration → m_velocity → m_position
    // Only affects player in non-spectator modes (spectator modes have no acceleration)
    Entity::Update(deltaSeconds);

    // Step 3: Update camera transform based on current camera mode
    // Camera positioning depends on final player position after physics
    UpdateCamera();
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Render player debug visualization
// Draw physics AABB as cyan wireframe (visible through walls) and position sphere at feet
void Player::Render() const
{
    // Display current camera mode and physics mode at top-left of screen
    // This provides essential feedback about current control state
    const char* cameraModeNames[] = {
        "FIRST_PERSON",   // eCameraMode::FIRST_PERSON
        "OVER_SHOULDER",  // eCameraMode::OVER_SHOULDER
        "SPECTATOR",      // eCameraMode::SPECTATOR
        "SPECTATOR_XY",   // eCameraMode::SPECTATOR_XY
        "INDEPENDENT"     // eCameraMode::INDEPENDENT
    };

    const char* physicsModeNames[] = {
        "WALKING",  // ePhysicsMode::WALKING
        "FLYING",   // ePhysicsMode::FLYING
        "NOCLIP"    // ePhysicsMode::NOCLIP
    };

    int cameraModeIndex = static_cast<int>(m_cameraMode);
    int physicsModeIndex = static_cast<int>(m_physicsMode);

    // Display mode information at top-left with key hints
    std::string modeDisplayText = Stringf("Camera: %s (C)  |  Physics: %s (V)",
        cameraModeNames[cameraModeIndex],
        physicsModeNames[physicsModeIndex]);

    // Position at top-left corner with small padding
    Vec2 position = Vec2(10.f, 1060.f);  // Near top-left (assuming 1080p screen height)
    float textSize = 20.f;
    Vec2 alignment = Vec2::ZERO;  // Left-aligned
    float duration = 0.0f;  // Single frame (redrawn every frame)
    Rgba8 textColor = Rgba8::YELLOW;  // High visibility yellow text

    DebugAddScreenText(modeDisplayText, position, textSize, alignment, duration, textColor, textColor);

    // Only render debug visualization if camera is not in first-person attached to this player
    // In first-person mode, we don't want to see our own collision box blocking the view
    if (m_cameraMode != eCameraMode::FIRST_PERSON)
    {
        // Get world-space AABB (m_physicsAABB transformed by m_position)
        AABB3 worldAABB = GetWorldAABB();

        // Draw AABB wireframe as 12 edges (cyan, X-ray mode for visibility through walls)
        // Duration 0.0f = single frame only (redrawn every frame)
        Rgba8 const wireframeColor = Rgba8::CYAN;
        float const lineDuration = 0.0f;
        float const lineThickness = 0.02f;  // Thin lines for wireframe
        eDebugRenderMode const mode = eDebugRenderMode::X_RAY;

        // Bottom face (4 edges at z = mins.z)
        Vec3 bottomBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
        Vec3 bottomBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_mins.z);
        Vec3 bottomTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);
        Vec3 bottomTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_mins.z);

        DebugAddWorldLine(bottomBL, bottomBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomBR, bottomTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTR, bottomTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTL, bottomBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Top face (4 edges at z = maxs.z)
        Vec3 topBL = Vec3(worldAABB.m_mins.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
        Vec3 topBR = Vec3(worldAABB.m_maxs.x, worldAABB.m_mins.y, worldAABB.m_maxs.z);
        Vec3 topTR = Vec3(worldAABB.m_maxs.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);
        Vec3 topTL = Vec3(worldAABB.m_mins.x, worldAABB.m_maxs.y, worldAABB.m_maxs.z);

        DebugAddWorldLine(topBL, topBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topBR, topTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topTR, topTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(topTL, topBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Vertical edges (4 edges connecting bottom to top)
        DebugAddWorldLine(bottomBL, topBL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomBR, topBR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTR, topTR, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);
        DebugAddWorldLine(bottomTL, topTL, lineThickness, lineDuration, wireframeColor, wireframeColor, mode);

        // Draw cyan sphere at player feet position (origin marker)
        DebugAddWorldWireSphere(m_position, 0.1f, lineDuration, wireframeColor, wireframeColor, mode);
    }
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
    // Assignment 6: Physics-based movement with acceleration instead of direct velocity manipulation

    // Debug: Reset position to origin on H key
    if (g_input->WasKeyJustPressed(KEYCODE_H))
    {
        if (m_game->IsAttractMode() == false)
        {
            m_position    = Vec3::ZERO;
            m_orientation = EulerAngles::ZERO;
        }
    }

    // Assignment 6: Camera mode cycling with C key
    // Cycle: FIRST_PERSON → OVER_SHOULDER → SPECTATOR → SPECTATOR_XY → INDEPENDENT → FIRST_PERSON
    if (g_input->WasKeyJustPressed(KEYCODE_C))
    {
        // Store previous mode to detect spectator transitions
        eCameraMode previousMode = m_cameraMode;

        // Cycle to next mode (modulo 5 for wraparound)
        int currentModeInt = static_cast<int>(m_cameraMode);
        int nextModeInt = (currentModeInt + 1) % 5;
        m_cameraMode = static_cast<eCameraMode>(nextModeInt);

        // When transitioning TO spectator modes, save current camera state
        // This preserves the player's current view when entering free-flying mode
        bool enteringSpectator = (previousMode == eCameraMode::FIRST_PERSON || previousMode == eCameraMode::OVER_SHOULDER);
        bool newModeIsSpectator = (m_cameraMode == eCameraMode::SPECTATOR ||
                                   m_cameraMode == eCameraMode::SPECTATOR_XY ||
                                   m_cameraMode == eCameraMode::INDEPENDENT);

        if (enteringSpectator && newModeIsSpectator)
        {
            // Preserve current camera position and orientation when detaching
            m_spectatorPosition = m_position;
            m_spectatorOrientation = m_orientation;
        }

        // Display mode change for debugging
        switch (m_cameraMode)
        {
            case eCameraMode::FIRST_PERSON:
                DebuggerPrintf("Camera Mode: FIRST_PERSON (Eye height, player not rendered)\n");
                break;
            case eCameraMode::OVER_SHOULDER:
                DebuggerPrintf("Camera Mode: OVER_SHOULDER (4m behind, player visible)\n");
                break;
            case eCameraMode::SPECTATOR:
                DebuggerPrintf("Camera Mode: SPECTATOR (Free-fly detached, full 3D)\n");
                break;
            case eCameraMode::SPECTATOR_XY:
                DebuggerPrintf("Camera Mode: SPECTATOR_XY (Free-fly detached, XY-plane only)\n");
                break;
            case eCameraMode::INDEPENDENT:
                DebuggerPrintf("Camera Mode: INDEPENDENT (Frozen camera, player moves)\n");
                break;
        }
    }

    // Assignment 6: Physics mode cycling with V key
    // Cycle: WALKING → FLYING → NOCLIP → WALKING
    if (g_input->WasKeyJustPressed(KEYCODE_V))
    {
        // Store previous mode for safety check
        ePhysicsMode previousMode = m_physicsMode;

        // Cycle to next mode (modulo 3 for wraparound)
        int currentModeInt = static_cast<int>(m_physicsMode);
        int nextModeInt = (currentModeInt + 1) % 3;
        m_physicsMode = static_cast<ePhysicsMode>(nextModeInt);

        // Safety: When exiting NOCLIP mode, push player out of blocks to prevent stuck state
        if (previousMode == ePhysicsMode::NOCLIP && m_physicsMode != ePhysicsMode::NOCLIP)
        {
            World* world = m_game->GetWorld();
            if (world != nullptr)
            {
                world->PushEntityOutOfBlocks(this);
            }
        }

        // Display mode change for debugging
        switch (m_physicsMode)
        {
            case ePhysicsMode::WALKING:
                DebuggerPrintf("Physics Mode: WALKING (Gravity + Collision, Jump enabled)\n");
                break;
            case ePhysicsMode::FLYING:
                DebuggerPrintf("Physics Mode: FLYING (No gravity, Collision enabled, Q/E vertical)\n");
                break;
            case ePhysicsMode::NOCLIP:
                DebuggerPrintf("Physics Mode: NOCLIP (No gravity, No collision, Q/E vertical)\n");
                break;
        }
    }

    // Assignment 6: Spectator camera mode handling
    // In SPECTATOR/SPECTATOR_XY modes: freeze player, move spectator camera instead
    // In INDEPENDENT mode: player moves normally, camera stays frozen
    bool isSpectatorMode = (m_cameraMode == eCameraMode::SPECTATOR || m_cameraMode == eCameraMode::SPECTATOR_XY);

    if (isSpectatorMode)
    {
        // Spectator mode: Move camera, not player
        // Build local-space movement vector from WASD input
        Vec3 localMovement = Vec3::ZERO;

        if (g_input->IsKeyDown(KEYCODE_W)) localMovement.x += 1.0f;  // Forward
        if (g_input->IsKeyDown(KEYCODE_S)) localMovement.x -= 1.0f;  // Backward
        if (g_input->IsKeyDown(KEYCODE_A)) localMovement.y += 1.0f;  // Left (positive = left direction)
        if (g_input->IsKeyDown(KEYCODE_D)) localMovement.y -= 1.0f;  // Right (negative = -left direction)

        // Q/E vertical movement (allowed in both SPECTATOR and SPECTATOR_XY)
        if (g_input->IsKeyDown(KEYCODE_Q)) localMovement.z -= 1.0f;  // Down
        if (g_input->IsKeyDown(KEYCODE_E)) localMovement.z += 1.0f;  // Up

        // SPECTATOR_XY mode: Lock Z movement (XY-plane only)
        if (m_cameraMode == eCameraMode::SPECTATOR_XY)
        {
            localMovement.z = 0.0f;
        }

        // Normalize to prevent diagonal speed exploit
        if (localMovement.GetLengthSquared() > 0.0f)
        {
            localMovement = localMovement.GetNormalized();
        }

        // Transform local movement to world space using spectator orientation
        Vec3 forward, left, up;
        m_spectatorOrientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

        Vec3 worldMovement = (forward * localMovement.x) + (left * localMovement.y) + (up * localMovement.z);

        // Apply sprint multiplier if Shift held
        float sprintMultiplier = g_input->IsKeyDown(KEYCODE_SHIFT) ? PLAYER_SPRINT_MULTIPLIER : 1.0f;

        // Update spectator camera position directly (not physics-based)
        float spectatorSpeed = 10.0f;  // Spectator camera movement speed (m/s)
        m_spectatorPosition += worldMovement * spectatorSpeed * sprintMultiplier * deltaSeconds;

        // Mouse look updates spectator orientation (not player orientation)
        m_spectatorOrientation.m_yawDegrees -= g_input->GetCursorClientDelta().x * 0.075f;
        m_spectatorOrientation.m_pitchDegrees += g_input->GetCursorClientDelta().y * 0.075f;
        m_spectatorOrientation.m_pitchDegrees = GetClamped(m_spectatorOrientation.m_pitchDegrees, -85.f, 85.f);

        // Early return: Don't process player movement in spectator modes
        UpdateCamera();
        return;
    }

    // Step 1: Build local-space movement vector from WASD input
    Vec3 localMovement = Vec3::ZERO;

    if (g_input->IsKeyDown(KEYCODE_W)) localMovement.x += 1.0f;  // Forward
    if (g_input->IsKeyDown(KEYCODE_S)) localMovement.x -= 1.0f;  // Backward
    if (g_input->IsKeyDown(KEYCODE_A)) localMovement.y += 1.0f;  // Left (positive = left direction)
    if (g_input->IsKeyDown(KEYCODE_D)) localMovement.y -= 1.0f;  // Right (negative = -left direction)

    // Q/E vertical movement only allowed in FLYING and NOCLIP modes
    if (m_physicsMode == ePhysicsMode::FLYING || m_physicsMode == ePhysicsMode::NOCLIP)
    {
        if (g_input->IsKeyDown(KEYCODE_Q)) localMovement.z -= 1.0f;  // Down
        if (g_input->IsKeyDown(KEYCODE_E)) localMovement.z += 1.0f;  // Up
    }

    // WALKING mode: Force Z component to zero (no manual vertical movement)
    if (m_physicsMode == ePhysicsMode::WALKING)
    {
        localMovement.z = 0.0f;
    }

    // Step 2: Normalize to prevent diagonal speed exploit (√2 speed when moving diagonally)
    if (localMovement.GetLengthSquared() > 0.0f)
    {
        localMovement = localMovement.GetNormalized();
    }

    // Step 3: Transform local movement to world space using player orientation
    Vec3 forward, left, up;
    m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

    Vec3 worldMovement = (forward * localMovement.x) + (left * localMovement.y) + (up * localMovement.z);

    // Step 4: Apply sprint multiplier if Shift held
    float sprintMultiplier = g_input->IsKeyDown(KEYCODE_SHIFT) ? PLAYER_SPRINT_MULTIPLIER : 1.0f;

    // Step 5: Add acceleration (force) for this frame
    // Physics system will integrate this into velocity in Entity::UpdatePhysics()
    m_acceleration += worldMovement * PLAYER_WALK_ACCELERATION * sprintMultiplier;

    // Assignment 6: Jump logic - Space key applies instant vertical velocity impulse
    // Only works in WALKING mode when player is grounded (no double-jump, no air-jump)
    if (g_input->WasKeyJustPressed(KEYCODE_SPACE))
    {
        // Check if player can jump: must be grounded AND in WALKING mode
        if (m_isOnGround && m_physicsMode == ePhysicsMode::WALKING)
        {
            // Apply instant upward velocity impulse (+8.5 m/s)
            // This is an impulse (instant velocity change), not acceleration
            m_velocity.z = PLAYER_JUMP_VELOCITY;
        }
    }

    // Mouse look: Update player orientation
    // INDEPENDENT mode: Mouse controls player orientation for WASD movement direction (camera stays frozen)
    // Other modes: Mouse controls both player and camera orientation
    m_orientation.m_yawDegrees -= g_input->GetCursorClientDelta().x * 0.075f;
    m_orientation.m_pitchDegrees += g_input->GetCursorClientDelta().y * 0.075f;
    m_orientation.m_pitchDegrees = GetClamped(m_orientation.m_pitchDegrees, -85.f, 85.f);

    // Note: UpdateCamera() now called in Player::Update() after Entity::Update() (physics)
    // This ensures camera positioning uses final player position after physics integration
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

    // Note: UpdateCamera() now called in Player::Update() after Entity::Update() (physics)
    // This ensures camera positioning uses final player position after physics integration
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Position camera based on current camera mode
void Player::UpdateCamera()
{
    switch (m_cameraMode)
    {
        case eCameraMode::FIRST_PERSON:
        {
            // Camera positioned at player eye height (1.65m above feet)
            // Orientation matches player orientation exactly
            // Player entity should not be rendered in this mode
            Vec3 eyePosition = GetEyePosition();
            m_worldCamera->SetPositionAndOrientation(eyePosition, m_orientation);
            break;
        }

        case eCameraMode::OVER_SHOULDER:
        {
            // Camera positioned 4m behind player eye position, with raycast wall clipping prevention
            // Player entity is visible in this mode (third-person view)
            Vec3 eyePosition = GetEyePosition();

            // Get forward vector from player orientation
            Vec3 forward, left, up;
            m_orientation.GetAsVectors_IFwd_JLeft_KUp(forward, left, up);

            // Calculate desired camera position (4m behind eye position)
            Vec3 desiredCameraPos = eyePosition - (forward * CAMERA_OVER_SHOULDER_DISTANCE);

            // Raycast from eye to desired camera position to check for walls
            // Direction is backward (-forward), distance is 4.0m
            RaycastResult result = m_game->GetWorld()->RaycastVoxel(eyePosition, -forward, CAMERA_OVER_SHOULDER_DISTANCE);

            // If ray hit a wall, pull camera forward to prevent clipping
            // Offset by 0.1m along impact normal to avoid z-fighting
            Vec3 actualCameraPos = result.m_didImpact
                ? (result.m_impactPosition + result.m_impactNormal * 0.1f)
                : desiredCameraPos;

            m_worldCamera->SetPositionAndOrientation(actualCameraPos, m_orientation);
            break;
        }

        case eCameraMode::SPECTATOR:
        {
            // Free-flying detached camera with full 3D movement
            // Camera uses m_spectatorPosition and m_spectatorOrientation (updated by input)
            // Player is frozen (no movement input applied to player)
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }

        case eCameraMode::SPECTATOR_XY:
        {
            // Free-flying detached camera with XY-plane only movement (Z locked)
            // Camera uses m_spectatorPosition and m_spectatorOrientation
            // Player is frozen (no movement input applied to player)
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }

        case eCameraMode::INDEPENDENT:
        {
            // Camera frozen at m_spectatorPosition with m_spectatorOrientation
            // Player continues to move normally (decoupled from camera)
            // Useful for debugging and observing player movement from fixed viewpoint
            m_worldCamera->SetPositionAndOrientation(m_spectatorPosition, m_spectatorOrientation);
            break;
        }
    }
}

//----------------------------------------------------------------------------------------------------
Camera* Player::GetCamera() const
{
    return m_worldCamera;
}

Vec3 const& Player::GetVelocity()
{
    return m_velocity;
}

//----------------------------------------------------------------------------------------------------
// Assignment 6: Helper method to calculate player eye position
// Returns player position offset by eye height (1.65m above feet)
Vec3 Player::GetEyePosition() const
{
    return m_position + Vec3(0.f, 0.f, PLAYER_EYE_HEIGHT);
}

//----------------------------------------------------------------------------------------------------
// Get current camera mode
eCameraMode Player::GetCameraMode() const
{
    return m_cameraMode;
}
