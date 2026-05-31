# UI Inline Formatting Context Plan

## Goal

Introduce a UI-specialized inline formatting path that can lay out HTML/UI inline
content directly from the existing `Node` / `UIWidget` / `UITextNode` tree,
without first reconstructing a parallel `Graphics::RichText::InlineItem` tree.

The current first-class inline boxes implementation is functionally correct and
must remain the baseline. This plan is about reducing duplicated data, improving
hot-path allocation behavior, and separating the generic `Graphics::RichText`
frontend from the browser-oriented UI frontend.

The desired end state is:

- `Graphics::RichText` remains a generic graphics primitive.
- The current `RichText::InlineItem` path remains valid for non-UI callers.
- UI/HTML inline layout can consume the existing UI node tree directly.
- Shared inline layout logic is reused instead of duplicating line-breaking,
  baseline, float, selection, and fragment behavior.
- UI fragments reference source UI nodes and drawables instead of copying large
  style/widget state into a mirrored RichText tree.
- The migration is incremental, with the existing RichText path available as the
  verified fallback until each UI phase is proven.

## Current Problem

The current UI pipeline is:

1. `UIRichText::rebuildRichText()` walks the UI child tree.
2. It builds a parallel `Graphics::RichText::mInlineItems` tree.
3. `RichTextInlineLayouter` flattens that tree into layout runs.
4. Layout produces `RenderParagraph`, `RenderSpan`, and `InlineFragment` data.
5. `BlockLayouter::positionRichTextChildren()` groups fragments by source node
   and maps geometry back to UI widgets/text nodes.
6. Drawing uses `RenderSpan` for text/atomic payloads and `InlineFragment` for
   inline box backgrounds, borders, decorations, and hit boxes.

This is acceptable for a generic rich text widget. For browser-like HTML it is
less attractive because:

- The UI node tree already exists and already stores most CSS/widget state.
- The RichText inline tree duplicates source hierarchy.
- Several fields are copied or bridged from UI to Graphics only to be mapped
  back to UI later.
- Some data is carried through several structures:
  - `UIWidget` / `UITextNode`
  - `RichText::InlineItem`
  - `InlineLayoutRun`
  - `RenderSpan`
  - `InlineFragment`
  - `BlockLayouter::FragmentBucket`
- Background/border fidelity has required more bridge fields, for example
  drawable pointers and fragment-color override flags.
- Allocation pressure remains visible in vectors, text objects, fragment lists,
  and repeated temporary structures.

## Non-Goals

- Do not make `Graphics::RichText` depend on UI types.
- Do not remove `Graphics::RichText::InlineItem` in this project phase.
- Do not rewrite the whole UI layout engine in one pass.
- Do not regress drawing, selection string, hit testing, selection rectangles,
  inline background/border painting, line-height, vertical-align, floats, tables,
  details/summary, forms, or invalid-width performance.
- Do not reintroduce legacy `RichText::Block`, `SpanBlock`, `CustomBlock`,
  `getBlocks()`, or `mBlocks`.
- Do not add compatibility layers for deleted RichText block APIs.

## Architectural Direction

Split the inline formatting engine into two conceptual layers:

1. A storage-agnostic inline layout core.
2. One or more frontends/providers that expose inline items to that core.

The initial frontends should be:

- `GraphicsRichTextInlineProvider`
  - Reads `Graphics::RichText::mInlineItems`.
  - Preserves current generic RichText behavior.
- `UIInlineProvider`
  - Walks `Node` / `UIWidget` / `UITextNode` children directly.
  - Resolves CSS/UI metrics from existing widgets.
  - Produces UI-oriented fragments that reference source nodes.

The layout core should not know whether items came from `RichText::InlineItem` or
the UI tree. It should operate on a compact item view/cursor API.

## Proposed Types

Exact names can change, but ownership boundaries should remain.

### Inline Item View

The shared layout core needs a lightweight, non-owning view of the current inline
item.

Conceptual shape:

