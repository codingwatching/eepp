# CSS Block Semantics — Full Compliance Plan

This plan addresses the remaining pragmatic deviations between the RichText-based layout engine and the CSS 2.1 block formatting context model. The goal is a fully compliant web engine with no special-case "richtext mode" vs. "HTML mode" distinction.

**AGENT DIRECTIVE (CRITICAL):** Compile and run the unit tests after EVERY step. Take a git stash snapshot (`git stash push -m "Step X.Y passed"`) upon passing a step. Do NOT proceed if any test regresses.

---

## Current Deviation Summary

| # | Deviation | Impact |
|---|-----------|--------|
| D1 | `isBlock && flowX > 0` allows blocks to sit beside floats when no inline content preceded them. CSS spec: block always starts on a new line (§9.4.1). | Blocks float-side-by-side in float-aware path instead of stacking vertically. |
| D2 | RichText `CustomBlock` with `isBlock` piggybacks line-break behavior onto a text-flow engine. A block formatting context should stack child boxes vertically, not inline them with `isBlock` flags. | Semantic confusion: the RichText engine conflates text flow and block layout. |
| D3 | Inline-block spans are decomposed into RichText text segments that wrap line-by-line, rather than being treated as a single opaque box. | The "solid box" semantics of inline-block are partially emulated with `atomicMaxX` / forced line breaks, but edge cases remain. |
| D4 | `<richtext>` widgets and `<div>` containers are both `UIRichText` with `CSSDisplay::Block`, yet `<richtext>` has historically flowed children inline. The tag-based distinction was removed; now both use CSS display semantics, which is correct. | No remaining deviation — this is resolved in the current PR. |

## Phase 1: Isolate RichText from Block Layout

The RichText engine (`Graphics::RichText`) should handle **text formatting only** — word wrapping, line breaking, glyph placement. It should NOT decide whether an element breaks to a new line. That decision belongs to the Block Formatting Context established by the `BlockLayouter`.

### Step 1.1: Remove `isBlock` from `RichText::CustomBlock` and `RichText::addCustomSize`

- `CustomBlock` drops the `isBlock` field.
- `addCustomSize` drops the `isBlock` parameter.
- `RichText::updateLayout()` removes all `isBlock`-gated line-break logic (both non-float and float-aware paths). Every `CustomBlock` flows as an inline-level atomic box.
- All `fillParent` / width-override logic stays in `UIRichText::rebuildRichText`.
- **Validation:** Compile. Many tests will fail — this is expected. Continue.

### Step 1.2: Move block-level line breaking into `BlockLayouter`

- `BlockLayouter::updateLayout()` now builds the RichText content in **per-child layers** rather than delegating everything to a single `rebuildRichText` call.
- For each child widget:
  - If the child is mergeable (text span, inline), it is added to the current RichText via `UIRichText::rebuildRichText`.
  - If the child is a block-level element (display != Inline and != InlineBlock), a **new line** is forced before the child. This is done by inserting a `\n` into the RichText or by finalizing the current line and starting a fresh `RenderParagraph`.
- The container's own `positionRichTextChildren` is called once after ALL children are placed.
- **Validation:** All tests pass. (Snapshot)

### Step 1.3: Remove the `flowX` workaround

