#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace manifold {

/// Keys that edit a TextField or move its cursor.
enum class EditKey : std::uint8_t {
    kBackspace,  ///< Deletes the character before the cursor.
    kDelete,     ///< Deletes the character after the cursor.
    kLeft,       ///< Moves the cursor one character back.
    kRight,      ///< Moves the cursor one character forward.
    kHome,       ///< Moves the cursor to the start.
    kEnd,        ///< Moves the cursor to the end.
};

/// One line of text that the player types, with a cursor, e.g. the input line of the console.
///
/// Only printable ASCII characters are kept, since the font has no others. The cursor is between
/// characters: 0 is before the first one, size() after the last one.
class TextField {
public:
    /// Inserts typed text at the cursor and moves the cursor after it.
    ///
    /// Characters that are not printable ASCII, such as line breaks or UTF-8 sequences, are left out.
    void insert(std::string_view text);

    /// Edits the text or moves the cursor. Does nothing where there is nothing to do, e.g. kLeft at the start.
    void apply(EditKey key);

    /// Replaces the text and puts the cursor at the end.
    void set_text(std::string text);

    /// Removes all text.
    void clear() { set_text({}); }

    const std::string& text() const { return text_; }
    std::size_t cursor() const { return cursor_; }

private:
    std::string text_;
    std::size_t cursor_ = 0;
};

/// Whether `c` is a printable ASCII character: from space to tilde.
inline bool is_printable_ascii(char c) { return c >= ' ' && c <= '~'; }

}  // namespace manifold
