# HTML Whitespace Collapsing: Moving from Parse-Time to Layout-Time

## Problem Statement

Currently, HTML whitespace collapsing (stripping/collapsing `\n  ` sequences between elements) happens at **parse time** via `HTMLFormatter::collapseXmlWhitespace`, which operates on the raw pugixml DOM tree. At this stage, CSS has not yet been fully resolved — the `display` property (which determines whether an element participates in inline formatting) hasn't been applied to widgets. This means:

1. Elements with `display: inline-block` set via `<style>` blocks are not recognized as inline by `isInlineNode`, causing whitespace between them to be incorrectly stripped.
2. Dynamic changes to `display` (via JavaScript or external CSS) cannot be handled.
3. The current hack (precomputing display from `<style>` blocks) is fragile and doesn't scale to external stylesheets, media queries, or dynamic updates.

**Desired behavior** (matching browsers): whitespace `\n  ` between two `<div>` elements with `display: inline-block` should produce a single space character, exactly as it would between two `<span>` elements.

## Root Cause

The call chain:
```
HTML string
  → gumbo parse
  → serialize to strict XML
  → pugixml parse
  → UIRichText::loadFromXmlNode / UITextSpan::loadFromXmlNode
      → collapseXmlWhitespace(text, pugixml_node)  ← PROBLEM: CSS unresolved here
      → if result is empty → skip (no UITextNode created)
  → later: rebuildRichText(reconstructed from widget tree)
```

At the time `collapseXmlWhitespace` runs, the `pugixml_node`'s siblings are div elements with unknown CSS display properties. The function uses `isInlineNode()` which only checks HTML tag names, not CSS.

## Browser-Correct Architecture

Browsers preserve ALL whitespace text nodes in the DOM tree. Whitespace collapsing happens during **layout** (box tree construction), when computed styles are fully resolved:

1. **DOM keeps all whitespace text nodes.** `\n  ` remains as a real `Text` node between the divs.
2. **Style computation** resolves `display` via the full cascade (inline styles, stylesheets, inheritance, default values).
3. **Box tree construction** checks each text node's adjacent siblings' computed `display`:
   - Both adjacent boxes are inline-level (`display: inline | inline-block | inline-flex | inline-table`)? → Collapse whitespace to a single space.
   - Either adjacent box is block-level? → Strip the space entirely.
   - Text node at start/end of a block container? → Strip the space entirely.
4. **Dynamic changes** (JS sets `display: block`) trigger re-layout, which re-evaluates whitespace boundaries.

## eepp Architecture Analysis

### The Good Separation

eepp already has the right primitives in the right places:

