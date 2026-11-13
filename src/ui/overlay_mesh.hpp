#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>

namespace manifold {

/// A vertex of an OverlayMesh.
struct OverlayVertex {
    glm::vec2 position;  ///< In pixels from the top-left corner of the picture, y down.
    glm::vec4 color;     ///< sRGB and opacity, each in [0, 1].
};

/// Height of a line of text at scale 1, in pixels.
inline constexpr float kTextLineHeight = 12.0f;

/// Width of one line of text at `scale`, in pixels. All characters are equally wide, see OverlayMesh::add_text().
float text_width(std::string_view text, float scale);

/// Splits text into lines that are at most `max_width` pixels wide at `scale`.
///
/// Lines are broken after the last space that fits, or between characters if a word is too long. Line
/// breaks in `text` are kept. Every line gets at least one character, so very narrow widths still end.
std::vector<std::string> wrap_text(std::string_view text, float max_width, float scale);

/// Flat shapes and text drawn over the picture, e.g. the console or later the parts of the game's interface.
///
/// Coordinates are in pixels from the top-left corner, with y down, as usual for interfaces. Every three
/// vertices form a triangle, drawn without culling. Text uses the small blocky font of stb_easy_font, which
/// has only printable ASCII characters. Every character gets a cell of the same width, so columns line up.
class OverlayMesh {
public:
    /// Adds a rectangle.
    ///
    /// \param top_left Its top-left corner.
    /// \param size     Its width and height.
    /// \param color    sRGB and opacity.
    void add_rectangle(const glm::vec2& top_left, const glm::vec2& size, const glm::vec4& color);

    /// Adds one line of text.
    ///
    /// \param top_left Top-left corner of the first character.
    /// \param text     The text. Characters that are not printable ASCII are shown as `?`.
    /// \param scale    Size of a font pixel, in pixels. A line is kTextLineHeight * scale high.
    /// \param color    sRGB and opacity.
    void add_text(const glm::vec2& top_left, std::string_view text, float scale, const glm::vec4& color);

    const std::vector<OverlayVertex>& vertices() const { return vertices_; }

private:
    std::vector<OverlayVertex> vertices_;
};

}  // namespace manifold
