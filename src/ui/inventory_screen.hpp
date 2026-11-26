#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "ui/item_grid.hpp"
#include "ui/overlay_mesh.hpp"
#include "ui/rect.hpp"
#include "ui/screen.hpp"
#include "ui/ui_event.hpp"

namespace manifold {

/// A selected item of an InventoryScreen.
struct ItemSelection {
    std::size_t side = 0;   ///< Which grid: see InventoryScreen::side_count().
    std::size_t index = 0;  ///< Which item of its inventory.

    bool operator==(const ItemSelection&) const = default;
};

/// A screen over the dimmed game with the party's items in a grid, see ItemGrid, and, when a container such as a chest
/// is open, the container's items in a grid to the left of it, each narrower. The game is paused while it is open.
///
/// - A click on an item selects it, and the line at the bottom shows its name and weight. A click on an empty cell
///   then moves the selected item to the end of that grid's inventory, which may be the same one, and the items after
///   its old place move up; the item stays selected.
/// - With a container open, a double click on an item moves it to the end of the other inventory, as Enter does
///   with the selected item. Without one, it only selects the item. Only a press on the item that the press before
///   selected counts as the second of a double click.
/// - The arrow keys move the selection in its grid, and Left and Right also from one grid to the other at their
///   edges. With nothing selected, they select the first item. Enter moves the selected item to the end of the other
///   inventory, and selects the item that comes into its place.
/// - The mouse wheel scrolls the grid under the pointer, and the keys scroll to the selected item.
/// - The Take all button, or T, moves all items of the container to the end of the party's, in their order.
/// - Escape or I closes the screen (UiCommand::kResume).
///
/// The party's grid shows the total weight of its items, see format_weight(); with a container open, the Take all
/// button is below the container's. Values of items are never shown.
class InventoryScreen : public Screen {
public:
    /// Sizes at scale 1, in interface pixels, see Placement::scale.
    static constexpr float kPadding = 10.0f;  ///< Between the edge of the panel and what is in it.
    static constexpr float kGap = 4.0f;       ///< Between the rows of the panel: titles, grids, buttons and text.
    static constexpr float kSideGap = 12.0f;  ///< Between two grids.
    static constexpr float kButtonHeight = 14.0f;
    static constexpr float kLabelPadding = 6.0f;  ///< Left and right of the label in a button.

    /// Cells of the grid of the party alone, see ItemGrid.
    static constexpr std::size_t kColumns = 12;
    static constexpr std::size_t kRows = 6;
    /// Cells of each grid when a container is open.
    static constexpr std::size_t kContainerColumns = 7;
    static constexpr std::size_t kContainerRows = 6;

    /// The label of the button that takes all items of the container.
    static constexpr const char* kTakeAllLabel = "Take all";

    /// Shows the party's items alone, titled "Inventory".
    ///
    /// \param catalog The kinds of items, for their names and weights. It must outlive the screen.
    /// \param party   The party's items, which the screen changes. It must outlive the screen.
    InventoryScreen(const ItemCatalog& catalog, Inventory& party);

    /// Shows the items of a container, titled `container_title`, on the left, and the party's on the right.
    ///
    /// \param container What the container holds, which the screen changes. It must outlive the screen.
    InventoryScreen(const ItemCatalog& catalog, Inventory& party, Inventory& container, std::string container_title);

    std::optional<UiCommand> handle(const UiEvent& event, const PixelSize& picture) override;
    void add_to(OverlayMesh& mesh, const PixelSize& picture) const override;
    void add_icons(std::vector<IconPlacement>& icons, const PixelSize& picture) const override;
    bool hides_game() const override { return false; }
    /// True, also for the party alone: the inventories of the level's containers are replaced by another level, and
    /// opening an inventory closes the one that is open (see ScreenStack::close_level_screens()) rather than stacking
    /// another over it.
    bool shows_level() const override { return true; }

    /// How many grids there are: 1 for the party alone, 2 with a container, which is then side 0 and the party side 1.
    std::size_t side_count() const { return sides_.size(); }
    /// The side of the party's grid.
    std::size_t party_side() const { return sides_.size() - 1; }
    /// The grid of `side`, e.g. to see which rows it shows.
    ///
    /// \throws std::out_of_range if there is no such side.
    const ItemGrid& grid(std::size_t side) const { return sides_.at(side).grid; }

    /// The selected item, if any. A selection that the inventory no longer has, e.g. after a console command took
    /// items, counts as none.
    std::optional<ItemSelection> selection() const;

    /// The rectangle of the panel over a picture of `picture` size.
    Rect panel(const PixelSize& picture) const;
    /// Where grid `side` is over a picture of `picture` size.
    Placement grid_placement(std::size_t side, const PixelSize& picture) const;
    /// The Take all button over a picture of `picture` size, or nothing without a container.
    std::optional<Rect> take_all_button(const PixelSize& picture) const;

private:
    /// A grid and the inventory it shows.
    struct Side {
        Inventory* inventory;
        std::string title;
        ItemGrid grid;
    };

    /// Handles a key, see the class description.
    std::optional<UiCommand> handle_key(NavKey key);
    /// Scrolls the grid under the pointer.
    void handle_scroll(const ScrollEvent& scroll, const PixelSize& picture);
    /// Selects or moves items with a click, or takes all with the button.
    void handle_pointer(const PointerEvent& pointer, const PixelSize& picture);
    /// Moves the selection by a key, see the class description.
    void navigate(NavKey key);
    /// Moves the selected item to the end of the other inventory, and selects the item that comes into its place.
    void move_selected_across();
    /// Moves the selected item to the end of the inventory of `side`, and selects it there.
    void move_selected_to(std::size_t side);
    /// Moves all items of the container to the end of the party's.
    void take_all();
    /// Selects `selection`, and scrolls its grid to it.
    void select(ItemSelection selection);

    /// The size of the content of the panel, without the padding, in pixels of the picture.
    glm::vec2 content_size(float scale) const;

    const ItemCatalog* catalog_;
    std::vector<Side> sides_;
    std::optional<ItemSelection> selection_;
    bool take_all_hovered_ = false;  ///< Whether the pointer is over the Take all button.
    /// The cell of the last press of the main button, if it selected the item there, which a double click needs.
    std::optional<ItemSelection> last_selecting_press_;
};

}  // namespace manifold
