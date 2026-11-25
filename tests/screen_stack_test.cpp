#include "ui/screen_stack.hpp"

#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 800, .height = 450};

/// A screen that answers every event with its command, draws one rectangle and the icon of item `command`, and shows
/// the level if `level` is set.
class FakeScreen : public Screen {
public:
    FakeScreen(UiCommand command, bool hides, bool level = false) : command_(command), hides_(hides), level_(level) {}

    std::optional<UiCommand> handle(const UiEvent& /*event*/, const PixelSize& /*picture*/) override {
        return command_;
    }
    void add_to(OverlayMesh& mesh, const PixelSize& /*picture*/) const override {
        mesh.add_rectangle({0.0f, 0.0f}, {1.0f, 1.0f}, {static_cast<float>(command_), 0.0f, 0.0f, 1.0f});
    }
    bool hides_game() const override { return hides_; }
    void add_icons(std::vector<IconPlacement>& icons, const PixelSize& /*picture*/) const override {
        icons.push_back({.item = static_cast<ItemId>(command_), .area = {}});
    }
    bool shows_level() const override { return level_; }

private:
    UiCommand command_;
    bool hides_;
    bool level_;
};

TEST(ScreenStackTest, EmptyStackHandlesNothing) {
    ScreenStack stack;
    EXPECT_TRUE(stack.empty());
    EXPECT_FALSE(stack.hides_game());
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), std::nullopt);
    EXPECT_TRUE(stack.build_layers(kPicture).empty());
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
    const std::vector<ScreenLayer> layers = stack.build_layers(kPicture);
    ASSERT_EQ(layers.size(), 2U);
    EXPECT_EQ(layers[0].mesh.vertices()[0].color.r, static_cast<float>(UiCommand::kLoad));
    EXPECT_EQ(layers[1].mesh.vertices()[0].color.r, static_cast<float>(UiCommand::kExit));

    stack.pop();
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), UiCommand::kLoad);
    stack.clear();
    EXPECT_TRUE(stack.empty());
}

TEST(ScreenStackTest, EachLayerHasTheShapesAndIconsOfItsScreenBottomFirst) {
    ScreenStack stack;
    stack.push(std::make_unique<FakeScreen>(UiCommand::kLoad, false));
    stack.push(std::make_unique<FakeScreen>(UiCommand::kExit, false));
    const std::vector<ScreenLayer> layers = stack.build_layers(kPicture);
    ASSERT_EQ(layers.size(), 2U);
    EXPECT_EQ(layers[0].mesh.vertices().size(), 6U);
    EXPECT_EQ(layers[0].mesh.vertices()[0].color.r, static_cast<float>(UiCommand::kLoad));
    ASSERT_EQ(layers[1].icons.size(), 1U);
    EXPECT_EQ(layers[1].icons[0].item, static_cast<ItemId>(UiCommand::kExit));
}

TEST(ScreenStackTest, ClosingLevelScreensKeepsTheOthersInOrder) {
    ScreenStack stack;
    stack.push(std::make_unique<FakeScreen>(UiCommand::kLoad, false));
    stack.push(std::make_unique<FakeScreen>(UiCommand::kResume, false, true));
    stack.push(std::make_unique<FakeScreen>(UiCommand::kExit, false));
    stack.push(std::make_unique<FakeScreen>(UiCommand::kNewGame, false, true));
    stack.close_level_screens();
    ASSERT_EQ(stack.size(), 2U);
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), UiCommand::kExit);
    stack.pop();
    EXPECT_EQ(stack.handle(NavKey::kConfirm, kPicture), UiCommand::kLoad);
}

}  // namespace
}  // namespace manifold
