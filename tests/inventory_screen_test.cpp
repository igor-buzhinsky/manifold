#include "ui/inventory_screen.hpp"

#include <cstddef>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <glm/common.hpp>
#include <glm/vec2.hpp>
#include <gtest/gtest.h>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "test_items.hpp"
#include "ui/item_grid.hpp"
#include "ui/item_split_dialog.hpp"
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
    const std::vector<ItemStack> items{{.item = kPebble}, {.item = kGoldBar}, {.item = kCrown},
                                       {.item = kPebble}, {.item = kGoldBar}, {.item = kCrown}};
    grid.scroll(1, items.size());
    std::vector<IconPlacement> icons;
    grid.add_icons(icons, placement, items);
    ASSERT_EQ(icons.size(), 4U);
    EXPECT_EQ(icons[0], (IconPlacement{.item = kCrown, .area = {.top_left = {2.0f, 2.0f}, .size = {36.0f, 36.0f}}}));
    EXPECT_EQ(icons[3].item, kCrown);
    EXPECT_EQ(icons[3].area.top_left, glm::vec2(44.0f, 44.0f));
}

TEST(ItemGridTest, NumbersOfStacksOfMoreThanOneItemAreAtTheBottomRightOfTheirCells) {
    const ItemGrid grid(2, 2);
    const Placement placement{.area = {.top_left = {0.0f, 0.0f}, .size = grid.size(1.0f)}, .scale = 1.0f};
    OverlayMesh single;
    grid.add_counts(single, placement, std::vector<ItemStack>{{.item = kPebble}, {.item = kCrown}});
    EXPECT_TRUE(single.vertices().empty());
    OverlayMesh counts;
    grid.add_counts(counts, placement, std::vector<ItemStack>{{.item = kPebble}, {.item = kCrown, .count = 12}});
    // The number and its shadow, in the second cell, near its bottom-right corner.
    OverlayMesh text;
    text.add_text({0.0f, 0.0f}, "12", 1.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    ASSERT_EQ(counts.vertices().size(), 2 * text.vertices().size());
    // Four characters fit into the cell.
    OverlayMesh large;
    grid.add_counts(large, placement, std::vector<ItemStack>{{.item = kPebble, .count = 9999}});
    for (const OverlayVertex& vertex : large.vertices()) {
        EXPECT_GE(vertex.position.x, 0.0f);
        EXPECT_LE(vertex.position.x, 20.0f);
    }
    // M is wider: the number fills the cell, and only its shadow reaches into the gap after it.
    OverlayMesh millions;
    grid.add_counts(millions, placement, std::vector<ItemStack>{{.item = kPebble, .count = 123456789}});
    for (const OverlayVertex& vertex : millions.vertices()) {
        EXPECT_GE(vertex.position.x, 0.0f);
        EXPECT_LE(vertex.position.x, 21.0f);
    }
    glm::vec2 low{1e9f};
    glm::vec2 high{-1e9f};
    for (const OverlayVertex& vertex : counts.vertices()) {
        low = glm::min(low, vertex.position);
        high = glm::max(high, vertex.position);
    }
    EXPECT_GE(low.x, 21.0f);
    EXPECT_GE(low.y, 10.0f);
    EXPECT_LE(high.x, 41.0f);
    EXPECT_LE(high.y, 20.0f);
    EXPECT_GE(high.x, 39.0f);
    EXPECT_GE(high.y, 18.0f);
}

TEST(ItemGridTest, LargeNumbersOfStacksAreShortened) {
    EXPECT_EQ(format_stack_count(2), "2");
    EXPECT_EQ(format_stack_count(9999), "9999");
    EXPECT_EQ(format_stack_count(12345), "12k");
    EXPECT_EQ(format_stack_count(999999), "999k");
    EXPECT_EQ(format_stack_count(1000000), "1M");
    EXPECT_EQ(format_stack_count(2147483647), "2B");
}

/// A catalog of `count` kinds of items, e.g. to fill a grid with stacks.
ItemCatalog make_many_items(std::size_t count) {
    std::vector<ItemType> items;
    items.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
        items.push_back({.key = "item_" + std::to_string(i), .name = "Item", .value = 1, .weight = 1, .chance = 1.0});
    }
    return ItemCatalog(std::move(items));
}

