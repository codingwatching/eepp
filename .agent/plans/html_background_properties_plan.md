# HTML Background Properties — Analysis & Implementation Plan

## 1. HTML CSS Background Property Specification (Full Set)

The CSS Backgrounds and Borders Module Level 3 defines these background properties:

| # | Property | Values | Default | Layered |
|---|----------|--------|---------|---------|
| 1 | `background-color` | `<color>` | `transparent` | No |
| 2 | `background-image` | `none \| <image> [, <image>]*` | `none` | Yes |
| 3 | `background-position` | `<position> [, <position>]*` | `0% 0%` | Yes |
| 4 | `background-size` | `auto \| cover \| contain \| <length-percentage>{1,2}` | `auto` | Yes |
| 5 | `background-repeat` | `<repeat-style> [, <repeat-style>]*` | `repeat` | Yes |
| 6 | `background-origin` | `border-box \| padding-box \| content-box` | `padding-box` | Yes |
| 7 | `background-clip` | `border-box \| padding-box \| content-box` | `border-box` | Yes |
| 8 | `background-attachment` | `scroll \| fixed \| local` | `scroll` | Yes |
| 9 | `background` (shorthand) | `<bg-layer> [, <bg-layer>]* <final-bg-layer>` | — | Yes |

**Repeat-style values:** `repeat-x | repeat-y | [repeat | space | round | no-repeat]{1,2}`

**Position syntax:** `<position> = [left | center | right | <length-percentage>] || [top | center | bottom | <length-percentage>] | [left | center | right | <length-percentage>] [top | center | bottom | <length-percentage>] | [center | [left | right] <length-percentage>?] && [center | [top | bottom] <length-percentage>?]`

**Shorthand syntax:** `[ <bg-layer> , ]* <final-bg-layer>` where each `<bg-layer> = <bg-image> || <bg-position> [ / <bg-size> ]? || <repeat-style> || <attachment> || <box> || <box>` and `<final-bg-layer> = <'background-color'> || <bg-image> || <bg-position> [ / <bg-size> ]? || <repeat-style> || <attachment> || <box> || <box>`

---

## 2. eepp Current Background Implementation

### 2.1 Property Coverage

| HTML Property | eepp Status | eepp Mapping |
|---------------|-------------|--------------|
| `background-color` | **Fully supported** | `PropertyId::BackgroundColor` → `UIBackgroundDrawable::setColor()` |
| `background-image` | **Fully supported** | `PropertyId::BackgroundImage` (indexed) → `LayerDrawable::setDrawable()` |
| `background-position` | **Partially supported** | `PropertyId::BackgroundPositionX/Y` (indexed, split axes) |
| `background-size` | **Partially supported** | `PropertyId::BackgroundSize` (indexed) |
| `background-repeat` | **Partially supported** | `PropertyId::BackgroundRepeat` (indexed, single keyword only) |
| `background-origin` | **NOT IMPLEMENTED** | — |
| `background-clip` | **NOT IMPLEMENTED** | — |
| `background-attachment` | **NOT IMPLEMENTED** | — |
| `background` (shorthand) | **Partially supported** | Missing `size`, origin, clip, attachment, comma-layers |

### 2.2 Architecture

```
UINode::drawBackground()
  └── UINodeDrawable::draw(position, size, alpha)
        ├── [1] mBackgroundColor.draw()          — solid fill (UIBackgroundDrawable)
        ├── [2] Stencil mask (if border-radius + layers)
        └── [3] For each LayerDrawable (reverse order):
              └── LayerDrawable::draw() — image/gradient with repeat, position, size
```

Components:
- **`UIBackgroundDrawable`** (`uinodedrawable(uibackgrounddrawable.hpp/cpp`): solid-color fill with border-radius support. Extends `Graphics::Drawable`.
- **`UINodeDrawable`** (`uinodedrawable.hpp/cpp`): container holding one `UIBackgroundDrawable` (background color) plus a `std::map<int, LayerDrawable*>` (background image layers).
- **`LayerDrawable`** (inner class of `UINodeDrawable`): represents one background image layer. Stores position (x/y strings), size equation, repeat mode, and the drawable itself.

Repeat modes (`UINodeDrawable::Repeat`):
```cpp
enum Repeat { RepeatXY, RepeatX, RepeatY, NoRepeat };
```

Currently mapped from CSS:
- `repeat` → `RepeatXY`
- `repeat-x` → `RepeatX`
- `repeat-y` → `RepeatY`
- `no-repeat` → `NoRepeat`

`space` and `round` are **not** implemented.

---

## 3. Detailed Differences: eepp vs HTML

### 3.1 Property: `background-position` (Highest Priority)

