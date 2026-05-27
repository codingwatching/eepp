# Font Weight Native Variant Loading Plan

## Goal

Extend `FontTrueType` and `FontFamily` to load and select real font files for
each `FontWeight` value (100–900), replacing the current binary bold/italic
variant model. The CSS `font-weight` property already parses and stores arbitrary
weights in `FontStyleConfig::Weight`; this plan makes the rendering pipeline and
font backend respect that value by picking the correct `.ttf` file instead of
falling back to synthetic emboldening for every weight ≥ 600.

## Current State (Post Phase 1)

Phase 1 (already merged) added:

- `FontWeight Weight` field to `FontStyleConfig`.
- `Text::stringToFontWeight()` / `Text::fontWeightToString()` conversion.
- `FontWeight` parameter on `UISceneNode::getFontFromNamesList` and `reevaluateFontStyle`.
- `setFontWeight()` / `getFontWeight()` on all text-displaying widgets.
- CSS `font` shorthand correctly preserves numeric weight values.
- Weight ≥ 600 sets the `Text::Bold` style bit for backward-compatible rendering.

What is **still missing**:

1. `FontTrueType` only has three variant pointers:
   `mFontBold`, `mFontItalic`, `mFontBoldItalic`.
2. `FontTrueType::getGlyph(bool bold, bool italic, ...)` only consults those
   three pointers; there is no weight-indexed lookup.
3. `FontFamily::loadFromRegular` only searches for Bold, Italic, and
   BoldItalic filename suffixes.
4. `UISceneNode::loadFontStyleVariants` only loads `FontWeight::Bold` and
   `FontWeight::Normal` variants.
5. All `Text` drawing code resolves `bool isBold` from `Style & Text::Bold`
   and passes it to `Font::getGlyph()` – there is no `FontWeight` in the
   rendering hot path.

Consequence: `font-weight: 500` (Medium) sets the config weight but cannot load
a Medium `.ttf` variant.  `font-weight: 800` (ExtraBold) behaves identically to
`font-weight: 700` (Bold) — both use the same bold font or synthetic bold.

## Target State

- `FontTrueType` holds a small weight-indexed map of sibling variants.
- `FontTrueType::getGlyph()` and the whole `Text` rendering pipeline accept
  `FontWeight` (or at minimum resolves the correct `FontTrueType*` before
  reaching `getGlyph`).
- `FontFamily` scans the font directory for all standard weight suffixes.
- `UISceneNode::loadFontStyleVariants` loads all discovered weight variants,
  not just Bold/Italic.
- `SystemFontResolver` results are used when local file discovery fails.
- The `Text::Bold` style flag continues to mean "synthetic bold where no
  variant was found", unchanged – we are adding real variants below it, not
  replacing it.

## Impact Assessment

### Where `bool bold` appears today

Every call path that touches glyph retrieval uses `bool bold, bool italic`:

- `Font::getGlyph()` (virtual, declared in `include/eepp/graphics/font.hpp`).
- `FontTrueType::getGlyph()` and all its private overloads.
- `FontTrueType::getGlyphDrawable()`, `getGlyphByIndex()`.
- `Font::getKerning()` (two overloads).
- `FontTrueType::isBold()`, `isItalic()`, `isBoldItalic()`, `hasBold()`,
  `hasItalic()`, `hasBoldItalic()`, `getBoldFont()`, `getItalicFont()`,
  `getBoldItalicFont()`, `setBoldFont()`, `setItalicFont()`,
  `setBoldItalicFont()`.
- `Text::draw()` and all private rendering helpers in `text.cpp` (roughly
  40 call sites that compute `isBold = (style & Text::Bold) != 0` and pass it
  down).
- `TextLayout` (harfbuzz shaping path).
- `FontBMFont` (bitmap font, likely only needs the `bool` bit).
- `FontSprite` (sprite font, same).
- `GlyphDrawable` creation.

### Where the Bold flag is set

- `FontStyleConfig::Style` bitmask.
- `Text::stringToStyleFlag()` result (used by `StyleSheetProperty::asFontStyle()`).
- `UISceneNode::getFontFromNamesList()` builds the `"Family#bold"` lookup key.
- Every widget's `setFontStyle()` and `getTextDecoration()` methods.
- `Text::styleFlagToString()` serialization.

## Implementation Plan

### Step 1 – Replace variant pointers with a weight map