| Component | Responsibility | Has CSS `display`? |
|---|---|---|
| `UIRichText` / `UITextSpan` (`loadFromXmlNode`) | Parse pugixml → create widget tree | ❌ No |
| `UIRichText::rebuildRichText` | Walk widget tree → rebuild `RichText` blocks | ✅ Yes (via `UIHTMLWidget::getDisplay()`) |
| `UITextNode` | Stores text content, participates in layout | N/A (is text, always inline) |
| `RichText` (Graphics layer) | Text layout engine: line-breaking, wrapping | ✅ Yes (via `addCustomSize`'s `isBlock`) |

The critical insight: **`rebuildRichText` already has full access to the computed display of every widget in the tree.** It already classifies widgets as block vs inline (line 743–751):

```cpp
CSSDisplay display = widget->asType<UIHTMLWidget>()->getDisplay();
if ( display == CSSDisplay::Inline || display == CSSDisplay::InlineBlock )
    isBlock = false;
```

This is the PERFECT place to handle whitespace. It already knows each widget's `display`, it already walks the tree in order, and it already has access to siblings (`getPreviousNode()/getNextNode()`).

### The `RichText` Pipeline (what `rebuildRichText` feeds)

`RichText` is a horizontal layout engine. Its blocks are:
- **SpanBlock** (`addSpan`): text with font style, margin, padding. Purely inline — text flows in the same line.
- **CustomBlock** (`addCustomSize`): a spacer with given dimensions. If `isBlock=true`, it breaks the current line and occupies full width. If `isBlock=false` (inline), it sits in the text flow at its given width.

`RichText` does NO whitespace collapsing on its own. It renders exactly the text it receives. If we feed it `"  \n  "` as a span, it renders two spaces, a newline, and two spaces.

### Current `processNode` Lambda (the target for our changes)

In `UIRichText::rebuildRichText(UILayout*, RichText&, IntrinsicMode)` at line 646, the `processNode` lambda handles each child node:

```cpp
auto processNode = [&]( Node* node, auto& processNodeRef ) -> void {
    // CASE 1: UITextNode → addSpan(text, style)
    if ( node->isTextNode() ) {
        richText.addSpan(textNode->getText(), style);
        return;
    }

    // CASE 2: Invisible widgets → skip
    if ( !node->isWidget() || !node->isVisible() ) return;

    // CASE 3: Mergeable spans (UITextSpan) → addSpan + recurse children
    if ( widget->isMergeable() ) {
        richText.addSpan(span->getText(), style, margin, padding);
        // ...recurse children...
    }

    // CASE 4: <br/> → addSpan("\n")
    // CASE 5: Other widgets → addCustomSize(size, isBlock, float, clear)
};
```

Children are iterated at line 802 in order:
```cpp
Node* child = container->getFirstChild();
while ( NULL != child ) {
    processNode( child, processNode );
    child = child->getNextNode();
}
```

## Proposed Architecture

### Core Principle: Move whitespace collapsing from `loadFromXmlNode` → `rebuildRichText`

Instead of collapsing at parse time and skipping empty text nodes, we:

1. **Preserve raw text** in `UITextNode` (and `UITextSpan::mText`). No whitespace collapsing at parse time.
2. **Collapse at layout time** in `processNode`, when the full widget tree (with computed `display`) is available.

### What Changes

#### 1. Remove early whitespace collapsing in `loadFromXmlNode`

**`UIRichText::loadFromXmlNode`** (line 599–607):
```
BEFORE:  String text = HTMLFormatter::collapseXmlWhitespace(child.value(), child);
         if (!text.empty()) { create UITextNode with collapsed text; }
AFTER:   String text = child.value();
         create UITextNode with raw text;  // even if all whitespace
```

**`UITextSpan::loadFromXmlNode`** (lines 412, 423):
```
BEFORE:  mText += HTMLFormatter::collapseXmlWhitespace(...)
         OR create UITextNode with collapsed text
AFTER:   mText += child.value()  // raw
         OR create UITextNode with raw text
```

**New `UITextNode` flag** `mIsWhitespaceOnly` (computed lazily or at creation):
```cpp
bool UITextNode::isWhitespaceOnly() const {
    for (char c : mText)
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v')
            return false;
    return true;
}
```

#### 2. Add `UIWidget::isInlineDisplay()`

A new virtual/helper method that returns whether a widget participates in inline formatting:

```cpp
bool UIWidget::isInlineDisplay() const {
    if ( isTextNode() )
        return true;  // UITextNode is always inline
    if ( isType( UI_TYPE_HTML_WIDGET ) ) {
        CSSDisplay d = static_cast<const UIHTMLWidget*>( this )->getDisplay();
        return d == CSSDisplay::Inline || d == CSSDisplay::InlineBlock;
    }
    return false; // non-HTML, non-text widgets default to block
}
```

Declared in `UIWidget` (inline in header, or in `.cpp`). This mirrors the Pug `display: inline | inline-block` semantics.

#### 3. Add whitespace-collapsing logic in `processNode`

In `rebuildRichText` → `processNode`, replace the `UITextNode` case (line 686–702) with:

```cpp
if ( node->isTextNode() ) {
    UITextNode* textNode = static_cast<UITextNode*>( node );
    String text = collapseInternalWhitespace( textNode->getText() );

    // Determine display type of adjacent siblings
    bool prevIsInline = false;
    Node* prev = node->getPreviousNode();
    while ( prev && prev->isTextNode() ) {
        UITextNode* ptn = static_cast<UITextNode*>( prev );
        if ( !ptn->isWhitespaceOnly() ) break;
        prev = prev->getPreviousNode();
    }
    if ( prev && prev->isWidget() ) {
        prevIsInline = prev->asType<UIWidget>()->isInlineDisplay();
    }

    bool nextIsInline = false;
    Node* next = node->getNextNode();
    while ( next && next->isTextNode() ) {
        UITextNode* ntn = static_cast<UITextNode*>( next );
        if ( !ntn->isWhitespaceOnly() ) break;
        next = next->getNextNode();
    }
    if ( next && next->isWidget() ) {
        nextIsInline = next->asType<UIWidget>()->isInlineDisplay();
    }

    // Strip leading space if prev is not inline (block boundary)
    if ( !prevIsInline && !text.empty() && text[0] == ' ' )
        text = text.substr( 1 );

    // Strip trailing space if next is not inline
    if ( !nextIsInline && !text.empty() && text.back() == ' ' )
        text = text.substr( 0, text.size() - 1 );

    if ( text.empty() )
        return;

    // Get style from parent
    FontStyleConfig style;
    if ( node->getParent()->isType( UI_TYPE_TEXTSPAN ) )
        style = node->getParent()->asType<UITextSpan>()->getFontStyleConfig();
    else if ( node->getParent()->isType( UI_TYPE_RICHTEXT ) )
        style = node->getParent()->asType<UIRichText>()->getRichText().getFontStyleConfig();
    else
        style = richText.getFontStyleConfig();

    richText.addSpan( text, style );
    return;
}
```

Where `collapseInternalWhitespace` is a simple local helper:

```cpp
static String collapseInternalWhitespace( const String& s ) {
    String out;
    out.reserve( s.size() );
    bool inSpace = false;
    for ( size_t i = 0; i < s.size(); ++i ) {
        if ( s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
             s[i] == '\r' || s[i] == '\v' ) {
            if ( !inSpace ) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += s[i];
            inSpace = false;
        }
    }
    return out;
}
```

#### 4. Handle whitespace in `UITextSpan::mText` (no-child-element spans)

When a `UITextSpan` has no child elements (line 420–426), text is accumulated directly into `mText`. Currently `collapseXmlWhitespace` is called. After the change, `mText` contains raw whitespace.

In `processNode` case 3 (mergeable spans), `hasOwnText` is checked:

```cpp
bool hasOwnText = !span->getText().empty() && NULL != span->getFontStyleConfig().Font;
if ( hasOwnText ) {
    richText.addSpan( span->getText(), span->getFontStyleConfig(), margin, padding );
}
```

The `span->getText()` will now contain raw whitespace. We need to collapse it here too:

```cpp
if ( hasOwnText ) {
    String collapsed = collapseInternalWhitespace( span->getText() );
    if ( !collapsed.empty() ) {
        richText.addSpan( collapsed, span->getFontStyleConfig(), margin, padding );
    }
}
```

Boundary stripping (leading/trailing) for `UITextSpan::mText` is handled by the same sibling-display logic as `UITextNode`, because the span itself has siblings whose display types are known.

Actually: for `UITextSpan` that stores text directly, we need to consider its OWN siblings when stripping leading/trailing spaces. This is the same as for `UITextNode` — check prev/next widget display types. The existing code for mergeable spans doesn't do this sibling check, but since mergeable spans already participate in inline formatting (they're inline by nature), the existing `collapseXmlWhitespace` handled the boundary stripping at parse time. After the migration, we need to add boundary stripping here.