| Aspect | HTML CSS Spec | eepp Current |
|--------|--------------|--------------|
| **Reference box** | `background-origin` controls it (default `padding-box`) | Always widget content area (`getPixelsSize()`) |
| **Percentage formula** | `(ref_box_size - image_size) × percentage` | Same formula — CORRECT |
| **Keyword mapping** | `left`=0%, `center`=50%, `right`=100%, `top`=0%, `bottom`=100% | Same mapping — CORRECT |
| **4-value syntax** | `right 10px top 20px` = 10px from right, 20px from top | Supported via split into posX/posY with 2 tokens each |
| **3-value syntax** | `right 10px top` (axis-swap handling) | Axis-swap in shorthand parser — CORRECT |
| **Axis determination** | First vertical keyword triggers axis swap | Shorthand parser detects `isYAxis(c1) \|\| isXAxis(c2)` — CORRECT |
| **Multi-layer comma-separated** | `10px 20px, 50% 50%` | Supported via comma-split in shorthand parser |
| **Percentage rounding** | Not specified; browsers round to sub-pixel | eepp rounds when input ends with `%` — difference in sub-pixel precision |

**Image atlas use case (the primary motivation):** `background-position` combined with `background-size` and a fixed container size is the standard web technique to render sprites/image atlases. For example:
```css
.icon {
    width: 32px; height: 32px;
    background-image: url(atlas.png);
    background-size: 256px 256px;   /* atlas dimensions */
    background-position: -64px -96px; /* offset into the atlas */
    background-repeat: no-repeat;
}
```
This renders a 32×32 region from the atlas at coordinates (64, 96). This works because:
1. `background-size` scales the image to the specified atlas dimensions
2. `background-position` offsets which part of the scaled image is visible
3. The widget size acts as a viewport/crop window

eepp's implementation already supports this pattern — the math is identical. However, verification and testing of this exact workflow is the top priority.

**Behavioral difference:** In HTML, `background-position: 0 0` places the image at the top-left of the **padding box**. In eepp, it places it at the top-left of the **content area** (widget rect). When borders and padding are non-zero, the HTML image is shifted inward by `border + padding`.

**The real difference** comes from:
1. **Borders**: HTML default `background-origin: padding-box` means image position ignores border width. In eepp, border is an overlay (Inside type) — background is unaffected.
2. **background-origin/content-box**: When origin is `content-box`, HTML uses content area as reference. eepp can't express this distinction.

### 3.2 Property: `background-repeat`

| Aspect | HTML CSS | eepp |
|--------|----------|------|
| **`repeat`** | Tile both axes | `RepeatXY` — CORRECT |
| **`repeat-x`** | Tile horizontally, no-repeat vertically | `RepeatX` — CORRECT |
| **`repeat-y`** | Tile vertically, no-repeat horizontally | `RepeatY` — CORRECT |
| **`no-repeat`** | No tiling | `NoRepeat` — CORRECT |
| **`space`** | Tile, space evenly, no clipping | **NOT IMPLEMENTED** |
| **`round`** | Tile, scale to fit whole number, no clipping | **NOT IMPLEMENTED** |
| **Two-value syntax** | `repeat no-repeat` = X:repeat, Y:no-repeat | **NOT IMPLEMENTED** — only single keyword |

### 3.3 Property: `background-size`

| Aspect | HTML CSS | eepp |
|--------|----------|------|
| **`auto`** | Natural size | Uses `mDrawable->getPixelsSize()` or `mSize` for rectangles — MATCHES |
| **`cover`** | Scale to cover, preserve ratio, may clip | `eemax(scale1, scale2)` — CORRECT |
| **`contain`** | Scale to fit, preserve ratio, no clipping | `eemin(scale1, scale2)`, only scales down — **DIFFERENCE**: HTML `contain` may scale UP if image is smaller than container; eepp only scales DOWN when `Scale1 < 1 \|\| Scale2 < 1`. |
| **Explicit `100px auto`** | Fixed width, proportional height | Supported — CORRECT |
| **Percentage values** | `50% 100%` relative to positioning area | Supported — CORRECT |

### 3.4 Property: `background-color`

Fully supported. No differences. eepp also has `background-tint` as an extension (no HTML equivalent for per-layer tint).

### 3.5 Property: `background-image`

Fully supported including multiple layers, gradients (`linear-gradient`), icons, textures. No HTML-visible differences.

### 3.6 Property: `background-origin` (NOT IMPLEMENTED)

This controls the **positioning reference box** for `background-position`:

| Value | Meaning |
|-------|---------|
| `border-box` | Position relative to outer border edge |
| `padding-box` (default) | Position relative to inner border edge (padding area) |
| `content-box` | Position relative to content area |

