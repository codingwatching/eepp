#include "utest.hpp"
#include <eepp/graphics/font.hpp>
#include <eepp/graphics/fontmanager.hpp>
#include <eepp/scene/node.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/ui/css/stylesheet.hpp>
#include <eepp/ui/css/stylesheetlength.hpp>
#include <eepp/ui/css/stylesheetproperty.hpp>
#include <eepp/ui/css/stylesheetspecification.hpp>
#include <eepp/ui/uiapplication.hpp>
#include <eepp/ui/uirichtext.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uistyle.hpp>
#include <eepp/ui/uitextspan.hpp>
#include <eepp/ui/uitextview.hpp>
#include <eepp/ui/uithememanager.hpp>
#include <eepp/ui/uiwidget.hpp>

using namespace EE;
using namespace EE::UI;
using namespace EE::UI::CSS;
using namespace EE::Scene;
using namespace EE::Graphics;

UTEST( CSSInheritance, HtmlXmlLoadingInheritance ) {
	UIApplication app(
		WindowSettings( 800, 600, "eepp - CSS Inheritance Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash() ), 1 );

	std::string xml = R"(
<html>
	<head>
		<style>
		body {
		  background-color: white;
		  color: #FF0000;
		}
		</style>
	</head>
<body>
	<div id="testdiv">This is not color black</div>
</body>
</html>
    )";

	UIWidget* root = app.getUI()->loadLayoutFromString( xml );
	EXPECT_TRUE( root != nullptr );

	UIRichText* div = root->querySelector( "#testdiv" )->asType<UIRichText>();
	EXPECT_TRUE( div != nullptr );

	// Check if the div inherited the color black (#FF0000) from body
	if ( Color( "#FF0000" ) != div->getFontColor() ) {
		printf( "div color is: %s\n", div->getFontColor().toHexString().c_str() );
	}
	EXPECT_TRUE( Color( "#FF0000" ) == div->getFontColor() );
}

UTEST( CSSInheritance, ComputedFontSize ) {
	for ( Float scale : { 1.f, 1.5f, 2.f, 2.5f } ) {
		UTEST_PRINT_STEP( String::format( "SCALE %.1f", scale ).c_str() );
		UIApplication app( WindowSettings( 800, 600, "eepp - CSS Inheritance Test",
										   WindowStyle::Default, WindowBackend::Default, 32 ),
						   UIApplication::Settings(
							   Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), scale ) );

		std::string xml = R"(
<html>
	<head>
		<style>
		body {
		  font-size: 16px;
		}
		h1 {
		  font-size: 2em;
		}
		</style>
	</head>
<body>
	<h1 id="testh1">test text</h1>
</body>
</html>
    )";

		UIWidget* root = app.getUI()->loadLayoutFromString( xml );
		EXPECT_TRUE( root != nullptr );

		UIRichText* h1 = root->querySelector( "#testh1" )->asType<UIRichText>();
		EXPECT_TRUE( h1 != nullptr );

		EXPECT_NEAR( 32u * scale, h1->getFontSize(), 1.f );

		Node* child = h1->getFirstChild();
		EXPECT_TRUE( child != nullptr );
		EXPECT_TRUE( child->isWidget() );

		UIWidget* childWidget = child->asType<UIWidget>();
		std::string pxStr = childWidget->getPropertyString(
			StyleSheetSpecification::instance()->getProperty( PropertyId::FontSize ) );
		EXPECT_FALSE( pxStr.empty() );
		EXPECT_NEAR( 32u * scale,
					 childWidget->lengthFromValue( StyleSheetProperty( "font-size", pxStr ) ),
					 1.f );
	}
}

UTEST( CSSInheritance, ComputedFontSizePercentageAndRem ) {
	for ( Float scale : { 1.f, 1.5f, 2.f, 2.5f } ) {
		UTEST_PRINT_STEP( String::format( "SCALE %.1f", scale ).c_str() );
		UIApplication app( WindowSettings( 800, 600, "eepp - CSS Inheritance Test 2",
										   WindowStyle::Default, WindowBackend::Default, 32 ),
						   UIApplication::Settings(
							   Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), scale ) );

		std::string xml = R"(
	<html>
	<head>
		<style>
		html {
		  font-size: 20px;
		}
		body {
		  font-size: 10px;
		}
		#parent {
		  font-size: 1.5rem; /* 1.5 * 20 = 30px */
		}
		.child {
		  font-size: 200%; /* 200% of 30px = 60px */
		}
		.generic {
		  font-size: 0.5em; /* 0.5 * 60px = 30px */
		}
		</style>
	</head>
	<body>
	<div id="parent">
		<div class="child">
			<div class="generic">
				<span id="targetspan">target</span>
			</div>
		</div>
	</div>
	</body>
	</html>
	)";

		UIWidget* root = app.getUI()->loadLayoutFromString( xml );
		EXPECT_TRUE( root != nullptr );

		UIWidget* targetSpan = root->querySelector( "#targetspan" );
		EXPECT_TRUE( targetSpan != nullptr );

		EXPECT_NEAR( 18u * scale, targetSpan->asType<UITextSpan>()->getFontSize(), 1.f );
	}
}

