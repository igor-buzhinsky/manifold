#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

#include "items/inventory.hpp"
#include "items/item_catalog.hpp"
#include "render/viewport.hpp"
#include "ui/item_grid.hpp"
#include "ui/item_split_dialog.hpp"
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
/// Items of a kind are in one stack, see Inventory, which takes one cell and shows its number of items.
///
/// - A click on a stack selects it, and the line at the bottom shows its name, number and weight. A click on an empty
///   cell then moves the selected stack to the end of that grid's inventory, which may be the same one, and the stacks
///   after its old place move up; the stack stays selected. In the other inventory, the items join the stack of their
///   kind if there is one, which is then selected.
/// - With a container open, a double click on a stack moves it to the other inventory, as Enter does with the selected
///   stack. Without one, it only selects the stack. Only a press on the stack that the press before selected counts
///   as the second of a double click. The second press of a double click on an empty cell does nothing.
/// - A move of a stack of more than one item to the other inventory, by any of these, first opens an ItemSplitDialog
///   over the screen, which asks how many of them to move, all by default. Without a container, stacks can't be
///   split. While the dialog is open, it gets all events except I, which closes the screen.
/// - The arrow keys move the selection in its grid, and Left and Right also from one grid to the other at their
///   edges. With nothing selected, they select the first stack. Enter moves the selected stack to the other
///   inventory, and selects the stack that comes into its place, or keeps it selected if only some of its items moved.
/// - The mouse wheel scrolls the grid under the pointer, and the keys scroll to the selected stack.
/// - The Take all button, or T, moves all items of the container to the party's, in their order, without asking.
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
    /// The numbers of items of the stacks, and the ItemSplitDialog if it is open, over a dimmed screen.
    void add_over_icons(OverlayMesh& mesh, const PixelSize& picture) const override;
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

    /// The selected stack, if any. A selection that the inventory no longer has, e.g. after a console command took
    /// items, counts as none.
    std::optional<ItemSelection> selection() const;

    /// The dialog that asks how many items of a stack to move, if it is open. One whose stack is no longer where it
    /// was, e.g. after a console command took items, counts as closed, and closes at the next event.
    const ItemSplitDialog* split_dialog() const;

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

    /// A move to the other inventory that waits for the ItemSplitDialog.
    struct PendingSplit {
        ItemSelection from;  ///< The stack whose items move.
        ItemId item;         ///< Its kind, to see that it is still there.
        std::size_t to_side;
        bool select_moved;  ///< See move().
        ItemSplitDialog dialog;
    };

    /// Handles a key, see the class description.
    std::optional<UiCommand> handle_key(NavKey key);
    /// Scrolls the grid under the pointer.
    void handle_scroll(const ScrollEvent& scroll, const PixelSize& picture);
    /// Selects or moves items with a click, or takes all with the button.
    void handle_pointer(const PointerEvent& pointer, const PixelSize& picture);
    /// Moves the selection by a key, see the class description.
    void navigate(NavKey key);
    /// Moves the selected stack to the other inventory, see move() without `select_moved`, after asking how many of its
    /// items if it has more than one.
    void move_selected_across();
    /// Moves the selected stack to the inventory of `side`, see move() with `select_moved`, after asking how many of
    /// its items if it has more than one and `side` is the other inventory.
    void move_selected_to(std::size_t side);
    /// Moves the selected stack to `side`, or opens the dialog first, see move_selected_across().
    void request_move(std::size_t side, bool select_moved);
    /// Moves `count` items of the stack `from` to the inventory of `side`, see move_items(). Then selects their stack
    /// there if `select_moved`, and otherwise what is left of the stack, or if nothing is, the stack that comes into
    /// its place, or the one before it if it was the last. A stack that can't take that many more items is left as it
    /// is, after a warning.
    void move(ItemSelection from, std::size_t side, int count, bool select_moved);
    /// Passes an event to the open ItemSplitDialog, and moves the items or closes it as it asks.
    std::optional<UiCommand> handle_split(const UiEvent& event, const PixelSize& picture);
    /// Moves all items of the container to the party's.
    void take_all();
    /// The cell under `point`, of any grid, empty or not.
    std::optional<ItemSelection> cell_at(const glm::vec2& point, const PixelSize& picture) const;
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
    /// The empty cell of the last press of the main button. The second press of a double click there is ignored.
    std::optional<ItemSelection> last_empty_press_;
    std::optional<PendingSplit> split_;  ///< See split_dialog().
};

}  // namespace manifold
