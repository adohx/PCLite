#ifndef PCLITE_SDL_WINDOW_H
#define PCLITE_SDL_WINDOW_H

#include "window.h"
#include "camera/camera_controller.h"
#include <SDL2/SDL.h>
#include <memory>
#include <string>

class SDLWindow : public Window {
public:
    SDLWindow(int width, int height, const std::string& title);
    ~SDLWindow() override;

    // Sets up GL matrices from camera[0], updates + renders all layers.
    void render() override;

    // Runs the SDL event loop until the window is closed or Esc is pressed.
    void run();

    // Takes ownership of the controller and syncs its screen size.
    void setController(std::unique_ptr<CameraController> controller);

    void onResize(int w, int h) override;

private:
    SDL_Window*   sdlWindow_ = nullptr;
    SDL_GLContext glContext_  = nullptr;
    bool          running_   = false;
    std::string   title_;

    std::unique_ptr<CameraController> controller_;

    // Per-button drag tracking (indexed by SDL button number)
    bool leftDragging_  = false;
    bool rightDragging_ = false;
    int  lastMouseX_    = 0;
    int  lastMouseY_    = 0;

    void handleEvent(const SDL_Event& e);
};

#endif // PCLITE_SDL_WINDOW_H
