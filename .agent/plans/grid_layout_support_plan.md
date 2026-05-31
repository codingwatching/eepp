# CSS Grid Layout Support Plan

> Status: PROPOSED - CSS Grid Layout Module Level 1 implementation plan.
> Target spec: https://www.w3.org/TR/css-grid-1/ (W3C Candidate Recommendation Draft, 26 March 2025).

## Goal

Implement CSS Grid Layout Module Level 1 in eepp's HTML compatibility layer as a spec-compliant
`UILayouter` implementation, following the same successful pattern used by `FlexLayouter`:

- wire CSS display/property parsing first,
- keep per-feature state compact and lazily allocated,
- collect grid items in order-modified document order,
- implement the spec algorithms directly,
- add phase-local unit tests and realistic `UIHTML` fixture tests,
- validate each phase with focused tests before the full suite.

Grid is not a replacement for the existing `UIGridLayout` widget. `UIGridLayout` is an eepp UI
layout container with its own XML/properties. CSS Grid must live in the HTML/CSS layout path and be
routed through `UILayouterManager`, like `FlexLayouter`, `BlockLayouter`, and `TableLayouter`.

## Reference Specifications

- CSS Grid Layout Module Level 1: https://www.w3.org/TR/css-grid-1/
  - Section 5: Grid containers
  - Section 6: Grid items
  - Section 7: Defining the grid
  - Section 8: Placing grid items
  - Section 9: Absolute positioning
  - Section 10: Alignment and spacing
  - Section 11: Grid layout algorithm
- CSS Box Alignment Level 3: used by grid for `justify-*`, `align-*`, and `place-*`.
- CSS Display Level 3: automatic blockification for children of grid containers.
- CSS Sizing Level 3: intrinsic sizing terms used by the track sizing algorithm.

Spec facts that should guide implementation:

- `display: grid` establishes a block-level grid container; `display: inline-grid` establishes an
  inline-level grid container.
- A grid container establishes an independent grid formatting context; floats do not intrude and
  margins of its contents do not collapse.
- Children of a grid container are blockified, and text that is not only inter-element whitespace
  becomes an anonymous grid item.
- `order` applies to grid items and affects auto-placement and paint order.
- Grid placement is visual only and must not change DOM order, speech order, or navigation order.
- Gutters are treated as fixed-size tracks for track sizing and appear only between tracks.
- Absolute-positioned boxes whose containing block is a grid container are part of the Level 1
  spec, but W3C marks "application of grid placement to absolutely-positioned boxes" as at-risk.

## Scope

Complete Level 1 support for:

- `display: grid` and `display: inline-grid`
- explicit grid definition:
  - `grid-template-rows`
  - `grid-template-columns`
  - `grid-template-areas`
  - `grid-template`
- implicit grid definition:
  - `grid-auto-rows`
  - `grid-auto-columns`
  - `grid-auto-flow: row | column | dense | row dense | column dense`
  - `grid`
- line placement:
  - `grid-row-start`
  - `grid-row-end`
  - `grid-column-start`
  - `grid-column-end`
  - `grid-row`
  - `grid-column`
  - `grid-area`
- track sizing:
  - fixed lengths and percentages
  - `auto`
  - `min-content`
  - `max-content`
  - `minmax()`
  - `fit-content()`
  - `fr`
  - `repeat(<integer>, ...)`
  - `repeat(auto-fill, ...)` and `repeat(auto-fit, ...)`
- gutters:
  - `row-gap`
  - `column-gap`
  - `gap`
- grid alignment:
  - item alignment: `justify-items`, `justify-self`, `align-items`, `align-self`
  - grid alignment: `justify-content`, `align-content`
  - auto margins in both axes
- nested grids, grid inside flex, flex inside grid
- blockification and anonymous grid item text nodes
- grid item automatic minimum size
- `order`-modified auto-placement and paint order
- out-of-flow children skipped from normal grid sizing and positioned through the existing
  containing-block path

## Non-Scope For Initial Level 1 Completion

