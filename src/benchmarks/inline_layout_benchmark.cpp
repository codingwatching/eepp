#include "../tests/unit_tests/utest.hpp"

#include <eepp/graphics/fontfamily.hpp>
#include <eepp/graphics/fonttruetype.hpp>
#include <eepp/graphics/richtext.hpp>
#include <eepp/system/clock.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/system/sys.hpp>
#include <eepp/window/engine.hpp>

using namespace EE;
using namespace EE::Graphics;
using namespace EE::Window;

static constexpr int numBoxes = 100;
static constexpr int numSpansPerBox = 20;
static constexpr int layoutIterations = 50;
static constexpr Float maxWidth = 800;

UTEST( Benchmark, InlineLayout ) {
	Engine::instance()->createWindow( WindowSettings(
		800, 600, "bench", WindowStyle::Default, WindowBackend::Default, 32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	if ( !font->loaded() ) {
		Engine::destroySingleton();
		UTEST_PRINT_INFO( "Failed to load font, skipping benchmark" );
		return;
	}
	FontFamily::loadFromRegular( font );

	RichText rt;
	rt.getFontStyleConfig().Font = font;
	rt.getFontStyleConfig().CharacterSize = 12;
	rt.setMaxWidth( maxWidth );

	String lorem = "Lorem ipsum dolor sit amet consectetur adipiscing elit sed do "
				   "eiusmod tempor incididunt ut labore et dolore magna aliqua ";
	String boldText = " **bold** ";

	for ( int b = 0; b < numBoxes; ++b ) {
		rt.pushInlineBox( { 0, 0, 0, 0 }, { 4, 0, 4, 0 }, 0,
						  RichText::BaselineAlignValue( RichText::BaselineAlignment::Top ) );
		for ( int s = 0; s < numSpansPerBox; ++s ) {
			rt.addInlineText( lorem, rt.getFontStyleConfig(), { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0,
							  RichText::BaselineAlignValue() );
			if ( s % 3 == 0 ) {
				FontStyleConfig boldStyle = rt.getFontStyleConfig();
				boldStyle.Style = Text::Bold;
				rt.addInlineText( boldText, boldStyle, { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0,
								  RichText::BaselineAlignValue() );
			}
		}
		rt.popInlineBox();
		rt.addInlineText( "\n", rt.getFontStyleConfig(), { 0, 0, 0, 0 }, { 0, 0, 0, 0 }, 0,
						  RichText::BaselineAlignValue() );
	}

	rt.updateLayout();
	size_t lineCount = rt.getLines().size();
	size_t fragmentCount = rt.getInlineFragments().size();

	Clock clock;
	for ( int i = 0; i < layoutIterations; ++i ) {
		rt.invalidateLayout();
		rt.updateLayout();
	}
	Time elapsed = clock.getElapsedTime();

	UTEST_PRINT_INFO( String::format( "Boxes: %d, spans/box: %d, iterations: %d", numBoxes,
									  numSpansPerBox, layoutIterations )
						  .c_str() );
	UTEST_PRINT_INFO(
		String::format( "Lines: %zu, fragments: %zu", lineCount, fragmentCount ).c_str() );
	UTEST_PRINT_INFO( String::format( "Total: %s", elapsed.toString().c_str() ).c_str() );
	UTEST_PRINT_INFO(
		String::format( "Per iteration: %lld us", elapsed.asMicroseconds() / layoutIterations )
			.c_str() );

	Engine::destroySingleton();
}

UTEST_MAIN()
