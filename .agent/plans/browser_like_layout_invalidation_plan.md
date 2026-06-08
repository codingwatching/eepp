# Browser-Like Layout Invalidation Plan

> Status: PROPOSED - source-emitted layout invalidation contract for replacing
> coarse `LayoutAttributeChange` bubbling in the HTML, `UIRichText`, table, and
> `UIWebView` layout path.

## Goal

Make layout invalidation describe the real dependency that became stale at the
point where the mutation happens, then let the receiving parent or formatting
context decide how far that dependency must propagate.

The desired end state is:

- `UIWidget::notifyLayoutAttrChange()` and
  `UIWidget::notifyLayoutAttrChangeParent()` carry an explicit layout
  invalidation reason.
- Parent widgets, layouters, and formatting-context owners consume that reason
  and choose local reflow, child reflow, intrinsic cache invalidation, document
  extent refresh, or no parent propagation.
- Re-entrant notifications generated during an active layout pass are coalesced
  into deferred reasons instead of disappearing or recursively rebuilding the
  same owner.
- Async resources and deferred stylesheets update final HTML geometry without
  relying on resize, hover, or tag-specific retries.
- Large Markdown/RichText documents keep the optimized behavior that avoids
  thousands of duplicate `UIRichText` rebuilds.
- `UIRichText` remains a normal reusable eepp widget. Browser document semantics
  stay in `UIWebView` or an eventual per-document scene.

This is not a plan to attach passive metadata to layouts. Dirty state is useful
only if it is emitted by the invalidation source and consumed by the receiver
that makes propagation decisions.

## Non-Goals

- Do not add reason bits that are never read by layout decisions.
- Do not infer all causes after the fact from `Msg->getSender()`.
- Do not restore eager parent recomputation from inside child notifications.
- Do not dirty every RichText ancestor for every descendant size change.
- Do not fix `html > body` or Hacker News fixtures with generic ancestor retries.
- Do not make WebView document behavior leak into standalone GUI layouts.
- Do not redesign the whole layout engine before adding a testable notification
  contract.

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

The current model has one coarse event:

```cpp
NodeMessage msg( this, NodeMessage::LayoutAttributeChange );
messagePost( &msg );
```

and an equally coarse parent event:

```cpp
NodeMessage msg( this, NodeMessage::LayoutAttributeChange );
mParentNode->messagePost( &msg );
```

That event can mean many incompatible things:

- this widget changed its own used size,
- this widget changed only paint state,
- a child changed normal-flow geometry,
- an inline formatting stream is stale,
- min-content or max-content changed,
- an async image received its natural size,
- a table cell changed row height or column intrinsic width,
- an out-of-flow child needs repositioning,
- a deferred stylesheet changed document-level constraints,
- root/body/viewport document extent must be reconciled.

Receivers currently guess from sender identity, widget type, current scene state,
and local guard flags. That creates two failure modes:

1. Eager parent propagation causes rebuild storms. Large Markdown can trigger
   thousands of redundant `UIRichText` rebuilds while the owner is already
   positioning descendants.
2. Over-coalescing loses a required ancestor or document update. HTML fixtures
   such as `bin/unit_tests/assets/html/hn_empty_thread.html` can remain stale
   during deferred layout until an unrelated resize or event invalidates enough
   of the tree.

The fix is not "more dirty flags". The fix is a contract:

> The mutation source emits what changed. The receiver decides what that means
> for its own layouter and whether the effect crosses its boundary.

## Design Invariant

Every layout invalidation must answer these questions:

1. What dependency became stale?
2. Who is the immediate receiver responsible for handling it?
3. Is this self-local, parent-affecting, formatting-context-affecting,
   intrinsic-size-affecting, out-of-flow, or document-level?
4. If a receiver is already in layout, what deferred reason must survive until
   the active pass finishes?
5. After the receiver reconciles, did anything observable by its parent change?

If an implementation cannot point to where these answers are emitted and
consumed, it is not solving the invalidation problem.

## Core Data Model

Use a compact integer bitset. Names can change, but the first version should be
expressive enough for source emission and receiver policy.

