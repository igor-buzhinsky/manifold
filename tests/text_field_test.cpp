#include "ui/text_field.hpp"

#include <gtest/gtest.h>

namespace manifold {
namespace {

TEST(TextFieldTest, InsertsAtCursorKeepingOnlyPrintableAscii) {
    TextField field;
    field.insert("camra");
    EXPECT_EQ(field.cursor(), 5U);
    field.apply(EditKey::kLeft);
    field.apply(EditKey::kLeft);
    field.insert("e\n\xC3\xA9");  // A line break and UTF-8 for an accented e are left out.
    EXPECT_EQ(field.text(), "camera");
    EXPECT_EQ(field.cursor(), 4U);
}

TEST(TextFieldTest, DeletesAndMovesWithinBounds) {
    TextField field;
    field.apply(EditKey::kBackspace);  // Nothing to delete.
    field.apply(EditKey::kLeft);
    field.insert("abc");
    field.apply(EditKey::kRight);  // Already at the end.
    EXPECT_EQ(field.cursor(), 3U);
    field.apply(EditKey::kBackspace);
    EXPECT_EQ(field.text(), "ab");
    field.apply(EditKey::kHome);
    field.apply(EditKey::kDelete);
    EXPECT_EQ(field.text(), "b");
    EXPECT_EQ(field.cursor(), 0U);
    field.apply(EditKey::kEnd);
    EXPECT_EQ(field.cursor(), 1U);
    field.apply(EditKey::kDelete);  // Nothing after the cursor.
    EXPECT_EQ(field.text(), "b");
}

TEST(TextFieldTest, SetTextPutsCursorAtEnd) {
    TextField field;
    field.set_text("quit");
    EXPECT_EQ(field.cursor(), 4U);
    field.clear();
    EXPECT_EQ(field.text(), "");
    EXPECT_EQ(field.cursor(), 0U);
}

}  // namespace
}  // namespace manifold
