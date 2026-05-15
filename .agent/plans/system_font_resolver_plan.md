# SystemFontResolver Architecture Plan

## Problem Statement

`eepp` has no mechanism to discover fonts installed on the host operating system. When CSS specifies `font-family: "Helvetica Neue", Arial, sans-serif`, the current resolution chain (`UISceneNode::getFontFromNamesList` → `FontManager::getByName`) can only find fonts that were explicitly loaded via `@font-face` or programmatic `FontTrueType::New`. System font keywords (`caption`, `icon`, `menu`, etc.) are parsed but silently discarded. This makes cross-platform CSS font-family resolution impossible.

**Goal:** Implement a cross-platform `SystemFontResolver` abstraction that maps CSS `font-family` + weight/style properties to physical font file paths (and face indices for `.ttc` files) by querying the host OS font subsystem.

---

## 1. Target Platform APIs

| Platform | EE_PLATFORM | OS API | Headers/Libraries |
|----------|-------------|--------|-------------------|
| Windows | `EE_PLATFORM_WIN` | DirectWrite | `<dwrite.h>`, `dwrite.lib` |
| macOS | `EE_PLATFORM_MACOS` | Core Text | `<CoreText/CoreText.h>`, `-framework CoreText` |
| iOS | `EE_PLATFORM_IOS` | Core Text | same as macOS |
| Linux | `EE_PLATFORM_LINUX` | Fontconfig | `<fontconfig/fontconfig.h>`, `-lfontconfig` |
| FreeBSD | `EE_PLATFORM_BSD` | Fontconfig | same as Linux |
| Android (API 29+) | `EE_PLATFORM_ANDROID` | NDK Font API | `<android/font.h>`, `<android/font_matcher.h>`, `-landroid` |
| Android (legacy) | `EE_PLATFORM_ANDROID` | XML fallback | `/system/etc/fonts.xml` |
| Haiku | `EE_PLATFORM_HAIKU` | Interface Kit / BFont | `<Font.h>`, `-lbe` |

---

## 2. Interface Definition

### 2.1 Header: `include/eepp/graphics/systemfontresolver.hpp`

