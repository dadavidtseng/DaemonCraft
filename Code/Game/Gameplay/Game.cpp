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
#include "Game/Framework/WorldGenConfig.hpp"
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
#include <cmath>
#include <vector>
#include <string>
//----------------------------------------------------------------------------------------------------
#include <Windows.h>
#include <shellapi.h>

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

    // CRITICAL FIX: Process input BEFORE world update to ensure dirty chunks are processed same frame
    // This fixes the flashing bug by eliminating the 2-3 frame delay between block modification and mesh rebuild
    UpdateFromInput();

    // CRITICAL: Check if F8 was pressed requesting game restart
    // If true, stop executing immediately to prevent use-after-delete crash
    // App will check RequestedNewGame() and call DeleteAndCreateNewGame() safely
    if (m_requestNewGame)
    {
        return;  // Do NOT execute any more code on this Game instance
    }

    // Update world AFTER input processing so block modifications trigger mesh rebuilds this frame
    UpdateWorld(gameDeltaSeconds);


    ShowTerrainDebugWindow();
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
        // Phase 1, Task 1.1: Updated block indices to match new Assignment 4 XML
        if (g_input->WasKeyJustPressed(NUMCODE_1) || g_input->WasKeyJustPressed(KEYCODE_UPARROW))
        {
            // Cycle to next block type: Glowstone(13) -> Cobblestone(14) -> ChiseledBrick(15) -> Glowstone(13)
            m_currentBlockType++;
            if (m_currentBlockType > 15) // BLOCK_CHISELED_BRICK
            {
                m_currentBlockType = 13; // Back to BLOCK_GLOWSTONE
            }
            DebuggerPrintf("Current block type: %d\n", m_currentBlockType);
        }

        if (g_input->WasKeyJustPressed(NUMCODE_2) || g_input->WasKeyJustPressed(KEYCODE_DOWNARROW))
        {
            // Cycle to previous block type: ChiseledBrick(15) -> Cobblestone(14) -> Glowstone(13) -> ChiseledBrick(15)
            m_currentBlockType--;
            if (m_currentBlockType < 13) // BLOCK_GLOWSTONE
            {
                m_currentBlockType = 15; // Back to BLOCK_CHISELED_BRICK
            }
            DebuggerPrintf("Current block type: %d\n", m_currentBlockType);
        }

        if (g_input->WasKeyJustPressed(KEYCODE_F8))
        {
            // Request game restart - flag will be checked by App after Update() completes
            // This prevents use-after-delete crash from calling DeleteAndCreateNewGame() mid-update
            m_requestNewGame = true;
            return;  // Stop processing input to avoid accessing deleted object
        }

