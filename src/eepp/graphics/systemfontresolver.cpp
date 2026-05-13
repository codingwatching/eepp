#include <eepp/core/string.hpp>
#include <eepp/graphics/systemfontresolver.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/system/log.hpp>
#include <eepp/window/engine.hpp>

#include <algorithm>
#include <cstring>

#if EE_PLATFORM == EE_PLATFORM_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dwrite.h>
#include <windows.h>
#pragma comment( lib, "dwrite.lib" )
#endif

#if EE_PLATFORM == EE_PLATFORM_MACOS || EE_PLATFORM == EE_PLATFORM_IOS
#include <CoreFoundation/CoreFoundation.h>
#include <CoreText/CoreText.h>
#endif

#if EE_PLATFORM == EE_PLATFORM_LINUX || EE_PLATFORM == EE_PLATFORM_BSD
#include <fontconfig/fontconfig.h>
#endif

#if EE_PLATFORM == EE_PLATFORM_ANDROID
#if __ANDROID_API__ >= 29
#include <android/font.h>
#include <android/font_matcher.h>
#endif
#include <pugixml/pugixml.hpp>
#endif

#if EE_PLATFORM == EE_PLATFORM_HAIKU
#include <Directory.h>
#include <Entry.h>
#include <Font.h>
#include <Path.h>
#include <String.h>
#endif

#include <ft2build.h>
#include FT_FREETYPE_H

