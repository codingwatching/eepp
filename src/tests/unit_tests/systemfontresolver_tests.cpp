#include "utest.hpp"

#include <eepp/graphics/fontmanager.hpp>
#include <eepp/graphics/fonttruetype.hpp>
#include <eepp/graphics/systemfontresolver.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/system/sys.hpp>

using namespace EE;
using namespace EE::Graphics;
using namespace EE::System;

static std::string getFontsDir() {
	return Sys::getProcessPath() + "../assets/fonts/";
}

UTEST( SystemFontResolver, singletonLifecycle ) {
	SystemFontResolver::setEnabled( true );
	UTEST_PRINT_STEP( "Create singleton" );
	auto* resolver = SystemFontResolver::createSingleton();
	EXPECT_TRUE( resolver != nullptr );
	EXPECT_TRUE( SystemFontResolver::existsSingleton() != nullptr );

	UTEST_PRINT_STEP( "Destroy singleton" );
	SystemFontResolver::destroySingleton();
	EXPECT_TRUE( SystemFontResolver::existsSingleton() == nullptr );

	UTEST_PRINT_STEP( "Re-create via instance()" );
	auto* resolver2 = SystemFontResolver::instance();
	EXPECT_TRUE( resolver2 != nullptr );
	EXPECT_TRUE( SystemFontResolver::existsSingleton() != nullptr );

	SystemFontResolver::destroySingleton();
	SystemFontResolver::setEnabled( false );
}

UTEST( SystemFontResolver, genericFamilyFromName ) {
	SystemFontResolver::setEnabled( true );
	EXPECT_EQ( GenericFamily::Serif, SystemFontResolver::genericFamilyFromName( "serif" ) );
	EXPECT_EQ( GenericFamily::SansSerif,
			   SystemFontResolver::genericFamilyFromName( "sans-serif" ) );
	EXPECT_EQ( GenericFamily::Monospace, SystemFontResolver::genericFamilyFromName( "monospace" ) );
	EXPECT_EQ( GenericFamily::Cursive, SystemFontResolver::genericFamilyFromName( "cursive" ) );
	EXPECT_EQ( GenericFamily::Fantasy, SystemFontResolver::genericFamilyFromName( "fantasy" ) );
	EXPECT_EQ( GenericFamily::SystemUi, SystemFontResolver::genericFamilyFromName( "system-ui" ) );
	EXPECT_EQ( GenericFamily::Emoji, SystemFontResolver::genericFamilyFromName( "emoji" ) );
	EXPECT_EQ( GenericFamily::None,
			   SystemFontResolver::genericFamilyFromName( "bogus-font-name" ) );
	EXPECT_EQ( GenericFamily::None, SystemFontResolver::genericFamilyFromName( "" ) );

	EXPECT_EQ( GenericFamily::SansSerif,
			   SystemFontResolver::genericFamilyFromName( "SANS-SERIF" ) );
	EXPECT_EQ( GenericFamily::Monospace,
			   SystemFontResolver::genericFamilyFromName( "  monospace  " ) );

	SystemFontResolver::destroySingleton();
	SystemFontResolver::setEnabled( false );
}

