#include "ui/inventory_screen.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "test_items.hpp"
#include "ui/item_grid.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {
namespace {

constexpr PixelSize kPicture{.width = 800, .height = 450};

/// The middle of a rectangle.
glm::vec2 middle(const Rect& rect) { return rect.top_left + (rect.size / 2.0f); }

TEST(ItemGridTest, HasRowsForAllItemsAndAnEmptyCellAfterThem) {
    const ItemGrid grid(4, 3);
    EXPECT_EQ(grid.rows(0), 3U);
    EXPECT_EQ(grid.rows(11), 3U);
    EXPECT_EQ(grid.rows(12), 4U);
    EXPECT_EQ(grid.rows(13), 4U);
    EXPECT_EQ(grid.size(2.0f), glm::vec2(((4.0f * 20.0f) + 3.0f + 2.0f + 3.0f) * 2.0f, ((3.0f * 20.0f) + 2.0f) * 2.0f));
}

TEST(ItemGridTest, ScrollsByRowsWithinTheItems) {
    ItemGrid grid(4, 3);
    grid.scroll(1, 11);
    EXPECT_EQ(grid.first_row(11), 0U);  // All rows are shown.
    grid.scroll(5, 30);
    EXPECT_EQ(grid.first_row(30), 5U);  // 8 rows, the last 3 shown.
    // When items go, the grid shows the last rows that it still has.
    EXPECT_EQ(grid.first_row(20), 3U);
    grid.scroll(-10, 30);
    EXPECT_EQ(grid.first_row(30), 0U);
    grid.scroll_to(17, 30);
    EXPECT_EQ(grid.first_row(30), 2U);
    grid.scroll_to(9, 30);
    EXPECT_EQ(grid.first_row(30), 2U);
    grid.scroll_to(3, 30);
    EXPECT_EQ(grid.first_row(30), 0U);
}

TEST(ItemGridTest, CellsAreFoundUnderPointsOfTheRowsShown) {
    ItemGrid grid(4, 3);
    const Placement placement{.area = {.top_left = {10.0f, 20.0f}, .size = grid.size(1.0f)}, .scale = 1.0f};
    EXPECT_EQ(grid.cell_at({10.0f, 20.0f}, 0, placement), 0U);
    EXPECT_EQ(grid.cell_at({10.0f + 21.0f + 5.0f, 20.0f + 42.0f + 5.0f}, 0, placement), 9U);
    EXPECT_FALSE(grid.cell_at({10.0f + 20.5f, 25.0f}, 0, placement).has_value());  // In the gap.
    EXPECT_FALSE(grid.cell_at({9.0f, 25.0f}, 0, placement).has_value());
    EXPECT_FALSE(grid.cell_at({10.0f + 84.0f, 25.0f}, 0, placement).has_value());  // Beyond the last column.
    grid.scroll(2, 20);
    EXPECT_EQ(grid.cell_at({15.0f, 25.0f}, 20, placement), 8U);
    EXPECT_FALSE(grid.cell_rect(7, 20, placement).has_value());
    ASSERT_TRUE(grid.cell_rect(8, 20, placement).has_value());
    EXPECT_EQ(grid.cell_rect(8, 20, placement)->top_left, glm::vec2(10.0f, 20.0f));
}

TEST(ItemGridTest, IconsOfTheItemsShownAreInsideTheirCells) {
    ItemGrid grid(2, 2);
    const Placement placement{.area = {.top_left = {0.0f, 0.0f}, .size = grid.size(2.0f)}, .scale = 2.0f};
    const std::vector<ItemId> items{kPebble, kGoldBar, kCrown, kPebble, kGoldBar, kCrown};
    grid.scroll(1, items.size());
    std::vector<IconPlacement> icons;
    grid.add_icons(icons, placement, items);
    ASSERT_EQ(icons.size(), 4U);
    EXPECT_EQ(icons[0], (IconPlacement{.item = kCrown, .area = {.top_left = {2.0f, 2.0f}, .size = {36.0f, 36.0f}}}));
    EXPECT_EQ(icons[3].item, kCrown);
    EXPECT_EQ(icons[3].area.top_left, glm::vec2(44.0f, 44.0f));
}

/// The inventories of a party and a chest, and the kinds of their items.
struct Fixture {
    ItemCatalog items = make_test_items();
    Inventory party{{kPebble, kGoldBar}};
    Inventory chest{{kCrown, kPebble, kGoldBar}};