```cpp
struct InlineItemView {
    enum class Type { TextRun, BoxStart, BoxEnd, AtomicBox, LineBreak };

    Type type;
    InlineSourceId source;
    InlineStyleId style;
    InlineBoxMetrics box;
    InlineTextRun text;
    InlineAtomicMetrics atomic;
    InlineFloat floatType;
    InlineClear clearType;
    BaselineAlignValue baselineAlign;
};
```

Important requirements:

- The view must not own strings, widgets, text objects, or child vectors.
- The view can be invalidated by advancing the provider cursor.
- The layout core may copy only the small fields needed for output fragments.
- Source identity must be stable enough to map layout results back to nodes.

### Inline Provider

The provider exposes the inline content stream in tree order.

Conceptual API:

```cpp
class InlineProvider {
  public:
    void reset();
    bool next( InlineItemView& out );
    FontStyleConfig resolveTextStyle( InlineStyleId style ) const;
    String::View text( const InlineTextRun& run ) const;
    Sizef atomicSize( const InlineAtomicMetrics& atomic ) const;
    Float atomicBaseline( const InlineAtomicMetrics& atomic ) const;
};
```

The API may be implemented as templates instead of virtual calls if profiling
shows virtual dispatch overhead matters. Start with the simplest design that
keeps the storage boundary clean.

### UI Source Identity

Avoid `void*` proliferation in new UI-specific structures.

Use a small typed source handle:

```cpp
struct UISourceRef {
    enum class Type { None, TextNode, Widget };
    Type type{ Type::None };
    Node* node{ nullptr };
};
```

The generic `Graphics::RichText` path can keep `InlineSource` as it exists today.
The UI path can use typed UI source refs internally.

### Layout Output

The shared core should produce storage-neutral line results, but the UI path
should avoid copying full widget metadata into fragments.

Conceptual output:

```cpp
struct InlineLineBox {
    Float y;
    Float height;
    Float baseline;
    Float width;
    SmallVector<InlineFragmentRef, 16> fragments;
};

struct InlineFragmentRef {
    enum class Type { TextRun, Box, AtomicBox };
    Type type;
    UISourceRef source;
    Rectf bounds;
    Rectf paintBounds;
    Int64 startCharIndex;
    Int64 endCharIndex;
    bool startsInlineBox;
    bool endsInlineBox;
    InlineFragmentPaint paint;
};
```

`InlineFragmentPaint` should be compact. For the UI path, prefer references to
existing UI drawables/styles rather than copied values.

## UI-Specific Implementation Strategy

### 1. Keep The Current RichText Path As Baseline

Before introducing the UI-specific path:

- Preserve the current `UIRichText::rebuildRichText()` path.
- Keep all current tests passing.
- Add any missing regression coverage for behavior discovered during the review:
  - background color plus border radius on inline anchors,
  - background images on inline boxes,
  - split inline background/border fragments,
  - nested inline box hit boxes.

The first UI-specific implementation should be hidden behind a feature flag or
internal switch so test comparisons can run both paths.

Possible flag:

```cpp
enum class InlineLayoutBackend {
    GraphicsRichText,
    UINodeTree
};
```

Default must remain the current backend until the UI path reaches parity.

### 2. Extract Layout Core From `RichTextInlineLayouter`

Move the reusable algorithms out of the private implementation that directly
depends on `std::vector<RichText::InlineItem>`.

Candidate responsibilities to extract:

- text run measurement,
- text wrapping,
- line construction,
- float placement,
- baseline alignment,
- line metric recomputation,
- fragment reconstruction,
- first/last inline box edge detection,
- selection/hit-test geometry helpers.

Do not extract everything at once. Start with a low-risk seam:

1. Build layout runs from a provider.
2. Keep the rest of `RichTextInlineLayouter` unchanged.
3. Prove that the RichText provider produces byte-for-byte equivalent output for
   current tests.

### 3. Introduce `GraphicsRichTextInlineProvider`

This provider adapts existing `RichText::InlineItem` data to the new provider
API.