- CSS Subgrid, which belongs to Grid Level 2.
- Masonry layout, which is not Grid Level 1.
- Writing modes beyond current eepp horizontal LTR assumptions. Keep the data model axis-aware so
  later `direction` / `writing-mode` support does not require rewriting the layouter.
- Fragmentation and print/columns.
- `display: contents`, unless it is already independently supported by the HTML layer.
- Full CSSOM used-value serialization of track lists. Implement enough `getPropertyString()` to be
  debuggable and testable, but do not block layout correctness on browser-perfect serialization.
- Grid placement for absolute-positioned boxes in phase 1. Treat this as a hardening phase because
  it is at-risk in the spec and must integrate with `UIHTMLWidget::getContainingBlock()` carefully.

## Current State

What exists:

- `FlexLayouter` provides the implementation template for a modern CSS layout algorithm:
  compact item structs, lazy widget state, style reads during item collection, phase-specific helper
  methods, intrinsic width APIs, final pixel application, and baseline exposure.
- `UIHTMLWidget` already stores flex state lazily in `UIHTMLWidgetFlexState`.
- `UILayouterManager::create()` already routes `Flex` / `InlineFlex` and blockifies children of
  flex containers before display dispatch.
- `UIHTMLWidget::drawChildren()` already supports `order`-modified painting for flex. Grid should
  reuse or generalize this mechanism instead of adding a parallel draw path.
- `row-gap`, `column-gap`, `gap`, `justify-content`, `align-items`, `align-content`, `align-self`,
  and `order` already exist for flex, but their CSS Alignment value model is not broad enough for
  grid. Grid needs `normal`, `start`, `end`, `self-start`, `self-end`, and `justify-items` /
  `justify-self`.
- `src/tests/unit_tests/uihtml_flex_test.cpp` is the model for spec-phased unit tests.

What does not exist:

- `CSSDisplay::Grid` / `CSSDisplay::InlineGrid`
- a CSS Grid track-list parser
- grid placement value parsing
- named area parsing and validation
- `GridLayouter`
- `src/tests/unit_tests/uihtml_grid_test.cpp`

## Design Decisions

### 1. Create `GridLayouter`, Do Not Reuse `UIGridLayout`

Create:

| File | Purpose |
|---|---|
| `include/eepp/ui/gridlayouter.hpp` | `GridLayouter` declaration and compact structs |
| `src/eepp/ui/gridlayouter.cpp` | CSS Grid placement, sizing, alignment, and pixel placement |
| `src/tests/unit_tests/uihtml_grid_test.cpp` | Unit tests grouped by spec phase |

`UIGridLayout` is unrelated to CSS Grid semantics and should remain untouched except for avoiding
name confusion in comments and docs.

### 2. Add Lazy Grid State To `UIHTMLWidget`

Follow the final flex implementation, not the original flex plan draft. Flex now uses
`UIHTMLWidgetFlexState`, because repeated direct style parsing in the hot layout path was not the
best end state.

Add a sibling:

```cpp
struct UIHTMLWidgetGridState {
	std::string templateRows{ "none" };
	std::string templateColumns{ "none" };
	std::string templateAreas{ "none" };
	std::string autoRows{ "auto" };
	std::string autoColumns{ "auto" };
	CSSGridAutoFlow autoFlow{ CSSGridAutoFlow::Row };
	bool autoFlowDense{ false };
	std::string rowStart{ "auto" };
	std::string rowEnd{ "auto" };
	std::string columnStart{ "auto" };
	std::string columnEnd{ "auto" };
	std::string area{ "auto" };
	CSSJustifyItems justifyItems{ CSSJustifyItems::Normal };
	CSSJustifySelf justifySelf{ CSSJustifySelf::Auto };
};
```

Keep `row-gap`, `column-gap`, `align-items`, `align-content`, `align-self`, `justify-content`, and
`order` in shared layout state where possible, but expand their value model so flex behavior remains
unchanged and grid can use CSS Alignment's grid values.

### 3. Parse To Structured Values Once Per Layout Pass

Grid values are too rich for ad hoc string checks in the algorithm. `GridLayouter::readContainerStyle()`
should parse strings into structured, pass-local data:

```cpp
struct GridTrackSize;
struct GridTrackList;
struct GridLinePlacement;
struct GridAreaTemplate;
struct GridItem;
struct GridTrack;
```

Keep parser helpers private to `gridlayouter.cpp` unless they become useful for tests or CSSOM.
Prefer stack-backed `SmallVector` where bounds are modest. Use heap-backed containers only for
track lists and named-line maps that can grow with author CSS.

### 4. Store Lines As 1-Based CSS Lines, Convert Late

The spec and authors use 1-based line numbers and negative indexes from the explicit grid end.
Use 1-based inclusive start / exclusive end lines in placement structs:

```cpp
struct GridSpan {
	int startLine{ 1 };
	int endLine{ 2 };
};
```

Only convert to zero-based track indexes when indexing `mRows` / `mColumns`.

### 5. Keep Axes Separate

Unlike flex, grid always sizes both axes. Use the same algorithm helpers twice:

- columns are the inline axis,
- rows are the block axis.

This keeps future writing-mode support possible without baking `x == column` and `y == row` into
every helper.

### 6. Reuse Paint Sorting

Generalize `UIHTMLWidget::mNeedsOrderSort` from "flex only" to "order-modified layout context".
Grid should set it when any grid item has non-zero or differing `order`, and `drawChildren()` should
sort grid children with the same stable order used for flex. If z-index handling needs more than
the current path, add that in the paint-order phase with focused tests.

## Files To Modify

| File | Change Summary |
|---|---|
| `include/eepp/ui/csslayouttypes.hpp` | Add `Grid`, `InlineGrid`, grid auto-flow enum, grid/box alignment enums or aliases |
| `src/eepp/graphics/csslayouttypes.cpp` | Add string conversion helpers for new display and grid/alignment enums |
| `include/eepp/ui/css/propertydefinition.hpp` | Add grid property IDs and `justify-items` / `justify-self` |
| `src/eepp/ui/css/stylesheetspecification.cpp` | Register grid properties, initial values, and shorthands |
| `src/eepp/ui/css/stylesheetproperty.cpp` | Expand `grid-template`, `grid`, `grid-row`, `grid-column`, `grid-area`, and place shorthands |
| `include/eepp/ui/uihtmlwidget.hpp` | Add `UIHTMLWidgetGridState`, getters/setters, `isGrid()` |
| `src/eepp/ui/uihtmlwidget.cpp` | Apply, serialize, invalidate, draw-sort, and baseline integration for grid |
| `include/eepp/ui/gridlayouter.hpp` | New `GridLayouter` class |
| `src/eepp/ui/gridlayouter.cpp` | New CSS Grid Level 1 implementation |
| `src/eepp/ui/uilayoutermanager.cpp` | Route `Grid` / `InlineGrid`; blockify children of grid containers |
| `src/eepp/ui/uirichtext.cpp` | Treat children of grid containers like flex items for rich-text exclusion and text sizing |
| `include/eepp/ui/uinode.hpp` | Add `friend class GridLayouter` only if direct internal pixel setters are needed |
| `premake4.lua` and generated makefiles | Add `gridlayouter.cpp` and `uihtml_grid_test.cpp` |
| `src/tests/unit_tests/uihtml_grid_test.cpp` | New focused grid unit tests |
| `src/tests/unit_tests/uihtml_tests.cpp` | Add realistic HTML fixture tests |
| `.agent/rules/html-layout-architecture.md` | Document `GridLayouter` routing and supported subset after implementation |

## Property And Initial Value Table

| Property | Initial Value | Applies To |
|---|---|---|
| `display` | existing | add `grid`, `inline-grid` |
| `grid-template-rows` | `none` | grid containers |
| `grid-template-columns` | `none` | grid containers |
| `grid-template-areas` | `none` | grid containers |
| `grid-template` | `none` | grid containers |
| `grid-auto-rows` | `auto` | grid containers |
| `grid-auto-columns` | `auto` | grid containers |
| `grid-auto-flow` | `row` | grid containers |
| `grid` | `none` | grid containers |
| `grid-row-start` | `auto` | grid items and relevant abspos boxes |
| `grid-column-start` | `auto` | grid items and relevant abspos boxes |
| `grid-row-end` | `auto` | grid items and relevant abspos boxes |
| `grid-column-end` | `auto` | grid items and relevant abspos boxes |
| `grid-row` | `auto` | shorthand |
| `grid-column` | `auto` | shorthand |
| `grid-area` | `auto` | shorthand or named area |
| `justify-items` | `normal` | grid containers |
| `justify-self` | `auto` | grid items |
| `row-gap` / `column-gap` / `gap` | `normal` | already exists; grid `normal` resolves to 0 |
| `order` | `0` | already exists; now also applies to grid items |

