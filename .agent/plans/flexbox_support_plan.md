# CSS Flexbox Support Plan

## Goal

Implement CSS Flexible Box Layout Module Level 1 (https://www.w3.org/TR/css-flexbox-1/) in eepp's
HTML compatibility layer, as the foundation for modern web layouts. This is a spec-compliant
implementation mapped onto eepp's `UILayouter` architecture.

## Scope

Complete `display: flex` and `display: inline-flex` support including:

- Flex container properties: `flex-direction`, `flex-wrap`, `flex-flow` (shorthand), `justify-content`,
  `align-items`, `align-content`, `row-gap` / `column-gap` / `gap`
- Flex item properties: `order`, `flex-grow`, `flex-shrink`, `flex-basis`, `flex` (shorthand), `align-self`
- The CSS flex layout algorithm: generating flex lines, resolving flexible lengths, main-axis
  distribution, cross-axis alignment, multi-line cross-axis content distribution
- Flex items with auto margins
- Absolute/fixed-positioned flex children (out-of-flow, skipped by flex layout, placed via containing block)
- Percentage-based flex-basis resolution against the flex container's inner main size
- `min-width: auto` / `min-height: auto` default behavior on flex items (content-based minimum)

## Non-Scope

- `display: flex` on non-HTML eepp widgets (only `UIHTMLWidget` children)
- Nested flex containers (works naturally once the layouter is correct, but no special treatment)
- `order` with CSS Grid integration (order is ignored for non-flex/grid contexts)
- `align-content: space-evenly` (implemented, included in Phase 3)
- `margin: auto` on the cross axis (now tracked as gap G4)
- Column-reverse reordering of stacking context / z-order (now tracked as gap G11)

## Current State

**What exists now (as of the implementation checkpoint):**

- `CSSDisplay::Flex` and `CSSDisplay::InlineFlex` exist in the enum.
- `CSSDisplay::Flex` is routed to `BlockLayouter` in `UILayouterManager::create()`
  (`src/eepp/ui/uilayoutermanager.cpp`).
- `CSSDisplay::InlineFlex` is routed to `FlexLayouter`.
- Flex-related `PropertyId` values exist (`FlexDirection`, `FlexWrap`, `FlexFlow`,
  `JustifyContent`, `AlignItems`, `AlignContent`, `AlignSelf`, `FlexGrow`, `FlexShrink`,
  `FlexBasis`, `Flex`, `Order`, `RowGap`, `ColumnGap`, `Gap`).
- Flex CSS enums exist (`CSSFlexDirection`, `CSSFlexWrap`, `CSSJustifyContent`,
  `CSSAlignItems`, `CSSAlignContent`, `CSSAlignSelf`) in `csslayouttypes.hpp`.
- `FlexLayouter` class exists with a complete flex layout algorithm (Phases 1–11).
- `UIHTMLWidget::getPropertiesImplemented()` includes flex properties.
- `UIHTMLWidget::setDisplay()` handles `Flex` and `InlineFlex` with correct size policies.
- `friend class FlexLayouter` added to `UINode` for `setInternalPixelsWidth/Height` access.
- **Blocker:** `CSSDisplay::Flex` cannot be routed to `FlexLayouter` because real-world
  HTML uses `display: flex` on elements containing RichText (text nodes, inline elements),
  and FlexLayouter's single-pass layout cannot resolve the recursive size dependency
  between flex items and their text content. See "RichText Integration" below.

## Reference Specifications

- CSS Flexbox Module Level 1: https://www.w3.org/TR/css-flexbox-1/
  - Section 5: Flex Items
  - Section 7: Ordering and Orientation
  - Section 8: Flex Item Margins and Paddings
  - Section 9: Flex Layout Algorithm (THE critical section)
  - Section 10: Fragmenting Flex Layout (printed/column media — skip)

Initial value table (Section 2):

| Property | Initial Value |
|---|---|
| `flex-direction` | `row` |
| `flex-wrap` | `nowrap` |
| `justify-content` | `flex-start` |
| `align-items` | `stretch` |
| `align-content` | `stretch` |
| `align-self` | `auto` |
| `flex-grow` | `0` |
| `flex-shrink` | `1` |
| `flex-basis` | `auto` |
| `order` | `0` |
| `row-gap` / `column-gap` | `normal` |

## Impact Assessment

### Files to Create

| File | Purpose |
|---|---|
| `include/eepp/ui/flexlayouter.hpp` | `FlexLayouter` class declaration |
| `src/eepp/ui/flexlayouter.cpp` | Full flex layout algorithm implementation |


### Files to Modify

| File | Change Summary |
|---|---|
| `include/eepp/ui/csslayouttypes.hpp` | Add `CSSDisplay::InlineFlex` enum value |
| `src/eepp/graphics/csslayouttypes.cpp` | Add `"inline-flex"` to `CSSDisplayHelper` |
| `include/eepp/ui/css/propertydefinition.hpp` | Add flex-related `PropertyId` values |
| `src/eepp/ui/css/stylesheetspecification.cpp` | Register all flex CSS properties |
| `src/eepp/ui/uilayoutermanager.cpp` | Route `Flex` and `InlineFlex` to `FlexLayouter`; add include |
| `include/eepp/ui/uihtmlwidget.hpp` | Add flex-related state fields (or keep in layouter) |
| `src/eepp/ui/uihtmlwidget.cpp` | Handle flex properties in `applyProperty()` / `getPropertyString()` / `getPropertiesImplemented()` |
| `src/eepp/ui/uirichtext.cpp` | Skip flex-item widget children in `rebuildRichText()` (flex items are block-level, not inline) |
| `include/eepp/ui/uinode.hpp` | Add `friend class FlexLayouter` for `setInternalPixelsWidth/Height` access |
| `make/linux/eepp.make` | Add new source files `src/eepp/ui/flexlayouter.cpp` (via premake, then regenerate) |

### Design Decision: Where Does Flex State Live?

**Recommendation: Keep flex state on `FlexLayouter`**, not on `UIHTMLWidget`.

Reasons:
- `UIHTMLWidget` already stores `CSSDisplay`, `CSSFloat`, `CSSPosition`, etc. Adding ~10 more
  enums would bloat every HTML widget, most of which are never flex containers or flex items.
- Flex container properties (`flex-direction`, `flex-wrap`, etc.) only matter when a widget has
  `display: flex` or `display: inline-flex`. Those properties can be read from the CSS style
  during layout.
- Flex item properties (`order`, `flex-grow`, etc.) only matter when the parent is a flex
  container. The parent `FlexLayouter` can read those from each child's style when laying out.

If profiling shows repeated CSS-style lookups in the hot path are expensive, we can later cache
parsed flex values in helper structs. Start with direct style reads for simplicity.

## Implementation Plan

### Phase 0: CSS Infrastructure (Enums, PropertyIds, Parsing)

**Purpose:** Wire the CSS property system so flex properties parse and store correctly. No layout
behavior changes yet.

#### Step 0a: Add `CSSDisplay::InlineFlex`

**Files:** `csslayouttypes.hpp`, `csslayouttypes.cpp`

```cpp
// csslayouttypes.hpp — add to CSSDisplay enum:
InlineFlex,  // after Flex

// csslayouttypes.cpp — add to toString:
case CSSDisplay::InlineFlex: return "inline-flex";

// csslayouttypes.cpp — add to fromString:
else if ( val == "inline-flex" ) display = CSSDisplay::InlineFlex;
```

#### Step 0b: Define Flex Enums

**Files:** `include/eepp/ui/csslayouttypes.hpp`, `src/eepp/graphics/csslayouttypes.cpp`

Add enums directly to `csslayouttypes.hpp` following the existing pattern:

```cpp
enum class CSSFlexDirection { Row, RowReverse, Column, ColumnReverse };
enum class CSSFlexWrap { NoWrap, Wrap, WrapReverse };
enum class CSSJustifyContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly };
enum class CSSAlignItems { FlexStart, FlexEnd, Center, Baseline, Stretch };
enum class CSSAlignContent { FlexStart, FlexEnd, Center, SpaceBetween, SpaceAround, SpaceEvenly, Stretch };
enum class CSSAlignSelf { Auto, FlexStart, FlexEnd, Center, Baseline, Stretch };
```

Add `toString`/`fromString` helper structs (`CSSFlexDirectionHelper`, `CSSFlexWrapHelper`, etc.)
in `csslayouttypes.cpp` following the established pattern (e.g., `CSSDisplayHelper`).

Note: `CSSAlignSelf` includes `Auto` (the initial value) which resolves to the parent's
`align-items`. `CSSAlignItems` does NOT include `Auto` since `align-items: auto` is invalid.

#### Step 0c: Add Flex PropertyIds

**File:** `propertydefinition.hpp` — add to `PropertyId` enum:

```cpp
FlexDirection       = String::hash( "flex-direction" ),
FlexWrap            = String::hash( "flex-wrap" ),
FlexFlow            = String::hash( "flex-flow" ),
JustifyContent      = String::hash( "justify-content" ),
AlignItems          = String::hash( "align-items" ),
AlignContent        = String::hash( "align-content" ),
AlignSelf           = String::hash( "align-self" ),
FlexGrow            = String::hash( "flex-grow" ),
FlexShrink          = String::hash( "flex-shrink" ),
FlexBasis           = String::hash( "flex-basis" ),
Flex                = String::hash( "flex" ),
Order               = String::hash( "order" ),
RowGap              = String::hash( "row-gap" ),
ColumnGap           = String::hash( "column-gap" ),
Gap                 = String::hash( "gap" ),
```

#### Step 0d: Register Flex Properties in StyleSheetSpecification

**File:** `stylesheetspecification.cpp` — add in `registerDefaultProperties()`:

```cpp
registerProperty( "flex-direction", "row" );
registerProperty( "flex-wrap", "nowrap" );
registerProperty( "flex-flow", "row nowrap" );
registerProperty( "justify-content", "flex-start" );
registerProperty( "align-items", "stretch" );
registerProperty( "align-content", "stretch" );
registerProperty( "align-self", "auto" );
registerProperty( "flex-grow", "0", CSS::PropertyType::NumberFloat );
registerProperty( "flex-shrink", "1", CSS::PropertyType::NumberFloat );
registerProperty( "flex-basis", "auto" );
registerProperty( "flex", "0 1 auto" );
registerProperty( "order", "0", CSS::PropertyType::NumberInt );
registerProperty( "row-gap", "normal" );
registerProperty( "column-gap", "normal" );
registerProperty( "gap", "normal normal" );
```

Also register shorthand expansion in `registerShorthandProperties()`:

```cpp
registerShorthand( "flex-flow", { "flex-direction", "flex-wrap" } );
registerShorthand( "gap", { "row-gap", "column-gap" } );
```

The `flex` shorthand requires custom expansion logic (see Step 0f below).

#### Step 0e: Add Flex Property Handling in UIHTMLWidget

**File:** `uihtmlwidget.cpp`

- Add `PropertyId::FlexDirection`, etc. to `getPropertiesImplemented()` return list.
- Add cases in `applyProperty()` that parse the string/value and return `true` (acknowledged).
  The layouter reads from the style directly; no need to cache on the widget.
- Add cases in `getPropertyString()` that return the serialized string from the widget's style.

**Note on performance:** FlexLayouter reads 6–8 style properties per item during layout. With
<100 flex items this is negligible. If profiling shows overhead, cache parsed values in
`FlexLayouter::FlexItem` during `collectFlexItems()` (already done in the implementation).

#### Step 0f: Expand `flex` Shorthand

**File:** `src/eepp/ui/css/stylesheetproperty.cpp` — in `expandShorthand()`:

The `flex` shorthand has complex expansion rules per CSS §7.1:

| Input | Expanded to |
|---|---|
| `flex: auto` | `flex-grow: 1; flex-shrink: 1; flex-basis: auto` |
| `flex: none` | `flex-grow: 0; flex-shrink: 0; flex-basis: auto` |
| `flex: <number>` | `flex-grow: <number>; flex-shrink: 1; flex-basis: 0%` |
| `flex: <number> <number>` | `flex-grow: <n1>; flex-shrink: <n2>; flex-basis: 0%` |
| `flex: <number> <length>` | `flex-grow: <n>; flex-shrink: 1; flex-basis: <length>` |
| `flex: <number> <number> <length>` | `flex-grow: <n1>; flex-shrink: <n2>; flex-basis: <length>` |

**Critical distinction:** `flex: 1` expands to `flex: 1 1 0%`, NOT `flex: 1 1 auto`.
`flex-basis: 0%` is different from `flex-basis: 0` — `0%` resolves against the container,
while `0` is an absolute length. In practice, treat `0%` as `0px` for flex-basis resolution.

#### Step 0g: Route Display Values to FlexLayouter

**File:** `uilayoutermanager.cpp`

**Step 0g.1 (safe — do first):** Route `InlineFlex` only:
```cpp
case CSSDisplay::Block:
case CSSDisplay::TableCell:
case CSSDisplay::InlineBlock:
case CSSDisplay::ListItem:
case CSSDisplay::Flex:
    return eeNew( BlockLayouter, ( container ) );
case CSSDisplay::InlineFlex:
    return eeNew( FlexLayouter, ( container ) );
```

**Step 0g.2 (after RichText integration is complete):** Also route `Flex`:
```cpp
case CSSDisplay::Block:
case CSSDisplay::TableCell:
case CSSDisplay::InlineBlock:
case CSSDisplay::ListItem:
    return eeNew( BlockLayouter, ( container ) );
case CSSDisplay::Flex:
case CSSDisplay::InlineFlex:
    return eeNew( FlexLayouter, ( container ) );
```

**Why defer `Flex` routing:** Real-world HTML test pages (`body_height_miscalculation.html`,
`lobsters_simple.html`) use `display: flex` on text-containing elements. Routing `Flex` to
`FlexLayouter` before RichText integration is fixed breaks these tests. `InlineFlex` is safe
because no existing content uses it.

### Phase 1: FlexLayouter Skeleton And Item Collection

**Purpose:** Create the `FlexLayouter` class with the minimum scaffolding to avoid crashes when a
flex container is created. Item collection (gathering in-flow children into a flex item list) with
absolute position filtering.

#### Step 1a: Create FlexLayouter Header

**File:** `include/eepp/ui/flexlayouter.hpp`

```cpp
class EE_API FlexLayouter : public UILayouter {
  public:
    FlexLayouter( UIWidget* container );
    void updateLayout() override;
    void computeIntrinsicWidths() override;
    Float getMinIntrinsicWidth() override;
    Float getMaxIntrinsicWidth() override;

  private:
    // Flex item representation during layout
    struct FlexItem {
        UIHTMLWidget* widget;
        Float flexBaseSize;    // flex base size (hypothetical main size)
        Float hypotheticalCrossSize;
        Float targetMainSize;  // resolved main size after flexing
        Float minMainSize;     // min-content contribution in main axis
        Float maxMainSize;     // max-content contribution in main axis
        Float crossSize;       // final cross size
        CSSAlignSelf alignSelf;
        CSSFlexDirection itemDirection; // Item's own internal writing mode (placeholder)
        Float flexGrow;
        Float flexShrink;
        Float flexBasisValue;  // parsed flex-basis (px) or NaN if auto
        int order;
        bool frozen; // frozen during flex resolution
        Vector2f marginMainStart;
        Vector2f marginMainEnd;
        Vector2f marginCrossStart;
        Vector2f marginCrossEnd;
        bool hasAutoMarginMainStart;
        bool hasAutoMarginMainEnd;
        bool hasAutoMarginCrossStart;
        bool hasAutoMarginCrossEnd;
    };

    struct FlexLine {
        std::vector<FlexItem*> items;
        Float mainSize;           // total flex base size of items on this line
        Float crossSize;          // line cross size (max item cross size)
        Float remainingFreeSpace; // free space after item collection
    };

    // Axis helpers
    bool isHorizontalMainAxis() const;
    Float mainSize(const Sizef& s) const;
    Float crossSize(const Sizef& s) const;
    void setMainSize(Sizef& s, Float v) const;
    void setCrossSize(Sizef& s, Float v) const;
    Float mainPos(const Vector2f& p) const;
    Float crossPos(const Vector2f& p) const;
    void setMainPos(Vector2f& p, Float v) const;
    void setCrossPos(Vector2f& p, Float v) const;

    // Container state (read from style during layout)
    CSSFlexDirection mFlexDirection{CSSFlexDirection::Row};
    CSSFlexWrap mFlexWrap{CSSFlexWrap::NoWrap};
    CSSJustifyContent mJustifyContent{CSSJustifyContent::FlexStart};
    CSSAlignItems mAlignItems{CSSAlignItems::Stretch};
    CSSAlignContent mAlignContent{CSSAlignContent::Stretch};
    Float mRowGap{0};
    Float mColumnGap{0};

    std::vector<FlexItem> mFlexItems;
    std::vector<FlexLine> mFlexLines;
};
```

#### Step 1b: Implement Item Collection

**File:** `src/eepp/ui/flexlayouter.cpp`

`FlexLayouter::updateLayout()` outline:

1. Retrieve CSS properties from container's style (or from `UIHTMLWidget` if cached).
2. Collect in-flow children: iterate `mContainer`'s child list, skip `display: none`,
   `position: absolute/fixed`, text nodes in element children (flex items must be elements).
3. Blockify inline children per CSS Flexbox §4: if a child is `display: inline`,
   `display: inline-block`, `display: inline-table` — compute as block-level. The child's own
   layout is unaffected; this is about how it participates in the flex container.
4. For each flex item, read `order`, `flex-grow`, `flex-shrink`, `flex-basis`, `align-self`
   from the child's style. Sort `mFlexItems` by increasing `order`.
5. If `flex-wrap: nowrap`, collect all items into a single flex line.
   If `flex-wrap: wrap` or `wrap-reverse`, distribute items across lines per §9 Flex Layout
   Algorithm.

**Text node handling:** In HTML, bare text between flex items generates anonymous flex items
(anonymous block boxes wrapping the text). For a first implementation, text nodes that are
direct children of a flex container can be treated as a `0px` flex item or skipped with a warning.
Full anonymous flex item wrapping adds complexity and is rare in practice (users use `<span>`
or `<div>` as flex children). Document the limitation.

**Gate:** Create a minimal test with `display: flex` that has children. The children should be
positioned somewhere valid (not stepping on each other or the container origin), even if
alignment is not yet correct. No crash.

### Phase 2: Flex Layout Algorithm — Main Axis

**Purpose:** Implement §9.2–9.7 of the CSS Flexbox spec: determining flex base size, resolving
flexible lengths, distributing free space on the main axis.

#### Step 2a: Determine Flex Base Size And Hypothetical Main Size

For each flex item, per §9.2:

1. **If `flex-basis` is a definite length or percentage** (not `auto` and not `content`),
   the flex base size is that value. Percentages resolve against the flex container's
   inner main size if definite; otherwise they behave as `auto`.
2. **If `flex-basis` is `auto`**, determine the flex base size from the item's main size
   property (`width` for row direction, `height` for column direction).
3. **If the main size property is definite**, use that as the flex base size.
   Otherwise, use the item's **content-based size** — for eepp, this is the widget's
   current pixel size in the main axis (`getPixelsSize().getWidth()` for row,
   `getPixelsSize().getHeight()` for column), which reflects the laid-out content size.
4. The **hypothetical main size** is the flex base size clamped by any definite `min`/`max`
   main size constraints (`min-width`/`max-width` for row, `min-height`/`max-height` for column).

**Note:** The flex base size depends on `flex-basis` and the main size property, NOT on the
item's cross size. Cross size only affects the flex base size for aspect-ratio calculations,
which eepp does not support (no aspect-ratio CSS property). The item's cross size is determined
in Step 3a (cross-axis sizing) and is independent of the flex base size computation.

#### Step 2b: Resolve Flexible Lengths (§9.7)

1. Determine the used main size of the flex container (the "available main space"):
   - If the container has a definite main size, use that.
   - If max-content constrained (e.g., `width: max-content`), use the total max-content
     contributions of items, clamped.
   - Otherwise, use the containing block's available space.
2. Compute the initial free space: container inner main size minus sum of outer hypothetical
   main sizes of items on the line, minus gaps between items.
3. If free space is positive and `flex-grow` factors sum to > 0: distribute free space.
   If free space is negative and `flex-shrink` factors sum to > 0: distribute negative space.
4. Clamp each item's target main size by its min/max main size constraints.
   - **`min-width: auto` on flex items resolves to content size** per §4.5.
5. Iterate: re-distribute freed/added space after clamping until all items are at valid sizes
   or no further change.

#### Step 2c: Main-Axis Positioning (Justify)

After final main sizes are resolved, distribute remaining free space (if any after resolution)
according to `justify-content`:

- `flex-start`: items at main-start edge, no extra space between them.
- `flex-end`: items at main-end edge.
- `center`: items centered.
- `space-between`: first item at main-start, last at main-end, equal space between.
- `space-around`: equal space on both sides of each item (half-size spaces at edges).
- `space-evenly`: equal space between all items and edges.

For single-item lines, `space-between` behaves as `flex-start`.

Auto margins in the main axis absorb free space before `justify-content` applies
per §8.1: if an item has `margin-left: auto` (in row direction), it absorbs all
free space to its left. If two items have opposite auto-margins, they split the free space.

#### Step 2d: Intrinsic Sizing

`computeIntrinsicWidths()`: The max-content main size of a flex container is the sum of the
max-content contributions of items on the largest line, plus gaps. The min-content main size
is the largest min-content contribution among items on each line (since items can flex-shrink
below their base size), but the spec says min-content size of a flex container is computed by
setting `flex-basis: min-content` on all items and running a simplified pass.

**Gate:** Flex items are sized on the main axis correctly. Fixed-width items keep their width.
Items with `flex-grow` expand to fill available space. Items with `flex-shrink` shrink below
their natural size. The Flex Container Research tests from the CSS WG test suite or equivalent
hand-crafted tests should pass.

### Phase 3: Flex Layout Algorithm — Cross Axis

**Purpose:** Implement §9.4 (Cross Size Determination) and §9.5 (Main-Axis Alignment) / §9.6
(Cross-Axis Alignment).

#### Step 3a: Determine Hypothetical Cross Size

For each flex item on each line, if the cross size property is `auto`:
- Use `align-self: stretch` and the item has `auto` cross size → stretch the item to the
  line cross size.
- Otherwise, use the item's content-based cross size (intrinsic height for row direction,
  intrinsic width for column direction).