```cpp
enum class LayoutInvalidationReason : Uint32 {
	None = 0,

	// The sender's own used geometry or layout-affecting state changed.
	SelfGeometry = 1u << 0,

	// A normal-flow child or descendant may need layout inside the receiver.
	NormalFlowChild = 1u << 1,

	// An out-of-flow child needs positioning relative to its containing block.
	OutOfFlowChild = 1u << 2,

	// Min-content, max-content, preferred, wrap-content, or auto-size
	// contribution may have changed.
	IntrinsicSize = 1u << 3,

	// The receiver's internal formatting stream is stale.
	FormattingContext = 1u << 4,

	// Style changed. Receivers should use property impact or local context to
	// decide whether this is geometry, intrinsic, formatting, or paint-only.
	Style = 1u << 5,

	// The change can affect document/root/body/scrollable extent.
	DocumentExtent = 1u << 6,

	// The change depends on viewport dimensions or fixed-position placement.
	Viewport = 1u << 7,

	// Paint/hit-test invalidation only. It must not trigger layout by itself.
	PaintOnly = 1u << 8,
};

using LayoutInvalidationFlags = Uint32;
```

The reason bits must travel with the notification. They should not be stored
only on the receiver before an untyped message is sent.

## LayoutInvalidation Payload

`NodeMessage::flags` can carry a simple bitset, but the contract should be named
in UI code so call sites are readable.

```cpp
using LayoutInvalidationFlags = Uint32;

struct LayoutInvalidation {
	LayoutInvalidationFlags reasons{ 0 };
};
```

Minimal first implementation:

```cpp
NodeMessage msg( this, NodeMessage::LayoutAttributeChange, reasons );
```

Then receivers read:

```cpp
auto reasons = static_cast<LayoutInvalidationFlags>( Msg->getFlags() );
if ( reasons == NodeMessage::NoMessage )
	reasons = legacyDefaultReasons( Msg );
```

Because `NodeMessage::NoMessage` is `eeINDEX_NOT_FOUND`, do not treat raw flags
blindly as a valid reason mask. Add a helper:

```cpp
LayoutInvalidationFlags layoutInvalidationFromMessage( const NodeMessage* msg );
```

That helper handles legacy messages and keeps the migration incremental.

## Notification API

The central API must be the source of truth.

```cpp
void UIWidget::notifyLayoutAttrChange();
void UIWidget::notifyLayoutAttrChange( LayoutInvalidationFlags reasons );

void UIWidget::notifyLayoutAttrChangeParent();
void UIWidget::notifyLayoutAttrChangeParent( LayoutInvalidationFlags reasons );
```

Compatibility overloads remain, but they must map to explicit defaults:

```cpp
void UIWidget::notifyLayoutAttrChange() {
	notifyLayoutAttrChange(
		toLayoutInvalidationFlags( LayoutInvalidationReason::SelfGeometry ) |
		toLayoutInvalidationFlags( LayoutInvalidationReason::IntrinsicSize ) );
}

void UIWidget::notifyLayoutAttrChangeParent() {
	notifyLayoutAttrChangeParent(
		toLayoutInvalidationFlags( LayoutInvalidationReason::NormalFlowChild ) |
		toLayoutInvalidationFlags( LayoutInvalidationReason::IntrinsicSize ) );
}
```

These defaults can be conservative during migration, but new and converted call
sites must pass precise reasons.

### Attribute Transactions

Current transactions store boolean flags such as `UI_ATTRIBUTE_CHANGED` and
`UI_PARENT_ATTRIBUTE_CHANGED`. That is not enough once reasons exist.

Add accumulated reason fields on `UIWidget`:

```cpp
LayoutInvalidationFlags mPendingLayoutReasons{ 0 };
LayoutInvalidationFlags mPendingParentLayoutReasons{ 0 };
```

During a transaction:

```cpp
if ( mAttributesTransactionCount != 0 ) {
	mPendingLayoutReasons |= reasons;
	mFlags |= UI_ATTRIBUTE_CHANGED;
	return;
}
```

When the transaction ends, emit one message with the merged reasons. Do not
emit multiple messages or lose the distinction between self and parent reasons.

### Intrinsic Cache Invalidation

`notifyLayoutAttrChange*()` currently calls `invalidateIntrinsicSize()`
unconditionally. Typed reasons should make this explicit:

```cpp
if ( reasons & IntrinsicSize )
	invalidateIntrinsicSize();
```