In eepp, background position is always relative to the widget's content area (which equals the padding area since padding is inside mSize, but the border reference is impossible).

**Impact:** When `background-origin: content-box` is specified in HTML and the element has padding, the image shifts inward by the padding amount compared to the default (`padding-box`). eepp cannot express this.

### 3.7 Property: `background-clip` (NOT IMPLEMENTED)

This controls the **painting/clipping area** for backgrounds:

| Value | Meaning |
|-------|---------|
| `border-box` (default) | Background paints to border outer edge |
| `padding-box` | Background paints to border inner edge (padding area) |
| `content-box` | Background paints only to content area |

In eepp, `mClipEnabled` controls a clip plane. When set (triggered by any layer having repeat != NoRepeat), it clips to the widget rect (the padding area). When not set, there's no clipping — the background can extend beyond the widget. There's no distinction between border/padding/content clip regions.

**Impact:** In HTML `background-clip: content-box`, the background color/image is clipped to the content area and does NOT appear under padding. eepp always paints the background under the padding area.

### 3.8 Property: `background-attachment` (NOT IMPLEMENTED)

| Value | Meaning |
|-------|---------|
| `scroll` (default) | Background scrolls with the element's containing block |
| `fixed` | Background fixed relative to the viewport |
| `local` | Background scrolls with the element's content (scrollable containers) |

In eepp, all backgrounds behave as if `scroll` — they're positioned relative to the widget and move with it. There's no viewport-relative or content-relative background positioning.

### 3.9 Property: `background` Shorthand

HTML full syntax supports:
```
background: [<bg-image> || <bg-position>[/<bg-size>]? || <repeat-style> || <attachment> || <box> || <box>]# <bg-color>
```

eepp current shorthand:
```cpp
registerShorthand("background",
    {"background-color", "background-image", "background-repeat", "background-position"},
    "background");
```

Missing from eepp shorthand:
1. **`background-size`** via `/` separator — e.g., `background: url(...) center / cover`
2. **`background-origin`** and **`background-clip`** box keywords
3. **`background-attachment`** keywords
4. **Comma-separated multi-layer** — e.g., `background: url(a.png) top, url(b.png) bottom`
5. **Token mapping bug** — the parser maps repeat to `value` (all tokens), not just the repeat token (line 1031: `properties.emplace_back(StyleSheetProperty(propNames[pos], value))` should use `tok`, not `value`)

---

## 4. Implementation Plan

### 4.0 Aspirational Objective & Priority Framing

The end goal of the HTML compatibility layer is to be able to render complex real-world websites such as **reddit.com**. The single most important background feature needed for this is `background-position` — it is pervasively used on reddit for image atlases, sprite sheets, and decorative positioning. Everything else in this plan is subordinate to getting `background-position` and its companion properties (`background-size`, `background-repeat`, and the `background` shorthand) working correctly in the HTML context.

**Priority split:**

| Category | Scope |
|----------|-------|
| **Must-Have** (Phase 1) | `background-position`, `background-repeat` (incl. two-value + space/round), `background-size` (fix contain), `background` shorthand (incl. `/size`, comma-layers). Also: define and register `background-origin`, `background-clip`, `background-attachment` with full **state-passing** plumbing (CSS → `UINode` → `UINodeDrawable` → `LayerDrawable`), but without full rendering implementation. |
| **Cool-to-Have** (Phase 2) | Full rendering implementation for `background-origin`, `background-clip`, `background-attachment`. |

Phase 2 only begins after Phase 1 is fully green — all tests pass, all must-have features implemented.

### 4.1 Step Completion Protocol

For **each step** completed, the implementer must:

1. **Add tests** validating the implementation (golden image tests for rendering changes, or unit/functional tests for parser/state changes, wherever practical).
2. **Build the project** (debug mode) and verify zero compilation errors.
3. **Run the relevant test suite** and confirm all tests pass (existing + newly added).
4. **Git stash** the completed step with a descriptive message:
   ```bash
   git stash push -m "plan: html-background phase1 step<N>: <description>"
   ```
   This ensures we can revert to any previous stable phase at any time. Each stash represents one stable checkpoint.

**Stash naming convention:** `plan: html-background phase<1|2> step<N>: <short description>`

### 4.2 Design Principle: Background Mode

Add a `BackgroundMode` enum to `UINodeDrawable` to distinguish native eepp mode from HTML compatibility mode:

```cpp
enum class BackgroundMode { Native, Html };
```

- **Native mode** (default for existing eepp widgets): preserves current behavior exactly. No new properties take effect.
- **HTML mode** (default for `UIHTMLWidget` descendants): enables full HTML background semantics.