/// One item of each of the first `count` kinds.
Inventory make_stacks(std::size_t count) {
    Inventory inventory;
    for (std::size_t i = 0; i < count; ++i) {
        inventory.add(static_cast<ItemId>(i));
    }
    return inventory;
}

/// An inventory of these stacks, in order.
Inventory make_inventory(const std::vector<ItemStack>& stacks) {
    Inventory inventory;
    for (const ItemStack& stack : stacks) {
        inventory.add(stack.item, stack.count);
    }
    return inventory;
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
    fixture.items = make_many_items(kColumns + 2);
    fixture.party = make_stacks(kColumns + 2);
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
    fixture.items = make_many_items(200);
    fixture.party = make_stacks(200);
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
    // The pebble joins the party's stack of pebbles, which is selected.
    screen.handle(press(Fixture::cell_point(screen, 1, 7, fixture.party)), kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar}}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 1, .index = 0}));
    // The crown starts a stack at the end.
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.chest)), kPicture);
    screen.handle(press(Fixture::cell_point(screen, 1, 7, fixture.party)), kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kGoldBar}));
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar}, {.item = kCrown}}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 1, .index = 2}));
    EXPECT_EQ(screen.split_dialog(), nullptr);
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
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar}}));
    // The stack after it is selected, then the one before it once it was the last.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_FALSE(screen.selection().has_value());
    EXPECT_EQ(fixture.party,
              make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar, .count = 2}, {.item = kCrown}}));
}

TEST(InventoryScreenTest, DoubleClickMovesAnItemToTheOtherSide) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    const glm::vec2 chest_item = Fixture::cell_point(screen, 0, 1, fixture.chest);
    screen.handle(press(chest_item), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = chest_item, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar}}));
    // And from the party into the chest.
    const glm::vec2 party_item = Fixture::cell_point(screen, 1, 1, fixture.party);
    screen.handle(press(party_item), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = party_item, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.chest, make_inventory({{.item = kCrown}, {.item = kGoldBar, .count = 2}}));
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}}));
    // A third press only selects.
    const glm::vec2 pebbles = Fixture::cell_point(screen, 1, 0, fixture.party);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = pebbles, .clicks = 3}, kPicture);
    EXPECT_EQ(fixture.party.size(), 1U);
    EXPECT_EQ(screen.split_dialog(), nullptr);
}

TEST(InventoryScreenTest, DoubleClickNeedsBothPressesOnTheSameItem) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    // Quick presses on two neighbouring items, which SDL may count as a double click, only select.
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.chest)), kPicture);
    screen.handle(
        PointerEvent{
            .action = PointerAction::kPress, .position = Fixture::cell_point(screen, 0, 1, fixture.chest), .clicks = 2},
        kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kPebble, kGoldBar}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
    // A double click on an empty cell moves the selected item there and leaves it there.
    const glm::vec2 empty = Fixture::cell_point(screen, 1, 2, fixture.party);
    screen.handle(press(empty), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = empty, .clicks = 2}, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kCrown, kGoldBar}));
    EXPECT_EQ(fixture.party, make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar}}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 1, .index = 0}));
}

