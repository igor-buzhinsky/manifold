#include "ui/debug_overlay.hpp"

#include <algorithm>
#include <cstddef>
#include <format>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_video.h>
#include <glm/gtc/type_ptr.hpp>
#include <imgui.h>
#include <imgui_impl_opengl3.h>
#include <imgui_impl_sdl3.h>
#include <imgui_internal.h>

#include "console/game_commands.hpp"
#include "game.hpp"
#include "platform/window.hpp"
#include "render/lighting.hpp"
#include "render/renderer.hpp"
#include "render/shadow_map.hpp"
#include "util/frame_times.hpp"
#include "world/player.hpp"

namespace manifold {

namespace {

/// GLSL version of ImGui's shaders, matching the OpenGL 4.1 context of Window.
constexpr const char* kGlslVersion = "#version 410";

/// Width of the overlay window before scaling, in screen units.
constexpr float kWidth = 380.0f;
/// Distance of the overlay window from the edges of the window before scaling, in screen units.
constexpr float kMargin = 10.0f;
/// Frame times at the top of the graph at least, in milliseconds: two frames at 60 frames per second.
constexpr float kGraphMinTop = 33.3f;
/// Height of the frame time graph before scaling, in screen units.
constexpr float kGraphHeight = 50.0f;

float to_degrees(float radians) { return radians * 180.0f / std::numbers::pi_v<float>; }

/// Shows a line of text, which, unlike ImGui::Text(), is formatted by std::format.
void text(std::string_view line) { ImGui::TextUnformatted(line.data(), line.data() + line.size()); }

/// Shows the frame rate, a graph of the last frame times, and what the renderer did.
void frame_section(const FrameTimes& frames, const RenderStats& stats, float scale) {
    const float average = frames.average();
    text(std::format("{:.0f} FPS, {:.2f} ms per frame, longest {:.2f} ms", average > 0.0f ? 1.0f / average : 0.0f,
                     average * 1000.0f, frames.longest() * 1000.0f));
    std::vector<float> milliseconds = frames.values();
    std::ranges::transform(milliseconds, milliseconds.begin(), [](float seconds) { return seconds * 1000.0f; });
    const float top = std::max(kGraphMinTop, frames.longest() * 1000.0f);
    ImGui::PlotLines("##frame_times", milliseconds.data(), static_cast<int>(milliseconds.size()), 0, nullptr, 0.0f, top,
                     ImVec2(-1.0f, kGraphHeight * scale));
    text(std::format("Lights {}, with shadows {}, shadow maps drawn {}", stats.lights, stats.shadowed_lights,
                     stats.shadow_maps_drawn));
}

/// Shows where the player is and how it moves, with a checkbox for noclip.
void player_section(Game& game) {
    const Player& player = game.player();
    text(std::format("Position ({:.2f}, {:.2f}, {:.2f})", player.position().x, player.position().y,
                     player.position().z));
    text(std::format("Yaw {:.1f} deg, pitch {:.1f} deg", to_degrees(player.yaw()), to_degrees(player.pitch())));
    text(std::format("Vertical speed {:.2f} m/s, {}", player.vertical_speed(),
                     player.on_ground() ? "on the ground" : "in the air"));
    bool noclip = game.noclip();
    if (ImGui::Checkbox("Noclip", &noclip)) {
        game.set_noclip(noclip);
    }
}

/// Shows checkboxes for the switches of the view, as the console commands of the same names set them.
void view_section(ViewSettings& view) {
    ImGui::Checkbox("Arrow", &view.arrow);
    ImGui::SameLine();
    ImGui::Checkbox("Crosshair", &view.crosshair);
    ImGui::SameLine();
    ImGui::Checkbox("Lighting", &view.lighting);
    ImGui::SameLine();
    ImGui::Checkbox("Shadows", &view.shadows);
}

/// Shows sliders for the lights and the shadow bias, and a button that resets them.
void light_section(ViewSettings& view) {
    LightSettings& lights = view.lights;
    constexpr ImGuiColorEditFlags kColorFlags = ImGuiColorEditFlags_Float;
    // The ambient light of a level can be above 1, see level_ambient(), which editing must keep.
    ImGui::ColorEdit3("Ambient", glm::value_ptr(lights.ambient), kColorFlags | ImGuiColorEditFlags_HDR);

    ImGui::SeparatorText("Player's light");
    ImGui::ColorEdit3("Color##player", glm::value_ptr(lights.player_color), kColorFlags);
    ImGui::SliderFloat("Intensity##player", &lights.player_intensity, 0.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Range##player", &lights.player_range, 0.5f, 40.0f, "%.1f m");

    ImGui::SeparatorText("Torches");
    ImGui::SliderFloat("Intensity##torch", &lights.torch.intensity, 0.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Range##torch", &lights.torch.range, 0.5f, 20.0f, "%.1f m");

    ImGui::SeparatorText("Exit");
    ImGui::SliderFloat("Intensity##exit", &lights.exit.intensity, 0.0f, 20.0f, "%.2f");
    ImGui::SliderFloat("Range##exit", &lights.exit.range, 0.5f, 20.0f, "%.1f m");

    ImGui::SeparatorText("Selection and shadows");
    int max_lights = static_cast<int>(lights.max_lights);
    if (ImGui::SliderInt("Most lights", &max_lights, 1, static_cast<int>(kMaxLights))) {
        lights.max_lights = static_cast<std::size_t>(std::max(max_lights, 1));
    }
    ImGui::SliderFloat("Fade distance", &lights.fade_distance, 0.0f, 10.0f, "%.2f m");
    ImGui::SliderFloat("Shadow bias", &view.shadow_bias, 0.0f, 0.05f, "%.4f m");

    if (ImGui::Button("Reset lights")) {
        view.lights = LightSettings{};
        view.lights.ambient = view.level_ambient;
        view.shadow_bias = kShadowBias;
    }
}

}  // namespace

DebugOverlay::DebugOverlay(const Window& window) : window_(window.sdl_window()) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    // ImGui would remember window positions in imgui.ini in the current directory. The overlay starts in
    // the same place every time instead.
    io.IniFilename = nullptr;
    ImGui::StyleColorsDark();
    // The scale is read once, so moving the window to a display with another scale keeps the old one.
    // Without SDL_WINDOW_HIGH_PIXEL_DENSITY, this is the display's content scale, which ImGui's examples use.
    const float display_scale = SDL_GetWindowDisplayScale(window_);
    scale_ = display_scale > 0.0f ? display_scale : 1.0f;
    ImGuiStyle& style = ImGui::GetStyle();
    style.ScaleAllSizes(scale_);
    style.FontScaleDpi = scale_;
    if (!ImGui_ImplSDL3_InitForOpenGL(window.sdl_window(), window.gl_context())) {
        ImGui::DestroyContext();
        throw std::runtime_error("Setting up ImGui for SDL3 failed");
    }
    if (!ImGui_ImplOpenGL3_Init(kGlslVersion)) {
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
        throw std::runtime_error("Setting up ImGui for OpenGL failed");
    }
}

DebugOverlay::~DebugOverlay() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
}

void DebugOverlay::set_input(bool on) {
    ImGuiIO& io = ImGui::GetIO();
    if (on && !input_) {
        io.ClearInputKeys();
        io.ClearInputMouse();
    } else if (!on && input_) {
        // ImGui would only let go of the control, and turn text input off, in its next frame, which may not
        // come while the overlay is hidden. The console window turns text input on again when it opens.
        if (io.WantTextInput) {
            SDL_StopTextInput(window_);
        }
        ImGui::ClearActiveID();
    }
    input_ = on;
}

// Not const, although no member changes: it changes ImGui's state, which belongs to the overlay.
// NOLINTNEXTLINE(readability-make-member-function-const)
bool DebugOverlay::handle_event(const SDL_Event& event) {
    if (!input_) {
        return false;
    }
    ImGui_ImplSDL3_ProcessEvent(&event);
    const bool keyboard_event =
        event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_TEXT_INPUT;
    // Motion is left to the game, which may want to know where the pointer is.
    const bool mouse_event = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP ||
                             event.type == SDL_EVENT_MOUSE_WHEEL;
    return (keyboard_event && wants_keyboard()) || (mouse_event && ImGui::GetIO().WantCaptureMouse);
}

bool DebugOverlay::wants_keyboard() const { return input_ && ImGui::GetIO().WantCaptureKeyboard; }

// Not const, although no member changes: it runs an ImGui frame, which changes ImGui's state.
// NOLINTNEXTLINE(readability-make-member-function-const)
void DebugOverlay::draw(Game& game, ViewSettings& view, const RenderStats& stats, const FrameTimes& frames) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    // In the top right corner, out of the way of the console window, which starts at the left.
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - (kMargin * scale_), kMargin * scale_), ImGuiCond_FirstUseEver,
                            ImVec2(1.0f, 0.0f));
    ImGui::SetNextWindowSize(ImVec2(kWidth * scale_, 0.0f), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Debug")) {
        if (ImGui::CollapsingHeader("Frame", ImGuiTreeNodeFlags_DefaultOpen)) {
            frame_section(frames, stats, scale_);
        }
        if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen)) {
            player_section(game);
        }
        if (ImGui::CollapsingHeader("View", ImGuiTreeNodeFlags_DefaultOpen)) {
            view_section(view);
        }
        if (ImGui::CollapsingHeader("Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
            light_section(view);
        }
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace manifold