The mode is set during widget construction. `UIHTMLWidget` constructor sets the mode to `Html`.

### 4.3 New CSS Properties (Defined in Phase 1, Full Rendering in Phase 2)

#### `background-origin` (indexed, per-layer)

```cpp
// propertydefinition.hpp
BackgroundOrigin = String::hash("background-origin"),
```
```cpp
// stylesheetspecification.cpp
registerProperty("background-origin", "padding-box").setIndexed();
```

Values: `border-box`, `padding-box`, `content-box`. Store in `LayerDrawable` as enum.

#### `background-clip` (indexed, per-layer)

```cpp
// propertydefinition.hpp
BackgroundClip = String::hash("background-clip"),
```
```cpp
// stylesheetspecification.cpp
registerProperty("background-clip", "border-box").setIndexed();
```

Values: `border-box`, `padding-box`, `content-box`. Store in `LayerDrawable` as enum.

#### `background-attachment` (indexed, per-layer)

```cpp
// propertydefinition.hpp
BackgroundAttachment = String::hash("background-attachment"),
```
```cpp
// stylesheetspecification.cpp
registerProperty("background-attachment", "scroll").setIndexed();
```

Values: `scroll`, `fixed`, `local`. Store in `LayerDrawable` as enum.

> **Phase 1 scope:** These properties are parsed, stored, and the full state pipeline works end-to-end (CSS string → `StyleSheetProperty` → `UIWidget::applyProperty` → `UINode` setter → `UINodeDrawable` → `LayerDrawable` field). Rendering based on these values is deferred to Phase 2.

#### `background-repeat` — extend for two-value + space/round

Expand `UINodeDrawable::Repeat`:
```cpp
enum class RepeatX { NoRepeat, Repeat, Space, Round };
enum class RepeatY { NoRepeat, Repeat, Space, Round };

struct RepeatMode {
    RepeatX x;
    RepeatY y;
};
```

Add two-value repeat parsing: `"repeat no-repeat"` → x=repeat, y=no-repeat. `"space round"` → x=space, y=round.

---

## 5. Phase 1 — Must-Have (Implementation Steps)

### Step 1: Add `BackgroundMode` to `UINodeDrawable`

**Files:** `include/eepp/ui/uinodedrawable.hpp`, `src/eepp/ui/uinodedrawable.cpp`

```cpp
// Header
enum class BackgroundMode { Native, Html };

class EE_API UINodeDrawable : public Drawable {
    // ...
    void setBackgroundMode(BackgroundMode mode);
    BackgroundMode getBackgroundMode() const;
protected:
    BackgroundMode mBackgroundMode{BackgroundMode::Native};
};
```

**Tests:** Unit test that `UINodeDrawable` defaults to `Native`, and `set/getBackgroundMode` round-trips correctly.

**Stash:** `plan: html-background phase1 step1: add BackgroundMode enum to UINodeDrawable`

---

### Step 2: Add origin/clip/attachment enums + fields to `LayerDrawable` (state plumbing only)

**File:** `include/eepp/ui/uinodedrawable.hpp`

```cpp
class LayerDrawable : public Drawable {
    // ...
    enum class Origin { PaddingBox, BorderBox, ContentBox };
    enum class Clip { BorderBox, PaddingBox, ContentBox };
    enum class Attachment { Scroll, Fixed, Local };

    static Origin originFromText(const std::string& text);
    static Clip clipFromText(const std::string& text);
    static Attachment attachmentFromText(const std::string& text);

    void setOrigin(const std::string& origin);
    void setClip(const std::string& clip);
    void setAttachment(const std::string& attachment);
    Origin getOrigin() const;
    Clip getClip() const;
    Attachment getAttachment() const;

    // New members (stored, but NOT consumed by rendering in Phase 1):
    std::string mOriginEq{"padding-box"};
    std::string mClipEq{"border-box"};
    std::string mAttachmentEq{"scroll"};
    Origin mOrigin{Origin::PaddingBox};
    Clip mClip{Clip::BorderBox};
    Attachment mAttachment{Attachment::Scroll};
};
```

**Tests:** Round-trip test: `originFromText("content-box") == Origin::ContentBox`, etc.

**Stash:** `plan: html-background phase1 step2: add origin clip attachment enums and fields to LayerDrawable`

---

### Step 3: Register `background-origin`, `background-clip`, `background-attachment` CSS properties

**File:** `src/eepp/ui/css/stylesheetspecification.cpp`

```cpp
// In registerDefaultProperties(), add after background-size:
registerProperty("background-origin", "padding-box").setIndexed();
registerProperty("background-clip", "border-box").setIndexed();
registerProperty("background-attachment", "scroll").setIndexed();
```