TEST(InventoryScreenTest, ThePressThatOpenedTheScreenIsNoFirstClick) {
    Fixture fixture;
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    // The first press of a double click on the chest in the world opened the screen; the second only selects.
    screen.handle(
        PointerEvent{
            .action = PointerAction::kPress, .position = Fixture::cell_point(screen, 0, 0, fixture.chest), .clicks = 2},
        kPicture);
    EXPECT_EQ(fixture.chest.size(), 3U);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
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
    EXPECT_EQ(fixture.party,
              make_inventory({{.item = kPebble, .count = 2}, {.item = kGoldBar, .count = 2}, {.item = kCrown}}));
    EXPECT_FALSE(screen.selection().has_value());
    // Stacks of more than one item are moved without asking.
    fixture.chest = make_inventory({{.item = kCrown, .count = 4}});
    screen.handle(NavKey::kTakeAll, kPicture);
    EXPECT_EQ(screen.split_dialog(), nullptr);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_EQ(fixture.party.stacks()[2], (ItemStack{.item = kCrown, .count = 5}));
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

TEST(InventoryScreenTest, EnterOnAStackOfManyAsksHowManyToMove) {
    Fixture fixture;
    fixture.chest = make_inventory({{.item = kCrown, .count = 3}, {.item = kPebble}});
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    screen.handle(NavKey::kDown, kPicture);
    screen.handle(NavKey::kConfirm, kPicture);
    // Nothing moves before the dialog is answered, and it gets the keys.
    const ItemSplitDialog* dialog = screen.split_dialog();
    ASSERT_NE(dialog, nullptr);
    EXPECT_EQ(dialog->max_count(), 3);
    EXPECT_EQ(dialog->amount(), 3);
    EXPECT_EQ(fixture.chest.item_count(), 4);
    screen.handle(NavKey::kLeft, kPicture);
    screen.handle(NavKey::kLeft, kPicture);
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    EXPECT_EQ(screen.split_dialog()->amount(), 1);
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(screen.split_dialog(), nullptr);
    EXPECT_EQ(fixture.chest, make_inventory({{.item = kCrown, .count = 2}, {.item = kPebble}}));
    EXPECT_EQ(fixture.party, Inventory({kPebble, kGoldBar, kCrown}));
    // The rest stays selected, and all of it moves by default, after which the next stack is selected.
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
    screen.handle(NavKey::kConfirm, kPicture);
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(fixture.chest, Inventory({kPebble}));
    EXPECT_EQ(fixture.party.stacks()[2], (ItemStack{.item = kCrown, .count = 3}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 0}));
}

TEST(InventoryScreenTest, AClickOnTheOtherGridAsksHowManyAndSelectsTheMovedItems) {
    Fixture fixture;
    fixture.party = make_inventory({{.item = kPebble, .count = 5}, {.item = kGoldBar}});
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    screen.handle(press(Fixture::cell_point(screen, 1, 0, fixture.party)), kPicture);
    const glm::vec2 empty = Fixture::cell_point(screen, 0, 5, fixture.chest);
    // Escape closes only the dialog, and moves nothing.
    screen.handle(press(empty), kPicture);
    ASSERT_NE(screen.split_dialog(), nullptr);
    EXPECT_FALSE(screen.handle(NavKey::kCancel, kPicture).has_value());
    EXPECT_EQ(screen.split_dialog(), nullptr);
    EXPECT_EQ(fixture.party.item_count(), 6);
    // The Move button with two less than all.
    screen.handle(press(empty), kPicture);
    ASSERT_NE(screen.split_dialog(), nullptr);
    const ItemSplitDialog& dialog = *screen.split_dialog();
    screen.handle(press(middle(dialog.minus_button(kPicture))), kPicture);
    screen.handle(press(middle(dialog.minus_button(kPicture))), kPicture);
    screen.handle(press(middle(dialog.move_button(kPicture))), kPicture);
    EXPECT_EQ(screen.split_dialog(), nullptr);
    EXPECT_EQ(fixture.party.stacks()[0], (ItemStack{.item = kPebble, .count = 2}));
    EXPECT_EQ(fixture.chest.stacks()[1], (ItemStack{.item = kPebble, .count = 4}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
}

TEST(InventoryScreenTest, StacksAreNotSplitWithinOneInventory) {
    Fixture fixture;
    fixture.party = make_inventory({{.item = kPebble, .count = 5}, {.item = kGoldBar}});
    fixture.chest = make_inventory({{.item = kCrown, .count = 2}, {.item = kGoldBar}});
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Wooden chest");
    // An empty cell of the same grid takes the whole stack to its end.
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.chest)), kPicture);
    screen.handle(press(Fixture::cell_point(screen, 0, 4, fixture.chest)), kPicture);
    EXPECT_EQ(screen.split_dialog(), nullptr);
    EXPECT_EQ(fixture.chest, make_inventory({{.item = kGoldBar}, {.item = kCrown, .count = 2}}));
    EXPECT_EQ(screen.selection(), (ItemSelection{.side = 0, .index = 1}));
    // Without a container, there is nothing to split.
    InventoryScreen alone(fixture.items, fixture.party);
    alone.handle(press(Fixture::cell_point(alone, 0, 0, fixture.party)), kPicture);
    alone.handle(
        PointerEvent{
            .action = PointerAction::kPress, .position = Fixture::cell_point(alone, 0, 0, fixture.party), .clicks = 2},
        kPicture);
    alone.handle(NavKey::kConfirm, kPicture);
    EXPECT_EQ(alone.split_dialog(), nullptr);
    alone.handle(press(Fixture::cell_point(alone, 0, 5, fixture.party)), kPicture);
    EXPECT_EQ(alone.split_dialog(), nullptr);
    EXPECT_EQ(fixture.party, make_inventory({{.item = kGoldBar}, {.item = kPebble, .count = 5}}));
}

