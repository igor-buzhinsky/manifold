#include "ui/text_field.hpp"

#include <string>
#include <string_view>
#include <utility>

namespace manifold {

void TextField::insert(std::string_view text) {
    std::string printable;
    for (const char c : text) {
        if (is_printable_ascii(c)) {
            printable += c;
        }
    }
    text_.insert(cursor_, printable);
    cursor_ += printable.size();
}

void TextField::apply(EditKey key) {
    switch (key) {
        case EditKey::kBackspace:
            if (cursor_ > 0) {
                --cursor_;
                text_.erase(cursor_, 1);
            }
            break;
        case EditKey::kDelete:
            if (cursor_ < text_.size()) {
                text_.erase(cursor_, 1);
            }
            break;
        case EditKey::kLeft:
            if (cursor_ > 0) {
                --cursor_;
            }
            break;
        case EditKey::kRight:
            if (cursor_ < text_.size()) {
                ++cursor_;
            }
            break;
        case EditKey::kHome:
            cursor_ = 0;
            break;
        case EditKey::kEnd:
            cursor_ = text_.size();
            break;
    }
}

void TextField::set_text(std::string text) {
    text_ = std::move(text);
    cursor_ = text_.size();
}

}  // namespace manifold