During migration, conservative defaults may include `IntrinsicSize`. Converted
paint-only call sites must not invalidate intrinsic caches.

## Source Emission Rules

The mutation source should emit the narrowest reason it knows. It does not need
to know every ancestor consequence.

### Widget Geometry

Setters for position, size, min/max size, size policy, margin, padding, border,
display, visibility states that affect layout, and layout gravity emit:

```cpp
notifyLayoutAttrChange( SelfGeometry | IntrinsicSize );
notifyLayoutAttrChangeParent( NormalFlowChild | IntrinsicSize );
```

If the property cannot affect intrinsic contribution, omit `IntrinsicSize`.

### Text And Inline Content

Text content, font metrics, line height, tab size, white-space, word wrapping,
and inline span metrics emit:

```cpp
notifyLayoutAttrChange( FormattingContext | IntrinsicSize );
notifyLayoutAttrChangeParent( NormalFlowChild | FormattingContext | IntrinsicSize );
```

For `UITextSpan` inside `UIRichText`, the nearest RichText owner should consume
the formatting reason. Ancestors should not be dirtied until the owner knows its
exposed result changed.

### Replaced Content

Image natural size, texture replacement, SVG intrinsic size, form control
default size, and other replaced content emit:

```cpp
notifyLayoutAttrChange( SelfGeometry | IntrinsicSize );
notifyLayoutAttrChangeParent( NormalFlowChild | IntrinsicSize | FormattingContext );
```

If the replaced element is out-of-flow, route to the containing block with
`OutOfFlowChild` instead of normal-flow parent sizing.

### CSS Property Application

CSS property application should classify the property and emit corresponding
reasons. Avoid a design where CSS classification only stores bits on the widget
and then sends an untyped message.

```cpp
LayoutInvalidationFlags reasons = classifyLayoutPropertyImpact( property, value );
if ( reasons & PaintOnly )
	invalidateDrawables();
if ( reasons & ~PaintOnly )
	notifyLayoutAttrChange( reasons );
```

Parent notification depends on the property:

- width, height, min/max, margin, display, position, float, and table/flex/grid
  participation usually notify parent.
- font and text metrics notify the formatting-context owner.
- paint-only properties do not notify layout.
- root/body/document special properties route through `UIWebView`.

### Async Stylesheet And Resource Load

Async events must emit through the same reasoned API:

- image load: replaced content intrinsic size.
- stylesheet load: document style change, not generic RichText child change.
- font load: text metrics and intrinsic size for affected formatting contexts.
- viewport resize: `Viewport | DocumentExtent` on the WebView document root.

## Property Impact Classification

Add classification near the CSS property layer. The result should be layout
invalidation flags, not a separate dead enum that nobody consumes.

Examples:

| Property class | Reasons |
|---|---|
| `color`, `background-color`, `cursor`, `pointer-events` | `PaintOnly` |
| `font-*`, `line-height`, `tab-size` | `Style | FormattingContext | IntrinsicSize` |
| `white-space`, `word-break`, `overflow-wrap` | `Style | FormattingContext | IntrinsicSize` |
| `width`, `height`, `min-*`, `max-*` | `Style | SelfGeometry | IntrinsicSize` |
| `margin`, `padding`, `border-width` | `Style | SelfGeometry | IntrinsicSize` |
| `display`, `box-sizing`, `visibility: collapse` | `Style | SelfGeometry | NormalFlowChild | IntrinsicSize` |
| `position`, `top/right/bottom/left`, `z-index` | `Style | SelfGeometry | OutOfFlowChild` depending on value |
| `float`, `clear` | `Style | FormattingContext | IntrinsicSize` |
| table layout properties | `Style | NormalFlowChild | IntrinsicSize` |
| flex/grid container properties | `Style | NormalFlowChild | IntrinsicSize` |
| root/body viewport-affecting properties | `Style | DocumentExtent | Viewport` |

This table should be conservative at first. Correctness is more important than
paint-only precision, but broad layout invalidation must show up in benchmarks.

## Receiver Policy

Receivers consume message reasons and decide what to do locally.

### Generic Layouts

Simple layouts such as linear, stack, relative, and grid can initially treat
layout-affecting reasons conservatively:

```cpp
if ( reasons & layoutAffectingMask )
	tryUpdateLayout();
```