namespace EE { namespace Graphics {

SINGLETON_DECLARE_IMPLEMENTATION( SystemFontResolver )

bool SystemFontResolver::sEnabled = false;

SystemFontResolver::SystemFontResolver() {}

SystemFontResolver::~SystemFontResolver() {
	invalidateCache();
}

void SystemFontResolver::setEnabled( bool enabled ) {
	sEnabled = enabled;
}

bool SystemFontResolver::isEnabled() {
	return sEnabled;
}

void SystemFontResolver::invalidateCache() {
	mResolveCache.clear();
	mGenericCache.clear();
	mCodepointFallbackCache.clear();
	mFontList.clear();
	mGenericFallbacks.clear();
	mFontListPopulated = false;
}

static std::string normalizeFamily( const std::string& family ) {
	return String::toLower( String::trim( family ) );
}

static const char* genericFamilyStrings[] = { "",		 "serif",	"sans-serif", "monospace",
											  "cursive", "fantasy", "system-ui",  "emoji" };

GenericFamily SystemFontResolver::genericFamilyFromName( const std::string& name ) {
	std::string lower = String::toLower( String::trim( name ) );
	for ( int i = 1; i <= static_cast<int>( GenericFamily::Emoji ); ++i ) {
		if ( lower == genericFamilyStrings[i] )
			return static_cast<GenericFamily>( i );
	}
	return GenericFamily::None;
}

void SystemFontResolver::ensureFontListPopulated() const {
	if ( !mFontListPopulated ) {
		populateFontList();
		populateGenericFallbacks();
		mFontListPopulated = true;
	}
}

const std::vector<FontDesc>& SystemFontResolver::enumerate() {
	ensureFontListPopulated();
	return mFontList;
}

std::vector<FontDesc> SystemFontResolver::enumerateFamily( const std::string& family ) {
	ensureFontListPopulated();
	std::vector<FontDesc> result;
	std::string normFamily = normalizeFamily( family );
	for ( const auto& desc : mFontList ) {
		if ( normalizeFamily( desc.family ) == normFamily )
			result.push_back( desc );
	}
	return result;
}

Uint64 SystemFontResolver::makeCacheKey( const std::string& normFamily, FontWeight weight,
										 FontStretch stretch, bool italic ) {
	return ( static_cast<Uint64>( String::hash( normFamily ) ) << 32 ) |
		   ( static_cast<Uint64>( static_cast<Uint16>( weight ) ) << 16 ) |
		   ( static_cast<Uint64>( static_cast<Uint8>( stretch ) ) << 8 ) |
		   ( static_cast<Uint64>( italic ) );
}

int SystemFontResolver::scoreMatch( const FontQuery& query, const FontDesc& candidate ) {
	int score = 0;
	bool familyMatched = false;

	std::string qFamily = normalizeFamily( query.family );
	std::string cFamily = normalizeFamily( candidate.family );

	if ( cFamily.empty() )
		return -1;

	if ( qFamily.empty() )
		return -1;

	if ( cFamily == qFamily ) {
		score -= 100;
		familyMatched = true;
	} else if ( String::startsWith( cFamily, qFamily ) ) {
		score -= 60;
		familyMatched = true;
	} else if ( String::startsWith( qFamily, cFamily ) ) {
		score -= 50;
		familyMatched = true;
	} else {
		score += 200;
	}

	if ( candidate.monospace && qFamily == "monospace" )
		score -= 50;

	int weightDiff =
		eeabs( static_cast<int>( candidate.weight ) - static_cast<int>( query.weight ) );
	score += weightDiff / 100;

	int stretchDiff =
		eeabs( static_cast<int>( candidate.stretch ) - static_cast<int>( query.stretch ) );
	score += stretchDiff * 10;

	if ( candidate.italic == query.italic )
		score -= 30;
	else
		score += 40;

	return familyMatched ? score : -1;
}

FontDesc SystemFontResolver::resolve( const FontQuery& query ) {
	if ( query.family.empty() )
		return FontDesc();

	ensureFontListPopulated();

	std::string normFamily = normalizeFamily( query.family );

	Uint64 key = makeCacheKey( normFamily, query.weight, query.stretch, query.italic );

	auto cacheIt = mResolveCache.find( key );
	if ( cacheIt != mResolveCache.end() )
		return cacheIt->second;

	GenericFamily generic = genericFamilyFromName( query.family );
	if ( generic != GenericFamily::None ) {
		FontDesc result = resolveGeneric( generic, query.weight, query.italic );
		mResolveCache[key] = result;
		return result;
	}

	FontDesc best;
	int bestScore = 100000;

	for ( const auto& desc : mFontList ) {
		int score = scoreMatch( query, desc );
		if ( score != -1 && score < bestScore ) {
			bestScore = score;
			best = desc;
		}
	}

	if ( bestScore == 100000 )
		mResolveCache[key] = FontDesc();
	else
		mResolveCache[key] = best;
	return mResolveCache[key];
}

FontDesc SystemFontResolver::resolveFromNamesList( const std::string& namesList, FontWeight weight,
												   bool italic ) {
	ensureFontListPopulated();

	FontDesc result;
		String::readBySeparatorStoppable(
		namesList,
		[&result, weight, italic]( std::string_view name ) {
			name = String::trim( name, ' ' );
			name = String::trim( name, '\'' );
			name = String::trim( name, '"' );
			FontQuery query;
			query.family = std::string{ name };
			query.weight = weight;
			query.italic = italic;
			result = SystemFontResolver::instance()->resolve( query );
			return !result.path.empty();
		},
		',' );

	return result;
}

FontDesc SystemFontResolver::resolveGeneric( GenericFamily generic, FontWeight weight,
											 bool italic ) {
	ensureFontListPopulated();

	Uint32 cacheKey = ( static_cast<Uint32>( generic ) << 16 ) |
					  ( static_cast<Uint32>( weight ) << 1 ) | ( italic ? 1 : 0 );

	auto it = mGenericCache.find( cacheKey );
	if ( it != mGenericCache.end() )
		return it->second;

	FontDesc result;

	for ( const auto& entry : mGenericFallbacks ) {
		if ( entry.generic == generic ) {
			int entryWeight = static_cast<int>( entry.desc.weight );
			int queryWeight = static_cast<int>( weight );
			int weightDiff = eeabs( entryWeight - queryWeight );
			bool styleMatch = entry.desc.italic == italic;
			int bestWeightDiff = result.path.empty()
									 ? 100000
									 : eeabs( static_cast<int>( result.weight ) -
											  static_cast<int>( weight ) );
			bool bestStyleMatch =
				result.path.empty() ? false : ( result.italic == italic );

			if ( styleMatch && !bestStyleMatch ) {
				result = entry.desc;
			} else if ( styleMatch == bestStyleMatch &&
						( result.path.empty() || weightDiff < bestWeightDiff ) ) {
				result = entry.desc;
			}
		}
	}

	mGenericCache[cacheKey] = result;
	return result;
}

FontDesc SystemFontResolver::getSystemFont() const {
	if ( !mGenericFallbacks.empty() ) {
		for ( const auto& entry : mGenericFallbacks ) {
			if ( entry.generic == GenericFamily::SystemUi )
				return entry.desc;
		}
	}
	return FontDesc();
}

FontDesc SystemFontResolver::getSystemMonospaceFont() const {
	if ( !mGenericFallbacks.empty() ) {
		for ( const auto& entry : mGenericFallbacks ) {
			if ( entry.generic == GenericFamily::Monospace )
				return entry.desc;
		}
	}
	return FontDesc();
}

FontDesc SystemFontResolver::getFallbackForCodepoint( Uint32 codepoint, FontWeight weight,
													  bool italic ) {
	ensureFontListPopulated();

	Uint32 cacheKey = codepoint;
	auto it = mCodepointFallbackCache.find( cacheKey );
	if ( it != mCodepointFallbackCache.end() ) {
		const std::string& path = it->second;
		if ( path.empty() )
			return FontDesc();
		for ( const auto& desc : mFontList ) {
			if ( desc.path == path ) {
				FontDesc result = desc;
				result.weight = weight;
				result.italic = italic;
				return result;
			}
		}
	}

	for ( const auto& desc : mFontList ) {
		if ( fontContainsCodepoint( desc.path, codepoint ) ) {
			mCodepointFallbackCache[cacheKey] = desc.path;
			FontDesc result = desc;
			result.weight = weight;
			result.italic = italic;
			return result;
		}
	}

	mCodepointFallbackCache[cacheKey] = "";
	return FontDesc();
}

bool SystemFontResolver::fontContainsCodepoint( const std::string& path, Uint32 codepoint ) {
	if ( !FileSystem::fileExists( path ) )
		return false;

	FT_Library ftLib;
	if ( FT_Init_FreeType( &ftLib ) != 0 )
		return false;

	FT_Face face;
	if ( FT_New_Face( ftLib, path.c_str(), 0, &face ) != 0 ) {
		FT_Done_FreeType( ftLib );
		return false;
	}

	bool hasGlyph = FT_Get_Char_Index( face, codepoint ) != 0;

	FT_Done_Face( face );
	FT_Done_FreeType( ftLib );
	return hasGlyph;
}

void SystemFontResolver::populateGenericFallbacks() const {
	mGenericFallbacks.clear();

	struct Mapping {
		GenericFamily generic;
		const char* family;
	};

	static const Mapping mappings[] = {
		{ GenericFamily::Serif, "times new roman" },
		{ GenericFamily::Serif, "times" },
		{ GenericFamily::Serif, "dejavu serif" },
		{ GenericFamily::Serif, "liberation serif" },
		{ GenericFamily::Serif, "noto serif" },
		{ GenericFamily::Serif, "serif" },
		{ GenericFamily::SansSerif, "arial" },
		{ GenericFamily::SansSerif, "helvetica" },
		{ GenericFamily::SansSerif, "dejavu sans" },
		{ GenericFamily::SansSerif, "liberation sans" },
		{ GenericFamily::SansSerif, "roboto" },
		{ GenericFamily::SansSerif, "noto sans" },
		{ GenericFamily::SansSerif, "sans-serif" },
		{ GenericFamily::Monospace, "consolas" },
		{ GenericFamily::Monospace, "menlo" },
		{ GenericFamily::Monospace, "dejavu sans mono" },
		{ GenericFamily::Monospace, "liberation mono" },
		{ GenericFamily::Monospace, "droid sans mono" },
		{ GenericFamily::Monospace, "noto sans mono" },
		{ GenericFamily::Monospace, "monospace" },
		{ GenericFamily::Cursive, "comic sans ms" },
		{ GenericFamily::Cursive, "apple chancery" },
		{ GenericFamily::Cursive, "cursive" },
		{ GenericFamily::Fantasy, "impact" },
		{ GenericFamily::Fantasy, "papyrus" },
		{ GenericFamily::Fantasy, "fantasy" },
		{ GenericFamily::SystemUi, "segoe ui" },
		{ GenericFamily::SystemUi, "sf pro" },
		{ GenericFamily::SystemUi, "dejavu sans" },
		{ GenericFamily::SystemUi, "system-ui" },
		{ GenericFamily::Emoji, "segoe ui emoji" },
		{ GenericFamily::Emoji, "apple color emoji" },
		{ GenericFamily::Emoji, "noto color emoji" },
		{ GenericFamily::Emoji, "emoji" },
	};

	for ( const auto& mapping : mappings ) {
		std::string searchFamily = normalizeFamily( mapping.family );
		for ( const auto& desc : mFontList ) {
			std::string descFamily = normalizeFamily( desc.family );
			if ( descFamily == searchFamily ) {
				mGenericFallbacks.push_back( { mapping.generic, desc } );
			}
		}
	}
}

// =====================================================================
// Platform: Windows (DirectWrite)
// =====================================================================
#if EE_PLATFORM == EE_PLATFORM_WIN

static std::string wideToUtf8( const WCHAR* wstr ) {
	if ( !wstr || !*wstr )
		return {};
	int len = WideCharToMultiByte( CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr );
	if ( len <= 0 )
		return {};
	std::string result( len - 1, '\0' );
	WideCharToMultiByte( CP_UTF8, 0, wstr, -1, &result[0], len, nullptr, nullptr );
	return result;
}

static IDWriteFactory* getDWriteFactory() {
	static IDWriteFactory* sFactory = nullptr;
	if ( !sFactory ) {
		HRESULT hr = DWriteCreateFactory( DWRITE_FACTORY_TYPE_SHARED, __uuidof( IDWriteFactory ),
										  reinterpret_cast<IUnknown**>( &sFactory ) );
		if ( FAILED( hr ) )
			return nullptr;
	}
	return sFactory;
}

void SystemFontResolver::populateFontList() const {
	IDWriteFactory* factory = getDWriteFactory();
	if ( !factory )
		return;

	IDWriteFontCollection* collection = nullptr;
	HRESULT hr = factory->GetSystemFontCollection( &collection, FALSE );
	if ( FAILED( hr ) || !collection )
		return;

	UINT32 familyCount = collection->GetFontFamilyCount();

	for ( UINT32 i = 0; i < familyCount; ++i ) {
		IDWriteFontFamily* fontFamily = nullptr;
		hr = collection->GetFontFamily( i, &fontFamily );
		if ( FAILED( hr ) || !fontFamily )
			continue;

		IDWriteLocalizedStrings* familyNames = nullptr;
		hr = fontFamily->GetFamilyNames( &familyNames );
		if ( FAILED( hr ) || !familyNames ) {
			fontFamily->Release();
			continue;
		}

		UINT32 nameLen = 0;
		hr = familyNames->GetStringLength( 0, &nameLen );
		if ( FAILED( hr ) ) {
			familyNames->Release();
			fontFamily->Release();
			continue;
		}

		std::wstring familyNameW( nameLen + 1, L'\0' );
		familyNames->GetString( 0, &familyNameW[0], nameLen + 1 );
		familyNames->Release();

		std::string familyName = wideToUtf8( familyNameW.c_str() );
		if ( familyName.empty() ) {
			fontFamily->Release();
			continue;
		}

		UINT32 fontCount = fontFamily->GetFontCount();
		for ( UINT32 j = 0; j < fontCount; ++j ) {
			IDWriteFont* dwriteFont = nullptr;
			hr = fontFamily->GetFont( j, &dwriteFont );
			if ( FAILED( hr ) || !dwriteFont )
				continue;

			BOOL isSymbol = dwriteFont->IsSymbolFont();
			DWRITE_FONT_WEIGHT dwWeight = dwriteFont->GetWeight();
			DWRITE_FONT_STRETCH dwStretch = dwriteFont->GetStretch();
			DWRITE_FONT_STYLE dwStyle = dwriteFont->GetStyle();

			IDWriteFontFace* fontFace = nullptr;
			hr = dwriteFont->CreateFontFace( &fontFace );
			if ( FAILED( hr ) || !fontFace ) {
				dwriteFont->Release();
				continue;
			}

			UINT32 fileCount = 0;
			hr = fontFace->GetFiles( &fileCount, nullptr );
			if ( FAILED( hr ) || fileCount == 0 ) {
				fontFace->Release();
				dwriteFont->Release();
				continue;
			}

			std::vector<IDWriteFontFile*> files( fileCount );
			hr = fontFace->GetFiles( &fileCount, files.data() );
			if ( FAILED( hr ) ) {
				fontFace->Release();
				dwriteFont->Release();
				continue;
			}

			for ( UINT32 k = 0; k < fileCount; ++k ) {
				IDWriteFontFile* fontFile = files[k];
				IDWriteFontFileLoader* loader = nullptr;
				hr = fontFile->GetLoader( &loader );
				if ( FAILED( hr ) || !loader ) {
					fontFile->Release();
					continue;
				}

				IDWriteLocalFontFileLoader* localLoader = nullptr;
				hr = loader->QueryInterface( __uuidof( IDWriteLocalFontFileLoader ),
											 reinterpret_cast<void**>( &localLoader ) );
				loader->Release();

				if ( FAILED( hr ) || !localLoader ) {
					fontFile->Release();
					continue;
				}

				const void* refKey = nullptr;
				UINT32 refKeySize = 0;
				hr = fontFile->GetReferenceKey( &refKey, &refKeySize );
				if ( FAILED( hr ) ) {
					localLoader->Release();
					fontFile->Release();
					continue;
				}

				UINT32 pathLen = 0;
				hr = localLoader->GetFilePathLengthFromKey( refKey, refKeySize, &pathLen );
				if ( FAILED( hr ) ) {
					localLoader->Release();
					fontFile->Release();
					continue;
				}

				std::wstring filePathW( pathLen + 1, L'\0' );
				hr = localLoader->GetFilePathFromKey( refKey, refKeySize, &filePathW[0],
													  pathLen + 1 );
				localLoader->Release();
				fontFile->Release();

				if ( FAILED( hr ) )
					continue;

				std::string filePath = wideToUtf8( filePathW.c_str() );
				if ( filePath.empty() )
					continue;

				if ( isSymbol )
					continue;

				FontDesc desc;
				desc.family = familyName;
				desc.path = filePath;
				desc.faceIndex = j;
				desc.weight = static_cast<FontWeight>( static_cast<Uint16>( dwWeight ) );
				desc.stretch = static_cast<FontStretch>( static_cast<Uint8>( dwStretch ) );
				desc.italic =
					( dwStyle == DWRITE_FONT_STYLE_ITALIC || dwStyle == DWRITE_FONT_STYLE_OBLIQUE );

				DWRITE_PANOSE panose;
				dwriteFont->GetPanose( &panose );
				desc.monospace = ( panose.familyKind == 2 && panose.text.panoseProportion == 9 );

				mFontList.push_back( desc );
			}

			fontFace->Release();
			dwriteFont->Release();
		}

		fontFamily->Release();
	}

	collection->Release();
}

// =====================================================================
// Platform: macOS / iOS (Core Text)
// =====================================================================
#elif EE_PLATFORM == EE_PLATFORM_MACOS || EE_PLATFORM == EE_PLATFORM_IOS

static std::string cfStringToStd( CFStringRef str ) {
	if ( !str )
		return {};
	CFIndex len = CFStringGetLength( str );
	CFIndex maxLen = CFStringGetMaximumSizeForEncoding( len, kCFStringEncodingUTF8 );
	std::string result( maxLen, '\0' );
	CFStringGetCString( str, &result[0], maxLen, kCFStringEncodingUTF8 );
	result.resize( strlen( result.c_str() ) );
	return result;
}

static FontWeight ctWeightToFontWeight( CGFloat weight ) {
	if ( weight <= -0.8 )
		return FontWeight::Thin;
	if ( weight <= -0.6 )
		return FontWeight::ExtraLight;
	if ( weight <= -0.4 )
		return FontWeight::Light;
	if ( weight <= 0.0 )
		return FontWeight::Normal;
	if ( weight <= 0.2 )
		return FontWeight::Medium;
	if ( weight <= 0.3 )
		return FontWeight::SemiBold;
	if ( weight <= 0.4 )
		return FontWeight::Bold;
	if ( weight <= 0.6 )
		return FontWeight::ExtraBold;
	return FontWeight::Black;
}

static FontStretch ctWidthToFontStretch( CGFloat width ) {
	if ( width <= -0.8 )
		return FontStretch::UltraCondensed;
	if ( width <= -0.6 )
		return FontStretch::ExtraCondensed;
	if ( width <= -0.4 )
		return FontStretch::Condensed;
	if ( width <= -0.2 )
		return FontStretch::SemiCondensed;
	if ( width <= 0.1 )
		return FontStretch::Normal;
	if ( width <= 0.4 )
		return FontStretch::SemiExpanded;
	if ( width <= 0.6 )
		return FontStretch::Expanded;
	if ( width <= 0.8 )
		return FontStretch::ExtraExpanded;
	return FontStretch::UltraExpanded;
}

void SystemFontResolver::populateFontList() const {
	CFArrayRef descriptors = CTFontManagerCopyAvailableFontFamilyNames();
	if ( !descriptors )
		return;

	CFIndex count = CFArrayGetCount( descriptors );

	for ( CFIndex i = 0; i < count; ++i ) {
		CFStringRef familyNameRef = (CFStringRef)CFArrayGetValueAtIndex( descriptors, i );
		std::string familyName = cfStringToStd( familyNameRef );
		if ( familyName.empty() )
			continue;

		CFMutableDictionaryRef queryDict =
			CFDictionaryCreateMutable( kCFAllocatorDefault, 1, &kCFTypeDictionaryKeyCallBacks,
									   &kCFTypeDictionaryValueCallBacks );
		CFDictionaryAddValue( queryDict, kCTFontFamilyNameAttribute, familyNameRef );

		CTFontDescriptorRef familyDesc = CTFontDescriptorCreateWithAttributes( queryDict );
		CFRelease( queryDict );

		CFSetRef mandatoryAttrs = CFSetCreate(
			kCFAllocatorDefault, (const void**)&kCTFontURLAttribute, 1, &kCFTypeSetCallBacks );

		CFArrayRef matchingDescs =
			CTFontDescriptorCreateMatchingFontDescriptors( familyDesc, mandatoryAttrs );
		CFRelease( mandatoryAttrs );
		CFRelease( familyDesc );

		if ( !matchingDescs ) {
			CFRelease( descriptors );
			return;
		}

		CFIndex matchCount = CFArrayGetCount( matchingDescs );

		for ( CFIndex j = 0; j < matchCount; ++j ) {
			CTFontDescriptorRef desc =
				(CTFontDescriptorRef)CFArrayGetValueAtIndex( matchingDescs, j );

			CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute( desc, kCTFontURLAttribute );
			if ( !url )
				continue;

			CFStringRef pathRef = CFURLCopyFileSystemPath( url, kCFURLPOSIXPathStyle );
			CFRelease( url );

			std::string fontPath = cfStringToStd( pathRef );
			CFRelease( pathRef );

			if ( fontPath.empty() )
				continue;

			CFNumberRef weightNum =
				(CFNumberRef)CTFontDescriptorCopyAttribute( desc, kCTFontWeightTrait );
			CGFloat weightVal = 0.0;
			if ( weightNum ) {
				CFNumberGetValue( weightNum, kCFNumberCGFloatType, &weightVal );
				CFRelease( weightNum );
			}

			CFNumberRef widthNum =
				(CFNumberRef)CTFontDescriptorCopyAttribute( desc, kCTFontWidthTrait );
			CGFloat widthVal = 0.0;
			if ( widthNum ) {
				CFNumberGetValue( widthNum, kCFNumberCGFloatType, &widthVal );
				CFRelease( widthNum );
			}

			CFNumberRef slantNum =
				(CFNumberRef)CTFontDescriptorCopyAttribute( desc, kCTFontSlantTrait );
			CGFloat slantVal = 0.0;
			if ( slantNum ) {
				CFNumberGetValue( slantNum, kCFNumberCGFloatType, &slantVal );
				CFRelease( slantNum );
			}

			CFNumberRef monoNum =
				(CFNumberRef)CTFontDescriptorCopyAttribute( desc, kCTFontMonoSpaceTrait );
			int isMono = 0;
			if ( monoNum ) {
				CFNumberGetValue( monoNum, kCFNumberIntType, &isMono );
				CFRelease( monoNum );
			}

			FontDesc fontDesc;
			fontDesc.family = familyName;
			fontDesc.path = fontPath;
			fontDesc.faceIndex = static_cast<Uint32>( j );
			fontDesc.weight = ctWeightToFontWeight( weightVal );
			fontDesc.stretch = ctWidthToFontStretch( widthVal );
			fontDesc.italic = ( slantVal > 0.0 );
			fontDesc.monospace = ( isMono != 0 );

			mFontList.push_back( fontDesc );
		}

		CFRelease( matchingDescs );
	}

	CFRelease( descriptors );
}

// =====================================================================
// Platform: Linux / FreeBSD (Fontconfig)
// =====================================================================
#elif EE_PLATFORM == EE_PLATFORM_LINUX || EE_PLATFORM == EE_PLATFORM_BSD

static FontWeight fcWeightToFontWeight( int fcWeight ) {
	if ( fcWeight <= FC_WEIGHT_THIN )
		return FontWeight::Thin;
	if ( fcWeight <= FC_WEIGHT_EXTRALIGHT )
		return FontWeight::ExtraLight;
	if ( fcWeight <= FC_WEIGHT_LIGHT )
		return FontWeight::Light;
	if ( fcWeight <= FC_WEIGHT_REGULAR )
		return FontWeight::Normal;
	if ( fcWeight <= FC_WEIGHT_MEDIUM )
		return FontWeight::Medium;
	if ( fcWeight <= FC_WEIGHT_SEMIBOLD )
		return FontWeight::SemiBold;
	if ( fcWeight <= FC_WEIGHT_BOLD )
		return FontWeight::Bold;
	if ( fcWeight <= FC_WEIGHT_EXTRABOLD )
		return FontWeight::ExtraBold;
	return FontWeight::Black;
}

static FontStretch fcWidthToFontStretch( int fcWidth ) {
	if ( fcWidth <= FC_WIDTH_ULTRACONDENSED )
		return FontStretch::UltraCondensed;
	if ( fcWidth <= FC_WIDTH_EXTRACONDENSED )
		return FontStretch::ExtraCondensed;
	if ( fcWidth <= FC_WIDTH_CONDENSED )
		return FontStretch::Condensed;
	if ( fcWidth <= FC_WIDTH_SEMICONDENSED )
		return FontStretch::SemiCondensed;
	if ( fcWidth <= FC_WIDTH_NORMAL )
		return FontStretch::Normal;
	if ( fcWidth <= FC_WIDTH_SEMIEXPANDED )
		return FontStretch::SemiExpanded;
	if ( fcWidth <= FC_WIDTH_EXPANDED )
		return FontStretch::Expanded;
	if ( fcWidth <= FC_WIDTH_EXTRAEXPANDED )
		return FontStretch::ExtraExpanded;
	return FontStretch::UltraExpanded;
}

void SystemFontResolver::populateFontList() const {
	if ( !FcInit() )
		return;

	FcPattern* pattern = FcPatternCreate();
	FcObjectSet* os = FcObjectSetBuild( FC_FAMILY, FC_FILE, FC_INDEX, FC_WEIGHT, FC_WIDTH, FC_SLANT,
										FC_SPACING, nullptr );

	FcFontSet* fontSet = FcFontList( nullptr, pattern, os );

	FcPatternDestroy( pattern );
	FcObjectSetDestroy( os );

	if ( !fontSet )
		return;

	for ( int i = 0; i < fontSet->nfont; ++i ) {
		FcPattern* font = fontSet->fonts[i];

		FcChar8* family = nullptr;
		if ( FcPatternGetString( font, FC_FAMILY, 0, &family ) != FcResultMatch || !family )
			continue;

		FcChar8* file = nullptr;
		if ( FcPatternGetString( font, FC_FILE, 0, &file ) != FcResultMatch || !file )
			continue;

		int fcIndex = 0;
		FcPatternGetInteger( font, FC_INDEX, 0, &fcIndex );

		int fcWeight = FC_WEIGHT_REGULAR;
		FcPatternGetInteger( font, FC_WEIGHT, 0, &fcWeight );

		int fcWidth = FC_WIDTH_NORMAL;
		FcPatternGetInteger( font, FC_WIDTH, 0, &fcWidth );

		int fcSlant = FC_SLANT_ROMAN;
		FcPatternGetInteger( font, FC_SLANT, 0, &fcSlant );

		int fcSpacing = FC_MONO;
		FcPatternGetInteger( font, FC_SPACING, 0, &fcSpacing );

		FontDesc desc;
		desc.family = reinterpret_cast<const char*>( family );
		desc.path = reinterpret_cast<const char*>( file );
		desc.faceIndex = fcIndex >= 0 ? static_cast<Uint32>( fcIndex ) : 0;
		desc.weight = fcWeightToFontWeight( fcWeight );
		desc.stretch = fcWidthToFontStretch( fcWidth );
		desc.italic = ( fcSlant == FC_SLANT_ITALIC || fcSlant == FC_SLANT_OBLIQUE );
		desc.monospace = ( fcSpacing == FC_MONO );

		mFontList.push_back( desc );
	}

	FcFontSetDestroy( fontSet );
}

// =====================================================================
// Platform: Android (NDK Font API + XML fallback)
// =====================================================================
#elif EE_PLATFORM == EE_PLATFORM_ANDROID

#if __ANDROID_API__ >= 29

void SystemFontResolver::populateFontList() const {
	AFontMatcher* matcher = AFontMatcher_create();
	if ( !matcher ) {
		Log::warning( "SystemFontResolver: AFontMatcher_create failed, trying XML fallback" );
		goto xml_fallback;
	}

	// Use the NDK font matcher to get system fonts
	// The AFontMatcher API provides only matching, not full enumeration.
	// Fall back to XML parsing for complete enumeration.

	AFontMatcher_destroy( matcher );
	goto xml_fallback;

xml_fallback:
	// XML fallback below
	populateFontListXml();
}

#else

void SystemFontResolver::populateFontList() const {
	populateFontListXml();
}

#endif

void SystemFontResolver::populateFontListXml() const {
	static const char* fontPaths[] = { "/system/etc/fonts.xml", "/system/fonts/fonts.xml",
									   "/vendor/etc/fonts.xml", nullptr };

	const char* xmlPath = nullptr;
	for ( int i = 0; fontPaths[i]; ++i ) {
		if ( FileSystem::fileExists( fontPaths[i] ) ) {
			xmlPath = fontPaths[i];
			break;
		}
	}

	if ( !xmlPath )
		return;

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file( xmlPath );
	if ( !result )
		return;

	pugi::xml_node families = doc.child( "familyset" );
	if ( !families )
		return;

	for ( pugi::xml_node familyNode : families.children( "family" ) ) {
		std::string familyName = familyNode.attribute( "name" ).as_string();
		if ( familyName.empty() )
			continue;

		for ( pugi::xml_node fontNode : familyNode.children( "font" ) ) {
			std::string fontFile = fontNode.text().as_string();
			if ( fontFile.empty() )
				continue;

			std::string fontPath = "/system/fonts/" + fontFile;
			if ( !FileSystem::fileExists( fontPath ) )
				fontPath = fontFile;

			std::string weightStr = fontNode.attribute( "weight" ).as_string();
			FontWeight weight = FontWeight::Normal;
			if ( !weightStr.empty() )
				weight = static_cast<FontWeight>( std::atoi( weightStr.c_str() ) );

			std::string styleStr = fontNode.attribute( "style" ).as_string();
			bool italic = ( styleStr == "italic" );

			FontDesc desc;
			desc.family = familyName;
			desc.path = fontPath;
			desc.faceIndex = 0;
			desc.weight = weight;
			desc.italic = italic;

			mFontList.push_back( desc );
		}
	}
}

// =====================================================================
// Platform: Haiku (BFont)
// =====================================================================
#elif EE_PLATFORM == EE_PLATFORM_HAIKU

void SystemFontResolver::populateFontList() const {
	static const char* fontDirs[] = { "/system/data/fonts", "/system/non-packaged/data/fonts",
									  nullptr };

	for ( int d = 0; fontDirs[d]; ++d ) {
		BDirectory dir( fontDirs[d] );
		if ( dir.InitCheck() != B_OK )
			continue;

		BEntry entry;
		while ( dir.GetNextEntry( &entry, true ) == B_OK ) {
			BPath path;
			if ( entry.GetPath( &path ) != B_OK )
				continue;

			std::string pathStr( path.Path() );
			if ( pathStr.empty() )
				continue;

			std::string ext = FileSystem::fileExtension( pathStr );
			if ( ext != "ttf" && ext != "otf" && ext != "ttc" && ext != "otc" && ext != "pfa" &&
				 ext != "pfb" && ext != "dfont" )
				continue;

			char nameBuf[B_FONT_FAMILY_LENGTH + 1] = {};
			char styleBuf[B_FONT_STYLE_LENGTH + 1] = {};
			bool isMono = false;

			BFont font;
			if ( font.SetFamilyAndStyle( nullptr, nullptr ) == B_OK ) {
				font_family family;
				font_style style;
				font.GetFamilyAndStyle( &family, &style );

				FontDesc desc;
				desc.family = family;
				desc.path = pathStr;
				desc.faceIndex = 0;
				desc.weight = FontWeight::Normal;

				std::string styleStr( style );
				if ( styleStr.find( "Bold" ) != std::string::npos ||
					 styleStr.find( "bold" ) != std::string::npos )
					desc.weight = FontWeight::Bold;
				if ( styleStr.find( "Italic" ) != std::string::npos ||
					 styleStr.find( "italic" ) != std::string::npos ||
					 styleStr.find( "Oblique" ) != std::string::npos )
					desc.italic = true;

				mFontList.push_back( desc );
			}
		}
	}
}

// =====================================================================
// Platform: Emscripten / Unknown (no system font access)
// =====================================================================
#else

void SystemFontResolver::populateFontList() const {}

#endif

}} // namespace EE::Graphics
