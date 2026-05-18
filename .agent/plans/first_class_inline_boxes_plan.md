# First-Class Inline Boxes Plan

## Goal

Replace the current RichText flattening approximation with a real inline formatting tree where inline boxes are modeled as first-class layout objects.

This is needed to correctly support nested inline CSS behavior such as:

- `vertical-align` on an inline parent containing child text or anchors.
- Inline margins, padding, borders, and backgrounds across nested content.
- Accurate line box ascent/descent aggregation.
- Correct inline hit boxes and child widget positioning after line wrapping.

The implementation must preserve the current good behavior from the baseline alignment work and avoid element-specific fixes.

## Specification References

Primary references:

- CSS 2.2 inline formatting model: https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
- CSS 2.2 line height and vertical-align: https://www.w3.org/TR/CSS22/visudet.html#line-height
- CSS 2.2 inline non-replaced box dimensions: https://www.w3.org/TR/CSS22/visudet.html#inline-non-replaced
- CSS Display Level 3: https://www.w3.org/TR/css-display-3/
- CSS Inline Layout Level 3: https://www.w3.org/TR/css-inline-3/

Important spec constraints:

- `vertical-align` applies to inline-level and table-cell boxes, but is not inherited.
- Inline boxes can contain text runs, other inline boxes, and atomic inline-level boxes.
- Inline-blocks are atomic in the parent inline formatting context and expose a baseline according to CSS rules.
- Line boxes are built from the inline boxes that participate in each line, including parent inline box metrics, not only leaf text runs.

## Current State

The current pipeline is:

1. `UIRichText::rebuildRichText()` recursively walks children.
2. Text nodes and inline spans are flattened into `RichText::SpanBlock`.
3. Inline-blocks and replaced content become `RichText::CustomBlock`.
4. `RichText::updateLayout()` lays out a flat list of spans/custom blocks.
5. `BlockLayouter::positionRichTextChildren()` maps render spans back to widgets.

This works for many cases, but loses the identity of parent inline boxes. The recent `getEffectiveInlineBaselineAlign()` workaround preserves the nearest explicit inline alignment when flattening nested inline text, but it is still an approximation.

Known limitations caused by flattening:

- Parent inline `vertical-align` can be lost when visible text is inside a child inline element.
- Inline parent padding/borders/backgrounds cannot be split correctly across line wraps.
- Nested inline hit boxes and visual boxes are reconstructed after the fact instead of being laid out directly.
- Future support for inline border painting, decoration propagation, and richer line-height behavior will be fragile.

## Target Model

Introduce an inline formatting model with explicit inline layout items.

Conceptual node types:

```cpp
struct InlineTextRun {
    String text;
    FontStyleConfig style;
    UITextNode* sourceNode;
};

struct InlineBox {
    UIWidget* sourceWidget;
    CSSBaselineAlignValue baselineAlign;
    Rectf margin;
    Rectf padding;
    BorderMetrics border;
    std::vector<InlineItem> children;
};

struct AtomicInlineBox {
    UIWidget* sourceWidget;
    Sizef marginBoxSize;
    Float baseline;
    CSSBaselineAlignValue baselineAlign;
    CSSFloat floatType;
    CSSClear clearType;
};
```

Exact names and storage should follow eepp style, but the key requirement is that inline parent boxes survive until line layout.

## Architecture

### 1. Build An Inline Tree

Replace or augment `UIRichText::rebuildRichText()` with a builder that emits an inline tree:

- Text nodes become text runs.
- True inline `UITextSpan` / `UIHTMLWidget` nodes become `InlineBox` nodes.
- Inline-blocks, images, controls, list items, and other atomic inline-level content become `AtomicInlineBox`.
- Out-of-flow descendants remain skipped and positioned later.
- Floats remain represented as atomic boxes with float metadata.

Whitespace collapsing should continue to happen in the builder, but it must operate across nested inline boundaries.

### 2. Shape And Split Text Runs

Line wrapping must be able to split text runs while preserving ancestor inline boxes.

Required behavior:

- A text run can produce multiple line fragments.
- Each line fragment carries the active ancestor inline box chain.
- Ancestor inline boxes generate per-line fragments when their content wraps.
- Leading/trailing inline margins/padding/borders apply according to whether the fragment is the first or last fragment of that inline box.

Initial implementation can support margins/padding first and leave border/background painting as a follow-up, as long as the fragment model is ready for it.

### 3. Compute Inline Metrics

Each inline item should expose:

- advance width,
- content box height,
- used line height,
- natural baseline,
- ascent/descent extents relative to its own baseline,
- `vertical-align` shift.

Text runs use font metrics:

- ascent,
- descent,
- line spacing,
- x-height fallback for `middle`.

Atomic inline boxes use:

- provided widget size plus margin,
- internal baseline for inline-blocks when available,
- bottom edge fallback according to CSS.

Inline parent boxes use the union of child fragments and their own padding/border/margin contribution.

### 4. Build Line Boxes From Fragments