They should still avoid acting on `PaintOnly`.

Later, these layouts can skip parent propagation if the child change cannot
affect their exposed size.

### Formatting-Context Owners

`UIRichText`, table wrappers, flex containers, and grid containers are
boundaries. A child notification should usually dirty the boundary first, not
immediately dirty arbitrary ancestors.

Policy:

- `FormattingContext` marks the owner formatting stream dirty.
- `IntrinsicSize` invalidates owner intrinsic caches.
- `NormalFlowChild` schedules local layout/reflow.
- parent notification is delayed until after owner reconciliation.

### Document Boundary

`UIWebView` or the future document scene consumes:

- `DocumentExtent`,
- `Viewport`,
- document-wide `Style`,
- root/body special sizing.

Document reasons should not be interpreted by ordinary GUI parents unless they
explicitly host a document.

## Owner Reconciliation Contract

Parent propagation should happen after the owner has reconciled its local
layout, not before.

Dangerous current shape:

```cpp
childChanged();
notifyLayoutAttrChangeParent();
tryUpdateLayout();
```

Preferred shape:

```cpp
markOwnerDirty( reasons );
owner layout/reflow runs;
LayoutEffect effect = compareExposedResultBeforeAfter();
if ( effect.affectsParent )
	notifyLayoutAttrChangeParent( effect.toParentReasons() );
```

This is the main browser-like behavior: dirty information moves upward only
when crossing a boundary is necessary.

## Layout Effect Result

Layout passes that own a boundary should report external effects.

```cpp
struct LayoutEffect {
	bool exposedSizeChanged{ false };
	bool intrinsicSizeChanged{ false };
	bool childGeometryChanged{ false };
	bool outOfFlowGeometryChanged{ false };
	bool documentExtentChanged{ false };
};
```

Minimal comparisons:

```cpp
bool exposedSizeChanged =
	eeabs( oldSize.x - newSize.x ) > 0.01f ||
	eeabs( oldSize.y - newSize.y ) > 0.01f;
```

Do not compute expensive intrinsic widths only to fill this struct. Use values
already computed by the layouter, dirty generation counters, or lazy cache
state. The result is useful only if it drives parent notification.

Mapping to parent reasons:

- `exposedSizeChanged` -> `NormalFlowChild | IntrinsicSize`
- `intrinsicSizeChanged` -> `IntrinsicSize`
- `outOfFlowGeometryChanged` -> `OutOfFlowChild`
- `documentExtentChanged` -> `DocumentExtent`
- `childGeometryChanged` without exposed size change usually does not notify
  parent layout, but may need paint/hit-test invalidation.

## Active Layout Guard

`UIRichText` and table layout can emit child messages while they are already
laying out descendants. Re-entering layout from those messages causes rebuild
storms.

The guard must not just return. It must save the reason it absorbed.

```cpp
if ( isUpdatingLayoutTree() && ownsSenderLayoutPass( Msg->getSender() ) ) {
	mDeferredLayoutReasons |= reasons;
	return 1;
}
```

At the end of the active pass:

```cpp
auto deferred = consumeDeferredLayoutReasons();
if ( deferred )
	reconcileDeferredReasons( deferred );
```

Rules:

- Reconcile deferred reasons at the owner boundary.
- Propagate upward only from the owner layout effect.
- Do not schedule an unconditional second RichText rebuild for every deferred
  child message.
- Do not convert deferred child messages into generic ancestor dirtying.

This guard is the difference between correct coalescing and lost invalidations.

## Dirty Queue Contract

`UISceneNode::mDirtyLayouts` can remain a coalescing set of `UILayout*`.
However, the reason data must already be on the queued layout because it came
from the notification message.

Requirements:

- If a queued layout receives additional reasons, merge them before deciding
  that no duplicate queue entry is needed.
- If adding an ancestor removes dirty descendants, preserve descendant reasons
  only if the ancestor is responsible for consuming them.
- If a descendant reason belongs to a boundary below the ancestor, keep the
  descendant queued or transfer the reason to that boundary, not blindly to the
  ancestor.
- Invalidations generated during a flush must remain queued for the next flush
  iteration.
- Keep reusable snapshot storage. Do not rebuild dirty queue allocation state on
  every flush.

