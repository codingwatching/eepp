# Browser-Like Layout Invalidation Plan

> Status: PROPOSED - roadmap for replacing coarse layout invalidation with typed,
> browser-like dirty propagation in the HTML/RichText layout path.

## Goal

Move eepp's HTML and `UIRichText` layout invalidation closer to browser-engine
semantics without giving up the current optimized coalescing behavior that made
large Markdown documents fast again.

The desired end state is:

- layout recomputation is proportional to the real dependency that changed,
- child changes preserve enough parent/ancestor dirtiness to be reconciled in the
  next layout phase,
- re-entrant invalidations during a layout pass are coalesced instead of causing
  parent layout loops,
- asynchronous resource changes, especially image and stylesheet loads, update
  final geometry without requiring a later unrelated invalidation,
- `UIRichText` remains usable outside `UIWebView`,
- `UIWebView` can evolve toward an isolated document/scene model without forcing
  the same constraints onto normal eepp GUI layouts.

This is not a request to recompute more aggressively. The whole point is to
retain the fast path while making the skipped work explicit and correct.

## References

- WebKit layout basics and dirty-bit model:
  https://webkit.org/blog/116/webcore-rendering-iii-layout-basics/
- Chromium LayoutNG architecture:
  https://developer.chrome.com/docs/chromium/layoutng/
- Browser rendering pipeline overview:
  https://web.dev/howbrowserswork/
- CSS 2 root/body/canvas background special case:
  https://www.w3.org/TR/CSS2/colors.html#background
- CSS Visual Formatting Model:
  https://www.w3.org/TR/CSS22/visuren.html

## Current Problem

The recent Markdown performance regression exposed two opposite failure modes in
the current invalidation model:

1. Eager parent recomputation can explode into thousands of redundant
   `UIRichText` rebuilds while the same layout tree is already being reconciled.
2. Over-coalescing can skip a required ancestor update, leaving visible stale
   geometry until some unrelated event, such as resize or hover, invalidates the
   layout again.

The first failure is why rendering `README.md` in `UIMarkdownView` became much
slower than ecode 0.8.0. The second failure appeared with delayed resource
changes:

- asynchronous image replacement resized a child after the initial HTML layout,
- asynchronous CSS load changed the size of the Hacker News table/body content,
- the affected rich-text/body subtree could become correct locally,
- a required ancestor, especially the root `html` element, could remain stale
  until the next unrelated layout invalidation.

The core problem is not simply "parents are not dirty enough". It is that eepp
mostly tracks layout dirtiness as one coarse `mDirtyLayout` state plus message
bubbling. Browser engines preserve more information:

- this node itself needs layout,
- a normal-flow child needs layout,
- an out-of-flow positioned child needs layout,
- intrinsic size changed,
- style changed,
- formatting-context contents changed,
- layout can stop at this boundary,
- layout must continue through this ancestor because its used size depends on
  the changed descendant.

Without those distinctions, eepp has to choose between two imprecise behaviors:
invalidating too broadly, or suppressing invalidations that later prove to be
required.

## Current Bridge Fix

The targeted `html > body` invalidation is a compatibility bridge, not a generic
layout invalidation strategy.

The root `html` element has browser-specific responsibilities that ordinary
`UIRichText` containers do not have. In particular, it must contain the body
extent, and CSS has several special root/body propagation rules. When async CSS
or table layout causes the body to grow after the root has already performed its
current layout pass, the root must be marked dirty so the document height is
reconciled on the next dirty-layout flush.

This bridge is acceptable only because it is scoped to the HTML root/body
contract. It must not become the model for arbitrary rich-text ancestors. A
generic version would recreate the Markdown performance regression by allowing
deep child changes to repeatedly dirty ancestors without understanding the
dependency that changed.

## Why Browser Engines Avoid This Failure

Browser engines do not treat layout invalidation as a request to immediately
re-run every interested ancestor. They mark objects dirty and later run layout
from carefully chosen roots.

Conceptually, a child change in a browser does two things:

1. It marks the child or formatting object as needing layout.
2. It preserves a child-dirty dependency on ancestors whose used geometry may
   depend on that child.

During the layout phase, the engine decides whether each ancestor must recompute
itself, descend into dirty children, or stop at a relayout boundary. That
decision is based on formatting context, containment, positioning, intrinsic
sizing, and style dependencies.

eepp does something similar at a high level with dirty-layout queues and
coalescing, but it lacks the typed dependency state needed to make the same
decision reliably. `UIRichText` makes this harder because it owns a parallel
formatted text stream built from child widgets. A descendant can change in a way
that invalidates:

- only its own widget geometry,
- the parent rich-text stream,
- the intrinsic size exposed by the rich-text owner,
- an ancestor block/table/flex layout,
- the document root extent.

Those are different invalidation scopes, but today they often flow through the
same parent layout-attribute message.

## Constraints

- Do not restore eager re-entrant parent recomputation.
- Do not make every rich-text child resize dirty every ancestor.
- Do not add tag-specific layout fixes where the behavior belongs to a generic
  CSS/layout concept.
- Keep `UIRichText` valid as a standalone widget, independent of `UIWebView`.
- Let the future per-`UIWebView` `UISceneNode` branch become a clean document
  boundary, but do not require that branch before fixing generic invalidation.
- Preserve the existing no-heap-allocation intent in hot dirty-layout queues.
  Reusable snapshots or `SmallVector`-style storage are preferred over
  allocating per invalidation flush.

## Proposed Architecture

Introduce typed layout dirtiness and propagate only the dependency that actually
changed.

Exact names can change, but the model should distinguish at least:

```cpp
enum class LayoutDirtyReason : Uint32 {
	None = 0,
	SelfLayout = 1 << 0,
	NormalChildLayout = 1 << 1,
	OutOfFlowChildLayout = 1 << 2,
	IntrinsicSize = 1 << 3,
	Style = 1 << 4,
	FormattingContext = 1 << 5,
	DocumentExtent = 1 << 6,
};
```

The state should live near `UILayout` / `UIHTMLWidget` boundaries rather than in
ad-hoc element code. HTML-specific reasons can be layered on top of the generic
bits when needed.

## Implementation Design

This section is intentionally detailed. The implementation should be incremental
and reversible, but every phase should move toward one invariant:

> Layout invalidation records what dependency became stale. Layout update later
> consumes that dependency at the smallest correct relayout boundary.

Do not start by changing all callers. First introduce storage and instrumentation
that behaves exactly like current `setLayoutDirty()`. Only after the baseline is
identical should callers start emitting more precise reasons.

### Core Data Model

Add a compact dirty-reason bitset. Prefer a plain integer-backed type over an
allocation-heavy structure.

Conceptual header-level API:

```cpp
enum class LayoutDirtyReason : Uint32 {
	None = 0,
	SelfLayout = 1u << 0,
	NormalChildLayout = 1u << 1,
	OutOfFlowChildLayout = 1u << 2,
	IntrinsicSize = 1u << 3,
	Style = 1u << 4,
	FormattingContext = 1u << 5,
	DocumentExtent = 1u << 6,
	Viewport = 1u << 7,
};

using LayoutDirtyFlags = Uint32;
```

`UILayout` should own the local dirty state:

```cpp
class UILayout : public UIWidget {
  protected:
	bool mDirtyLayout{ false };
	LayoutDirtyFlags mDirtyReasons{ 0 };
	LayoutDirtyFlags mDeferredDirtyReasons{ 0 };
	bool mUpdatingLayoutTree{ false };
};
```

The initial compatibility behavior should be:

```cpp
void UILayout::setLayoutDirty() {
	setLayoutDirty( LayoutDirtyReason::SelfLayout );
}
```

Then add the explicit API:

```cpp
void UILayout::setLayoutDirty( LayoutDirtyReason reason );
void UILayout::addLayoutDirtyReasons( LayoutDirtyFlags reasons );
LayoutDirtyFlags UILayout::consumeLayoutDirtyReasons();
LayoutDirtyFlags UILayout::getLayoutDirtyReasons() const;
```

The reason bits must be merged, never replaced:

```cpp
mDirtyReasons |= toBits( reason );
```

If a layout is already dirty and receives a new reason, the dirty queue should
not add a duplicate node, but the node must retain the additional reason. This
is the first major difference from the current boolean-only state.

### Dirty Queue Entries

`UISceneNode::mDirtyLayouts` can remain a set of `UILayout*` for the first
implementation. Reason bits live on the `UILayout`, so the queue does not need
to allocate an entry object per invalidation.

Keep the existing reusable snapshot strategy:

- `mDirtyLayouts` remains the live coalescing set.
- `mDirtyLayoutsSnapshot` remains reusable `SmallVector<UILayout*, 64>`.
- `updateDirtyLayouts()` copies pointers into the snapshot, then clears the set.
- invalidations created during the pass stay in `mDirtyLayouts` for the next
  outer invalidation-depth iteration.

Do not move `mDirtyLayouts` into a local temporary. Moving the set transfers its
buckets and makes large documents rebuild allocation state on the next wave.

The dirty queue can later evolve to:

```cpp
struct DirtyLayoutEntry {
	UILayout* layout{ nullptr };
	LayoutDirtyFlags reasons{ 0 };
};
```

but that should happen only if profiling shows per-layout reason reads are not
enough. The first version should avoid expanding queue memory and focus on
correct semantics.

### Coalescing Rules

The current queue performs two important coalescing operations:

1. If a dirty ancestor already exists and the path is all layouts, skip adding
   the child.
2. If adding an ancestor, remove dirty descendants that the ancestor will update.

Typed reasons change when those rules are valid.

Safe ancestor coalescing:

- `SelfLayout` on a child can be coalesced into a dirty ancestor only if the
  ancestor's update will recursively update that child before using stale child
  geometry.
- `NormalChildLayout`, `FormattingContext`, and `IntrinsicSize` cannot be
  blindly discarded when an ancestor is already dirty. Their reason bits must be
  preserved somewhere the ancestor update can see.
- `OutOfFlowChildLayout` should coalesce toward the containing block, not the
  normal parent chain.
- `DocumentExtent` should coalesce toward the document root, not ordinary UI
  ancestors.

Initial conservative rule:

```cpp
if ( dirtyAncestorAlreadyQueued ) {
	ancestor->addLayoutDirtyReasons( reasonsThatAncestorMustKnow );
	node->addLayoutDirtyReasons( reasonsThatChildMustKeep );
	return;
}
```

For phase 2, `reasonsThatAncestorMustKnow` can simply be all reasons. That keeps
behavior conservative without adding more queue entries. Later phases can split
reasons more precisely.

When adding an ancestor and removing dirty descendants, do not lose descendant
reason bits. Either:

- merge descendant reasons into the ancestor before erasing the descendant from
  `mDirtyLayouts`, or
- leave descendants queued when their reasons are not implied by the ancestor.

The first option is simpler but can be too broad. The second option is more
browser-like, because child-dirty work can remain below a relayout boundary. The
recommended path is:

1. Phase 2: merge reasons upward to preserve correctness.
2. Phase 3+: stop merging reasons that belong to a child formatting context or
   out-of-flow containing block.

### Update Pass Contract

`UILayout::updateLayoutTree()` should consume reasons in a controlled order.

Current behavior:

```cpp
mUpdatingLayoutTree = true;
updateLayout();
for ( auto layout : mLayouts )
	layout->updateLayoutTree();
mUpdatingLayoutTree = false;
onLayoutUpdate();
```

This is parent-first. That is fine for many eepp layouts, but it is the source
of several HTML issues: a parent can measure child geometry before child layout
has discovered final intrinsic or block size.

Do not flip the whole tree to child-first. That would break layouters that must
assign child constraints before children can lay out. Instead, make the contract
explicit:

- parent layout establishes constraints,
- child layouts settle under those constraints,
- parent may perform a bounded reconciliation step if child exposed geometry
  changed in a way the parent depends on.

Conceptual shape:

```cpp
void UILayout::updateLayoutTree() {
	LayoutDirtyFlags reasons = consumeLayoutDirtyReasons();
	mUpdatingLayoutTree = true;

	LayoutUpdateResult before = captureExposedLayoutResult();
	updateLayoutWithReasons( reasons );

	for ( auto layout : mLayouts )
		layout->updateLayoutTree();

	LayoutDirtyFlags childEffects = consumeDeferredDirtyReasons();
	if ( needsLocalReconcile( reasons, childEffects ) ) {
		updateLayoutWithReasons( childEffects );
	}

	mUpdatingLayoutTree = false;

	LayoutUpdateResult after = captureExposedLayoutResult();
	propagateLayoutResult( before, after, reasons | childEffects );
	onLayoutUpdate();
}
```

The first implementation does not need all these functions. It can start with:

- record dirty reasons,
- preserve deferred reasons during active layout,
- expose `onLayoutUpdate()` enough data to compare old/new size.

But this is the direction: absorbed invalidations must become explicit deferred
reasons, not vanish.

### Layout Update Result

Each layout pass should eventually report what changed externally.

Conceptual result:

```cpp
struct LayoutUpdateResult {
	Sizef oldSize;
	Sizef newSize;
	Float oldMinIntrinsicWidth{ 0 };
	Float newMinIntrinsicWidth{ 0 };
	Float oldMaxIntrinsicWidth{ 0 };
	Float newMaxIntrinsicWidth{ 0 };
	bool positionChanged{ false };
	bool childGeometryChanged{ false };
	bool documentExtentChanged{ false };
};
```

