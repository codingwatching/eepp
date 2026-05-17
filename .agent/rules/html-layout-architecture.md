# HTML Layout Architecture

This document describes the eepp GUI HTML compatibility layer: the subset of HTML/CSS layout implemented through `UIHTMLWidget`, `UIRichText`, `RichText`, and the `UILayouter` family.

The goal is browser-compatible behavior where implemented, not a parallel eepp-specific layout language. When adding HTML/CSS features, start from the relevant specification and then map that behavior onto the existing architecture.

## Spec-First Requirement

Agents must implement HTML/CSS features by following the official specifications first. Do not add element-specific hacks just to match one fixture if the behavior belongs to a generic CSS concept.

Primary references:

- HTML Living Standard: https://html.spec.whatwg.org/
- CSS specifications index: https://www.w3.org/Style/CSS/specs.en.html
- CSS Display: https://www.w3.org/TR/css-display-3/
- CSS Visual Formatting Model, including inline formatting and positioning: https://www.w3.org/TR/CSS22/visuren.html
- CSS line height and inline-block baseline rules: https://www.w3.org/TR/CSS22/visudet.html#line-height
- CSS Lists and Counters: https://www.w3.org/TR/css-lists-3/

Required workflow for new HTML/CSS behavior:

- Identify the spec section that defines the behavior.
- Implement the behavior at the correct abstraction level, usually display/layout/formatting/list/positioning, not by tag name.
- Preserve HTML element defaults from the spec, such as `summary { display: list-item; }`.
- Add focused tests that encode the generic behavior and, when useful, a real HTML fixture that triggered the issue.
- If eepp intentionally supports only a subset of a spec, document the limitation close to the implementation or in this file.

## Core Concepts

### UIHTMLWidget

`UIHTMLWidget` is the base class for HTML-like elements. It stores parsed CSS layout state such as `display`, `position`, `float`, `clear`, list style, and data attributes. It does not own all layout math directly. Instead, it uses `UILayouterManager` to instantiate the appropriate `UILayouter` for its `CSSDisplay`.

Important responsibilities:

- CSS property application and invalidation.
- Out-of-flow positioning support through containing blocks.
- Exposing a `RichText` object for elements that render mixed inline content.

### Layouters

Layout math is separated from widgets into `UILayouter` implementations:

- `BlockLayouter`: Handles block-like containers, including `display: block`, `inline-block`, `list-item`, `table-cell`, and the current `flex` placeholder path. It delegates inline formatting to `RichText`, then maps the resulting physical spans back to child widgets.
- `TableLayouter`: Handles `display: table` and encapsulates HTML table column width distribution, rows, sections, and cell positioning.
- `InlineLayouter`: A no-op layouter for true inline text-span elements. Inline formatting is owned by the nearest rich-text/block formatting context so normal widget layout does not override text flow.
- `NoneLayouter`: Handles `display: none` by skipping layout/rendering participation.

`UILayouterManager::create()` is the dispatch point. Before adding a new display mode, check whether it should create a new formatting context, participate in an existing one, or be represented as a rich-text custom box.

## RichText Integration

`UIRichText` is the primary container for mixed text and widget content.

`UIRichText::rebuildRichText()` recursively walks normal-flow children and builds a `Graphics::RichText` stream:

- Text nodes and inline text spans become `RichText::SpanBlock` entries via `addSpan()`.
- `<br>` contributes a line break.
- Inline-blocks, list items, replaced controls, images, and block-like widgets become `RichText::CustomBlock` entries via `addCustomSize()`.
- Out-of-flow children are skipped here and positioned later.

`RichText::updateLayout()` performs line wrapping and inline formatting. `BlockLayouter::positionRichTextChildren()` then consumes `RichText::RenderSpan`s and assigns pixel positions/sizes back to DOM widgets.

### UITextNode