```cpp
#ifndef EE_GRAPHICS_SYSTEMFONTRESOLVER_HPP
#define EE_GRAPHICS_SYSTEMFONTRESOLVER_HPP

#include <eepp/config.hpp>
#include <eepp/system/singleton.hpp>
#include <eepp/graphics/base.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

namespace EE { namespace Graphics {

/** Weight scale matching CSS font-weight (100-900 + keywords mapped to numeric) */
enum class FontWeight : Uint16 {
    Thin       = 100,
    ExtraLight = 200,
    Light      = 300,
    Normal     = 400,
    Medium     = 500,
    SemiBold   = 600,
    Bold       = 700,
    ExtraBold  = 800,
    Black      = 900
};

/** Categorization of font stretch/width */
enum class FontStretch : Uint8 {
    UltraCondensed = 1,
    ExtraCondensed = 2,
    Condensed      = 3,
    SemiCondensed  = 4,
    Normal         = 5,
    SemiExpanded   = 6,
    Expanded       = 7,
    ExtraExpanded  = 8,
    UltraExpanded  = 9
};

/** Query sent to the OS font subsystem */
struct FontQuery {
    std::string family;       ///< CSS font-family string (e.g. "Arial", "sans-serif")
    FontWeight  weight   { FontWeight::Normal };
    FontStretch stretch  { FontStretch::Normal };
    bool        italic   { false };
};

/** Resolved physical font from the OS */
struct FontDesc {
    std::string path;          ///< Full filesystem path to the font file
    Uint32      faceIndex {0}; ///< Face index for .ttc/.otc TrueType Collections
    FontWeight  weight    { FontWeight::Normal };
    FontStretch stretch   { FontStretch::Normal };
    bool        italic    { false };
    bool        monospace { false };
};

/** Generic font family classification */
enum class GenericFamily : Uint8 {
    Serif,
    SansSerif,
    Monospace,
    Cursive,
    Fantasy,
    SystemUi,     ///< OS default UI font
    Emoji,
    Unknown
};

/** @brief Cross-platform system font discovery and resolution.
 *
 * Queries the host OS font subsystem to enumerate installed fonts and resolve
 * CSS font-family names to physical file paths. All results are cached to
 * avoid repeated OS API calls. This is NOT a Font subclass — it is a
 * pure resolver that returns file paths consumable by FontTrueType.
 */
class EE_API SystemFontResolver {
    SINGLETON_DECLARE_HEADERS( SystemFontResolver )

  public:
    ~SystemFontResolver();

    /** Enumerate all fonts installed on the system. Expensive — call once, results cached. */
    const std::vector<FontDesc>& enumerate();

    /** Enumerate fonts matching a specific family name. */
    std::vector<FontDesc> enumerateFamily( const std::string& family );

    /** Find the best matching font file for a given query.
     *  Searches by family, then falls back to generic family defaults. */
    FontDesc resolve( const FontQuery& query );

    /** Resolve with a font-family CSS list string (e.g. "Helvetica Neue, Arial, sans-serif").
     *  Returns the best match across the entire list. */
    FontDesc resolveFromNamesList( const std::string& namesList, FontWeight weight, bool italic );

    /** Map a generic CSS family keyword to the OS default font path. */
    FontDesc resolveGeneric( GenericFamily generic, FontWeight weight, bool italic );

    /** Get the default system UI font path. */
    FontDesc getSystemFont() const;

    /** Get the default monospace font path. */
    FontDesc getSystemMonospaceFont() const;

    /** Find a fallback font that contains a specific Unicode codepoint. */
    FontDesc getFallbackForCodepoint( Uint32 codepoint, FontWeight weight, bool italic );

    /** Check if a font file contains a specific codepoint. */
    bool fontContainsCodepoint( const std::string& path, Uint32 codepoint,
                                Uint32 faceIndex = 0 );

    /** Clear the internal caches. Call when fonts are installed/uninstalled at runtime. */
    void invalidateCache();

  protected:
    SystemFontResolver();

  private:
    /** Populate the full font list from the OS. Platform-specific. */
    void populateFontList();

    /** Map a generic family to platform-default font paths. */
    void populateGenericFallbacks();

    /** The raw enumerated list from the OS. Populated once, never cleared. */
    std::vector<FontDesc> mFontList;
    bool mFontListPopulated{ false };

    /** Cache: (family_lower, weight, stretch, italic) → FontDesc.
     *  Key uses the lower-cased family name to make lookups case-insensitive. */
    struct CacheKey {
        String::HashType familyHash;
        Uint16 weight;
        Uint8  stretch;
        bool   italic;

        bool operator==( const CacheKey& o ) const {
            return familyHash == o.familyHash && weight == o.weight &&
                   stretch == o.stretch && italic == o.italic;
        }
    };
    struct CacheKeyHasher {
        Uint64 operator()( const CacheKey& k ) const {
            return ( static_cast<Uint64>( k.familyHash ) << 32 ) |
                   ( static_cast<Uint64>( k.weight )     << 16 ) |
                   ( static_cast<Uint64>( k.stretch )    <<  8 ) |
                   ( static_cast<Uint64>( k.italic )           );
        }
    };
    mutable UnorderedMap<CacheKey, FontDesc, CacheKeyHasher> mResolveCache;

    /** Cache: generic + weight + italic → FontDesc */
    mutable UnorderedMap<Uint32, FontDesc> mGenericCache;

    /** Cache for codepoint fallback lookups. */
    mutable UnorderedMap<Uint32, std::string> mCodepointFallbackCache;

    /** Pre-computed generic family mappings (e.g. "sans-serif" → platform default). */
    struct GenericEntry {
        GenericFamily generic;
        FontDesc      desc;
    };
    std::vector<GenericEntry> mGenericFallbacks;

    /** Best-match scoring: given a query and a candidate, compute a fit score (lower is better). */
    static int scoreMatch( const FontQuery& query, const FontDesc& candidate );
};

}} // namespace EE::Graphics

#endif
```

### 2.2 Key Design Decisions

**Singleton Pattern.** Uses the existing `SINGLETON_DECLARE_HEADERS` / `SINGLETON_DECLARE_IMPLEMENTATION` macros (same as `FontManager`). The singleton is created on first `instance()` call.

**Not a Font subclass.** The resolver returns `FontDesc` structs (file paths), not `Font*` objects. This avoids coupling font discovery to the FreeType/GPU resource lifecycle. The consumer (`FontManager` or `UISceneNode`) is responsible for creating `FontTrueType` instances from the paths.