Avoid computing intrinsic widths for every layout pass in phase 1. Intrinsic
fields should be lazy or provided only by layouters that already computed them.

The important minimal comparison is:

```cpp
bool exposedSizeChanged =
	eeabs( oldSize.x - newSize.x ) > 0.01f ||
	eeabs( oldSize.y - newSize.y ) > 0.01f;
```

If exposed size did not change, do not notify ancestors. If child positions
changed but the owner size did not, redraw/hit-testing may need invalidation,
but parent layout usually does not.

### Active Layout Guard

The current `UIRichText` guard is performance-critical:

- while the owner is already rebuilding/positioning its formatting stream,
  descendant `LayoutAttributeChange` messages must not re-enter the owner,
- re-entry produces thousands of duplicate `rebuildRichText()` calls on large
  Markdown documents.

The future guard should not simply `return 1`. It should record why it returned.

Conceptual API:

```cpp
void UILayout::deferLayoutDirtyReason( LayoutDirtyReason reason ) {
	mDeferredDirtyReasons |= toBits( reason );
}
```

In `UIRichText::onMessage()`:

```cpp
if ( mUISceneNode->isUpdatingLayouts() && mUpdatingLayoutTree && !isOutOfFlow() ) {
	invalidateIntrinsicSize();
	deferLayoutDirtyReason( LayoutDirtyReason::FormattingContext );
	return 1;
}
```

Do not use this as a generic "retry me every time" switch. A previous attempt to
dirty direct layout children from this guard fixed some stale cases but regressed
the README benchmark to thousands of extra rebuilds. The deferred reason must be
interpreted at the owner boundary and propagated only if the owner exposes a new
size/intrinsic result.

### Message Translation Layer

`NodeMessage::LayoutAttributeChange` is too coarse. Keep it for compatibility,
but translate it near the receiver into dirty reasons.

Initial mapping:

| Sender / condition | Receiver | Dirty reason |
|---|---|---|
| own size/padding/margin/style changed | self layout | `SelfLayout` |
| child normal-flow size changed | parent layout | `NormalChildLayout` |
| child text/font/style changed inside `UIRichText` | nearest rich-text owner | `FormattingContext | IntrinsicSize` |
| image natural size changed | image and rich-text owner | `IntrinsicSize | FormattingContext` |
| absolute child changed | containing block | `OutOfFlowChildLayout` |
| fixed child changed | viewport/document root | `OutOfFlowChildLayout | Viewport` |
| stylesheet combined in `UIWebView` | document root | `Style | DocumentExtent | Viewport` |

Do not try to infer all of this inside `UIWidget::notifyLayoutAttrChangeParent()`
at first. That function lacks enough CSS/layout context. Prefer receiver-side
translation in `UIRichText`, `UIHTMLTable`, block/flex/grid layouters, and
document root handling.

### Relayout Boundaries

A relayout boundary is a node where dirty work can usually stop because the
boundary owns a formatting context and can decide whether its exposed result
changed.

Boundaries to model:

- `UIRichText` formatting-context owner.
- `UIHTMLTable` / `TableLayouter`.
- flex and grid containers.
- out-of-flow containing blocks.
- `UIWebView` document root.
- future per-document `UISceneNode`.

Rules:

- Dirty descendants inside a boundary first dirty the boundary owner.
- Ancestors are notified only if the boundary's exposed size, intrinsic size, or
  document extent changed.
- Boundaries with definite sizes can often avoid propagating size dirtiness while
  still updating child geometry.
- Out-of-flow descendants must not dirty normal-flow auto size. They dirty their
  containing block positioning context.

### Document Boundary Implementation

The recent deferred-CSS Hacker News issue showed a concrete current-architecture
gap:

- local file CSS can now load asynchronously through `<link defer>`,
- CSS can finish during `loadLayoutNodes()` while `mIsLoading` is still true,
- `combineStyleSheet()` then sets `mStyleDuringLoad` and returns,
- after loading finishes, styles are applied, but WebView document sizing was
  not refreshed the way it is on viewport resize,
- `#bigbox > td` and `#bigbox > td > table` initially had stale heights and only
  became correct after a 1px viewport-height change.

The bridge for the current architecture is:

- `UIWebView::refreshDocumentLayout()` owns WebView document refresh.
- It calls `containerUpdate()`.
- It calls `updateHTMLMinHeightForDocument()`.
- It dirties `webview::doc` if it is a layout.
- `UISceneNode::combineStyleSheet()` calls this after forced stylesheet reload.
- `UISceneNode::loadLayoutNodes()` calls it when `mStyleDuringLoad` was set.

