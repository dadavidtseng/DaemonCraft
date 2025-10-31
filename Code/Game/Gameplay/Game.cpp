//----------------------------------------------------------------------------------------------------
// Game.cpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#include "Game/Gameplay/Game.hpp"
//----------------------------------------------------------------------------------------------------
#include "Game/Definition/BlockDefinition.hpp"
#include "Game/Framework/App.hpp"
#include "Game/Framework/Chunk.hpp"
#include "Game/Framework/GameCommon.hpp"
#include "Game/Gameplay/Player.hpp"
//----------------------------------------------------------------------------------------------------
#include "Engine/Core/Clock.hpp"
#include "Engine/Core/EngineCommon.hpp"
#include "Engine/Core/ErrorWarningAssert.hpp"
#include "Engine/Input/InputSystem.hpp"
#include "Engine/Platform/Window.hpp"
#include "Engine/Renderer/BitmapFont.hpp"
#include "Engine/Renderer/DebugRenderSystem.hpp"
#include "Engine/Renderer/Renderer.hpp"
#include "Engine/Resource/ResourceSubsystem.hpp"
//----------------------------------------------------------------------------------------------------
#include "ThirdParty/imgui/imgui.h"
#include <algorithm>
#include <vector>
#include <string>
//----------------------------------------------------------------------------------------------------
#include <Windows.h>
#include <shellapi.h>

//----------------------------------------------------------------------------------------------------
// Helper function to open files with system default program
//----------------------------------------------------------------------------------------------------
static void OpenFileWithSystemDefault(const char* filePath)
{
    // Convert relative path to absolute path
    char absolutePath[MAX_PATH];
    DWORD result = GetFullPathNameA(filePath, MAX_PATH, absolutePath, nullptr);

    if (result == 0)
    {
        DebuggerPrintf("Failed to get full path for: %s (Error code: %d)\n", filePath, GetLastError());
        return;
    }

    // Check if file exists
    DWORD fileAttributes = GetFileAttributesA(absolutePath);
    if (fileAttributes == INVALID_FILE_ATTRIBUTES)
    {
        DebuggerPrintf("File does not exist: %s\n", absolutePath);
        return;
    }

    // Use ShellExecuteA to open file with default associated program
    HINSTANCE shellResult = ShellExecuteA(
        nullptr,        // Parent window handle
        "open",         // Operation
        absolutePath,   // File to open (now absolute path)
        nullptr,        // Parameters
        nullptr,        // Working directory
        SW_SHOW         // Show command
    );

    // Check for errors (values <= 32 indicate errors)
    if ((INT_PTR)shellResult <= 32)
    {
        DebuggerPrintf("Failed to open file: %s (Error code: %d)\n", absolutePath, (int)(INT_PTR)shellResult);
    }
    else
    {
        DebuggerPrintf("Successfully opened file: %s\n", absolutePath);
    }
}

//----------------------------------------------------------------------------------------------------
Game::Game()
{
    SpawnPlayer();

    m_screenCamera = new Camera();

    Vec2 const bottomLeft = Vec2::ZERO;
    // Vec2 const screenTopRight = Vec2(SCREEN_SIZE_X, SCREEN_SIZE_Y);
    Vec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();

    m_screenCamera->SetOrthoGraphicView(bottomLeft, clientDimensions);
    m_screenCamera->SetNormalizedViewport(AABB2::ZERO_TO_ONE);
    m_gameClock = new Clock(Clock::GetSystemClock());

#if defined GAME_DEBUG_MODE
    DebugAddWorldBasis(Mat44(), -1.f);

    Mat44 transform;

    transform.SetIJKT3D(-Vec3::Y_BASIS, Vec3::X_BASIS, Vec3::Z_BASIS, Vec3(0.25f, 0.f, 0.25f));
    DebugAddWorldText("X-Forward", transform, 0.25f, Vec2::ONE, -1.f, Rgba8::RED);

    transform.SetIJKT3D(-Vec3::X_BASIS, -Vec3::Y_BASIS, Vec3::Z_BASIS, Vec3(0.f, 0.25f, 0.5f));
    DebugAddWorldText("Y-Left", transform, 0.25f, Vec2::ZERO, -1.f, Rgba8::GREEN);

    transform.SetIJKT3D(-Vec3::X_BASIS, Vec3::Z_BASIS, Vec3::Y_BASIS, Vec3(0.f, -0.25f, 0.25f));
    DebugAddWorldText("Z-Up", transform, 0.25f, Vec2(1.f, 0.f), -1.f, Rgba8::BLUE);
#endif

    sBlockDefinition::InitializeDefinitionFromFile("Data/Definitions/BlockSpriteSheet_BlockDefinitions.xml");

    m_world = new World();
}

//----------------------------------------------------------------------------------------------------
Game::~Game()
{
    GAME_SAFE_RELEASE(m_gameClock);
    GAME_SAFE_RELEASE(m_screenCamera);
    GAME_SAFE_RELEASE(m_player);
    GAME_SAFE_RELEASE(m_world);
}

