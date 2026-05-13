#ifndef EE_GRAPHICS_SYSTEMFONTRESOLVER_HPP
#define EE_GRAPHICS_SYSTEMFONTRESOLVER_HPP

#include <eepp/config.hpp>
#include <eepp/graphics/base.hpp>
#include <eepp/system/singleton.hpp>

#include <string>
#include <vector>

namespace EE { namespace Graphics {

enum class FontWeight : Uint16 {
	Thin = 100,
	ExtraLight = 200,
	Light = 300,
	Normal = 400,
	Medium = 500,
	SemiBold = 600,
	Bold = 700,
	ExtraBold = 800,
	Black = 900
};

enum class FontStretch : Uint8 {
	UltraCondensed = 1,
	ExtraCondensed = 2,
	Condensed = 3,
	SemiCondensed = 4,
	Normal = 5,
	SemiExpanded = 6,
	Expanded = 7,
	ExtraExpanded = 8,
	UltraExpanded = 9
};

enum class GenericFamily : Uint8 {
	None,
	Serif,
	SansSerif,
	Monospace,
	Cursive,
	Fantasy,
	SystemUi,
	Emoji
};

struct FontQuery {
	std::string family;
	FontWeight weight{ FontWeight::Normal };
	FontStretch stretch{ FontStretch::Normal };
	bool italic{ false };
};

struct FontDesc {
	std::string family;
	std::string path;
	Uint32 faceIndex{ 0 };
	FontWeight weight{ FontWeight::Normal };
	FontStretch stretch{ FontStretch::Normal };
	bool italic{ false };
	bool monospace{ false };
};

class EE_API SystemFontResolver {
	SINGLETON_DECLARE_HEADERS( SystemFontResolver )

  public:
	~SystemFontResolver();

	const std::vector<FontDesc>& enumerate();

	std::vector<FontDesc> enumerateFamily( const std::string& family );

	FontDesc resolve( const FontQuery& query );

	FontDesc resolveFromNamesList( const std::string& namesList, FontWeight weight, bool italic );

	FontDesc resolveGeneric( GenericFamily generic, FontWeight weight, bool italic );

	FontDesc getSystemFont() const;

	FontDesc getSystemMonospaceFont() const;

	FontDesc getFallbackForCodepoint( Uint32 codepoint, FontWeight weight, bool italic );

	bool fontContainsCodepoint( const std::string& path, Uint32 codepoint );

	void invalidateCache();

	void ensureFontListPopulated() const;

	static void setEnabled( bool enabled );

	static bool isEnabled();

	static GenericFamily genericFamilyFromName( const std::string& name );

  protected:
	SystemFontResolver();

  private:
	void populateFontList() const;

	void populateGenericFallbacks() const;

	static int scoreMatch( const FontQuery& query, const FontDesc& candidate );

	mutable std::vector<FontDesc> mFontList;
	mutable bool mFontListPopulated{ false };

	static Uint64 makeCacheKey( const std::string& normFamily, FontWeight weight,
								FontStretch stretch, bool italic );

	mutable UnorderedMap<Uint64, FontDesc> mResolveCache;

	mutable UnorderedMap<Uint32, FontDesc> mGenericCache;

	mutable UnorderedMap<Uint32, std::string> mCodepointFallbackCache;

	struct GenericEntry {
		GenericFamily generic;
		FontDesc desc;
	};
	mutable std::vector<GenericEntry> mGenericFallbacks;

	static bool sEnabled;
};

}} // namespace EE::Graphics

#endif
