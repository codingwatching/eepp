#include "compareimages.hpp"
#include "utest.hpp"

#include <eepp/graphics/fontfamily.hpp>
#include <eepp/graphics/fonttruetype.hpp>
#include <eepp/graphics/renderer/renderer.hpp>
#include <eepp/scene/keyevent.hpp>
#include <eepp/scene/scenemanager.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/system/sys.hpp>
#include <eepp/ui/css/stylesheetparser.hpp>
#include <eepp/ui/css/stylesheetspecification.hpp>
#include <eepp/ui/tools/htmlformatter.hpp>
#include <eepp/ui/tools/uiwidgetinspector.hpp>
#include <eepp/ui/uicheckbox.hpp>
#include <eepp/ui/uihtmldetails.hpp>
#include <eepp/ui/uihtmlinput.hpp>
#include <eepp/ui/uihtmltable.hpp>
#include <eepp/ui/uihtmltextarea.hpp>
#include <eepp/ui/uihtmltextinput.hpp>
#include <eepp/ui/uinodedrawable.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uitextspan.hpp>
#include <eepp/ui/uitheme.hpp>
#include <eepp/ui/uithememanager.hpp>
#include <eepp/ui/uiwebview.hpp>
#include <eepp/window/engine.hpp>
#include <eepp/window/input.hpp>

#include <cstdlib>
#include <iostream>

using namespace EE;
using namespace EE::Graphics;
using namespace EE::Window;
using namespace EE::Scene;
using namespace EE::UI;
using namespace EE::UI::Tools;

static void init_ui_test() {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	sceneNode->setColorSchemePreference( ColorSchemePreference::Light );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
}

UTEST( UIHTMLTable, complexLayout ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 656, "HTML Tables Test", WindowStyle::Default, WindowBackend::Default,
						32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/hn_thread_test.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	/* while ( win->isRunning() ) */ {
		win->getInput()->update();
		SceneManager::instance()->update();

		win->clear();
		SceneManager::instance()->draw();
		win->display();
	}

	auto hnMain = sceneNode->getRoot()->find( "hnmain" );
	auto bigbox = sceneNode->getRoot()->find( "bigbox" );
	auto commentTree = sceneNode->getRoot()->findByClass( "comment-tree" );
	auto votelinks = sceneNode->getRoot()->findByClass( "votelinks" );
	auto commentTd = sceneNode->getRoot()->findByClass( "default" );
	auto comment = sceneNode->getRoot()->findByClass( "comment" );
	auto commtext = sceneNode->getRoot()->findByClass( "commtext" );

	EXPECT_GT( commentTree->getPixelsSize().getWidth(), 0 );
	EXPECT_GT( commentTree->getPixelsSize().getHeight(), 0 );

	EXPECT_GT( comment->getPixelsSize().getWidth(), 0 );
	EXPECT_GT( commtext->getPixelsSize().getWidth(), 0 );

	EXPECT_GT( commentTd->getPixelsSize().getWidth(), 0 );
	EXPECT_GT( commentTd->getPixelsSize().getHeight(), 0 );

	EXPECT_GE( hnMain->getPixelsSize().getHeight(), bigbox->getPixelsSize().getHeight() );
	Float totalTds = commentTd->getPixelsSize().getWidth() + votelinks->getPixelsSize().getWidth();
	Float mainTotal = hnMain->getPixelsSize().getWidth();

	EXPECT_GT( totalTds, 0 );
	EXPECT_GT( mainTotal, 0 );

	EXPECT_NEAR( totalTds, mainTotal, 0.1 );
	compareImages( utest_state, utest_result, win, "eepp-uihtmltable-complex-layout", "html" );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, complexLayout2 ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 650, "HTML Tables Test 2", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/hn_threaded_test.html", html );
	// Keep this fixture close to the captured viewport. Apple Software Renderer drops the
	// large offscreen solid-background quad if the original full HN thread is used, even
	// though GPU renderers and other software renderers rasterize it correctly.
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	const Color expectedMainBg( "#f6f6ef" );
	UIWidget* hnMain = nullptr;
	for ( int i = 0; i < 8; i++ ) {
		win->getInput()->update();
		SceneManager::instance()->update();

		if ( !hnMain )
			hnMain = sceneNode->getRoot()->find( "hnmain" )->asType<UIWidget>();

		win->clear();
		SceneManager::instance()->draw();
		win->display();

		if ( hnMain && hnMain->getBackgroundColor() == expectedMainBg )
			break;
	}

	ASSERT_TRUE( hnMain != nullptr );
	EXPECT_STDSTREQ( hnMain->getBackgroundColor().toHexString(), expectedMainBg.toHexString() );

	compareImages( utest_state, utest_result, win, "eepp-uihtmltable-complex-layout-2", "html" );

	Engine::destroySingleton();
}

UTEST( UIHTML, redditOldThreadWebViewSmoke ) {
	if ( std::getenv( "EEPP_REDDIT_OLD_THREAD_VISUAL" ) == nullptr )
		UTEST_SKIP( "set EEPP_REDDIT_OLD_THREAD_VISUAL=1 to render the old Reddit fixture" );

	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 1000, "Old Reddit Thread Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	if ( !FileSystem::fileExists( "assets/html/reddit_old_thread_files/reddit.ETA_etA2z5U.css" ) ) {
		Engine::destroySingleton();
		UTEST_SKIP( "old Reddit fixture CSS asset is not readable" );
	}

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	UITheme* theme = UITheme::load( "breeze", "breeze", "", font, "assets/ui/breeze.css" );
	ASSERT_TRUE( theme != nullptr );
	sceneNode->setStyleSheet( theme->getStyleSheet() );
	themeManager->setDefaultFont( font )->setDefaultTheme( theme )->add( theme );

	UIWebView* webView = UIWebView::New();
	webView->setParent( sceneNode->getRoot() );
	webView->setPixelsSize( win->getWidth(), win->getHeight() );
	webView->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	webView->loadURI(
		URI( "file://" + Sys::getProcessPath() + "assets/html/reddit_old_thread.html" ) );

	win->setClearColor( Color::White );
	for ( int i = 0; i < 8; i++ ) {
		win->getInput()->update();
		SceneManager::instance()->update();
		win->clear();
		SceneManager::instance()->draw();
		win->display();
	}

	auto side = sceneNode->getRoot()->findByClass( "side" );
	auto siteTable = sceneNode->getRoot()->find( "siteTable" );
	auto midcol = sceneNode->getRoot()->findByClass( "midcol" );
	auto entry = sceneNode->getRoot()->findByClass( "entry" );
	auto arrow = sceneNode->getRoot()->findByClass( "arrow" );

	ASSERT_TRUE( side != nullptr );
	ASSERT_TRUE( siteTable != nullptr );
	ASSERT_TRUE( midcol != nullptr );
	ASSERT_TRUE( entry != nullptr );
	ASSERT_TRUE( arrow != nullptr );

	UIWidget* content =
		siteTable->getParent()->isWidget() ? siteTable->getParent()->asType<UIWidget>() : nullptr;
	ASSERT_TRUE( content != nullptr );

	Vector2f sidePos = side->asType<UIWidget>()->convertToWorldSpace( { 0, 0 } );
	Vector2f contentPos = content->convertToWorldSpace( { 0, 0 } );
	Vector2f midcolPos = midcol->asType<UIWidget>()->convertToWorldSpace( { 0, 0 } );
	Vector2f entryPos = entry->asType<UIWidget>()->convertToWorldSpace( { 0, 0 } );

	std::cerr << "old reddit rects: "
			  << "side=(" << sidePos.x << "," << sidePos.y << " "
			  << side->asType<UIWidget>()->getPixelsSize().getWidth() << "x"
			  << side->asType<UIWidget>()->getPixelsSize().getHeight() << ") "
			  << "content=(" << contentPos.x << "," << contentPos.y << " "
			  << content->getPixelsSize().getWidth() << "x" << content->getPixelsSize().getHeight()
			  << ") "
			  << "midcol=(" << midcolPos.x << "," << midcolPos.y << " "
			  << midcol->asType<UIWidget>()->getPixelsSize().getWidth() << "x"
			  << midcol->asType<UIWidget>()->getPixelsSize().getHeight() << ") "
			  << "entry=(" << entryPos.x << "," << entryPos.y << " "
			  << entry->asType<UIWidget>()->getPixelsSize().getWidth() << "x"
			  << entry->asType<UIWidget>()->getPixelsSize().getHeight() << ")" << std::endl;

	if ( !FileSystem::fileExists( "output" ) )
		FileSystem::makeDir( "output" );
	win->getFrontBufferImage().saveToFile( "output/eepp-reddit-old-thread-current.webp",
										   Image::SaveType::WEBP );

	Engine::destroySingleton();
}

UTEST( UIRichText, anchorMargins ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 800, 600, "Anchor Margins Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/anchor_margins.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );

	for ( auto anchor : anchors ) {
		auto a = anchor->asType<UIAnchorSpan>();
		EXPECT_EQ( anchor->getPixelsSize().getHeight(),
				   a->getFont()->getLineSpacing( a->getFontSize() ) );
	}

	compareImages( utest_state, utest_result, win, "eepp-ui-anchor-margins", "html" );

	Engine::destroySingleton();
}