Acceptance criteria:

- No behavior changes.
- `RichText.*` passes.
- `UIRichText.*` passes through the existing RichText backend.
- No new UI includes in `Graphics::RichText`.

This phase proves the provider abstraction without changing UI behavior.

### 4. Introduce `UIInlineProvider`

The UI provider should walk the actual children of the `UIRichText` container.

Responsibilities:

- Traverse text nodes and inline widgets in tree order.
- Apply the same whitespace collapsing rules as current
  `UIRichText::rebuildRichText()`.
- Represent true inline widgets as box start/end items.
- Represent inline-blocks, replaced elements, floats, controls, list markers,
  and line breaks as atomic items.
- Skip invisible nodes.
- Skip out-of-flow descendants where current layout does.
- Resolve margins, padding, borders, background, text decoration, line-height,
  baseline alignment, float, and clear from existing `UIWidget` state.
- Keep source references as `UITextNode*` / `UIWidget*`.

Important: the UI provider must not allocate a mirror tree.

The provider may keep a traversal stack. That stack should use `SmallVector` and
contain only node pointers and small state:

```cpp
struct UITraversalFrame {
    Node* node;
    Node* nextChild;
    UIWidget* inlineBox;
    bool emittedStart;
};
```

### 5. Shared Whitespace Collapsing

Whitespace collapsing is currently embedded in `UIRichText::rebuildRichText()`.
The UI provider needs equivalent behavior without materializing a copied text
tree.

Plan:

- Extract whitespace state into a small helper:

```cpp
struct InlineWhitespaceState {
    bool shouldCollapse;
    bool lastRunEndsWithSpace;
    bool atBlockBoundary;
};
```

- For each text node, produce a `String::View` or a compact transformed buffer.
- Avoid allocating a new `String` when no trimming/collapse is needed.
- If trimming is needed, prefer range slicing over copying.
- If internal whitespace normalization is needed, use a reusable scratch buffer
  owned by the provider or layout context.

Acceptance tests:

- Existing whitespace tests stay green:
  - `UIRichText.WhitespaceCollapseTest`
  - `UIRichText.WhitespaceCollapseCodeTest`
  - `UIRichText.WhitespaceCollapseBRTest`
  - `UITextNode_Regression.WhitespaceCollapseDoesNotCreateSpuriousNodes`

### 6. UI Text Measurement Without Per-Run `Text` Allocation

The current path creates `Text` objects for inline runs and render spans. The UI
path should eventually avoid that for layout measurement.

Incremental strategy:

1. Keep existing `Text` drawing for final rendering.
2. Introduce a measurement helper that can compute:
   - width,
   - wraps,
   - min intrinsic width,
   - max intrinsic width,
   - line height,
   - baseline,
   from `String::View + FontStyleConfig`.
3. Cache shaped/wrapped results by source text node generation, style generation,
   max width, and shaper settings.
4. Only materialize `Text` or shaped draw payloads for final draw fragments.

Do not start with a large text/shaper rewrite. First remove repeated allocation
from obvious layout-only paths.

### 7. UI Fragment Mapping

Once the UI provider exists, `BlockLayouter::positionRichTextChildren()` should
consume UI fragments directly.

Current `BlockLayouter` groups `RichText::InlineFragment`s by source pointer.
The UI path can avoid the map when possible:

- Fragments already contain typed `UISourceRef`.
- During fragment generation, append fragment pointers/ranges directly to a
  per-node layout cache.
- Each `UITextNode` / `UITextSpan` can receive hit boxes from its source
  fragments without a separate `UnorderedMap` pass.

Potential structures:

```cpp
struct UIInlineLayoutResult {
    SmallVector<InlineLineBox, 8> lines;
    SmallVector<UIInlineFragment, 64> fragments;
    UnorderedMap<Node*, FragmentRange> sourceRanges; // transitional only
};
```

Longer term, avoid `sourceRanges` by storing fragment ranges on the relevant UI
nodes during layout, or by preserving fragment order and resolving during the
same traversal.