However, the sibling info is available in the `processNode` lambda. We can add the same prev/next check:

```cpp
if ( hasOwnText ) {
    String collapsed = collapseInternalWhitespace( span->getText() );
    
    // Boundary strip (same logic as UITextNode above)
    // ...

    if ( !collapsed.empty() ) {
        richText.addSpan( collapsed, span->getFontStyleConfig(), margin, padding );
    }
}
```

#### 5. Clean up `HTMLFormatter`

After migration, the following functions are no longer needed:
- `HTMLFormatter::isInlineNode`
- `HTMLFormatter::hasSignificantText`
- `HTMLFormatter::getLogicalPrev`
- `HTMLFormatter::getLogicalNext`
- `HTMLFormatter::collapseXmlWhitespace`
- `HTMLFormatter::precomputeDisplayStyles`
- The static `sDocInlineSelectors` map

They can be removed (or deprecated/moved to a legacy namespace for a transition period).

The ONLY remaining function in `HTMLFormatter` would be `HTMLtoXML` (Gumbo parse + serialize to strict XML).

### Why This Is Simple

The change boils down to:

1. **Delete** ~120 lines of whitespace logic from `htmlformatter.cpp` (`collapseXmlWhitespace` + 4 helpers + precompute + static map)
2. **Remove** 2 calls to `collapseXmlWhitespace` in `uirichtext.cpp` and `uitextspan.cpp`, keeping the raw text
3. **Add** ~40 lines in `uirichtext.cpp` (`collapseInternalWhitespace` + sibling boundary checks in `processNode`)
4. **Add** `UIWidget::isInlineDisplay()` (~10 lines)

