#include "ui/screen_stack.hpp"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

void ScreenStack::push(std::unique_ptr<Screen> screen) { screens_.push_back(std::move(screen)); }

void ScreenStack::pop() {
    if (!screens_.empty()) {
        screens_.pop_back();
    }
}

std::optional<UiCommand> ScreenStack::handle(const UiEvent& event, const PixelSize& picture) {
    if (screens_.empty()) {
        return std::nullopt;
    }
    return screens_.back()->handle(event, picture);
}

OverlayMesh ScreenStack::build_mesh(const PixelSize& picture) const {
    OverlayMesh mesh;
    for (const std::unique_ptr<Screen>& screen : screens_) {
        screen->add_to(mesh, picture);
    }
    return mesh;
}

bool ScreenStack::hides_game() const {
    return std::ranges::any_of(screens_, [](const std::unique_ptr<Screen>& screen) { return screen->hides_game(); });
}

}  // namespace manifold