UTEST( UIRichText, spanPadding ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 800, 600, "Span Padding Test", WindowStyle::Default, WindowBackend::Default,
						32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/span_padding.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-span-padding", "html" );

	Engine::destroySingleton();
}

UTEST( UIRichText, anchorPadding ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 800, 600, "Anchor Span Padding Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/anchor_padding.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-anchor-padding", "html" );

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );
	ASSERT_GE( anchors.size(), (size_t)1 );
	auto downloadLink = anchors[0]->asType<UIWidget>();
	EXPECT_NEAR( downloadLink->getPixelsSize().getWidth(), 81.f, 3.f );
	EXPECT_NEAR( downloadLink->getPixelsSize().getHeight(), 28.f, 3.f );

	Engine::destroySingleton();
}

UTEST( UIRichText, anchorPaddingLineHeight ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 800, 600, "Anchor Padding LineHeight Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/anchor_padding_lineheight.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-anchor-padding-lineheight", "html" );

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );
	ASSERT_GE( anchors.size(), (size_t)1 );
	auto downloadLink = anchors[0]->asType<UIWidget>();
	EXPECT_NEAR( downloadLink->getPixelsSize().getWidth(), 81.f, 3.f );
	EXPECT_NEAR( downloadLink->getPixelsSize().getHeight(), 28.f, 3.f );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineBaselineAlignmentProperties ) {
	Engine::instance()->createWindow( WindowSettings( 800, 600, "Inline Baseline Alignment Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	sceneNode->getUIThemeManager()->setDefaultFont( font );
	sceneNode->loadLayoutFromString( R"html(
		<vbox layout_width="wrap_content" layout_height="wrap_content">
			<RichText id="baseline" font-size="20dp" layout_width="wrap_content" layout_height="wrap_content">
				A<span id="base_box" display="inline-block" layout_width="20dp" layout_height="20dp" vertical-align="baseline">X</span>
			</RichText>
			<RichText id="middle" font-size="20dp" layout_width="wrap_content" layout_height="wrap_content">
				A<span id="middle_box" display="inline-block" layout_width="20dp" layout_height="20dp" vertical-align="middle"><span id="middle_child">X</span></span>
			</RichText>
			<RichText id="alignment_middle" font-size="20dp" layout_width="wrap_content" layout_height="wrap_content">
				A<span id="alignment_box" display="inline-block" layout_width="20dp" layout_height="20dp" alignment-baseline="middle">X</span>
			</RichText>
		</vbox>
	)html" );
	sceneNode->updateDirtyLayouts();

	auto* baseline = sceneNode->getRoot()->find( "baseline" )->asType<UIRichText>();
	auto* middle = sceneNode->getRoot()->find( "middle" )->asType<UIRichText>();
	auto* alignmentMiddle = sceneNode->getRoot()->find( "alignment_middle" )->asType<UIRichText>();
	auto* baselineBox = sceneNode->getRoot()->find( "base_box" )->asType<UIHTMLWidget>();
	auto* middleBox = sceneNode->getRoot()->find( "middle_box" )->asType<UIHTMLWidget>();
	auto* middleChild = sceneNode->getRoot()->find( "middle_child" )->asType<UIHTMLWidget>();
	auto* alignmentBox = sceneNode->getRoot()->find( "alignment_box" )->asType<UIHTMLWidget>();
	ASSERT_TRUE( baseline != nullptr );
	ASSERT_TRUE( middle != nullptr );
	ASSERT_TRUE( alignmentMiddle != nullptr );
	ASSERT_TRUE( baselineBox != nullptr );
	ASSERT_TRUE( middleBox != nullptr );
	ASSERT_TRUE( middleChild != nullptr );
	ASSERT_TRUE( alignmentBox != nullptr );

	EXPECT_EQ( baselineBox->getBaselineAlign().type, CSSBaselineAlignment::Baseline );
	EXPECT_EQ( middleBox->getBaselineAlign().type, CSSBaselineAlignment::Middle );
	EXPECT_EQ( middleChild->getBaselineAlign().type, CSSBaselineAlignment::Baseline );
	EXPECT_EQ( alignmentBox->getBaselineAlign().type, CSSBaselineAlignment::Middle );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, complexLayout3 ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 650, "HTML Tables Test 3", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/hn_frontpage.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-uihtmltable-complex-layout-3", "html" );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, nestedPerformance ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	// Create nested tables.
	UIHTMLTable* rootTable = UIHTMLTable::New();
	rootTable->setLayoutSizePolicy( SizePolicy::MatchParent, SizePolicy::WrapContent );
	rootTable->setParent( sceneNode->getRoot() );

	UIHTMLTable* currentTable = rootTable;
	for ( int i = 0; i < 10; ++i ) {
		UIHTMLTableRow* row = UIHTMLTableRow::New();
		row->setParent( currentTable );
		UIHTMLTableCell* cell = UIHTMLTableCell::New( "td" );
		cell->setParent( row );

		UIHTMLTable* childTable = UIHTMLTable::New();
		childTable->setLayoutSizePolicy( SizePolicy::MatchParent, SizePolicy::WrapContent );
		childTable->setParent( cell );
		currentTable = childTable;
	}

	UIHTMLTableRow* row = UIHTMLTableRow::New();
	row->setParent( currentTable );
	UIHTMLTableCell* cell = UIHTMLTableCell::New( "td" );
	cell->setParent( row );
	UITextSpan* span = UITextSpan::New();
	span->setParent( cell );
	span->setText( "Deeply nested text" );

	Clock clock;
	sceneNode->updateDirtyLayouts();

	Log::info( "Time for nested layout (10 levels): %.2f ms",
			   clock.getElapsedTime().asMilliseconds() );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, specifiedWidth ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	sceneNode->loadLayoutFromString(
		R"(<table style="width: 500px; height: wrap-content;">
			<tr>
				<td id="c1">C1</td>
				<td id="c2" style="width: 200px;">C2</td>
			</tr>
		</table>)" );

	sceneNode->updateDirtyLayouts();

	auto c1 = sceneNode->getRoot()->find( "c1" );
	auto c2 = sceneNode->getRoot()->find( "c2" );

	ASSERT_TRUE( c1 != nullptr );
	ASSERT_TRUE( c2 != nullptr );

	// Cell 2 should be at least 200px.
	EXPECT_GE( c2->getPixelsSize().getWidth(), 200.f );
	// Total width should be 500px (minus padding if any, but default is 0).
	EXPECT_NEAR( c1->getPixelsSize().getWidth() + c2->getPixelsSize().getWidth(), 500.f, 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, nestedSpecifiedWidth ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	sceneNode->loadLayoutFromString(
		R"(<table style="width: 500px; height: wrap-content;">
			<tr>
				<td id="c1"><img style="width: 50px; height: 10px;" /></td>
				<td id="c2">Flexible text content that should take the rest of the space</td>
			</tr>
		</table>)" );

	sceneNode->updateDirtyLayouts();

	auto c1 = sceneNode->getRoot()->find( "c1" );
	auto c2 = sceneNode->getRoot()->find( "c2" );

	ASSERT_TRUE( c1 != nullptr );
	ASSERT_TRUE( c2 != nullptr );

	// Cell 1 should be exactly 50px because it's rigid (only contains fixed-width image)
	// and Cell 2 is flexible.
	EXPECT_NEAR( c1->getPixelsSize().getWidth(), 50.f, 1.f );
	EXPECT_NEAR( c2->getPixelsSize().getWidth(), 450.f, 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTMLInput, sizeAttribute ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<vbox layout_width="wrap_content" layout_height="wrap_content">
			<input id="i1" size="10" />
			<input id="i2" size="20" />
			<input id="i3" />
			<input id="i_pwd" type="password" />
			<input id="i_mode_pwd" input-mode="password" />
			<input id="i_chk" type="checkbox" />
		</vbox>
	)html" );

	auto c1 = sceneNode->getRoot()->find( "i1" )->asType<UIHTMLInput>();
	auto c2 = sceneNode->getRoot()->find( "i2" )->asType<UIHTMLInput>();
	auto c3 = sceneNode->getRoot()->find( "i3" )->asType<UIHTMLInput>();
	auto cp = sceneNode->getRoot()->find( "i_pwd" )->asType<UIHTMLInput>();
	auto cm = sceneNode->getRoot()->find( "i_mode_pwd" )->asType<UIHTMLInput>();
	auto cc = sceneNode->getRoot()->find( "i_chk" )->asType<UIHTMLInput>();

	ASSERT_TRUE( c1 != nullptr );
	ASSERT_TRUE( c2 != nullptr );
	ASSERT_TRUE( c3 != nullptr );
	ASSERT_TRUE( cp != nullptr );
	ASSERT_TRUE( cm != nullptr );
	ASSERT_TRUE( cc != nullptr );

	auto i1 = c1->getChildWidget()->asType<UIHTMLTextInput>();
	auto i2 = c2->getChildWidget()->asType<UIHTMLTextInput>();
	auto i3 = c3->getChildWidget()->asType<UIHTMLTextInput>();

	ASSERT_TRUE( i1 != nullptr );
	ASSERT_TRUE( i2 != nullptr );
	ASSERT_TRUE( i3 != nullptr );

	EXPECT_EQ( i1->getHtmlSize(), 10u );
	EXPECT_EQ( i2->getHtmlSize(), 20u );
	EXPECT_EQ( i3->getHtmlSize(), 20u );

	EXPECT_GT( i2->getPixelsSize().getWidth(), i1->getPixelsSize().getWidth() );
	EXPECT_NEAR( i2->getPixelsSize().getWidth(), i3->getPixelsSize().getWidth(), 1.f );

	EXPECT_TRUE( cp->getChildWidget()->isType( UI_TYPE_TEXTINPUT ) );
	EXPECT_TRUE( cp->getChildWidget()->asType<UITextInput>()->getMode() ==
				 UITextInput::TextInputMode::Password );
	EXPECT_TRUE( cp->getChildWidget()->asType<UITextInput>()->getMode() ==
				 UITextInput::TextInputMode::Password );
	EXPECT_TRUE( cm->getChildWidget()->asType<UITextInput>()->getMode() ==
				 UITextInput::TextInputMode::Password );
	EXPECT_TRUE( cc->getChildWidget()->isType( UI_TYPE_CHECKBOX ) );

	Engine::destroySingleton();
}

UTEST( UIHTML, DataProperties ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<html>
			<body>
				<div id="target"
					 data-role="hero"
					 data-tags="featured primary"
					 data-lang="en-US"
					 data-language="cpp"
					 data-id="user-42"
					 data-empty="">
				</div>
				<div id="missing-control"></div>
			</body>
		</html>
	)html" ) );

	auto* target = sceneNode->getRoot()->find( "target" )->asType<UIHTMLWidget>();
	ASSERT_TRUE( target != nullptr );

	EXPECT_TRUE( target->hasDataProperty( "data-role" ) );
	EXPECT_TRUE( target->hasDataProperty( "DATA-ROLE" ) );
	EXPECT_TRUE( target->hasDataProperty( "data-empty" ) );
	EXPECT_FALSE( target->hasDataProperty( "data-missing" ) );
	EXPECT_TRUE( target->getDataPropertyString( "data-role" ) == "hero" );
	EXPECT_TRUE( target->getDataPropertyString( "data-empty", "fallback" ) == "" );
	EXPECT_TRUE( target->getDataPropertyString( "data-missing", "fallback" ) == "fallback" );
	EXPECT_TRUE( target->getPropertyString( "data-role" ) == "hero" );

	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-role]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-role=\"hero\"]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-tags~=\"featured\"]" ).size(),
			   (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-lang|=\"en\"]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-id^=\"user-\"]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-id$=\"-42\"]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-id*=\"ser\"]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-empty]" ).size(), (size_t)1 );
	EXPECT_EQ( sceneNode->getRoot()->querySelectorAll( "[data-missing]" ).size(), (size_t)0 );

	EXPECT_TRUE( target->getDataPropertyString( "data-language" ) == "cpp" );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, closedByDefault ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d"><summary id="s">Label</summary><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* summary = sceneNode->getRoot()->find( "s" )->asType<UIHTMLSummary>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( summary != nullptr );
	ASSERT_TRUE( content != nullptr );
	EXPECT_FALSE( details->isOpen() );
	EXPECT_TRUE( summary->isVisible() );
	EXPECT_FALSE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, summaryListStyleType ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d">
			<summary id="s1">Default</summary>
			<summary id="s2" style="list-style-type: disclosure-open;">Open marker</summary>
			<summary id="s3" style="list-style-type: decimal;">Decimal marker</summary>
		</details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	const auto* propDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );
	ASSERT_TRUE( propDef != nullptr );

	auto* s1 = sceneNode->getRoot()->find( "s1" )->asType<UIHTMLSummary>();
	auto* s2 = sceneNode->getRoot()->find( "s2" )->asType<UIHTMLSummary>();
	auto* s3 = sceneNode->getRoot()->find( "s3" )->asType<UIHTMLSummary>();
	ASSERT_TRUE( s1 != nullptr );
	ASSERT_TRUE( s2 != nullptr );
	ASSERT_TRUE( s3 != nullptr );

	EXPECT_TRUE( s1->getPropertyString( propDef ) == "disclosure-closed" );
	EXPECT_TRUE( s2->getPropertyString( propDef ) == "disclosure-open" );
	EXPECT_TRUE( s3->getPropertyString( propDef ) == "decimal" );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, summaryListStyleNoneClearsDefaultPadding ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details>
			<summary id="default">Default marker</summary>
			<summary id="none" style="list-style-type: none;">No marker</summary>
			<summary id="explicit" style="list-style-type: none; padding-left: 7px;">
				Explicit padding
			</summary>
		</details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* defaultSummary = sceneNode->getRoot()->find( "default" )->asType<UIHTMLSummary>();
	auto* noneSummary = sceneNode->getRoot()->find( "none" )->asType<UIHTMLSummary>();
	auto* explicitSummary = sceneNode->getRoot()->find( "explicit" )->asType<UIHTMLSummary>();
	ASSERT_TRUE( defaultSummary != nullptr );
	ASSERT_TRUE( noneSummary != nullptr );
	ASSERT_TRUE( explicitSummary != nullptr );

	EXPECT_GT( defaultSummary->getPixelsPadding().Left, 0.f );
	EXPECT_NEAR( noneSummary->getPixelsPadding().Left, 0.f, 0.5f );
	EXPECT_NEAR( explicitSummary->getPixelsPadding().Left, 7.f, 0.5f );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, inlineBlockSummaryListStyleNoneSize ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	std::string html;
	FileSystem::fileGet( "assets/html/lobsters_item.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->querySelector( ".caches" )->asType<UIHTMLDetails>();
	auto* summary =
		sceneNode->getRoot()->querySelector( ".caches summary" )->asType<UIHTMLSummary>();
	auto* author = sceneNode->getRoot()->querySelector( ".u-author" )->asType<UIWidget>();
	auto* time = sceneNode->getRoot()->querySelector( "time" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( summary != nullptr );
	ASSERT_TRUE( author != nullptr );
	ASSERT_TRUE( time != nullptr );

	EXPECT_EQ( details->getDisplay(), CSSDisplay::InlineBlock );
	EXPECT_EQ( summary->getListStyleType(), CSSListStyleType::None );
	EXPECT_NEAR( summary->getPixelsPadding().Left, 0.f, 0.5f );
	EXPECT_LE( details->getPixelsSize().getHeight(),
			   eemax( author->getPixelsSize().getHeight(), time->getPixelsSize().getHeight() ) +
				   1.f );
	EXPECT_NEAR( details->getPixelsPosition().y, author->getPixelsPosition().y, 2.f );
	EXPECT_NEAR( details->getPixelsPosition().y, time->getPixelsPosition().y, 2.f );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, openAttribute ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d" open><summary>Label</summary><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( content != nullptr );
	EXPECT_TRUE( details->isOpen() );
	EXPECT_TRUE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, openAttributeExplicitFalse ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d" open="false"><summary>Label</summary><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( content != nullptr );
	EXPECT_FALSE( details->isOpen() );
	EXPECT_FALSE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, toggleViaMouse ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d"><summary id="s">Label</summary><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* summary = sceneNode->getRoot()->find( "s" )->asType<UIHTMLSummary>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( summary != nullptr );
	ASSERT_TRUE( content != nullptr );

	summary->onMouseClick( summary->getPixelsPosition().asInt(), EE_BUTTON_LMASK );
	EXPECT_TRUE( details->isOpen() );
	EXPECT_TRUE( content->isVisible() );
	summary->onMouseClick( summary->getPixelsPosition().asInt(), EE_BUTTON_LMASK );
	EXPECT_FALSE( details->isOpen() );
	EXPECT_FALSE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, toggleViaKeyboard ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d"><summary id="s">Label</summary><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* summary = sceneNode->getRoot()->find( "s" )->asType<UIHTMLSummary>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( summary != nullptr );

	KeyEvent enter( summary, Event::KeyDown, KEY_RETURN, SCANCODE_RETURN, 0, 0 );
	KeyEvent space( summary, Event::KeyDown, KEY_SPACE, SCANCODE_SPACE, 0, 0 );
	EXPECT_EQ( summary->onKeyDown( enter ), 1u );
	EXPECT_TRUE( details->isOpen() );
	EXPECT_EQ( summary->onKeyDown( space ), 1u );
	EXPECT_FALSE( details->isOpen() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, toggleEvent ) {
	init_ui_test();
	auto* details = UIHTMLDetails::New();
	details->setParent( SceneManager::instance()->getUISceneNode()->getRoot() );
	int toggleCount = 0;
	details->on( Event::OnToggle, [&toggleCount]( const Event* ) { toggleCount++; } );

	details->setOpen( true );
	details->setOpen( true );
	details->setOpen( false );
	EXPECT_EQ( toggleCount, 2 );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, autoSummary ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d"><p id="p">Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( content != nullptr );
	auto* summary = details->findSummaryChild();
	ASSERT_TRUE( summary != nullptr );
	EXPECT_TRUE( summary->isVisible() );
	EXPECT_FALSE( content->isVisible() );
	EXPECT_TRUE( summary->toggleParentDetails() );
	EXPECT_TRUE( details->isOpen() );
	EXPECT_TRUE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, multipleSummaries ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d">
			<summary id="s1">One</summary>
			<summary id="s2">Two</summary>
			<p id="p">Content</p>
		</details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* s1 = sceneNode->getRoot()->find( "s1" )->asType<UIHTMLSummary>();
	auto* s2 = sceneNode->getRoot()->find( "s2" )->asType<UIHTMLSummary>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( s1 != nullptr );
	ASSERT_TRUE( s2 != nullptr );
	EXPECT_TRUE( s1->isVisible() );
	EXPECT_FALSE( s2->isVisible() );
	EXPECT_TRUE( s1->toggleParentDetails() );
	EXPECT_TRUE( details->isOpen() );
	EXPECT_TRUE( s2->isVisible() );
	EXPECT_FALSE( s2->toggleParentDetails() );
	EXPECT_TRUE( details->isOpen() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, nested ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="outer" open>
			<summary id="outer_s">Outer</summary>
			<details id="inner">
				<summary id="inner_s">Inner</summary>
				<p id="inner_p">Inner content</p>
			</details>
		</details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* outer = sceneNode->getRoot()->find( "outer" )->asType<UIHTMLDetails>();
	auto* inner = sceneNode->getRoot()->find( "inner" )->asType<UIHTMLDetails>();
	auto* innerSummary = sceneNode->getRoot()->find( "inner_s" )->asType<UIHTMLSummary>();
	ASSERT_TRUE( outer != nullptr );
	ASSERT_TRUE( inner != nullptr );
	ASSERT_TRUE( innerSummary != nullptr );
	EXPECT_TRUE( outer->isOpen() );
	EXPECT_FALSE( inner->isOpen() );
	EXPECT_TRUE( innerSummary->toggleParentDetails() );
	EXPECT_TRUE( outer->isOpen() );
	EXPECT_TRUE( inner->isOpen() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, hiddenChildPreserved ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<details id="d" open><summary>Label</summary><p id="p" hidden>Content</p></details>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* details = sceneNode->getRoot()->find( "d" )->asType<UIHTMLDetails>();
	auto* content = sceneNode->getRoot()->find( "p" )->asType<UIWidget>();
	ASSERT_TRUE( details != nullptr );
	ASSERT_TRUE( content != nullptr );
	EXPECT_FALSE( content->isVisible() );
	details->setOpen( false );
	details->setOpen( true );
	EXPECT_FALSE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, dynamicChildAddedWhileClosed ) {
	init_ui_test();
	auto* details = UIHTMLDetails::New();
	details->setParent( SceneManager::instance()->getUISceneNode()->getRoot() );
	auto* summary = UIHTMLSummary::New();
	summary->setParent( details );
	details->setOpen( false );
	auto* content = UIRichText::NewParagraph();
	content->setParent( details );
	details->updateLayout();

	EXPECT_TRUE( summary->isVisible() );
	EXPECT_FALSE( content->isVisible() );

	Engine::destroySingleton();
}

UTEST( UIHTMLTextArea, rowsColsAttribute ) {
	init_ui_test();
	auto* scene = SceneManager::instance()->getUISceneNode();
	auto* c1_raw = scene->loadLayoutFromString( R"html(
		<vbox layout_width="wrap_content" layout_height="wrap_content">
			<textarea id="t1" rows="2" cols="20"></textarea>
			<textarea id="t2" rows="4" cols="40"></textarea>
		</vbox>
	)html" );
	ASSERT_TRUE( c1_raw != nullptr );
	auto* t1 = c1_raw->find( "t1" )->asType<UIHTMLTextArea>();
	auto* t2 = c1_raw->find( "t2" )->asType<UIHTMLTextArea>();
	ASSERT_TRUE( t1 != nullptr );
	ASSERT_TRUE( t2 != nullptr );
	EXPECT_EQ( t1->getRows(), 2u );
	EXPECT_EQ( t1->getCols(), 20u );
	EXPECT_EQ( t2->getRows(), 4u );
	EXPECT_EQ( t2->getCols(), 40u );
	EXPECT_GT( t2->getPixelsSize().getWidth(), t1->getPixelsSize().getWidth() );
	EXPECT_GT( t2->getPixelsSize().getHeight(), t1->getPixelsSize().getHeight() );

	Engine::destroySingleton();
}

UTEST( UIHTML, FormControlsDefaultInlineBlock ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<div id="container" style="width: 600px;">
			<input id="i1" size="5">
			<input id="i2" size="5">
			<textarea id="t1" rows="2" cols="8"></textarea>
			<textarea id="t2" rows="2" cols="8"></textarea>
		</div>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	auto* i1 = sceneNode->getRoot()->find( "i1" )->asType<UIHTMLInput>();
	auto* i2 = sceneNode->getRoot()->find( "i2" )->asType<UIHTMLInput>();
	auto* t1 = sceneNode->getRoot()->find( "t1" )->asType<UIHTMLTextArea>();
	auto* t2 = sceneNode->getRoot()->find( "t2" )->asType<UIHTMLTextArea>();

	ASSERT_TRUE( i1 != nullptr );
	ASSERT_TRUE( i2 != nullptr );
	ASSERT_TRUE( t1 != nullptr );
	ASSERT_TRUE( t2 != nullptr );

	EXPECT_EQ( i1->getDisplay(), CSSDisplay::InlineBlock );
	EXPECT_EQ( i2->getDisplay(), CSSDisplay::InlineBlock );

	EXPECT_EQ( i1->getPixelsPosition().y, i2->getPixelsPosition().y );
	EXPECT_LT( i1->getPixelsPosition().x, i2->getPixelsPosition().x );
	EXPECT_EQ( t1->getPixelsPosition().y, t2->getPixelsPosition().y );
	EXPECT_LT( t1->getPixelsPosition().x, t2->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( UIHTMLTable, tableLayoutFixed ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	sceneNode->loadLayoutFromString(
		R"(<table style="width: 600px; table-layout: fixed;">
			<tr>
				<td id="c1" style="width: 100px;">C1</td>
				<td id="c2" style="width: 200px;">C2</td>
				<td id="c3">C3</td>
			</tr>
			<tr>
				<td style="width: 500px;">C4 (Should be ignored)</td>
				<td>C5</td>
				<td>C6</td>
			</tr>
		</table>)" );

	sceneNode->updateDirtyLayouts();

	auto c1 = sceneNode->getRoot()->find( "c1" );
	auto c2 = sceneNode->getRoot()->find( "c2" );
	auto c3 = sceneNode->getRoot()->find( "c3" );

	ASSERT_TRUE( c1 != nullptr );
	ASSERT_TRUE( c2 != nullptr );
	ASSERT_TRUE( c3 != nullptr );

	// Total width is 600px. C1=100, C2=200, C3 takes remaining 300px.
	EXPECT_NEAR( c1->getPixelsSize().getWidth(), 100.f, 1.f );
	EXPECT_NEAR( c2->getPixelsSize().getWidth(), 200.f, 1.f );
	EXPECT_NEAR( c3->getPixelsSize().getWidth(), 300.f, 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTMLBody, backgroundColorPropagation ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "HTML Tables Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	sceneNode->loadLayoutFromString(
		R"(<html id="html_el">
			<body id="body_el" style="background-color: red; max-width: 960px;">
			</body>
		</html>)" );

	sceneNode->updateDirtyLayouts();

	auto html_el = sceneNode->getRoot()->find( "html_el" );
	auto body_el = sceneNode->getRoot()->find( "body_el" );

	ASSERT_TRUE( html_el != nullptr );
	ASSERT_TRUE( body_el != nullptr );

	// HTML element should have inherited the red background color, and body should be transparent
	EXPECT_TRUE( html_el->asType<UIWidget>()->getBackgroundColor() == Color::Red );
	EXPECT_TRUE( body_el->asType<UIWidget>()->getBackgroundColor() == Color::Transparent );

	Engine::destroySingleton();
}

UTEST( UIHTMLBody, maxWidthResizingBug ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 768, "HTML Resize Bug",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );

	UI::CSS::StyleSheetParser parser;
	parser.loadFromFile( "assets/html/dwarmstrong/style.css" );
	sceneNode->setStyleSheet( parser.getStyleSheet() );

	std::string htmlContent;
	FileSystem::fileGet( "assets/html/dwarmstrong/dwarmstrong.html", htmlContent );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( htmlContent ) );

	sceneNode->getRoot()->setSize( 1024, 768 );
	sceneNode->updateDirtyLayouts();

	auto body_el = sceneNode->getRoot()->findByType( UI_TYPE_HTML_BODY )->asType<UIWidget>();
	ASSERT_TRUE( body_el != nullptr );
	Float widthAt1024 = body_el->getPixelsSize().getWidth();
	EXPECT_NEAR( widthAt1024, 960.f,
				 10.f ); // It should be around 960px (minus some margins if any)

	sceneNode->getRoot()->setSize( 2048, 768 );
	sceneNode->updateDirtyLayouts();
	Float widthAt2048 = body_el->getPixelsSize().getWidth();
	EXPECT_NEAR( widthAt2048, 960.f, 10.f ); // Body should stay 960px even when parent is huge

	sceneNode->getRoot()->setSize( 1024, 768 );
	sceneNode->updateDirtyLayouts();

	Float widthAfterResize = body_el->getPixelsSize().getWidth();
	EXPECT_NEAR( widthAt1024, widthAfterResize, 1.f );

	Engine::destroySingleton();
}

UTEST( UILayout, marginAuto ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Margin Auto Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	auto* container = sceneNode->loadLayoutFromString(
		R"(<vbox id="container">
			<widget id="child" style="margin: 0 auto;" />
		</vbox>)" );

	auto child = sceneNode->getRoot()->find( "child" );
	ASSERT_TRUE( child != nullptr );

	UIWidget* childWidget = child->asType<UIWidget>();
	UIWidget* contWidget = container->asType<UIWidget>();

	contWidget->setSize( 500, 500 );
	childWidget->setSize( 100, 100 );
	sceneNode->updateDirtyLayouts();

	Float expectedMarginX =
		( contWidget->getPixelsSize().getWidth() - childWidget->getPixelsSize().getWidth() ) / 2.f;

	// Margin left/right should be auto computed to expectedMarginX
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Left, expectedMarginX, 1.f );
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Right, expectedMarginX, 1.f );
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Top, 0.f, 1.f );
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Bottom, 0.f, 1.f );

	// Resize parent and see if margins re-evaluate automatically
	contWidget->setSize( 800, 800 );
	sceneNode->updateDirtyLayouts();

	expectedMarginX =
		( contWidget->getPixelsSize().getWidth() - childWidget->getPixelsSize().getWidth() ) / 2.f;

	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Left, expectedMarginX, 1.f );
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Right, expectedMarginX, 1.f );

	// Now test resize of child
	childWidget->setSize( 200, 100 );
	sceneNode->updateDirtyLayouts();

	expectedMarginX =
		( contWidget->getPixelsSize().getWidth() - childWidget->getPixelsSize().getWidth() ) / 2.f;

	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Left, expectedMarginX, 1.f );
	EXPECT_NEAR( childWidget->getLayoutPixelsMargin().Right, expectedMarginX, 1.f );

	Engine::destroySingleton();
}

