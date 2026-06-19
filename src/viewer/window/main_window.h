#ifndef PCLITE_MAIN_WINDOW_H
#define PCLITE_MAIN_WINDOW_H

#include "viewport.h"
#include <SDL2/SDL.h>
#include <functional>
#include <memory>
#include <string>

// Owns the OS window, GL context and ImGui docking chrome, and lays out a
// classic "toolbar / status bar / properties / central view" arrangement
// around a Viewport — analogous to a Qt QMainWindow with the 3D view as its
// central widget and the other panels as dockable widgets.
class MainWindow {
public:
    MainWindow(int width, int height, const std::string& title);
    ~MainWindow();

    // The central 3D view: add layers/cameras/the controller through this.
    Viewport& viewport() { return *viewport_; }

    // Content drawn once per frame inside the docked "Properties" panel.
    void setPropertiesCallback(std::function<void()> callback);

    // Runs the SDL event loop until the window is closed or Esc is pressed.
    void run();

private:
    SDL_Window*   sdlWindow_ = nullptr;
    SDL_GLContext glContext_ = nullptr;
    bool          running_   = false;
    std::string   title_;
    float         fps_       = 0.f;

    std::unique_ptr<Viewport> viewport_;
    std::function<void()> propertiesCallback_;

    bool  dockLayoutChecked_ = false;
    float viewportOriginX_  = 0.f;
    float viewportOriginY_  = 0.f;
    bool  viewportHovered_  = false;

    void handleEvent(const SDL_Event& e);
    void buildDefaultDockLayout(unsigned int dockspaceId);
    void drawUI();
};

#endif // PCLITE_MAIN_WINDOW_H