**File:** `include/eepp/ui/css/propertydefinition.hpp`

```cpp
BackgroundOrigin = String::hash("background-origin"),
BackgroundClip = String::hash("background-clip"),
BackgroundAttachment = String::hash("background-attachment"),
```

**Tests:** Verify properties are registered and accessible via `StyleSheetSpecification`.

**Stash:** `plan: html-background phase1 step3: register new background CSS properties`

---

### Step 4: Dispatch new properties in `UIWidget::applyProperty()` + `getPropertyString()`

**File:** `src/eepp/ui/uiwidget.cpp`

```cpp
case PropertyId::BackgroundOrigin:
    setBackgroundOrigin(attribute.value(), attribute.getIndex());
    break;
case PropertyId::BackgroundClip:
    setBackgroundClip(attribute.value(), attribute.getIndex());
    break;
case PropertyId::BackgroundAttachment:
    setBackgroundAttachment(attribute.value(), attribute.getIndex());
    break;
```

Add reverse-lookup in `getPropertyString()` for all three.

**Tests:** Set properties via CSS string, read back via `getPropertyString()`, verify round-trip.

**Stash:** `plan: html-background phase1 step4: dispatch new background properties in applyProperty`

---

### Step 5: Add setters to `UINode` → `UINodeDrawable` → `LayerDrawable`

**Files:** `include/eepp/ui/uinode.hpp`, `src/eepp/ui/uinode.cpp`

```cpp
UINode* setBackgroundOrigin(const std::string& origin, int index = 0);
UINode* setBackgroundClip(const std::string& clip, int index = 0);
UINode* setBackgroundAttachment(const std::string& att, int index = 0);
```

Each delegates to `setBackgroundFillEnabled(true)->setDrawableOrigin(index, origin)` etc.

**Files:** `include/eepp/ui/uinodedrawable.hpp`, `src/eepp/ui/uinodedrawable.cpp`

```cpp
void UINodeDrawable::setDrawableOrigin(int index, const std::string& origin);
void UINodeDrawable::setDrawableClip(int index, const std::string& clip);
void UINodeDrawable::setDrawableAttachment(int index, const std::string& att);
```

Each calls `getLayer(index)->setOrigin(origin)` etc. and invalidates.

**Tests:** Set origin/clip/attachment on a widget, verify the `LayerDrawable` fields hold the correct values.

**Stash:** `plan: html-background phase1 step5: add state pipeline UINode->UINodeDrawable->LayerDrawable`

---

### Step 6: Extend `background-repeat` — two-value syntax + `space`/`round`

**File:** `include/eepp/ui/uinodedrawable.hpp`

```cpp
enum class RepeatX { NoRepeat, Repeat, Space, Round };
enum class RepeatY { NoRepeat, Repeat, Space, Round };
```

**File:** `src/eepp/ui/uinodedrawable.cpp`

Replace current `Repeat` enum usage with `RepeatMode {x, y}`. Update `repeatFromText()` to support:
- Single keyword: `"repeat"` → `{RepeatX::Repeat, RepeatY::Repeat}`
- Two-value: `"repeat no-repeat"` → `{RepeatX::Repeat, RepeatY::NoRepeat}`
- `"space"` / `"round"` keywords (Phase 1: stored as state, rendering in step 8)

**File:** `src/eepp/ui/uinodedrawable.cpp` — `LayerDrawable::draw()`

Replace the `switch(mRepeat)` with independent X and Y repeat handling. Current `NoRepeat`/`RepeatX`/`RepeatY`/`RepeatXY` behavior maps to the same rendering. `Space`/`Round` are stored but not rendered yet (step 8).

**Tests:**
- `repeatFromText("repeat no-repeat")` → `{Repeat, NoRepeat}`
- `repeatFromText("space round")` → `{Space, Round}`
- Golden image test: `background-repeat: no-repeat repeat` rendered as independent axes.

**Stash:** `plan: html-background phase1 step6: two-value repeat + space/round parsing`

---

### Step 7: Fix `background-size: contain` scaling-up behavior

**File:** `src/eepp/ui/uinodedrawable.cpp` — `LayerDrawable::calcDrawableSize()`

HTML `contain` scales the image **both up and down** to fit within the container while preserving aspect ratio. eepp currently only scales **down**:

```cpp
// Current (incorrect):
if ( Scale1 < 1 || Scale2 < 1 ) {
    Scale1 = eemin( Scale1, Scale2 );
    size = Sizef( drawableSize.getWidth() * Scale1, drawableSize.getHeight() * Scale1 );
} else {
    size = drawableSize;
}

// Fixed (HTML-compatible):
Scale1 = eemin( Scale1, Scale2 );
size = Sizef( drawableSize.getWidth() * Scale1, drawableSize.getHeight() * Scale1 );
```