UTEST( UILayout, listStyleTypeDecimal ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<html>
			<ol>
				<li id="li1" style="list-style-type: decimal;">First item</li>
				<li id="li2" style="list-style-type: decimal;">Second item</li>
				<li id="li3" style="list-style-type: decimal;">Third item</li>
			</ol>
		</html>
	)html" );

	sceneNode->updateDirtyLayouts();

	const auto* propDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );
	ASSERT_TRUE( propDef != nullptr );

	auto* li1 = sceneNode->getRoot()->find( "li1" )->asType<UIRichText>();
	auto* li2 = sceneNode->getRoot()->find( "li2" )->asType<UIRichText>();
	auto* li3 = sceneNode->getRoot()->find( "li3" )->asType<UIRichText>();

	ASSERT_TRUE( li1 != nullptr );
	ASSERT_TRUE( li2 != nullptr );
	ASSERT_TRUE( li3 != nullptr );

	EXPECT_TRUE( li1->getPropertyString( propDef ) == "decimal" );
	EXPECT_TRUE( li2->getPropertyString( propDef ) == "decimal" );
	EXPECT_TRUE( li3->getPropertyString( propDef ) == "decimal" );

	Engine::destroySingleton();
}

UTEST( UILayout, listStyleTypeDisc ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<html>
			<ul>
				<li id="li1" style="list-style-type: disc;">Bullet item</li>
			</ul>
		</html>
	)html" );

	sceneNode->updateDirtyLayouts();

	const auto* propDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );
	ASSERT_TRUE( propDef != nullptr );

	auto* li1 = sceneNode->getRoot()->find( "li1" )->asType<UIRichText>();
	ASSERT_TRUE( li1 != nullptr );

	EXPECT_TRUE( li1->getPropertyString( propDef ) == "disc" );

	Engine::destroySingleton();
}

