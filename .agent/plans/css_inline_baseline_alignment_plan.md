# CSS Inline Baseline Alignment Plan

## Goal

Add spec-aligned support for CSS inline baseline alignment now that `RichText::CustomBlock` can carry real baselines.

Initial scope:

- `vertical-align` for inline-level boxes and table cells where already practical.
- `alignment-baseline` as an inline SVG/CSS alignment property that can share the same inline baseline machinery.
- Shared internal representation for baseline alignment so text spans, inline-blocks, images/custom blocks, and future SVG text can use the same layout path.

This must be implemented as generic inline formatting behavior, not as element-specific fixes.

## Specification References

Primary references:

- CSS 2.2 `vertical-align`: https://www.w3.org/TR/CSS22/visudet.html#propdef-vertical-align
- CSS 2.2 inline formatting model: https://www.w3.org/TR/CSS22/visuren.html#inline-formatting
- CSS Inline Layout Level 3 alignment terms: https://www.w3.org/TR/css-inline-3/
- SVG 2 `alignment-baseline`: https://www.w3.org/TR/SVG2/text.html#AlignmentBaselineProperty

Important spec notes:

- `vertical-align` applies to inline-level boxes and table cells, and is not inherited.
- Percentage values for `vertical-align` refer to the element's own `line-height`.
- Keyword values adjust a box relative to the parent line box baseline, text metrics, or line box.
- `alignment-baseline` is inherited in SVG contexts and should eventually apply to SVG text/layout. For eepp's HTML compatibility layer, it can initially map to the same baseline-alignment value type used by inline formatting.

## Current State

Relevant implementation points:

- `RichText::SpanBlock` stores text, margin, padding, and optional line-height.
- `RichText::CustomBlock` stores size, float/clear, and baseline.
- `RichText::updateLayout()` computes `line.maxAscent`, then assigns `span.position.y` from the baseline.
- Text spans currently use `line.maxAscent - textBlock->getCharacterSize()`, which is only an approximation of baseline alignment.
- Custom blocks use `line.maxAscent - custom.baseline`, which is the right extension point for inline-blocks and replaced content.
- `UIRichText::rebuildRichText()` already computes custom-block baselines from internal RichText lines.
- There is no implemented `vertical-align` property. Existing `row-valign` / `row-vertical-align` is unrelated table row UI behavior and should not be reused as CSS `vertical-align`.

## Design

### 1. Add A Shared Alignment Type

Add a CSS-facing value type, likely in `csslayouttypes.hpp`, such as:

```cpp
enum class CSSBaselineAlignment {
    Baseline,
    Sub,
    Super,
    TextTop,
    TextBottom,
    Middle,
    Top,
    Bottom,
    Length,
    Percentage,
    Auto
};
```

Also store the numeric offset for `<length>` and `<percentage>` values. A small struct is preferable to overloading the enum:

```cpp
struct CSSBaselineAlignValue {
    CSSBaselineAlignment type{ CSSBaselineAlignment::Baseline };
    std::string specified;
    Float value{ 0.f };
    bool percentage{ false };
};
```

The actual names can be adjusted to match existing eepp style.

### 2. Register CSS Properties

In `StyleSheetSpecification::registerDefaultProperties()`:

- Register `vertical-align` with initial value `baseline`, not inherited.
- Register `alignment-baseline` with initial value `baseline` or `auto` according to the chosen supported subset. Mark it inherited only if implementation targets the SVG/SVG-text behavior from the spec.

Do not alias `vertical-align` to existing `row-valign`; that would conflate separate CSS concepts.

### 3. Store The Computed Values On HTML Widgets

Add baseline alignment storage at the lowest common inline-participation layer:

- `UIHTMLWidget` should store the parsed value for boxes that become `CustomBlock`s.
- `UITextSpan` / `UIRichText` should expose it for text spans and nested inline content.
- Replaced elements such as `UIHTMLImage` inherit this through `UIHTMLWidget` once they are represented as custom blocks.

Implementation options:

- Add fields and accessors to `UIHTMLWidget`, and let `UIRichText`/`UITextSpan` use the same inherited logic.
- Or add the property to `UIWidget` only if non-HTML UI widgets need baseline alignment in RichText. Prefer `UIHTMLWidget` for now.

Property application:

- `vertical-align` accepts keywords, lengths, and percentages.
- `alignment-baseline` accepts a keyword subset initially: `baseline`, `middle`, `text-before-edge`/`text-top`, `text-after-edge`/`text-bottom`, `central`, `before-edge`, `after-edge`, `hanging`, `mathematical`, and `auto` as feasible.
- Unsupported keywords should parse to a known fallback only if the spec allows it; otherwise leave the property unapplied.

### 4. Extend RichText Blocks With Alignment Metadata

Extend `RichText::SpanBlock` and `RichText::CustomBlock` with baseline alignment metadata:

```cpp
CSSBaselineAlignValue baselineAlign;
```

Update:

- `RichText::addSpan(...)`
- `RichText::addCustomSize(...)`
- all call sites in `UIRichText::rebuildRichText()`
- line-break custom block construction