### 8. Drawing

The UI-specific draw path should draw from UI fragments and source widgets.

Rules:

- Text fragments draw text using source text/style data.
- Inline box backgrounds should use the source widget background drawable when
  it has real background data or radius.
- Font background color and widget background radius must compose correctly.
- Border drawables should come from the source widget.
- Split inline boxes must respect `startsInlineBox` and `endsInlineBox`.
- If the current drawable APIs cannot suppress continuation sides cleanly, add a
  drawable-level side mask/clipping API rather than duplicating border painting
  logic in RichText.

Suggested follow-up API:

```cpp
enum class BoxSideMask : Uint8 {
    None = 0,
    Left = 1 << 0,
    Top = 1 << 1,
    Right = 1 << 2,
    Bottom = 1 << 3,
    All = Left | Top | Right | Bottom
};

struct DrawableBoxPaintOptions {
    BoxSideMask sides{ BoxSideMask::All };
    const Color* colorOverride{ nullptr };
};
```

Then add an overload for UI drawables that need it:

```cpp
void draw( const Vector2f& position, const Sizef& size,
           const DrawableBoxPaintOptions& options );
```

Do not add this until the current fragment behavior is covered by tests.

## Current RichText Optimization Plan

These optimizations can be done even before the UI-specialized backend exists.

### A. Reuse Persistent Output Storage

Current public/internal storage:

- `std::vector<InlineItem> mInlineItems`
- `std::vector<InlineFragment> mInlineFragments`
- `std::vector<RenderParagraph> mLines`
- `std::vector<RenderSpan> RenderParagraph::spans`
- `std::vector<InlineItem> InlineItem::Box::children`

Do not blindly replace all of these with `SmallVector`.

Recommended changes:

- Keep top-level `mInlineItems`, `mInlineFragments`, and `mLines` as
  `std::vector` unless profiling shows most documents are tiny. These can grow
  large in HTML.
- Preserve capacity across rebuilds. Prefer `clear()` and refill over assigning
  a temporary vector.
- Change `RichTextInlineLayouter::rebuildFragments()` to fill an output vector
  passed by reference:

```cpp
static void rebuildFragments( const InlineItems& items,
                              const Lines& lines,
                              std::vector<InlineFragment>& out );
```

- Avoid `mInlineFragments = rebuildFragments(...)` because it may discard useful
  capacity or force extra moves.
- Consider `SmallVector<RenderSpan, 8>` for `RenderParagraph::spans` only after
  checking object size and typical spans-per-line.

### B. Reduce `Text` Object Allocation

High-priority allocation source:

- `RichText::addInlineText()` allocates `std::shared_ptr<Text>`.
- `appendTextRenderSpan()` creates render text payloads for substrings.

Incremental plan:

1. Add a `RenderTextRun` payload that can reference:
   - source `Text*`, or
   - source string view/range plus `FontStyleConfig`.
2. Keep existing `Text` draw path initially.
3. Reuse `Text` objects from a per-`RichText` pool for render spans.
4. Reset and refill pooled `Text` objects during layout.
5. Later, replace pooled `Text` objects with a lighter shaped-text fragment if
   Text supports enough low-level drawing hooks.

Acceptance criteria:

- `RichText.RichTextTest` remains green with shaper disabled/enabled/enabled
  without optimizations.
- `FontRendering.TextBackgroundColor` remains green.
- Selection color application still works.

### C. Cache Ancestor Metadata

Current hot pattern:

- paths are stored as `SmallVector<size_t, 4>`,
- helpers repeatedly resolve inline ancestor boxes,
- first/last leaf checks can recursively scan children.

Optimization:

- During layout-run construction, compute an `InlineAncestorChain` once.
- Store cached values on `InlineLayoutRun`:
  - effective baseline alignment,
  - inherited text decoration,
  - start spacing,
  - end spacing,
  - first/last leaf flags for each ancestor edge,
  - line-height edge contribution.