Important compatibility note: existing flex registration treats `align-items` initial value as
`stretch`, because that is Flexbox's effective initial behavior in this implementation. CSS Grid's
alignment initial behavior is `normal`, which usually stretches non-replaced items. Do not regress
flex tests while expanding alignment parsing. A practical approach is:

- preserve flex getters' defaults as they are today,
- add grid-specific alignment getters that interpret absent values as grid `normal`,
- parse `normal` and `start` / `end` values without forcing flex to store invalid states.

## Implementation Plan

### Phase 0: CSS Infrastructure And Display Routing

Purpose: parse grid declarations and route `display: grid` safely without layout behavior beyond a
minimal no-crash layouter.

Steps:

1. Add `CSSDisplay::Grid` and `CSSDisplay::InlineGrid`.
2. Add grid property IDs.
3. Register grid longhands and simple shorthands.
4. Add `UIHTMLWidgetGridState`, getters, setters, and `isGrid()`.
5. Add `GridLayouter` skeleton with no-op collection and intrinsic width stubs.
6. Route `Grid` / `InlineGrid` to `GridLayouter`.
7. Add `parentIsGridContainer()` blockification in `UILayouterManager::create()`.
8. Update `UIRichText::rebuildRichText()` so direct child spans/text under a grid container are not
   incorrectly owned by the parent's inline formatting context.

Gate tests:

- `GridProperties.displayGridEnumConversions`
- `GridProperties.longhandDefaults`
- `GridContainer.emptyContainerDoesNotCrash`
- existing `FlexContainer*` tests still pass

### Phase 1: Track List Parser

Purpose: parse grid track syntax into a structured representation before implementing placement.

Supported in this phase:

- `none`
- fixed lengths and percentages
- `auto`
- `min-content`
- `max-content`
- `fr`
- `minmax(<track-breadth>, <track-breadth>)`
- `fit-content(<length-percentage>)`
- line-name brackets `[foo bar]`
- `repeat(<positive-integer>, <track-list>)`

Defer to Phase 12:

- `repeat(auto-fill, ...)`
- `repeat(auto-fit, ...)`

Data model:

```cpp
enum class GridTrackBreadthType {
	Length,
	Percentage,
	Flex,
	Auto,
	MinContent,
	MaxContent,
	FitContent
};

struct GridTrackBreadth {
	GridTrackBreadthType type;
	Float value;
	std::string raw;
};

struct GridTrackSize {
	GridTrackBreadth min;
	GridTrackBreadth max;
};

struct GridExplicitTrack {
	SmallVector<std::string, 2> beforeLineNames;
	GridTrackSize size;
};
```

Parser tests:

- fixed tracks: `100px 1fr auto`
- line names: `[start] 1fr [mid a] 2fr [end]`
- `repeat(3, 10px [x] 1fr)`
- invalid repeat count
- `minmax(100px, 1fr)`
- `fit-content(200px)`
- percentage tokens remain percentages until layout resolves them

### Phase 2: Named Areas Parser

Purpose: implement `grid-template-areas` validation and named area resolution.

Rules:

- each string creates one row,
- each non-dot token creates or extends a named area,
- all rows must have equal token count and at least one token,
- each named area must form one filled rectangle,
- named areas generate implicit `foo-start` and `foo-end` line names for both axes.

Tests:

- rectangular named areas pass,
- `.` null cells are accepted,
- uneven row widths invalidate the declaration,
- disconnected area tokens invalidate the declaration,
- named area placement via `grid-area: header`,
- implicit line names from areas resolve with line placement.