UTEST( UILayout, listStyleTypeDisclosure ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<html>
			<ul>
				<li id="li1" style="list-style-type: disclosure-closed;">Closed</li>
				<li id="li2" style="list-style-type: disclosure-open;">Open</li>
			</ul>
		</html>
	)html" );

	sceneNode->updateDirtyLayouts();

	const auto* propDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );
	ASSERT_TRUE( propDef != nullptr );

	auto* li1 = sceneNode->getRoot()->find( "li1" )->asType<UIRichText>();
	auto* li2 = sceneNode->getRoot()->find( "li2" )->asType<UIRichText>();
	ASSERT_TRUE( li1 != nullptr );
	ASSERT_TRUE( li2 != nullptr );

	EXPECT_TRUE( li1->getPropertyString( propDef ) == "disclosure-closed" );
	EXPECT_TRUE( li2->getPropertyString( propDef ) == "disclosure-open" );

	Engine::destroySingleton();
}

UTEST( UILayout, listStyleShorthand ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<html>
			<ol>
				<li id="li1" style="list-style: decimal outside;">First</li>
				<li id="li2" style="list-style: lower-alpha inside;">Second</li>
				<li id="li3" style="list-style: none;">Third</li>
			</ol>
			<ul>
				<li id="li4" style="list-style: disc;">Bullet</li>
				<li id="li5" style="list-style: square outside;">Square</li>
				<li id="li6" style="list-style: circle;">Circle</li>
			</ul>
		</html>
	)html" );

	sceneNode->updateDirtyLayouts();

	const auto* typeDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );
	const auto* posDef = StyleSheetSpecification::instance()->getProperty( "list-style-position" );

	for ( const char* id : { "li1", "li2", "li3", "li4", "li5", "li6" } ) {
		auto* li = sceneNode->getRoot()->find( id )->asType<UIWidget>();
		ASSERT_TRUE( li != nullptr );
		EXPECT_TRUE( li->isType( UI_TYPE_HTML_LIST_ITEM ) );
	}

	EXPECT_TRUE( sceneNode->getRoot()->find( "li1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "decimal" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "li1" )->asType<UIRichText>()->getPropertyString(
					 posDef ) == "outside" );

	EXPECT_TRUE( sceneNode->getRoot()->find( "li2" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "lower-alpha" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "li2" )->asType<UIRichText>()->getPropertyString(
					 posDef ) == "inside" );

	EXPECT_TRUE( sceneNode->getRoot()->find( "li3" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "none" );

	EXPECT_TRUE( sceneNode->getRoot()->find( "li4" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "disc" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "li5" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "square" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "li5" )->asType<UIRichText>()->getPropertyString(
					 posDef ) == "outside" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "li6" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "circle" );

	Engine::destroySingleton();
}