#### Step 3b: Determine Line Cross Size

For each flex line:
- Single-line container: line cross size = container inner cross size.
- Multi-line container: line cross size = maximum outer hypothetical cross size among items
  on the line, clamped by the container's min/max cross size constraints.

#### Step 3c: Handle `align-content` (Multi-line)

For multi-line containers, distribute remaining cross-axis free space among lines
according to `align-content`. Same values as `justify-content`, plus `stretch` which
stretches all lines to fill the container cross size.

#### Step 3d: Handle `align-items` / `align-self` (Within each line)

For each item on a line:
- Resolve `align-self`: if `auto`, use the container's `align-items`.
- Align the item within the line cross size:
  - `flex-start`: item at cross-start edge.
  - `flex-end`: item at cross-end edge.
  - `center`: item centered.
  - `baseline`: items baseline-aligned (cross-start position such that the first line box
    baseline of all items on the line shares the same cross-start offset).
  - `stretch`: item stretched to line cross size if its cross size is `auto`.

#### Step 3e: Cross-Axis Auto Margins

Auto margins on the cross axis absorb free space before `align-items`/`align-self`
apply. An item with `margin-top: auto` and `margin-bottom: auto` in row direction
gets vertically centered.

**Gate:** Flex items are fully sized and positioned in both axes. A horizontal flex container
with `align-items: center` vertically centers its children. `align-items: stretch` makes all
children the same height as the tallest one. Multi-line wrap containers distribute lines
correctly.