The total net change is approximately -80 lines, with simpler logic in the right place.

### Edge Cases Handled

| Scenario | How it's handled |
|---|---|
| Whitespace-only text between two inline-block divs | `prevIsInline=true, nextIsInline=true` → space kept |
| Whitespace-only text between two block divs | `prevIsInline=false, nextIsInline=false` → stripped entirely |
| Whitespace at start of container | `prev` is null → `prevIsInline=false` → leading space stripped |
| Whitespace at end of container | `next` is null → `nextIsInline=false` → trailing space stripped |
| Text " hello world " with neighbor blocks | Internal spaces collapsed, leading/trailing stripped |
| Two consecutive whitespace text nodes | `while (prev->isTextNode())` skips them to find real boundary |
| `<span>text</span> <img/>` | Both span and img are inline → space kept |
| Dynamic JS changes `display` | Every `rebuildRichText` call re-evaluates, up-to-date `getDisplay()` |
| External stylesheets | `getDisplay()` reflects the full cascade at layout time |
| `<pre>` or `<code>` elements | These set `white-space: pre` via CSS, which should suppress collapsing entirely. The `csslayouttypes.hpp` has no `CSSWhiteSpace` enum yet — this plan does NOT add it, but the `processNode` change could check for a future `CSSWhiteSpace` property to bypass collapsing. |

### What This Plan Does NOT Handle (Known Limitations)

1. **Deep logical prev/next traversal**: The original `getLogicalPrev/Next` traverses up/down the tree to find the "visually closest" element. For example:
   ```html
   <span><b>hello</b></span> <img/>
   ```
   The space between `</span>` and `<img/>` is logically between `</b>` and `<img/>`. The current plan only checks DIRECT siblings. For this specific case, both `span` and `img` are inline-direct-siblings, so it works. The only case this fails is:
   ```html
   <div><span>text</span> </div>
   ```
   Here the space is between `</span>` and `</div>`. `span` is inline, `</div>` is a container end (null next sibling). Our check: `next` is null → `nextIsInline=false`, so trailing space stripped. This is correct: trailing space at end of block container is stripped.

   The deep traversal is only needed when the immediate sibling is a wrapper with only inline children — and in those cases, the wrapper's `isInlineDisplay()` returns true, which gives the same result as drilling down would. So direct-sibling check covers all practical cases.