UTEST( UILayout, listStyleInheritanceFromUl ) {
	init_ui_test();
	auto* sceneNode = SceneManager::instance()->getUISceneNode();
	sceneNode->loadLayoutFromString( R"html(
		<html>
			<head>
				<style>
					ul.a { list-style-type: circle; }
					ul.b { list-style-type: disc; }
					ul.c { list-style-type: square; }
					ol.d { list-style-type: decimal; }
					ol.h { list-style-type: upper-roman; }
				</style>
			</head>
			<body>
				<ol class="h">
					<li id="h1">Coffee</li>
				</ol>
				<ul class="a">
					<li id="a1">Coffee</li>
				</ul>
				<ul class="b">
					<li id="b1">Coffee</li>
				</ul>
				<ul class="c">
					<li id="c1">Coffee</li>
				</ul>
				<ol class="d">
					<li id="d1">Coffee</li>
				</ol>
			</body>
		</html>
	)html" );

	sceneNode->updateDirtyLayouts();

	const auto* typeDef = StyleSheetSpecification::instance()->getProperty( "list-style-type" );

	EXPECT_TRUE( sceneNode->getRoot()->find( "h1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "upper-roman" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "a1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "circle" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "b1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "disc" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "c1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "square" );
	EXPECT_TRUE( sceneNode->getRoot()->find( "d1" )->asType<UIRichText>()->getPropertyString(
					 typeDef ) == "decimal" );

	Engine::destroySingleton();
}

UTEST( UIHTMLDetails, lobstersInlineBlockCachesWidth ) {
	Engine::instance()->createWindow( WindowSettings( 424, 184, "HTML Details Lobsters Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( R"html(
		<!doctype html>
		<html>
		<head></head>
		<body>
			<div id="inside">
				<ol class="stories list">
					<li id="story_4g74mw" class="story">
						<div class="story_liner h-entry">
							<div class="voters">
								<a class="upvoter" href="/login">109</a>
							</div>
							<div class="details">
								<span role="heading" aria-level="1" class="link h-cite u-repost-of">
									<a class="u-url" href="https://ratfactor.com/ascetic-computing">Ascetic Computing</a>
								</span>
								<span class="tags">
									<a class="tag tag_practices" href="/t/practices">practices</a>
								</span>
								<a class="domain" href="/domains/ratfactor.com">ratfactor.com</a>
								<div class="byline">
									<a tabindex="-1" aria-hidden="true" href="/~jbauer"><img class="avatar" alt="jbauer avatar" src="/avatars/jbauer-16.png" width="16" height="16"></a>
									<span> via </span>
									<a class="u-author h-card" href="/~jbauer">jbauer</a>
									<time>20 hours ago</time>
									<span aria-hidden="true"> | </span>
									<details class="caches" name="caches">
										<summary>caches</summary>
										<ul>
											<li><a href="https://web.archive.org/">Archive.org</a></li>
											<li><a href="https://ghostarchive.org/">Ghostarchive</a></li>
										</ul>
									</details>
									<span class="comments_label">
										<span aria-hidden="true"> | </span>
										<a role="heading" aria-level="2" href="/s/4g74mw/ascetic_computing">17 comments</a>
									</span>
								</div>
							</div>
						</div>
					</li>
				</ol>
			</div>
		</body>
		</html>
	)html" ) );
	sceneNode->updateDirtyLayouts();

	sceneNode->combineStyleSheet( R"css(
		body, textarea, input, button {
			font-family: "helvetica neue", arial, sans-serif;
			line-height: 1.45em;
		}
		ol.stories {
			padding: 0;
			list-style: none;
			margin: 0;
		}
		div.voters {
			float: left;
			text-align: center;
			width: 40px;
		}
		li.story {
			clear: both;
		}
		ol.stories li.story div.story_liner {
			padding-top: 0.25em;
			padding-bottom: 0.25em;
			word-break: break-word;
		}
		li div.details {
			padding-top: 0.1em;
			margin-left: 32px;
		}
		li .link {
			font-weight: bold;
			vertical-align: middle;
		}
		li .link a {
			text-decoration: none;
		}
		li.story a.tag {
			vertical-align: middle;
		}
		li .tags {
			margin-right: 0.25em;
		}
		li .domain {
			font-style: italic;
			text-decoration: none;
			vertical-align: middle;
		}
		img.avatar {
			border-radius: 8px;
			height: 16px;
			margin-bottom: 2px;
			margin-right: 2px;
			vertical-align: middle;
			width: 16px;
		}
		li.story .byline {
			margin-top: 1px;
		}
		.caches {
			display: inline-block;
			position: relative;
		}
		.caches summary {
			list-style: none;
		}
		.caches ul {
			position: absolute;
			white-space: nowrap;
			list-style: none;
			padding: 0;
			z-index: 1;
		}
		.caches a {
			text-decoration: none;
			display: block;
			padding: 3px 7px;
		}
		@media only screen and (max-width: 480px) {
			div#inside {
				margin: 0.5rem;
			}
			ol.stories {
				margin: 0 0 0 -0.5rem;
				padding-left: 0;
			}
			div.voters {
				margin-left: 0.25em;
				margin-top: 0px;
				width: 30px;
			}
			ol.stories.list {
				margin-top: 0;
			}
			ol.stories.list li.story {
				display: table;
			}
			ol.stories.list li.story div.story_liner {
				display: table-cell;
				padding-top: 0.5em;
				padding-bottom: 0.75em;
				width: 100%;
			}
			li div.details {
				margin: 0 0 0 36px;
			}
		}
	)css" );
	sceneNode->updateDirtyLayouts();

	auto* caches = sceneNode->getRoot()->querySelector( ".caches" )->asType<UIHTMLDetails>();
	auto* comments = sceneNode->getRoot()->querySelector( ".comments_label" );
	auto* byline = sceneNode->getRoot()->querySelector( ".byline" );

	EXPECT_LT( caches->getPixelsSize().getWidth(), byline->getPixelsSize().getWidth() * 0.5f );
	EXPECT_NEAR( caches->getPixelsPosition().y, comments->getPixelsPosition().y, 1.f );

	Engine::destroySingleton();
}

UTEST( UIBorder, renderingVariations ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 656, "Border Rendering Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/border_tests.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-border-rendering", "html", 12 );

	Engine::destroySingleton();
}

UTEST( UIBorder, renderingVariations2 ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "Border Rendering Test 2", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( "assets/html/border_tests_2.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-border-rendering-2", "html", 12 );

	Engine::destroySingleton();
}

static UISceneNode* init_test_inline_block() {
	FontTrueType* font = nullptr;
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );
	font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	FontFamily::loadFromRegular( font );
	FontTrueType* monoFont = FontTrueType::New( "monospace" );
	monoFont->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	UISceneNode* sceneNode = UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	SceneManager::instance()->setCurrentUISceneNode( sceneNode );
	UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	themeManager->applyDefaultTheme( sceneNode->getRoot() );
	return sceneNode;
}