This is not "perfect invalidation"; it is a scoped document-boundary repair.
The reason it belongs here, not in generic `UIRichText`, is that stylesheet
combines are document-level mutations. They can alter inherited metrics and
viewport/root/body constraints across the entire document. A generic RichText
ancestor retry fixes symptoms but reintroduces the Markdown rebuild storm.

Future typed invalidation should replace this bridge with:

```cpp
documentRoot->setLayoutDirty(
	LayoutDirtyReason::Style |
	LayoutDirtyReason::DocumentExtent |
	LayoutDirtyReason::Viewport );
```

The document root would then refresh root/body/viewport state as part of
consuming those reasons.

### Intrinsic Size Cache Invalidation

Intrinsic caches exist in several places:

- `UIWidget::mIntrinsicWidthsDirty`,
- `UIRichText::mIntrinsicWidthsDirty`,
- `UILayouter::mIntrinsicWidthsDirty`,
- `TableLayouter` column min/max vectors,
- flex/grid item measurements.

Typed invalidation must make cache invalidation explicit.

Examples:

- `FormattingContext` implies `IntrinsicSize` for `UIRichText` only if text,
  inline block, float, or wrapping-affecting style changed.
- `Style` implies `IntrinsicSize` when the property affects metrics:
  `font-*`, `line-height`, `white-space`, `word-break`, `overflow-wrap`,
  margins/padding/borders, width/min/max constraints, display, position, float,
  table/flex/grid properties.
- `Style` does not imply `IntrinsicSize` for paint-only properties:
  color, background color, text decoration when decoration does not affect
  metrics, cursor, pointer-events.

The first implementation can conservatively invalidate intrinsic size for all
style changes in HTML content. Later optimize by property category. Correctness
comes first, but benchmark counters must catch over-broad invalidations.

### Property Categories

Add helper classification near the CSS/property layer:

```cpp
enum class LayoutPropertyImpact : Uint8 {
	None,
	PaintOnly,
	SelfGeometry,
	ChildGeometry,
	IntrinsicSize,
	FormattingContext,
	Document,
};
```

Examples:

| Property | Impact |
|---|---|
| `color` | `PaintOnly` |
| `background-color` | `PaintOnly` |
| `font-size` | `FormattingContext | IntrinsicSize` |
| `line-height` | `FormattingContext | IntrinsicSize` |
| `white-space` | `FormattingContext | IntrinsicSize` |
| `width` / `height` | `SelfGeometry | IntrinsicSize` |
| `min-width` / `max-width` | `SelfGeometry | IntrinsicSize` |
| `display` | `ChildGeometry | FormattingContext | IntrinsicSize` |
| `position` | `SelfGeometry | OutOfFlowChildLayout` depending on value |
| `float` / `clear` | `FormattingContext | IntrinsicSize` |
| table layout properties | `IntrinsicSize | ChildGeometry` |
| flex/grid properties | `ChildGeometry | IntrinsicSize` |

This allows style reload to emit reasoned invalidation instead of every setter
calling the same coarse message path.

### Parent Notification Policy

Ancestor notification should happen after an owner has reconciled its own
layout, not before.

Current dangerous pattern:

```cpp
childChanged();
notifyLayoutAttrChangeParent();
tryUpdateLayout();
```

This lets a generic ancestor become the queued dirty layout and measure stale
child geometry before the actual owner has recomputed.

Preferred pattern:

```cpp
markOwnerDirty( reason );
owner recomputes;
if ( owner exposed result changed )
	notify dependent ancestor;
```

For existing code, migrate class by class:

1. `UIRichText`: do not relay descendant changes before owner reflow.
2. `UIHTMLTable`: invalidate table intrinsic widths locally; notify ancestors
   only when table exposed size/intrinsic result changes.
3. `FlexLayouter` / `GridLayouter`: keep item collection local; notify parent
   only when container size/intrinsic result changes.
4. Generic `UILinearLayout` / `UIGridLayout` remain simpler; they are not HTML
   formatting contexts unless explicitly used as document wrappers.

### Out-Of-Flow Handling

Out-of-flow positioning needs separate dirty routing.

Rules:

- `position: absolute` dirties the nearest containing block.
- `position: fixed` dirties the viewport/document root.
- out-of-flow size/position does not contribute to normal-flow parent auto size,
  except for current compatibility code that intentionally includes some
  absolute descendants in body effective content height.
- `UIRichText::rebuildRichText()` should continue skipping out-of-flow children.
- `updateOutOfFlowPosition()` should run after normal-flow layout because it
  depends on containing block geometry.

Typed reason:

