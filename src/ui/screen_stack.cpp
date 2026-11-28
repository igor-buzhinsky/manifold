#include "ui/screen_stack.hpp"

#include <algorithm>
#include <cstddef>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "render/viewport.hpp"
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

std::vector<ScreenLayer> ScreenStack::build_layers(const PixelSize& picture) const {
    std::vector<ScreenLayer> layers(screens_.size());
    for (std::size_t i = 0; i < screens_.size(); ++i) {
        screens_[i]->add_to(layers[i].mesh, picture);
        screens_[i]->add_icons(layers[i].icons, picture);
        screens_[i]->add_over_icons(layers[i].over_icons, picture);
    }
    return layers;
}

void ScreenStack::close_level_screens() {
    std::erase_if(screens_, [](const std::unique_ptr<Screen>& screen) { return screen->shows_level(); });
}

bool ScreenStack::hides_game() const {
    return std::ranges::any_of(screens_, [](const std::unique_ptr<Screen>& screen) { return screen->hides_game(); });
}

}  // namespace manifold