Always apply the minimum scale, even if it means scaling up (when both scales are > 1).

**Tests:** Golden image test with an image smaller than its container using `background-size: contain`. The image should scale UP to fill the smaller dimension.

**Stash:** `plan: html-background phase1 step7: fix background-size contain to scale up`

---

### Step 8: Implement `space` and `round` repeat rendering

**File:** `src/eepp/ui/uinodedrawable.cpp` — `LayerDrawable::draw()`

- **`space`**: Calculate how many whole images fit in the container. Distribute remaining space evenly as gaps between images.
  ```cpp
  int count = eefloor(mSize.getWidth() / mDrawableSize.getWidth());
  if (count < 1) count = 1;
  Float gap = (mSize.getWidth() - count * mDrawableSize.getWidth()) / (count + 1);
  // Draw 'count' images, each offset by 'gap' from the previous
  ```

- **`round`**: Calculate how many images fit, scale so a whole number fits exactly.
  ```cpp
  Float scale = mSize.getWidth() / mDrawableSize.getWidth();
  int count = eemax(1, (int)Math::round(scale));
  Float roundedWidth = mSize.getWidth() / count;
  Float aspectRatio = mDrawableSize.getHeight() / mDrawableSize.getWidth();
  Float roundedHeight = roundedWidth * aspectRatio;
  // Draw 'count' images at the rounded size
  ```

Both must interact correctly with `background-position` (first tile starts from the computed offset, then tiles outward in both directions).

**Tests:**
- Golden image: `background-repeat: space` — verify even gap distribution across 3+ tiles.
- Golden image: `background-repeat: round` — verify tiles are scaled to fit exactly without gaps.

**Stash:** `plan: html-background phase1 step8: space and round repeat rendering`

---

### Step 9: Rewrite `background` shorthand parser

**File:** `src/eepp/ui/css/stylesheetspecification.cpp`

1. Expand shorthand property list:
   ```cpp
   registerShorthand("background",
       {"background-color", "background-image", "background-position", "background-size",
        "background-repeat", "background-attachment", "background-origin", "background-clip"},
       "background");
   ```

2. Rewrite shorthand parser to:
   - **Split by comma** for multi-layer support
   - **Detect `/` separator** for `position / size`
   - Recognize `border-box`/`padding-box`/`content-box` as origin/clip (first = origin, second = clip)
   - Recognize `scroll`/`fixed`/`local` as attachment
   - Include `space` and `round` in repeat keyword list
   - **Fix the token bug**: line 1031 uses `value` (all tokens) instead of `tok` for repeat
   - Generate indexed `StyleSheetProperty` values with comma-separated indices per layer

3. The parser must produce valid output for all CSS3 `background` shorthand forms:
   ```
   background: #f00 url(a.png) top left / 50% auto no-repeat;
   background: url(a.png) center / cover, url(b.png) top left no-repeat, #ccc;
   background: padding-box border-box url(a.png) fixed;
   ```

**Tests:**
- Parser unit test: feed shorthand strings and verify expanded properties.
- Golden image: render an element with `background: url(...) center / cover no-repeat` via shorthand.

**Stash:** `plan: html-background phase1 step9: rewrite background shorthand parser`

---

### Step 10: Enable HTML mode by default for HTML widgets

**File:** `src/eepp/ui/uihtmlwidget.cpp`

In `UIHTMLWidget` constructor or initialization, ensure the background mode is set to `Html`:
```cpp
// In constructor or first background access:
getBackground()->setBackgroundMode(UINodeDrawable::BackgroundMode::Html);
```

**Tests:** Verify that `UIHTMLWidget` instances have `BackgroundMode::Html` on their background drawable by default.

**Stash:** `plan: html-background phase1 step10: enable html mode in UIHTMLWidget`

---

### Step 11: End-to-end image atlas verification

Create a comprehensive golden image test that exercises the image atlas/sprite sheet use case end-to-end:

```html
<!-- Test: image atlas rendering -->
<div style="width:32px; height:32px;
            background-image: url(atlas.png);
            background-size: 256px 256px;
            background-position: -64px -96px;
            background-repeat: no-repeat;">
</div>
```

Also test multiple atlas cells in a grid, and percentage-based atlas positioning.

**Tests:** Golden image comparing the rendered atlas cell against a reference render (e.g., a cropped version of the atlas).

**Stash:** `plan: html-background phase1 step11: image atlas end-to-end verification`

---

### Phase 1 Summary Table

