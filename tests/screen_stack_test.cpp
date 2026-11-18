#include "ui/screen_stack.hpp"

#include <memory>
#include <optional>

#include <gtest/gtest.h>

#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 800, .height = 450};

/// A screen that answers every event with its command and draws one rectangle.
class FakeScreen : public Screen {
public:
    FakeScreen(UiCommand command, bool hides) : command_(command), hides_(hides) {}

    std::optional<UiCommand> handle(const UiEvent& /*event*/, const PixelSize& /*picture*/) override {
        return command_;
    }
    void add_to(OverlayMesh& mesh, const PixelSize& /*picture*/) const override {
        mesh.add_rectangle({0.0f, 0.0f}, {1.0f, 1.0f}, {static_cast<float>(command_), 0.0f, 0.0f, 1.0f});
    }
    bool hides_game() const override { return hides_; }

private:
    UiCommand command_;
    bool hides_;
};

TEST(ScreenStackTest, EmptyStackHandlesNothing) {
    ScreenStack stack;
    EXPECT_TRUE(stack.empty());
    EXPECT_FALSE(stack.hides_game());
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), std::nullopt);
    EXPECT_TRUE(stack.build_mesh(kPicture).vertices().empty());
    stack.pop();  // Does nothing.
    EXPECT_TRUE(stack.empty());
}

TEST(ScreenStackTest, TopScreenGetsEventsAndAllAreDrawnBottomFirst) {
    ScreenStack stack;
    stack.push(std::make_unique<FakeScreen>(UiCommand::kLoad, true));
    stack.push(std::make_unique<FakeScreen>(UiCommand::kExit, false));
    EXPECT_EQ(stack.size(), 2U);
    EXPECT_TRUE(stack.hides_game());
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), UiCommand::kExit);
    const OverlayMesh mesh = stack.build_mesh(kPicture);
    ASSERT_EQ(mesh.vertices().size(), 12U);
    EXPECT_EQ(mesh.vertices()[0].color.r, static_cast<float>(UiCommand::kLoad));
    EXPECT_EQ(mesh.vertices()[6].color.r, static_cast<float>(UiCommand::kExit));

    stack.pop();
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), UiCommand::kLoad);
    stack.clear();
    EXPECT_TRUE(stack.empty());
}

}  // namespace
}  // namespace manifold