**Case-insensitive matching.** CSS font-family names are case-insensitive. Internal lookups use `String::toLower()` on family names before hashing/querying.

**faceIndex support.** TrueType Collections (`.ttc`, `.otc`) contain multiple faces in a single file. The `faceIndex` field in `FontDesc` tells `FontTrueType` which face to load from the file.

---

## 3. Factory Routing (Platform Selection)

### 3.1 Source File Organization

```
src/eepp/graphics/
├── systemfontresolver.hpp         # Public header
├── systemfontresolver.cpp         # Common implementation (cache logic, matching, generic fallbacks)
├── systemfontresolver_win.cpp     # Windows / DirectWrite
├── systemfontresolver_macos.cpp   # macOS+iOS / Core Text
├── systemfontresolver_linux.cpp   # Linux+BSD / Fontconfig
├── systemfontresolver_android.cpp # Android / NDK Font API + XML fallback
└── systemfontresolver_haiku.cpp   # Haiku / BFont
```

### 3.2 Routing in `systemfontresolver.hpp`

```cpp
#if   EE_PLATFORM == EE_PLATFORM_WIN
    #define EE_SYSTEMFONT_PLATFORM_DEFINED
#elif EE_PLATFORM == EE_PLATFORM_MACOS || EE_PLATFORM == EE_PLATFORM_IOS
    #define EE_SYSTEMFONT_PLATFORM_DEFINED
#elif EE_PLATFORM == EE_PLATFORM_LINUX || EE_PLATFORM == EE_PLATFORM_BSD
    #define EE_SYSTEMFONT_PLATFORM_DEFINED
#elif EE_PLATFORM == EE_PLATFORM_ANDROID
    #define EE_SYSTEMFONT_PLATFORM_DEFINED
#elif EE_PLATFORM == EE_PLATFORM_HAIKU
    #define EE_SYSTEMFONT_PLATFORM_DEFINED
#elif EE_PLATFORM == EE_PLATFORM_EMSCRIPTEN
    // Emscripten: no system font access; use baked-in fonts only
#endif
```

### 3.3 `systemfontresolver.cpp` Common Implementation

