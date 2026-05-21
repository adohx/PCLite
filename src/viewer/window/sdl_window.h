#ifndef PCLITE_SDL_WINDOW_H
#define PCLITE_SDL_WINDOW_H

#include "window.h"
#include <SDL2/SDL.h>
#include <string>

class SDLWindow : public Window {
public:
    SDLWindow(int width, int height, const std::string& title);
    ~SDLWindow() override;

    // Sets up GL matrices from camera[0], updates + renders all layers.
    void render() override;

    // Runs the SDL event loop until the window is closed or Esc is pressed.
    void run();

    // Initial orbit parameters (call before run())
    void setOrbitDistance(float d) { distance_ = d; }
    void setOrbitAzimuth(float a)  { azimuth_  = a; }
    void setOrbitElevation(float e){ elevation_ = e; }

private:
    SDL_Window*   sdlWindow_ = nullptr;
    SDL_GLContext glContext_  = nullptr;
    bool          running_   = false;

    float azimuth_    = 45.f;
    float elevation_  = 30.f;
    float distance_   = 50.f;
    bool  dragging_   = false;
    int   lastMouseX_ = 0;
    int   lastMouseY_ = 0;

    void updateCameraPosition();
    void handleEvent(const SDL_Event& e);
};

#endif //PCLITE_SDL_WINDOW_H
