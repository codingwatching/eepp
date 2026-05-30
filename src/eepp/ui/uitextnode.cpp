#include <eepp/graphics/text.hpp>
#include <eepp/ui/uirichtext.hpp>
#include <eepp/ui/uistyle.hpp>
#include <eepp/ui/uitextnode.hpp>

namespace EE { namespace UI {

UITextNode* UITextNode::New() {
	return eeNew( UITextNode, () );
}

UITextNode::UITextNode() : UIWidget( "textnode" ) {
	mNodeFlags |= NODE_FLAG_TEXTNODE;
	mNodeFlags &= ~NODE_FLAG_OVER_FIND_ALLOWED;
	mFlags |= UI_HTML_ELEMENT;
}

UITextNode::~UITextNode() {}

Uint32 UITextNode::getType() const {
	return UI_TYPE_TEXTNODE;
}

bool UITextNode::isType( const Uint32& type ) const {
	return UITextNode::getType() == type ? true : UIWidget::isType( type );
}

void UITextNode::draw() {
	if ( mText.empty() )
		return;

	// In flex layout, text nodes are anonymous flex items. The parent flex
	// container does not render their text via RichText (it was cleared),
	// so the text node must draw itself.
	Node* parent = getParent();
	bool isFlexItem =
		parent && parent->isType( UI_TYPE_HTML_WIDGET ) && parent->asType<UIHTMLWidget>()->isFlex();
	if ( isFlexItem ) {
		// Walk up parent chain to find a UIRichText ancestor with font info
		Node* n = parent;
		while ( n ) {
			if ( n->isType( UI_TYPE_RICHTEXT ) ) {
				auto& fontConfig = n->asType<UIRichText>()->getRichText().getFontStyleConfig();
				if ( fontConfig.Font ) {
					FontStyleConfig fc = fontConfig;
					Float alpha = getAlpha();
					if ( alpha < 1.f )
						fc.FontColor.a = (Uint8)( (Float)fc.FontColor.a * alpha );
					Text::draw( mText, mScreenPos.trunc(), fc );
				}
				break;
			}
			n = n->getParent();
		}
	}
}

std::vector<PropertyId> UITextNode::getPropertiesImplemented() const {
	auto props = UIWidget::getPropertiesImplemented();
	auto local = { PropertyId::Text };
	props.insert( props.end(), local.begin(), local.end() );
	return props;
}

std::string UITextNode::getPropertyString( const PropertyDefinition* propertyDef,
										   const Uint32& propertyIndex ) const {
	if ( NULL == propertyDef )
		return "";

	switch ( propertyDef->getPropertyId() ) {
		case PropertyId::Text:
			return mText;
		default:
			break;
	}

	const StyleSheetProperty* prop = getUIStyle()->getProperty( propertyDef->getPropertyId() );
	if ( prop )
		return prop->value();

	if ( propertyDef->isInherited() && getParent() && getParent()->isWidget() )
		return static_cast<UIWidget*>( getParent() )
			->getPropertyString( propertyDef, propertyIndex );

	return UIWidget::getPropertyString( propertyDef, propertyIndex );
}

const String& UITextNode::getText() const {
	return mText;
}

void UITextNode::setText( const String& text ) {
	if ( mText != text ) {
		mText = text;
		notifyLayoutAttrChange();
	}
}

bool UITextNode::isWhitespaceOnly() const {
	for ( char c : mText ) {
		if ( c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\v' )
			return false;
	}
	return true;
}

}} // namespace EE::UI
