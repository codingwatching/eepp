#include <eepp/core/memorymanager.hpp>
#include <eepp/ui/blocklayouter.hpp>
#include <eepp/ui/flexlayouter.hpp>
#include <eepp/ui/inlinelayouter.hpp>
#include <eepp/ui/nonelayouter.hpp>
#include <eepp/ui/tablelayouter.hpp>
#include <eepp/ui/uilayouter.hpp>
#include <eepp/ui/uilayoutermanager.hpp>
#include <eepp/ui/uitextspan.hpp>

namespace EE { namespace UI {

static bool parentIsFlexContainer( UIWidget* widget ) {
	Node* parent = widget->getParent();
	if ( !parent || !parent->isWidget() )
		return false;
	UIWidget* parentWidget = parent->asType<UIWidget>();
	if ( !parentWidget->isType( UI_TYPE_HTML_WIDGET ) )
		return false;
	CSSDisplay parentDisplay = parentWidget->asType<UIHTMLWidget>()->getDisplay();
	return parentDisplay == CSSDisplay::Flex || parentDisplay == CSSDisplay::InlineFlex;
}

UILayouter* UILayouterManager::create( CSSDisplay display, UIWidget* container ) {
	// Blockification per CSS Flexbox §4: children of flex containers are block-level
	if ( parentIsFlexContainer( container ) ) {
		return eeNew( BlockLayouter, ( container ) );
	}

	switch ( display ) {
		case CSSDisplay::Block:
		case CSSDisplay::TableCell:
		case CSSDisplay::InlineBlock:
		case CSSDisplay::ListItem:
			return eeNew( BlockLayouter, ( container ) );
		case CSSDisplay::Flex:
		case CSSDisplay::InlineFlex:
			return eeNew( FlexLayouter, ( container ) );
		case CSSDisplay::Inline:
			if ( container->isType( UI_TYPE_TEXTSPAN ) )
				return eeNew( InlineLayouter, ( container ) );
			return eeNew( BlockLayouter, ( container ) );
		case CSSDisplay::Table:
			return eeNew( TableLayouter, ( container ) );
		case CSSDisplay::None:
			return eeNew( NoneLayouter, ( container ) );
		default:
			return nullptr;
	}
}

}} // namespace EE::UI