- With block-line-breaking moved into `BlockLayouter`, the `flowX` / `curX` saving logic in the float-aware path is no longer needed. Blocks always start on a new line (per CSS).
- Remove the `flowX` variable and associated logic from `RichText::updateLayout()` float-aware path.
- Remove the "block overflow below floats" block (it's superseded by the block-layouter approach).
- **Validation:** `UIHTML.blockFlow`, `UIHTML.blockFlowFloat`, all `UIHTMLFloat.*` tests pass. (Snapshot)

## Phase 2: Full Block Formatting Context

### Step 2.1: Block children interact with floats

- When `BlockLayouter` places a block child and there are active floats (from the RichText engine at the current Y), the block child's margin box must respect the float constraints:
  - If the block fits in the available width (between left-floats and right-floats), it stays on the same line but its width is narrowed.
  - If the block does NOT fit, it moves to the next "float-free" Y (below all active floats) — effectively an implicit clear.
- Implement this in `BlockLayouter::updateLayout()` by querying `RichText` for the current float state.
- The existing float overflow logic in `RichText` (for float-on-float overflow) stays in place. The block-vs-float overflow is now handled in `BlockLayouter`.
- **Validation:** `UIHTMLFloat.floatWrapsContentBelowWhenTooWide` and all float tests pass. (Snapshot)

### Step 2.2: `positionRichTextChildren` handles block-level Y offsets

- Currently, block and inline children are positioned by the same `positionRichTextChildren` loop, which walks RichText lines and assigns positions.
- With the new approach, block children are placed at the start of a new RichText line (their own line). Their Y position is determined by the line they occupy.
- `BlockLayouter` may need to track which children are block vs. inline so it can query the correct line Y.
- **Validation:** All block-positioning tests pass (`UITextNode_BlockLayouter.*`, `UIHTML.blockFlow`, `UIRichText.MarginsTest`). (Snapshot)

## Phase 3: Inline-Block Box Semantics

### Step 3.1: Treat inline-block as an opaque `CustomBlock`

- In `UIRichText::rebuildRichText`, when a child span has `isInlineBlock() == true`, do NOT flatten its text into the parent RichText.
- Instead, render the inline-block's content into its OWN `RichText` instance (via its own `BlockLayouter`), producing a single `Sizef` representing the box.
- Add this box to the parent RichText via `addCustomSize` (with `floatType = None`, `clearType = None`). No `isBlock` flag needed.
- This makes the inline-block a single opaque rectangle in the parent's inline flow — exactly matching CSS.
- Remove the `atomicMaxX` tracking and multiline forced-break logic from `RichText::updateLayout()` (both paths).
- **Validation:** `UIHTML.InlineBlockBrowserTest`, `UIHTML.InlineBlock*` tests pass. (Snapshot)

### Step 3.2: Inline-block height and baseline alignment

- CSS inline-blocks align to the parent's baseline. The `RichText` line layout must handle the inline-block's height correctly (via `maxAscent` and `lineHeight` in `RenderParagraph`).
- If the inline-block contains text, its own RichText produces a height. This height must be passed to the parent's `addCustomSize` as the box height.
- Baseline of the inline-block = baseline of its last line of text (or bottom if no text).
- **Validation:** Inline-block vertical alignment tests pass. (Snapshot)

## Phase 4: Cleanup and Regression

### Step 4.1: Remove dead code

- Remove `isBlock`-related fields from `SpanBlock` and `CustomBlock` structs.
- Remove `isBlock` parameter from `RichText::addSpan` (line-height variant), `RichText::addCustomSize`.
- Remove `atomicMaxX` tracking from both layout paths.
- Remove `flowX` logic from the float-aware path.
- Remove the "block overflow below floats" special case from the float-aware path.
- **Validation:** Full test suite (280 tests). Must all pass. (Snapshot)

### Step 4.2: Add spec-compliance regression tests

- Write tests for edge cases identified during migration:
  - Block after float with no inline content: block must start on a new line (D1 resolution).
  - Block with explicit width after float: block starts below float if it doesn't fit.
  - Float → Block → Inline sequence: block goes below float, inline flows beside block.
  - Inline-block beside float: inline-block box flows beside float, not decomposed.
  - Nested inline-blocks: outer inline-block contains inner inline-block.
- **Validation:** All new tests pass. (Snapshot)

---

## Migration Order (Dependency Graph)

```
Phase 1 (isolate RichText)
  ├─ 1.1 Remove isBlock from RichText
  ├─ 1.2 Move line breaking to BlockLayouter
  └─ 1.3 Remove flowX workaround
      ↓
Phase 2 (block formatting context)
  ├─ 2.1 Block-float interaction in BlockLayouter
  └─ 2.2 positionRichTextChildren block Y offsets
      ↓
Phase 3 (inline-block box semantics)
  ├─ 3.1 Inline-block as opaque CustomBlock
  └─ 3.2 Baseline alignment
      ↓
Phase 4 (cleanup)
  ├─ 4.1 Remove dead code
  └─ 4.2 Spec-compliance regression tests
```

Each phase is a self-contained checkpoint. No phase should leave the test suite in a broken state.
