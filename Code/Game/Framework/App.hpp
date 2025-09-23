//----------------------------------------------------------------------------------------------------
// App.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once
#include "Engine/Core/EventSystem.hpp"

//-Forward-Declaration--------------------------------------------------------------------------------
class Camera;
class JobSystem;

//----------------------------------------------------------------------------------------------------
class App
{
public:
    App();
    ~App();

    void Startup();
    void Shutdown();

    void RunMainLoop();
    void RunFrame();

    void DeleteAndCreateNewGame();

    // JobSystem access for global use
    static JobSystem* GetJobSystem() { return s_jobSystem; }

    static bool OnCloseButtonClicked(EventArgs& args);
    static void RequestQuit();
    static bool m_isQuitting;

private:
    void BeginFrame() const;
    void Update();
    void Render() const;
    void EndFrame() const;

    void UpdateCursorMode();

    Camera* m_devConsoleCamera = nullptr;
    
    // Global JobSystem instance
    static JobSystem* s_jobSystem;
};