This should reduce repeated calls to:

- `resolveInlineBox()`
- `inlineAncestorStartSpacing()`
- `inlineAncestorEndSpacing()`
- `inlineAncestorTextDecoration()`
- `isFirstInlineLeafInBox()`
- `isLastInlineLeafInBox()`

### D. Flatten Internal Inline Storage

For generic `Graphics::RichText`, consider replacing nested child vectors with a
flat arena.

Current:

```cpp
struct Box {
    std::vector<InlineItem> children;
};
```

Potential:

```cpp
struct InlineItem {
    InlineItemKind kind;
    size_t firstChild;
    size_t childCount;
    size_t parent;
    Payload payload;
};

std::vector<InlineItem> mInlineItems;
```

Benefits:

- fewer per-box heap allocations,
- better traversal locality,
- parent access without path resolution,
- child ranges instead of recursive vector ownership,
- easier fragment source indexing.

Costs:

- builder API becomes more complex,
- moving/erasing items is harder,
- tests and helper functions need significant updates.

Recommendation:

- Do not start here.
- First extract provider/layout seams.
- If we keep optimizing generic `RichText`, flat arena is the bigger structural
  optimization after storage reuse and text allocation reduction.

### E. Avoid Hot-Path Hash Maps Where Possible

Current state:

- `BlockLayouter` owns reusable `UnorderedMap<void*, FragmentBucket>` maps.
- Bucket lists use `SmallVector`.

Next improvements:

- For the UI-specialized backend, avoid grouping by pointer after layout.
- Emit source fragment ranges during fragment generation.
- If maps remain needed, consider:
  - keeping them persistent and capacity-stable,
  - using typed keys (`Node*`) instead of `void*`,
  - clearing buckets without freeing bucket vectors,
  - avoiding `operator[]` when lookup-only behavior is intended.

## Migration Phases

Every phase has a hard completion gate:

- Build and all required focused tests for that phase must pass.
- The full unit test suite must pass unless the phase explicitly says it is
  documentation-only and has no code changes.
- `git diff --check` must pass.
- A recovery checkpoint must be saved with `git stash`.
- The checkpoint must be re-applied immediately so the working tree keeps the
  completed phase changes.

Checkpoint workflow:

```bash
git stash push -u -m "ui-inline-formatting-context phase N: <short description>"
git stash apply stash@{0}
```

Do not use `git stash pop` for checkpoints. The stash entry must remain in the
stash list as the last known-good recoverable snapshot. If a later phase fails or
becomes hard to unwind, recover from the latest passing phase checkpoint instead
of manually reconstructing the working tree.

Each phase's final notes must record:

- validation commands run,
- pass/fail counts,
- any known flaky reruns, especially Xvfb cookie failures,
- stash checkpoint message,
- stash reference if available.

### Phase 0: Baseline And Metrics

Purpose: establish behavior and performance numbers before refactoring.

Tasks:

- Record current focused test timings:
  - `UIRichText.InvalidWidthLengthComputation3`
  - `UIRichText.*`
  - `UIBackground.*`
  - `UIHTMLTable.complexLayout*`
  - `UIHTMLFloat.*`
- Add a debug allocation counter or targeted instrumentation if available.
- Document the number of:
  - inline items,
  - render spans,
  - inline fragments,
  - text objects,
  for representative HTML examples.

Representative examples:

- `bin/unit_tests/assets/html/background_positioning.html`
- inline anchor wrapping case,
- inline-block browser test,
- details/summary fixture,
- table complex layout fixture.

Exit criteria:

- Baseline numbers are recorded in the agent loop or this plan.
- No code behavior changes.
- Required validation passes.
- Stash checkpoint is created and re-applied, even if the phase only records
  baseline documentation. This preserves a named restore point before code
  refactoring starts.

### Phase 1: Extract Shared Layout Run Provider

Purpose: introduce the provider seam while preserving current behavior.

Tasks:

- Define provider/item-view types in a Graphics-safe location.
- Implement `GraphicsRichTextInlineProvider`.
- Convert `buildLayoutRuns()` to consume the provider.
- Keep downstream layout code unchanged as much as possible.

Validation:

- `RichText.*`
- `UIRichText.*`
- `UIHTMLFloat.*`
- `UIHTMLTable.complexLayout*`
- full suite
- `git diff --check`

Exit criteria:

- Output behavior is unchanged.
- Current RichText backend remains default.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 2: Extract Fragment Builder Provider Hooks

Purpose: make fragment reconstruction independent of `RichText::InlineItem`.

Tasks:

- Move leaf collection and box accumulator logic behind provider/source queries.
- Keep `RichText::InlineFragment` output for the Graphics backend.
- Introduce parallel UI fragment types only if needed.

Validation:

- Selection rect tests.
- Hit testing tests.
- Wrapped inline border/background tests.
- Text decoration propagation tests.

Exit criteria:

- RichText backend still behaves exactly as before.
- Fragment construction no longer requires direct recursive access to
  `std::vector<RichText::InlineItem>`.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 3: Build Experimental UI Provider

Purpose: walk the UI node tree directly.

Tasks:

- Add `UIInlineProvider`.
- Implement traversal for:
  - direct text nodes,
  - nested inline spans,
  - inline-block widgets,
  - floats,
  - `<br>`,
  - invisible and out-of-flow nodes.
- Port whitespace collapsing into provider state.
- Add a switch to run one `UIRichText` instance through the experimental backend.

Validation:

- Compare layout results between RichText backend and UI backend for selected
  tests.
- Initially compare:
  - total character count,
  - selection string,
  - line count,
  - text node bounds,
  - widget hit boxes,
  - inline fragment bounds.

Exit criteria:

- Experimental backend can pass a small focused subset without becoming default.
- Existing default backend remains green.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 4: UI Layout Result And Mapping

Purpose: remove the map-back step for the UI backend.

Tasks:

- Add `UIInlineLayoutResult`.
- Store source refs as typed `Node*` / `UIWidget*` / `UITextNode*`.
- Map text node bounds and widget hit boxes directly from source fragments.
- Keep `BlockLayouter` support for both current RichText fragments and new UI
  fragments during transition.

Validation:

- `UITextNode_BlockLayouter.*`
- `UITextNode_RichTextRebuild.*`
- `UIRichText.selection`
- nested span over-find tests.

Exit criteria:

- UI backend does not need `RichText::InlineSource` or pointer grouping maps for
  normal fragment mapping.
- Existing default backend remains green.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 5: UI Drawing Backend

Purpose: draw inline UI fragments from source UI nodes and drawables.

Tasks:

- Draw inline backgrounds/borders from source widget drawables.
- Preserve font background color plus widget radius composition.
- Preserve split inline first/last edge flags.
- Keep text drawing consistent with current `RichText` behavior.
- Decide whether to add drawable side-mask API for perfect split border
  continuation behavior.

Validation:

- `UIRichText.InlineParentFontBackgroundColorUsesBorderRadiusDrawable`
- `UIRichText.InlineParentBorderIsPreservedInFragments`
- `UIBackground.*`
- `UIBorder.*`
- image comparison tests if available.

Exit criteria:

- UI backend draws the same or better than current backend for covered cases.
- Existing default backend remains green.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 6: Performance And Memory Cleanup

Purpose: remove the duplicated RichText inline tree from the UI backend.

Tasks:

- Stop calling `UIRichText::rebuildRichText()` for containers using UI backend.
- Keep generic `RichText` only as the public API for non-UI use.
- Remove transitional bridge fields from UI fragments when no longer needed.
- Keep current RichText bridge fields if generic RichText still needs them.
- Re-measure allocations and timings from Phase 0.

Exit criteria:

- UI backend is measurably better on memory allocations.
- No significant timing regression.
- Existing full suite passes.
- Required validation passes.
- Stash checkpoint is created and re-applied.

