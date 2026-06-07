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
