# CSS `white-space-collapse` Support Plan

## Goal

Implement real CSS Text Level 4 `white-space-collapse` behavior in the HTML/RichText pipeline.

The immediate product goal is that `<pre><code>` can render as normal HTML/RichText content, preserving
code indentation, blank lines, and line breaks without the current `UICodeEditor` fallback in
`UIRichText::loadFromXmlNode()`.

Reference specs:

- CSS Text Module Level 4: https://drafts.csswg.org/css-text-4/#white-space-collapsing
- Stable TR snapshot: https://www.w3.org/TR/css-text-4/#white-space-collapsing

## Implementation Status

Recovered and implemented in the current workspace on 2026-06-01, without refreshing any golden
images.

Implemented:

- Raw PCDATA is preserved when loading `UIRichText` and `UITextSpan`; CSS whitespace processing now
  happens during RichText rebuild.
- `WhiteSpaceCollapse::Discard` exists and `white-space-collapse` parsing/stringification covers
  `collapse`, `preserve`, `preserve-breaks`, `preserve-spaces`, `break-spaces`, and `discard`.
- `white-space` accepts the legacy values plus direct collapse keywords and `wrap`/`nowrap` tokens.
- `UIRichText::rebuildRichText()` applies the effective collapse mode for text nodes, inline spans,
  and rebuilt `UITextSpan` roots.
- `RichText` has a whitespace wrap mode for preserved and `break-spaces` content; `break-spaces`
  contributes preserved spaces/tabs to intrinsic width and wraps after preserved spaces/tabs.
- CSS `tab-size` is registered as inherited with initial value `8`; `UIRichText` stores the value and
  passes it to `RichText`/`Text` measurement and wrapping.
- Base HTML CSS now includes `pre { white-space: pre; }`.
- Generic HTML `<pre><code>` renders as normal RichText content.
- The old read-only `UICodeEditor` initialization is retained only when a `<pre><code>` block is
  inside `UI_TYPE_MARKDOWNVIEW`, or when `UIRichText::setUseCodeEditorForPreCodeBlocks( true )` is
  explicitly enabled. The global switch defaults to disabled.
- Regression fixtures were restored:
  - `bin/unit_tests/assets/html/pre.code.html`
  - `bin/unit_tests/assets/html/pre.code.2.html`
  - `bin/unit_tests/assets/html/pre_code_block.html`
- A final segment break immediately before an element end tag is discarded before CSS whitespace
  collapse/transformation, matching browser behavior for preformatted HTML such as
  `<pre><code>...\n</code></pre>`.

Validated:

- `make -C make/linux -j$(nproc)`
- `UIHTML.PreCode*`
- `UIHTML.WhiteSpaceCollapse*`
- `UIRichText.WhiteSpaceCollapse*`
- `RichText.BreakSpaces*`
- `UITextNode_EdgeCases.EmptyTextNodesDontAffectLayout`
- `UIHTML.WhiteSpaceNowrap*`
- `UIHTML.AnchorsSizing`
- `UIHTMLTable.complexLayout*` against the original existing golden images
- Full Xvfb/ASAN unit suite:
  `ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x1024x24" bin/unit_tests/eepp-unit_tests-debug`
  ran 478 test cases: 477 passed, 1 skipped (`UIHTML.redditOldThreadWebViewSmoke`).
- `git diff --check`
- No golden images were refreshed for this feature.

Pending implementation work:

- None for the `white-space-collapse` support described by this plan.

Known follow-up, not part of this recovery:

- Browser-parity investigation for apparent high pixel-density `em`/font-size differences in code
  blocks. At pixel density 1, the current `pre_code_block.html` spacing is covered by unit tests.

## Baseline State Before This Plan

- `UIRichText` already stores:
  - `WhiteSpaceCollapse::Collapse`
  - `WhiteSpaceCollapse::Preserve`
  - `WhiteSpaceCollapse::PreserveBreaks`
  - `WhiteSpaceCollapse::PreserveSpaces`
  - `WhiteSpaceCollapse::BreakSpaces`
- CSS parsing already recognized `white-space` and `white-space-collapse`.
- `white-space: nowrap` is partially working because it maps to `mLineWrap = false`.
- The actual text processing was incomplete:
  - `UIRichText::loadFromXmlNode()` collapses all PCDATA through
    `UIRichText::collapseInternalWhitespace()` before style resolution can know the effective
    `white-space-collapse` value.
  - `UIRichText::rebuildRichText()` only distinguishes `Collapse` from all other values.
  - `preserve-breaks`, `preserve-spaces`, and `break-spaces` do not get their spec behavior.
  - `break-spaces` has no special line-breaking or intrinsic-size support.