2. **`white-space` CSS property**: We don't have `CSSWhiteSpace` yet. The `processNode` change can easily accommodate it later. For now, elements like `<pre>` would have their whitespace collapsed (which is wrong for `<pre>`). But `<pre>` currently handles its content differently in the widget tree — its text goes into `UICodeEditor`, not `UIRichText`, so this is a non-issue in practice.

3. **Multiple consecutive whitespace-only text nodes**: The `while (prev->isTextNode())` loop skips them correctly. But if there are 3+ consecutive whitespace nodes, only the middle one survives (as a single space), while the first and last get boundary-stripped. This matches HTML behavior.

4. **`UITextSpan` with child elements**: When a `UITextSpan` has child elements, text is stored in child `UITextNode`s, which are processed by the outer `processNode` loop (since the span is mergeable, its children are recursed). So the `UITextNode` whitespace logic handles it. The `UITextSpan`'s own `hasOwnText` path is only for spans without child elements.

### Testing Strategy

1. **Background atlas test** (`bin/unit_tests/assets/html/background_atlas.html`): The 20 inline-block tile divs should have 1px spaces between them (matching browser rendering). Delete the golden image before running.

2. **New whitespace-specific HTML tests**:
   - `whitespace_inline_blocks.html`: 4 inline-block divs with `\n  ` between each — should render with spaces.
   - `whitespace_block_divs.html`: 4 block divs with `\n  ` between each — spaces should be stripped.
   - `whitespace_mixed.html`: Mix of block and inline-block elements with whitespace — test boundary conditions.
   - `whitespace_text_nodes.html`: Text like `" hello  world "` between various widget types — test internal collapsing + boundary stripping.

3. **Visual golden test**: Create a new golden image test with the above HTML files, comparing against expected pixel output.

4. **Regression**: Run full test suite (262 tests) to ensure no existing tests break.

## Step-by-Step Implementation Plan

### Step 1: Add `UIWidget::isInlineDisplay()`

**File:** `include/eepp/ui/uiwidget.hpp` (declaration), `src/eepp/ui/uiwidget.cpp` (definition)

```cpp
// In uiwidget.hpp, in the public section:
bool isInlineDisplay() const;

// In uiwidget.cpp:
bool UIWidget::isInlineDisplay() const {
    if ( isTextNode() )
        return true;
    if ( isType( UI_TYPE_HTML_WIDGET ) ) {
        CSSDisplay d = static_cast<const UIHTMLWidget*>( this )->getDisplay();
        return d == CSSDisplay::Inline || d == CSSDisplay::InlineBlock;
    }
    return false;
}
```

Include `uihtmlwidget.hpp` in `uiwidget.cpp` if not already. (It currently includes `uinode.hpp` → `uihtmlwidget.hpp` is likely already reachable.)

### Step 2: Add `UITextNode::isWhitespaceOnly()`

**File:** `include/eepp/ui/uitextnode.hpp`, `src/eepp/ui/uitextnode.cpp`

```cpp
// In uitextnode.hpp:
bool isWhitespaceOnly() const;

// In uitextnode.cpp:
bool UITextNode::isWhitespaceOnly() const {
    for ( char c : mText ) {
        if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v' )
            return false;
    }
    return true;
}
```

### Step 3: Add `collapseInternalWhitespace` helper to `UIRichText`

**File:** `src/eepp/ui/uirichtext.cpp`

Add as a file-local anonymous namespace function before `rebuildRichText`:

```cpp
namespace {
String collapseInternalWhitespace( const String& s ) {
    String out;
    out.reserve( s.size() );
    bool inSpace = false;
    for ( size_t i = 0; i < s.size(); ++i ) {
        if ( s[i] == ' ' || s[i] == '\t' || s[i] == '\n' ||
             s[i] == '\r' || s[i] == '\v' ) {
            if ( !inSpace ) {
                out += ' ';
                inSpace = true;
            }
        } else {
            out += s[i];
            inSpace = false;
        }
    }
    return out;
}
}
```