### Phase 7: Make UI Backend Default

Purpose: switch the production UI/HTML path.

Tasks:

- Flip default backend for `UIRichText`.
- Keep fallback switch temporarily for debugging.
- Update plans and comments that describe RichText reconstruction as the main UI
  path.
- Remove fallback only after enough soak time.

Validation:

- Full suite.
- Targeted HTML/UI visual smoke tests.
- Any available screenshot/image-diff tests.

Exit criteria:

- UI backend is default.
- Generic `Graphics::RichText` remains green independently.
- Required validation passes.
- Stash checkpoint is created and re-applied.

## Test Matrix

Always run after broad changes:

```bash
make -C make/linux -j$(nproc)
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="RichText.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIRichText.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UITextNode_RichTextRebuild.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UITextNode_BlockLayouter.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTMLFloat.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTMLTable.complexLayout*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIBackground.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug
git diff --check
```

If UI tests fail with:

```text
Invalid MIT-MAGIC-COOKIE-1 key
```

rerun the failed filter sequentially before investigating unrelated crashes.

## Checkpoint Policy

Each implementation phase must leave a recoverable stash snapshot. The checkpoint
is not a replacement for keeping the working tree active; it is a safety net.

Required sequence at the end of every passing phase:

1. Run the phase-specific focused tests.
2. Run the full suite unless the phase had no code changes.
3. Run `git diff --check`.
4. Create a checkpoint:

```bash
git stash push -u -m "ui-inline-formatting-context phase N: <short description>"
```

5. Re-apply it immediately:

```bash
git stash apply stash@{0}
```

6. Confirm the working tree still contains the phase changes.
7. Record the stash message in `.agent/plans/first_class_inline_boxes_agent_loop.md`
   or a dedicated continuation log for this plan.

Important details:

- Use `git stash push -u` so new plan files, new tests, fixtures, and other
  untracked artifacts are included in the checkpoint.
- Do not use `git stash pop`; popping destroys the checkpoint.
- Do not checkpoint a failing phase as if it were passing.
- If a phase needs partial experimental work that does not pass yet, stash it
  separately with a clear `wip-failing` message before reverting or switching
  direction. Do not call that stash a phase checkpoint.
- If the worktree already contains unrelated user changes, do not revert them.
  Include them in the checkpoint only if they are part of the active phase or
  unavoidable in the shared working tree; otherwise document the dirty files
  before checkpointing.

## Acceptance Criteria

The plan is complete when:

- `Graphics::RichText` remains generic and independent of UI.
- UI/HTML inline layout can run without constructing a full
  `RichText::InlineItem` mirror tree.
- Existing UI nodes remain the source of truth for widget style/drawable data.
- The UI backend preserves:
  - drawing,
  - background color,
  - border radius,
  - border drawing,
  - background images,
  - text decoration,
  - selection string,
  - selection rectangles,
  - hit testing,
  - text node bounds,
  - inline widget hit boxes,
  - float layout,
  - inline-block layout,
  - baseline and vertical-align behavior.
- Allocation counts and/or memory usage improve on representative HTML.
- Full unit suite passes.

## Risks

- Provider abstraction may become too generic and obscure the layout algorithm.
  Keep item views concrete and small.
- UI provider traversal may accidentally diverge from current whitespace
  behavior. Add tests before switching defaults.
- Text measurement without `Text` objects can drift from actual draw behavior.
  Keep Text-backed rendering until measurement parity is proven.
- Fragment output can become fragmented across too many types. Avoid splitting
  UI and Graphics outputs until a concrete dependency forces it.
- Drawable side masking may require deeper changes in border/background drawing.
  Treat that as a follow-up fidelity feature unless tests require it.

## Recommended First Task

Start with Phase 0 and Phase 1 only.

Do not begin by writing the UI provider. First extract a provider seam from the
current `RichText::InlineItem` path and prove that the generic backend still
passes all tests. That gives us a stable extension point for the UI provider
without risking a broad rewrite.