Initial conservative implementation can merge reasons upward while tests are
added. The end state should respect boundaries so a document root does not
force unnecessary RichText rebuilds.

## RichText Plan

`UIRichText` is a formatting-context owner. It should not behave like a passive
layout container that forwards every child message.

Add explicit state:

```cpp
LayoutInvalidationFlags mFormattingDirtyReasons{ 0 };
LayoutInvalidationFlags mDeferredLayoutReasons{ 0 };
Uint32 mFormattingGeneration{ 0 };
Uint32 mLastLaidOutFormattingGeneration{ 0 };
```

Receiver behavior:

- `FormattingContext` marks the rich text stream dirty.
- `IntrinsicSize` invalidates RichText intrinsic caches.
- `NormalFlowChild` schedules local reflow/child positioning.
- `OutOfFlowChild` schedules out-of-flow positioning but does not rebuild the
  normal-flow stream unless needed.
- `PaintOnly` does not rebuild layout.

Update behavior:

1. Capture old exposed size and intrinsic generation state.
2. Rebuild the `Graphics::RichText` stream only if formatting is dirty or
   constraints changed.
3. Reflow text with current constraints.
4. Position child widgets/text nodes.
5. Coalesce messages emitted by that positioning into deferred reasons.
6. Compare exposed result.
7. Notify parent only if exposed size or intrinsic contribution changed.

Important cases:

- text content changes,
- inline span metric style changes,
- anchor/span padding and margin changes,
- inline-block custom widget size changes,
- async image natural size changes,
- closed/open details content changing formatting,
- out-of-flow descendants,
- Markdown README benchmark.

The active guard must preserve the reason that was absorbed. Lost reasons are
the stale-layout bug; recursive handling is the performance bug.

## Table Plan

Tables are not simple block containers. A cell change can affect:

- cell intrinsic width,
- column distribution,
- row height,
- table used width,
- table used height,
- containing block height,
- document extent.

`UIHTMLTable` and `TableLayouter` should consume reasoned messages from rows,
cells, and cell contents.

Suggested state:

```cpp
LayoutInvalidationFlags mTableDirtyReasons{ 0 };
bool mColumnWidthsDirty{ true };
bool mRowHeightsDirty{ true };
```

Suggested result:

```cpp
struct TableLayoutEffect {
	bool columnWidthsChanged{ false };
	bool rowHeightsChanged{ false };
	bool intrinsicSizeChanged{ false };
	bool tableSizeChanged{ false };
};
```

Rules:

- Cell `IntrinsicSize` invalidates column intrinsic widths.
- Cell block-size changes invalidate row heights.
- Table parent is notified only after the table pass knows whether table size or
  intrinsic contribution changed.
- Packing/layout-in-progress child messages are deferred into table dirty
  reasons, not blindly retried.
- `DocumentExtent` is emitted only by the document boundary after table size
  affects root/body/scrollable extent.

The Hacker News empty-thread fixture should be covered here because the page
height is table-dominated after deferred CSS applies.

## Out-Of-Flow Plan

Out-of-flow layout has separate dependency routing.

Rules:

- `position: absolute` emits `OutOfFlowChild` to the nearest containing block.
- `position: fixed` emits `OutOfFlowChild | Viewport` to the document/viewport
  boundary.
- Out-of-flow geometry does not normally contribute to parent auto size.
- Existing compatibility behavior that includes some absolute descendants in
  body effective content height must be documented and scoped.
- `UIRichText::rebuildRichText()` should continue excluding out-of-flow
  descendants from the normal inline stream.
- `updateOutOfFlowPosition()` runs after normal-flow constraints are known.

Do not downgrade out-of-flow changes to `NormalFlowChild`; that recreates broad
ancestor invalidation and incorrect auto-size dependencies.

## WebView Document Boundary Plan

Current architecture:

- `UIWebView` is hosted in the normal UI scene.
- `mDocContainer` owns the loaded document widgets.
- `html` and `body` are ordinary UI nodes with browser-specific sizing rules
  layered on top.
- Stylesheets and resources can complete asynchronously while the document is
  loading or while layout is already flushing.

Near-term boundary:

- `UIWebView` owns document-level invalidation entry points.
- Async stylesheet completion emits `Style | DocumentExtent | Viewport` to the
  WebView document root.