| Step | Description | Complexity | Dependencies |
|------|-------------|------------|--------------|
| 1 | Add `BackgroundMode` enum + field to `UINodeDrawable` | Low | None |
| 2 | Add origin/clip/attachment enums + fields to `LayerDrawable` | Low | Step 1 |
| 3 | Register new CSS properties (`origin`, `clip`, `attachment`) | Low | None |
| 4 | Dispatch new properties in `UIWidget::applyProperty()` | Low | Step 3 |
| 5 | Add setters to `UINode` → `UINodeDrawable` → `LayerDrawable` | Low | Step 2, 3 |
| 6 | Extend `background-repeat` — two-value + space/round parsing | Medium | Step 2 |
| 7 | Fix `background-size: contain` scaling-up | Low | None |
| 8 | Implement `space` and `round` repeat rendering | Medium | Step 6 |
| 9 | Rewrite `background` shorthand parser | High | Step 6, 8 |
| 10 | Enable HTML mode in `UIHTMLWidget` | Low | Step 1 |
| 11 | End-to-end image atlas verification | Low | All above |

---

## 6. Phase 2 — Cool-to-Have (Implementation Steps)

> **Prerequisite:** Phase 1 must be fully complete with all tests passing.

### Step 12: Update `calcPosition()` for `background-origin`

**File:** `src/eepp/ui/uinodedrawable.cpp` — `LayerDrawable::calcPosition()`

When `mBackgroundMode == Html`:
- `Origin::PaddingBox`: reference width = padding box size (current behavior for mSize, since padding is inside)
- `Origin::BorderBox`: reference width = padding box + border widths (border box)
- `Origin::ContentBox`: reference width = padding box - padding (content area)

```cpp
Sizef refSize = mSize;
if (mContainer->getBackgroundMode() == BackgroundMode::Html) {
    switch (mOrigin) {
        case Origin::BorderBox:
            refSize = getBorderBoxSize();
            break;
        case Origin::ContentBox:
            refSize = getContentBoxSize();
            break;
        case Origin::PaddingBox:
        default:
            refSize = mSize;
            break;
    }
}
// Use refSize instead of mSize in percentage/offset calculations
```

**Tests:** Golden image tests for each origin value with non-zero padding/border. Verify image position shifts correctly.

**Stash:** `plan: html-background phase2 step12: background-origin rendering`

---

### Step 13: Implement `background-clip` in the draw pipeline

**File:** `src/eepp/ui/uinodedrawable.cpp` — `UINodeDrawable::draw()`

When `mBackgroundMode == Html`:
- Determine the clip rect for each layer based on `mClip`
- Use clip plane to restrict drawing to the clip rect

```cpp
if (mBackgroundMode == BackgroundMode::Html) {
    for (auto& layer : mGroup) {
        Clip clip = layer.second->getClip();
        Rectf clipRect = getClipRect(clip);
        GLi->getClippingMask()->clipPlaneEnable(clipRect.Left, clipRect.Top,
                                                 clipRect.getWidth(), clipRect.getHeight());
        layer.second->draw(position, size);
        GLi->getClippingMask()->clipPlaneDisable();
    }
}
```

The solid `mBackgroundColor` fill must also respect the clip.

**Tests:** Golden image tests for each clip value. `content-box` should clip background to content area. `padding-box` should show background in padding but not under border.

**Stash:** `plan: html-background phase2 step13: background-clip rendering`

---

### Step 14: Implement `background-attachment`

**File:** `src/eepp/ui/uinodedrawable.cpp`

- **`scroll`** (default): Background scrolls with the element — current behavior. No change needed.
- **`fixed`**: Background is fixed relative to the viewport (root scene node). Position is calculated using viewport coordinates, not element coordinates.
- **`local`**: Background scrolls with the element's content. Only meaningful for scrollable containers.

```cpp
Vector2f getEffectivePosition() const {
    if (mAttachment == Attachment::Fixed) {
        return mContainer->getOwner()->getUISceneNode()->getPosition() + mOffset;
    } else if (mAttachment == Attachment::Local) {
        Vector2f scrollOff = mContainer->getOwner()->getScrollOffset();
        return mPosition + mOffset - scrollOff;
    }
    return mPosition + mOffset;
}
```

**Tests:** Golden image tests for `fixed` and `local` attachment with scrollable containers.

**Stash:** `plan: html-background phase2 step14: background-attachment rendering`

---

### Step 15: Add `UINode::convertLength` support for box references

**File:** `include/eepp/ui/uinode.hpp`, `src/eepp/ui/uinode.cpp`

For `background-origin: border-box` and `content-box`, the percentage reference for `background-position` needs to use the correct box sizes. `getBorderBoxDiff()` and `getPixelsPadding()` provide these.