**Files:** `fonttruetype.hpp`, `fonttruetype.cpp`

Replace:
```cpp
FontTrueType* mFontBold{ nullptr };
FontTrueType* mFontItalic{ nullptr };
FontTrueType* mFontBoldItalic{ nullptr };
```
with:
```cpp
UnorderedMap<Uint32, FontTrueType*> mWeightVariants;
```

The key is `static_cast<Uint32>(weight) | (italic ? (1u << 31) : 0)`. 9 weights
× 2 italic states = 18 possible entries.  `UnorderedMap` gives O(1) lookup on
every glyph retrieval in the hot path.

Add lookup methods:
```cpp
FontTrueType* getVariant( FontWeight weight, bool italic ) const;
void setVariant( FontWeight weight, bool italic, FontTrueType* font );
```

Keep the old `getBoldFont()`, `setBoldFont()`, etc. as deprecation wrappers
that call through to `getVariant(FontWeight::Bold, ...)`.  Over a transition
period call sites can migrate; afterwards the wrappers can be removed.

### Step 2 – Pass FontWeight into FontTrueType::getGlyph

**Files:** `fonttruetype.hpp`/`.cpp`, `text.cpp`, `textlayout.cpp`

Add a `FontWeight` overload on `FontTrueType` (not the base `Font` class,
keeping `Font::getGlyph(bool bold, ...)` unchanged for `FontBMFont`/
`FontSprite`):

```cpp
Glyph getGlyph( Uint32 codePoint, unsigned int characterSize, FontWeight weight,
                bool italic, Float outlineThickness = 0 ) const;
```

Internally this method:
1. Looks up `mWeightVariants` with key `(weight | italicBit)`.
2. If a variant exists, delegates to `variant->getGlyphByIndex(...)`.
3. If no variant exists, calls its own `getGlyphByIndex(..., bold, italic, ...)`
   where `bold = (weight >= FontWeight::SemiBold)` (synthetic emboldening
   fallback).

The call sites in `text.cpp` go from:
```cpp
bool isBold = (style & Text::Bold) != 0;
auto glyph = font->getGlyph(codepoint, fontSize, isBold, isItalic, outline);
```
to:
```cpp
FontWeight weight = mFontStyleConfig.Weight;
auto* ttf = static_cast<FontTrueType*>(font);
auto glyph = ttf->getGlyph(codepoint, fontSize, weight, isItalic, outline);
```

Variant lookup is O(1) hash map inside `getGlyph` — call sites don't repeat it.
Same pattern applies to `getGlyphDrawable()`, `getKerning()`, and the harfbuzz
shaping path in `TextLayout`.

### Step 3 – Expand FontFamily::loadFromRegular

**File:** `fontfamily.cpp`

Add weight suffix patterns to `findType` searches:

| Weight | Suffixes |
|--------|----------|
| Thin (100) | `Thin`, `thin`, `Th`, `Hairline` |
| ExtraLight (200) | `ExtraLight`, `Extra-Light`, `UltraLight`, `Ultra-Light`, `xl`, `XLt` |
| Light (300) | `Light`, `light`, `Lt` |
| Normal (400) | `Regular`, `regular`, `Rg` _(already handled)_ |
| Medium (500) | `Medium`, `medium`, `Md` |
| SemiBold (600) | `SemiBold`, `Semi-Bold`, `DemiBold`, `Demi-Bold`, `Sb`, `Dm` |
| Bold (700) | `Bold`, `bold`, `Bd` _(already handled)_ |
| ExtraBold (800) | `ExtraBold`, `Extra-Bold`, `UltraBold`, `Ultra-Bold`, `xb`, `XBd` |
| Black (900) | `Black`, `Heavy`, `Blk`, `Hv` |

Load all discovered variants via the new `setVariant()` API.

### Step 4 – Update UISceneNode::loadFontStyleVariants

**File:** `uiscenenode.cpp`

Replace the hardcoded `loadVariant(FontWeight::Bold, false)` and
`loadVariant(FontWeight::Normal, true)` calls with a loop that attempts
every supported weight.  For each weight, resolve via
`SystemFontResolver` (since not all fonts ship files for every weight).

### Step 5 – Update font name keys to include weight

**Files:** `uiscenenode.cpp`, `fontmanager.cpp`

Currently the lookup key is `"Family#bold"`.  For a Medium variant the key
should be `"Family#weight-medium"` or `"Family#w500"`.  Decide on a stable
encoding:

Proposal (short, grep-friendly):
```
Family#w400      → weight 400
Family#w700      → weight 700
Family#w700|italic → weight 700 + italic
```

Update `Text::styleFlagToString()` (or add a companion) and all call sites
that build font names.

### Step 6 – Audit call sites that check the Bold flag

Every widget's `getTextDecoration()` masks out Bold/Italic from Style:
```cpp
flags &= ~(Text::Bold | Text::Italic);
```
This continues to work unchanged — `Text::Bold` is still a valid flag.

However, `setFontStyle()` callers that pass `Text::Bold` should ideally also
call `setFontWeight(FontWeight::Bold)` to keep the config consistent.  This can
be done internally in `setFontStyle()`: if the Bold bit is toggled, also update
`Weight` to Bold or Normal.  (Low risk, easy to add.)

### Step 7 – Tests

| Test | Description |
|------|-------------|
| `FontRendering.variantLookupByWeight` | `getVariant(Medium, false)` returns the correct `FontTrueType*` |
| `FontRendering.siblingDiscoveryAllWeights` | `FontFamily::loadFromRegular` discovers Thin through Black |
| `FontRendering.syntheticBoldFallback` | When no variant exists for weight ≥ 600, synthetic bold is used |
| `FontRendering.nonBoldWeightNoSynthetic` | Weight 300/400/500 produces no synthetic emboldening |
| UIHTML font-weight round-trip | CSS `font-weight: 500` → `getFontWeight() == FontWeight::Medium` |

## Risk Analysis

| Risk | Mitigation |
|------|-----------|
| Breaking `Font::getGlyph` API | Use Option A: keep `bool bold`, add weight→variant resolution in `Text` call sites only. |
| Third-party `Font` subclasses | `FontBMFont` and `FontSprite` don't use FreeType; they can ignore the weight map and keep the `bool bold` path unchanged. |
| Font name key format changes | Define the new format early, implement both old and new in `FontManager::getByName()` during transition, drop the old format afterwards. |
| Memory: weight variants double the loaded font count | Stack-weight these as lazy-loaded. `FontTrueType` variants stay `nullptr` until `getVariant()` fires, which triggers a `FontManager` lookup and disk load. |
| `SystemFontResolver` is expensive | Cache results per (family, weight, italic) tuple.  Already partially cached in `mResolveCache`. |

## Migration Sequence

1. Add weight map to `FontTrueType` (backward-compat wrappers around old API).
2. Add `FontWeight` resolution in `Text::draw()` call sites (Option A).
3. Expand `FontFamily::loadFromRegular` suffix discovery.
4. Update `UISceneNode::loadFontStyleVariants`.
5. Add weight-encoded font name keys.
6. Sync `setFontStyle()` to update `Weight` when Bold bit changes.
7. Add tests.
8. (Future) Migrate `Font::getGlyph` signature to Option B if desired.

## Files Affected (estimated)

| File | Change |
|------|--------|
| `include/eepp/graphics/font.hpp` | Add `getVariant()` virtual? Or keep in `FontTrueType` only. |
| `include/eepp/graphics/fonttruetype.hpp` | WeightEntry struct, vector, new methods. |
| `src/eepp/graphics/fonttruetype.cpp` | Implementation. |
| `src/eepp/graphics/fontfamily.cpp` | Extended suffix discovery. |
| `src/eepp/ui/uiscenenode.cpp` | Weight-aware variant loading. |
| `src/eepp/graphics/text.cpp` | ~40 call sites: resolve variant by weight before getGlyph. |
| `src/eepp/graphics/textlayout.cpp` | Same pattern for harfbuzz shaping. |
| `include/eepp/graphics/text.hpp` | `FontWeight` parameter on internal helpers. |
| `src/eepp/ui/uitextview.cpp` | `setFontStyle` → sync Weight. |
| `src/eepp/ui/uicodeeditor.cpp` | Same. |
| `src/eepp/ui/uitooltip.cpp` | Same. |
| `src/eepp/ui/uitextspan.cpp` | Same. |
| `src/eepp/ui/uirichtext.cpp` | Same. |
| `src/eepp/ui/uiconsole.cpp` | Same. |
| `src/eepp/graphics/fontmanager.cpp` | Weight-aware name key in lookup. |
| `src/tests/unit_tests/fontrendering_tests.cpp` | New tests. |