UTEST( UIHTML, InlineBlock ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 768, "Inline Block Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<!DOCTYPE html>
<html>
<head>
<style>
ul > li {
	display: inline-block;
	border: 1px solid red;
}
</style>
</head>
<body>
	<ul class="flat-list buttons">
		<li><a href="#">6 comments</a></li>
		<li><a class="post-sharing-button" href="#">share</a></li>
		<li><a href="#">save</a></li>
		<li><span><a href="#">hide</a></span></li>
		<li><a href="#">report</a></li>
		<li><a href="#">crosspost</a></li>
	</ul>
</body>
</html>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );

	auto ul = sceneNode->getRoot()->findByTag( "ul" );
	ASSERT_TRUE( ul != nullptr );

	// Force layout update
	sceneNode->update( Seconds( 1 ) );

	auto lis = ul->findAllByTag( "li" );
	EXPECT_EQ( lis.size(), (size_t)6 );

	for ( auto li : lis ) {
		EXPECT_EQ( li->asType<UIHTMLWidget>()->getDisplay(), CSSDisplay::InlineBlock );
		EXPECT_EQ( li->getLayoutWidthPolicy(), SizePolicy::WrapContent );
		EXPECT_GT( li->getPixelsSize().getWidth(), 0 );
		EXPECT_LT( li->getPixelsSize().getWidth(), ul->getPixelsSize().getWidth() );
		EXPECT_GT( li->getPixelsSize().getHeight(), 0 );
	}

	// Check if they are on the same line (inline-block)
	if ( lis.size() >= 2 ) {
		EXPECT_EQ( lis[0]->getPixelsPosition().y, lis[1]->getPixelsPosition().y );
		EXPECT_LT( lis[0]->getPixelsPosition().x, lis[1]->getPixelsPosition().x );
	}

	Engine::destroySingleton();
}