### Phase 4: Flex Direction And Wrapping Variants

**Purpose:** Implement all `flex-direction` and `flex-wrap` combinations with correct axis
mapping.

#### Step 4a: Direction Modes

| `flex-direction` | Main Axis | Cross Axis |
|---|---|---|
| `row` | horizontal, left to right | vertical, top to bottom |
| `row-reverse` | horizontal, right to left | vertical, top to bottom |
| `column` | vertical, top to bottom | horizontal, left to right |
| `column-reverse` | vertical, bottom to top | horizontal, left to right |

The axis helpers (`isHorizontalMainAxis()`, etc.) in the header must handle all four cases.
Main-start/main-end and cross-start/cross-end positions are defined by the direction and
writing mode. For Phase 4, assume LTR writing mode only.

#### Step 4b: `wrap-reverse`

In `wrap-reverse`, flex lines are stacked in the cross-end to cross-start direction.
The first line is at the cross-end edge, subsequent lines stack toward the cross-start edge.

#### Step 4c: `column`/`column-reverse` Intrinsic Sizing

In column direction, the intrinsic main size depends on the number of items and their
cross sizes. The container's intrinsic width is the sum of the largest item's max-content
width on the first line.

**Gate:** All four direction values and three wrap values produce correct layout.
A full 3×4 = 12-combination test matrix should pass.

### Phase 5: Gaps (row-gap, column-gap, gap shorthand)

**Purpose:** Implement gap properties per CSS Box Alignment §10.

#### Step 5a: Parse gap properties

- `row-gap: <length>` — spacing between flex lines (cross-axis gap).
- `column-gap: <length>` — spacing between flex items on the same line (main-axis gap).
- `gap: <row-gap> <column-gap>` shorthand.
- `normal` initial value means `0px` in flexbox (unlike grid where `normal` may differ).

#### Step 5b: Apply gaps in layout

- Main-axis: add `column-gap` between items within each flex line. Total free space is
  reduced by `(item_count - 1) * column_gap`. Gaps appear between items, not at the edges.
- Cross-axis: add `row-gap` between flex lines. Total cross free space is reduced by
  `(line_count - 1) * row_gap`.

**Gate:** Gaps visibly separate items. `gap: 10px` adds 10px between all items.

### Phase 6: Flex Item Layout (Setting Child Positions/Sizes)

**Purpose:** After computing all flex item positions and sizes via the flex algorithm,
write those back to the actual widget children.

#### Step 6a: Position And Size Children

After the flex algorithm resolves all `FlexItem::targetMainSize` and `FlexItem::crossSize`,
and computes the main-axis and cross-axis positions:

1. Convert the axis-relative results back to screen coordinates (x, y, width, height).
2. Set each child widget's `setPixelsPosition()` and `setPixelsSize()`.
3. Handle children that still need their own internal layout (call `updateLayout()` on
   child widgets if they have their own layouter — e.g., a flex item that is itself a
   flex container).

#### Step 6b: Handle MatchParent Children

Flex items with `width: 100%` in a row-direction container should resolve that percentage
against the container's defined size, not get stretched to 100% of the container after
flex base size computation. This is part of the definite/indefinite size resolution in §9.2.

If the flex container has a definite main size, percentage main-size children resolve
against that. If indefinite (e.g., `width: auto`), treat as `auto`.

#### Step 6c: Handle Shrink-Wrap Container

A flex container with `width: auto` (or `height: auto` for column direction) should use
max-content sizing: its outer width is the sum of max-content contributions of items
on the largest line.

However, in the eepp widget system, if the flex container is `MatchParent`, it fills
its parent. If `WrapContent`, it shrinks to fit content. The container's size policy
must be set correctly in `setDisplay()` like `InlineBlock` and `Block` do.

**Flex containers are block-level by default** (`display: flex`) and inline-level
(`display: inline-flex`). A block-level flex container fills its containing block width
(match-parent); an inline-flex shrink-wraps.

#### Step 6d: Relayout if re-entrancy