Default value must preserve current behavior: baseline alignment.

### 5. Compute Text Baselines From Font Metrics

Replace the current text offset approximation:

```cpp
line.maxAscent - textBlock->getCharacterSize()
```

with real font metrics:

- ascent from `Font::getAscent(characterSize)`,
- descent/line spacing if available,
- used line height from `SpanBlock::lineHeight` or normal font line spacing.

The text baseline for a text span should be its ascent from the top of its own inline box. This aligns with the existing custom-block model, where baseline is an offset from the top of the box.

### 6. Apply Baseline Alignment During Line Layout

Add a helper in `RichText`, conceptually:

```cpp
Float resolveBaselineOffset(
    const RenderParagraph& line,
    const RenderSpan& span,
    Float naturalBaseline,
    const CSSBaselineAlignValue& align,
    const FontStyleConfig* parentFontStyle );
```

Supported `vertical-align` behavior:

- `baseline`: no extra shift.
- `<length>`: raise the box by positive length and lower by negative length.
- `<percentage>`: same as length, resolved against the element's own line-height.
- `sub` / `super`: use font-derived or conservative spec-compatible offsets. Prefer a font metric if available; otherwise use a documented fraction of font size.
- `text-top`: align the top of the box with the parent content area's top.
- `text-bottom`: align the bottom of the box with the parent content area's bottom.
- `middle`: align the midpoint of the box with the parent baseline plus half x-height. If x-height is unavailable, use a documented fallback based on font size.
- `top`: align the top of the box with the top of the line box.
- `bottom`: align the bottom of the box with the bottom of the line box.

Implementation may need two passes:

1. Compute natural baselines, ascent/descent extents, and provisional line box metrics.
2. Apply vertical-align shifts and expand line height if shifted boxes extend outside current line bounds.

This must be done in both RichText paths:

- fast path without floats,
- float-aware path.

Float placement must remain edge-aligned and should ignore inline baseline alignment, as documented in `html-layout-architecture.md`.

### 7. Map `alignment-baseline`

For the first implementation:

- If `vertical-align` is specified, it controls CSS inline alignment for HTML inline-level boxes.
- If `alignment-baseline` is specified and `vertical-align` is not specified, map supported keywords to the shared baseline alignment value for inline/SVG contexts.
- Keep unsupported SVG-specific values documented as TODOs.

Do not pretend full SVG text layout is complete if only inline block alignment is wired.

## Implementation Steps

1. Add property IDs/specification entries for `vertical-align` and `alignment-baseline`.
2. Add parser/helper functions for baseline alignment values.
3. Store computed alignment value on `UIHTMLWidget` / `UIRichText` / `UITextSpan`.
4. Extend `RichText::SpanBlock` and `RichText::CustomBlock` to carry alignment metadata.
5. Update `UIRichText::rebuildRichText()` to pass each node's computed alignment into `addSpan()` / `addCustomSize()`.
6. Replace text baseline approximation with real font ascent-based baseline.
7. Implement alignment resolution in `RichText::updateLayout()` fast path.
8. Mirror the same alignment resolution in the float-aware path.
9. Add tests.
10. Update `.agent/rules/html-layout-architecture.md` if implementation details differ from this plan.

## Tests

Add narrow unit tests in `richtext_tests.cpp`:

- Text span with `vertical-align: baseline` preserves current baseline placement.
- Inline custom block with `vertical-align: middle` is vertically centered relative to surrounding text.
- Inline custom block with `vertical-align: text-top` and `text-bottom`.
- Inline custom block with positive and negative length values.
- Inline custom block with percentage values based on its own line-height.
- Mixed text sizes where font ascent, not character size, controls baseline.

Add HTML/UI tests in `uihtml_tests.cpp`:

- `img { vertical-align: middle; }` in a text line.
- `details.caches { display: inline-block; vertical-align: baseline; }` remains unchanged from the Lobsters regression.
- `details.caches { vertical-align: middle; }` moves only by baseline alignment, not by changing element height.
- `alignment-baseline: middle` maps to the same behavior where supported.

Regression guard:

- Existing `UIHTMLDetails.*`, `UILayout.listStyle*`, and `UIRichText.MinMaxWidthChildren` must remain green.

## Risks

- `vertical-align: top` and `bottom` require line-box-level alignment and may need a clean second pass to avoid circular line-height changes.
- Text metrics may differ by font backend. Tests should use tolerances and the existing unit-test font.
- SVG-specific `alignment-baseline` keywords are more nuanced than HTML inline `vertical-align`; document any unsupported values clearly.
- Existing tests may encode the old character-size baseline approximation. Prefer updating them to assert spec behavior rather than preserving the approximation.

## Non-Goals For First Pass

- Full SVG text layout.
- CSS Box Alignment properties unrelated to inline baselines.
- Table-cell `vertical-align` beyond mapping to existing table-cell placement if the table layouter does not already expose the needed hooks.
- Browser-perfect x-height if the active font API cannot expose it yet.