Houses:
- `populateFontList()` stubs (where each platform's implementation gets compiled in)
- `resolve()` — the main matching algorithm: lower-case lookup in `mResolveCache`, iterate `mFontList`, compute `scoreMatch()` scores, return best.
- `resolveGeneric()` — look up `mGenericCache`, fall back to `isFamilyInList()` search, else use hard-coded platform defaults.
- `resolveFromNamesList()` — split by comma, try each family name, `resolve()` each.
- `getFallbackForCodepoint()` — iterate font list, call `fontContainsCodepoint()`, cache results.
- `fontContainsCodepoint()` — use FreeType `FT_New_Face(..., faceIndex, ...)`, `FT_Get_Char_Index()`, then `FT_Done_Face()`.
- `scoreMatch()` — weighted scoring: exact family match (0), family substring match (10), generic match (50), weight distance, italic mismatch penalty, stretch distance.

### 3.4 Platform Implementations (in separate .cpp files)

Each platform file implements only `SystemFontResolver::populateFontList()` and `SystemFontResolver::getSystemFont()`. Common code stays in `systemfontresolver.cpp`.

| Platform | populateFontList Implementation |
|----------|-------------------------------|
| Windows | `IDWriteFactory::GetSystemFontCollection()` → enumerate `IDWriteFontFamily` → `IDWriteFont` → `IDWriteLocalizedStrings` for family name → `IDWriteFontFace::GetFiles()` for paths |
| macOS/iOS | `CTFontManagerCopyAvailableFontFamilyNames()` → for each family: `CTFontDescriptorCreateMatchingFontDescriptors()` → `CTFontDescriptorCopyAttribute(kCTFontURLAttribute)` |
| Linux/BSD | `FcInit()` → `FcPatternCreate()` → `FcFontList()` → `FcPatternGetString(FC_FILE)`, `FcPatternGetInteger(FC_INDEX)`, `FcPatternGetInteger(FC_WEIGHT)`, `FcPatternGetInteger(FC_SLANT)` |
| Android (29+) | `AFontMatcher_create()` → `AFontMatcher_setFamilyVariant()` → use `AFontMatcher` to enumerate |
| Android (legacy) | Parse `/system/etc/fonts.xml` for `<family>` → `<font>` elements |
| Haiku | `BFont::get_family_and_style()` + iterate `BPath` for font directories |

---

## 4. Memory & Performance Strategy

### 4.1 Caching Architecture (Two Layers)

```
┌──────────────────────────────────────────────────────────┐
│ Layer 1: Enumerated Font List (mFontList)                │
│ - Populated ONCE per process lifetime                    │
│ - std::vector<FontDesc>, pre-sorted by family            │
│ - Populated lazily on first enumerate()/resolve() call   │
│ - Thread-safe: populated under mutex, read-only after    │
│ - Memory: ~2-5KB per 1000 system fonts (path strings)    │
└──────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 2: Resolution Cache (mResolveCache)                │
│ - Key: (familyHash, weight, stretch, italic)             │
│ - Value: FontDesc                                        │
│ - Built lazily: first resolve(family, weight) calls      │
│ - Hit rate: ~100% after warm-up (font queries are few)   │
│ - Thread-safe: lockless reads after population           │
└──────────────────────────────────────────────────────────┘
                            │
                            ▼
┌──────────────────────────────────────────────────────────┐
│ Layer 2b: Codepoint Fallback Cache                       │
│ - Key: Uint32 codepoint                                  │
│ - Value: font file path string                           │
│ - Built lazily per codepoint                             │
│ - Hit rate: ~100% for commonly-missing CJK/emoji chars   │
└──────────────────────────────────────────────────────────┘
```

### 4.2 Performance Characteristics

| Operation | Cold Path | Hot Path |
|-----------|-----------|----------|
| `enumerate()` | 10-80ms (OS query + path normalization) | <1us (cached) |
| `resolve(family, weight)` | First call per family: enumerates all fonts if not yet done | <2us (hash lookup + array scan of family matches) |
| `resolveFromNamesList("Arial, sans-serif", ...)` | Same as resolve for first family | <10us (try each family in list, first hit returns) |
| `getFallbackForCodepoint(0x65E5)` | ~5ms (FreeType FT_New_Face + FT_Get_Char_Index + FT_Done_Face, may scan multiple fonts) | <1us (cached string + path) |
| `fontContainsCodepoint()` | ~1-3ms per font file (FT_New_Face, FT_Get_Char_Index, FT_Done_Face) | Result not cached separately (only via getFallbackForCodepoint) |

### 4.3 Render-Loop Safety

**Critical invariant:** `SystemFontResolver` is NEVER called during the render loop. It is called only during:
1. Style resolution (when CSS `font-family` is applied to a widget — this happens during layout/initialization, not drawing)
2. `UISceneNode::getFontFromNamesList()` — called during `applyProperty()` which runs on the UI thread but outside the draw cycle
3. Font fallback codepoint lookups — triggered by `FontTrueType::getGlyph()`, which calls `eeASSERT( Engine::isMainThread() )` and runs on the main thread but MUST NOT call the OS. See Section 6 for the async fallback strategy.

**For fallback codepoint resolution**, the OS query is too slow for the render path. Instead:
- `getFallbackForCodepoint()` with cache hit → O(1) return, safe for render
- `getFallbackForCodepoint()` with cache miss → queues a deferred load, returns "not found" to render thread, triggers a callback once resolved

### 4.4 Memory Footprint Strategy

- **`mFontList`** (vector of FontDesc): Each entry is ~80 bytes (path string SSO + metadata). For 1000 fonts: ~80KB. Acceptable.
- **`mResolveCache`** (hash map): Each entry is ~100 bytes. For 100 resolved queries: ~10KB.
- **`mCodepointFallbackCache`**: Each entry is ~80 bytes. For 1000 codepoints: ~80KB.
- **Total budget:** Under 200KB. No heap pressure beyond this.

---

## 5. FreeType Integration (faceIndex + Loading)

### 5.1 Changes to `FontTrueType::loadFromFile`

**Current** (fonttruetype.cpp:350):
```cpp
if ( FT_New_Face( static_cast<FT_Library>( mLibrary ), filename.c_str(), 0, &face ) != 0 ) {
```

**Modified** — add default parameter `faceIndex = 0`:
```cpp
bool loadFromFile( const std::string& filename, Uint32 faceIndex = 0 );
```

Internal change — use the parameter:
```cpp
if ( FT_New_Face( static_cast<FT_Library>( mLibrary ), filename.c_str(),
                  static_cast<FT_Long>( faceIndex ), &face ) != 0 ) {
```

### 5.2 Changes to `FontTrueType::loadFromMemory`

**Current** (fonttruetype.cpp:384-385):
```cpp
if ( FT_New_Memory_Face( static_cast<FT_Library>( mLibrary ),
                         reinterpret_cast<const FT_Byte*>( ptr ),
                         static_cast<FT_Long>( sizeInBytes ), 0, &face ) != 0 ) {
```

**Modified** — add `faceIndex` parameter:
```cpp
bool loadFromMemory( const void* data, std::size_t sizeInBytes, bool copyData = true,
                      Uint32 faceIndex = 0 );
```

### 5.3 Changes to `FontTrueType::loadFromStream`

**Current** (fonttruetype.cpp:427):
```cpp
if ( FT_Open_Face( static_cast<FT_Library>( mLibrary ), &args, 0, &face ) != 0 ) {
```

**Modified** — add `faceIndex` parameter:
```cpp
bool loadFromStream( IOStream& stream, Uint32 faceIndex = 0 );
```

### 5.4 Changes to `FontTrueType::loadFromPack`

Add `faceIndex` passthrough:
```cpp
bool loadFromPack( Pack* pack, std::string filePackPath, Uint32 faceIndex = 0 );
```

### 5.5 Changes to `FontTrueType` Static Factory

Add a new constructor variant:
```cpp
static FontTrueType* New( const std::string& FontName, const std::string& filename,
                           Uint32 faceIndex );
```

### 5.6 New Member: `mFaceIndex`

```cpp
// In fonttruetype.hpp, protected section:
Uint32 mFaceIndex{ 0 };  ///< Face index for .ttc TrueType Collections
```

Stored on construction and passed through to FreeType calls. Since all current callers use `faceIndex = 0`, this is a purely additive, non-breaking change.

---

## 6. Fallback Glyph Routing Strategy

### 6.1 Current Fallback Chain in `FontTrueType::getGlyph`

The existing fallback chain (fonttruetype.cpp:549-619) is:
1. Emoji fallback (color emoji font → emoji font)
2. BoldItalic variant
3. Bold variant
4. Italic variant
5. Own glyph index lookup
6. FontManager fallback fonts (if `mEnableFallbackFont`)

### 6.2 Proposed: Add OS-Level Fallback as Step 7

After step 6 fails (no fallback font from FontManager contains the glyph), add:

```cpp
// Step 7: Ask the OS for a font containing this codepoint
if ( 0 == idx && mEnableFallbackFont && mEnableSystemFallback ) {
    FontDesc fallbackDesc = SystemFontResolver::instance()->getFallbackForCodepoint(
        codePoint, FontWeight::Normal, false );
    if ( !fallbackDesc.path.empty() ) {
        FontTrueType* systemFallback = getOrLoadSystemFallbackFont( fallbackDesc );
        if ( systemFallback && ( idx = systemFallback->getGlyphIndex( codePoint ) ) ) {
            if ( mIsMonospace && mEnableDynamicMonospace ) {
                mIsMonospaceComplete = false;
                mUsingFallback = true;
            }
            return systemFallback->getGlyphByIndex( idx, characterSize, bold, italic,
                                                    outlineThickness, getPage( characterSize ) );
        }
    }
}
```

### 6.3 New Member: `mEnableSystemFallback`

```cpp
// In fonttruetype.hpp:
bool mEnableSystemFallback{ true };

public:
bool isSystemFallbackEnabled() const;
void setEnableSystemFallback( bool enableSystemFallback );
```

### 6.4 System Fallback Font Cache in FontManager

To avoid repeatedly loading the same system font file for every codepoint miss, `FontManager` maintains a small LRU cache of loaded system fallback fonts:

```cpp
// In fontmanager.hpp, protected section:
std::vector<std::unique_ptr<FontTrueType>> mSystemFallbackFonts;
static constexpr Uint32 MAX_SYSTEM_FALLBACK_FONTS = 8;

public:
FontTrueType* getOrLoadSystemFallbackFont( const FontDesc& desc );
```

**Flow:**
1. `FontManager::getOrLoadSystemFallbackFont()` checks if a `FontTrueType*` for the path+faceIndex already exists in `mSystemFallbackFonts`.
2. If yes, returns cached pointer.
3. If no, calls `FontTrueType::New(family, desc.path, desc.faceIndex)`, adds to cache. If cache exceeds `MAX_SYSTEM_FALLBACK_FONTS`, evicts LRU.

### 6.5 Asynchronous Codepoint Fallback Prefetch

For codepoints that the `getFallbackForCodepoint()` cache misses on, we have two options:

**Option A (Recommended): Synchronous with fast path.** On first miss for a codepoint, `getFallbackForCodepoint()` scans the already-enumerated `mFontList` in-memory (no OS calls), checks `fontContainsCodepoint()` with FreeType's `FT_New_Face` + `FT_Get_Char_Index` + `FT_Done_Face`. Since `FT_New_Face` for a single font file is ~0.5-2ms and we may need to scan ~20 fonts to find a match, total cost for first miss is ~10-40ms. This is acceptable because:
- It only happens once per unique missing codepoint (result is cached)
- It happens outside the render loop (during layout/initialization)
- For CJK text, common hanzi/kanji will be in the first few system CJK fonts

**Option B (Future optimization): Deferred.** On cache miss, enqueue a background task, return empty. Font will appear on next frame after load completes.

**We implement Option A for initial release.**

### 6.6 `fontContainsCodepoint()` Implementation Strategy

```cpp
bool SystemFontResolver::fontContainsCodepoint( const std::string& path, Uint32 codepoint,
                                                 Uint32 faceIndex ) {
    // Use FreeType to quickly check without creating a full FontTrueType
    FT_Library ftLib;
    if ( FT_Init_FreeType( &ftLib ) != 0 )
        return false;

    FT_Face face;
    if ( FT_New_Face( ftLib, path.c_str(), static_cast<FT_Long>( faceIndex ), &face ) != 0 ) {
        FT_Done_FreeType( ftLib );
        return false;
    }

    bool hasGlyph = FT_Get_Char_Index( face, codepoint ) != 0;

    FT_Done_Face( face );
    FT_Done_FreeType( ftLib );
    return hasGlyph;
}
```

**Optimization:** Use a per-thread FreeType library to avoid `FT_Init_FreeType`/`FT_Done_FreeType` overhead. Store in thread-local storage.

### 6.7 Integration Point: `UISceneNode::getFontFromNamesList`

Modify the cascade so that after `FontManager::getByName()` fails for all names in the list, `SystemFontResolver::resolveFromNamesList()` is called:

```cpp
Font* UISceneNode::getFontFromNamesList( std::string_view names ) const {
    Font* font = nullptr;
    String::readBySeparatorStoppable(
        names,
        [&font]( std::string_view name ) {
            name = String::trim( name, ' ' );
            name = String::trim( name, '\'' );
            font = FontManager::instance()->getByName( std::string{ name } );
            return font == nullptr;
        },
        ',' );

    // NEW: System font fallback
    if ( !font && SystemFontResolver::existsSingleton() ) {
        FontDesc desc = SystemFontResolver::instance()->resolveFromNamesList(
            std::string{ names }, FontWeight::Normal, false );
        if ( !desc.path.empty() ) {
            // Load via FontFamily or FontTrueType::New
            FontTrueType* ttf = FontTrueType::New( desc.path, desc.path, desc.faceIndex );
            if ( ttf && ttf->loaded() )
                font = ttf;
        }
    }

    return font;
}
```

---

## 7. Changes to CSS Shorthand Parser

### 7.1 `stylesheetspecification.cpp` — System Font Keywords

**Current state** (line 1221-1226): Six CSS system font keywords (`caption`, `icon`, `menu`, `message-box`, `small-caption`, `status-bar`) are detected but return empty.

**Fix:** When a system font keyword is detected, resolve it via `SystemFontResolver` and return the appropriate sub-properties:

```cpp
static const UnorderedMap<std::string, GenericFamily> systemFontToGeneric = {
    { "caption",      GenericFamily::SystemUi },
    { "icon",         GenericFamily::SystemUi },
    { "menu",         GenericFamily::SystemUi },
    { "message-box",  GenericFamily::SystemUi },
    { "small-caption",GenericFamily::SystemUi },
    { "status-bar",   GenericFamily::SystemUi },
};

for ( const auto& sysFont : systemFontToGeneric ) {
    if ( lowerVal == sysFont.first ) {
        if ( SystemFontResolver::existsSingleton() ) {
            FontDesc desc = SystemFontResolver::instance()->resolveGeneric(
                sysFont.second, FontWeight::Normal, false );
            // Build StyleSheetProperty vector with resolved family and metadata
            return {
                StyleSheetProperty( "font-family", desc.path ),
                // ... font-style, font-size from system font metadata
            };
        }
        return {}; // No resolver → no system fonts available
    }
}
```

---

## 8. Generic Font Family Resolution

CSS defines five generic font families: `serif`, `sans-serif`, `monospace`, `cursive`, `fantasy`. Plus `system-ui`, `emoji`, `math`, `fangsong` (CSS Fonts Level 4).

### 8.1 Default Mappings Per Platform

| Generic | Windows | macOS | Linux (Fontconfig) | Android | Haiku |
|---------|---------|-------|---------------------|---------|-------|
| `serif` | "Times New Roman" | "Times" | `FC_SERIF` → "DejaVu Serif" or "Liberation Serif" | "Noto Serif" | "Noto Serif" |
| `sans-serif` | "Arial" | "Helvetica" | `FC_SANS` → "DejaVu Sans" or "Liberation Sans" | "Roboto" | "Noto Sans" |
| `monospace` | "Consolas" | "Menlo" | `FC_MONO` → "DejaVu Sans Mono" or "Liberation Mono" | "Droid Sans Mono" | "Noto Sans Mono" |
| `cursive` | "Comic Sans MS" | "Apple Chancery" | `FC_SANS` (fallback) | "Dancing Script" | "Noto Sans" |
| `fantasy` | "Impact" | "Papyrus" | `FC_SANS` (fallback) | "Noto Sans" | "Noto Sans" |
| `system-ui` | "Segoe UI" | "SF Pro" | Fontconfig default | "Roboto" | "Noto Sans" |
| `emoji` | "Segoe UI Emoji" | "Apple Color Emoji" | "Noto Color Emoji" | "Noto Color Emoji" | "Noto Color Emoji" |

These are resolved in `resolveGeneric()` via `mGenericFallbacks`, populated per-platform in `populateGenericFallbacks()`.

### 8.2 Integration into `resolve()`

When `resolve()` is called with a family name, the matching algorithm:
1. Check `mResolveCache` (hash hit)
2. Search `mFontList` for exact family match
3. If family is a known CSS generic keyword → call `resolveGeneric()`
4. If no match found → try nearest family in `mFontList` (Levenshtein/prefix match)
5. Return empty `FontDesc` if nothing found

---

## 9. Thread Safety

| Structure | Access Pattern | Thread Safety |
|-----------|---------------|---------------|
| `mFontList` | Write-once on populate, read-only after | Mutex-guarded populate, lockless reads after flag set |
| `mResolveCache` | Lazy insert, concurrent reads | Fine-grained mutex per insert; reads are lockless (insertions are atomic from reader POV since we store plain structs) |
| `mCodepointFallbackCache` | Lazy insert, concurrent reads | Same as above |
| `mGenericCache` | Populated at startup, read-only after | No synchronization needed after init |
| `fontContainsCodepoint()` | Creates/destroys FT_Library per call | Must not be called concurrently for same library. Per-call FT_Library init/done is safe. |

**FreeType Library Sharing:** For `fontContainsCodepoint()`, each call creates a fresh `FT_Library`, opens the face, checks, and cleans up. This is threadsafe (FreeType 2.10+ is threadsafe when using separate libraries per thread). For optimization, a per-thread `FT_Library` thread-local can be used.

---

## 10. Implementation Order

### Phase 1: Core Interface + Stub
1. Create `include/eepp/graphics/systemfontresolver.hpp` — full public interface
2. Create `src/eepp/graphics/systemfontresolver.cpp` — common implementation (enumeration caching, matching/scoring, generic fallback lookup, `resolveFromNamesList`, `fontContainsCodepoint`)
3. Create no-op stubs for all five platform files (return empty lists)
4. Integrate into `premake4.lua` / build system
5. Compile and verify

### Phase 2: Platform Implementations
6. Linux/BSD Fontconfig (`systemfontresolver_linux.cpp`) → test
7. Windows DirectWrite (`systemfontresolver_win.cpp`) → test on Windows
8. macOS/iOS Core Text (`systemfontresolver_macos.cpp`) → test on macOS
9. Android NDK API 29+ + XML fallback (`systemfontresolver_android.cpp`)
10. Haiku BFont (`systemfontresolver_haiku.cpp`)

### Phase 3: FreeType faceIndex Integration
11. Add `faceIndex` parameter to all `loadFrom*` methods (default 0, backward compatible)
12. Add `mFaceIndex` member to `FontTrueType`
13. Add `FontTrueType::New(family, path, faceIndex)` factory

### Phase 4: Fallback Glyph Routing
14. Add `mEnableSystemFallback` flag to `FontTrueType`
15. Implement `FontManager::getOrLoadSystemFallbackFont()`
16. Add OS fallback step to `FontTrueType::getGlyph()` and `getGlyphDrawable()`
17. Add codepoint fallback cache + `fontContainsCodepoint()`

### Phase 5: CSS Integration
18. Fix system font keywords in `stylesheetspecification.cpp` font shorthand parser
19. Add generic font family resolution to `getFontFromNamesList` in `uiscenenode.cpp`
20. Wire `SystemFontResolver::resolveFromNamesList()` into `getFontFromNamesList`

### Phase 6: Testing & Validation
21. Write unit tests for `SystemFontResolver` (mockable with known font fixtures)
22. Run existing font rendering tests to verify no regressions
23. Test on all platforms

---

## 11. Risk Assessment

| Risk | Severity | Mitigation |
|------|----------|------------|
| FreeType init/done overhead in `fontContainsCodepoint()` | MEDIUM | Per-thread `FT_Library` cache in thread-local; amortized across codepoint batches |
| Fontconfig being slow on first query | MEDIUM | Lazy populate; `FcFontList()` is the unavoidable first-hit cost (~60ms) |
| Android XML parsing for legacy fallback | LOW | Only on API < 29; happens once, cached |
| TTC face index mismatches | LOW | Each platform API provides the face index natively (Fontconfig: `FC_INDEX`, DirectWrite: `IDWriteFontFace::GetIndex`, Core Text: implicit) |
| Breaking existing loadFromFile callers | NONE | All `faceIndex` parameters default to `0` |
| Memory from cached system fallback fonts | LOW | `MAX_SYSTEM_FALLBACK_FONTS = 8`, capped |
| Emscripten platform | LOW | No system fonts available; resolver returns empty; CSS relies on `@font-face` or bundled fonts only |

---

## 12. Verification

### Unit Tests
- `SystemFontResolver.enumerate` — verify `mFontList` is non-empty on all desktop platforms
- `SystemFontResolver.resolve_ExactFamily` — query "Arial" → verify path exists
- `SystemFontResolver.resolve_GenericFamily` — query "sans-serif" → verify valid path
- `SystemFontResolver.resolveFromNamesList` — "NonExistent, sans-serif" → returns sans-serif path
- `SystemFontResolver.fontContainsCodepoint` — ASCII codepoints in known fonts
- `SystemFontResolver.getFallbackForCodepoint` — CJK codepoint (0x65E5) → valid CJK font path

### Integration Tests
- `FontTrueType.loadFromTTC` — load from a .ttc file with faceIndex > 0
- `UISceneNode.getFontFromNamesList_SystemFallback` — CSS font-family with only system font names
- `FontManager.systemFallbackFonts` — verify LRU eviction behavior

### Existing Test Regression
- All font rendering tests must pass unchanged (`FontRendering.*`, `Text.*`)
- All UI layout tests must pass (system fonts only activate on codepoint miss, which existing tests should not trigger)

---

## 13. Non-Scope (Future Extensions)

- **Font variations (variable fonts with weight/width/slant axes):** The `FontQuery`/`FontDesc` structs can be extended later with axis coordinates.
- **Background font enumeration (async):** For very large font collections (1000+ fonts), `populateFontList()` could be moved to a background thread.
- **Font installation monitoring:** Detecting OS font install/uninstall at runtime and invalidating caches.
- **Emoji font discovery:** Currently hard-coded; could leverage `SystemFontResolver` to auto-detect emoji fonts.
- **PDF/print font embedding:** Using resolved file paths to embed fonts in generated PDFs.