UTEST( CSSInheritance, ExplicitColorInherit ) {
	UIApplication app(
		WindowSettings( 800, 600, "eepp - CSS Color Inherit Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash() ), 1 );

	std::string xml = R"(
<html>
	<head>
		<style>
		body {
			color: #FF0000;
		}
		#child {
			color: inherit;
		}
		</style>
	</head>
<body>
	<div>color set on body<div id="child">should be red via inherit</div></div>
</body>
</html>
    )";

	UIWidget* root = app.getUI()->loadLayoutFromString( xml );
	EXPECT_TRUE( root != nullptr );

	UIRichText* child = root->querySelector( "#child" )->asType<UIRichText>();
	EXPECT_TRUE( child != nullptr );

	EXPECT_TRUE( Color( "#FF0000" ) == child->getFontColor() );
}

UTEST( CSSInheritance, ExplicitFontSizeInherit ) {
	for ( Float scale : { 1.f, 1.5f, 2.f } ) {
		UTEST_PRINT_STEP( String::format( "SCALE %.1f", scale ).c_str() );
		UIApplication app( WindowSettings( 800, 600, "eepp - CSS FontSize Inherit Test",
										   WindowStyle::Default, WindowBackend::Default, 32 ),
						   UIApplication::Settings(
							   Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), scale ) );

		std::string xml = R"(
<html>
	<head>
		<style>
		body {
			font-size: 16px;
		}
		#parent {
			font-size: 24px;
		}
		#child {
			font-size: inherit;
		}
		</style>
	</head>
<body>
	<div id="parent">parent<div id="child">child with inherit</div></div>
</body>
</html>
    )";

		UIWidget* root = app.getUI()->loadLayoutFromString( xml );
		EXPECT_TRUE( root != nullptr );

		UIRichText* child = root->querySelector( "#child" )->asType<UIRichText>();
		EXPECT_TRUE( child != nullptr );
		EXPECT_NEAR( 24u * scale, child->getFontSize(), 1.f );
	}
}

UTEST( CSSInheritance, ExplicitFontSizeInheritEm ) {
	for ( Float scale : { 1.f, 1.5f, 2.f } ) {
		UTEST_PRINT_STEP( String::format( "SCALE %.1f", scale ).c_str() );
		UIApplication app( WindowSettings( 800, 600, "eepp - CSS FontSize Inherit Em Test",
										   WindowStyle::Default, WindowBackend::Default, 32 ),
						   UIApplication::Settings(
							   Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), scale ) );

		std::string xml = R"(
<html>
	<head>
		<style>
		body {
			font-size: 24px;
		}
		#parent {
			font-size: 1.5em;
		}
		#child {
			font-size: inherit;
		}
		</style>
	</head>
<body>
	<div id="parent">parent (1.5em = 36px)<div id="child">inherit should be 36px</div></div>
</body>
</html>
    )";

		UIWidget* root = app.getUI()->loadLayoutFromString( xml );
		EXPECT_TRUE( root != nullptr );

		UIRichText* child = root->querySelector( "#child" )->asType<UIRichText>();
		EXPECT_TRUE( child != nullptr );
		// 1.5 * 24 = 36px; inherit resolves to parent's computed 36px, not 1.5em
		EXPECT_NEAR( 36u * scale, child->getFontSize(), 1.f );
	}
}

UTEST( CSSInheritance, ExplicitFontFamilyInherit ) {
	UIApplication app(
		WindowSettings( 800, 600, "eepp - CSS FontFamily Inherit Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash() ), 1 );

	std::string xml = R"(
<html>
	<head>
		<style>
		#parent {
			font-family: monospace;
		}
		#child {
			font-family: inherit;
		}
		</style>
	</head>
<body>
	<div id="parent"><div id="child">child with font-family: inherit</div></div>
</body>
</html>
    )";

	UIWidget* root = app.getUI()->loadLayoutFromString( xml );
	EXPECT_TRUE( root != nullptr );

	UIRichText* parent = root->querySelector( "#parent" )->asType<UIRichText>();
	EXPECT_TRUE( parent != nullptr );
	UIRichText* child = root->querySelector( "#child" )->asType<UIRichText>();
	EXPECT_TRUE( child != nullptr );

	if ( parent->getFont() && child->getFont() ) {
		EXPECT_TRUE( parent->getFont() == child->getFont() );
	}
}