#if defined GAME_DEBUG_MODE
        if (m_showDebugInfo)
        {
            DebugAddScreenText(Stringf("Player Position: (%.2f, %.2f, %.2f)", m_player->m_position.x, m_player->m_position.y, m_player->m_position.z), Vec2(0.f, 120.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

            // Add debug info for current block type and coordinates
            // Phase 1, Task 1.1: Updated block type names and indices for Assignment 4
            DebugAddScreenText(Stringf("Current Block Type: [%d] - Glowstone[13] Cobblestone[14] ChiseledBrick[15]", m_currentBlockType), Vec2(0.f, 140.f), 20.f, Vec2::ZERO, 0.f, Rgba8::WHITE, Rgba8::WHITE);

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
            // Request game restart - same as F8 keyboard handler
            m_requestNewGame = true;
            return;  // Stop processing input
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
Vec3 Game::GetPlayerVelocity() const
{
    if (m_player)
    {
        return m_player->GetVelocity();
    }

    return Vec3::ZERO;
}

//----------------------------------------------------------------------------------------------------
void Game::ShowTerrainDebugWindow()
{
	if (!g_worldGenConfig)
	{
		return; // Config not initialized
	}

	if (!ImGui::Begin("Terrain Generation Debug", nullptr, ImGuiWindowFlags_MenuBar))
	{
		ImGui::End();
		return;
	}

	// Menu bar
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("Save Config"))
			{
				g_worldGenConfig->SaveToXML("Data/GameConfig.xml");
			}
			if (ImGui::MenuItem("Load Config"))
			{
				g_worldGenConfig->LoadFromXML("Data/GameConfig.xml");
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Reset to Defaults"))
			{
				g_worldGenConfig->ResetToDefaults();
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
        {
            if (ImGui::MenuItem("Regenerate Chunks"))
            {
                // Trigger chunk regeneration - forces fresh procedural terrain generation
                // This is critical for rapid iteration when tuning world generation parameters
                if (m_world != nullptr)
                {
                    m_world->RegenerateAllChunks();
                }
            }

            ImGui::Separator();

            // Phase 0, Task 0.4: Debug Visualization Mode Selection
            if (ImGui::BeginMenu("Debug Visualization"))
            {
                if (m_world != nullptr)
                {
                    DebugVisualizationMode currentMode = m_world->GetDebugVisualizationMode();

                    if (ImGui::MenuItem("Normal Terrain", nullptr, currentMode == DebugVisualizationMode::NORMAL_TERRAIN))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::NORMAL_TERRAIN);
                    }

                    ImGui::Separator();
                    ImGui::Text("Noise Layers:");
                    ImGui::Separator();

                    if (ImGui::MenuItem("Temperature", nullptr, currentMode == DebugVisualizationMode::TEMPERATURE))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::TEMPERATURE);
                    }

                    if (ImGui::MenuItem("Humidity", nullptr, currentMode == DebugVisualizationMode::HUMIDITY))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::HUMIDITY);
                    }

                    if (ImGui::MenuItem("Continentalness", nullptr, currentMode == DebugVisualizationMode::CONTINENTALNESS))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::CONTINENTALNESS);
                    }

                    if (ImGui::MenuItem("Erosion", nullptr, currentMode == DebugVisualizationMode::EROSION))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::EROSION);
                    }

                    if (ImGui::MenuItem("Weirdness", nullptr, currentMode == DebugVisualizationMode::WEIRDNESS))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::WEIRDNESS);
                    }

                    if (ImGui::MenuItem("Peaks & Valleys", nullptr, currentMode == DebugVisualizationMode::PEAKS_VALLEYS))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::PEAKS_VALLEYS);
                    }

                    if (ImGui::MenuItem("Biome Type", nullptr, currentMode == DebugVisualizationMode::BIOME_TYPE))
                    {
                        m_world->SetDebugVisualizationMode(DebugVisualizationMode::BIOME_TYPE);
                    }
                }
                ImGui::EndMenu();
            }

            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }

    // Tab system for different debug categories
    if (ImGui::BeginTabBar("TerrainDebugTabs"))
    {
        if (ImGui::BeginTabItem("Curves"))
        {
            ShowCurvesTab();
            ImGui::EndTabItem();
        }

        // Tab 2: Biome Noise Parameters
        if (ImGui::BeginTabItem("Biome Noise"))
        {
            ShowBiomeNoiseTab();
            ImGui::EndTabItem();
        }

        // Tab 3: 3D Density Terrain
        if (ImGui::BeginTabItem("Density"))
        {
            ShowDensityTab();
            ImGui::EndTabItem();
        }

        // Tab 4: Cave Parameters
        if (ImGui::BeginTabItem("Caves"))
        {
            ShowCavesTab();
            ImGui::EndTabItem();
        }

        // Tab 5: Trees
        if (ImGui::BeginTabItem("Trees"))
        {
            ShowTreesTab();
            ImGui::EndTabItem();
        }

        // Tab 6: Carvers (Ravines & Rivers)
        if (ImGui::BeginTabItem("Carvers"))
        {
            ShowCarversTab();
            ImGui::EndTabItem();
        }

        // Tab 5: Visualization
        if (ImGui::BeginTabItem("Visualization"))
        {
            ImGui::Text("Noise Layer Visualization");
            ImGui::Separator();

            // Toggle individual noise layers
            static bool showTemperature = true;
            static bool showHumidity = true;
            static bool showContinentalness = true;
            static bool showErosion = true;
            static bool showWeirdness = true;
            static bool showPeaksValleys = true;

            ImGui::Text("Toggle Noise Layers:");
            ImGui::Checkbox("Temperature", &showTemperature);
            ImGui::SameLine();
            ImGui::Checkbox("Humidity", &showHumidity);
            ImGui::SameLine();
            ImGui::Checkbox("Continentalness", &showContinentalness);

            ImGui::Checkbox("Erosion", &showErosion);
            ImGui::SameLine();
            ImGui::Checkbox("Weirdness", &showWeirdness);
            ImGui::SameLine();
            ImGui::Checkbox("Peaks & Valleys", &showPeaksValleys);

            ImGui::Separator();

            // Visualization options
            static bool showBiomeOverlay = true;
            static bool showChunkBoundaries = false;
            static bool showHeatmapColors = true;
            static float visualizationScale = 1.0f;

            ImGui::Text("Visualization Options:");
            ImGui::Checkbox("Biome Overlay", &showBiomeOverlay);
            ImGui::Checkbox("Chunk Boundaries", &showChunkBoundaries);
            ImGui::Checkbox("Heatmap Colors", &showHeatmapColors);
            ImGui::SliderFloat("Display Scale", &visualizationScale, 0.1f, 2.0f, "%.1fx");

            ImGui::Separator();

            if (ImGui::Button("Generate Visualization"))
            {
                // Generate noise layer visualization
            }

            ImGui::SameLine();

            if (ImGui::Button("Clear Visualization"))
            {
                // Clear visualization
            }

            // Simple visualization area placeholder
            ImGui::Text("");
            ImGui::Text("Visualization will appear here:");
            ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[Heatmap/Noise visualization placeholder]");

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

//----------------------------------------------------------------------------------------------------
// GetGraphInteraction - Transform mouse position to graph coordinates
//----------------------------------------------------------------------------------------------------
Game::GraphInteraction Game::GetGraphInteraction(ImVec2 graphMin, ImVec2 graphSize, float minValue, float maxValue)
{
    GraphInteraction result;
    result.mouseScreenPos = ImGui::GetMousePos();

    // Check if mouse is in graph bounds
    ImVec2 graphMax(graphMin.x + graphSize.x, graphMin.y + graphSize.y);
    result.isMouseInGraph = (result.mouseScreenPos.x >= graphMin.x && result.mouseScreenPos.x <= graphMax.x &&
                             result.mouseScreenPos.y >= graphMin.y && result.mouseScreenPos.y <= graphMax.y);

    if (result.isMouseInGraph)
    {
        // Transform screen coordinates to graph coordinates
        float normalizedX = (result.mouseScreenPos.x - graphMin.x) / graphSize.x;  // [0, 1]
        float normalizedY = (graphMax.y - result.mouseScreenPos.y) / graphSize.y;  // [0, 1] (flipped Y)

        result.mouseT = normalizedX * 2.0f - 1.0f;  // [0, 1] → [-1, 1]
        result.mouseValue = minValue + normalizedY * (maxValue - minValue);
    }
    else
    {
        result.mouseT = 0.0f;
        result.mouseValue = 0.0f;
    }

    return result;
}

//----------------------------------------------------------------------------------------------------
// FindHoveredPoint - Hit-test control points, return index or -1
//----------------------------------------------------------------------------------------------------
int Game::FindHoveredPoint(PiecewiseCurve1D const& curve, GraphInteraction const& interaction,
                           ImVec2 graphMin, ImVec2 graphSize, float minValue, float maxValue)
{
    if (!interaction.isMouseInGraph)
        return -1;

    float const HOVER_THRESHOLD = 10.0f;  // pixels
    int numPoints = curve.GetNumPoints();
    ImVec2 graphMax(graphMin.x + graphSize.x, graphMin.y + graphSize.y);

    for (int i = 0; i < numPoints; ++i)
    {
        PiecewiseCurve1D::ControlPoint pt = curve.GetPoint(i);

        // Transform point to screen space
        float normalizedX = (pt.t + 1.0f) / 2.0f;  // [-1, 1] → [0, 1]
        float normalizedY = (pt.value - minValue) / (maxValue - minValue);  // [0, 1]

        float screenX = graphMin.x + normalizedX * graphSize.x;
        float screenY = graphMax.y - normalizedY * graphSize.y;  // Flipped Y

        // Calculate distance to mouse
        float dx = interaction.mouseScreenPos.x - screenX;
        float dy = interaction.mouseScreenPos.y - screenY;
        float distance = sqrtf(dx * dx + dy * dy);

        if (distance <= HOVER_THRESHOLD)
            return i;
    }

    return -1;
}

//----------------------------------------------------------------------------------------------------
// ShowCurveEditor - Interactive ImGui curve editor for PiecewiseCurve1D
// Assignment 4: Phase 5B.4 - Curve editor with mouse interaction
//----------------------------------------------------------------------------------------------------
void Game::ShowCurveEditor(char const* label, PiecewiseCurve1D& curve, float minValue, float maxValue)
{
    ImGui::Text("%s", label);

    // Get or create editor state for this curve
    CurveEditorState& state = m_curveEditorStates[label];

    // Display current control points
    int numPoints = curve.GetNumPoints();
    ImGui::Text("Control Points: %d", numPoints);

    // Plot preview - Calculate graph area and create interactive canvas
    ImVec2 const graphSize(300, 150);

    // Get cursor position BEFORE creating the button
    ImVec2 const cursorPos = ImGui::GetCursorScreenPos();

    // Create invisible button with unique ID per curve to capture mouse input (prevents window dragging)
    char buttonID[64];
    snprintf(buttonID, sizeof(buttonID), "##graph_canvas_%s", label);
    ImGui::InvisibleButton(buttonID, graphSize);
    bool isGraphHovered = ImGui::IsItemHovered();
    bool isGraphActive = ImGui::IsItemActive();

    // Use the cursor position we saved BEFORE the button was created
    ImVec2 const graphMin = cursorPos;
    ImVec2 const graphMax(graphMin.x + graphSize.x, graphMin.y + graphSize.y);

    // Get mouse interaction state
    GraphInteraction interaction = GetGraphInteraction(graphMin, graphSize, minValue, maxValue);

    // Draw graph background using DrawList at the button's actual position
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Change background color based on hover/active state for visual feedback
    ImU32 bgColor = IM_COL32(50, 50, 50, 255);      // Default: dark gray
    ImU32 borderColor = IM_COL32(100, 100, 100, 255); // Default: gray

    if (isGraphActive)
    {
        bgColor = IM_COL32(60, 60, 80, 255);        // Active: slightly blue tint
        borderColor = IM_COL32(150, 150, 200, 255);  // Active: bright blue border
    }
    else if (isGraphHovered)
    {
        bgColor = IM_COL32(55, 55, 55, 255);        // Hovered: slightly lighter
        borderColor = IM_COL32(120, 120, 150, 255);  // Hovered: lighter blue border
    }

    drawList->AddRectFilled(graphMin, graphMax, bgColor);
    drawList->AddRect(graphMin, graphMax, borderColor);

    // Draw curve
    if (numPoints >= 2)
    {
        for (int i = 0; i < 100; ++i)
        {
            float t0 = -1.0f + (i / 99.0f) * 2.0f;
            float t1 = -1.0f + ((i + 1) / 99.0f) * 2.0f;

            float v0 = curve.Evaluate(t0);
            float v1 = curve.Evaluate(t1);

            // CLAMP values to range to prevent off-screen rendering
            if (v0 < minValue) v0 = minValue;
            if (v0 > maxValue) v0 = maxValue;
            if (v1 < minValue) v1 = minValue;
            if (v1 > maxValue) v1 = maxValue;

            // Map to graph space
            float x0 = graphMin.x + ((t0 + 1.0f) / 2.0f) * graphSize.x;
            float x1 = graphMin.x + ((t1 + 1.0f) / 2.0f) * graphSize.x;
            float y0 = graphMax.y - ((v0 - minValue) / (maxValue - minValue)) * graphSize.y;
            float y1 = graphMax.y - ((v1 - minValue) / (maxValue - minValue)) * graphSize.y;

            drawList->AddLine(ImVec2(x0, y0), ImVec2(x1, y1), IM_COL32(0, 255, 0, 255), 2.0f);
        }
    }

    // Find hovered point
    state.hoveredPointIndex = FindHoveredPoint(curve, interaction, graphMin, graphSize, minValue, maxValue);

    // Handle drag initiation
    if (state.hoveredPointIndex >= 0 && ImGui::IsMouseClicked(0))
    {
        state.isDragging = true;
        state.draggedPointIndex = state.hoveredPointIndex;
    }

    // Handle dragging
    if (state.isDragging && ImGui::IsMouseDown(0))
    {
        if (interaction.isMouseInGraph)
        {
            float newT = interaction.mouseT;
            float newValue = interaction.mouseValue;

            // Clamp to graph bounds
            newT = (newT < -1.0f) ? -1.0f : (newT > 1.0f ? 1.0f : newT);
            newValue = (newValue < minValue) ? minValue : (newValue > maxValue ? maxValue : newValue);

            // Clamp to neighboring points (maintain sorted order)
            if (state.draggedPointIndex > 0)
            {
                float prevT = curve.GetPoint(state.draggedPointIndex - 1).t;
                if (newT <= prevT) newT = prevT + 0.001f;
            }
            if (state.draggedPointIndex < numPoints - 1)
            {
                float nextT = curve.GetPoint(state.draggedPointIndex + 1).t;
                if (newT >= nextT) newT = nextT - 0.001f;
            }

            curve.SetPoint(state.draggedPointIndex, newT, newValue);
        }
    }

    // Handle drag release
    if (state.isDragging && ImGui::IsMouseReleased(0))
    {
        state.isDragging = false;
        state.draggedPointIndex = -1;
    }

    // Double-click to add point
    if (interaction.isMouseInGraph && state.hoveredPointIndex == -1 && ImGui::IsMouseDoubleClicked(0))
    {
        float newT = interaction.mouseT;
        float newValue = curve.Evaluate(newT);  // Snap to existing curve
        curve.AddPoint(newT, newValue);
    }

    // Right-click to remove point
    if (state.hoveredPointIndex >= 0 && ImGui::IsMouseClicked(1) && numPoints > 2)
    {
        curve.RemovePoint(state.hoveredPointIndex);
        state.hoveredPointIndex = -1;  // Clear hover after removal
    }

    // Draw control points
    for (int i = 0; i < numPoints; ++i)
    {
        PiecewiseCurve1D::ControlPoint pt = curve.GetPoint(i);
        float x = graphMin.x + ((pt.t + 1.0f) / 2.0f) * graphSize.x;

        // CLAMP value to range to prevent off-screen rendering
        float clampedValue = pt.value;
        if (clampedValue < minValue) clampedValue = minValue;
        if (clampedValue > maxValue) clampedValue = maxValue;

        float y = graphMax.y - ((clampedValue - minValue) / (maxValue - minValue)) * graphSize.y;

        // Determine visual state
        bool isHovered = (i == state.hoveredPointIndex);
        bool isDragged = (i == state.draggedPointIndex && state.isDragging);

        if (isDragged)
        {
            // Dragging state: bright white, larger
            drawList->AddCircleFilled(ImVec2(x, y), 8.0f, IM_COL32(255, 255, 255, 255));
        }
        else if (isHovered)
        {
            // Hover state: white outline, slightly larger
            drawList->AddCircleFilled(ImVec2(x, y), 7.0f, IM_COL32(255, 255, 255, 200));
        }
        else
        {
            // Normal state: yellow
            drawList->AddCircleFilled(ImVec2(x, y), 5.0f, IM_COL32(255, 255, 0, 255));
        }
    }

    // Cursor changes
    if (state.hoveredPointIndex >= 0)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    else if (interaction.isMouseInGraph)
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);  // Use Arrow instead of Crosshair (not available in this ImGui version)
    }

    // Tooltip
    if (state.hoveredPointIndex >= 0)
    {
        PiecewiseCurve1D::ControlPoint pt = curve.GetPoint(state.hoveredPointIndex);
        ImGui::SetTooltip("Point %d\nt: %.3f\nValue: %.2f", state.hoveredPointIndex, pt.t, pt.value);
    }

    // Note: InvisibleButton already advances cursor, so no need for ImGui::Dummy

    // Optional: Keep text-based editing for precision
    ImGui::Text("Text Editing (for precision):");
    for (int i = 0; i < numPoints; ++i)
    {
        ImGui::PushID(i);

        PiecewiseCurve1D::ControlPoint pt = curve.GetPoint(i);
        float t = pt.t;
        float value = pt.value;

        ImGui::Text("Point %d:", i);
        ImGui::SameLine();

        bool changed = false;
        if (ImGui::DragFloat("T", &t, 0.01f, -1.0f, 1.0f))
        {
            changed = true;
        }
        ImGui::SameLine();
        if (ImGui::DragFloat("Value", &value, 0.01f, minValue, maxValue))
        {
            changed = true;
        }

        if (changed)
        {
            curve.SetPoint(i, t, value);
        }

        ImGui::SameLine();
        if (ImGui::Button("Remove") && numPoints > 2)
        {
            curve.RemovePoint(i);
            numPoints = curve.GetNumPoints();
        }

        ImGui::PopID();
    }

    // Add new point button
    if (ImGui::Button("Add Point"))
    {
        curve.AddPoint(0.0f, (minValue + maxValue) * 0.5f);
    }
}