**Stash:** `plan: html-background phase2 step15: convertLength reference box support`

---

### Phase 2 Summary Table

| Step | Description | Complexity | Dependencies |
|------|-------------|------------|--------------|
| 12 | Update `calcPosition()` for `background-origin` | Medium | Phase 1 complete |
| 13 | Implement `background-clip` in draw pipeline | Medium | Phase 1 complete |
| 14 | Implement `background-attachment` | High | Phase 1 complete |
| 15 | Add `convertLength` support for box references | Low | Step 12 |

---

## 7. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| New properties break existing eepp widgets | LOW | `BackgroundMode::Native` is default; all new behavior gated behind `BackgroundMode::Html` |
| `background-clip` clipping conflicts with existing clip plane | MEDIUM | Per-layer clip planes must be properly stacked |
| `fixed` attachment requires viewport tracking | MEDIUM | `UISceneNode` already tracks viewport; need invalidation on scroll |
| `local` attachment needs scroll offset access | MEDIUM | Only scrollable widgets provide offsets; gate on capability check |
| Shorthand parser rewrite breaks existing stylesheets | HIGH | Backward compat for native mode. New parsing only active under `BackgroundMode::Html` |
| Comma-separated multi-layer in shorthand | MEDIUM | Indexed properties already support multi-layer via comma in individual properties |

---

## 8. Non-Scope

- **`background-color`** — no changes needed. Fully HTML-compatible.
- **`background-image` — gradient syntax** — already supports `linear-gradient`. Radial-gradient and conic-gradient are out of scope.
- **`background-blend-mode`** — CSS3 compositing. Out of scope.
- **`background-repeat-x` / `background-repeat-y`** — CSS longhand properties. Out of scope; two-value `background-repeat` shorthand is sufficient.
- **Non-HTML widgets** — they continue using `BackgroundMode::Native` with unchanged behavior.

---

## 9. Test Impact

### New Tests (Must-Have — Phase 1)

| # | Test | Type |
|---|------|------|
| 1 | `BackgroundMode` round-trip get/set | Unit |
| 2 | `originFromText` / `clipFromText` / `attachmentFromText` parsing | Unit |
| 3 | New CSS properties registered and parsed | Unit |
| 4 | `applyProperty` → `getPropertyString` round-trip for new properties | Unit |
| 5 | `UINode` setter → `LayerDrawable` field round-trip | Unit |
| 6 | `repeatFromText` two-value and space/round | Unit |
| 7 | `background-repeat: no-repeat repeat` (two-value, independent axes) | Golden image |
| 8 | `background-size: contain` scaling UP (image smaller than container) | Golden image |
| 9 | `background-repeat: space` (even gap distribution) | Golden image |
| 10 | `background-repeat: round` (scaled to fit exactly) | Golden image |
| 11 | `background` shorthand: `/size` syntax | Unit + Golden image |
| 12 | `background` shorthand: comma-separated multi-layer | Unit + Golden image |
| 13 | Image atlas: `background-position` + `background-size` + fixed widget | Golden image |
| 14 | UIHTMLWidget defaults to `BackgroundMode::Html` | Unit |

### New Tests (Cool-to-Have — Phase 2)

| # | Test | Type |
|---|------|------|
| 15 | `background-origin: content-box` with padding | Golden image |
| 16 | `background-origin: border-box` | Golden image |
| 17 | `background-clip: content-box` | Golden image |
| 18 | `background-clip: padding-box` | Golden image |
| 19 | `background-attachment: fixed` | Golden image |
| 20 | `background-attachment: local` with scrollable content | Golden image |

### Existing Test Safety

Existing tests pass unchanged because:
- `BackgroundMode::Native` is the default for all existing widgets
- No rendering code paths in Native mode are modified
- New properties default to HTML-standard values that match current eepp behavior (e.g., `background-origin: padding-box` = current positioning behavior)

---

## 10. Summary of Key Differences (background-position Focus)

1. **Reference box awareness**: HTML's `background-position` values (especially percentages) are relative to the positioning area controlled by `background-origin` (default `padding-box`). eepp hardcodes a single reference box (widget size = padding box in eepp's model). *Addressed in Phase 2.*

2. **No `background-origin` support**: eepp cannot express `background-origin: content-box` or `border-box`. *Addressed in Phase 2.*

3. **Percentage rounding**: eepp rounds percentage-calculated positions to integers. HTML browsers use sub-pixel positioning.

4. **The actual position math is the same** — the formula `(container - image) × percentage` is correctly implemented. The keyword mappings are correct. The core image atlas workflow works already; Phase 1 validates and hardens it.