The line builder should produce `InlineLine` objects containing fragment boxes, not just leaf render spans.

Each line should know:

- line y,
- line height,
- baseline,
- max ascent/descent,
- fragment list,
- width.

`vertical-align: top` and `bottom` need line-box-level resolution. The algorithm may require:

1. Build provisional line with baseline-aligned items.
2. Resolve line height.
3. Apply top/bottom aligned items.
4. Recompute final extents if needed.

This should replace duplicated logic in both RichText paths. The float-aware path can still provide available width and float placement, but inline vertical alignment should be shared.

### 5. Preserve Widget Mapping

After layout, map generated inline fragments back to source nodes:

- Text nodes receive one or more hit boxes.
- Inline widgets receive one or more fragment boxes.
- Inline-block/replaced widgets receive one atomic fragment.
- Parent inline boxes spanning multiple lines receive multiple fragment boxes.

`BlockLayouter::positionRichTextChildren()` should consume this structured result instead of inferring child positions from flattened spans.

### 6. Drawing And Hit Testing

Drawing should eventually use the same fragment list:

- Text runs draw at fragment positions.
- Inline parent backgrounds/borders draw per fragment.
- Decorations can be applied consistently over fragmented inline boxes.
- Hit testing uses fragment boxes directly.

Initial migration can keep existing drawing for text while using fragment boxes for widget placement, but the final target should remove duplicated reconstruction logic.

## Migration Strategy

### Phase 1: Non-Behavioral Data Model

- Add internal inline item/fragment structs behind `RichText`.
- Keep existing flat `SpanBlock` / `CustomBlock` APIs working.
- Add debug-only or test-only accessors for generated fragments.
- Do not change rendering behavior yet.

### Phase 2: Build Inline Tree From HTML

- Add a new builder path in `UIRichText::rebuildRichText()`.
- Emit nested inline boxes for true inline widgets.
- Keep a compatibility path if needed while tests are ported.
- Preserve whitespace collapsing behavior exactly.

### Phase 3: Shared Line Layout

- Implement line construction from inline items.
- Reuse existing wrapping decisions where possible.
- Keep float handling outside the inline algorithm, but feed available line widths into it.
- Ensure fast path and float-aware path share vertical alignment code.

### Phase 4: Widget Position Mapping

- Update `BlockLayouter::positionRichTextChildren()` to use inline fragments.
- Support multi-fragment inline widgets.
- Preserve existing positions for atomic inline-blocks, images, list markers, details/summary, and form controls.

### Phase 5: Remove Flattening Workarounds

- Remove `getEffectiveInlineBaselineAlign()` once parent inline boxes directly participate in line layout.
- Remove any code that copies parent inline alignment into descendant flattened spans.
- Keep `vertical-align` non-inherited.

### Phase 6: Painting Improvements

- Use inline fragments for inline backgrounds, borders, and decoration painting.
- Add support for split inline backgrounds/borders across wrapped lines.
- Review selection rendering and hit boxes against the new fragments.

## Tests

Add narrow unit tests for the inline layout engine:

- Nested inline parent with `vertical-align: bottom` and child anchor text aligns the parent box bottom to the line bottom.
- Child inline element does not inherit `vertical-align` in computed style.
- Nested inline `vertical-align: middle` does not affect sibling baselines incorrectly.
- Inline parent with text before and after child creates one coherent inline box.
- Inline parent split across two wrapped lines produces two fragments.
- Inline-block baseline still follows CSS bottom-edge fallback when it has no in-flow line boxes.
- Inline-block with in-flow internal line boxes exposes its last line baseline.

Add UIHTML fixture tests:

- `reddit_header.html`: `.hover.pagename.redditname` bottom-aligns in `#header-bottom-left`.
- `inline_block.html`: footer/share-button heights remain close to browser behavior.
- Existing `reddit_header_icons.html` remains unchanged.
- `UIHTMLDetails.*`, `UIRichText.MinMaxWidthChildren`, and `UILayout.listStyle*` remain green.

Add RichText tests:

- Multi-line nested inline box fragments preserve source widget hit boxes.
- `vertical-align: top` and `bottom` resolve after final line height is known.
- Length and percentage vertical-align values work for nested inline boxes.

## Risks

- Whitespace collapsing can regress at inline boundaries.
- Text selection and hit testing may shift if fragment ownership changes.
- Float-aware layout can diverge from the fast path if shared inline code is not factored carefully.
- Multi-line inline parent fragments can expose existing assumptions that each widget has one rectangle.
- Golden image diffs are expected; prioritize numeric layout invariants and real browser comparisons before updating goldens.

## Completion Criteria

- Nested inline CSS alignment works without copying inherited values to descendants.
- `vertical-align` remains non-inherited in computed style.
- Parent inline boxes have fragment boxes and can affect line metrics directly.
- Existing baseline alignment workaround is removed.
- Current HTML regression tests pass, except intentionally updated goldens.
- New tests cover nested inline boxes, fragmented inline boxes, and realistic reddit header behavior.