```cpp
containingBlock->setLayoutDirty( LayoutDirtyReason::OutOfFlowChildLayout );
```

Do not let this become `NormalChildLayout`, or fixed/absolute elements will
reintroduce unnecessary parent auto-size recomputation.

### Tables

Table layout needs a dedicated result object because table dependencies are
multi-stage:

1. collect rows/cells,
2. compute intrinsic column widths,
3. resolve table used width,
4. assign cell widths,
5. lay out cell contents,
6. compute row heights,
7. set table wrapper size.

`TableLayouter` should eventually expose:

```cpp
struct TableLayoutResult {
	bool rowHeightsChanged{ false };
	bool columnWidthsChanged{ false };
	bool minIntrinsicWidthChanged{ false };
	bool maxIntrinsicWidthChanged{ false };
	bool tableSizeChanged{ false };
};
```

When a cell changes:

- dirty table intrinsic widths if the change can affect min/max width,
- dirty row heights if the cell block-size can change,
- do not notify the table parent until the table pass knows whether
  `tableSizeChanged` or intrinsic widths changed.

Important: a previous attempt to schedule generic table retries during packing
did not fix the deferred-CSS issue and was not the right abstraction. Tables
need result-based propagation, not blind retry.

### RichText

`UIRichText` should grow a small formatting dirty state:

```cpp
bool mFormattingContextDirty{ false };
bool mIntrinsicSizeDirty{ true };
LayoutDirtyFlags mDeferredFormattingReasons{ 0 };
```

The update path should be:

1. collect old exposed size and maybe cached intrinsic generation,
2. rebuild rich text only if formatting context is dirty or layout constraints
   changed,
3. update `Graphics::RichText` layout,
4. position child widgets/text nodes,
5. compare exposed size,
6. notify parent only if exposed size/intrinsic result changed.

Avoid rebuilding the rich-text stream for:

- pure child position updates,
- paint-only style changes,
- fixed-position descendant changes,
- descendant notifications already produced by this same active positioning
  pass.

Potential generation counters:

```cpp
Uint32 mFormattingGeneration{ 0 };
Uint32 mLastLaidOutFormattingGeneration{ 0 };
Uint32 mConstraintGeneration{ 0 };
```

These can prevent repeated rebuilds when the dirty queue processes multiple
layout roots that point at the same RichText owner.

### WebView Document Root

Current architecture:

- `UIWebView` is a scroll view in the normal UI scene.
- `mDocContainer` is a `UILinearLayout`.
- `html` and `body` are children inside that container.
- root/body/document sizing is maintained partly by `UIWebView`.

Near-term:

- keep WebView-specific document refresh in `UIWebView`,
- route async stylesheet load and resource completion through document refresh
  instead of generic ancestor invalidation,
- keep the helper scoped to `UI_TYPE_WEBVIEW` and `UI_TYPE_HTML_HTML`.

Future per-document scene:

- each `UIWebView` owns a document `UISceneNode`,
- the document scene has its own dirty style/layout queues,
- root/body/viewport handling is a document-scene responsibility,
- host UI receives only final scrollable size / viewport invalidations,
- standalone `UIRichText` continues to use shared formatting and layouters but
  does not inherit WebView document semantics.

### Rollback Checkpoint Implementation

At the end of every phase:

```sh
git stash push -m "phase-N topic validated-tests" -- <changed files>
git stash apply stash@{0}
```

Do not use `git stash pop`; the checkpoint must remain. If a phase spans many
files, include all relevant files explicitly. If generated files or unrelated
dirty user files exist, do not stash them unless they belong to the phase.

Validation should be mentioned in the stash message:

```text
phase-3 richtext-formatting-dirty validated-markdown-hn-image
phase-5 webview-document-boundary validated-deferred-css-markdown
```

Before creating the stash:

- run focused tests,
- run `Benchmark.MarkdownReadme`,
- run `git diff --check`,
- inspect `git status --short` and make sure unrelated files are not included.

## Propagation Rules

### Self Layout

Use when the widget's own used position, size, padding, margin, border, or style
state requires recalculation.

Propagation:

- enqueue the widget or nearest layout root,
- notify parent only if the parent's layout depends on this widget's used size
  or position.

### Normal-Flow Child Layout

Use when an in-flow descendant changed and the parent may need to reconcile child
geometry during its next layout.

Propagation:

- preserve a child-dirty bit on ancestors until the layout phase consumes it,
- stop at formatting-context boundaries that can prove the ancestor's exposed
  size is unchanged,
- continue upward when intrinsic size or auto size may change.

### Out-Of-Flow Child Layout