UTEST( UIHTML, BlockList ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 768, "Block List Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<ul id="block-list">
	<li style="height: 20px">Item 1</li>
	<li style="height: 20px">Item 2</li>
</ul>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto ul = sceneNode->getRoot()->findByTag( "ul" );
	ASSERT_TRUE( ul != nullptr );
	EXPECT_GT( ul->getPixelsSize().getWidth(), 0 );

	auto lis = ul->findAllByTag( "li" );
	EXPECT_EQ( lis.size(), (size_t)2 );

	for ( auto li : lis ) {
		EXPECT_EQ( li->asType<UIHTMLWidget>()->getDisplay(), CSSDisplay::ListItem );
		EXPECT_GT( li->getChildCount(), (size_t)0 );
		EXPECT_TRUE( li->asType<UIRichText>()->getRichTextPtr()->getFontStyleConfig().Font !=
					 nullptr );
		EXPECT_GT( li->asType<UIRichText>()->getRichTextPtr()->getFontStyleConfig().CharacterSize,
				   0 );
		EXPECT_GT( li->asType<UIRichText>()->getRichTextPtr()->getSize().getWidth(), 0 );
		EXPECT_GT( li->asType<UIRichText>()->getRichTextPtr()->getSize().getHeight(), 0 );
		EXPECT_GT( li->getPixelsSize().getWidth(), 0 );
		EXPECT_GT( li->getPixelsSize().getHeight(), 0 );
	}

	// They should be one above the other (block)
	EXPECT_LT( lis[0]->getPixelsPosition().y, lis[1]->getPixelsPosition().y );
	EXPECT_EQ( lis[0]->getPixelsPosition().x, lis[1]->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineList ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 768, "Inline List Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<ul style="display: block">
	<li style="display: inline">Item 1</li>
	<li style="display: inline">Item 2</li>
</ul>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto ul = sceneNode->getRoot()->findByTag( "ul" );
	ASSERT_TRUE( ul != nullptr );
	EXPECT_GT( ul->getPixelsSize().getWidth(), 0 );

	auto lis = ul->findAllByTag( "li" );
	EXPECT_EQ( lis.size(), (size_t)2 );

	for ( auto li : lis ) {
		EXPECT_EQ( li->asType<UIHTMLWidget>()->getDisplay(), CSSDisplay::Inline );
		EXPECT_EQ( li->getLayoutWidthPolicy(), SizePolicy::WrapContent );
		EXPECT_GT( li->getPixelsSize().getWidth(), 0 );
		EXPECT_LT( li->getPixelsSize().getWidth(), ul->getPixelsSize().getWidth() );
	}

	// They should be on the same line (inline)
	EXPECT_EQ( lis[0]->getPixelsPosition().y, lis[1]->getPixelsPosition().y );
	EXPECT_LT( lis[0]->getPixelsPosition().x, lis[1]->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineBlockExplicitWidth ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 768, "Inline Block Explicit Width Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<div style="width: 200px">
	<div id="d1" style="display: inline-block; width: 150px; height: 50px"></div>
	<div id="d2" style="display: inline-block; width: 150px; height: 50px"></div>
</div>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto d1 = sceneNode->getRoot()->find( "d1" )->asType<UIWidget>();
	auto d2 = sceneNode->getRoot()->find( "d2" )->asType<UIWidget>();
	ASSERT_TRUE( d1 != nullptr && d2 != nullptr );

	// They should NOT be on the same line because 150 + 150 > 200
	EXPECT_LT( d1->getPixelsPosition().y, d2->getPixelsPosition().y );
	EXPECT_EQ( d1->getPixelsPosition().x, d2->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineBlockMixedContent ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Inline Block Mixed Content Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<div>
	Some text
	<div id="ib" style="display: inline-block; width: 50px; height: 50px; background-color: red"></div>
	more text
</div>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto ib = sceneNode->getRoot()->find( "ib" )->asType<UIWidget>();
	ASSERT_TRUE( ib != nullptr );

	// The inline-block should have a non-zero position and be somewhat centered vertically if it
	// follows text flow
	EXPECT_GT( ib->getPixelsPosition().x, 0 );
	EXPECT_GT( ib->getPixelsSize().getWidth(), 0 );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineBlockWrapIssue ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Inline Block Wrap Issue Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	std::string html;
	FileSystem::fileGet( "assets/html/inline_block_wrap.html", html );

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto h2 = sceneNode->getRoot()->find( "h2-wrap" );
	ASSERT_TRUE( h2 != nullptr );

	auto rt = h2->asType<UIRichText>()->getRichTextPtr();

	EXPECT_EQ( (size_t)1, rt->getLines().size() );

	Engine::destroySingleton();
}

UTEST( UIHTML, InlineBlockBrowserTest ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Inline Block Browser Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	std::string html;
	FileSystem::fileGet( "assets/html/is_inline_block.html", html );

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	auto ib = sceneNode->getRoot()->find( "ib" )->asType<UIWidget>();
	auto t1 = sceneNode->getRoot()->find( "t1" )->asType<UIWidget>();
	auto t2 = sceneNode->getRoot()->find( "t2" )->asType<UIWidget>();

	ASSERT_TRUE( ib != nullptr && t1 != nullptr && t2 != nullptr );

	// If it drops to the next line:
	EXPECT_GT( ib->getPixelsPosition().y, t1->getPixelsPosition().y );
	// And t2 should be AFTER ib (either horizontally or vertically)
	EXPECT_GE( t2->getPixelsPosition().y,
			   ib->getPixelsPosition().y + ib->getPixelsSize().getHeight() );
	if ( t2->getPixelsPosition().y == ib->getPixelsPosition().y ) {
		EXPECT_GE( t2->getPixelsPosition().x,
				   ib->getPixelsPosition().x + ib->getPixelsSize().getWidth() );
	}
	EXPECT_EQ( ib->getPixelsPosition().x, t2->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( UIHTML, HeightExpansion ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Height Expansion Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/ensoft/" );

	std::string html;
	FileSystem::fileGet( "assets/html/ensoft/ensoft.html", html );

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );

	// Wait a bit and update again to make sure layouts are computed
	sceneNode->updateDirtyLayouts();

	auto htmlNode = sceneNode->getRoot()->findByType( UI_TYPE_HTML_HTML );
	auto bodyNode = sceneNode->getRoot()->findByType( UI_TYPE_HTML_BODY );

	ASSERT_TRUE( htmlNode != nullptr );
	ASSERT_TRUE( bodyNode != nullptr );

	auto htmlWidget = htmlNode->asType<UIWidget>();
	auto bodyWidget = bodyNode->asType<UIWidget>();

	EXPECT_GT( htmlWidget->getSize().getHeight(), 0 );
	EXPECT_GT( bodyWidget->getSize().getHeight(), 0 );

	EXPECT_GE( htmlWidget->getSize().getHeight(), bodyWidget->getSize().getHeight() );

	auto barNode = sceneNode->getRoot()->find( "bar" );
	ASSERT_TRUE( barNode != nullptr );

	auto barWidget = barNode->asType<UIWidget>();
	auto barHTML = barNode->asType<UIHTMLWidget>();

	EXPECT_EQ( barHTML->getCSSPosition(), CSSPosition::Fixed );

	EXPECT_NEAR( barWidget->getPixelsSize().getWidth(), 250.f, 1.f );

	auto rootWidget = sceneNode->getRoot();
	EXPECT_GT( rootWidget->getPixelsSize().getHeight(), 0 );
	EXPECT_NEAR( barWidget->getPixelsSize().getHeight(), rootWidget->getPixelsSize().getHeight(),
				 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTML, HeightExpansion_FixedDoesNotExpand ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Height Expansion Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	const std::string html = R"html(
<!DOCTYPE html>
<html>
<body style="margin: 0; padding: 0;">
    <div style="height: 100px;"></div>
    <div style="position: fixed; top: 500px; height: 50px;"></div>
</body>
</html>
)html";

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );
	sceneNode->updateDirtyLayouts();

	auto bodyNode = sceneNode->getRoot()->findByType( UI_TYPE_HTML_BODY );
	ASSERT_TRUE( bodyNode != nullptr );

	auto bodyWidget = bodyNode->asType<UIWidget>();

	// The height should be 100px, not 550px because the fixed div should be ignored.
	EXPECT_NEAR( bodyWidget->getPixelsSize().getHeight(), 100.f, 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTML, BodyHeightMiscalculationFixture ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Body Height Miscalculation Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/body_height_miscalculation.html", html );

	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	sceneNode->update( Seconds( 1 ) );
	sceneNode->updateDirtyLayouts();

	auto bodyNode = sceneNode->getRoot()->findByType( UI_TYPE_HTML_BODY );
	ASSERT_TRUE( bodyNode != nullptr );

	auto bodyWidget = bodyNode->asType<UIWidget>();

	EXPECT_GT( bodyWidget->getPixelsSize().getHeight(), 3000.f );
	EXPECT_LT( bodyWidget->getPixelsSize().getHeight(), 6000.f );

	Engine::destroySingleton();
}

UTEST( UIHTML, ContactFormLayout ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 653, "Contact Form Layout Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/ensoft/" );

	std::string html;
	FileSystem::fileGet( "assets/html/ensoft/contact.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );

	sceneNode->update( Seconds( 1 ) );
	sceneNode->updateDirtyLayouts();

	auto form = sceneNode->getRoot()->find( "form-contact" );
	ASSERT_TRUE( form != nullptr );
	auto formWidget = form->asType<UIWidget>();
	EXPECT_GT( formWidget->getPixelsSize().getHeight(), 0 );

	auto ul = formWidget->findByTag( "ul" );
	ASSERT_TRUE( ul != nullptr );
	auto ulWidget = ul->asType<UIWidget>();
	Float ulHeight = ulWidget->getPixelsSize().getHeight();
	EXPECT_GT( ulHeight, 0 );

	auto lis = ulWidget->findAllByTag( "li" );
	EXPECT_EQ( lis.size(), (size_t)7 );

	Float totalLiHeight = 0;
	int visibleLiCount = 0;
	for ( auto li : lis ) {
		Float liH = li->getPixelsSize().getHeight();
		if ( liH > 0 )
			visibleLiCount++;
		totalLiHeight += liH;
	}
	EXPECT_EQ( visibleLiCount, 6 );
	EXPECT_GT( totalLiHeight, 0 );
	EXPECT_NEAR( ulHeight, totalLiHeight, 1.f );

	auto contactBox = sceneNode->getRoot()->find( "contact-box" );
	ASSERT_TRUE( contactBox != nullptr );
	auto cbWidget = contactBox->asType<UIWidget>();
	EXPECT_GT( cbWidget->getPixelsSize().getHeight(), 10 );

	auto content = sceneNode->getRoot()->find( "content" );
	ASSERT_TRUE( content != nullptr );
	auto contentWidget = content->asType<UIWidget>();
	EXPECT_GT( contentWidget->getPixelsSize().getHeight(), 60 );

	auto bodyNode = sceneNode->getRoot()->findByType( UI_TYPE_HTML_BODY );
	ASSERT_TRUE( bodyNode != nullptr );
	auto bodyWidget = bodyNode->asType<UIWidget>();
	EXPECT_GT( bodyWidget->getPixelsSize().getHeight(), 0 );

	Engine::destroySingleton();
}

UTEST( UIBackground, imageAtlasPositioning ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 656, "Background Atlas Test", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/background_atlas.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	// Verify the atlas image was actually loaded — scan all nodes for a loaded drawable
	bool foundLoadedImage = false;
	sceneNode->getRoot()->forEachNode( [&foundLoadedImage]( Node* node ) {
		if ( foundLoadedImage || !node->isWidget() )
			return;
		auto* bg = node->asType<UIWidget>()->getBackground();
		if ( !bg )
			return;
		auto* layer = bg->getLayer( 0 );
		if ( layer && layer->getDrawable() ) {
			Sizef sz = layer->getDrawable()->getPixelsSize();
			if ( sz.getWidth() == 1024.f && sz.getHeight() == 512.f )
				foundLoadedImage = true;
		}
	} );
	ASSERT_TRUE( foundLoadedImage );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-background-atlas", "html", 4 );

	Engine::destroySingleton();
}

UTEST( UIBackground, imageAtlasPositioningPixelDensity2 ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "Background Atlas Test PD2", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	EE::Graphics::PixelDensity::setPixelDensity( 2.0f );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/background_atlas.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	compareImages( utest_state, utest_result, win, "eepp-ui-background-atlas-pd2", "html", 4 );

	Engine::destroySingleton();
	EE::Graphics::PixelDensity::setPixelDensity( 1.0f );
}

UTEST( UIBackground, InlineBlockImageSpans ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "inline-block image spans", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/inline_block.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );
	auto spans = sceneNode->getRoot()->querySelectorAll( "a > span" );

	EXPECT_GT( anchors.size(), (size_t)0 );
	EXPECT_GT( spans.size(), (size_t)0 );

	for ( auto anchor : anchors ) {
		EXPECT_GT( anchor->getPixelsSize().getWidth(), 0 );
		EXPECT_GT( anchor->getPixelsSize().getHeight(), 0 );
	}

	for ( auto span : spans ) {
		EXPECT_GT( span->getPixelsSize().getWidth(), 0 );
		EXPECT_GT( span->getPixelsSize().getHeight(), 0 );
	}

	compareImages( utest_state, utest_result, win, "eepp-ui-inline-block-image-spans", "html", 4 );

	Engine::destroySingleton();
}

UTEST( UIBackground, InlineBlockImageFixedSize ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "inline-block image fixed size", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/reddit_header_icons.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );

	EXPECT_GT( anchors.size(), (size_t)0 );

	for ( auto anchor : anchors ) {
		EXPECT_EQ( anchor->getPixelsSize().getWidth(), 15 );
		EXPECT_EQ( anchor->getPixelsSize().getHeight(), 12 );
	}

	Engine::destroySingleton();
}

UTEST( UIHTML, RedditHeaderPagenameBottomAlign ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "reddit header pagename bottom align", WindowStyle::Default,
						WindowBackend::Default, 32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/reddit_header.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->getInput()->update();
	SceneManager::instance()->update();

	auto* headerLeft = sceneNode->getRoot()->find( "header-bottom-left" )->asType<UIHTMLWidget>();
	auto* logo = sceneNode->getRoot()->find( "header-img" )->asType<UIHTMLWidget>();
	auto* page = sceneNode->getRoot()->querySelector( ".pagename" )->asType<UIHTMLWidget>();
	auto* jumpToContent = sceneNode->getRoot()->find( "jumpToContent" )->asType<UIHTMLWidget>();
	ASSERT_TRUE( headerLeft != nullptr );
	ASSERT_TRUE( logo != nullptr );
	ASSERT_TRUE( page != nullptr );
	ASSERT_TRUE( jumpToContent != nullptr );

	EXPECT_EQ( logo->getBaselineAlign().type, CSSBaselineAlignment::Bottom );
	EXPECT_EQ( page->getBaselineAlign().type, CSSBaselineAlignment::Bottom );
	EXPECT_NEAR( headerLeft->getPixelsSize().getHeight(),
				 page->getPixelsPosition().y + page->getPixelsSize().getHeight(), 0.5f );
	EXPECT_EQ( CSSPosition::Absolute, jumpToContent->getCSSPosition() );
	EXPECT_NEAR( -865.f, jumpToContent->convertToWorldSpace( { 0, 0 } ).x, 1.f );
	EXPECT_NEAR( 25.f, jumpToContent->convertToWorldSpace( { 0, 0 } ).y, 1.f );

	Engine::destroySingleton();
}

UTEST( UIHTML, AnchorsSizing ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "anchors sizing", WindowStyle::Default, WindowBackend::Default,
						32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/lobsters_simple.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	auto anchors = sceneNode->getRoot()->findAllByTag( "a" );

	EXPECT_GT( anchors.size(), (size_t)0 );

	for ( auto anchor : anchors ) {
		auto a = anchor->asType<UIAnchorSpan>();
		if ( a->getDisplay() == CSSDisplay::None )
			continue;
		EXPECT_GT( a->getPixelsSize().getHeight(), 0 );
		if ( !a->getText().empty() && a->getFontStyleConfig().Font ) {
			String text = a->getText();
			text.trim();
			if ( !text.empty() )
				EXPECT_GE( a->getPixelsSize().getWidth(),
						   Text::getTextWidth( text, a->getFontStyleConfig() ) );
		}
	}

	Engine::destroySingleton();
}

static UISceneNode* createWinAndLoadHTML( std::string winName, std::string htmlPath ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, winName, WindowStyle::Default, WindowBackend::Default, 32, {}, 1,
						false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	if ( font == nullptr || !font->loaded() )
		return nullptr;
	FontFamily::loadFromRegular( font );

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );
	std::string html;
	FileSystem::fileGet( htmlPath, html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	return sceneNode;
}

UTEST( UIHTML, blockFlow ) {
	auto sceneNode = createWinAndLoadHTML( "HTML Block Flow", "assets/html/block_flow.html" );
	ASSERT_TRUE( sceneNode != nullptr );
	auto sections = sceneNode->getRoot()->findAllByClass( "language-section" );

	ASSERT_EQ( sections.size(), (size_t)6 );

	// Each section is display block so we expect a single section per line
	// if sections position are not equal it means that some sections are floating
	Float ref = sections[0]->getPixelsPosition().x;
	for ( auto section : sections )
		EXPECT_EQ( section->getPixelsPosition().x, ref );

	Engine::destroySingleton();
}

UTEST( UIHTML, blockFlowFloat ) {
	auto sceneNode =
		createWinAndLoadHTML( "HTML Block Flow", "assets/html/block_flow_float_left.html" );
	ASSERT_TRUE( sceneNode != nullptr );
	auto sections = sceneNode->getRoot()->findAllByClass( "language-section" );

	ASSERT_EQ( sections.size(), (size_t)6 );

	// Each section is display block with float: left and width 48% so we expect two sections
	// per line, and each odd index should be to the right
	Float refLeft = sections[0]->getPixelsPosition().x;
	Float refRight = sections[1]->getPixelsPosition().x;
	for ( size_t idx = 0; idx < sections.size(); idx++ ) {
		Float expected = idx % 2 == 0 ? refLeft : refRight;
		EXPECT_EQ( sections[idx]->getPixelsPosition().x, expected );
	}
	Engine::destroySingleton();
}

UTEST( FontTrueType, glyphScaleZeroDimensionsNoCrash ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Glyph Scale Test",
													  WindowStyle::Default, WindowBackend::Default,
													  32, {}, 1, false, true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	ASSERT_TRUE( font != nullptr && font->loaded() );
	FontFamily::loadFromRegular( font );

	for ( unsigned int size : { 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u } ) {
		for ( Uint32 ch : { 'A', 'Z', 'a', 'z', '0', '9', '!', '@', '#', '$' } ) {
			const Glyph& glyph = font->getGlyph( ch, size, false, false, 0.f );
			(void)glyph;
		}
	}

	UI::UISceneNode* sceneNode = UI::UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	UI::UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );

	sceneNode->loadLayoutFromString( R"html(
		<vbox layout_width="match_parent" layout_height="match_parent">
			<TextView id="tiny" text="Tiny text" font_size="1" />
			<TextView id="small" text="Small text" font_size="3" />
			<TextView id="normal" text="Normal text" font_size="14" />
		</vbox>
	)html" );

	sceneNode->updateDirtyLayouts();

	auto tiny = sceneNode->getRoot()->find( "tiny" );
	auto small = sceneNode->getRoot()->find( "small" );
	auto normal = sceneNode->getRoot()->find( "normal" );

	ASSERT_TRUE( tiny != nullptr );
	ASSERT_TRUE( small != nullptr );
	ASSERT_TRUE( normal != nullptr );

	EXPECT_GT( normal->getPixelsSize().getWidth(), 0 );
	EXPECT_GT( normal->getPixelsSize().getHeight(), 0 );

	Engine::destroySingleton();
}

UTEST( UIHTML, LiFloatLeft ) {
	auto win = Engine::instance()->createWindow(
		WindowSettings( 1024, 653, "li float left", WindowStyle::Default, WindowBackend::Default,
						32, {}, 1, false, true ),
		ContextSettings( false, 0, 0, GLv_default, true, false ) );
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );

	UI::UISceneNode* sceneNode = init_test_inline_block();

	sceneNode->setURI( "file://" + Sys::getProcessPath() + "assets/html/" );

	std::string html;
	FileSystem::fileGet( "assets/html/float_li.html", html );
	sceneNode->loadLayoutFromString( HTMLFormatter::HTMLtoXML( html ) );
	win->setClearColor( Color::White );

	win->getInput()->update();
	SceneManager::instance()->update();

	win->clear();
	SceneManager::instance()->draw();
	win->display();

	auto livec = sceneNode->getRoot()->findAllByTag( "li" );

	ASSERT_GT( livec.size(), (size_t)0 );

	auto refY = livec[0]->getPixelsPosition().y;

	for ( size_t i = 1; i < livec.size(); i++ )
		EXPECT_NEAR( refY, livec[i]->getPixelsPosition().y, 1.f );

	Engine::destroySingleton();
}