### Phase 3: Grid Item Collection And Blockification

Purpose: mirror `FlexLayouter::collectFlexItems()` for grid semantics.

Collection rules:

- include in-flow visible children,
- skip `display: none`,
- skip absolute/fixed children from normal grid layout,
- include `visibility: collapse` as normal for now unless a spec-specific collapse behavior is
  implemented later,
- ignore `float` and `clear` on grid items,
- ignore inter-element whitespace text nodes,
- wrap non-whitespace direct text into anonymous grid items.

Implementation note:

Flex currently lays out `UITextNode` anonymous flex items directly. Grid should initially do the
same for text nodes, but keep a clear boundary for future anonymous wrapper boxes if inline text needs
multiple child fragments inside one grid item.

Tests:

- `GridContainer.collectsInFlowChildren`
- `GridContainer.skipsOutOfFlowChildren`
- `GridContainer.skipsDisplayNoneChildren`
- `GridContainer.ignoresWhitespaceTextNodes`
- `GridContainer.anonymousTextNodeItem`
- `GridContainer.blockifiesInlineChildren`
- `GridContainer.floatDoesNotAffectGridItemPlacement`

### Phase 4: Definite Placement

Purpose: support line-based placement without auto-placement complexity.

Implement:

- parse `<grid-line>`:
  - `auto`
  - integer line
  - negative integer line against explicit grid end
  - `span <integer>`
  - custom identifier
  - `<integer> <custom-ident>`
  - `span <custom-ident>` and `span <integer> <custom-ident>`
- `grid-row` and `grid-column` shorthand expansion,
- `grid-area` four-value shorthand expansion,
- conflict handling:
  - swap start/end if reversed,
  - remove equal end line,
  - remove end span if both sides are spans,
  - named-only span becomes span 1,
- implicit track growth for explicit placements outside declared bounds.

Tests:

- `grid-column: 1 / 3`
- `grid-row: 2`
- `grid-column: 1 / span 2`
- negative end line: `grid-column: 1 / -1`
- reversed placement swaps lines,
- equal start/end becomes one-track span,
- named line placement,
- named area shorthand placement,
- explicit placement creates implicit tracks outside declared bounds.

### Phase 5: Sparse Auto-Placement

Purpose: implement CSS Grid Section 8.5 sparse item placement in order-modified document order.

Implement:

- `grid-auto-flow: row`
- `grid-auto-flow: column`
- auto-placement cursor,
- fully auto items,
- one-axis definite and one-axis auto items,
- auto spans,
- implicit row/column generation.

Tests:

- row flow fills columns then creates new rows,
- column flow fills rows then creates new columns,
- explicitly placed items occupy cells before auto items,
- auto-placement honors `order`,
- sparse placement does not backtrack into earlier holes,
- item with `grid-column: span 2` skips too-small holes.

### Phase 6: Dense Auto-Placement

Purpose: implement `grid-auto-flow: dense`.

Tests:

- dense backfills earlier holes,
- dense can visually reorder later items,
- dense still uses order-modified document order as the item iteration order,
- dense with spans does not overlap occupied cells.

### Phase 7: Basic Track Sizing

Purpose: implement enough of Section 11 to produce correct sizes for common explicit and implicit
grids, then harden intrinsic sizing in later phases.

Implement:

1. Establish explicit and implicit tracks.
2. Resolve fixed lengths.
3. Resolve percentages against definite container content size.
4. Treat indefinite percentages as `auto`.
5. Initialize intrinsic tracks with base size 0 and growth limit infinity.
6. Size `auto`, `min-content`, and `max-content` tracks from non-spanning item contributions.
7. Account for gutters as fixed tracks in the calculations.
8. Compute container auto block size as max-content grid size.

Tests:

- fixed columns/rows position items at exact offsets,
- percentage columns resolve against container width,
- percentage rows resolve against definite container height,
- indefinite percentage behaves as auto,
- auto column sizes to max child width,
- max-content track uses max intrinsic width,
- min-content track uses min intrinsic width,
- implicit tracks use `grid-auto-rows` / `grid-auto-columns`,
- gaps affect item positions and container auto size.