TEST(InventoryScreenTest, DoubleClickOnAStackOfManyAsksAndTheInventoryKeyCloses) {
    Fixture fixture;
    fixture.chest = make_inventory({{.item = kCrown, .count = 2}});
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Golden chest");
    const glm::vec2 crowns = Fixture::cell_point(screen, 0, 0, fixture.chest);
    screen.handle(press(crowns), kPicture);
    screen.handle(PointerEvent{.action = PointerAction::kPress, .position = crowns, .clicks = 2}, kPicture);
    ASSERT_NE(screen.split_dialog(), nullptr);
    // The dialog is drawn over the icons.
    OverlayMesh with_dialog;
    screen.add_over_icons(with_dialog, kPicture);
    OverlayMesh dialog;
    screen.split_dialog()->add_to(dialog, kPicture);
    EXPECT_GT(with_dialog.vertices().size(), dialog.vertices().size());
    EXPECT_EQ(screen.handle(NavKey::kInventory, kPicture), UiCommand::kResume);
}

TEST(InventoryScreenTest, ADialogWhoseStackWasTakenElsewhereCloses) {
    Fixture fixture;
    fixture.chest = make_inventory({{.item = kCrown, .count = 2}});
    InventoryScreen screen(fixture.items, fixture.party, fixture.chest, "Golden chest");
    screen.handle(NavKey::kDown, kPicture);
    screen.handle(NavKey::kConfirm, kPicture);
    ASSERT_NE(screen.split_dialog(), nullptr);
    fixture.chest.take(0);
    fixture.chest.add(kPebble);
    EXPECT_EQ(screen.split_dialog(), nullptr);
    // The next event goes to the screen.
    EXPECT_EQ(screen.handle(NavKey::kCancel, kPicture), UiCommand::kResume);
    // A stack that lost some items moves at most what it has.
    fixture.chest = make_inventory({{.item = kCrown, .count = 3}});
    screen.handle(NavKey::kDown, kPicture);
    screen.handle(NavKey::kConfirm, kPicture);
    ASSERT_NE(screen.split_dialog(), nullptr);
    fixture.chest.take(0, 1);
    screen.handle(NavKey::kConfirm, kPicture);
    EXPECT_TRUE(fixture.chest.empty());
    EXPECT_EQ(fixture.party.stacks()[2], (ItemStack{.item = kCrown, .count = 2}));
}

TEST(InventoryScreenTest, TheLineOfTheSelectedStackHasItsNumberAndWeight) {
    Fixture fixture;
    fixture.party = make_inventory({{.item = kPebble, .count = 12}});
    InventoryScreen screen(fixture.items, fixture.party);
    OverlayMesh before;
    screen.add_to(before, kPicture);
    screen.handle(press(Fixture::cell_point(screen, 0, 0, fixture.party)), kPicture);
    OverlayMesh after;
    screen.add_to(after, kPicture);
    OverlayMesh name;
    name.add_text({0.0f, 0.0f}, "Pebble x12, weight 3.000", 1.0f, {1.0f, 1.0f, 1.0f, 1.0f});
    OverlayMesh border;
    border.add_rectangle({0.0f, 0.0f}, {1.0f, 1.0f}, {1.0f, 1.0f, 1.0f, 1.0f});
    EXPECT_EQ(after.vertices().size(), before.vertices().size() + name.vertices().size() + border.vertices().size());
    // The number is drawn over the icon.
    OverlayMesh counts;
    screen.add_over_icons(counts, kPicture);
    EXPECT_FALSE(counts.vertices().empty());
}

}  // namespace
}  // namespace manifold
