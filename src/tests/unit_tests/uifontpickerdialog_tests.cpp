#include "utest.hpp"
#include <eepp/system/filesystem.hpp>
#include <eepp/system/threadpool.hpp>
#include <eepp/ui/tools/uifontpickerdialog.hpp>
#include <eepp/ui/uiapplication.hpp>
#include <eepp/ui/uilistview.hpp>
#include <eepp/ui/uipushbutton.hpp>
#include <eepp/ui/uiscenenode.hpp>

using namespace EE;
using namespace EE::UI;
using namespace EE::UI::Tools;

template <typename Predicate> static void pumpUntil( UISceneNode* sceneNode, Predicate predicate ) {
	for ( size_t i = 0; i < 1000 && !predicate(); i++ ) {
		sceneNode->update( Time::Zero );
		Sys::sleep( Milliseconds( 5 ) );
	}
}

UTEST( UIFontPickerDialog, PreselectsExternalFontPath ) {
	UIApplication app(
		WindowSettings( 320, 240, "eepp - UIFontPickerDialog Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), 1 ) );

	const std::string fontPath = Sys::getProcessPath() + "assets/fonts/NotoSansKR-Regular.ttf";
	ASSERT_TRUE( FileSystem::fileExists( fontPath ) );

	UIFontPickerDialog* dialog = UIFontPickerDialog::New();
	dialog->setSelectedFont( fontPath );

	EXPECT_STDSTREQ( fontPath, dialog->getSelection().font.path );
	EXPECT_FALSE( dialog->getSelection().font.family.empty() );
	EXPECT_FALSE( dialog->getFamilyList()->getSelection().isEmpty() );
	EXPECT_FALSE( dialog->getStyleList()->getSelection().isEmpty() );

	UIFontPickerDialog* selectionDialog = UIFontPickerDialog::New();
	UIFontSelection selection;
	selection.font.path = fontPath;
	selectionDialog->setSelection( selection );

	EXPECT_STDSTREQ( fontPath, selectionDialog->getSelection().font.path );
	EXPECT_FALSE( selectionDialog->getSelection().font.family.empty() );
}

UTEST( UIFontPickerDialog, AsyncLoadPreservesExternalFontPreselection ) {
	UIApplication app(
		WindowSettings( 320, 240, "eepp - UIFontPickerDialog Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), 1 ) );
	app.getUI()->setThreadPool( ThreadPool::createShared( 1 ) );

	const std::string fontPath = Sys::getProcessPath() + "assets/fonts/NotoSansKR-Regular.ttf";
	ASSERT_TRUE( FileSystem::fileExists( fontPath ) );

	UIFontPickerDialog* dialog = UIFontPickerDialog::New();
	EXPECT_FALSE( dialog->getButtonOK()->isEnabled() );

	dialog->setSelectedFont( fontPath );
	pumpUntil( app.getUI(), [dialog] { return dialog->getButtonOK()->isEnabled(); } );

	EXPECT_TRUE( dialog->getButtonOK()->isEnabled() );
	EXPECT_STDSTREQ( fontPath, dialog->getSelection().font.path );
	EXPECT_FALSE( dialog->getSelection().font.family.empty() );
	EXPECT_FALSE( dialog->getFamilyList()->getSelection().isEmpty() );
	EXPECT_FALSE( dialog->getStyleList()->getSelection().isEmpty() );
}

UTEST( UIFontPickerDialog, ApplyButtonEmitsOnApply ) {
	UIApplication app(
		WindowSettings( 320, 240, "eepp - UIFontPickerDialog Test", WindowStyle::Default,
						WindowBackend::Default, 32 ),
		UIApplication::Settings( Sys::getProcessPath() + ".." + FileSystem::getOSSlash(), 1 ) );

	UIFontPickerDialog* dialog = UIFontPickerDialog::New( UIFontPickerDialog::DefaultFlags |
														  UIFontPickerDialog::ShowApplyButton );
	bool applied = false;
	bool picked = false;
	bool confirmed = false;
	dialog->on( Event::OnApply, [&applied]( const Event* ) { applied = true; } );
	dialog->on( Event::OnConfirm, [&confirmed]( const Event* ) { confirmed = true; } );
	dialog->setOnFontPicked( [&picked]( const UIFontSelection& ) { picked = true; } );

	dialog->getButtonApply()->sendCommonEvent( Event::MouseClick );

	EXPECT_TRUE( applied );
	EXPECT_TRUE( picked );
	EXPECT_FALSE( confirmed );
}