    /// The point in the middle of cell `cell` of grid `side` of `screen`.
    static glm::vec2 cell_point(const InventoryScreen& screen, std::size_t side, std::size_t cell,
                                const Inventory& inventory) {
        const std::optional<Rect> rect =
            screen.grid(side).cell_rect(cell, inventory.size(), screen.grid_placement(side, kPicture));
        return rect ? middle(*rect) : glm::vec2(-1.0f);
    }
};

/// A press of the main button at `point`.
UiEvent press(const glm::vec2& point) { return PointerEvent{.action = PointerAction::kPress, .position = point}; }

TEST(InventoryScreenTest, ClickSelectsAnItemAndAClickOnAnEmptyCellMovesItToTheEnd) {
    Fixture fixture;
    fixture.party = Inventory({kPebble, kGoldBar, kCrown});
    InventoryScreen screen(fixture.items, fixture.party);
    EXPECT_EQ(screen.side_count(), 1U);
    EXPECT_FALSE(screen.selection().has_value());
    // An empty cell does nothing without a selection.
    screen.handle(press(Fixture::cell_point(screen, 0, 5, fixture.party)), kPicture);
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kCrown}));
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.party)), kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(press(Fixture::cell_point(screen, 0, 10, fixture.party)), kPicture);
    EXPECT_EQ(fixture.party, Inventory({kGoldBar, kCrown, kPebble}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 2}));
}

TEST(InventoryScreenTest, SelectedItemsNameAndWeightAndTheTotalWeightAreShown) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party);
    // The text is drawn as quads, so its presence is seen from the number of vertices.
    OverlayMesh before;
    screen.add_to(before, kPicture);
    screen.handle(press(Fixture::cell_point(screen, 0, 1, fixture.party)), kPicture);
    OverlayMesh after;
    screen.add_to(after, kPicture);
    OverlayMesh name;
    name.add_text({0.0f, 0.0f}, "Gold bar, weight 1.000", 1.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    OverlayMesh border;
    border.add_rectangle({0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    EXPECT_EQ(after.vertices().size(), before.vertices().size() + name.vertices().size() + border.vertices().size());
}

TEST(InventoryScreenTest, ArrowKeysMoveTheSelectionWithinTheItems) {
    constexpr std::size_t kColumns = InventoryScreen::kColumns;
    // A full row and two items in the next.
    Fixture fixture;
    fixture.party = Inventory(std::vector<ItemId>(kColumns + 2, kPebble));
    InventoryScreen screen(fixture.items, fixture.party);
    screen.handle(NavKey::kRight, kPicture);  // Selects the first item.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(NavKey::kLeft, kPicture);
    EXPECT_EQ(screen.selection()->index, 0U);
    screen.handle(NavKey::kDown, kPicture);
    EXPECT_EQ(screen.selection()->index, kColumns);
    screen.handle(NavKey::kRight, kPicture);
    screen.handle(NavKey::kRight, kPicture);  // There is no item after the last.
    EXPECT_EQ(screen.selection()->index, kColumns + 1);
    screen.handle(NavKey::kUp, kPicture);
    EXPECT_EQ(screen.selection()->index, 1U);
    screen.handle(NavKey::kUp, kPicture);
    EXPECT_EQ(screen.selection()->index, 1U);
    // Down from a cell above the last row goes to the last item when there is none below.
    for (int i = 0; i < 5; ++i) {
        screen.handle(NavKey::kRight, kPicture);
    }
    EXPECT_EQ(screen.selection()->index, 6U);
    screen.handle(NavKey::kDown, kPicture);
    EXPECT_EQ(screen.selection()->index, kColumns + 1);
    // Enter does nothing without a container.
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(fixture.party.size(), kColumns + 2);
}

TEST(InventoryScreenTest, KeysScrollToTheSelectionAndTheWheelScrollsTheGridUnderThePointer) {
    Fixture fixture;
    fixture.party = Inventory(std::vector<ItemId>(200, kPebble));
    InventoryScreen screen(fixture.items, fixture.party);
    screen.handle(NavKey::kDown, kPicture);
    for (int i = 0; i < 7; ++i) {
        screen.handle(NavKey::kDown, kPicture);
    }
    EXPECT_EQ(screen.selection()->index, 7 * InventoryScreen::kColumns);
    // Row 7 is shown at the bottom of the 6 rows.
    EXPECT_EQ(Fixture::cell_point(screen, 0, 0, fixture.party), glm::vec2(-1.0f));
    const glm::vec2 grid = middle(screen.grid_placement(0, kPicture).area);
    screen.handle(ScrollEvent{.steps = 1.0f, .position = grid}, kPicture);
    screen.handle(ScrollEvent{.steps = 0.25f, .position = grid}, kPicture);
    EXPECT_NE(Fixture::cell_point(screen, 0, 0, fixture.party), glm::vec2(-1.0f));
    screen.handle(ScrollEvent{.steps = -1.0f, .position = {0.0f, 0.0f}}, kPicture);  // Not over the grid.
    EXPECT_NE(Fixture::cell_point(screen, 0, 0, fixture.party), glm::vec2(-1.0f));
}

TEST(InventoryScreenTest, EscapeAndTheInventoryKeyClose) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party);
    EXPECT_EQ(screen.handle(NavKey::kCancel, kPicture), UiCommand::kResume);
    EXPECT_EQ(screen.handle(NavKey::kInventory, kPicture), UiCommand::kResume);
    EXPECT_FALSE(screen.hides_game());
    EXPECT_TRUE(screen.shows_level());
    EXPECT_FALSE(screen.take_all_button(kPicture).has_value());
}