- `<pre><code>` was special-cased in `UIRichText::loadFromXmlNode()` into a read-only `UICodeEditor`.
  That workaround is now limited to markdown/global opt-in only.

## Spec Behavior To Support

Implement the full `white-space-collapse` value set:

| Value | Required behavior |
|---|---|
| `collapse` | Collapse white-space runs and segment breaks into a single space, with existing block-boundary trimming. |
| `preserve` | Preserve spaces, tabs, and segment breaks. Segment breaks become forced line breaks. |
| `preserve-breaks` | Collapse spaces/tabs, but preserve segment breaks as forced line breaks. |
| `preserve-spaces` | Preserve spaces; convert tabs and segment breaks to spaces. |
| `break-spaces` | Preserve spaces/tabs/segment breaks like `preserve`, but allow a wrap opportunity after every preserved space and make trailing preserved spaces measurable/non-hanging. |
| `discard` | Spec-defined but not broadly implemented by browsers. Add enum/parser support and implement as "remove all collapsible white-space characters" so the engine is complete and deterministic. |

`white-space` shorthand must continue mapping correctly:

| `white-space` | collapse mode | wrap mode |
|---|---|---|
| `normal` | `collapse` | wrap |
| `nowrap` | `collapse` | nowrap |
| `pre` | `preserve` | nowrap |
| `pre-wrap` | `preserve` | wrap |
| `pre-line` | `preserve-breaks` | wrap |
| `break-spaces` | `break-spaces` | wrap |

Also accept two-keyword Level 4 forms where practical, e.g. `white-space: preserve nowrap`,
`white-space: preserve wrap`, `white-space: collapse nowrap`, and direct
`white-space-collapse: preserve-spaces`.

## Implementation Plan

### Phase 1: Preserve Source Text Until Layout

**Files:**

- `src/eepp/ui/uirichtext.cpp`
- `include/eepp/ui/uirichtext.hpp`
- `src/tests/unit_tests/richtext_tests.cpp`
- `src/tests/unit_tests/uihtml_tests.cpp`

Steps:

1. Stop calling `collapseInternalWhitespace()` when creating `UITextNode` from PCDATA in
   `UIRichText::loadFromXmlNode()`.
2. Store raw parsed text in `UITextNode`; CSS whitespace processing must happen only during rich-text
   rebuild/layout.
3. Keep `collapseInternalWhitespace()` temporarily for existing callers/tests, but move toward a new
   policy-driven helper.
4. Add a focused regression proving that a raw text node under `white-space-collapse: preserve`
   keeps repeated spaces and `\n` characters through `rebuildRichText()`.

This phase must keep existing collapsed HTML indentation behavior by moving that collapse decision
from load time to `rebuildRichText()`, not by preserving all source indentation in default layout.

### Phase 2: Add A Whitespace Normalization Policy

**Files:**

- `include/eepp/ui/uirichtext.hpp`
- `src/eepp/ui/uirichtext.cpp`

Introduce a small helper around the current enum:

```cpp
struct WhiteSpaceProcessing {
    WhiteSpaceCollapse collapse;
    bool lineWrap;
    bool preservesSegmentBreaks;
    bool preservesSpaces;
    bool convertsTabsToSpaces;
    bool breakAfterSpaces;
    bool discardWhitespace;
};
```

The exact shape can be simplified, but the call site needs explicit booleans instead of repeated
`collapse == X` checks.

Add a helper with one responsibility:

```cpp
static String processWhiteSpaceForLayout(
    String::View raw,
    WhiteSpaceProcessing policy,
    WhiteSpaceBoundary boundary );
```

`WhiteSpaceBoundary` should carry only the cross-node state needed by inline formatting:

- whether the previous emitted text ended with collapsible space,
- whether the logical previous/next item is inline,
- whether block-boundary trimming is allowed,
- whether the caller is inside an inline box.

Keep this allocation-conscious:

- Continue using `String::View` where text is passed through unchanged.
- Allocate a transformed `String` only when the policy actually changes the bytes.
- Reuse the existing local transformed string pattern already used for `text-transform`.

### Phase 3: Apply Policy In `rebuildRichText()`

**File:** `src/eepp/ui/uirichtext.cpp`