### Phase 8: Flexible Tracks

Purpose: implement `fr` tracks per Section 7.2.4 and Section 11.7.

Rules:

- distribute leftover space after non-flexible tracks hit max,
- divide by flex factors,
- preserve partial-fill behavior for total flex factors under 1,
- when available space is indefinite, resolve `fr` from max-content contributions and flex factors.

Tests:

- `1fr 1fr` splits leftover space equally,
- `1fr 2fr` uses 1:2 ratio,
- fixed plus `fr` subtracts fixed track first,
- `.25fr .25fr .25fr` consumes 75 percent of leftover, leaving the rest unfilled,
- indefinite container sizes `fr` tracks from content,
- `minmax(100px, 1fr)` respects the minimum.

### Phase 9: Spanning Item Track Sizing

Purpose: complete Section 11.5 distribution for items spanning multiple tracks.

Implement:

- intrinsic contributions from span > 1 items,
- distribute extra space across spanned tracks,
- respect growth limits,
- handle spanned gutters,
- support min/max track sizing functions with spanning items,
- keep the implementation iterative but bounded.

Tests:

- spanning item grows two auto columns,
- spanning item respects fixed track plus flexible/intrinsic track,
- max-content spanning item distributes required width,
- minmax growth limit caps distribution,
- gaps crossed by spans are included.

### Phase 10: Grid Alignment And Auto Margins

Purpose: implement Section 10 item alignment and grid alignment.

Implement:

- `justify-items` and `justify-self` in the inline axis,
- `align-items` and `align-self` in the block axis,
- `normal` behavior:
  - stretch non-replaced items without preferred aspect ratio or natural size,
  - use natural/fit-content sizing for replaced/aspect-ratio-like items where eepp exposes it,
- `start`, `end`, `center`, `stretch`,
- auto margins absorb positive free space before self-alignment,
- overflowing auto margins resolve to zero,
- `justify-content` and `align-content` align the whole grid inside the container,
- `space-between`, `space-around`, `space-evenly`, and `stretch` for track distribution.

Tests:

- default `normal` stretches ordinary item to its grid area,
- `justify-self: center`,
- `align-self: end`,
- `justify-items` inherited by items unless overridden,
- auto margin centers item and disables self-alignment on that axis,
- `justify-content: center` moves all tracks,
- `align-content: space-between` changes row gaps after track sizing,
- `align-content: stretch` expands auto tracks.

### Phase 11: `inline-grid`, Intrinsic Sizes, And Baseline

Purpose: integrate grid containers with the surrounding formatting context.

Implement:

- `display: inline-grid` as an atomic inline-level box like `inline-block` / `inline-flex`,
- shrink-wrap inline-grid using max-content contributions,
- `GridLayouter::computeIntrinsicWidths()`,
- grid container baseline for use by parent inline/flex/grid contexts,
- nested grid baseline from first/last relevant grid item per spec, with practical fallback to the
  first in-flow item baseline or content-box bottom when unavailable.

Tests:

- inline-grid participates in a line and exposes non-zero size,
- inline-grid shrink-wraps fixed and intrinsic tracks,
- grid inside flex can baseline-align,
- flex inside grid receives its assigned area size before flex layout,
- nested grid inside grid lays out once without re-entry loops.

### Phase 12: `repeat(auto-fill)` And `repeat(auto-fit)`

Purpose: complete auto-repeat support.

Implement:

- compute repetition count from definite container size,
- honor gaps,
- clamp to at least one repetition when required by the spec,
- collapse empty `auto-fit` tracks after placement,
- keep `auto-fill` tracks present even when empty.

Tests:

- `repeat(auto-fill, 100px)` creates the expected number of columns,
- `auto-fill` includes empty tracks,
- `auto-fit` collapses empty tracks,
- auto-repeat with gaps,
- auto-repeat with minmax.

### Phase 13: Shorthand Completion

Purpose: finish the author-facing property surface after core layout is correct.

Implement:

- `grid-template` shorthand,
- `grid` shorthand,
- `place-items`,
- `place-self`,
- `place-content` if not already supported globally.