- Viewport resize emits `Viewport | DocumentExtent`.
- Root/body sizing reconciliation lives in WebView/document code, not generic
  RichText.
- Standalone `UIRichText` never receives document extent semantics unless it is
  inside a WebView document boundary.

Future boundary:

- Each `UIWebView` owns a document `UISceneNode`.
- Style, layout, resource, paint, and scrollable extent queues are document
  local.
- The host scene sees only the WebView widget and final scrollable size.

The near-term plan should not block on the future scene split, but it should
avoid choices that make that split harder.

## `hn_empty_thread.html` Regression Target

The fixture `bin/unit_tests/assets/html/hn_empty_thread.html` must become a
first-class regression test for this work.

The test should prove:

- deferred stylesheet application does not leave stale table/body/html height,
- the correct geometry is reached during layout flush without requiring a
  synthetic 1px viewport resize,
- document extent is refreshed through WebView/document invalidation, not a
  generic RichText ancestor retry,
- the fix does not increase Markdown RichText rebuild counts beyond the accepted
  baseline.

Suggested assertions:

- `#bigbox > td` height is correct after initial document load and dirty flush.
- inner table height matches expected content height.
- `body` bottom is contained by `html`.
- WebView scrollable document height matches the reconciled root/body extent.
- Repeating the same flush without new changes performs no additional RichText
  rebuild storm.

Use exact fixture expectations from the existing test helpers where possible.
Avoid screenshot-only validation for this bug; geometry assertions are clearer.

## Instrumentation And Budgets

Keep counters close to the layout path:

- dirty layout invalidations,
- dirty layout flush iterations,
- layout tree updates,
- `UIRichText::rebuildRichText()` count,
- table layout pass count,
- deferred reasons absorbed,
- parent notifications emitted,
- document boundary refreshes,
- benchmark wall time for Markdown README.

The counters should distinguish:

- source notifications emitted,
- receiver notifications consumed,
- deferred notifications absorbed,
- parent notifications produced after reconciliation.

This prevents a false success where tests pass by doing far more layout work.

Baseline fixtures:

- `Benchmark.MarkdownReadme`
- `bin/unit_tests/assets/html/hn_empty_thread.html`
- async image replacement in RichText/HTML
- table/body/html height invariant
- `UIHTMLTable.*`
- `UIRichText.*`
- focused WebView deferred CSS tests

## Implementation Phases

Each phase must be validated and stashed before moving on:

```sh
git stash push -m "phase-N topic validated-tests" -- <changed files>
git stash apply stash@{0}
```

Do not `pop` or drop checkpoint stashes. They are rollback snapshots.

### Phase 0: Revert Experimental Dead Data

Start from the current known-good behavior, not from passive reason storage.

Remove or ignore:

- layout reason bits that are not emitted by `notifyLayoutAttrChange*()`,
- layouter result fields that are not consumed,
- receiver-side guesses that only add metadata after an untyped message,
- document extent retries that are not tied to the WebView document boundary.

Validation:

- current focused HTML/RichText/table tests pass,
- `Benchmark.MarkdownReadme` matches the known optimized baseline,
- `hn_empty_thread.html` still reproduces the stale deferred-layout issue.

This phase is intentionally not a fix. It establishes the baseline.

### Phase 1: Add Reasoned Notification API

Add `LayoutInvalidationReason`, `LayoutInvalidationFlags`, and typed overloads
for:

- `UIWidget::notifyLayoutAttrChange( flags )`,
- `UIWidget::notifyLayoutAttrChangeParent( flags )`.

Use `NodeMessage::flags` for the first implementation. Add helpers so callers
do not hand-roll casts or sentinel checks.

Add transaction accumulation:

- `mPendingLayoutReasons`,
- `mPendingParentLayoutReasons`.

Compatibility behavior:

- old no-argument overloads map to conservative explicit defaults,
- old receivers still work,
- no behavior changes expected except better introspection.

Validation:

- full build,
- focused UI layout tests,
- `git diff --check`,
- benchmark counters unchanged.

### Phase 2: Convert High-Signal Emitters

Convert emitters where the source clearly knows the reason:

- `UIWidget::onSizeChange()` -> `SelfGeometry | IntrinsicSize`,
- size policy/min/max/margin/padding/border setters,
- `UITextView` text/font/line metrics,
- `UITextSpan` metric-affecting setters,
- `UIImage`/replaced content natural size changes,
- `UIHTMLWidget::applyProperty()` using CSS property impact classification.

Do not convert every call site blindly. A converted call site should be more
precise than the compatibility default.

Validation:

- no regression in generic UI tests,
- paint-only property changes do not trigger layout invalidation once converted,
- intrinsic cache invalidation still happens for metric changes.

### Phase 3: Convert RichText Receiver And Active Guard

Make `UIRichText::onMessage()` consume `LayoutAttributeChange` reasons.

Implement:

- formatting dirty state,
- deferred reason accumulation while `UIRichText` is actively updating,
- local reflow decision based on `FormattingContext`, `IntrinsicSize`,
  `NormalFlowChild`, and `OutOfFlowChild`,
- parent notification only after exposed size/intrinsic contribution changes.

Important: the active guard must preserve reasons. It must not recursively call
parent layout, and it must not silently return with no effect.

Validation:

- `UIRichText.*`,
- inline-block and anchor/span padding tests,
- async image replacement test,
- `Benchmark.MarkdownReadme` with rebuild counters.

### Phase 4: Convert Table Boundary

Make `UIHTMLTable` and `TableLayouter` consume source-emitted reasons.

Implement:

- cell intrinsic dirtying,
- row height dirtying,
- table result/effect reporting,
- parent notification after table reconciliation,
- deferral during table packing/layout without generic retries.

Validation:

- `UIHTMLTable.*`,
- complex table fixtures,
- table/body/html height tests,
- no Markdown benchmark regression.

### Phase 5: WebView Document Boundary

Route document-wide invalidations through `UIWebView`.

Implement:

- document-root invalidation entry point accepting reason flags,
- async stylesheet completion emits `Style | DocumentExtent | Viewport`,
- viewport resize emits `Viewport | DocumentExtent`,
- root/body/document extent reconciliation consumes document reasons,
- standalone RichText remains unaffected.

Validation:

- `hn_empty_thread.html` geometry is correct after initial deferred layout flush,
- existing deferred CSS WebView test passes,
- document height does not require synthetic resize,
- full `UIHTML.*` focused suite.

### Phase 6: Queue Coalescing With Reasons

Once receivers use reasons, refine `UISceneNode` dirty queue coalescing.

Implement:

- merge additional reasons into already queued layouts,
- preserve descendant reasons when ancestor coalescing is valid,
- do not move boundary-local reasons to ancestors that cannot consume them,
- keep invalidations generated during a flush for the next iteration.

Validation:

- dirty flush iteration counters,
- Markdown benchmark,
- WebView deferred CSS fixture,
- full unit suite.

### Phase 7: Remove Compatibility Defaults Where Safe

After important emitters are converted, audit remaining no-argument
`notifyLayoutAttrChange*()` calls.

For each remaining call:

- convert to precise reasons,
- document why the conservative default is still required, or
- prove it is unreachable/unimportant.

Only then consider making the no-argument overload private, deprecated, or
debug-assert in new code.

Validation:

- full unit suite,
- benchmark suite,
- review of remaining untyped call sites.

## Review Checklist

Before accepting any implementation phase, verify:

- New reason bits are emitted by `notifyLayoutAttrChange*()` or a document
  boundary API, not just written to local state.
- Every new reason consumer has a visible decision tied to that reason.
- Deferred active-layout handling preserves reasons and consumes them later.
- Parent notification happens after owner reconciliation when crossing a
  formatting/table/document boundary.
- Paint-only changes do not invalidate layout.
- `hn_empty_thread.html` improves without a resize workaround.
- `Benchmark.MarkdownReadme` does not regain thousands of duplicate rebuilds.
- Stash checkpoints include only files changed by that phase.

## Success Criteria

The plan is successful when:

- `hn_empty_thread.html` reaches correct table/body/html/WebView document
  geometry during the initial dirty flush.
- Async CSS and async image layout changes propagate through reasoned
  invalidation paths.
- Large Markdown documents keep the optimized RichText rebuild count.
- Receivers make layout decisions from source-emitted reasons, not sender-type
  guesses or passive dirty metadata.
- The old coarse notification path is either migrated or clearly isolated as a
  conservative compatibility fallback.