Replace the current `shouldCollapse` boolean with a full effective policy:

1. Resolve the effective whitespace mode from the nearest `UIRichText`/`UITextSpan` ancestor.
2. Apply the same policy to:
   - standalone `UITextNode` content,
   - inline `UITextSpan::getText()` content,
   - `UIRichText` self text when a `UITextSpan` is being rebuilt directly.
3. Preserve the existing logical-prev/logical-next handling for default collapsed HTML whitespace.
4. Make block-boundary trimming conditional:
   - allowed for `collapse` and `preserve-breaks` around collapsible spaces,
   - not allowed for `preserve`, `preserve-spaces`, or `break-spaces`.
5. Treat `<br>` as a forced line break independent of the whitespace collapse mode.
6. Make `UITextNode::setLayoutCharCount()` reflect the processed layout string length, while leaving
   raw node text intact.

Expected examples:

- Default HTML source indentation around block children still disappears.
- `white-space-collapse: preserve` keeps `"  a\n  b"`.
- `white-space-collapse: preserve-breaks` turns `"  a\n   b"` into `" a\n b"` behavior.
- `white-space-collapse: preserve-spaces` turns tabs/newlines into spaces without collapsing spaces.
- `white-space-collapse: discard` emits no spaces/tabs/newlines.

### Phase 4: Teach `RichText` About `break-spaces`

**Files:**

- `include/eepp/graphics/richtext.hpp`
- `src/eepp/graphics/richtext.cpp`
- `include/eepp/graphics/linewrap.hpp`
- `src/eepp/graphics/linewrap.cpp`
- `src/tests/unit_tests/richtext_tests.cpp`

`break-spaces` cannot be fully implemented only by preprocessing strings. It changes wrapping and
intrinsic sizing:

1. Add a compact wrap behavior flag to `RichText`, for example:

   ```cpp
   enum class WhiteSpaceWrapMode {
       Normal,
       Preserve,
       BreakSpaces
   };
   ```

   Or use smaller booleans if that fits the existing code better.

2. Set it from `UIRichText::rebuildRichText()` based on the effective container policy.
3. Thread the behavior into `RichTextInlineLayouter` and `LineWrap::computeLineBreaksEx()`.
4. For `break-spaces`, add a soft wrap opportunity after every preserved space/tab.
5. Ensure trailing preserved spaces contribute to rendered width and min/max intrinsic widths.
6. Keep the default `RichText` behavior unchanged for non-HTML callers.

This is the phase most likely to expose assumptions in line wrapping and intrinsic sizing. Keep the
first patch narrow: only add the flags needed by `break-spaces`, then expand tests.

### Phase 5: CSS Shorthand And Enum Completeness

**Files:**

- `include/eepp/ui/uirichtext.hpp`
- `src/eepp/ui/uirichtext.cpp`
- `src/eepp/ui/css/stylesheetspecification.cpp`
- `src/tests/unit_tests/uihtml_tests.cpp`

Steps:

1. Add `WhiteSpaceCollapse::Discard`.
2. Fix redundant checks in `toWhiteSpaceCollapse()` and include `discard`.
3. Extend `fromWhiteSpaceCollapse()` and `fromWhiteSpace()`.
4. Extend `applyWhiteSpace()` to parse:
   - legacy single-keyword values,
   - `break-spaces`,
   - direct collapse keywords,
   - `wrap` / `nowrap` when present as Level 4 shorthand components.
5. Keep `white-space-trim` out of scope unless it already exists elsewhere; explicitly document this
   limitation near `applyWhiteSpace()` or in `html-layout-architecture.md`.

### Phase 6: Replace The Generic `<pre><code>` Fallback

**File:** `src/eepp/ui/uirichtext.cpp`

Once the whitespace behavior is covered:

1. Remove the generic `mTag == "pre" && child.name() == "code"` branch that creates `UICodeEditor`.
2. Keep the same `UICodeEditor` path only for `UI_TYPE_MARKDOWNVIEW` ancestors and for the explicit
   global opt-in switch.
3. Let ordinary HTML `<code>` be created through `UIWidgetCreator::createFromName()` or the existing
   fallback to `UITextSpan`.
4. Ensure base HTML CSS gives the right defaults:
   - `pre { white-space: pre; }` or equivalent.
   - `code { font-family: monospace; }` if not already present.
   - `pre code` remains normal inline/flow content inside the `<pre>`.