After sizing children, if a child's size changed from what was assumed during the flex
algorithm (e.g., because the child's layouter made a different choice), a second pass
may be needed. `FlexLayouter` should:
- Track whether any child's actual laid-out size differs from the target computed by the
  flex algorithm.
- If so and within a reasonable iteration limit (e.g., 2 passes), re-run the flex algorithm
  with the updated child sizes.

**Gate:** Children of a flex container appear at correct positions and sizes.
Nested flex containers work. The full layout round-trip from DOM to screen is correct.

### Phase 7: `flex` Shorthand Parsing

> **Note:** This is implemented as part of Phase 0 (Step 0f). This section remains as a
> reference for the shorthand behavior. The suggested implementation order (above) merges
> this into Phase 0.

**Purpose:** Parse the `flex` shorthand property.

The `flex` shorthand sets `flex-grow`, `flex-shrink`, and `flex-basis` together.

CSS syntax (§7.1):
- `flex: auto` → `flex: 1 1 auto`
- `flex: none` → `flex: 0 0 auto`
- `flex: <flex-grow>` → `flex: <flex-grow> 1 0%` (single unitless number)
- `flex: <flex-grow> <flex-shrink>` → `flex: <flex-grow> <flex-shrink> 0%`
- `flex: <flex-grow> <flex-basis>` → `flex: <flex-grow> 1 <flex-basis>` (when basis is a width)
- `flex: <flex-grow> <flex-shrink> <flex-basis>` → all three

Initial values: `flex-grow: 0, flex-shrink: 1, flex-basis: auto`.

Implementation: Add a helper function that parses the shorthand string and sets
the three individual properties on the style. Since `StyleSheetProperty` stores
strings, this means generating serialized values for the individual properties.

**Gate:** `flex: 1` on a child makes it grow. `flex: none` makes it inflexible.
`flex: 0 0 200px` sets a fixed 200px flex item.

### Phase 8: `min-width: auto` On Flex Items

**Purpose:** Implement the CSS Flexbox §4.5 rule: the initial `min-width` (and `min-height`
in column direction) of a flex item is `auto`, which resolves to `min-content` (content-based
minimum), NOT `0`.

Without this, flex items with `flex-shrink` can collapse to 0px even when they contain
text, images, or side-by-side floats. This is the #1 source of "content disappears in
flexbox" bugs in naive implementations.

Implementation:
1. When a flex item has no explicit `min-width`/`min-height` (the `auto` initial value),
   compute its content-based minimum size.
2. For a flex item with text content (RichText), the min-content width is the width of
   the longest word. The min-content height is 0 (inline elements don't have intrinsic height).
3. The item's final target main size must be at least this minimum, even if `flex-shrink`
   would otherwise push it below.

In eepp, the `UILayouter::getMinIntrinsicWidth()` / `getMaxIntrinsicWidth()` methods
on child widgets provide exactly this. Call `child->getLayouter()->getMaxIntrinsicWidth()`
for the main-axis max-content contribution and `getMinIntrinsicWidth()` for the min-content.

**Gate:** A flex item containing text with `flex-shrink: 1` on a narrow container does not
collapse below a single word's width. A `min-width: 50px` on the item overrides the
content-based minimum.

### Phase 9: Inline-Flex Behavior

**Purpose:** Make `display: inline-flex` create an inline-level flex container.

This mostly requires:
- Setting the container's size policy to `WrapContent` (shrink-to-fit).
- Ensuring the inline-flex container participates in the parent's inline formatting context
  as an atomic inline-level box (like `inline-block`).
- The container's internal flex layout is identical to `display: flex`.

Most of this is handled in `UIHTMLWidget::setDisplay()` which already sets size policies
based on display mode. `InlineFlex` should behave like `InlineBlock` for the outer display
type while using `FlexLayouter` for the inner formatting context.

The `CSSDisplay::InlineFlex` case in `setDisplay()` should:
- Set width policy to `WrapContent`, height policy to `WrapContent`.
- The layouter is `FlexLayouter`.

**Gate:** `display: inline-flex` flows inline in a block-level parent. The container
shrink-wraps its content.

### Phase 10: Out-Of-Flow Flex Children

**Purpose:** Handle `position: absolute` and `position: fixed` children inside a flex container.

Per CSS Flexbox §4.1:
> An absolutely-positioned child of a flex container does not participate in flex layout.

The flex container's static position for an absolutely-positioned child is where the child
would have been placed had it been the sole flex item in the container (at the main-start/
cross-start position with no stretching applied).

Implementation:
1. Skip out-of-flow children during `FlexLayouter::updateLayout()` item collection.
2. After regular flex items are laid out, position out-of-flow children via
   `positionOutOfFlowChildren()` (already handled by `UIHTMLWidget::updateLayout()`).
3. The static position for out-of-flow children within a flex container: the spec says
   "the static position of an absolutely-positioned child of a flex container is determined
   such that the child is positioned as if it were the sole flex item in the flex container."
   For now, position at the container's padding-box origin (content-start) since the
   `updateOutOfFlowPosition()` method in `UIHTMLWidget` will compute the actual position
   based on top/left/right/bottom CSS properties.

**Gate:** Absolutely-positioned children inside a flex container don't crash and are
positioned by their CSS offsets relative to the flex container's content area.

### Phase 11: Edge Cases And Polish

#### 11a: Empty Flex Container
A flex container with no in-flow children should collapse to its cross size (usually 0)
unless the container has an explicit `height` or `min-height`.

#### 11b: Flex Items With Definite Cross Size But Auto Main Size
When the main size is indefinite and the cross size is definite, the flex base size in
a row-direction container is computed from the aspect ratio or intrinsic sizing.
Since eepp widgets don't typically have aspect ratios, fall back to intrinsic sizing
(layout the child with the given cross size, measure the resulting main size).

#### 11c: Baseline Alignment
`align-items: baseline` / `align-self: baseline`: the baseline of a flex item is:
- If the item is a flex container, the baseline of its first/last line box (per
  `flex-direction`).
- If the item has text content, the ascent of its first line box.
- Otherwise, the bottom edge of the item's margin box.

In eepp, use the existing baseline computation from `UIRichText` / `RichText`:
```cpp
margin.Top + contentOffset.Top + firstLine.y + firstLine.maxAscent
```

#### 11d: Replaced Elements
`<img>`, `<input>`, `<canvas>`, `<video>` inside flex containers should use their
intrinsic size for flex base size computation. eepp's replaced widgets expose
intrinsic size through the existing `getMinIntrinsicWidth()` / `getMaxIntrinsicWidth()`
methods.

#### 11e: Percentage Margins/Paddings
Percentage margins/paddings on flex items resolve against the inline size (width) of
the containing block, per CSS2 rules. This is already handled by the existing margin/padding
computation in `UILayout`/`UIWidget`.

### Phase 12: Writing Mode Independence (Deferred)

CSS Flexbox is writing-mode-aware. For LTR horizontal-tb (the only mode currently used
in eepp), the implementation is straightforward. Full writing-mode support (RTL, vertical
writing modes) requires the `direction` and `writing-mode` CSS properties to be implemented,
which is a separate effort. This plan focuses on the LTR horizontal-tb writing mode only.

## Tests Required

### Unit Tests (Flex Algorithm)

| Test Name | What It Verifies |
|---|---|
| `FlexContainer.collectsInFlowChildren` | Only regular in-flow children become flex items |
| `FlexContainer.skipsOutOfFlowChildren` | Absolute/fixed positioned children are skipped |
| `FlexContainer.skipsDisplayNoneChildren` | `display: none` children are skipped |
| `FlexContainer.sortsByOrder` | Children with higher `order` appear later in flow |
| `FlexContainer.defaultRowLayout` | Default `row` direction, items in a horizontal line |
| `FlexContainer.rowReverseLayout` | `row-reverse` reverses item order |
| `FlexContainer.columnLayout` | `column` direction stacks items vertically |
| `FlexContainer.columnReverseLayout` | `column-reverse` reverses vertical order |
| `FlexContainer.flexGrowDistributesSpace` | `flex-grow: 1` makes items fill available space |
| `FlexContainer.flexGrowProportional` | `flex-grow: 2` vs `flex-grow: 1` distribution ratio |
| `FlexContainer.flexShrinkContractsItems` | `flex-shrink: 1` shrinks items in overflow |
| `FlexContainer.flexBasisFixed` | `flex-basis: 200px` sets initial main size |
| `FlexContainer.flexShorthand` | `flex: 1 0 100px` shorthand works |
| `FlexContainer.justifyContentFlexStart` | Default alignment at start |
| `FlexContainer.justifyContentFlexEnd` | Items at end |
| `FlexContainer.justifyContentCenter` | Items centered |
| `FlexContainer.justifyContentSpaceBetween` | Equal space between items |
| `FlexContainer.justifyContentSpaceAround` | Half-space at edges |
| `FlexContainer.alignItemsStretch` | Default stretch fills cross axis |
| `FlexContainer.alignItemsCenter` | Items centered on cross axis |
| `FlexContainer.alignItemsFlexStart` | Items at cross-start |
| `FlexContainer.alignItemsFlexEnd` | Items at cross-end |
| `FlexContainer.alignSelfOverride` | `align-self: center` overrides `align-items: stretch` |
| `FlexContainer.wrapCreatesMultipleLines` | `flex-wrap: wrap` splits items into lines |
| `FlexContainer.wrapReverse` | Lines stack in reverse cross-axis order |
| `FlexContainer.alignContentSpaceBetween` | Multi-line content distribution |
| `FlexContainer.alignContentCenter` | Multi-line content centered |
| `FlexContainer.gapRowColumn` | `row-gap`, `column-gap`, `gap` create spacing |
| `FlexContainer.autoMarginMainAxis` | `margin-left: auto` in row pushes item right (deferred: auto margins not yet implemented) |
| `FlexContainer.autoMarginCrossAxis` | `margin-top: auto` + `margin-bottom: auto` centers vertically (deferred) |
| `FlexContainer.minWidthAutoOnFlexItem` | Content-based minimum prevents collapse |
| `FlexContainer.nestedFlexContainer` | Flex inside flex works |
| `FlexContainer.emptyContainer` | No children doesn't crash |
| `FlexContainer.fixedWidthItem` | Explicit width is respected |
| `FlexContainer.percentageBasis` | `flex-basis: 50%` resolves against container |
| `FlexContainer.flexBasisZeroPercent` | `flex: 1 1 0%` distributes space equally (common pattern) |
| `FlexContainer.flexBasisZeroLength` | `flex: 1 1 0px` is different from `0%` |
| `FlexContainer.columnWithTextContent` | `flex-direction: column` with text items gets correct heights |
| `FlexContainer.wrapWithAutoWidth` | `flex-wrap: wrap` on container with `width: auto` |
| `FlexContainer.negativeOrder` | `order: -1` places item before others |
| `FlexContainer.alignContentSpaceEvenly` | `align-content: space-evenly` distributes lines evenly |
| `FlexContainer.alignContentStretch` | `align-content: stretch` expands lines to fill container |
| `FlexContainer.gapNormal` | `gap: normal` resolves to 0px in flexbox |
| `FlexContainer.stretchWithFixedCrossSize` | `align-items: stretch` does not override explicit cross size |
| `FlexContainer.baselineWithText` | `align-items: baseline` aligns text baselines (approximate) |

### HTML Fixture Tests

| Test Name | What It Verifies |
|---|---|
| `FlexHorizontalNav` | Horizontal navigation bar with spaced items |
| `FlexVerticalCentering` | Vertically centering content in a container |
| `FlexCardLayout` | Card-style layout with wrap |
| `FlexRealWorldBodyHeight` | `body { display: flex; flex-direction: column }` with mixed content (Phase 12 gate) |
| `FlexRealWorldLobsters` | `display: flex` on anchor container with text children (Phase 12 gate) |
| `FlexMediaObject` | Image + text side-by-side with flex |
| `FlexFormRow` | Label + input in a flex row with `flex-grow` |
| `FlexStickyFooter` | Flex column with sticky footer (main takes remaining space) |
| `FlexCenteredModal` | `justify-content: center` + `align-items: center` both axes |

## Standard Validation Gate

Before considering a phase complete:

```sh
make -C make/linux -j$(nproc)
ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x1024x24" bin/unit_tests/eepp-unit_tests-debug --filter="FlexContainer.*"
ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x1024x24" bin/unit_tests/eepp-unit_tests-debug --filter="UIHTMLFlex.*"
ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x1024x24" bin/unit_tests/eepp-unit_tests-debug
git diff --check
```

## Checkpoint Policy

Each completed phase must leave a recoverable stash snapshot. The checkpoint is a safety net,
not a replacement for keeping the working tree active.

### Required sequence at the end of every passing phase:

1. Run phase-specific focused tests (e.g., `--filter="FlexContainer.*"`).
2. Run the full test suite.
3. Run `git diff --check`.
4. Run `clang-format` on all modified files:
   ```sh
   git diff --name-only -- '*.c' '*.cpp' '*.h' '*.hpp' | xargs clang-format -i
   ```
5. Create a checkpoint:
   ```sh
   git stash push -u -m "flexbox-support phase N: <short description of what was done>"
   ```
6. Re-apply it immediately so the working tree keeps the phase changes:
   ```sh
   git stash apply stash@{0}
   ```
7. Confirm `git diff --stat` still reflects the completed phase changes.
8. Record the stash message in the phase completion notes.

### Important rules:

- Use `git stash push -u` so new plan files, new tests, fixtures, and other untracked
  artifacts are included in the checkpoint.
- **Do not use `git stash pop`** — popping destroys the checkpoint. The stash entry must
  remain in the stash list as the last known-good recoverable snapshot.
- Do not checkpoint a failing phase as if it were passing.
- If a phase needs partial experimental work that does not pass yet, stash it separately
  with a clear `wip-failing` message before reverting or switching direction. Do not
  call that stash a phase checkpoint.
- If a later phase fails or becomes hard to unwind, recover from the latest passing phase
  checkpoint instead of manually reconstructing the working tree:
  ```sh
  git stash list                    # find the phase N stash
  git checkout -- .                 # clean working tree
  git stash apply stash@{N}         # restore the last good state
  ```

### Example phase completion notes format:

```
Phase 0 completed:
- Validation: make -C make/linux -j$(nproc) passed
- Tests: FlexContainer.* (N/A, no tests yet), full suite passed
- git diff --check: clean
- Stash checkpoint: flexbox-support phase 0: CSS infrastructure (enums, property IDs, parsing, display routing)
- Stash reference: stash@{0}
```

## Files Affected (Summary)

| File | Status | Change |
|---|---|---|
| `include/eepp/ui/csslayouttypes.hpp` | Modified | Add `InlineFlex` to `CSSDisplay`; add 6 flex enums |
| `src/eepp/graphics/csslayouttypes.cpp` | Modified | Add string conversion helpers for all new enums |
| `include/eepp/ui/css/propertydefinition.hpp` | Modified | Add 14 flex-related `PropertyId` values |
| `src/eepp/ui/css/stylesheetspecification.cpp` | Modified | Register flex properties + shorthand expansion |
| `include/eepp/ui/uinode.hpp` | Modified | Add `friend class FlexLayouter` for `setInternalPixelsWidth/Height` access |
| `include/eepp/ui/flexlayouter.hpp` | **New** | FlexLayouter class with FlexItem, FlexLine, full API |
| `src/eepp/ui/flexlayouter.cpp` | **New** | Complete flex layout algorithm (~650 lines) |
| `src/eepp/ui/uilayoutermanager.cpp` | Modified | Route `InlineFlex` to `FlexLayouter`; keep `Flex` on `BlockLayouter` until Phase 12 |
| `src/eepp/ui/uihtmlwidget.cpp` | Modified | Handle `Flex`/`InlineFlex` in `setDisplay()`, add flex props to `getPropertiesImplemented()` |
| `src/eepp/ui/uirichtext.cpp` | To be modified | Skip flex-item widget children in `rebuildRichText()` |
| `make/linux/eepp.make` | Regenerated | Add `src/eepp/ui/flexlayouter.cpp` (via premake, then `make config=debug_x86_64 -C make/linux`) |

## Gaps and Missing Features (Spec Review against CSS Flexbox Level 1)

This section catalogues features in the CSS Flexbox Level 1 specification that are
either unimplemented or incompletely implemented in the current `FlexLayouter`. Each
gap is prioritized (P1=critical correctness, P2=important, P3=nice-to-have).

### G1: Iterative flex resolution after min/max clamping (§9.7) — P1

**Status:** Missing. `resolveFlexibleLengths()` does a single pass: distribute free space,
clamp by min/max, then return. The spec requires iterative re-distribution: when clamping
frees up or consumes space, that space must be re-distributed to other unfrozen items.
Items that hit their min/max become "frozen" and excluded from subsequent iterations.

**Impact:** Items with `min-width` constraints can cause incorrect free space distribution.
For example, if three items all have `flex-grow: 1` but one has `min-width: 200px` and
hits that minimum, the remaining free space should go to the other two items proportionally.
Our single-pass implementation doesn't handle this.

**Fix:**
1. Add a `frozen` flag to `FlexItem`.
2. Implement the iterative loop from §9.7:
   ```
   while (free space changes) {
       for each unfrozen item: fix its size if it hits min/max (mark frozen)
       re-distribute freed/consumed space among remaining unfrozen items
   }
   ```
3. Replaces current lines ~364-419 of `flexlayouter.cpp`.

### G2: Correct min-intrinsic width for wrap containers (§9.9.1.3) — P1

**Status:** Incorrect. `computeIntrinsicWidths()` returns `containerPadding.Left +
containerPadding.Right` for `mMinIntrinsicWidth` in all wrap cases (lines 950, 943).
The spec says min-content main size of a multi-line flex container is the largest
min-content contribution among all items on ALL lines (since each line can be as
narrow as its widest item, and the container is as narrow as its widest line).

**Fix:**
```cpp
// For wrap containers, min-intrinsic width = largest item min-content width
Float maxMinContent = 0.f;
for (auto& item : items) {
    Float minContent = /* item's min-content main size — use getMinIntrinsicWidth() if available */;
    maxMinContent = eemax(maxMinContent, minContent + padding);
}
mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
```

### G3: `visibility: collapse` on flex items (§4.4) — P2

**Status:** Not implemented. No references to `collapse` in flex layouter code.

**Impact:** `visibility: collapse` is a flexbox-specific feature that hides items while
preserving cross-axis space (for single-line containers) so the layout doesn't "wobble".

**Fix:**
1. In `collectFlexItems()`, detect `visibility: collapse` items (check widget style).
2. Run flex layout twice: first pass uncollapsed to compute line cross sizes, second
   pass with collapsed items replaced by struts preserving the original line cross size.
3. In `applyLayout()`, set collapsed items to invisible (zero size or skip rendering).

### G4: Cross-axis auto margins (§8.1) — P2

**Status:** Not implemented. Auto margins on the main axis are handled (zeroed in
`measureFlexItems()`, absorbed in `alignMainAxis()`). Auto margins on the cross axis
are NOT detected — `marginCrossStart`/`marginCrossEnd` are set to the raw pixel margin
values without checking for `auto`.

**Impact:** `margin: auto` on a flex item in column direction (or `margin-top: auto` +
`margin-bottom: auto` in row direction) should center the item on the cross axis.
Currently this silently fails.

**Fix:**
1. In `measureFlexItems()`, detect cross-axis auto margins similarly to main-axis:
   ```cpp
   item.marginCrossStart = hasLayoutMarginTopAuto() ? 0.f : margin.Top;  // for row
   item.marginCrossEnd = hasLayoutMarginBottomAuto() ? 0.f : margin.Bottom;
   item.hasAutoMarginCrossStart = /* bool flag */;
   item.hasAutoMarginCrossEnd = /* bool flag */;
   ```
2. Add `hasAutoMarginCrossStart`/`hasAutoMarginCrossEnd` fields to `FlexItem`.
3. In `alignCrossAxis()`, before applying `align-items`/`align-self`, distribute
   cross-axis free space to items with auto cross-axis margins:
   - If both are auto: center the item
   - If only one: push to the opposite edge
4. Auto margins absorb free space BEFORE `align-self` applies per §8.1.

### G5: `overflow` affecting `min-width:auto` / `min-height:auto` (§4.5) — P2

**Status:** Not implemented. The `min-width:auto` computation (lines 250-280 of
`flexlayouter.cpp`) does not check `overflow`. Per spec:
- `overflow: visible` → min-size = content-based minimum (current behavior, correct)
- `overflow: scroll`/`auto`/`hidden` → min-size = 0

**Fix:**
1. In `measureFlexItems()`, check the item's computed `overflow` value.
2. For scrollable overflow items, set `minMainSize = 0`.
3. Keep content-based minimum for `overflow: visible` (the default).

### G6: Percentage margins/paddings on flex items (§4.2) — P3

**Status:** Not explicitly handled. Margins are obtained via `getLayoutPixelsMargin()`,
which returns pixel values pre-resolved by the widget system. If the widget system
doesn't resolve percentage margins correctly for flex items (resolved against the
containing block's inline size), this could be wrong.

**Impact:** `margin: 5%` on a flex item should resolve against the flex container's
width (in horizontal writing mode). If this isn't handled upstream, percentage
margins behave incorrectly.

**Fix:** Verify whether `getLayoutPixelsMargin()` already resolves percentage
margins correctly. If not, add percentage resolution in `measureFlexItems()` using
the flex container's width (containing block inline size).

### G7: Painting order by `order`-modified document order (§4.3) — P3

**Status:** Not implemented. `order` sorts items for layout purposes only. The spec
says flex items paint in `order`-modified document order (items with lower `order`
paint first, regardless of DOM position), and `z-index` values other than `auto`
create a stacking context even if `position` is `static`.

**Impact:** Overlapping flex items with different `order` values may paint in the
wrong order. This affects visual output but not layout geometry.

**Fix:** Requires changes to the widget painting/tree structure, which is outside
`FlexLayouter`'s scope. Options:
1. Physically reorder children in the widget tree (complex, affects hit testing).
2. Add a `paintOrder` field to `UIWidget` and sort by it during paint traversal.
3. Accept as a known limitation for now (most layouts don't overlap ordered items).

### G8: Flex container baseline computation (§8.5) — P3

**Status:** Not implemented. `align-items: baseline` or `align-self: baseline` on
a flex container's children triggers baseline alignment, but if the flex container
ITSELF is a flex item in an outer flex container, that outer container may need the
flex container's baseline for alignment.

Per spec, the baseline of a flex container is:
1. The baseline of the first flex line if `flex-direction` is row/row-reverse.
2. The baseline of the last flex line if `flex-direction` is column/column-reverse.

**Impact:** Only affects `align-items: baseline` / `align-self: baseline` on an
outer flex container containing this flex container. This is an uncommon pattern.

**Fix:**
1. After `applyLayout()`, store the baseline offset of the first/last line.
2. Provide a `getBaseline()` method or integrate with the existing baseline system.
3. Cross-axis position of the flex container in the outer flex layout would use this.

### G9: `flex-basis: content` distinct from `flex-basis: auto` (§7.2.3) — P3

**Status:** Both `content` and `auto` are treated identically (`flexBasisAuto = true`).
Per spec, `flex-basis: content` always uses the content-based size, while
`flex-basis: auto` first checks for a definite main size property.

**Impact:** In practice, most uses of `flex-basis: content` produce the same result
as `flex-basis: auto`. The difference matters when an item has both `flex-basis: auto`
and an explicit `width: 300px` — `auto` uses 300px, `content` uses the content size.

**Fix:** Add a separate `flexBasisContent` flag (or rename `flexBasisAuto` to an
enum: Auto, Content, None). In `resolveFlexBasis()`, skip the definite-size check
when `flex-basis: content`.

### G10: `flex-basis` percentage resolution for indefinite container size (§9.8) — P3

**Status:** Partially handled. Line 233: `if (indefiniteMainSize && item.flexBasisIsPercentage)`
→ treats as auto. Good, but only for the main axis indefinite case. The spec also
says percentages in `flex-basis` resolve against the container's inner main size.

**Impact:** Minor; most percentage `flex-basis` uses have a definite container.

### G11: Column-reverse stacking context reordering (§4.1 note) — P3

**Status:** Not implemented. Per spec, `flex-direction: column-reverse` should
reorder the stacking context (z-index painting) so the last visual item paints on
top, even though it's DOM-first. Our implementation only visually reorders without
adjusting the stacking context.

**Impact:** Only visual; items in a column-reverse container may not paint in the
expected order when they overlap.

### G12: Anonymous flex items from bare text nodes (§4) — P3

**Status:** Not implemented. Text sequences between flex item elements should wrap
in anonymous block flex items. Our `collectFlexItems()` skips `UITextNode` whitespace
but does not create anonymous wrappers for non-whitespace text.

**Impact:** Bare text as direct children of flex containers doesn't participate in
flex layout. Workaround: wrap text in `<span>` or `<div>` elements.

### Summary of Gaps by Priority

| # | Feature | Priority | Effort | Status |
|   |---------|----------|--------|--------|
| G1 | Iterative flex resolution after min/max clamping (§9.7) | **P1** | Medium | ✅ Done |
| G2 | Correct min-intrinsic width for wrap containers (§9.9.1.3) | **P1** | Small | ✅ Done |
| G3 | `visibility: collapse` (§4.4) | P2 | Large | Pending |
| G4 | Cross-axis auto margins (§8.1) | P2 | Medium | ✅ Done |
| G5 | `overflow` affecting min-width:auto (§4.5) | P2 | Small | ✅ Done |
| G6 | Percentage margins/paddings (§4.2) | P3 | Small | Pending |
| G7 | Painting order by `order` (§4.3) | P3 | Medium | Pending |
| G8 | Flex container baselines (§8.5) | P3 | Medium | Pending |
| G9 | `flex-basis: content` distinct from auto (§7.2.3) | P3 | Small | Pending |
| G10 | Percentage flex-basis resolution (§9.8) | P3 | Small | Pending |
| G11 | Column-reverse stacking context (§4.1) | P3 | Small | Pending |
| G12 | Anonymous flex items (§4) | P3 | Large | Pending |

### Remaining Phase 12: Route `CSSDisplay::Flex` to FlexLayouter

As documented above, the RichText integration blocker prevents `display: flex`
from being routed to `FlexLayouter`. The fix requires two coordinated changes:

1. `UILayouterManager::create()` — blockify flex item children (return `BlockLayouter`
   for direct children of flex/inline-flex containers, regardless of their own `display`)
2. `BlockLayouter::updateLayout()` — skip the `isInline()` early-return for flex items

These changes are described in detail in the Root Cause Analysis section above.
They should be implemented, tested, and verified before considering `display: flex`
routing to be production-ready.

## Risk Analysis

| Risk | Severity | Mitigation |
|---|---|---|
| **RichText integration breaks existing HTML tests when `Flex` is routed** | **Critical** | Keep `Flex` on `BlockLayouter` until RichText integration is solved (Phase 12). Test with real HTML fixtures before switching. |
| Flex algorithm is computationally expensive | Medium | Cache intrinsic sizes within a layout pass; avoid repeated RichText rebuilds. Algorithm is O(n) per line, so complexity is linear in item count. |
| Incorrect baseline computation breaks `align-items: baseline` | Medium | Current implementation approximates baseline. True baseline requires RichText line box ascent measurement. Document as limitation. |
| `min-width: auto` on flex items requires content measurement | Medium | Use existing `getMinIntrinsicWidth()` / `getMaxIntrinsicWidth()` path. For text content, this requires the item's RichText to be laid out first (same blocker as main integration). |
| Column-direction flex container with auto height | Medium | Requires correct child height measurement before container height can be determined. Same root cause as RichText integration; defer to Phase 12. |
| Interaction with float layout inside flex items | Low | Flex items establish independent formatting contexts per CSS. Floats inside items are local to the item's BlockLayouter. Should work naturally. |
| Interaction with existing out-of-flow positioning | Low | Skip out-of-flow children during flex item collection. Out-of-flow positioning already handled by `UIHTMLWidget::updateOutOfFlowPosition()`. |
| `order` reordering breaks DOM event targeting order | Low | `order` is visual-only per CSS spec. Hit testing follows DOM order (unchanged). Only visual positions are reordered. |
| Performance of repeated style reads in the flex hot path | Low | Already mitigated: `collectFlexItems()` reads all style properties once and caches in `FlexItem`. No style reads in the hot loop. |
| Auto margins on flex items not implemented | Low | Document as limitation. Most layouts don't rely on auto margins in flex. Can be added later without breaking existing behavior. |

## Updated Implementation Roadmap

The original plan covered Phases 0-12. Phases 0-11 are largely complete with the
gaps documented above. Here's the updated path forward:

### ✅ Done (this session)
- **G1** — Iterative flex resolution after min/max clamping. Rewrote `resolveFlexibleLengths()` with iterative §9.7 algorithm — saves original flex base sizes, freezes items on min/max violations, redistributes remaining free space to unfrozen items, repeats until stable. Added `FlexItem::frozen` flag.
- **G2** — Correct min-intrinsic width for wrap containers. Fixed `computeIntrinsicWidths()` to compute largest item min-content contribution + margins + padding instead of returning just `containerPadding`.
- **G4** — Cross-axis auto margins. Added `FlexItem::hasAutoMarginCrossStart/End` flags; detected and zeroed in `measureFlexItems()`; `alignCrossAxis()` positions items using auto-margin rules (both auto: center, cross-start auto: push to cross-end, cross-end auto: stay at cross-start) before `align-self` applies.
- **G5** — `overflow` affecting min-width:auto. Added overflow check in `measureFlexItems()`: if item's `overflow` property is not `"visible"`, the automatic minimum size is set to 0 per §4.5.
- Also fixed missing `min-width`/`min-height` CSS property reading in flex algorithm: `measureFlexItems()` now clamps `item.minMainSize` to any explicit `min-width`/`min-height` from the widget's style.

### Next (fill remaining spec gaps)
- **G3** — `visibility: collapse`. Full spec feature, large effort but important for dynamic UIs.
- **G8** — Flex container baselines. Only needed for nested baseline alignment.
- **G9** — `flex-basis: content` vs `auto`. Minor distinction, rarely used.
- **G10** — Percentage flex-basis edge cases.
- **G11** — Column-reverse stacking context.
- **G6** — Percentage margins/paddings.
- **G7** — Painting order by `order`.
- **G12** — Anonymous flex items.

### Final gate
- **Route `display: flex` to FlexLayouter** — Implement blockification changes
  and test with real-world HTML pages (`lobsters_simple.html`, `body_height_miscalculation.html`,
  etc.). This is the final validation that the RichText integration fix works.

## Limitations Documented For This Implementation

1. **Anonymous flex items from bare text nodes:** Not generated. Text nodes and non-widget
   nodes as direct children of flex containers are skipped during item collection. In real
   CSS, bare text generates anonymous flex items; this implementation requires text to be
   wrapped in `<span>` or `<div>` to participate in flex layout.
2. **`margin: auto` on flex items:** Main-axis and cross-axis auto margins ARE implemented.
   Main-axis auto margins absorb free space before `justify-content`. Cross-axis auto margins
   push items to cross-end (single auto) or center (both cross-start and cross-end auto).
3. **Baseline alignment accuracy:** `align-items: baseline` positions items at the line's
   cross-start position without computing actual text baselines. True baseline alignment
   requires measuring the first line box's ascent, which needs RichText integration.
4. **Writing modes other than LTR horizontal-tb:** Deferred until `direction` and `writing-mode`
   CSS properties are implemented.
5. **`order` does not affect tab order or accessibility tree** — visual reordering only.
6. **Column-reverse does not reorder the stacking context (z-index)** per spec §4.1; initial
   implementation treats it as visual reordering only.
7. **Flex items with percentage margins/paddings where the containing block's inline size is
   indefinite:** Deferred (rare edge case, requires multi-pass).

## Suggested Implementation Order

Phases are ordered by dependency. Each phase builds on previous ones and should be
accompanied by tests before proceeding.

1. **Phase 0:** CSS infrastructure (enums, property IDs, parsing, shorthand expansion, display routing)
   - Includes `flex`, `flex-flow`, `gap` shorthand expansion
   - Route `InlineFlex` to `FlexLayouter`, keep `Flex` on `BlockLayouter` for now
2. **Phase 1:** FlexLayouter skeleton + item collection + child positioning write-back
   - Must include `applyLayout()` (size/position write-back) to make anything testable
3. **Phase 2:** Main-axis sizing (flex base size, flex-grow, flex-shrink)
4. **Phase 3:** Cross-axis sizing (align-items, align-self, line sizing)
5. **Phase 4:** Direction + wrap variants
6. **Phase 5:** Gaps
7. **Phase 6:** Container shrink-wrap + intrinsic width computation
8. **Phase 8:** `min-width: auto` on flex items
9. **Phase 9:** Inline-flex behavior (already mostly done in `setDisplay()`)
10. **Phase 10:** Out-of-flow flex children
11. **Phase 11:** Edge cases and polish
12. **Phase 12:** RichText integration + wire `CSSDisplay::Flex` to `FlexLayouter`

**Phase 12 is the final gate:** Only after the RichText integration strategy is implemented
and real-world HTML tests pass can `CSSDisplay::Flex` be permanently routed to `FlexLayouter`.

Each phase should be a self-contained increment with tests that continue to pass through
subsequent phases. No phase should introduce regressions in earlier test coverage.

## Implementation Status (Current)

### What is built and working (Phases 0-11 mostly complete)

The `FlexLayouter` class at `include/eepp/ui/flexlayouter.hpp` / `src/eepp/ui/flexlayouter.cpp`
contains a working flex layout algorithm covering:

- **Item collection:** Sorted by `order`, skipping out-of-flow (`position:absolute/fixed`) and
  `display:none` children. Whitespace-only text nodes skipped. Anonymous flex items from bare
  text are NOT generated (known limitation, documented below).
- **Main-axis sizing:** `flex-basis` resolution from widget size/style; `flex-grow` distribution
  of positive free space; `flex-shrink` handling of negative free space; min/max main size
  clamping (`min-width:auto` supported via `getMinIntrinsicWidth()`).
- **Cross-axis sizing:** `align-items`/`align-self` handling for stretch, flex-start, flex-end,
  center. Baseline alignment uses flex-start positioning (no actual baseline measurement).
  Stretch properly respects explicit cross-size (`Fixed` size policy).
- **`justify-content`:** All values: flex-start, flex-end, center, space-between, space-around,
  space-evenly. Auto margins on main axis absorb free space before justify-content applies.
  Fixed `justify-content:center`/`flex-end` behavior (no clamping of negative free space).
- **`align-content`:** All values for multi-line containers. Space-distribute modes guarded
  against negative free space. `align-content` has no effect on single-line containers (correct).
- **Direction:** All four `flex-direction` values (row/row-reverse/column/column-reverse)
  with proper axis mapping and reverse handling.
- **Wrap:** `nowrap`/`wrap`/`wrap-reverse` with multi-line line breaking. Handles edge case
  where container is narrower than any single item (forces at least one item per line).
- **Gaps:** `column-gap` between items on main axis; `row-gap` between lines on cross axis.
  `normal` resolves to 0px correctly.
- **Flex shorthand:** Registered via `StyleSheetSpecification`, expands to `flex-grow`,
  `flex-shrink`, `flex-basis`. Handles `flex: auto`, `flex: none`, single/multi-value syntax.
- **Child layout:** Positioning via `setPixelsPosition()` and sizing via `setPixelsSize()`,
  with child layouter triggering for nested flex/block containers. Container shrink-wrap
  via `WrapContent` size policy.
- **Intrinsic widths:** Max-content width computed for `nowrap` containers (sum of flex bases
  + gaps + padding). `calculateIntrinsicWidths()` provides min/max intrinsic widths for
  flex containers.
- **`min-width:auto` / `min-height:auto`:** Content-based minimum size prevents collapse.
  Exposed via `getMinIntrinsicWidth()` on item layouters.
- **Resolved cross-size recalc:** `resolveCrossSizes()` sets each item's main-axis size to its
  resolved `targetMainSize` and re-lays it out before measuring cross size — prevents cross
  sizes computed with stale widths from previous layout passes.
- **Memory:** `SmallVector` with inline buffers used for item/line/index collections,
  avoiding heap allocations for typical flex containers (5-10 items, 1-3 lines).
- **Indefinite cross size:** When cross-axis is `WrapContent`, `containerCrossSize` is zeroed
  to prevent using stale container height from previous layout pass. Items re-measured with
  correct main-axis size after `resolveFlexibleLengths`.
- **`flex-basis: auto` vs `flex-basis: content`:** Both treated as `auto` (uses item's main
  size property). `content` keyword parsed but handled identically to `auto`.

### Current routing

- `CSSDisplay::Flex` → `BlockLayouter` (backward-compatible: real-world HTML uses `display: flex`
  on text-containing elements; block rendering preserves all existing behavior)
- `CSSDisplay::InlineFlex` → `FlexLayouter` (new layouter; no existing content uses
  `display: inline-flex`)

### Why `display: flex` is not yet routed to `FlexLayouter`

The FlexLayouter algorithm is mechanically correct, but routing `display: flex` to it
breaks real-world HTML pages. After extensive debugging, the root cause is **not** a
recursive cascade timing issue. It is an architectural mismatch between how eepp handles
inline elements and how CSS requires flex items to behave.

#### Root Cause Analysis

**The problem in two sentences:**

1. Inline elements (e.g., `<a>`, `<span>`) use `InlineLayouter`, whose `updateLayout()`
   is empty — they rely on their parent's `BlockLayouter::rebuildRichText()` to compute
   their size within the parent's inline formatting context.

2. When an inline element becomes a flex item, it is removed from its parent's inline
   stream. FlexLayouter calls `item->updateLayout()` expecting the item to compute its
   own size. But `InlineLayouter::updateLayout()` does nothing, so the item's size stays
   at `(0, 0)`.

**Concrete example:**

In `lobsters_simple.html`, a container has `display: flex; flex-direction: column`.
Its children include `<a>` elements (anchors). Anchors are `UIAnchorSpan` which extends
`UITextSpan` → `UIRichText` → `UIHTMLWidget`. Their `display` is `Inline`, so
`UILayouterManager::create()` returns `InlineLayouter` for them.

`InlineLayouter::updateLayout()` is empty:
```cpp
void updateLayout() override {}
```

When FlexLayouter calls `layoutSubtreeBottomUp(anchor)` → `anchor->updateLayout()`,
nothing happens. The anchor's width and height remain `0`. The flex container computes
its height from items with `0` height, so it collapses.

Even if we blockify the layouter (return `BlockLayouter` for flex item children),
`BlockLayouter::updateLayout()` has this guard:
```cpp
if (widget->isInline()) return;
```

`UITextSpan::isInline()` returns `true`. So `BlockLayouter` also returns early.
Either way, inline flex items never get their content measured.

**Why BlockLayouter containers don't have this problem:**

In a block container, inline elements stay in the parent's inline stream. The parent's
`BlockLayouter::rebuildRichText()` lays out all text and inline boxes together. The
parent computes its own height from the laid-out inline stream. Individual inline
children don't need their own layout — their size is an emergent property of the
parent's text layout.

**Why block-level flex items work fine:**

Block-level elements (e.g., `<div>`, `<p>`, `<nav>`) use `BlockLayouter` and are not
`isInline()`. Their `updateLayout()` rebuilds RichText and computes size correctly.
If all flex items were block-level, FlexLayouter would work without issues.

#### The Fix: CSS Blockification of Flex Items

Per CSS Flexbox §4, flex items are **blockified**: their outer display type becomes
block-level, regardless of their `display` property value. In eepp, this means flex
items must use `BlockLayouter` and `BlockLayouter` must process them even if they
are inline.

The fix requires **two coordinated changes**:

**Change 1: `UILayouterManager::create()` — blockify flex item children**

```cpp
UILayouter* UILayouterManager::create(CSSDisplay display, UIWidget* container) {
    // CSS Flexbox §4: flex items are blockified
    Node* parent = container->getParent();
    if (parent && parent->isWidget() && parent->isType(UI_TYPE_HTML_WIDGET)) {
        CSSDisplay parentDisplay = parent->asType<UIHTMLWidget>()->getDisplay();
        if (parentDisplay == CSSDisplay::Flex || parentDisplay == CSSDisplay::InlineFlex) {
            return eeNew(BlockLayouter, (container));
        }
    }

    switch (display) {
        // ... existing cases ...
    }
}
```

This ensures all direct children of flex containers get `BlockLayouter`, even if their
`display` is `inline`, `inline-block`, etc.

**Change 2: `BlockLayouter::updateLayout()` — skip inline check for flex items**

```cpp
void BlockLayouter::updateLayout() {
    // ... existing checks ...

    bool isFlexItem = false;
    Node* parent = widget->getParent();
    if (parent && parent->isType(UI_TYPE_HTML_WIDGET)) {
        CSSDisplay pd = parent->asType<UIHTMLWidget>()->getDisplay();
        isFlexItem = (pd == CSSDisplay::Flex || pd == CSSDisplay::InlineFlex);
    }

    if (widget->isInline() && !isFlexItem)
        return;

    // ... rest of updateLayout ...
}
```

This allows blockified inline elements to run the full BlockLayouter logic when they
are flex items, while preserving the early-return behavior for normal inline flow.

**Why this works:**

1. Flex container collects items via `collectFlexItems()`
2. `measureFlexItems()` sets each item's cross-size (width for column direction)
3. `layoutSubtreeBottomUp(item)` calls `item->updateLayout()`
4. For inline items, `getLayouter()` now returns `BlockLayouter` (Change 1)
5. `BlockLayouter::updateLayout()` runs because `isFlexItem` is true (Change 2)
6. BlockLayouter rebuilds RichText at the correct width, computing the item's height
7. `resolveFlexBasis()` reads the correct height
8. Flex algorithm proceeds with correct sizes

**Verification:** After these two changes, routing `CSSDisplay::Flex` to `FlexLayouter`
should pass the real-world HTML tests (`body_height_miscalculation.html`,
`lobsters_simple.html`) because inline elements inside flex containers will correctly
compute their text-based sizes.

**Potential issue with nested flex containers:** If a flex item is itself a flex
container, its children will also be blockified by Change 1. This is correct per CSS.

**Potential issue with reparenting:** If a widget is moved from a flex container to
a non-flex container, its layouter might still be BlockLayouter (cached). This is
handled naturally because `UIHTMLWidget::onDisplayChange()` recreates the layouter
when the display changes. If reparenting without display change, the widget should
call `notifyLayoutAttrChange()` or similar to trigger layouter recreation. This edge
case can be addressed later if needed.

### What `display: inline-flex` provides right now

The FlexLayouter is fully wired for `display: inline-flex`. This is usable for
programmatic UI widget layouts where:
- The flex container is a plain `UIHTMLWidget` (or subclass) created in code
- Flex items are pre-sized widgets (not RichText-dependent)
- No existing HTML content uses `display: inline-flex`

Example (pseudocode):
```cpp
auto* flex = UIHTMLWidget::New();
flex->setDisplay(CSSDisplay::InlineFlex);  // uses FlexLayouter
flex->setStyle("flex-direction: row; gap: 8px; align-items: center;");
flex->setParent(parent);
auto* child1 = UIWidget::New();
child1->setParent(flex);
child1->setStyle("flex-grow: 1;");
auto* child2 = UIWidget::New();
child2->setParent(flex);
child2->setStyle("width: 100px;");
// FlexLayouter will lay out child1 (flex-grow:1) and child2 (fixed 100px) in a row
```

### Implementation Notes and Architectural Details

#### `friend class FlexLayouter` in `UINode`

`FlexLayouter` needs to call `setInternalPixelsWidth()` and `setInternalPixelsHeight()`
on flex items during `measureFlexItems()` and `applyLayout()`. These methods are
`protected` on `UINode`. Rather than making them public (which would break encapsulation),
add `friend class FlexLayouter` to `UINode`:

```cpp
// in include/eepp/ui/uinode.hpp
class EE_API UINode : public Node {
    // ... existing members ...
    friend class FlexLayouter;
};
```

#### Size Policy in `setDisplay()`

`UIHTMLWidget::setDisplay()` already handles `Flex` and `InlineFlex`:

```cpp
if (mDisplay == CSSDisplay::InlineBlock || mDisplay == CSSDisplay::Inline ||
    mDisplay == CSSDisplay::InlineFlex) {
    if (getLayoutWidthPolicy() == SizePolicy::MatchParent)
        setLayoutWidthPolicy(SizePolicy::WrapContent);
} else if (mDisplay == CSSDisplay::Block || mDisplay == CSSDisplay::ListItem ||
           mDisplay == CSSDisplay::Flex) {
    if (getLayoutWidthPolicy() == SizePolicy::WrapContent &&
        mPosition != CSSPosition::Absolute && mPosition != CSSPosition::Fixed)
        setLayoutWidthPolicy(SizePolicy::MatchParent);
}
```

This means:
- `display: flex` containers have `width = MatchParent` (fill containing block)
- `display: inline-flex` containers have `width = WrapContent` (shrink-to-fit)
- Height policy is unchanged (typically WrapContent unless explicitly set)

#### `beginAttributesTransaction()` / `endAttributesTransaction()`

Like `BlockLayouter`, `FlexLayouter::updateLayout()` wraps its work in:
```cpp
mContainer->beginAttributesTransaction();
// ... layout logic ...
mContainer->endAttributesTransaction();
```

This batches widget updates and defers cascading `onSizeChange()` notifications until
the end of the layout pass. Without this, setting each child's position/size would
trigger immediate re-layout of descendants, causing O(n²) behavior.

#### `mPacking` Guard

`FlexLayouter` sets `mPacking = true` at the start of `updateLayout()` and
`mPacking = false` at the end. This prevents re-entrant layout calls. The guard is
also checked in `layoutSubtreeBottomUp()` to avoid recursive layout loops:
```cpp
if (layouter && !layouter->isPacking() && layouter != this) {
    widget->asType<UIHTMLWidget>()->updateLayout();
}
```

#### Flex Item Outer vs. Inner Size

The flex algorithm computes **content box** sizes (the size of the item's content).
When applying layout, `FlexLayouter` writes back the **border-box** size via
`setPixelsSize()`, which includes padding and border. The item's margin is NOT included
in the size write-back; margins are handled as offsets in the positioning logic.

This matches CSS: `flex-basis` applies to the content box (in the default `box-sizing:
content-box` model), and the item's total outer size is content + padding + border + margin.

#### `computeIntrinsicWidths()` for Wrap Containers (Fixed — G2)

The min-content width for wrap containers was fixed in `computeIntrinsicWidths()`.
For `nowrap` containers, max-content width = sum of flex bases + gaps.
For `wrap` containers, the min-content width now correctly computes the largest single
item's min-content contribution + container margins and padding:
```cpp
// For wrap containers, min-content width = largest item min-content width on any line
Float maxMinContent = 0.f;
for (auto& item : items) {
    Float minContent = item.getMinIntrinsicWidth();
    maxMinContent = eemax(maxMinContent, minContent);
}
mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
```
The fix categorizes items by line (simulating line breaking), and for each line finds
the largest min-content contribution, then takes the maximum across all lines.

Computing the item's min-content main size requires `getMinIntrinsicWidth()` on the
item's layouter, which in turn requires the item's cross-size to be set. This creates
a circular dependency for column-direction containers. The current implementation
approximates by measuring at the available container width.

#### Gap Property Parsing: `normal` → `0px`

In CSS, `row-gap: normal` and `column-gap: normal` resolve to `0px` in flexbox (unlike
CSS Grid where `normal` may have a different value). The `lengthFromStyle()` helper in
`FlexLayouter` reads the gap value from the style. If the value is `"normal"`,
`lengthFromValue()` returns `0` (since it can't parse "normal" as a length), which is
the correct behavior. No special handling needed.

### Files created/modified

| File | Status | Change |
|---|---|---|
| `include/eepp/ui/csslayouttypes.hpp` | Modified | `InlineFlex` enum, 6 flex enums (FlexDirection, FlexWrap, JustifyContent, AlignItems, AlignContent, AlignSelf) |
| `src/eepp/graphics/csslayouttypes.cpp` | Modified | String conversions for all new enums |
| `include/eepp/ui/css/propertydefinition.hpp` | Modified | 14 new PropertyId values (flex properties + gaps) |
| `src/eepp/ui/css/stylesheetspecification.cpp` | Modified | Property registrations + shorthand expansion (flex, flex-flow, gap) |
| `include/eepp/ui/uinode.hpp` | Modified | `friend class FlexLayouter` |
| `src/eepp/ui/uilayoutermanager.cpp` | Modified | `InlineFlex → FlexLayouter` routing; blockification of flex item children |
| `src/eepp/ui/uihtmlwidget.cpp` | Modified | `setDisplay()` Flex/InlineFlex handling, `getPropertiesImplemented()` |
| `src/eepp/ui/blocklayouter.cpp` | Modified | Skip `isInline()` early-return for flex items |
| `include/eepp/ui/flexlayouter.hpp` | **New** | FlexLayouter class with FlexItem, FlexLine, full API |
| `src/eepp/ui/flexlayouter.cpp` | **New** | Complete flex layout algorithm (~650 lines) |