UTEST( SystemFontResolver, enumerate ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();
	const auto& fonts = resolver->enumerate();
	UTEST_PRINTF( "Enumerated %zu system fonts\n", fonts.size() );

#if EE_PLATFORM == EE_PLATFORM_LINUX || EE_PLATFORM == EE_PLATFORM_BSD
	EXPECT_TRUE_MSG( fonts.size() > 0, "Fontconfig should find fonts on Linux/BSD" );
#elif EE_PLATFORM == EE_PLATFORM_WIN
	EXPECT_TRUE_MSG( fonts.size() > 0, "DirectWrite should find fonts on Windows" );
#elif EE_PLATFORM == EE_PLATFORM_MACOS || EE_PLATFORM == EE_PLATFORM_IOS
	EXPECT_TRUE_MSG( fonts.size() > 0, "CoreText should find fonts on macOS/iOS" );
#endif

	for ( const auto& desc : fonts ) {
		EXPECT_FALSE_MSG( desc.path.empty(),
						  ( "Font path must not be empty for family: " + desc.family ).c_str() );
		break;
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, enumerateFamily ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();
	auto fonts = resolver->enumerateFamily( "DejaVu Sans" );
	UTEST_PRINTF( "Found %zu DejaVu Sans fonts\n", fonts.size() );

	for ( const auto& desc : fonts ) {
		EXPECT_FALSE( desc.path.empty() );
	}

	if ( !fonts.empty() ) {
		UTEST_PRINT_STEP( "Verify faceIndex is set" );
		EXPECT_EQ( 0, fonts[0].faceIndex );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, findVerdana ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();
	FontQuery query;
	query.family = "Verdana";
	query.weight = FontWeight::Normal;
	query.italic = false;
	FontDesc desc = resolver->resolve( query );
#if EE_PLATFORM == EE_PLATFORM_LINUX || EE_PLATFORM == EE_PLATFORM_BSD || \
	EE_PLATFORM == EE_PLATFORM_WIN || EE_PLATFORM == EE_PLATFORM_MACOS
	if ( resolver->enumerate().size() > 0 ) {
		if ( !desc.path.empty() ) {
			EXPECT_STDSTREQ( "Verdana", desc.family );
		}
	}
#endif
	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveFromNamesList ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve with fallback to sans-serif" );
	FontDesc desc = resolver->resolveFromNamesList( "NonExistentFont12345, sans-serif",
													FontWeight::Normal, false );

#if EE_PLATFORM == EE_PLATFORM_EMSCRIPTEN
	EXPECT_TRUE_MSG( desc.path.empty(), "Emscripten has no system fonts" );
#else
	if ( !desc.path.empty() ) {
		EXPECT_FALSE( desc.family.empty() );
	}
#endif

	UTEST_PRINT_STEP( "Resolve empty string" );
	FontDesc descEmpty = resolver->resolveFromNamesList( "", FontWeight::Normal, false );
	EXPECT_TRUE( descEmpty.path.empty() );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolve ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve a known family" );
	FontQuery query;
	query.family = "sans-serif";
	query.weight = FontWeight::Normal;
	query.italic = false;
	FontDesc desc = resolver->resolve( query );

	if ( !desc.path.empty() ) {
		EXPECT_FALSE( desc.family.empty() );
	}

	UTEST_PRINT_STEP( "Resolve with bogus family" );
	query.family = "zzz_not_a_real_font_family_zzz";
	FontDesc descBogus = resolver->resolve( query );
	EXPECT_TRUE( descBogus.path.empty() );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveGeneric ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve monospace" );
	FontDesc descMono =
		resolver->resolveGeneric( GenericFamily::Monospace, FontWeight::Normal, false );
	UTEST_PRINTF( "Monospace default: %s at %s\n", descMono.family.c_str(), descMono.path.c_str() );

	UTEST_PRINT_STEP( "Resolve sans-serif" );
	FontDesc descSans =
		resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Normal, false );

	UTEST_PRINT_STEP( "Resolve serif" );
	FontDesc descSerif =
		resolver->resolveGeneric( GenericFamily::Serif, FontWeight::Normal, false );

	UTEST_PRINT_STEP( "Resolve None" );
	FontDesc descNone = resolver->resolveGeneric( GenericFamily::None, FontWeight::Normal, false );
	EXPECT_TRUE( descNone.path.empty() );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, fontContainsCodepoint ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	std::string fontPath = getFontsDir() + "DejaVuSansMono.ttf";
	ASSERT_TRUE_MSG( FileSystem::fileExists( fontPath ),
					 ( "Font file not found: " + fontPath ).c_str() );

	UTEST_PRINT_STEP( "ASCII letter 'A'" );
	EXPECT_TRUE( resolver->fontContainsCodepoint( fontPath, 'A' ) );

	UTEST_PRINT_STEP( "CJK character U+65E5 (日)" );
	bool hasCJK = resolver->fontContainsCodepoint( fontPath, 0x65E5 );
	UTEST_PRINTF( "DejaVuSansMono has CJK U+65E5: %s\n", hasCJK ? "yes" : "no" );

	UTEST_PRINT_STEP( "Non-existent file" );
	EXPECT_FALSE( resolver->fontContainsCodepoint( "/nonexistent/font.ttf", 'A' ) );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, getFallbackForCodepoint ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Look up fallback for CJK U+65E5 (日)" );
	FontDesc desc = resolver->getFallbackForCodepoint( 0x65E5, FontWeight::Normal, false );

	if ( !desc.path.empty() ) {
		UTEST_PRINTF( "Fallback for U+65E5: %s (%s)\n", desc.family.c_str(), desc.path.c_str() );
		EXPECT_FALSE( desc.family.empty() );
	}

	UTEST_PRINT_STEP( "Look up fallback for Arabic U+0627 (ا)" );
	FontDesc descArabic = resolver->getFallbackForCodepoint( 0x0627, FontWeight::Normal, false );
	if ( !descArabic.path.empty() ) {
		UTEST_PRINTF( "Fallback for U+0627: %s (%s)\n", descArabic.family.c_str(),
					  descArabic.path.c_str() );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, invalidateCache ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Cache a resolve result" );
	FontQuery query;
	query.family = "sans-serif";
	query.weight = FontWeight::Normal;
	FontDesc desc1 = resolver->resolve( query );

	UTEST_PRINT_STEP( "Invalidate cache" );
	resolver->invalidateCache();

	UTEST_PRINT_STEP( "Re-resolve after invalidation" );
	FontDesc desc2 = resolver->resolve( query );

	EXPECT_STDSTREQ( desc1.path, desc2.path );
	EXPECT_STDSTREQ( desc1.family, desc2.family );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, getSystemFont ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	FontDesc sysFont = resolver->getSystemFont();
	FontDesc monoFont = resolver->getSystemMonospaceFont();

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, genericFallbackPurity ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	std::vector<const char*> scriptSuffixes = {
		"georgian", "cjk", "arabic", "hebrew", "armenian", "lao", "thai",
		"devanagari", "tamil", "bengali", "gurmukhi", "gujarati", "oriya",
		"telugu", "kannada", "malayalam", "sinhala", "khmer", "tibetan",
		"myanmar", "ethiopic", "cherokee", "canadian", "mongolian", "yi",
		"nko", "tifinagh", "vai", "bamum", "coptic", "glagolitic", "gothic",
		"old", "ugaritic", "osmanya", "osmanya", "phags", "syloti"
	};

	auto checkFamily = [&scriptSuffixes]( const FontDesc& desc, const char* genericName ) {
		if ( desc.path.empty() )
			return;
		std::string lowerFamily = String::toLower( desc.family );
		for ( const char* suffix : scriptSuffixes ) {
			std::string suffixStr( suffix );
			if ( lowerFamily.find( suffixStr ) != std::string::npos ) {
				UTEST_PRINTF( "WARNING: %s fallback resolved to script-specific font: %s\n",
							  genericName, desc.family.c_str() );
			}
		}
	};

	UTEST_PRINT_STEP( "Check serif generic fallback" );
	FontDesc serif = resolver->resolveGeneric( GenericFamily::Serif, FontWeight::Normal, false );
	checkFamily( serif, "serif" );

	UTEST_PRINT_STEP( "Check sans-serif generic fallback" );
	FontDesc sans = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Normal, false );
	checkFamily( sans, "sans-serif" );

	UTEST_PRINT_STEP( "Check monospace generic fallback" );
	FontDesc mono = resolver->resolveGeneric( GenericFamily::Monospace, FontWeight::Normal, false );
	checkFamily( mono, "monospace" );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveFromNamesListRealWorld ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve with double-quoted font names" );
	FontDesc desc1 = resolver->resolveFromNamesList(
		"\"Helvetica Neue\", Arial, Helvetica, \"Nimbus Sans L\", sans-serif",
		FontWeight::Normal, false );
	UTEST_PRINTF( "Resolved: %s (%s)\n", desc1.family.c_str(), desc1.path.c_str() );

	UTEST_PRINT_STEP( "Resolve with single-quoted font names" );
	FontDesc desc2 = resolver->resolveFromNamesList(
		"'Times New Roman', serif",
		FontWeight::Normal, false );
	UTEST_PRINTF( "Resolved: %s (%s)\n", desc2.family.c_str(), desc2.path.c_str() );

	UTEST_PRINT_STEP( "Resolve with mixture of quoted and unquoted" );
	FontDesc desc3 = resolver->resolveFromNamesList(
		"Roboto, \"Helvetica Neue\", Arial, sans-serif",
		FontWeight::Normal, false );
	UTEST_PRINTF( "Resolved: %s (%s)\n", desc3.family.c_str(), desc3.path.c_str() );

	UTEST_PRINT_STEP( "Georgia, serif (regression: substring overmatch)" );
	FontDesc desc4 = resolver->resolveFromNamesList(
		"Georgia, \"Bitstream Charter\", serif",
		FontWeight::Normal, false );
	UTEST_PRINTF( "Resolved: %s (%s)\n", desc4.family.c_str(), desc4.path.c_str() );
	if ( !desc4.path.empty() ) {
		std::string lower = String::toLower( desc4.family );
		EXPECT_TRUE_MSG( lower == "georgia" || lower == "times new roman" ||
							 lower.find( "serif" ) != std::string::npos,
						 ( "Should resolve to Georgia or a serif font, got: " + desc4.family )
							 .c_str() );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveGenericWeights ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve sans-serif at multiple weights" );
	FontDesc normal = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Normal, false );
	FontDesc bold = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Bold, false );
	FontDesc light = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Light, false );
	FontDesc black = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Black, false );

	UTEST_PRINTF( "Normal: %s\n", normal.family.c_str() );
	UTEST_PRINTF( "Bold:   %s\n", bold.family.c_str() );
	UTEST_PRINTF( "Light:  %s\n", light.family.c_str() );
	UTEST_PRINTF( "Black:  %s\n", black.family.c_str() );

	if ( !normal.path.empty() )
		EXPECT_FALSE( normal.family.empty() );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveGenericWeightPreference ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Normal weight should prefer Normal or close weight" );
	FontDesc normal = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Normal, false );
	if ( !normal.path.empty() ) {
		int diff = eeabs( (int)normal.weight - (int)FontWeight::Normal );
		EXPECT_TRUE_MSG( diff <= 300,
						 ( "Normal weight diff too large: " + normal.family + " weight=" +
						   String::toString( (int)normal.weight ) )
							 .c_str() );
	}

	UTEST_PRINT_STEP( "Bold weight should prefer Bold or close weight" );
	FontDesc bold = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Bold, false );
	if ( !bold.path.empty() ) {
		int diff = eeabs( (int)bold.weight - (int)FontWeight::Bold );
		UTEST_PRINTF( "Bold: %s (weight=%d diff=%d)\n", bold.family.c_str(), (int)bold.weight, diff );
	}

	UTEST_PRINT_STEP( "Light weight should prefer Light or close weight" );
	FontDesc light = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Light, false );
	if ( !light.path.empty() ) {
		int diff = eeabs( (int)light.weight - (int)FontWeight::Light );
		UTEST_PRINTF( "Light: %s (weight=%d diff=%d)\n", light.family.c_str(), (int)light.weight, diff );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveGenericItalicFallback ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Request italic — should return a font even if no italic variant" );
	FontDesc italic = resolver->resolveGeneric( GenericFamily::SansSerif, FontWeight::Normal, true );
	if ( !italic.path.empty() ) {
		UTEST_PRINTF( "Italic: %s (italic=%d)\n", italic.family.c_str(), (int)italic.italic );
		EXPECT_FALSE( italic.family.empty() );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, resolveBoldFromNamesList ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Resolve Arial with bold weight" );
	FontDesc desc = resolver->resolveFromNamesList( "Arial, sans-serif", FontWeight::Bold, false );
	UTEST_PRINTF( "Bold: %s (weight=%d)\n", desc.family.c_str(), (int)desc.weight );
	if ( !desc.path.empty() ) {
		EXPECT_FALSE( desc.family.empty() );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, invalidateCachePersistence ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "First resolve" );
	FontQuery query;
	query.family = "sans-serif";
	query.weight = FontWeight::Normal;
	FontDesc first = resolver->resolve( query );
	UTEST_PRINTF( "First: %s (%s)\n", first.family.c_str(), first.path.c_str() );

	UTEST_PRINT_STEP( "Invalidate cache" );
	resolver->invalidateCache();

	UTEST_PRINT_STEP( "Second resolve after invalidation" );
	FontDesc second = resolver->resolve( query );
	UTEST_PRINTF( "Second: %s (%s)\n", second.family.c_str(), second.path.c_str() );

	EXPECT_STDSTREQ( first.path, second.path );
	EXPECT_STDSTREQ( first.family, second.family );

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( SystemFontResolver, glyphFallbackRoundTrip ) {
	SystemFontResolver::setEnabled( true );
	auto* resolver = SystemFontResolver::instance();

	UTEST_PRINT_STEP( "Look up fallback for CJK U+65E5" );
	FontDesc cjk = resolver->getFallbackForCodepoint( 0x65E5, FontWeight::Normal, false );
	if ( !cjk.path.empty() ) {
		UTEST_PRINTF( "CJK fallback: %s\n", cjk.family.c_str() );
		EXPECT_TRUE( resolver->fontContainsCodepoint( cjk.path, 0x65E5 ) );
	}

	UTEST_PRINT_STEP( "Look up fallback for Arabic U+0627" );
	FontDesc arabic = resolver->getFallbackForCodepoint( 0x0627, FontWeight::Normal, false );
	if ( !arabic.path.empty() ) {
		UTEST_PRINTF( "Arabic fallback: %s\n", arabic.family.c_str() );
		EXPECT_TRUE( resolver->fontContainsCodepoint( arabic.path, 0x0627 ) );
	}

	UTEST_PRINT_STEP( "Verify ASCII fallback" );
	FontDesc ascii = resolver->getFallbackForCodepoint( 'A', FontWeight::Normal, false );
	if ( !ascii.path.empty() ) {
		UTEST_PRINTF( "ASCII fallback: %s\n", ascii.family.c_str() );
		EXPECT_TRUE( resolver->fontContainsCodepoint( ascii.path, 'A' ) );
	}

	SystemFontResolver::setEnabled( false );
	SystemFontResolver::destroySingleton();
}

UTEST( FontTrueType_faceIndex, loadWithDefaultFaceIndex ) {
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	std::string fontPath = getFontsDir() + "DejaVuSansMono.ttf";
	ASSERT_TRUE( FileSystem::fileExists( fontPath ) );

	FontTrueType* font = FontTrueType::New( "Test-faceIndex-default" );
	bool loaded = font->loadFromFile( fontPath );
	ASSERT_TRUE( loaded );
	EXPECT_TRUE( font->loaded() );

	eeDelete( font );
}

UTEST( FontTrueType_faceIndex, newWithFaceIndex ) {
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	std::string fontPath = getFontsDir() + "DejaVuSansMono.ttf";
	ASSERT_TRUE( FileSystem::fileExists( fontPath ) );

	FontTrueType* font = FontTrueType::New( "Test-faceIndex-explicit", fontPath, 0 );
	ASSERT_TRUE( font != nullptr );
	EXPECT_TRUE( font->loaded() );

	eeDelete( font );
}

UTEST( FontTrueType_faceIndex, loadFromMemoryFaceIndex ) {
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	std::string fontPath = getFontsDir() + "DejaVuSansMono.ttf";
	ASSERT_TRUE( FileSystem::fileExists( fontPath ) );

	ScopedBuffer buf;
	FileSystem::fileGet( fontPath, buf );

	FontTrueType* font = FontTrueType::New( "Test-faceIndex-memory" );
	bool loaded = font->loadFromMemory( buf.get(), buf.length(), true, 0 );
	ASSERT_TRUE( loaded );
	EXPECT_TRUE( font->loaded() );

	eeDelete( font );
}
