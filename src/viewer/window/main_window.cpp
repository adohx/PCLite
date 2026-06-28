#include "main_window.h"
#include <glad/gl.h>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <stdexcept>
#include <string>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_sdl2.h"
#include "imgui_impl_opengl3.h"
#include "version.h"

MainWindow::MainWindow(int w, int h, const std::string& title) : title_(title) {
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
        throw std::runtime_error(SDL_GetError());

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

    sdlWindow_ = SDL_CreateWindow(title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, w, h,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!sdlWindow_)
        throw std::runtime_error(SDL_GetError());

    glContext_ = SDL_GL_CreateContext(sdlWindow_);
    SDL_GL_SetSwapInterval(1);

    if (!gladLoadGL((GLADloadfunc)SDL_GL_GetProcAddress))
        throw std::runtime_error("Failed to load OpenGL via glad");

    glEnable(GL_DEPTH_TEST);
    glClearColor(0.08f, 0.08f, 0.12f, 1.f);
    glPointSize(2.f);

    // ImGui's default font is ~13px, which reads as illegibly small on a
    // modern desktop monitor regardless of window/drawable size (the earlier
    // drawable-vs-requested-size heuristic only helped when the window
    // happened to get auto-maximized onto a huge display; a normal-sized
    // window still got the tiny unscaled font). Just use a fixed, generous
    // baseline scale for the font and all widget metrics.
    constexpr float kUiScale = 2.0f;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(kUiScale);

    ImFontConfig fontConfig;
    fontConfig.SizePixels = 13.0f * kUiScale;
    io.Fonts->AddFontDefault(&fontConfig);

    ImGui_ImplSDL2_InitForOpenGL(sdlWindow_, glContext_);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    viewport_ = std::make_unique<Viewport>();
}

MainWindow::~MainWindow() {
    viewport_.reset();  // releases the FBO while the GL context is still alive

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    if (glContext_) SDL_GL_DeleteContext(glContext_);
    if (sdlWindow_) SDL_DestroyWindow(sdlWindow_);
    SDL_Quit();
}

void MainWindow::setPropertiesCallback(std::function<void()> callback) {
    propertiesCallback_ = std::move(callback);
}

void MainWindow::setToolbarCallback(std::function<void()> callback) {
    toolbarCallback_ = std::move(callback);
}

void MainWindow::setHubContentCallback(std::function<void()> callback) {
    hubContentCallback_ = std::move(callback);
}

void MainWindow::setFileMenuCallback(std::function<void()> callback) {
    fileMenuCallback_ = std::move(callback);
}

void MainWindow::buildDefaultDockLayout(unsigned int dockspaceId) {
    ImGui::DockBuilderAddNode(dockspaceId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspaceId, ImGui::GetMainViewport()->Size);

    ImGuiID center = dockspaceId;
    ImGuiID top, bottom, right;
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Down,  0.08f, &bottom, &center);
    // Tall enough to fit a row of buttons at the 2x UI scale (ScaleAllSizes
    // in the constructor) -- 0.07 was sized for a single plain text label
    // and clips anything taller once real controls (the measurement mode
    // buttons) were added.
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Up,    0.12f, &top,    &center);
    ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, &right,  &center);

    ImGui::DockBuilderDockWindow("Toolbar",    top);
    ImGui::DockBuilderDockWindow("Status",     bottom);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Viewport",   center);

    ImGui::DockBuilderFinish(dockspaceId);
}