5. Preserve `data-language` as ordinary data/style metadata. Do not reintroduce syntax-highlighting
   behavior through the generic HTML path in this feature.

### Phase 7: Tests

Add tests before removing the fallback.

#### RichText Unit Tests

Add focused tests in `src/tests/unit_tests/richtext_tests.cpp`:

- `WhiteSpaceCollapseCollapseDefault`
- `WhiteSpaceCollapsePreserveSpacesAndBreaks`
- `WhiteSpaceCollapsePreserveBreaks`
- `WhiteSpaceCollapsePreserveSpaces`
- `WhiteSpaceCollapseBreakSpacesWrapsAfterSpaces`
- `WhiteSpaceCollapseDiscard`
- `WhiteSpaceCollapseAcrossInlineSpanBoundaries`

Assertions should inspect generated lines, text fragments, or text-node layout char counts. Avoid
pixel-only assertions unless testing trailing-space width.

#### UIHTML Tests

Add focused tests in `src/tests/unit_tests/uihtml_tests.cpp`:

- `WhiteSpaceCollapsePreCodePreservesIndentation`
- `PreCodeUsesCodeEditorOnlyForMarkdownAncestorOrGlobalOptIn`
- `WhiteSpaceCollapsePreLinePreservesBreaksOnly`
- `WhiteSpaceCollapseBreakSpacesAffectsIntrinsicWidth`

For the `<pre><code>` regression:

```html
<pre id="pre"><code id="code">if (x) {
    return 1;
}
</code></pre>
```

Verify:

- generic HTML `code` is not a `UICodeEditor`.
- markdown/global opt-in `code` is a `UICodeEditor`.
- indentation before `return` survives.
- line count is at least 3.
- the `<pre>`/`<code>` content participates in normal RichText layout.

#### Existing Tests To Re-run

Focused:

```sh
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIRichText.*WhiteSpace*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTML.WhiteSpace*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UITextNode.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTMLFloat.*whitespace*"
```

Broader:

```sh
make -C make/linux -j$(nproc)
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug --filter="UIHTML.*"
projects/scripts/xvfb-run-eepp bin/unit_tests/eepp-unit_tests-debug
git diff --check
```

If the local runner uses the older direct `xvfb-run` form, keep the same ASAN/screen settings:

```sh
ASAN_OPTIONS=detect_leaks=0 xvfb-run -a -s "-screen 0 1280x1024x24" bin/unit_tests/eepp-unit_tests-debug --filter="UIHTML.WhiteSpace*"
```

## Risks And Constraints

- Do not collapse raw text at parse time. The spec says whitespace processing is for rendering and
  must not mutate underlying document data.
- Do not make a tag-specific `<pre>` or `<code>` workaround. The feature belongs to CSS text
  processing and inline formatting.
- Be careful with source indentation in fixture HTML. Removing parse-time collapse will expose many
  indentation-only text nodes unless default `collapse` behavior remains correct in `rebuildRichText()`.
- `break-spaces` affects the line breaker and intrinsic sizing. It is not enough to preserve bytes.
- Keep changes allocation-conscious. The RichText rebuild path is hot during layout invalidation.
- Preserve current inline metadata behavior: backgrounds, borders, selection, anchor hit boxes,
  inline-block baselines, floats, and out-of-flow skipping.

## Suggested Patch Order

1. Add tests documenting current failure for preserved raw text and `<pre><code>`.
2. Move PCDATA collapse from load time to policy-driven rebuild time.
3. Implement all non-`break-spaces` collapse modes.
4. Add RichText/LineWrap support for `break-spaces`.
5. Add `discard` parser/state behavior.
6. Restrict the `<pre><code>` `UICodeEditor` fallback to markdown/global opt-in.
7. Update `html-layout-architecture.md` with the final whitespace-processing notes.
8. Run focused tests, then `UIHTML.*`, then the full suite.

## Completion Criteria

- All `white-space-collapse` values have parser, property-string, layout, and test coverage.
- Existing `white-space: nowrap` behavior remains passing.
- Generic HTML `<pre><code>` renders through normal HTML/RichText nodes, not `UICodeEditor`.
- Markdown `<pre><code>` can still render through read-only `UICodeEditor`.
- Code indentation and blank lines are preserved in `<pre>` and `white-space: pre` content.
- Default HTML whitespace still collapses around block and inline boundaries.
- `break-spaces` wraps and measures trailing spaces according to CSS Text Level 4.
- Full unit suite passes under Xvfb/ASAN.