TEST(InventoryScreenTest, ClicksMoveItemsBetweenTheChestAndTheParty) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    ASSERT_EQ(screen.side_count(), 2U);
    EXPECT_EQ(screen.party_side(), 1U);
    // The chest is on the left.
    EXPECT_LT(screen.grid_placement(0, kPicture).area.top_left.x, screen.grid_placement(1, kPicture).area.top_left.x);
    screen.handle(press(Fixture::cell_point(screen, 0, 1, fixture.chest)), kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
    screen.handle(press(Fixture::cell_point(screen, 1, 7, fixture.party)), kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kPebble}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 1, .index = 2}));
    // And back.
    screen.handle(press(Fixture::cell_point(screen, 1, 0, fixture.party)), kPicture);
    screen.handle(press(Fixture::cell_point(screen, 0, 4, fixture.chest)), kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar, kPebble}));
    EXPECT_EQ(fixture.party, Inventory({kGoldBar, kPebble}));
}

TEST(InventoryScreenTest, KeysMoveTheSelectionAcrossAndEnterMovesTheItemToTheOtherSide) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Golden chest");
    screen.handle(NavKey::kDown, kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(NavKey::kRight, kPicture);
    screen.handle(NavKey::kRight, kPicture);
    screen.handle(NavKey::kRight, kPicture);  // From the last item of the chest to the party's first.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 1, .index = 0}));
    screen.handle(NavKey::kLeft, kPicture);  // Back to the chest's item nearest to the last column.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 2}));
    screen.handle(NavKey::kLeft, kPicture);
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kPebble}));
    // The item after it is selected, then the one before it once it was the last.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_FALSE(screen.selection().has_value());
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kPebble, kGoldBar, kCrown}));
}

TEST(InventoryScreenTest, DoubleClickMovesAnItemToTheOtherSide) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    const glm::vec2 chest_item = Fixture::cell_point(screen, 0, 1, fixture.chest);
    screen.handle(press(chest_item), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = chest_item, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kPebble}));
    // And from the party into the chest.
    const glm::vec2 party_item = Fixture::cell_point(screen, 1, 0, fixture.party);
    screen.handle(press(party_item), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = party_item, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar, kPebble}));
    EXPECT_EQ(fixture.party, Inventory({kGoldBar, kPebble}));
    // A third press only selects, and a fourth is a double click again.
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = party_item, .clicks = 3}, kPicture);
    EXPECT_EQ(fixture.party.size(), 2U);
}

TEST(InventoryScreenTest, DoubleClickWithoutAContainerOnlySelects) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party);
    const glm::vec2 item = Fixture::cell_point(screen, 0, 0, fixture.party);
    screen.handle(press(item), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = item, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
}

TEST(InventoryScreenTest, TakeAllMovesTheChestsItemsInTheirOrder) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.chest)), kPicture);
    const std::optional<Rect> button = screen.take_all_button(kPicture);
    ASSERT_TRUE(button.has_value());
    screen.handle(press(middle(*button)), kPicture);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kCrown, kPebble, kGoldBar}));
    EXPECT_FALSE(screen.selection().has_value());
    fixture.chest = Inventory({kCrown});
    screen.handle(NavKey::kTakeAll, kPicture);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_EQ(fixture.party.size(), 6U);
}

TEST(InventoryScreenTest, SelectionOfAnItemThatWasTakenElsewhereIsNone) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party);
    screen.handle(press(Fixture::cell_point(screen, 0, 1, fixture.party)), kPicture);
    ASSERT_TRUE(screen.selection().has_value());
    fixture.party.take(1);
    EXPECT_FALSE(screen.selection().has_value());
    OverlayMesh mesh;
    screen.add_to(mesh, kPicture);  // Doesn't show the item that is gone.
}

TEST(InventoryScreenTest, PanelIsCenteredAndHoldsTheGridsAndTheButton) {
    Fixture fixture;
    const InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    const Rect panel = screen.panel(kPicture);
    EXPECT_NEAR(panel.top_left.x + (panel.size.x / 2.0f), 400.0f, 2.0f);
    EXPECT_NEAR(panel.top_left.y + (panel.size.y / 2.0f), 225.0f, 2.0f);
    const auto inside = [&panel](const Rect& rect) {
        return rect.top_left.x >= panel.top_left.x && rect.top_left.y >= panel.top_left.y &&
               rect.top_left.x + rect.size.x <= panel.top_left.x + panel.size.x &&
               rect.top_left.y + rect.size.y <= panel.top_left.y + panel.size.y;
    };
    EXPECT_TRUE(inside(screen.grid_placement(0, kPicture).area));
    EXPECT_TRUE(inside(screen.grid_placement(1, kPicture).area));
    EXPECT_TRUE(inside(*screen.take_all_button(kPicture)));
    std::vector<IconPlacement> icons;
    screen.add_icons(icons, kPicture);
    EXPECT_EQ(icons.size(), 5U);
}

}  // namespace
}  // namespace manifold