//----------------------------------------------------------------------------------------------------
// ShowCurvesTab - ImGui tab for terrain shaping curves
// Assignment 4: Phase 5B.4 - Continentalness, Erosion, and Peaks & Valleys curves
//----------------------------------------------------------------------------------------------------
void Game::ShowCurvesTab()
{
    ImGui::Text("Terrain Shaping Curves");
    ImGui::Separator();

    // Continentalness curve
    if (ImGui::CollapsingHeader("Continentalness Curve", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID("ContinentalnessCurve");  // Unique ID scope for this curve

        ImGui::Text("Maps continentalness noise to height offset");
        ImGui::Text("Range: [%.1f, %.1f]", g_worldGenConfig->curves.continentalnessHeightMin,
                    g_worldGenConfig->curves.continentalnessHeightMax);

        ShowCurveEditor("Continentalness", g_worldGenConfig->continentalnessCurve,
                        g_worldGenConfig->curves.continentalnessHeightMin,
                        g_worldGenConfig->curves.continentalnessHeightMax);

        ImGui::DragFloat("Height Min", &g_worldGenConfig->curves.continentalnessHeightMin, 1.0f, -50.0f, 0.0f);
        ImGui::DragFloat("Height Max", &g_worldGenConfig->curves.continentalnessHeightMax, 1.0f, 0.0f, 100.0f);

        ImGui::PopID();
    }

    // Erosion curve - FORCE OPEN TO TEST
    if (ImGui::CollapsingHeader("Erosion Curve", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::PushID("ErosionCurve");  // Unique ID scope for this curve

        ImGui::Text("Maps erosion noise to terrain scale multiplier");
        ImGui::Text("Range: [%.1f, %.1f]", g_worldGenConfig->curves.erosionScaleMin,
                    g_worldGenConfig->curves.erosionScaleMax);

        ShowCurveEditor("Erosion", g_worldGenConfig->erosionCurve,
                        g_worldGenConfig->curves.erosionScaleMin,
                        g_worldGenConfig->curves.erosionScaleMax);

        ImGui::DragFloat("Scale Min", &g_worldGenConfig->curves.erosionScaleMin, 0.1f, 0.0f, 1.0f);
        ImGui::DragFloat("Scale Max", &g_worldGenConfig->curves.erosionScaleMax, 0.1f, 1.0f, 5.0f);

        ImGui::PopID();
    }

    // Peaks & Valleys curve
    if (ImGui::CollapsingHeader("Peaks & Valleys Curve"))
    {
        ImGui::PushID("PeaksValleysCurve");  // Unique ID scope for this curve

        ImGui::Text("Maps peaks/valleys noise to height modifier");
        ImGui::Text("Range: [%.1f, %.1f]", g_worldGenConfig->curves.pvHeightMin,
                    g_worldGenConfig->curves.pvHeightMax);

        ShowCurveEditor("Peaks & Valleys", g_worldGenConfig->peaksValleysCurve,
                        g_worldGenConfig->curves.pvHeightMin,
                        g_worldGenConfig->curves.pvHeightMax);

        ImGui::DragFloat("PV Height Min", &g_worldGenConfig->curves.pvHeightMin, 1.0f, -30.0f, 0.0f);
        ImGui::DragFloat("PV Height Max", &g_worldGenConfig->curves.pvHeightMax, 1.0f, 0.0f, 50.0f);

        ImGui::PopID();
    }
}

//----------------------------------------------------------------------------------------------------
// ShowBiomeNoiseTab - ImGui tab for 6-layer biome noise parameters
// Assignment 4: Phase 5B.4 - Temperature, Humidity, Continentalness, Erosion, Weirdness, PV
//----------------------------------------------------------------------------------------------------
void Game::ShowBiomeNoiseTab()
{
    ImGui::Text("Biome Noise Parameters (6 Layers)");
    ImGui::Separator();

    // Temperature
    if (ImGui::CollapsingHeader("Temperature (T)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("T Scale", &g_worldGenConfig->biomeNoise.temperatureScale, 10.0f, 100.0f, 10000.0f);
        ImGui::DragInt("T Octaves", &g_worldGenConfig->biomeNoise.temperatureOctaves, 0.1f, 1, 6);
        ImGui::DragFloat("T Persistence", &g_worldGenConfig->biomeNoise.temperaturePersistence, 0.01f, 0.1f, 1.0f);
    }

    // Humidity
    if (ImGui::CollapsingHeader("Humidity (H)"))
    {
        ImGui::DragFloat("H Scale", &g_worldGenConfig->biomeNoise.humidityScale, 10.0f, 100.0f, 10000.0f);
        ImGui::DragInt("H Octaves", &g_worldGenConfig->biomeNoise.humidityOctaves, 0.1f, 1, 6);
        ImGui::DragFloat("H Persistence", &g_worldGenConfig->biomeNoise.humidityPersistence, 0.01f, 0.1f, 1.0f);
    }

    // Continentalness
    if (ImGui::CollapsingHeader("Continentalness (C)"))
    {
        ImGui::DragFloat("C Scale", &g_worldGenConfig->biomeNoise.continentalnessScale, 10.0f, 100.0f, 5000.0f);
        ImGui::DragInt("C Octaves", &g_worldGenConfig->biomeNoise.continentalnessOctaves, 0.1f, 1, 6);
        ImGui::DragFloat("C Persistence", &g_worldGenConfig->biomeNoise.continentalnessPersistence, 0.01f, 0.1f, 1.0f);
    }

    // Erosion
    if (ImGui::CollapsingHeader("Erosion (E)"))
    {
        ImGui::DragFloat("E Scale", &g_worldGenConfig->biomeNoise.erosionScale, 10.0f, 100.0f, 5000.0f);
        ImGui::DragInt("E Octaves", &g_worldGenConfig->biomeNoise.erosionOctaves, 0.1f, 1, 6);
        ImGui::DragFloat("E Persistence", &g_worldGenConfig->biomeNoise.erosionPersistence, 0.01f, 0.1f, 1.0f);
    }

    // Weirdness
    if (ImGui::CollapsingHeader("Weirdness (W)"))
    {
        ImGui::DragFloat("W Scale", &g_worldGenConfig->biomeNoise.weirdnessScale, 10.0f, 100.0f, 5000.0f);
        ImGui::DragInt("W Octaves", &g_worldGenConfig->biomeNoise.weirdnessOctaves, 0.1f, 1, 6);
        ImGui::DragFloat("W Persistence", &g_worldGenConfig->biomeNoise.weirdnessPersistence, 0.01f, 0.1f, 1.0f);
        ImGui::Text("Note: PV = 1 - |(3 * |W|) - 2| (calculated from Weirdness)");
    }
}

//----------------------------------------------------------------------------------------------------
// ShowDensityTab - ImGui tab for 3D density terrain parameters
// Assignment 4: Phase 5B.4 - 3D noise, slides, biasing, sea level
//----------------------------------------------------------------------------------------------------
void Game::ShowDensityTab()
{
    ImGui::Text("3D Density Terrain Parameters");
    ImGui::Separator();

    // 3D Density Noise
    if (ImGui::CollapsingHeader("3D Density Noise", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Density Scale", &g_worldGenConfig->density.densityNoiseScale, 5.0f, 50.0f, 500.0f);
        ImGui::DragInt("Density Octaves", &g_worldGenConfig->density.densityNoiseOctaves, 0.1f, 1, 5);
        ImGui::DragFloat("Bias Per Block", &g_worldGenConfig->density.densityBiasPerBlock, 0.001f, 0.0f, 0.5f);
        ImGui::Text("Formula: D(x,y,z) = N3D(x,y,z,scale) + bias * (DEFAULT_HEIGHT - z)");
    }

    // Slides
    if (ImGui::CollapsingHeader("Slides"))
    {
        ImGui::Text("Top Slide (world ceiling smoothing)");
        ImGui::DragInt("Top Start", &g_worldGenConfig->density.topSlideStart, 1.0f, 80, 127);
        ImGui::DragInt("Top End", &g_worldGenConfig->density.topSlideEnd, 1.0f, 80, 127);

        ImGui::Separator();

        ImGui::Text("Bottom Slide (world floor smoothing)");
        ImGui::DragInt("Bottom Start", &g_worldGenConfig->density.bottomSlideStart, 1.0f, 0, 40);
        ImGui::DragInt("Bottom End", &g_worldGenConfig->density.bottomSlideEnd, 1.0f, 0, 40);
    }

    // Terrain Height
    if (ImGui::CollapsingHeader("Terrain Height"))
    {
        ImGui::DragFloat("Default Terrain Height", &g_worldGenConfig->density.defaultTerrainHeight, 1.0f, 50.0f, 100.0f);
        ImGui::DragFloat("Sea Level", &g_worldGenConfig->density.seaLevel, 1.0f, 50.0f, 100.0f);
        ImGui::Text("Note: Terrain height is modified by curve offsets (continentalness, erosion, PV)");
    }
}

//----------------------------------------------------------------------------------------------------
// ShowCavesTab - ImGui tab for cheese and spaghetti cave parameters
// Assignment 4: Phase 5B.4 - Cheese caverns and spaghetti tunnels
//----------------------------------------------------------------------------------------------------
void Game::ShowCavesTab()
{
    ImGui::Text("Cave Generation Parameters");
    ImGui::Separator();

    // Cheese caves
    if (ImGui::CollapsingHeader("Cheese Caves (Large Caverns)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Cheese Scale", &g_worldGenConfig->caves.cheeseNoiseScale, 5.0f, 20.0f, 200.0f);
        ImGui::DragInt("Cheese Octaves", &g_worldGenConfig->caves.cheeseNoiseOctaves, 0.1f, 1, 5);
        ImGui::DragFloat("Cheese Threshold", &g_worldGenConfig->caves.cheeseThreshold, 0.01f, 0.1f, 0.9f);
        ImGui::DragInt("Cheese Seed Offset", &g_worldGenConfig->caves.cheeseNoiseSeedOffset, 1, 0, 100);
        ImGui::Text("Lower threshold = more caves");
    }

    // Spaghetti caves
    if (ImGui::CollapsingHeader("Spaghetti Caves (Winding Tunnels)"))
    {
        ImGui::DragFloat("Spaghetti Scale", &g_worldGenConfig->caves.spaghettiNoiseScale, 5.0f, 10.0f, 100.0f);
        ImGui::DragInt("Spaghetti Octaves", &g_worldGenConfig->caves.spaghettiNoiseOctaves, 0.1f, 1, 5);
        ImGui::DragFloat("Spaghetti Threshold", &g_worldGenConfig->caves.spaghettiThreshold, 0.01f, 0.1f, 0.9f);
        ImGui::DragInt("Spaghetti Seed Offset", &g_worldGenConfig->caves.spaghettiNoiseSeedOffset, 1, 0, 100);
        ImGui::Text("Lower threshold = more caves");
    }

    // Safety parameters
    if (ImGui::CollapsingHeader("Safety Parameters"))
    {
        ImGui::DragInt("Min Cave Depth From Surface", &g_worldGenConfig->caves.minCaveDepthFromSurface, 1, 0, 20);
        ImGui::DragInt("Min Cave Height Above Lava", &g_worldGenConfig->caves.minCaveHeightAboveLava, 1, 0, 10);
        ImGui::Text("Prevents caves from breaking through surface or affecting lava layer");
    }
}

//----------------------------------------------------------------------------------------------------
// ShowTreesTab - ImGui tab for tree placement parameters
// Assignment 4: Phase 5B.4 - Tree noise and placement thresholds
//----------------------------------------------------------------------------------------------------
void Game::ShowTreesTab()
{
    ImGui::Text("Tree Placement Parameters");
    ImGui::Separator();

    if (ImGui::CollapsingHeader("Tree Noise", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::DragFloat("Tree Noise Scale", &g_worldGenConfig->trees.treeNoiseScale, 1.0f, 5.0f, 50.0f);
        ImGui::DragInt("Tree Noise Octaves", &g_worldGenConfig->trees.treeNoiseOctaves, 0.1f, 1, 4);
        ImGui::DragFloat("Tree Placement Threshold", &g_worldGenConfig->trees.treePlacementThreshold, 0.01f, 0.0f, 1.0f);
        ImGui::DragInt("Min Tree Spacing", &g_worldGenConfig->trees.minTreeSpacing, 1, 1, 10);
        ImGui::Text("Higher threshold = fewer trees");
        ImGui::Text("Note: Trees are placed based on biome type (see Phase 3B implementation)");
    }
}

//----------------------------------------------------------------------------------------------------
// ShowCarversTab - ImGui tab for ravine and river carver parameters
// Assignment 4: Phase 5B.4 - Ravine and river carving parameters
//----------------------------------------------------------------------------------------------------
void Game::ShowCarversTab()
{
    ImGui::Text("Carver Parameters (Ravines & Rivers)");
    ImGui::Separator();

    // Ravine carvers
    if (ImGui::CollapsingHeader("Ravine Carver", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::Text("Path Noise (2D)");
        ImGui::DragFloat("Ravine Path Scale", &g_worldGenConfig->carvers.ravinePathNoiseScale, 10.0f, 100.0f, 2000.0f);
        ImGui::DragInt("Ravine Path Octaves", &g_worldGenConfig->carvers.ravinePathNoiseOctaves, 0.1f, 1, 5);
        ImGui::DragFloat("Ravine Path Threshold", &g_worldGenConfig->carvers.ravinePathThreshold, 0.01f, 0.5f, 0.95f);
        ImGui::DragInt("Ravine Seed Offset", &g_worldGenConfig->carvers.ravineNoiseSeedOffset, 1, 0, 100);

        ImGui::Separator();
        ImGui::Text("Width Noise (Secondary)");
        ImGui::DragFloat("Ravine Width Scale", &g_worldGenConfig->carvers.ravineWidthNoiseScale, 5.0f, 20.0f, 200.0f);
        ImGui::DragInt("Ravine Width Octaves", &g_worldGenConfig->carvers.ravineWidthNoiseOctaves, 0.1f, 1, 4);
        ImGui::DragInt("Ravine Width Min", &g_worldGenConfig->carvers.ravineWidthMin, 1, 1, 10);
        ImGui::DragInt("Ravine Width Max", &g_worldGenConfig->carvers.ravineWidthMax, 1, 3, 20);

        ImGui::Separator();
        ImGui::Text("Depth");
        ImGui::DragInt("Ravine Depth Min", &g_worldGenConfig->carvers.ravineDepthMin, 1, 10, 60);
        ImGui::DragInt("Ravine Depth Max", &g_worldGenConfig->carvers.ravineDepthMax, 1, 30, 100);
        ImGui::DragFloat("Ravine Edge Falloff", &g_worldGenConfig->carvers.ravineEdgeFalloff, 0.01f, 0.0f, 1.0f);

        ImGui::Text("Higher threshold = rarer ravines (0.85 recommended for very rare)");
    }

    // River carvers
    if (ImGui::CollapsingHeader("River Carver"))
    {
        ImGui::Text("Path Noise (2D)");
        ImGui::DragFloat("River Path Scale", &g_worldGenConfig->carvers.riverPathNoiseScale, 10.0f, 100.0f, 2000.0f);
        ImGui::DragInt("River Path Octaves", &g_worldGenConfig->carvers.riverPathNoiseOctaves, 0.1f, 1, 5);
        ImGui::DragFloat("River Path Threshold", &g_worldGenConfig->carvers.riverPathThreshold, 0.01f, 0.5f, 0.95f);
        ImGui::DragInt("River Seed Offset", &g_worldGenConfig->carvers.riverNoiseSeedOffset, 1, 0, 100);

        ImGui::Separator();
        ImGui::Text("Width Noise (Secondary)");
        ImGui::DragFloat("River Width Scale", &g_worldGenConfig->carvers.riverWidthNoiseScale, 5.0f, 20.0f, 200.0f);
        ImGui::DragInt("River Width Octaves", &g_worldGenConfig->carvers.riverWidthNoiseOctaves, 0.1f, 1, 4);
        ImGui::DragInt("River Width Min", &g_worldGenConfig->carvers.riverWidthMin, 1, 3, 15);
        ImGui::DragInt("River Width Max", &g_worldGenConfig->carvers.riverWidthMax, 1, 5, 30);

        ImGui::Separator();
        ImGui::Text("Depth");
        ImGui::DragInt("River Depth Min", &g_worldGenConfig->carvers.riverDepthMin, 1, 1, 10);
        ImGui::DragInt("River Depth Max", &g_worldGenConfig->carvers.riverDepthMax, 1, 3, 20);
        ImGui::DragFloat("River Edge Falloff", &g_worldGenConfig->carvers.riverEdgeFalloff, 0.01f, 0.0f, 1.0f);

        ImGui::Text("Lower threshold = more common rivers (0.70 recommended)");
    }
}
