#include "utest.h"

#include <eepp/graphics/fontfamily.hpp>
#include <eepp/graphics/fonttruetype.hpp>
#include <eepp/scene/scenemanager.hpp>
#include <eepp/system/filesystem.hpp>
#include <eepp/ui/css/stylesheetparser.hpp>
#include <eepp/ui/css/stylesheetspecification.hpp>
#include <eepp/ui/flexlayouter.hpp>
#include <eepp/ui/tools/htmlformatter.hpp>
#include <eepp/ui/uihtmlwidget.hpp>
#include <eepp/ui/uirichtext.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uitextspan.hpp>
#include <eepp/ui/uithememanager.hpp>
#include <eepp/window/engine.hpp>
#include <eepp/window/window.hpp>

using namespace EE;
using namespace EE::UI;
using namespace EE::Window;
using namespace EE::Graphics;

static void init_flex_test() {
	FileSystem::changeWorkingDirectory( Sys::getProcessPath() );
	FontTrueType* font = FontTrueType::New( "NotoSans-Regular" );
	font->loadFromFile( "../assets/fonts/NotoSans-Regular.ttf" );
	FontFamily::loadFromRegular( font );
	UISceneNode* sceneNode = UISceneNode::New();
	SceneManager::instance()->add( sceneNode );
	SceneManager::instance()->setCurrentUISceneNode( sceneNode );
	UIThemeManager* themeManager = sceneNode->getUIThemeManager();
	themeManager->setDefaultFont( font );
	themeManager->applyDefaultTheme( sceneNode->getRoot() );
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 1: Item Collection
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, collectsInFlowChildren ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Both children should be positioned by the flex layouter
	EXPECT_GT( child1->getPixelsPosition().x, -1.f );
	EXPECT_GT( child2->getPixelsPosition().x, -1.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, skipsOutOfFlowChildren ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* normalChild = UIHTMLWidget::New();
	normalChild->setParent( flex );
	normalChild->setPixelsSize( 100, 50 );
	normalChild->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* absoluteChild = UIHTMLWidget::New();
	absoluteChild->setParent( flex );
	absoluteChild->setCSSPosition( CSSPosition::Absolute );
	absoluteChild->setPixelsSize( 100, 50 );
	absoluteChild->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	absoluteChild->setPixelsPosition( 0, 0 );

	sceneNode->updateDirtyLayouts();

	// Normal child should be laid out by flex; absolute should stay at its explicit position
	EXPECT_GT( normalChild->getPixelsPosition().x, -1.f );
	EXPECT_NEAR( absoluteChild->getPixelsPosition().x, 0.f, 1.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, skipsDisplayNoneChildren ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* visibleChild = UIHTMLWidget::New();
	visibleChild->setParent( flex );
	visibleChild->setPixelsSize( 100, 50 );
	visibleChild->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* hiddenChild = UIHTMLWidget::New();
	hiddenChild->setParent( flex );
	hiddenChild->setDisplay( CSSDisplay::None );
	hiddenChild->setPixelsSize( 100, 50 );
	hiddenChild->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Visible child should be laid out; hidden child should not affect layout
	EXPECT_GT( visibleChild->getPixelsPosition().x, -1.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, sortsByOrder ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* childA = UIHTMLWidget::New();
	childA->setParent( flex );
	childA->setPixelsSize( 100, 50 );
	childA->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	childA->applyProperty( StyleSheetProperty( "order", "2" ) );

	UIHTMLWidget* childB = UIHTMLWidget::New();
	childB->setParent( flex );
	childB->setPixelsSize( 100, 50 );
	childB->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	childB->applyProperty( StyleSheetProperty( "order", "1" ) );

	sceneNode->updateDirtyLayouts();

	// childB (order:1) should come before childA (order:2)
	EXPECT_LT( childB->getPixelsPosition().x, childA->getPixelsPosition().x );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 2: Direction and Wrap
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, defaultRowLayout ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Default row: children should be side-by-side
	EXPECT_NEAR( child1->getPixelsPosition().x, 0.f, 1.f );
	EXPECT_GT( child2->getPixelsPosition().x, child1->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( FlexContainer, rowReverseLayout ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-direction", "row-reverse" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Row-reverse: first child should be at the right
	EXPECT_GT( child1->getPixelsPosition().x, child2->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( FlexContainer, columnLayout ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-direction", "column" ) );
	flex->setPixelsSize( 500, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Column: children should be stacked vertically
	EXPECT_NEAR( child1->getPixelsPosition().x, 0.f, 1.f );
	EXPECT_GT( child2->getPixelsPosition().y, child1->getPixelsPosition().y );

	Engine::destroySingleton();
}

UTEST( FlexContainer, columnReverseLayout ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-direction", "column-reverse" ) );
	flex->setPixelsSize( 500, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Column-reverse: first child should be at the bottom
	EXPECT_GT( child1->getPixelsPosition().y, child2->getPixelsPosition().y );

	Engine::destroySingleton();
}

UTEST( FlexContainer, wrapCreatesMultipleLines ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-wrap", "wrap" ) );
	flex->setPixelsSize( 250, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// Three items of 100px width each in a 250px container should wrap to two lines
	for ( int i = 0; i < 3; i++ ) {
		UIHTMLWidget* child = UIHTMLWidget::New();
		child->setParent( flex );
		child->setPixelsSize( 100, 50 );
		child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	}

	sceneNode->updateDirtyLayouts();

	Node* firstChild = flex->getFirstChild();
	Node* lastChild = flex->getLastChild();
	ASSERT_TRUE( firstChild && firstChild->isWidget() );
	ASSERT_TRUE( lastChild && lastChild->isWidget() );

	// First and last child should be on different lines (different Y)
	EXPECT_GT( lastChild->asType<UIWidget>()->getPixelsPosition().y,
			   firstChild->asType<UIWidget>()->getPixelsPosition().y );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 3: Main-Axis Distribution (justify-content)
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, justifyContentFlexStart ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Default flex-start: child at left edge
	EXPECT_NEAR( child->getPixelsPosition().x, 0.f, 1.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, justifyContentCenter ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "justify-content", "center" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Center: child centered in 500px container
	EXPECT_NEAR( child->getPixelsPosition().x, 200.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, justifyContentFlexEnd ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "justify-content", "flex-end" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Flex-end: child at right edge
	EXPECT_NEAR( child->getPixelsPosition().x, 400.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, justifyContentSpaceBetween ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "justify-content", "space-between" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// space-between: first at start, last at end
	EXPECT_NEAR( child1->getPixelsPosition().x, 0.f, 5.f );
	EXPECT_NEAR( child2->getPixelsPosition().x, 400.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, justifyContentSpaceAround ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "justify-content", "space-around" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// space-around: equal space on both sides
	// Total space = 500 - 200 = 300. 2 slots = 300/2 = 150 each
	// child1 x = 150/2 = 75
	EXPECT_NEAR( child1->getPixelsPosition().x, 75.f, 10.f );
	EXPECT_NEAR( child2->getPixelsPosition().x, 325.f, 10.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, justifyContentSpaceEvenly ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "justify-content", "space-evenly" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// space-evenly: 3 equal gaps for 2 items
	// Total space = 500 - 200 = 300. 3 gaps = 300/3 = 100 each
	// child1 x = 100
	EXPECT_NEAR( child1->getPixelsPosition().x, 100.f, 10.f );
	EXPECT_NEAR( child2->getPixelsPosition().x, 300.f, 10.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 4: Cross-Axis Alignment (align-items)
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, alignItemsStretch ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "stretch" ) );
	flex->setPixelsSize( 500, 300 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::WrapContent );

	sceneNode->updateDirtyLayouts();

	// Stretch: child should fill the 300px cross axis (minus padding)
	EXPECT_GT( child->getPixelsSize().getHeight(), 200.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, alignItemsCenter ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "center" ) );
	flex->setPixelsSize( 500, 300 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Center: child centered vertically in 300px container
	EXPECT_NEAR( child->getPixelsPosition().y, 125.f, 10.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, alignItemsFlexStart ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "flex-start" ) );
	flex->setPixelsSize( 500, 300 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// flex-start: child at top
	EXPECT_NEAR( child->getPixelsPosition().y, 0.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, alignItemsFlexEnd ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "flex-end" ) );
	flex->setPixelsSize( 500, 300 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// flex-end: child at bottom
	EXPECT_NEAR( child->getPixelsPosition().y, 250.f, 5.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 5: align-self Override
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, alignSelfOverride ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "stretch" ) );
	flex->setPixelsSize( 500, 300 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::WrapContent );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child2->setStyleSheetProperty( StyleSheetProperty( "align-self", "center" ) );

	sceneNode->updateDirtyLayouts();

	// child1 should stretch, child2 should be centered
	EXPECT_GT( child1->getPixelsSize().getHeight(), 200.f );
	EXPECT_NEAR( child2->getPixelsPosition().y, 125.f, 10.f );
	EXPECT_NEAR( child2->getPixelsSize().getHeight(), 50.f, 5.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 6: Flexible Lengths (flex-grow, flex-shrink, flex-basis)
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, flexGrowDistributesSpace ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child1->setStyleSheetProperty( StyleSheetProperty( "flex-grow", "1" ) );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// child1 (flex-grow:1) should expand to fill remaining space
	EXPECT_GT( child1->getPixelsSize().getWidth(), 100.f );
	EXPECT_NEAR( child1->getPixelsSize().getWidth(), 400.f, 10.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, flexGrowProportional ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child1->setStyleSheetProperty( StyleSheetProperty( "flex-grow", "2" ) );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child2->setStyleSheetProperty( StyleSheetProperty( "flex-grow", "1" ) );

	sceneNode->updateDirtyLayouts();

	// Available: 500 - 200 = 300. child1 gets 2/3 (200), child2 gets 1/3 (100)
	// child1 width = 100 + 200 = 300, child2 width = 100 + 100 = 200
	EXPECT_NEAR( child1->getPixelsSize().getWidth(), 300.f, 15.f );
	EXPECT_NEAR( child2->getPixelsSize().getWidth(), 200.f, 15.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, flexShrinkContractsItems ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 300, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 200, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 200, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Total base 400 > container 300, items should shrink
	EXPECT_LT( child1->getPixelsSize().getWidth(), 200.f );
	EXPECT_LT( child2->getPixelsSize().getWidth(), 200.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, flexBasisFixed ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child->setStyleSheetProperty( StyleSheetProperty( "flex-basis", "200px" ) );

	sceneNode->updateDirtyLayouts();

	// flex-basis: 200px should be the starting size before flexing
	EXPECT_NEAR( child->getPixelsSize().getWidth(), 200.f, 10.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 7: Flex Shorthand
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, flexShorthand ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child->setStyleSheetProperty( StyleSheetProperty( "flex", "1 0 100px" ) );

	sceneNode->updateDirtyLayouts();

	// flex: 1 0 100px -> flex-grow:1, flex-shrink:0, flex-basis:100px
	// With 500px container, one item grows to fill: width = 100 + 400 = 500
	EXPECT_NEAR( child->getPixelsSize().getWidth(), 500.f, 10.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 8: Multi-Line Cross-Axis Distribution (align-content)
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, alignContentSpaceBetween ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-wrap", "wrap" ) );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-content", "space-between" ) );
	flex->setPixelsSize( 250, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// 3 items, 100px each, container 250px wide -> 2 lines
	for ( int i = 0; i < 3; i++ ) {
		UIHTMLWidget* child = UIHTMLWidget::New();
		child->setParent( flex );
		child->setPixelsSize( 100, 50 );
		child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	}

	sceneNode->updateDirtyLayouts();

	// space-between: first line at top, last line at bottom
	Node* first = flex->getFirstChild();
	Node* last = flex->getLastChild();
	ASSERT_TRUE( first && first->isWidget() );
	ASSERT_TRUE( last && last->isWidget() );

	EXPECT_NEAR( first->asType<UIWidget>()->getPixelsPosition().y, 0.f, 5.f );
	EXPECT_NEAR( last->asType<UIWidget>()->getPixelsPosition().y, 350.f, 10.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, alignContentCenter ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-wrap", "wrap" ) );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-content", "center" ) );
	flex->setPixelsSize( 250, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// 3 items, 100px each, container 250px wide -> 2 lines (100 + 100 = 200)
	for ( int i = 0; i < 3; i++ ) {
		UIHTMLWidget* child = UIHTMLWidget::New();
		child->setParent( flex );
		child->setPixelsSize( 100, 50 );
		child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	}

	sceneNode->updateDirtyLayouts();

	// Center: lines centered in 400px container
	// 2 lines of 50px each = 100px. Free = 300px. Offset = 150px
	Node* first = flex->getFirstChild();
	ASSERT_TRUE( first && first->isWidget() );
	EXPECT_NEAR( first->asType<UIWidget>()->getPixelsPosition().y, 150.f, 20.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, alignContentStretch ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-wrap", "wrap" ) );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-content", "stretch" ) );
	flex->setPixelsSize( 250, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// 3 items, 100px each, container 250px wide -> 2 lines
	for ( int i = 0; i < 3; i++ ) {
		UIHTMLWidget* child = UIHTMLWidget::New();
		child->setParent( flex );
		child->setPixelsSize( 100, 50 );
		child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::WrapContent );
	}

	sceneNode->updateDirtyLayouts();

	// Stretch: lines stretched to fill container cross size
	// 400px / 2 lines = 200px per line. Items should be stretched.
	Node* child = flex->getFirstChild();
	ASSERT_TRUE( child && child->isWidget() );
	EXPECT_GT( child->asType<UIWidget>()->getPixelsSize().getHeight(), 100.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 9: Gap Properties
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, gapRowColumn ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "column-gap", "20px" ) );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// With 20px gap, child2 should start at 100 + 20 = 120
	EXPECT_NEAR( child2->getPixelsPosition().x, 120.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, gapShorthand ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "gap", "10px 30px" ) );
	flex->setPixelsSize( 500, 400 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// gap: 10px 30px -> row-gap=10px, column-gap=30px
	EXPECT_NEAR( child2->getPixelsPosition().x, 130.f, 5.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 10: Inline-Flex
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, inlineFlexShrinkWraps ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::InlineFlex );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Inline-flex should shrink-wrap to content width
	EXPECT_NEAR( flex->getPixelsSize().getWidth(), 100.f, 20.f );
	EXPECT_EQ( flex->getLayoutWidthPolicy(), SizePolicy::WrapContent );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 11: Edge Cases
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, emptyContainer ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Empty flex container should not crash and should have padding-only size
	EXPECT_GT( flex->getPixelsSize().getWidth(), 0.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, nestedFlexContainer ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* outer = UIHTMLWidget::New();
	outer->setParent( sceneNode->getRoot() );
	outer->setDisplay( CSSDisplay::Flex );
	outer->setPixelsSize( 500, 200 );
	outer->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* inner = UIHTMLWidget::New();
	inner->setParent( outer );
	inner->setDisplay( CSSDisplay::Flex );
	inner->setPixelsSize( 200, 100 );
	inner->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( inner );
	child->setPixelsSize( 50, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Nested flex should work without crashing
	EXPECT_GT( child->getPixelsPosition().x, -1.f );
	EXPECT_GT( child->getPixelsPosition().y, -1.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, negativeOrder ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child1 = UIHTMLWidget::New();
	child1->setParent( flex );
	child1->setPixelsSize( 100, 50 );
	child1->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child1->setStyleSheetProperty( StyleSheetProperty( "order", "0" ) );

	UIHTMLWidget* child2 = UIHTMLWidget::New();
	child2->setParent( flex );
	child2->setPixelsSize( 100, 50 );
	child2->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child2->setStyleSheetProperty( StyleSheetProperty( "order", "-1" ) );

	sceneNode->updateDirtyLayouts();

	// child2 (order:-1) should come before child1 (order:0)
	EXPECT_LT( child2->getPixelsPosition().x, child1->getPixelsPosition().x );

	Engine::destroySingleton();
}

UTEST( FlexContainer, fixedWidthItem ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 150, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child->applyProperty( StyleSheetProperty( "width", "150px" ) );
	child->setLayoutWidthPolicy( SizePolicy::Fixed );

	sceneNode->updateDirtyLayouts();

	// Fixed width should be respected
	EXPECT_NEAR( child->getPixelsSize().getWidth(), 150.f, 5.f );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 12: Blockification / RichText Integration
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, blockifiesInlineChildren ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// Create an inline span as a child of the flex container
	UITextSpan* span = UITextSpan::New();
	span->setParent( flex );
	span->setText( "Hello Flex" );
	span->applyProperty( StyleSheetProperty( "display", "inline" ) );

	sceneNode->updateDirtyLayouts();

	// Inline child of flex should be blockified and get a non-zero size from BlockLayouter
	ASSERT_TRUE( span->isType( UI_TYPE_HTML_WIDGET ) );
	auto* layouter = span->asType<UIHTMLWidget>()->getLayouter();
	ASSERT_TRUE( layouter != nullptr );
	// The fact that the span got laid out (non-zero size) proves blockification worked.
	// Without blockification, InlineLayouter::updateLayout() is empty, so size stays 0.
	EXPECT_GT( span->getPixelsSize().getWidth(), 0.f );
	EXPECT_GT( span->getPixelsSize().getHeight(), 0.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, textSpanInsideFlexGetsSize ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	UITextSpan* span = UITextSpan::New();
	span->setParent( flex );
	span->setText( "Hello" );
	span->applyProperty( StyleSheetProperty( "display", "inline" ) );

	sceneNode->updateDirtyLayouts();

	// The text span should get a non-zero size from blockified layout
	EXPECT_GT( span->getPixelsSize().getWidth(), 0.f );
	EXPECT_GT( span->getPixelsSize().getHeight(), 0.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, flexDirectionColumnWithTextContent ) {
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setStyleSheetProperty( StyleSheetProperty( "flex-direction", "column" ) );

	UITextSpan* span = UITextSpan::New();
	span->setParent( flex );
	span->setText( "Line One" );

	UITextSpan* span2 = UITextSpan::New();
	span2->setParent( flex );
	span2->setText( "Line Two" );

	sceneNode->updateDirtyLayouts();

	// Column with text items should stack vertically and compute height
	EXPECT_GT( flex->getPixelsSize().getHeight(), 0.f );
	EXPECT_GT( span2->getPixelsPosition().y, span->getPixelsPosition().y );

	Engine::destroySingleton();
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 13: Property Parsing
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexProperties, flexFlowShorthand ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "flex-flow" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = spec->getShorthandParser( "flex-flow" )( shorthand, "row-reverse wrap" );
	ASSERT_EQ( props.size(), (size_t)2 );
	EXPECT_TRUE( props[0].asString() == "row-reverse" );
	EXPECT_TRUE( props[1].asString() == "wrap" );
}

UTEST( FlexProperties, gapShorthandTwoValues ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "gap" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = shorthand->parse( "10px 20px" );
	ASSERT_EQ( props.size(), (size_t)2 );
	EXPECT_TRUE( props[0].asString() == "10px" );
	EXPECT_TRUE( props[1].asString() == "20px" );
}

UTEST( FlexProperties, gapShorthandOneValue ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "gap" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = shorthand->parse( "15px" );
	ASSERT_EQ( props.size(), (size_t)2 );
	EXPECT_TRUE( props[0].asString() == "15px" );
	EXPECT_TRUE( props[1].asString() == "15px" );
}

UTEST( FlexProperties, flexShorthandAuto ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "flex" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = spec->getShorthandParser( "flex" )( shorthand, "auto" );
	ASSERT_EQ( props.size(), (size_t)3 );
	EXPECT_TRUE( props[0].asString() == "1" );	  // flex-grow
	EXPECT_TRUE( props[1].asString() == "1" );	  // flex-shrink
	EXPECT_TRUE( props[2].asString() == "auto" ); // flex-basis
}

UTEST( FlexProperties, flexShorthandNone ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "flex" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = spec->getShorthandParser( "flex" )( shorthand, "none" );
	ASSERT_EQ( props.size(), (size_t)3 );
	EXPECT_TRUE( props[0].asString() == "0" );	  // flex-grow
	EXPECT_TRUE( props[1].asString() == "0" );	  // flex-shrink
	EXPECT_TRUE( props[2].asString() == "auto" ); // flex-basis
}

UTEST( FlexProperties, flexShorthandOneNumber ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "flex" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = spec->getShorthandParser( "flex" )( shorthand, "2" );
	ASSERT_EQ( props.size(), (size_t)3 );
	EXPECT_TRUE( props[0].asString() == "2" );	// flex-grow
	EXPECT_TRUE( props[1].asString() == "1" );	// flex-shrink
	EXPECT_TRUE( props[2].asString() == "0%" ); // flex-basis
}

UTEST( FlexProperties, flexShorthandThreeValues ) {
	auto* spec = CSS::StyleSheetSpecification::instance();

	const auto* shorthand = spec->getShorthand( "flex" );
	ASSERT_TRUE( shorthand != nullptr );

	auto props = spec->getShorthandParser( "flex" )( shorthand, "1 0 200px" );
	ASSERT_EQ( props.size(), (size_t)3 );
	EXPECT_TRUE( props[0].asString() == "1" );	   // flex-grow
	EXPECT_TRUE( props[1].asString() == "0" );	   // flex-shrink
	EXPECT_TRUE( props[2].asString() == "200px" ); // flex-basis
}

UTEST( FlexProperties, enumConversions ) {
	EXPECT_EQ( CSSFlexDirectionHelper::fromString( "row" ), CSSFlexDirection::Row );
	EXPECT_EQ( CSSFlexDirectionHelper::fromString( "row-reverse" ), CSSFlexDirection::RowReverse );
	EXPECT_EQ( CSSFlexDirectionHelper::fromString( "column" ), CSSFlexDirection::Column );
	EXPECT_EQ( CSSFlexDirectionHelper::fromString( "column-reverse" ),
			   CSSFlexDirection::ColumnReverse );

	EXPECT_EQ( CSSFlexWrapHelper::fromString( "nowrap" ), CSSFlexWrap::NoWrap );
	EXPECT_EQ( CSSFlexWrapHelper::fromString( "wrap" ), CSSFlexWrap::Wrap );
	EXPECT_EQ( CSSFlexWrapHelper::fromString( "wrap-reverse" ), CSSFlexWrap::WrapReverse );

	EXPECT_EQ( CSSJustifyContentHelper::fromString( "flex-start" ), CSSJustifyContent::FlexStart );
	EXPECT_EQ( CSSJustifyContentHelper::fromString( "center" ), CSSJustifyContent::Center );
	EXPECT_EQ( CSSJustifyContentHelper::fromString( "space-between" ),
			   CSSJustifyContent::SpaceBetween );
	EXPECT_EQ( CSSJustifyContentHelper::fromString( "space-around" ),
			   CSSJustifyContent::SpaceAround );
	EXPECT_EQ( CSSJustifyContentHelper::fromString( "space-evenly" ),
			   CSSJustifyContent::SpaceEvenly );

	EXPECT_EQ( CSSAlignItemsHelper::fromString( "stretch" ), CSSAlignItems::Stretch );
	EXPECT_EQ( CSSAlignItemsHelper::fromString( "center" ), CSSAlignItems::Center );
	EXPECT_EQ( CSSAlignItemsHelper::fromString( "flex-start" ), CSSAlignItems::FlexStart );
	EXPECT_EQ( CSSAlignItemsHelper::fromString( "flex-end" ), CSSAlignItems::FlexEnd );
	EXPECT_EQ( CSSAlignItemsHelper::fromString( "baseline" ), CSSAlignItems::Baseline );

	EXPECT_EQ( CSSAlignContentHelper::fromString( "stretch" ), CSSAlignContent::Stretch );
	EXPECT_EQ( CSSAlignContentHelper::fromString( "center" ), CSSAlignContent::Center );
	EXPECT_EQ( CSSAlignContentHelper::fromString( "space-between" ),
			   CSSAlignContent::SpaceBetween );
	EXPECT_EQ( CSSAlignContentHelper::fromString( "space-evenly" ), CSSAlignContent::SpaceEvenly );

	EXPECT_EQ( CSSAlignSelfHelper::fromString( "auto" ), CSSAlignSelf::Auto );
	EXPECT_EQ( CSSAlignSelfHelper::fromString( "stretch" ), CSSAlignSelf::Stretch );
	EXPECT_EQ( CSSAlignSelfHelper::fromString( "center" ), CSSAlignSelf::Center );
}

// ─────────────────────────────────────────────────────────────────────────────
// Phase 14: Flex Algorithm Bug Fixes (G1, G2, G4, G5)
// ─────────────────────────────────────────────────────────────────────────────

UTEST( FlexContainer, iterativeFlexResolutionWithMinWidths ) {
	// G1: When a flex item hits its min-width, remaining free space is
	// redistributed to other items (iterative §9.7 algorithm).
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 500, 100 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );

	// Three items with flex: 1 1 0% (equal flex-grow, zero base size)
	UIHTMLWidget* a = UIHTMLWidget::New();
	a->setParent( flex );
	a->setPixelsSize( 10, 50 );
	a->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	a->setStyleSheetProperty( StyleSheetProperty( "flex", "1 1 0%" ) );

	UIHTMLWidget* b = UIHTMLWidget::New();
	b->setParent( flex );
	b->setPixelsSize( 10, 50 );
	b->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	b->setStyleSheetProperty( StyleSheetProperty( "flex", "1 1 0%" ) );

	// c has min-width:200px, so it should stay at 200 while a and b split
	// the remaining 300px → 150px each
	UIHTMLWidget* c = UIHTMLWidget::New();
	c->setParent( flex );
	c->setPixelsSize( 10, 50 );
	c->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	c->setStyleSheetProperty( StyleSheetProperty( "flex", "1 1 0%" ) );
	c->setStyleSheetProperty( StyleSheetProperty( "min-width", "200px" ) );

	sceneNode->updateDirtyLayouts();

	EXPECT_NEAR( a->getPixelsSize().getWidth(), 150.f, 5.f );
	EXPECT_NEAR( b->getPixelsSize().getWidth(), 150.f, 5.f );
	EXPECT_NEAR( c->getPixelsSize().getWidth(), 200.f, 5.f );

	Engine::destroySingleton();
}

UTEST( FlexContainer, crossAxisAutoMargins ) {
	// G4: margin: auto on the cross axis should center/position the item
	// within the line before align-self applies (§8.1).
	Engine::instance()->createWindow( WindowSettings( 1024, 650, "Flex Test", WindowStyle::Default,
													  WindowBackend::Default, 32, {}, 1, false,
													  true ),
									  ContextSettings( false, 0, 0, GLv_default, true, false ) );
	init_flex_test();
	UISceneNode* sceneNode = SceneManager::instance()->getUISceneNode();

	// Row-direction flex container with fixed height
	UIHTMLWidget* flex = UIHTMLWidget::New();
	flex->setParent( sceneNode->getRoot() );
	flex->setDisplay( CSSDisplay::Flex );
	flex->setPixelsSize( 400, 200 );
	flex->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	flex->setStyleSheetProperty( StyleSheetProperty( "align-items", "flex-start" ) );

	// Single item with margin-top: auto and margin-bottom: auto
	UIHTMLWidget* child = UIHTMLWidget::New();
	child->setParent( flex );
	child->setPixelsSize( 100, 50 );
	child->setLayoutSizePolicy( SizePolicy::Fixed, SizePolicy::Fixed );
	child->setStyleSheetProperty( StyleSheetProperty( "margin", "auto" ) );

	sceneNode->updateDirtyLayouts();

	// With both margin-top and margin-bottom as auto, the item should be
	// vertically centered. Container is 200px, item is 50px.
	// Item Y should be (200 - 50) / 2 = 75
	// (Flex container has no padding in this setup)
	EXPECT_NEAR( child->getPixelsPosition().y, 75.f, 5.f );

	Engine::destroySingleton();
}