void MainWindow::drawMenuBar() {
    if (!ImGui::BeginMainMenuBar()) return;

    if (ImGui::BeginMenu("File")) {
        if (fileMenuCallback_) fileMenuCallback_();
        ImGui::Separator();
        if (ImGui::MenuItem("Exit")) running_ = false;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Help")) {
        std::string label = "PCLite v" + std::string(PCLITE_VERSION_STRING);
        ImGui::MenuItem(label.c_str(), nullptr, false, false); // display-only, not clickable
        ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
}

void MainWindow::drawHub() {
    viewportHovered_ = false; // the Viewport panel isn't drawn in Hub mode

    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::Begin("ProjectHub", nullptr, flags);
    if (hubContentCallback_) hubContentCallback_();
    ImGui::End();
    ImGui::PopStyleVar(2);
}

void MainWindow::drawUI() {
    drawMenuBar();

    if (mode_ == Mode::Hub) {
        drawHub();
        return;
    }

    const ImGuiViewport* mainViewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(mainViewport->WorkPos);
    ImGui::SetNextWindowSize(mainViewport->WorkSize);
    ImGui::SetNextWindowViewport(mainViewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDocking;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("MainWindowDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    ImGuiID dockspaceId = ImGui::GetID("MainWindowDockspace");
    if (!dockLayoutChecked_) {
        dockLayoutChecked_ = true;
        // Only lay out the defaults the first time this dockspace is ever
        // seen; once imgui.ini exists it remembers whatever the user dragged.
        if (ImGui::DockBuilderGetNode(dockspaceId) == nullptr)
            buildDefaultDockLayout(dockspaceId);
    }
    ImGui::DockSpace(dockspaceId, ImVec2(0.f, 0.f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();

    ImGui::Begin("Toolbar");
    ImGui::TextUnformatted("PCLite");
    if (toolbarCallback_) {
        ImGui::SameLine();
        ImGui::TextUnformatted("|");
        ImGui::SameLine();
        toolbarCallback_();
    }
    ImGui::End();

    ImGui::Begin("Status");
    ImGui::Text("FPS: %.1f", fps_);
    ImGui::End();

    ImGui::Begin("Properties");
    if (propertiesCallback_) propertiesCallback_();
    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.f, 0.f));
    ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleVar();

    ImVec2 avail = ImGui::GetContentRegionAvail();
    int vw = std::max(1, (int)avail.x);
    int vh = std::max(1, (int)avail.y);
    if (vw != viewport_->width() || vh != viewport_->height())
        viewport_->onResize(vw, vh);
    viewport_->render();

    ImVec2 origin = ImGui::GetCursorScreenPos();
    viewportOriginX_ = origin.x;
    viewportOriginY_ = origin.y;
    ImGui::Image((ImTextureID)(intptr_t)viewport_->textureId(),
                 ImVec2((float)vw, (float)vh), ImVec2(0, 1), ImVec2(1, 0));
    viewportHovered_ = ImGui::IsItemHovered();

    // Measurement value labels: plain 2D screen-space text drawn over the
    // rendered image, so they always face the screen by construction (no
    // 3D billboard geometry needed) -- ImGui's draw list/font isn't usable
    // from inside Viewport's own GL render pass, so this has to happen here.
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    for (auto& label : viewport_->measurementScreenLabels())
        drawList->AddText(ImVec2(origin.x + label.x, origin.y + label.y),
                           IM_COL32(255, 255, 0, 255), label.text.c_str());

    ImGui::End();
}

void MainWindow::handleEvent(const SDL_Event& e) {
    ImGui_ImplSDL2_ProcessEvent(&e);
    const ImGuiIO& io = ImGui::GetIO();

    switch (e.type) {
    case SDL_QUIT:
        running_ = false;
        break;

    case SDL_KEYDOWN:
        if (e.key.keysym.sym == SDLK_ESCAPE) running_ = false;
        if (!io.WantCaptureKeyboard) viewport_->onKey(e.key.keysym.sym, true);
        break;

    case SDL_KEYUP:
        if (!io.WantCaptureKeyboard) viewport_->onKey(e.key.keysym.sym, false);
        break;

    case SDL_MOUSEBUTTONDOWN:
        // Note: io.WantCaptureMouse is true merely because the cursor is
        // over the Viewport's own ImGui window, so it can't be used to
        // gate input here. viewportHovered_ (IsItemHovered() on the image,
        // which already excludes occlusion by other floating panels) is
        // the correct and sufficient check.
        if (viewportHovered_)
            viewport_->onMouseButton((float)e.button.x - viewportOriginX_,
                                      (float)e.button.y - viewportOriginY_,
                                      e.button.button, true, e.button.clicks);
        break;

    case SDL_MOUSEBUTTONUP:
        // Always deliver mouse-up so a drag started in the viewport ends
        // cleanly even if the cursor left the panel (or ImGui now wants it).
        viewport_->onMouseButton((float)e.button.x - viewportOriginX_,
                                  (float)e.button.y - viewportOriginY_,
                                  e.button.button, false);
        break;

    case SDL_MOUSEMOTION:
        // Always forward motion too, so an in-progress drag keeps tracking
        // once the cursor moves outside the viewport panel's bounds.
        viewport_->onMouseMove((float)e.motion.x - viewportOriginX_,
                                (float)e.motion.y - viewportOriginY_);
        break;

    case SDL_MOUSEWHEEL:
        if (viewportHovered_)
            viewport_->onScroll((float)e.wheel.y);
        break;

    default:
        break;
    }
}

void MainWindow::run() {
    using Clock = std::chrono::steady_clock;
    auto fpsTimer = Clock::now();
    int frameCount = 0;

    running_ = true;
    auto lastTime = Clock::now();
    while (running_) {
        auto now = Clock::now();
        float dt = std::chrono::duration<float>(now - lastTime).count();
        lastTime = now;
        dt = std::min(dt, 0.1f);

        SDL_Event e;
        while (SDL_PollEvent(&e))
            handleEvent(e);

        viewport_->update(dt);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        drawUI();

        ImGui::Render();

        int winW = 0, winH = 0;
        SDL_GetWindowSize(sdlWindow_, &winW, &winH);
        glViewport(0, 0, winW, winH);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        SDL_GL_SwapWindow(sdlWindow_);

        ++frameCount;
        float elapsed = std::chrono::duration<float>(now - fpsTimer).count();
        if (elapsed >= 1.0f) {
            fps_ = frameCount / elapsed;
            SDL_SetWindowTitle(sdlWindow_,
                (title_ + "  |  FPS: " + std::to_string(static_cast<int>(fps_))).c_str());
            fpsTimer = now;
            frameCount = 0;
        }
    }
}
