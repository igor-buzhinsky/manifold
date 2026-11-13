#include "console/console_window.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <format>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "util/log.hpp"

namespace manifold {

namespace {

/// The input line starts with this.
constexpr std::string_view kPrompt = "> ";

constexpr glm::vec4 kBackground{0.0f, 0.0f, 0.0f, 0.8f};
constexpr glm::vec4 kBorder{0.5f, 0.5f, 0.55f, 1.0f};
constexpr glm::vec4 kInputColor{1.0f, 1.0f, 1.0f, 1.0f};

}  // namespace

void ConsoleWindow::add_output(LogLevel level, std::string_view message) {
    LineKind kind = LineKind::kInfo;
    if (level == LogLevel::kWarning) {
        kind = LineKind::kWarning;
    } else if (level == LogLevel::kError) {
        kind = LineKind::kError;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t end = message.find('\n', start);
        add_line(std::string(message.substr(start, end == std::string_view::npos ? end : end - start)), kind);
        if (end == std::string_view::npos) {
            return;
        }
        start = end + 1;
    }
}

void ConsoleWindow::add_line(std::string text, LineKind kind) {
    output_.push_back({.text = std::move(text), .kind = kind});
    if (output_.size() > kMaxOutputLines) {
        output_.pop_front();
    }
    // A view scrolled up stays on the same lines.
    if (scroll_ > 0) {
        scroll_ = std::min(scroll_ + 1, output_.size() - 1);
    }
}

std::optional<std::string> ConsoleWindow::handle(ConsoleKey key) {
    switch (key) {
        case ConsoleKey::kSubmit: {
            std::string command = input_.text();
            input_.clear();
            history_index_.reset();
            scroll_ = 0;
            if (command.find_first_not_of(' ') == std::string::npos) {
                return std::nullopt;
            }
            add_line(std::string(kPrompt) + command, LineKind::kCommand);
            if (history_.empty() || history_.back() != command) {
                history_.push_back(command);
                if (history_.size() > kMaxHistory) {
                    history_.erase(history_.begin());
                }
            }
            return command;
        }
        case ConsoleKey::kHistoryBack:
            if (history_.empty() || history_index_ == 0U) {
                break;
            }
            if (!history_index_) {
                draft_ = input_.text();
                history_index_ = history_.size();
            }
            --*history_index_;
            input_.set_text(history_[*history_index_]);
            break;
        case ConsoleKey::kHistoryForward:
            if (!history_index_) {
                break;
            }
            ++*history_index_;
            if (*history_index_ == history_.size()) {
                history_index_.reset();
                input_.set_text(draft_);
            } else {
                input_.set_text(history_[*history_index_]);
            }
            break;
        case ConsoleKey::kScrollUp:
            scroll_ = output_.empty() ? 0 : std::min(scroll_ + kScrollStep, output_.size() - 1);
            break;
        case ConsoleKey::kScrollDown:
            scroll_ = scroll_ > kScrollStep ? scroll_ - kScrollStep : 0;
            break;
    }
    return std::nullopt;
}

OverlayMesh ConsoleWindow::build_mesh(const PixelSize& size) const {
    // The font is tiny, so it is scaled by whole pixels: twice at 900 pixels high.
    const float scale = std::max(1.0f, std::floor(static_cast<float>(size.height) / 400.0f));
    const float line_height = kTextLineHeight * scale;
    const float margin = 4.0f * scale;
    const auto width = static_cast<float>(size.width);
    const float height = std::floor(static_cast<float>(size.height) / 2.0f);

    OverlayMesh mesh;
    mesh.add_rectangle({0.0f, 0.0f}, {width, height}, kBackground);
    mesh.add_rectangle({0.0f, height}, {width, scale}, kBorder);

    // The input line, at the bottom, with a bar before the character at the cursor.
    float y = height - margin - line_height;
    mesh.add_text({margin, y}, std::string(kPrompt) + input_.text(), scale, kInputColor);
    const float cursor_x = margin + text_width(std::string(kPrompt) + input_.text().substr(0, input_.cursor()), scale);
    mesh.add_rectangle({cursor_x, y - scale}, {scale, line_height}, kInputColor);

    // The output above it, newest at the bottom. A view scrolled up says so.
    const float max_width = width - (2.0f * margin);
    if (scroll_ > 0) {
        y -= line_height;
        mesh.add_text({margin, y}, std::format("--- {} newer lines below (Page Down) ---", scroll_), scale, kBorder);
    }
    for (std::size_t skipped = 0; skipped < output_.size(); ++skipped) {
        if (skipped < scroll_) {
            continue;
        }
        const Line& line = output_[output_.size() - 1 - skipped];
        glm::vec4 color{0.8f, 0.8f, 0.8f, 1.0f};
        switch (line.kind) {
            case LineKind::kInfo:
                break;
            case LineKind::kWarning:
                color = {1.0f, 0.85f, 0.3f, 1.0f};
                break;
            case LineKind::kError:
                color = {1.0f, 0.45f, 0.4f, 1.0f};
                break;
            case LineKind::kCommand:
                color = {0.55f, 0.85f, 1.0f, 1.0f};
                break;
        }
        const std::vector<std::string> wrapped = wrap_text(line.text, max_width, scale);
        for (const std::string& part : std::views::reverse(wrapped)) {
            y -= line_height;
            if (y < margin) {
                return mesh;
            }
            mesh.add_text({margin, y}, part, scale, color);
        }
    }
    return mesh;
}

}  // namespace manifold
