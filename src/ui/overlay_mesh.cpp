#include "ui/overlay_mesh.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <stb_easy_font.h>

#include "ui/text_field.hpp"

namespace manifold {

namespace {

/// Replaces characters that the font doesn't have with `?`, which stb_easy_font needs.
std::string printable(std::string_view text) {
    std::string result(text);
    for (char& c : result) {
        if (!is_printable_ascii(c)) {
            c = '?';
        }
    }
    return result;
}

/// A vertex as stb_easy_font_print() writes it.
struct FontVertex {
    float x;
    float y;
    float z;
    std::array<unsigned char, 4> color;
};
static_assert(sizeof(FontVertex) == 16);

/// Width of the cell of every character at scale 1, in pixels, including the space to the next one.
///
/// Almost all characters of stb_easy_font fit. The few wider ones, such as `@`, are squeezed.
constexpr float kCharacterAdvance = 7.0f;

/// Width of a character in stb_easy_font at scale 1, including the space to the next one.
float font_advance(char c) {
    std::array<char, 2> character{c, '\0'};
    return static_cast<float>(stb_easy_font_width(character.data()));
}

}  // namespace

float text_width(std::string_view text, float scale) {
    return static_cast<float>(text.size()) * kCharacterAdvance * scale;
}

std::vector<std::string> wrap_text(std::string_view text, float max_width, float scale) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t end = text.find('\n', start);
        std::string_view paragraph = text.substr(start, end == std::string_view::npos ? end : end - start);
        if (paragraph.empty()) {
            lines.emplace_back();
        }
        while (!paragraph.empty()) {
            // The longest start of the paragraph that fits, with at least one character.
            std::size_t fit = 0;
            while (fit < paragraph.size() &&
                   (fit == 0 || text_width(paragraph.substr(0, fit + 1), scale) <= max_width)) {
                ++fit;
            }
            if (fit < paragraph.size()) {
                const std::size_t space = paragraph.substr(0, fit + 1).rfind(' ');
                if (space != std::string_view::npos && space > 0) {
                    fit = space;
                }
            }
            lines.emplace_back(paragraph.substr(0, fit));
            paragraph.remove_prefix(fit);
            if (paragraph.starts_with(' ')) {
                paragraph.remove_prefix(1);  // The space where the line was broken.
            }
        }
        if (end == std::string_view::npos) {
            return lines;
        }
        start = end + 1;
    }
}

void OverlayMesh::add_rectangle(const glm::vec2& top_left, const glm::vec2& size, const glm::vec4& color) {
    const glm::vec2 bottom_right = top_left + size;
    const glm::vec2 top_right{bottom_right.x, top_left.y};
    const glm::vec2 bottom_left{top_left.x, bottom_right.y};
    for (const glm::vec2& corner : {top_left, bottom_left, bottom_right, top_left, bottom_right, top_right}) {
        vertices_.push_back({.position = corner, .color = color});
    }
}

void OverlayMesh::add_text(const glm::vec2& top_left, std::string_view text, float scale, const glm::vec4& color) {
    // stb_easy_font needs about 270 bytes per character on average, and fewer than 1000 for any.
    constexpr std::size_t kBufferBytes = 1000;
    std::vector<FontVertex> buffer(kBufferBytes / sizeof(FontVertex));
    const std::string line = printable(text);
    // One character at a time, each in a cell of the same width, so that columns line up, e.g. in the plan
    // of a level.
    for (std::size_t i = 0; i < line.size(); ++i) {
        std::array<char, 2> character{line[i], '\0'};
        const int quads = stb_easy_font_print(0.0f, 0.0f, character.data(), nullptr, buffer.data(),
                                              static_cast<int>(buffer.size() * sizeof(FontVertex)));
        const glm::vec2 cell = top_left + glm::vec2(static_cast<float>(i) * kCharacterAdvance * scale, 0.0f);
        const glm::vec2 cell_scale{std::min(1.0f, kCharacterAdvance / font_advance(line[i])) * scale, scale};
        for (int quad = 0; quad < quads; ++quad) {
            const std::size_t first = static_cast<std::size_t>(quad) * 4;
            const glm::vec2 corner{buffer[first].x, buffer[first].y};
            const glm::vec2 opposite{buffer[first + 2].x, buffer[first + 2].y};
            add_rectangle(cell + (corner * cell_scale), (opposite - corner) * cell_scale, color);
        }
    }
}

}  // namespace manifold
