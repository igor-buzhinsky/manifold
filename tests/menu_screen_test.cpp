#include "ui/menu_screen.hpp"

#include <cstddef>
#include <optional>
#include <string>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "render/viewport.hpp"
#include "ui/game_menus.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"
#include "ui/ui_style.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 1600, .height = 900};

/// Clicks the center of button `index` of `menu`.
std::optional<UiCommand> click(MenuScreen& menu, std::size_t index) {
    const Rect rect = menu.buttons().layout(menu.buttons_placement(kPicture))[index];
    const glm::vec2 center = rect.top_left + (rect.size / 2.0f);
    menu.handle(PointerEvent{.action = PointerAction::kPress, .position = center}, kPicture);
    return menu.handle(PointerEvent{.action = PointerAction::kRelease, .position = center}, kPicture);
}

TEST(UiStyleTest, ScaleIsTheHeightOver300) {
    EXPECT_FLOAT_EQ(ui_scale({.width = 400, .height = 225}), 0.75f);
    EXPECT_FLOAT_EQ(ui_scale({.width = 800, .height = 450}), 1.5f);
    EXPECT_FLOAT_EQ(ui_scale({.width = 1600, .height = 900}), 3.0f);
    EXPECT_FLOAT_EQ(ui_scale({.width = 1920, .height = 1080}), 3.6f);
}

TEST(MenuScreenTest, PanelIsCenteredAroundTheButtons) {
    const auto menu = make_game_menu(true);
    const Rect panel = menu->panel(kPicture);
    const Placement buttons = menu->buttons_placement(kPicture);
    EXPECT_EQ(buttons.scale, 3.0f);
    // Centered to within an interface pixel.
    EXPECT_NEAR(panel.top_left.x + (panel.size.x / 2.0f), 800.0f, 3.0f);
    EXPECT_NEAR(panel.top_left.y + (panel.size.y / 2.0f), 450.0f, 3.0f);
    EXPECT_EQ(buttons.area.size.y, ButtonList::height(4, 3.0f));
    EXPECT_GE(buttons.area.size.x, MenuScreen::kMinButtonWidth * 3.0f);
    EXPECT_TRUE(panel.contains(buttons.area.top_left));
    EXPECT_TRUE(panel.contains(buttons.area.top_left + buttons.area.size - glm::vec2(1.0f)));
}

TEST(MenuScreenTest, ButtonsAreAsWideAsTheLongestLabelNeeds) {
    const MenuScreen menu(MenuConfig{
        .title = "T", .items = {{.label = std::string(60, 'x')}}, .cancel = std::nullopt, .hides_game = false});
    const Placement buttons = menu.buttons_placement(kPicture);
    EXPECT_GE(buttons.area.size.x, text_width(std::string(60, 'x'), 3.0f) + (2.0f * MenuScreen::kLabelPadding * 3.0f));
}

TEST(MenuScreenTest, GameMenuChoosesByKeysAndMouseAndEscapeResumes) {
    const auto menu = make_game_menu(true);
    EXPECT_FALSE(menu->hides_game());
    EXPECT_EQ(menu->handle(NavKey::kConfirm, kPicture), UiCommand::kResume);
    menu->handle(NavKey::kDown, kPicture);
    EXPECT_EQ(menu->handle(NavKey::kConfirm, kPicture), UiCommand::kNewGame);
    EXPECT_EQ(click(*menu, 2), UiCommand::kLoad);
    EXPECT_EQ(click(*menu, 3), UiCommand::kExit);
    EXPECT_EQ(menu->handle(NavKey::kCancel, kPicture), UiCommand::kResume);
}

TEST(MenuScreenTest, GameMenuWithoutASaveCantLoad) {
    const auto menu = make_game_menu(false);
    EXPECT_EQ(click(*menu, 2), std::nullopt);
    menu->handle(NavKey::kDown, kPicture);
    menu->handle(NavKey::kDown, kPicture);  // Over Load game to Exit game.
    EXPECT_EQ(menu->handle(NavKey::kConfirm, kPicture), UiCommand::kExit);
}

TEST(MenuScreenTest, StartMenuHidesTheGameAndCantBeLeftWithEscape) {
    const auto menu = make_start_menu(true);
    EXPECT_TRUE(menu->hides_game());
    EXPECT_EQ(menu->handle(NavKey::kCancel, kPicture), std::nullopt);
    EXPECT_EQ(menu->handle(NavKey::kConfirm, kPicture), UiCommand::kLoad);
    EXPECT_EQ(click(*menu, 1), UiCommand::kNewGame);
    EXPECT_EQ(click(*menu, 2), UiCommand::kExit);
}

TEST(MenuScreenTest, StartMenuWithoutASaveCantContinue) {
    const auto menu = make_start_menu(false);
    EXPECT_EQ(menu->buttons().focus(), 1U);
    EXPECT_EQ(click(*menu, 0), std::nullopt);
    EXPECT_EQ(menu->handle(NavKey::kConfirm, kPicture), UiCommand::kNewGame);
}

TEST(MenuScreenTest, BackgroundCoversThePicture) {
    for (const bool hides : {false, true}) {
        const MenuScreen menu(
            MenuConfig{.title = "T", .items = {{.label = "A"}}, .cancel = std::nullopt, .hides_game = hides});
        OverlayMesh mesh;
        menu.add_to(mesh, kPicture);
        ASSERT_GE(mesh.vertices().size(), 6U);
        EXPECT_EQ(mesh.vertices()[0].color, hides ? kUiBlackColor : kUiDimColor);
        glm::vec2 far_corner{0.0f};
        for (int i = 0; i < 6; ++i) {
            far_corner = glm::max(far_corner, mesh.vertices()[i].position);
        }
        EXPECT_EQ(far_corner, glm::vec2(1600.0f, 900.0f));
    }
}

}  // namespace
}  // namespace manifold