//----------------------------------------------------------------------------------------------------
void Game::Update()
{
    float const gameDeltaSeconds   = static_cast<float>(m_gameClock->GetDeltaSeconds());
    float const systemDeltaSeconds = static_cast<float>(Clock::GetSystemClock().GetDeltaSeconds());

    UpdateEntities(gameDeltaSeconds, systemDeltaSeconds);
    DebugAddScreenText(Stringf("Time: %.2f\nFPS: %.2f\nScale: %.1f", m_gameClock->GetTotalSeconds(), 1.f / m_gameClock->GetDeltaSeconds(), m_gameClock->GetTimeScale()), m_screenCamera->GetOrthographicTopRight() - Vec2(250.f, 60.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);
    UpdateWorld(gameDeltaSeconds);
    UpdateFromInput();

    // ImGui Debug Window (Phase 0, Task 0.1: IMGUI Integration Test) - Direct Integration
    ImGui::Begin("SimpleMiner Debug", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    ImGui::Text("IMGUI Integration Successful!");
    ImGui::Separator();

    ImGui::Text("Game Stats:");
    ImGui::Text("  FPS: %.1f", 1.f / gameDeltaSeconds);
    ImGui::Text("  Time: %.2f", m_gameClock->GetTotalSeconds());
    ImGui::Text("  Time Scale: %.2f", m_gameClock->GetTimeScale());

    if (m_player && m_player->GetCamera())
    {
        Vec3 camPos = m_player->GetCamera()->GetPosition();
        ImGui::Separator();
        ImGui::Text("Camera Position:");
        ImGui::Text("  X: %.2f", camPos.x);
        ImGui::Text("  Y: %.2f", camPos.y);
        ImGui::Text("  Z: %.2f", camPos.z);
    }

    ImGui::Separator();
    ImGui::Text("Phase 0: Prerequisites");
    ImGui::BulletText("Task 0.1: IMGUI Integration - COMPLETE");
    ImGui::BulletText("Task 0.2: Curve Editor - Pending");
    ImGui::BulletText("Task 0.3: Chunk Regen Controls - Pending");
    ImGui::BulletText("Task 0.4: Noise Visualization - Pending");

    // Demo Window Button
    ImGui::Separator();
    ImGui::Text("ImGui Windows:");

    if (ImGui::Button(m_showDemoWindow ? "Hide Demo Window" : "Show Demo Window"))
    {
        m_showDemoWindow = !m_showDemoWindow;
    }

    if (ImGui::Button(m_showInspectorWindow ? "Hide Inspector" : "Show Inspector"))
    {
        m_showInspectorWindow = !m_showInspectorWindow;
    }

    if (ImGui::Button(m_showConsoleWindow ? "Hide Console" : "Show Console"))
    {
        m_showConsoleWindow = !m_showConsoleWindow;
    }

    // Show windows based on toggles
    if (m_showDemoWindow)
    {
        ShowSimpleDemoWindow();
    }

    if (m_showInspectorWindow)
    {
        ShowInspectorWindow();
    }

    if (m_showConsoleWindow)
    {
        ShowDebugLogWindow();
    }

    ImGui::End();
}

//----------------------------------------------------------------------------------------------------
void Game::Render() const
{
    //-Start-of-Game-Camera---------------------------------------------------------------------------

    g_renderer->BeginCamera(*m_player->GetCamera());

    if (m_gameState == eGameState::GAME)
    {
        RenderEntities();
        m_world->Render();
        Vec2 screenDimensions = Window::s_mainWindow->GetScreenDimensions();
        Vec2 windowDimensions = Window::s_mainWindow->GetWindowDimensions();
        Vec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();
        Vec2 windowPosition   = Window::s_mainWindow->GetWindowPosition();
        Vec2 clientPosition   = Window::s_mainWindow->GetClientPosition();
        DebugAddScreenText(Stringf("ScreenDimensions=(%.1f,%.1f)", screenDimensions.x, screenDimensions.y), Vec2(0, 0), 20.f, Vec2::ZERO, 0.f);
        DebugAddScreenText(Stringf("WindowDimensions=(%.1f,%.1f)", windowDimensions.x, windowDimensions.y), Vec2(0, 20), 20.f, Vec2::ZERO, 0.f);
        DebugAddScreenText(Stringf("ClientDimensions=(%.1f,%.1f)", clientDimensions.x, clientDimensions.y), Vec2(0, 40), 20.f, Vec2::ZERO, 0.f);
        DebugAddScreenText(Stringf("WindowPosition=(%.1f,%.1f)", windowPosition.x, windowPosition.y), Vec2(0, 60), 20.f, Vec2::ZERO, 0.f);
        DebugAddScreenText(Stringf("ClientPosition=(%.1f,%.1f)", clientPosition.x, clientPosition.y), Vec2(0, 80), 20.f, Vec2::ZERO, 0.f);

        RenderPlayerBasis();
    }

    g_renderer->EndCamera(*m_player->GetCamera());

    //-End-of-Game-Camera-----------------------------------------------------------------------------
    //------------------------------------------------------------------------------------------------
    if (m_gameState == eGameState::GAME)
    {
        DebugRenderWorld(*m_player->GetCamera());
    }
    //------------------------------------------------------------------------------------------------
    //-Start-of-Screen-Camera-------------------------------------------------------------------------

    g_renderer->BeginCamera(*m_screenCamera);

    if (m_gameState == eGameState::ATTRACT)
    {
        RenderAttractMode();
    }

    g_renderer->EndCamera(*m_screenCamera);

    //-End-of-Screen-Camera---------------------------------------------------------------------------
    if (m_gameState == eGameState::GAME)
    {
        DebugRenderScreen(*m_screenCamera);
    }
}

//----------------------------------------------------------------------------------------------------
bool Game::IsAttractMode() const
{
    return m_gameState == eGameState::ATTRACT;
}

void Game::UpdateFromInput()
{
    UpdateFromKeyBoard();
    UpdateFromController();
}

//----------------------------------------------------------------------------------------------------
void Game::UpdateFromKeyBoard()
{
    if (m_gameState == eGameState::ATTRACT)
    {
        if (g_input->WasKeyJustPressed(KEYCODE_ESC))
        {
            App::RequestQuit();
        }

        if (g_input->WasKeyJustPressed(KEYCODE_SPACE))
        {
            m_gameState = eGameState::GAME;
        }
    }

    if (m_gameState == eGameState::GAME)
    {
        if (g_input->WasKeyJustPressed(KEYCODE_ESC))
        {
            m_gameState = eGameState::ATTRACT;
        }

        if (g_input->WasKeyJustPressed(KEYCODE_P))
        {
            m_gameClock->TogglePause();
        }

        if (g_input->WasKeyJustPressed(KEYCODE_O))
        {
            m_gameClock->StepSingleFrame();
        }

        if (g_input->IsKeyDown(KEYCODE_T))
        {
            m_gameClock->SetTimeScale(0.1f);
        }

        if (g_input->WasKeyJustReleased(KEYCODE_T))
        {
            m_gameClock->SetTimeScale(1.f);
        }

        if (g_input->WasKeyJustPressed(KEYCODE_F2))
        {
            if (m_world != nullptr)
            {
                m_world->ToggleGlobalChunkDebugDraw();
            }
        }

        if (g_input->WasKeyJustPressed(KEYCODE_F3))
        {
            m_showDebugInfo = !m_showDebugInfo;
            DebuggerPrintf("Debug Info Display: %s\n", m_showDebugInfo ? "ON" : "OFF");
        }

        // ========================================
        // DIGGING AND PLACING SYSTEM
        // ========================================

        // LMB - Dig highest non-air block at or below camera position
        if (g_input->WasKeyJustPressed(KEYCODE_LEFT_MOUSE))
        {
            if (m_world != nullptr && m_player != nullptr)
            {
                Vec3 const cameraPos = m_player->GetCamera()->GetPosition();
                bool const success   = m_world->DigBlockAtCameraPosition(cameraPos);

                if (!success)
                {
                    DebuggerPrintf("No block to dig at camera position\n");
                }
            }
        }

        // RMB - Place current block type above highest non-air block at or below camera position
        if (g_input->WasKeyJustPressed(KEYCODE_RIGHT_MOUSE))
        {
            if (m_world != nullptr && m_player != nullptr)
            {
                Vec3 const cameraPos = m_player->GetCamera()->GetPosition();
                bool const success   = m_world->PlaceBlockAtCameraPosition(cameraPos, m_currentBlockType);
                if (!success)
                {
                    DebuggerPrintf("Cannot place block at camera position\n");
                }
            }
        }

        // Block type cycling - Use number keys to cycle through the three block types
        if (g_input->WasKeyJustPressed(NUMCODE_1) || g_input->WasKeyJustPressed(KEYCODE_UPARROW))
        {
            // Cycle to next block type: Glowstone(9) -> Cobblestone(10) -> ChiseledBrick(11) -> Glowstone(9)
            m_currentBlockType++;
            if (m_currentBlockType > 11) // BLOCK_CHISELED_BRICK
            {
                m_currentBlockType = 9; // Back to BLOCK_GLOWSTONE
            }
            DebuggerPrintf("Current block type: %d\n", m_currentBlockType);
        }

        if (g_input->WasKeyJustPressed(NUMCODE_2) || g_input->WasKeyJustPressed(KEYCODE_DOWNARROW))
        {
            // Cycle to previous block type: ChiseledBrick(11) -> Cobblestone(10) -> Glowstone(9) -> ChiseledBrick(11)
            m_currentBlockType--;
            if (m_currentBlockType < 9) // BLOCK_GLOWSTONE
            {
                m_currentBlockType = 11; // Back to BLOCK_CHISELED_BRICK
            }
            DebuggerPrintf("Current block type: %d\n", m_currentBlockType);
        }

        if (g_input->WasKeyJustPressed(KEYCODE_F8))
        {
            g_app->DeleteAndCreateNewGame();
            return;
        }

#if defined GAME_DEBUG_MODE
        if (m_showDebugInfo)
        {
            DebugAddScreenText(Stringf("Player Position: (%.2f, %.2f, %.2f)", m_player->m_position.x, m_player->m_position.y, m_player->m_position.z), Vec2(0.f, 120.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

            // Add debug info for current block type and coordinates
            DebugAddScreenText(Stringf("Current Block Type: [%d] - Glowstone[9] Cobblestone[10] ChiseledBrick[11]", m_currentBlockType), Vec2(0.f, 140.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

            // Calculate chunk and local coordinates for player position
            IntVec3 playerGlobalCoords = IntVec3((int)m_player->m_position.x, (int)m_player->m_position.y, (int)m_player->m_position.z);
            IntVec2 chunkCoords        = Chunk::GetChunkCoords(playerGlobalCoords);
            IntVec3 localCoords        = Chunk::GlobalCoordsToLocalCoords(playerGlobalCoords);

            DebugAddScreenText(Stringf("ChunkCoords: (%d, %d) LocalCoords: (%d, %d, %d) GlobalCoords: (%d, %d, %d)",
                                       chunkCoords.x, chunkCoords.y,
                                       localCoords.x, localCoords.y, localCoords.z,
                                       playerGlobalCoords.x, playerGlobalCoords.y, playerGlobalCoords.z), Vec2(0.f, 160.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

            // Display current chunk ID and world statistics
            if (m_world)
            {
                if (Chunk const* currentChunk = m_world->GetChunk(chunkCoords))
                {
                    DebugAddScreenText(Stringf("Current Chunk: (%d, %d)",
                                               currentChunk->GetChunkCoords().x,
                                               currentChunk->GetChunkCoords().y), Vec2(0.f, 180.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);
                }

                // Add world statistics like in the screenshot
                DebugAddScreenText(Stringf("Chunks: %d Vertices: %d Indices: %d",
                                           m_world->GetActiveChunkCount(),
                                           m_world->GetTotalVertexCount(),
                                           m_world->GetTotalIndexCount()), Vec2(0.f, 200.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

                // Add JobSystem metrics
                DebugAddScreenText(Stringf("=== Job System ==="), Vec2(0.f, 220.f), 20.f, Vec2::ZERO, 0.f, Rgba8::YELLOW, Rgba8::YELLOW);
                DebugAddScreenText(Stringf("Pending Jobs - Generate: %d Load: %d Save: %d",
                                           m_world->GetPendingGenerateJobCount(),
                                           m_world->GetPendingLoadJobCount(),
                                           m_world->GetPendingSaveJobCount()), Vec2(0.f, 240.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);
            }
        }
#endif
    }
}

//----------------------------------------------------------------------------------------------------
void Game::UpdateFromController()
{
    XboxController const& controller = g_input->GetController(0);

    if (m_gameState == eGameState::ATTRACT)
    {
        if (controller.WasButtonJustPressed(XBOX_BUTTON_BACK))
        {
            App::RequestQuit();
        }

        if (controller.WasButtonJustPressed(XBOX_BUTTON_START))
        {
            m_gameState = eGameState::GAME;
        }
    }

    if (m_gameState == eGameState::GAME)
    {
        if (controller.WasButtonJustPressed(XBOX_BUTTON_BACK))
        {
            m_gameState = eGameState::ATTRACT;
        }

        if (controller.WasButtonJustPressed(XBOX_BUTTON_B))
        {
            m_gameClock->TogglePause();
        }

        if (controller.WasButtonJustPressed(XBOX_BUTTON_Y))
        {
            m_gameClock->StepSingleFrame();
        }

        if (controller.WasButtonJustPressed(XBOX_BUTTON_X))
        {
            m_gameClock->SetTimeScale(0.1f);
        }

        if (controller.WasButtonJustReleased(XBOX_BUTTON_X))
        {
            m_gameClock->SetTimeScale(1.f);
        }

        if (controller.WasButtonJustReleased(XBOX_BUTTON_START))
        {
            g_app->DeleteAndCreateNewGame();
        }
    }
}

//----------------------------------------------------------------------------------------------------
void Game::UpdateEntities(float const gameDeltaSeconds, float const systemDeltaSeconds) const
{
    UNUSED(gameDeltaSeconds)
    m_player->Update(systemDeltaSeconds);
}

void Game::UpdateWorld(float const gameDeltaSeconds)
{
    if (m_world == nullptr) return;
    m_world->Update(gameDeltaSeconds);
}

//----------------------------------------------------------------------------------------------------
void Game::RenderAttractMode() const
{
    Vec2 clientDimensions = Window::s_mainWindow->GetClientDimensions();

    VertexList_PCU verts;
    AddVertsForDisc2D(verts, Vec2(clientDimensions.x * 0.5f, clientDimensions.y * 0.5f), 300.f, 10.f, Rgba8::YELLOW);
    g_renderer->SetModelConstants();
    g_renderer->SetBlendMode(eBlendMode::OPAQUE);
    g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
    g_renderer->SetSamplerMode(eSamplerMode::BILINEAR_CLAMP);
    g_renderer->SetDepthMode(eDepthMode::DISABLED);
    g_renderer->BindTexture(nullptr);
    g_renderer->BindShader(g_renderer->CreateOrGetShaderFromFile("Data/Shaders/Default"));
    g_renderer->DrawVertexArray(verts);
}

//----------------------------------------------------------------------------------------------------
void Game::RenderEntities() const
{
    // g_renderer->SetModelConstants(m_player->GetModelToWorldTransform());
    m_player->Render();
}

void Game::RenderPlayerBasis() const
{
    VertexList_PCU verts;

    Vec3 const worldCameraPosition = m_player->GetCamera()->GetPosition();
    Vec3 const forwardNormal       = m_player->GetCamera()->GetOrientation().GetAsMatrix_IFwd_JLeft_KUp().GetIBasis3D().GetNormalized();

    // Add vertices in world space.
    AddVertsForArrow3D(verts, worldCameraPosition + forwardNormal, worldCameraPosition + forwardNormal + Vec3::X_BASIS * 0.1f, 0.8f, 0.001f, 0.003f, Rgba8::RED);
    AddVertsForArrow3D(verts, worldCameraPosition + forwardNormal, worldCameraPosition + forwardNormal + Vec3::Y_BASIS * 0.1f, 0.8f, 0.001f, 0.003f, Rgba8::GREEN);
    AddVertsForArrow3D(verts, worldCameraPosition + forwardNormal, worldCameraPosition + forwardNormal + Vec3::Z_BASIS * 0.1f, 0.8f, 0.001f, 0.003f, Rgba8::BLUE);

    g_renderer->SetModelConstants();
    g_renderer->SetBlendMode(eBlendMode::OPAQUE);
    g_renderer->SetRasterizerMode(eRasterizerMode::SOLID_CULL_BACK);
    g_renderer->SetSamplerMode(eSamplerMode::POINT_CLAMP);
    g_renderer->SetDepthMode(eDepthMode::DISABLED);
    g_renderer->BindTexture(nullptr);
    g_renderer->DrawVertexArray(static_cast<int>(verts.size()), verts.data());
}

//----------------------------------------------------------------------------------------------------
void Game::SpawnPlayer()
{
    m_player = new Player(this);
}

//----------------------------------------------------------------------------------------------------
Vec3 Game::GetPlayerCameraPosition() const
{
    if (m_player && m_player->GetCamera())
    {
        return m_player->GetCamera()->GetPosition();
    }

    return Vec3::ZERO;
}

//----------------------------------------------------------------------------------------------------
void Game::ShowSimpleDemoWindow()
{
    ImGui::Begin("ImGui Demo Window", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

    if (ImGui::CollapsingHeader("Basic Widgets"))
    {
        // Buttons
        if (ImGui::Button("Button"))
        {
            // Handle button click
        }
        ImGui::SameLine();
        if (ImGui::Button("Another Button"))
        {
            // Handle button click
        }

        // Checkbox
        static bool checkbox_val = false;
        ImGui::Checkbox("Enable Feature", &checkbox_val);

        // Radio buttons
        static int radio_option = 0;
        ImGui::RadioButton("Option A", &radio_option, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Option B", &radio_option, 1);
        ImGui::SameLine();
        ImGui::RadioButton("Option C", &radio_option, 2);
    }

    if (ImGui::CollapsingHeader("Sliders"))
    {
        // Float slider
        static float float_val = 0.0f;
        ImGui::SliderFloat("Float Slider", &float_val, 0.0f, 1.0f);

        // Int slider
        static int int_val = 0;
        ImGui::SliderInt("Int Slider", &int_val, 0, 100);

        // Range slider
        static float range_val = 0.0f;
        ImGui::SliderFloat("Range Slider", &range_val, -10.0f, 10.0f);
    }

    if (ImGui::CollapsingHeader("Color Controls"))
    {
        // Color picker
        static float color[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
        ImGui::ColorEdit3("Color", color);

        // Color button
        static ImVec4 color_button = ImVec4(1.0f, 0.0f, 0.0f, 1.0f);
        if (ImGui::ColorButton("Color Button", color_button))
        {
            // Handle color selection
        }
    }

    if (ImGui::CollapsingHeader("Text Input"))
    {
        // Text input
        static char text_buf[256] = "Hello, ImGui!";
        ImGui::InputText("Text Input", text_buf, sizeof(text_buf));

        // Multiline text
        static char text_multiline[1024] = "This is a\nmultiline\n text area.";
        ImGui::InputTextMultiline("Multiline", text_multiline, sizeof(text_multiline));
    }

    if (ImGui::CollapsingHeader("Progress Bars"))
    {
        // Progress bar
        static float progress = 0.0f;
        ImGui::ProgressBar(progress, ImVec2(200, 0));
        ImGui::SameLine();
        ImGui::Text("Progress: %.0f%%", progress * 100);

        if (ImGui::Button("Add 25%"))
        {
            progress = (std::min)(progress + 0.25f, 1.0f);
        }
        ImGui::SameLine();
        if (ImGui::Button("Reset"))
        {
            progress = 0.0f;
        }
    }

    if (ImGui::CollapsingHeader("Simple Tree"))
    {
        if (ImGui::TreeNode("Root Node"))
        {
            if (ImGui::TreeNode("Child 1"))
            {
                ImGui::Text("Leaf content 1");
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Child 2"))
            {
                ImGui::Text("Leaf content 2");
                if (ImGui::TreeNode("Sub-child"))
                {
                    ImGui::Text("Sub-leaf content");
                    ImGui::TreePop();
                }
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }

    // Advanced Input Widgets
    if (ImGui::CollapsingHeader("Advanced Input"))
    {
        static float drag_float = 0.0f;
        ImGui::DragFloat("Drag Float", &drag_float, 0.01f, 0.0f, 100.0f);
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Drag to adjust value");

        static float input_float = 0.0f;
        ImGui::InputFloat("Input Float", &input_float);

        static int input_int = 0;
        ImGui::InputInt("Input Int", &input_int);

        static float angle = 0.0f;
        ImGui::SliderAngle("Rotation", &angle);

        static float vec3[3] = { 0.0f, 0.0f, 0.0f };
        ImGui::DragFloat3("Position", vec3, 0.1f);
    }

    // Tables & Data Display
    if (ImGui::CollapsingHeader("Tables"))
    {
        if (ImGui::BeginTable("DemoTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Type");
            ImGui::TableSetupColumn("Value");
            ImGui::TableHeadersRow();

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Position");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Vec3");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("(10.0, 5.0, 2.0)");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Health");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Int");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("100");

            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("Speed");
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("Float");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("5.5");

            ImGui::EndTable();
        }
    }

    // Tabs
    if (ImGui::CollapsingHeader("Tabs"))
    {
        if (ImGui::BeginTabBar("DemoTabs"))
        {
            if (ImGui::BeginTabItem("Tab 1"))
            {
                ImGui::Text("This is Tab 1 content");
                ImGui::BulletText("Feature A");
                ImGui::BulletText("Feature B");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 2"))
            {
                ImGui::Text("This is Tab 2 content");
                ImGui::BulletText("Setting X");
                ImGui::BulletText("Setting Y");
                ImGui::EndTabItem();
            }
            if (ImGui::BeginTabItem("Tab 3"))
            {
                ImGui::Text("This is Tab 3 content");
                static bool option_enabled = false;
                ImGui::Checkbox("Enable Option", &option_enabled);
                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }

    // Child Windows & Scrolling
    if (ImGui::CollapsingHeader("Child Windows"))
    {
        ImGui::Text("Scrollable child region:");
        ImGui::BeginChild("ChildRegion", ImVec2(0, 100), true, ImGuiWindowFlags_HorizontalScrollbar);
        for (int i = 0; i < 20; i++)
        {
            ImGui::Text("Line %d - This is a scrollable content area", i);
        }
        ImGui::EndChild();
    }

    // Popups & Modals
    if (ImGui::CollapsingHeader("Popups & Modals"))
    {
        if (ImGui::Button("Open Modal"))
        {
            ImGui::OpenPopup("DemoModal");
        }

        if (ImGui::BeginPopupModal("DemoModal", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("This is a modal dialog");
            ImGui::Separator();
            ImGui::Text("Click OK to close");

            if (ImGui::Button("OK", ImVec2(120, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::SameLine();
        if (ImGui::Button("Right-Click Menu"))
        {
            ImGui::OpenPopup("ContextMenu");
        }

        if (ImGui::BeginPopup("ContextMenu"))
        {
            if (ImGui::Selectable("Option 1")) { /* Handle option 1 */ }
            if (ImGui::Selectable("Option 2")) { /* Handle option 2 */ }
            if (ImGui::Selectable("Option 3")) { /* Handle option 3 */ }
            ImGui::EndPopup();
        }
    }

    // Plotting
    if (ImGui::CollapsingHeader("Plotting"))
    {
        static float values[90] = {};
        static int values_offset = 0;
        static float refresh_time = 0.0f;

        // Generate sine wave data
        if (refresh_time == 0.0f)
        {
            for (int i = 0; i < 90; i++)
            {
                values[i] = sinf(i * 0.1f);
            }
        }
        refresh_time += 0.016f;

        ImGui::PlotLines("Sine Wave", values, 90, values_offset, nullptr, -1.0f, 1.0f, ImVec2(0, 80));

        static float histogram[10] = { 0.1f, 0.3f, 0.5f, 0.7f, 0.9f, 0.7f, 0.5f, 0.3f, 0.2f, 0.1f };
        ImGui::PlotHistogram("Histogram", histogram, 10, 0, nullptr, 0.0f, 1.0f, ImVec2(0, 80));
    }

    ImGui::End();
}

//----------------------------------------------------------------------------------------------------
void Game::ShowInspectorWindow()
{
    ImGui::Begin("Inspector", &m_showInspectorWindow);

    // GameObject Header
    ImGui::Text("GameObject: Player");
    ImGui::Separator();

    // Transform Component (always present)
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        static float position[3] = { 10.0f, 5.0f, 2.0f };
        static float rotation[3] = { 0.0f, 45.0f, 0.0f };
        static float scale[3] = { 1.0f, 1.0f, 1.0f };

        ImGui::DragFloat3("Position", position, 0.1f);
        ImGui::DragFloat3("Rotation", rotation, 1.0f, -180.0f, 180.0f);
        ImGui::DragFloat3("Scale", scale, 0.01f, 0.01f, 10.0f);

        if (ImGui::Button("Reset Transform"))
        {
            position[0] = position[1] = position[2] = 0.0f;
            rotation[0] = rotation[1] = rotation[2] = 0.0f;
            scale[0] = scale[1] = scale[2] = 1.0f;
        }
    }

    // Dynamic Script Components
    static std::vector<std::string> scriptComponents = { "PlayerController", "Health", "Inventory" };
    static std::vector<bool> componentEnabled = { true, true, true };

    for (size_t i = 0; i < scriptComponents.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));

        // Component header with checkbox
        bool enabled = componentEnabled[i];
        if (ImGui::Checkbox("##enabled", &enabled))
        {
            componentEnabled[i] = enabled;
        }
        ImGui::SameLine();

        // Component name - clickable and double-clickable
        const char* componentName = scriptComponents[i].c_str();

        // Use Selectable instead of CollapsingHeader for double-click detection
        bool isSelected = false;
        if (ImGui::Selectable(componentName, &isSelected, ImGuiSelectableFlags_AllowDoubleClick))
        {
            if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            {
                // Construct script path (working directory is already Run/)
                std::string scriptPath = "Data/Scripts/" + scriptComponents[i] + ".js";
                DebuggerPrintf("Double-clicked component '%s', attempting to open: %s\n",
                              componentName, scriptPath.c_str());

                OpenFileWithSystemDefault(scriptPath.c_str());
            }
        }

        // Tooltip to show script path
        if (ImGui::IsItemHovered())
        {
            std::string scriptPath = "Data/Scripts/" + scriptComponents[i] + ".js";
            ImGui::SetTooltip("Double-click to open:\n%s", scriptPath.c_str());
        }

        // Use TreeNode for collapsible properties instead
        if (ImGui::TreeNode("Properties"))
        {
            ImGui::Indent();

            // Example properties based on component type
            if (scriptComponents[i] == "PlayerController")
            {
                static float moveSpeed = 5.5f;
                static float jumpHeight = 2.0f;
                static bool canDoubleJump = true;

                ImGui::DragFloat("Move Speed", &moveSpeed, 0.1f, 0.0f, 20.0f);
                ImGui::DragFloat("Jump Height", &jumpHeight, 0.1f, 0.0f, 10.0f);
                ImGui::Checkbox("Can Double Jump", &canDoubleJump);
            }
            else if (scriptComponents[i] == "Health")
            {
                static int maxHealth = 100;
                static int currentHealth = 85;
                static float regenRate = 1.0f;

                ImGui::DragInt("Max Health", &maxHealth, 1.0f, 1, 1000);
                ImGui::SliderInt("Current Health", &currentHealth, 0, maxHealth);
                ImGui::DragFloat("Regen Rate", &regenRate, 0.1f, 0.0f, 10.0f);

                ImGui::ProgressBar(static_cast<float>(currentHealth) / maxHealth);
            }
            else if (scriptComponents[i] == "Inventory")
            {
                static int capacity = 20;
                static int itemCount = 12;
                static bool autoSort = true;

                ImGui::DragInt("Capacity", &capacity, 1.0f, 1, 100);
                ImGui::Text("Items: %d/%d", itemCount, capacity);
                ImGui::Checkbox("Auto Sort", &autoSort);

                if (ImGui::Button("Clear Inventory"))
                {
                    itemCount = 0;
                }
            }

            ImGui::Separator();

            // Remove Component Button
            if (ImGui::Button("Remove Component"))
            {
                scriptComponents.erase(scriptComponents.begin() + i);
                componentEnabled.erase(componentEnabled.begin() + i);
                ImGui::Unindent();
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }

            ImGui::Unindent();
            ImGui::TreePop();
        }

        ImGui::PopID();
    }

    // Add Component Section
    ImGui::Separator();
    if (ImGui::Button("Add Component", ImVec2(-1, 0)))
    {
        ImGui::OpenPopup("AddComponentPopup");
    }

    // Add Component Popup Menu
    if (ImGui::BeginPopup("AddComponentPopup"))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.8f, 1.0f, 1.0f), "Available Components:");
        ImGui::Separator();

        if (ImGui::Selectable("Rigidbody"))
        {
            scriptComponents.push_back("Rigidbody");
            componentEnabled.push_back(true);
        }
        if (ImGui::Selectable("Collider"))
        {
            scriptComponents.push_back("Collider");
            componentEnabled.push_back(true);
        }
        if (ImGui::Selectable("AudioSource"))
        {
            scriptComponents.push_back("AudioSource");
            componentEnabled.push_back(true);
        }
        if (ImGui::Selectable("Light"))
        {
            scriptComponents.push_back("Light");
            componentEnabled.push_back(true);
        }
        if (ImGui::Selectable("Camera"))
        {
            scriptComponents.push_back("Camera");
            componentEnabled.push_back(true);
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}
//----------------------------------------------------------------------------------------------------
void Game::ShowDebugLogWindow()
{
    // Log entry structure
    struct LogEntry
    {
        std::string message;
        int severity; // 0=Info, 1=Warning, 2=Error
        std::string timestamp;
    };

    // Static log storage
    static std::vector<LogEntry> logs;
    static bool initialized = false;
    static int logCounter = 0;

    // Initialize with some dummy logs
    if (!initialized)
    {
        logs.push_back({ "Application started successfully", 0, "10:30:01" });
        logs.push_back({ "Loading resources...", 0, "10:30:02" });
        logs.push_back({ "Texture 'player.png' loaded", 0, "10:30:03" });
        logs.push_back({ "Audio file 'bgm.ogg' not found", 1, "10:30:04" });
        logs.push_back({ "World generation complete", 0, "10:30:05" });
        logs.push_back({ "Player spawned at (0, 0, 64)", 0, "10:30:06" });
        logs.push_back({ "Warning: Low memory detected", 1, "10:30:10" });
        logs.push_back({ "Chunk (0, 0) generated", 0, "10:30:11" });
        logs.push_back({ "Chunk (1, 0) generated", 0, "10:30:12" });
        logs.push_back({ "Error: Failed to load shader 'water.hlsl'", 2, "10:30:15" });
        logs.push_back({ "Network connection established", 0, "10:30:20" });
        logCounter = static_cast<int>(logs.size());
        initialized = true;
    }

    ImGui::Begin("Console", &m_showConsoleWindow);

    // Toolbar
    ImGui::Text("Debug Console");
    ImGui::SameLine(ImGui::GetWindowWidth() - 180);

    if (ImGui::SmallButton("Clear"))
    {
        logs.clear();
        logCounter = 0;
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Add Info"))
    {
        char buffer[64];
        sprintf_s(buffer, "Info message #%d", ++logCounter);
        logs.push_back({ buffer, 0, "10:35:00" });
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Add Warning"))
    {
        char buffer[64];
        sprintf_s(buffer, "Warning message #%d", ++logCounter);
        logs.push_back({ buffer, 1, "10:35:00" });
    }

    ImGui::SameLine();
    if (ImGui::SmallButton("Add Error"))
    {
        char buffer[64];
        sprintf_s(buffer, "Error message #%d", ++logCounter);
        logs.push_back({ buffer, 2, "10:35:00" });
    }

    ImGui::Separator();

    // Filter controls
    static bool showInfo = true;
    static bool showWarnings = true;
    static bool showErrors = true;
    static char filterText[256] = "";

    ImGui::Checkbox("Info", &showInfo);
    ImGui::SameLine();
    ImGui::Checkbox("Warnings", &showWarnings);
    ImGui::SameLine();
    ImGui::Checkbox("Errors", &showErrors);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputText("Filter", filterText, sizeof(filterText));

    ImGui::Separator();

    // Statistics
    int infoCount = 0, warningCount = 0, errorCount = 0;
    for (const auto& log : logs)
    {
        if (log.severity == 0) infoCount++;
        else if (log.severity == 1) warningCount++;
        else if (log.severity == 2) errorCount++;
    }
    ImGui::Text("Total: %d | Info: %d | Warnings: %d | Errors: %d",
                static_cast<int>(logs.size()), infoCount, warningCount, errorCount);

    ImGui::Separator();

    // Log display with auto-scroll
    static bool autoScroll = true;
    ImGui::Checkbox("Auto-scroll", &autoScroll);

    ImGui::BeginChild("ScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    for (const auto& log : logs)
    {
        // Filter by severity
        if (log.severity == 0 && !showInfo) continue;
        if (log.severity == 1 && !showWarnings) continue;
        if (log.severity == 2 && !showErrors) continue;

        // Filter by text
        if (filterText[0] != '\0')
        {
            if (log.message.find(filterText) == std::string::npos)
                continue;
        }

        // Color based on severity
        ImVec4 color;
        const char* prefix;
        if (log.severity == 0)
        {
            color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f); // Light gray for info
            prefix = "[INFO]";
        }
        else if (log.severity == 1)
        {
            color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); // Yellow for warning
            prefix = "[WARN]";
        }
        else
        {
            color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); // Red for error
            prefix = "[ERROR]";
        }

        // Display log line
        ImGui::TextColored(color, "[%s] %s %s", log.timestamp.c_str(), prefix, log.message.c_str());
    }

    // Auto-scroll to bottom
    if (autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}