### Step 4: Modify `processNode` in `rebuildRichText` — `UITextNode` case

**File:** `src/eepp/ui/uirichtext.cpp`, function `rebuildRichText(UILayout*, RichText&, IntrinsicMode)`

Replace lines 686–702 (the `UITextNode` block) with the new whitespace-aware version described in section 3 above.

### Step 5: Modify `processNode` — `UITextSpan` hasOwnText case

**File:** `src/eepp/ui/uirichtext.cpp`, same function

Wrap the `hasOwnText` path (lines 716–717) with `collapseInternalWhitespace` and boundary stripping.

### Step 6: Remove `collapseXmlWhitespace` calls from `loadFromXmlNode`

**File:** `src/eepp/ui/uirichtext.cpp`, `UIRichText::loadFromXmlNode`

Replace line 601:
```
// OLD: String text = Tools::HTMLFormatter::collapseXmlWhitespace( child.value(), child );
// NEW: const char* text = child.value();
```
And remove the `if (!text.empty())` guard — always create `UITextNode`.

**File:** `src/eepp/ui/uitextspan.cpp`, `UITextSpan::loadFromXmlNode`

Replace lines 412 and 423 similarly.

### Step 7: Clean up `HTMLFormatter`

**Files:** `include/eepp/ui/tools/htmlformatter.hpp`, `src/eepp/ui/tools/htmlformatter.cpp`

Remove:
- `isInlineNode`
- `hasSignificantText`
- `getLogicalPrev`
- `getLogicalNext`
- `collapseXmlWhitespace`
- `precomputeDisplayStyles`
- `sDocInlineSelectors` static map
- `collectStyleText` helper
- `parseCssForDisplayInline` helper
- Associated includes (`<unordered_map>`, `<unordered_set>` if no longer needed)

Keep only `HTMLtoXML`.

Also remove `precomputeDisplayStyles` calls from `UIRichText::loadFromXmlNode` and `UITextSpan::loadFromXmlNode`.

### Step 8: Build and fix compilation

### Step 9: Delete golden image and run atlas test

```bash
rm bin/unit_tests/assets/html/eepp-ui-background-atlas.webp
ASAN_OPTIONS=detect_leaks=0 xvfb-run bin/unit_tests/eepp-unit_tests-debug --filter="UIBackground.imageAtlasPositioning"
```

The test should pass and regenerate the golden image with spaces between tiles.

### Step 10: Run full test suite

```bash
ASAN_OPTIONS=detect_leaks=0 xvfb-run bin/unit_tests/eepp-unit_tests-debug
```

All 262+ tests should pass.

### Step 11: Stash

```bash
git stash push -m "plan: html-whitespace step11: all steps complete"
```

## Files Modified

| File | Change |
|---|---|
| `include/eepp/ui/uiwidget.hpp` | Add `isInlineDisplay()` declaration |
| `src/eepp/ui/uiwidget.cpp` | Add `isInlineDisplay()` definition |
| `include/eepp/ui/uitextnode.hpp` | Add `isWhitespaceOnly()` declaration |
| `src/eepp/ui/uitextnode.cpp` | Add `isWhitespaceOnly()` definition |
| `src/eepp/ui/uirichtext.cpp` | Add `collapseInternalWhitespace` helper; rewrite `processNode` cases; remove `collapseXmlWhitespace` and `precomputeDisplayStyles` calls |
| `src/eepp/ui/uitextspan.cpp` | Remove `collapseXmlWhitespace` calls; keep raw text |
| `include/eepp/ui/tools/htmlformatter.hpp` | Remove whitespace-related method declarations |
| `src/eepp/ui/tools/htmlformatter.cpp` | Remove whitespace-related implementations |