`UITextNode` is a lightweight non-rendering node for raw parsed text (`node_pcdata`). Its text is extracted during `rebuildRichText()` and rendered by the parent `UIRichText`. After wrapping, `BlockLayouter` assigns it position and size for debugging and hit-box accounting, but `UITextNode::draw()` remains a no-op.

### Custom Blocks And Baselines

`RichText::CustomBlock` represents atomic inline-level or block-like widgets inside the rich-text stream. It carries:

- physical size,
- float/clear state,
- a baseline offset,
- virtual line-break state.

The default custom-block baseline is the bottom edge to preserve old behavior for generic drawables and spacers. HTML widgets that expose internal `RichText` should provide a CSS-compatible baseline derived from their last in-flow internal line box:

```cpp
margin.Top + contentOffset.Top + lastLine.y + lastLine.maxAscent
```

This is required for `display: inline-block` and for nested rich-text widgets such as `<details><summary>...</summary></details>`. A taller inline-block caused by inherited `line-height` should keep its real height but align by baseline in the parent inline formatting context.

Do not fix baseline problems by special-casing individual elements, zeroing `line-height`, or changing element display defaults. The correct layer is generic inline formatting and custom-block baseline propagation.

## Display And Flow Rules

### Inline Content

True inline content is formatted by the nearest `UIRichText`/`BlockLayouter` context. Inline widgets should not be independently positioned by ordinary widget layout.

### Inline-Block

`display: inline-block` creates an atomic inline-level box in the parent inline formatting context and an internal formatting context for its children. It should:

- participate in the parent line as a custom block,
- preserve its own internal line-height and content sizing,
- expose its internal baseline when it has in-flow line boxes,
- fallback to its bottom edge only when it has no in-flow line boxes.

### List Items And Markers

`display: list-item` is block-like for layout but has marker behavior. Shared marker rendering belongs in `UIHTMLListStyle` and related list-item/summary code, not duplicated per element.

Requirements:

- `list-style-type` must be parsed and rendered according to the supported CSS list values.
- `list-style-type: none` disables marker rendering and marker spacing.
- `<summary>` defaults to `display: list-item` and uses disclosure marker defaults as defined by HTML rendering rules.
- `disclosure-open` and `disclosure-closed` should use eepp's primitive marker drawing facilities, not textual `v` or `>` approximations.

### Out-Of-Flow Positioning

Elements with `position: absolute` or `position: fixed`:

- are ignored by standard layouters and `UIRichText::rebuildRichText()`,
- do not contribute to normal-flow auto size,
- are positioned at the end of the parent's `updateLayout()` using `positionOutOfFlowChildren()`,
- use `getContainingBlock()` for absolute positioning and the `UISceneNode` root for fixed positioning.

Relative positioning should preserve normal-flow space and then offset painting/positioning according to CSS semantics.

Floats are represented in `RichText::CustomBlock` with `CSSFloat`/`CSSClear` metadata and handled by the float-aware RichText path. Float placement is edge-aligned and should not be altered by inline baseline alignment.

## Pixel Math

All layouters must use pixel (`Px`) APIs for layout calculations:

- Use `getPixelsSize()`, `getPixelsPadding()`, `getPixelsContentOffset()`, and `getLayoutPixelsMargin()`.
- Do not mix these with `getSize()` or `getPadding()` in layout math. Those return density-independent pixels (`dp`) and will break HiDPI calculations.

Convert only at clear API boundaries where the surrounding code expects dp.

## Testing Expectations

For HTML/CSS layout work, prefer narrow tests plus one realistic fixture when the bug came from real content:

- Unit-level tests for parser/style/layout primitives.
- RichText tests for inline formatting, wrapping, baselines, custom blocks, floats, and line-height.
- UIHTML fixture tests for browser-like element interactions such as details/summary, tables, forms, lists, images, and positioned descendants.
- Regression assertions should verify layout invariants, not screenshot pixels only.

When possible, compare against browser behavior manually or with a reference capture, then encode the spec behavior in assertions.