Tests:

- `grid-template: "head head" auto "nav main" 1fr / 100px 1fr`
- `grid: auto-flow 1fr / 100px`
- `grid: none / auto-flow 1fr`
- shorthand resets unspecified subproperties to their initial values,
- `place-items: center stretch`,
- `place-self: end center`,
- `place-content: center space-between`.

### Phase 14: Absolute Positioning In Grid Containing Blocks

Purpose: integrate Section 9 after normal grid layout is stable.

Implement carefully:

- out-of-flow children do not affect normal grid sizing,
- grid placement properties on abspos children can define their containing grid area,
- offsets then resolve against that containing block,
- if placement is `auto`, use the grid container padding box / existing containing block behavior,
- preserve current `UIHTMLWidget::positionOutOfFlowChildren()` validation.

Tests:

- absolute child skipped by normal grid layout,
- absolute child with normal `left/top` positions against grid container,
- absolute child with `grid-area` uses that area as containing block,
- fixed child remains scene-root relative,
- existing `UIHTMLWidget.positionOutOfFlow_*` tests pass.

### Phase 15: Paint Order, Z-Index, And Overlap

Purpose: finish visual ordering behavior for overlapping grid items.

Implement:

- stable order-modified paint order for grid children,
- reverse nothing for grid; placement controls geometry, not source order direction,
- z-index stacking behavior for grid items with `z-index` other than auto, even when static,
- avoid changing non-grid child paint order.

Tests:

- overlapping grid items paint in order-modified document order,
- negative order paints before default order,
- equal order remains stable source order,
- z-index item paints above later source-order item,
- flex paint-order tests still pass.

### Phase 16: Real HTML Fixtures And Browser Comparison

Purpose: validate real-world CSS Grid usage beyond algorithm unit tests.

Add `UIHTML` fixture tests for:

- card gallery: `repeat(auto-fill, minmax(...))`
- app shell: named template areas with header/nav/main/footer
- form layout: labels and controls with `grid-auto-flow: row dense`
- nested grid inside flex header/body layout
- overlapping named areas with z-index
- inline-grid inside paragraph

When practical, manually compare one or two fixtures against a browser capture and encode layout
invariants rather than screenshot pixels.

## Suggested `GridLayouter` Shape

```cpp
class EE_API GridLayouter : public UILayouter {
  public:
	GridLayouter( UIWidget* container ) : UILayouter( container ) {}

	void updateLayout() override;
	void computeIntrinsicWidths() override;
	Float getMinIntrinsicWidth() override;
	Float getMaxIntrinsicWidth() override;
	Float getBaseline() const { return mContainerBaseline; }

  protected:
	void readContainerStyle();
	void collectGridItems();
	void resolveTemplateAreas();
	void resolveDefinitePlacements();
	void autoPlaceItems();
	void buildImplicitTracks();
	void sizeTracksForAxis( GridAxis axis );
	void alignTracks();
	void alignItems();
	void applyLayout();

	SmallVector<GridItem, 16> mItems;
	std::vector<GridTrack> mColumns;
	std::vector<GridTrack> mRows;
	GridAreaTemplate mTemplateAreas;
	Float mColumnGap{ 0.f };
	Float mRowGap{ 0.f };
	Float mContainerBaseline{ 0.f };
};
```

Keep the public API as small as `FlexLayouter`. Most algorithm detail should stay private and
test-covered through behavior, not through a broad exported helper surface.

## Test Plan

Create `src/tests/unit_tests/uihtml_grid_test.cpp` and mirror the flex test organization:

- `GridProperties.*` for enum conversion, defaults, parser, and shorthand expansion.
- `GridContainer.*` for direct layout behavior.
- `UIHTML.Grid*` for realistic fixtures in `uihtml_tests.cpp`.

Minimum unit groups:

| Group | Coverage |
|---|---|
| Phase 0 | display routing, empty container, property defaults |
| Parser | track lists, line names, repeat, minmax, fit-content, template areas |
| Collection | in-flow children, out-of-flow skip, text nodes, blockification |
| Placement | definite lines, spans, negative lines, names, areas, conflicts |
| Auto-placement | row/column flow, sparse/dense, order |
| Track sizing | fixed, percentage, auto, min/max-content, implicit tracks |
| Flexible tracks | `fr`, partial-fill, indefinite container |
| Spanning | multi-track intrinsic contribution distribution |
| Alignment | item self alignment, content alignment, auto margins, gaps |
| Integration | inline-grid, nested grid/flex, baselines, abspos, paint order |

Name tests after the behavior, not the implementation helper. Example:

```cpp
UTEST( GridContainer, fixedTracksPlaceItemsAtGridLines )
UTEST( GridContainer, templateAreasPlaceNamedItems )
UTEST( GridContainer, sparseAutoPlacementDoesNotBackfillHoles )
UTEST( GridContainer, frTracksDistributeLeftoverSpace )
UTEST( GridContainer, spanningItemGrowsAutoTracks )
UTEST( GridContainer, justifySelfCenterPositionsInsideGridArea )
```

## Validation Gate

Before considering a phase complete:

```sh
make -C make/linux -j$(nproc)
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="GridProperties*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="GridContainer*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTML.Grid*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="FlexContainer*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="FlexProperties*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug
git diff --check
```

If a phase touches out-of-flow positioning, also run:

```sh
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTML.HeightExpansion"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTMLWidget.positionOutOfFlow_*"
```

After editing C++ sources:

```sh
git diff --name-only -- '*.c' '*.cpp' '*.h' '*.hpp' | xargs clang-format -i
```

## Checkpoint Policy

Use the same checkpoint discipline as the flex implementation.

At the end of every passing phase:

1. Run phase-specific focused tests.
2. Run relevant flex regression tests if shared alignment, gap, order, display, or rich-text code
   changed.
3. Run the full suite when the phase touches shared layout infrastructure.
4. Run `git diff --check`.
5. Run `clang-format` on modified C/C++ files.
6. Create a recoverable stash checkpoint:
   ```sh
   git stash push -u -m "grid-layout phase N: <short description>"
   ```
7. Re-apply it immediately:
   ```sh
   git stash apply stash@{0}
   ```
8. Record validation and stash message in phase notes.

Do not checkpoint a failing phase as complete.

## Risk Register

| Risk | Mitigation |
|---|---|
| Track sizing algorithm complexity | Implement fixed/auto/fr first, then spanning distribution, each with focused tests |
| Alignment enum changes regress flex | Preserve flex defaults and run `FlexContainer*` / `FlexProperties*` after shared CSS alignment edits |
| RichText accidentally owns grid children | Mirror flex's parent-container checks and add tests for inline spans/text under grid |
| Auto-placement overlap bugs | Maintain an explicit occupancy matrix and test sparse/dense with spanning items |
| Infinite implicit grid growth | Cap internal growth to the highest required line plus bounded cursor progress; fail safe on pathological input |
| Percentage/intrinsic cycles | Follow Section 11 order, resolve columns then rows, repeat only the specified once-only intrinsic update |
| Out-of-flow regressions | Keep abspos grid placement until late phase and run existing positioned-layout tests |
| Existing `UIGridLayout` confusion | Keep CSS Grid names under `GridLayouter` and document that `UIGridLayout` is unrelated |
| Performance on large grids | Use stack vectors for normal cases, avoid per-cell heap allocations where track spans can be represented as ranges |

## Definition Of Done

- `display: grid` and `display: inline-grid` route through `GridLayouter`.
- All scoped Level 1 properties parse, serialize reasonably, invalidate layout, and participate in
  layout where applicable.
- Grid placement and track sizing follow CSS Grid Sections 8 and 11.
- Grid item and grid content alignment follow Section 10 for current horizontal LTR mode.
- Anonymous text grid items, blockification, nested flex/grid, auto margins, gaps, order, and paint
  order are covered by tests.
- Realistic `UIHTML.Grid*` fixture tests cover common web layout patterns.
- Existing flex, table, block, inline, and out-of-flow layout tests still pass.
- `.agent/rules/html-layout-architecture.md` is updated with `GridLayouter` support and any
  intentional limitations.
