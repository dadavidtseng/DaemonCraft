//----------------------------------------------------------------------------------------------------
// GameCommon.hpp
//----------------------------------------------------------------------------------------------------

//----------------------------------------------------------------------------------------------------
#pragma once

#define GAME_DEBUG
#include "Engine/Core/ErrorWarningAssert.hpp"

//-Forward-Declaration--------------------------------------------------------------------------------
struct Rgba8;
struct Vec2;
class App;
class AudioSystem;
class BitmapFont;
class Game;
class LightSubsystem;
class Renderer;
class RandomNumberGenerator;
class ResourceSubsystem;

//----------------------------------------------------------------------------------------------------
// one-time declaration
extern App*                   g_app;
extern AudioSystem*           g_audio;
extern BitmapFont*            g_bitmapFont;
extern Game*                  g_game;
extern Renderer*              g_renderer;
extern RandomNumberGenerator* g_rng;
extern LightSubsystem*        g_lightSubsystem;
extern ResourceSubsystem*     g_resourceSubsystem;

//----------------------------------------------------------------------------------------------------
void DebugDrawRing(Vec2 const& center, float radius, float thickness, Rgba8 const& color);
void DebugDrawLine(Vec2 const& start, Vec2 const& end, float thickness, Rgba8 const& color);


//----------------------------------------------------------------------------------------------------
template <typename T>
void GAME_SAFE_RELEASE(T*& pointer)
{
    if (pointer == nullptr) ERROR_AND_DIE("test")
    delete pointer;
    pointer = nullptr;
}