UTEST( CSSInheritance, ExplicitBackgroundColorInherit ) {
	UIApplication app(
		WindowSettings( 800, 600, "eepp - CSS BGColor Inherit Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash() ), 1 );

	std::string xml = R"(
<html>
	<head>
		<style>
		body {
			background-color: #00FF00;
		}
		#child {
			background-color: inherit;
		}
		</style>
	</head>
<body>
	<div id="child">child with background-color: inherit</div>
</body>
</html>
    )";

	UIWidget* root = app.getUI()->loadLayoutFromString( xml );
	EXPECT_TRUE( root != nullptr );

	UIRichText* child = root->querySelector( "#child" )->asType<UIRichText>();
	EXPECT_TRUE( child != nullptr );

	EXPECT_TRUE( Color( "#00FF00" ) == child->getBackgroundColor() );
}

UTEST( CSSUnits, ExFallback ) {
	StyleSheetLength len( "100ex" );
	Float result = len.asPixels( 0, Sizef::Zero, 96, 16, 16 );
	EXPECT_EQ( Math::round( 100.f * 16.f * 0.5f ), result );
}

UTEST( CSSUnits, ChFallback ) {
	StyleSheetLength len( "100ch" );
	Float result = len.asPixels( 0, Sizef::Zero, 96, 16, 16 );
	EXPECT_EQ( Math::round( 100.f * 16.f * 0.5f ), result );
}

UTEST( CSSUnits, ExChWithFont ) {
	UIApplication app(
		WindowSettings( 800, 600, "eepp - CSS Units Ex/Ch Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash() ), 1.f );

	Graphics::Font* font =
		app.getUI()->getUIThemeManager()->getDefaultFont();
	EXPECT_TRUE( font != nullptr );

	Float elFontSize = 24.f;

	Float resultEx = StyleSheetLength( "1ex" ).asPixels( 0, Sizef::Zero, 96, elFontSize, 16, font );
	Float resultCh = StyleSheetLength( "1ch" ).asPixels( 0, Sizef::Zero, 96, elFontSize, 16, font );
	Float resultEm = StyleSheetLength( "1em" ).asPixels( 0, Sizef::Zero, 96, elFontSize, 16, font );
	Float fallback = Math::round( elFontSize * 0.5f );

	EXPECT_GT( resultEx, 0.f );
	EXPECT_GT( resultCh, 0.f );
	EXPECT_EQ( elFontSize, resultEm );
	EXPECT_LE( resultEx, resultEm );
	EXPECT_LE( resultCh, resultEm );
	EXPECT_NE( fallback, resultEx );
	EXPECT_NE( fallback, resultCh );
}

UTEST( CSSUnits, Integration ) {
	for ( Float scale : { 1.f, 1.5f, 2.f } ) {
		UTEST_PRINT_STEP( String::format( "SCALE %.1f", scale ).c_str() );
		UIApplication app( WindowSettings( 800, 600, "eepp - CSS Units Ex/Ch Integration Test",
										   WindowStyle::Default, WindowBackend::Default, 32 ),
						   UIApplication::Settings(
							   Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), scale ) );

		std::string xml = R"(
<html>
	<head>
		<style>
		.text {
			font-size: 20px;
		}
		#exbox {
			width: 10ex;
			height: 2ex;
		}
		#chbox {
			width: 10ch;
			height: 2ch;
		}
		</style>
	</head>
<body>
	<div id="exbox" class="text">x</div>
	<div id="chbox" class="text">0</div>
</body>
</html>
		)";

		UIWidget* root = app.getUI()->loadLayoutFromString( xml );
		EXPECT_TRUE( root != nullptr );

		UIRichText* exbox = root->querySelector( "#exbox" )->asType<UIRichText>();
		EXPECT_TRUE( exbox != nullptr );
		EXPECT_GT( exbox->getPixelsSize().getWidth(), 0.f );
		EXPECT_GT( exbox->getPixelsSize().getHeight(), 0.f );

		UIRichText* chbox = root->querySelector( "#chbox" )->asType<UIRichText>();
		EXPECT_TRUE( chbox != nullptr );
		EXPECT_GT( chbox->getPixelsSize().getWidth(), 0.f );
		EXPECT_GT( chbox->getPixelsSize().getHeight(), 0.f );
	}
}