Use for `position: absolute` and `position: fixed` descendants.

Propagation:

- dirty the containing block or fixed-position root for placement,
- do not dirty normal-flow ancestors for auto-size contribution, because
  out-of-flow boxes do not affect normal-flow sizing.

### Intrinsic Size

Use when a widget's min-content, max-content, preferred, or auto-size
contribution changes.

Examples:

- image natural size appears after async load,
- text/font metrics change,
- rich-text wrapping changes exposed min/max width,
- table column intrinsic widths change.

Propagation:

- propagate through ancestors that use intrinsic size,
- avoid propagating through containers with definite sizes when the change cannot
  affect their used size.

### Formatting Context

Use when the internal formatting stream of a context is stale.

Examples:

- `UIRichText::rebuildRichText()` contents changed,
- inline text span style affects line metrics,
- custom block size affects line wrapping,
- float/clear participation changes.

Propagation:

- dirty the formatting-context owner first,
- notify ancestors only after the owner knows whether its exposed size changed,
- avoid synchronous parent recomputation while the owner is already updating its
  own layout tree.

### Document Extent

Use for document-root invariants, not for generic widget layout.

Examples:

- HTML root must contain body content extent,
- viewport/document scrollable height changed,
- root/body special CSS propagation affected document metrics.

Propagation:

- route through `UIWebView` document root or the future per-document
  `UISceneNode`,
- keep this bit out of normal eepp GUI containers unless they explicitly opt
  into document-like behavior.

## RichText-Specific Direction

`UIRichText` should be treated as a formatting-context owner.

When a descendant changes:

1. If the descendant is in normal flow and participates in the rich-text stream,
   mark the nearest `UIRichText` formatting context dirty.
2. During the next layout pass, rebuild/reflow that context once.
3. Compare the context's exposed geometry/intrinsic size before notifying
   ancestors.
4. If the context is already updating its layout tree, coalesce the descendant
   notification into the active pass instead of recursively recomputing the
   parent.

This preserves the successful Markdown optimization: thousands of inline/block
children can settle into one owner-level rich-text rebuild sequence instead of
causing repeated ancestor rebuilds.

The missing piece is that the coalesced dirty reason must survive until the
active layout phase can decide whether ancestor geometry changed. A boolean
"skip because updating" guard is too weak unless it stores the reason that was
skipped.

## Table-Specific Direction

Tables are high-risk because a local change can affect:

- cell intrinsic widths,
- column width distribution,
- row heights,
- table wrapper size,
- containing block size,
- document extent.

`TableLayouter` should expose explicit invalidation results after layout:

- no exposed size change,
- block-axis size changed,
- inline-axis intrinsic width changed,
- both axes changed.

Ancestors should react to that result instead of guessing from generic child
messages. This matters for real pages such as Hacker News, where the page height
is effectively the height of one large table after async CSS applies.

## WebView And Document Boundaries

The branch where each `UIWebView` owns its own `UISceneNode` is the right long
term direction for HTML documents.

Benefits:

- document dirty queues are isolated from the normal eepp GUI scene,
- root/body/viewport special behavior has a clear owner,
- resource loading can target a document lifecycle,
- document memory and cached layout state can be released as a unit,
- future stacking context, paint invalidation, and scrollable viewport behavior
  can live in the document scene instead of leaking into generic widgets.

This should not remove standalone `UIRichText`. The intended split is:

- `UIRichText`: reusable formatting-context widget for eepp GUI and embedded
  HTML-like content.
- `UIWebView`: document host with browser-like root/body/viewport/resource
  semantics.
- shared layouters: block, inline, table, flex, grid, and positioning logic used
  by both when applicable.

## Implementation Phases

Before starting the phases, keep a persistent rollback checkpoint workflow:

- after each phase is working and validated, create a named `git stash` copy for
  that phase,
- treat these stashes as archival safety snapshots, not temporary working
  stashes,
- do not drop or pop the checkpoint stash after recovering from it; use
  non-destructive recovery, such as applying it to a scratch branch or copying
  the relevant diff,
- include the phase number, topic, and validation status in the stash message,
  for example `phase-2 typed-dirty-state validated-markdown-hn`,
- create a fresh checkpoint for every materially different phase result, so
  later experiments can roll back to the last known-good implementation without
  losing intermediate progress.

### Phase 1: Instrumentation Guardrails

Keep the benchmark and counters that exposed the regression.

Required metrics:

- total dirty-layout invalidations,
- rich-text rebuild count,
- layout-tree update count,
- dirty flush wall time,
- Markdown README benchmark total time.

Required fixtures:

- `Benchmark.MarkdownReadme`,
- async image resize in a rich-text ancestor,
- async CSS load for Hacker News-like table/body/root sizing,
- focused table/body/html height invariant.

The benchmark should remain cheap enough to run during layout work and should
fail loudly if a "correctness" change silently restores thousands of redundant
rebuilds.

### Phase 2: Add Typed Dirty State

Add compact dirty-reason bits to the layout path.

Requirements:

- no heap allocation per invalidation,
- queue coalescing remains stable,
- invalidations generated during a flush are preserved for the next flush
  iteration,
- existing `setLayoutDirty()` can initially map to `SelfLayout` for
  compatibility,
- new code paths use more precise reasons where known.

Start by plumbing the state without changing behavior. Tests and benchmarks
should remain equivalent to the current optimized implementation.

### Phase 3: Convert RichText Notifications

Convert `UIRichText`, `UITextSpan`, and replaced inline/custom blocks to emit
formatting-context and intrinsic-size dirty reasons.

Important cases:

- text content changes,
- inline style changes that affect metrics,
- custom block size changes,
- image natural size changes after async load,
- min/max width queries,
- active `UIRichText::updateLayoutTree()` coalescing.

The active-layout guard should record skipped dirty reasons instead of merely
returning. At the end of the current layout pass, the owner can decide whether
its exposed size changed and whether parent propagation is required.

### Phase 4: Convert HTML Block/Table Boundaries

Teach block and table layouters to report the external effect of their layout
pass.

At minimum:

- child geometry changed but container size did not,
- container block-size changed,
- container inline-size/intrinsic width changed,
- document extent may have changed.

Use those results to replace broad parent dirtying with targeted propagation.

### Phase 5: Make UIWebView A Document Boundary

After the per-`UIWebView` scene branch is merged or ready, move root/body,
viewport, resource-loading, and document extent invalidation behind that
boundary.

Expected changes:

- root/body bridge becomes document-root logic,
- async CSS/image invalidation targets the document scene,
- dirty layout queues for HTML documents are isolated from normal UI scene
  queues,
- document-level metrics can be flushed after resource completion without
  touching unrelated GUI widgets.

### Phase 6: Remove Compatibility Bridges

Once typed invalidation and document boundaries cover the root/body async cases,
revisit the special `html > body` bridge.

It can be removed only when tests prove:

- async CSS updates root height correctly,
- delayed image load updates document height correctly,
- normal standalone `UIRichText` resizing works,
- Markdown benchmark remains near the optimized baseline,
- the sensitive layout tests continue passing.

## Testing Plan

Keep and expand these tests:

- `Benchmark.MarkdownReadme`
  - validates rebuild counts and timing,
  - protects against re-entrant parent recomputation regressions.
- `UIHTML.HtmlContainsTableBodyHeight`
  - validates root/body/table height after async stylesheet application.
- async image resize test
  - creates an image/custom block with initially unknown size,
  - resizes it after initial layout,
  - asserts rich-text and ancestor geometry update without viewport resize.
- standalone `UIRichText` resize test
  - ensures the fix is not only `UIWebView`-specific.
- out-of-flow child resize test
  - verifies absolute/fixed descendants do not dirty normal-flow auto size.
- existing sensitive tests:
  - `GridContainer.newsblurReducedGrid`,
  - `UIHTML.ContactFormLayout`,
  - `UIRichText.MinMaxWidthChildren`.

When adding tests, prefer invariants over pixel-perfect snapshots:

- parent contains child bottom edge,
- body/html contain table/body content,
- intrinsic min/max width changes are reflected,
- fixed-size ancestors are not unnecessarily recomputed,
- benchmark counters stay under known thresholds.

## Risks

- A typed invalidation system can become more complicated than the current bug if
  the bits are too granular or inconsistently applied.
- RichText is both a widget and a formatting-context bridge. It must not leak
  browser-only document concepts into normal eepp GUI usage.
- Tables can invalidate both intrinsic inline size and final block size. Treating
  them as a normal block child will miss real dependencies.
- Definite-size containers need careful handling: they may need child placement
  updates without needing parent size propagation.
- Performance can regress silently if correctness tests do not also track
  recomputation counts.

## Near-Term Guidance

For current fixes before the full architecture lands:

- Prefer narrowly scoped invalidation at the formatting-context owner.
- Preserve dirty reasons generated during an active layout pass.
- Do not synchronously recompute parents from inside a child layout update.
- Compare exposed geometry before notifying ancestors.
- Keep document-root fixes scoped to document-root invariants.
- Validate every correctness fix with the Markdown benchmark counters.
