#include <cmath>
#include <eepp/graphics/fontmanager.hpp>
#include <eepp/graphics/primitives.hpp>
#include <eepp/graphics/text.hpp>
#include <eepp/scene/scenemanager.hpp>
#include <eepp/system/scopedop.hpp>
#include <eepp/ui/css/propertydefinition.hpp>
#include <eepp/ui/css/stylesheetparser.hpp>
#include <eepp/ui/uiborderdrawable.hpp>
#include <eepp/ui/uicodeeditor.hpp>
#include <eepp/ui/uilayouter.hpp>
#include <eepp/ui/uinodedrawable.hpp>
#include <eepp/ui/uirichtext.hpp>
#include <eepp/ui/uiscenenode.hpp>
#include <eepp/ui/uistyle.hpp>
#include <eepp/ui/uitextnode.hpp>
#include <eepp/ui/uitextspan.hpp>
#include <eepp/ui/uithememanager.hpp>
#include <eepp/ui/uiwidgetcreator.hpp>

#define PUGIXML_HEADER_ONLY
#include <pugixml/pugixml.hpp>

namespace EE { namespace UI {

UIRichText::WhiteSpaceCollapse UIRichText::toWhiteSpaceCollapse( std::string val ) {
	String::toLowerInPlace( val );
	if ( "preserve" == val )
		return WhiteSpaceCollapse::Preserve;
	if ( "preserve-breaks" == val || "preserve-breaks" == val )
		return WhiteSpaceCollapse::PreserveBreaks;
	if ( "preserve-spaces" == val || "preserve-spaces" == val )
		return WhiteSpaceCollapse::PreserveSpaces;
	if ( "break-spaces" == val || "break-spaces" == val )
		return WhiteSpaceCollapse::BreakSpaces;
	return WhiteSpaceCollapse::Collapse;
}

std::string UIRichText::fromWhiteSpaceCollapse( WhiteSpaceCollapse val ) {
	switch ( val ) {
		case WhiteSpaceCollapse::Preserve:
			return "preserve";
		case WhiteSpaceCollapse::PreserveBreaks:
			return "preserve-breaks";
		case WhiteSpaceCollapse::PreserveSpaces:
			return "preserve-spaces";
		case WhiteSpaceCollapse::BreakSpaces:
			return "break-spaces";
		case WhiteSpaceCollapse::Collapse:
		default:
			return "collapse";
	}
}

std::string UIRichText::fromWhiteSpace( WhiteSpaceCollapse collapse, bool lineWrap ) {
	switch ( collapse ) {
		case WhiteSpaceCollapse::Preserve:
			return lineWrap ? "pre-wrap" : "pre";
		case WhiteSpaceCollapse::PreserveBreaks:
			return "pre-line";
		case WhiteSpaceCollapse::BreakSpaces:
			return "break-spaces";
		case WhiteSpaceCollapse::PreserveSpaces:
			return lineWrap ? "preserve-spaces" : "preserve nowrap";
		case WhiteSpaceCollapse::Collapse:
		default:
			return lineWrap ? "normal" : "nowrap";
	}
}

UIHTMLHtml* UIHTMLHtml::New( const std::string& tag ) {
	return eeNew( UIHTMLHtml, ( tag ) );
}

UIHTMLHtml::UIHTMLHtml( const std::string& tag ) : UIRichText( tag ) {
	enableReportSizeChangeToChildren();
	getUISceneNode()->loadHTMLBaseCSS();
}

Uint32 UIHTMLHtml::getType() const {
	return UI_TYPE_HTML_HTML;
}

bool UIHTMLHtml::isType( const Uint32& type ) const {
	return UIHTMLHtml::getType() == type ? true : UIRichText::isType( type );
}

bool UIHTMLHtml::applyProperty( const StyleSheetProperty& attribute ) {
	if ( !checkPropertyDefinition( attribute ) )
		return false;

	switch ( attribute.getPropertyDefinition()->getPropertyId() ) {
		case PropertyId::Width:
		case PropertyId::Height:
			return false; // Ignore width and height set from CSS
		default:
			break;
	}

	return UIRichText::applyProperty( attribute );
}

UILineBreak* UILineBreak::New( const std::string& tag ) {
	return eeNew( UILineBreak, ( tag ) );
}

UILineBreak::UILineBreak( const std::string& tag ) : UIRichText( tag ) {}

Uint32 UILineBreak::getType() const {
	return UI_TYPE_BR;
}

bool UILineBreak::isType( const Uint32& type ) const {
	return UILineBreak::getType() == type ? true : UIHTMLWidget::isType( type );
}

UIHTMLBody* UIHTMLBody::New( const std::string& tag ) {
	return eeNew( UIHTMLBody, ( tag ) );
}

UIHTMLBody::UIHTMLBody( const std::string& tag ) : UIRichText( tag ) {
	enableReportSizeChangeToChildren();
	getUISceneNode()->loadHTMLBaseCSS();
}

Uint32 UIHTMLBody::getType() const {
	return UI_TYPE_HTML_BODY;
}

bool UIHTMLBody::isType( const Uint32& type ) const {
	return UIHTMLBody::getType() == type ? true : UIRichText::isType( type );
}

bool UIHTMLBody::applyProperty( const StyleSheetProperty& attribute ) {
	if ( !checkPropertyDefinition( attribute ) )
		return false;

	switch ( attribute.getPropertyDefinition()->getPropertyId() ) {
		case PropertyId::Width:
		case PropertyId::Height:
			return false; // Ignore width and height set from CSS
		case PropertyId::BackgroundColor:
		case PropertyId::BackgroundImage:
		case PropertyId::BackgroundTint:
		case PropertyId::BackgroundPositionX:
		case PropertyId::BackgroundPositionY:
		case PropertyId::BackgroundRepeat:
		case PropertyId::BackgroundSize: {
			if ( getParent() && getParent()->isType( UI_TYPE_HTML_HTML ) ) {
				UIWidget* htmlParent = getParent()->asType<UIWidget>();
				if ( htmlParent->getBackgroundColor() == Color::Transparent ||
					 mPropagatedBackground ) {
					mPropagatedBackground = true;
					htmlParent->applyProperty( attribute );
					return true;
				}
			}
			break;
		}
		case PropertyId::MinHeight:
			mMinHeightLocal = attribute.asStyleSheetLength();
			return true;
		default:
			break;
	}

	return UIRichText::applyProperty( attribute );
}

void UIHTMLBody::updateLayout() {
	UIRichText::updateLayout();

	if ( mChild && mChild->isWidget() ) {
		Float maxH = 0;
		Node* child = mChild;
		Float minHeight = std::max(
			mMinHeightLocal.asPixels(
				getParent()->isUINode() ? getParent()->asType<UINode>()->getPixelsSize().getHeight()
										: getParent()->getSize().getHeight(),
				getSceneNode()->getPixelsSize(), getSceneNode()->getDPI() ),
			getMinSize().getHeight() );

		while ( child ) {
			if ( child->isWidget() ) {
				UIWidget* widget = child->asType<UIWidget>();
				bool isFixed = false;
				if ( widget->isType( UI_TYPE_HTML_WIDGET ) &&
					 widget->asType<UIHTMLWidget>()->getCSSPosition() == CSSPosition::Fixed ) {
					isFixed = true;
				}
				if ( !isFixed ) {
					Float childH =
						widget->getPixelsPosition().y + widget->getPixelsSize().getHeight();
					maxH = std::max( maxH, childH );
				}
			}
			child = child->getNextNode();
		}
		if ( maxH > 0 ) {
			Float dpH = std::trunc( PixelDensity::pxToDp( maxH ) );
			if ( dpH != minHeight )
				setMinHeight( dpH );
		}
	}
}

UIHTMLHead* UIHTMLHead::New() {
	return eeNew( UIHTMLHead, () );
}

UIHTMLHead::UIHTMLHead() : UIWidget() {
	mVisible = false;
	mEnabled = false;
}

Uint32 UIHTMLHead::getType() const {
	return UI_TYPE_HTML_HEAD;
}

bool UIHTMLHead::isType( const Uint32& type ) const {
	return UIHTMLHead::getType() == type ? true : UIWidget::isType( type );
}

UIRichText* UIRichText::NewHtml() {
	auto* html = UIHTMLHtml::New( "html" );
	html->setClipType( ClipType::None );
	return html;
}

UIRichText* UIRichText::NewBody() {
	auto* body = UIHTMLBody::New( "body" );
	body->setClipType( ClipType::None );
	return body;
}

UIRichText* UIRichText::NewBr() {
	return UILineBreak::New( "br" );
};

UIRichText* UIRichText::NewHr() {
	auto* w = UILineBreak::New( "hr" );
	w->mMinHeightEq = "1dp";
	return w;
};

UIRichText* UIRichText::New() {
	return eeNew( UIRichText, () );
}

UIRichText* UIRichText::NewWithTag( const std::string& tag ) {
	return eeNew( UIRichText, ( tag ) );
}

UIRichText::UIRichText( const std::string& tag ) : UIHTMLWidget( tag ) {
	mFlags |= UI_HTML_ELEMENT | UI_LOADS_ITS_CHILDREN | UI_OWNS_CHILDREN_POSITION;

	UISceneNode* sceneNode =
		getUISceneNode() ? getUISceneNode() : SceneManager::instance()->getUISceneNode();
	UITheme* theme = sceneNode ? sceneNode->getUIThemeManager()->getDefaultTheme() : nullptr;

	if ( NULL != theme && NULL != theme->getDefaultFont() ) {
		mRichText.getFontStyleConfig().Font = theme->getDefaultFont();
	} else if ( sceneNode && NULL != sceneNode->getUIThemeManager()->getDefaultFont() ) {
		mRichText.getFontStyleConfig().Font = sceneNode->getUIThemeManager()->getDefaultFont();
	}

	if ( NULL != theme ) {
		mRichText.getFontStyleConfig().CharacterSize = theme->getDefaultFontSize();
	} else if ( sceneNode ) {
		mRichText.getFontStyleConfig().CharacterSize =
			sceneNode->getUIThemeManager()->getDefaultFontSize();
	}

	mRichText.getFontStyleConfig().FontColor = Color::Black;

	setLayoutSizePolicy( SizePolicy::MatchParent, SizePolicy::WrapContent );
}

Uint32 UIRichText::getType() const {
	return UI_TYPE_RICHTEXT;
}

bool UIRichText::isType( const Uint32& type ) const {
	return UIRichText::getType() == type ? true : UIHTMLWidget::isType( type );
}

const RichText& UIRichText::getRichText() {
	return mRichText;
}

void UIRichText::draw() {
	if ( mVisible && 0.f != mAlpha ) {
		UIWidget::draw();

		if ( mRichText.getSize().getWidth() > 0.f ) {
			Rectf contentOffset = getPixelsContentOffset();
			if ( isClipped() ) {
				clipSmartEnable( mScreenPos.x + contentOffset.Left,
								 mScreenPos.y + contentOffset.Top,
								 mSize.getWidth() - contentOffset.Left - contentOffset.Right,
								 mSize.getHeight() - contentOffset.Top - contentOffset.Bottom );
			}

			if ( isType( UI_TYPE_TEXTSPAN ) && !asType<UITextSpan>()->isInline() &&
				 asType<UITextSpan>()->getFontBackgroundColor() != Color::Transparent ) {
				Primitives p;
				p.setColor( asType<UITextSpan>()->getFontBackgroundColor() );
				p.drawRectangle( Rectf( mScreenPos.trunc(), mSize.floor() ), 0.f, Vector2f::One );
			}

			mRichText.draw( std::trunc( mScreenPos.x ) + (int)contentOffset.Left,
							std::trunc( mScreenPos.y ) + (int)contentOffset.Top, Vector2f::One, 0.f,
							getBlendMode() );

			if ( isClipped() )
				clipSmartDisable();
		}
	}
}

bool UIRichText::applyProperty( const StyleSheetProperty& attribute ) {
	if ( !checkPropertyDefinition( attribute ) )
		return false;

	switch ( attribute.getPropertyDefinition()->getPropertyId() ) {
		case PropertyId::FontFamily: {
			Font* font =
				getUISceneNode()->getFontFromNamesList( attribute.value(), getFontStyle() );
			if ( NULL != font && font->loaded() ) {
				setFont( font );
			}
			break;
		}
		case PropertyId::FontSize:
			setFontSize( lengthFromValue( attribute ) );
			break;
		case PropertyId::TextDecoration:
			setTextDecoration( attribute.asTextDecoration() );
			break;
		case PropertyId::FontStyle:
			setFontStyle( attribute.asFontStyle() );
			break;
		case PropertyId::FontWeight:
			setFontWeight( Text::stringToFontWeight( attribute.value() ) );
			break;
		case PropertyId::Color: {
			Color color = attribute.asColor();
			if ( color == Color::Transparent &&
				 attribute.getValue().find( "var(" ) != std::string::npos ) {
				// Do not set unresolved colors
				break;
			}
			setFontColor( attribute.asColor() );
			break;
		}
		case PropertyId::BackgroundColor:
			setBackgroundColor( attribute.asColor() );
			setFontBackgroundColor( attribute.asColor() );
			break;
		case PropertyId::TextShadowColor:
			setFontShadowColor( attribute.asColor() );
			break;
		case PropertyId::TextShadowOffset:
			setFontShadowOffset( attribute.asVector2f() );
			break;
		case PropertyId::TextStrokeWidth:
			setOutlineThickness( lengthFromValue( attribute ) );
			break;
		case PropertyId::TextStrokeColor:
			setOutlineColor( attribute.asColor() );
			break;
		case PropertyId::SelectionColor:
			setSelectionColor( attribute.asColor() );
			break;
		case PropertyId::SelectionBackColor:
			setSelectionBackColor( attribute.asColor() );
			break;
		case PropertyId::TextSelection:
			setTextSelectionEnabled( attribute.asBool() );
			break;
		case PropertyId::TextAlign: {
			std::string align = String::toLower( attribute.value() );
			if ( align == "center" )
				setTextAlign( TEXT_ALIGN_CENTER );
			else if ( align == "left" )
				setTextAlign( TEXT_ALIGN_LEFT );
			else if ( align == "right" )
				setTextAlign( TEXT_ALIGN_RIGHT );
			break;
		}
		case PropertyId::LineHeight:
			setLineHeightEq( attribute.value() );
			break;
		case PropertyId::TextIndent:
			setTextIndentEq( attribute.value() );
			break;
		case PropertyId::WhiteSpace:
			applyWhiteSpace( attribute.value() );
			break;
		case PropertyId::WhiteSpaceCollapse:
			setWhiteSpaceCollapse( toWhiteSpaceCollapse( attribute.value() ) );
			break;
		case PropertyId::TextTransform:
			setTextTransform( TextTransform::fromString( attribute.asString() ) );
			break;
		default:
			return UIHTMLWidget::applyProperty( attribute );
	}

	return true;
}

std::string UIRichText::getPropertyString( const PropertyDefinition* propertyDef,
										   const Uint32& propertyIndex ) const {
	if ( NULL == propertyDef )
		return "";

	switch ( propertyDef->getPropertyId() ) {
		case PropertyId::FontFamily:
			return NULL != getFont() ? getFont()->getName() : "";
		case PropertyId::FontSize:
			return String::fromFloat( PixelDensity::pxToDp( getFontSize() ), "dp" );
		case PropertyId::FontStyle:
			return Text::styleFlagToString( getFontStyle() );
		case PropertyId::FontWeight:
			return Text::fontWeightToString( mRichText.getFontStyleConfig().Weight );
		case PropertyId::TextDecoration:
			return Text::styleFlagToString( getTextDecoration() );
		case PropertyId::Color:
			return getFontColor().toHexString();
		case PropertyId::BackgroundColor:
			return getFontBackgroundColor().toHexString();
		case PropertyId::TextShadowColor:
			return getFontShadowColor().toHexString();
		case PropertyId::TextShadowOffset:
			return String::fromFloat( getFontShadowOffset().x ) + " " +
				   String::fromFloat( getFontShadowOffset().y );
		case PropertyId::TextStrokeWidth:
			return String::fromFloat( PixelDensity::dpToPx( getOutlineThickness() ), "px" );
		case PropertyId::TextStrokeColor:
			return getOutlineColor().toHexString();
		case PropertyId::SelectionColor:
			return getSelectionColor().toHexString();
		case PropertyId::SelectionBackColor:
			return getSelectionBackColor().toHexString();
		case PropertyId::TextSelection:
			return isTextSelectionEnabled() ? "true" : "false";
		case PropertyId::TextAlign:
			return getTextAlign() == TEXT_ALIGN_CENTER
					   ? "center"
					   : ( getTextAlign() == TEXT_ALIGN_RIGHT ? "right" : "left" );
		case PropertyId::LineHeight:
			return mLineHeightEq.empty() ? "normal" : mLineHeightEq;
		case PropertyId::TextIndent:
			return mTextIndentEq.empty() ? "0" : mTextIndentEq;
		case PropertyId::WhiteSpace:
			return fromWhiteSpace( mWhiteSpaceCollapse, mLineWrap );
		case PropertyId::WhiteSpaceCollapse:
			return fromWhiteSpaceCollapse( mWhiteSpaceCollapse );
		case PropertyId::TextTransform:
			return TextTransform::toString( getTextTransform() );
		default:
			return UIHTMLWidget::getPropertyString( propertyDef, propertyIndex );
	}
}

std::vector<PropertyId> UIRichText::getPropertiesImplemented() const {
	auto props = UIHTMLWidget::getPropertiesImplemented();
	auto local = {
		PropertyId::FontFamily,		 PropertyId::FontSize,			 PropertyId::FontStyle,
		PropertyId::Color,			 PropertyId::TextShadowColor,	 PropertyId::TextShadowOffset,
		PropertyId::TextStrokeWidth, PropertyId::TextStrokeColor,	 PropertyId::TextAlign,
		PropertyId::SelectionColor,	 PropertyId::SelectionBackColor, PropertyId::TextSelection,
		PropertyId::TextDecoration,	 PropertyId::LineHeight,		 PropertyId::TextIndent,
		PropertyId::WhiteSpace,		 PropertyId::WhiteSpaceCollapse, PropertyId::TextTransform };
	props.insert( props.end(), local.begin(), local.end() );
	return props;
}

Font* UIRichText::getFont() const {
	return mRichText.getFontStyleConfig().Font;
}

UIRichText* UIRichText::setFont( Font* font ) {
	if ( NULL != font && mRichText.getFontStyleConfig().Font != font ) {
		mRichText.getFontStyleConfig().Font = font;
		mRichText.invalidate();
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();
	}
	return this;
}

Uint32 UIRichText::getFontSize() const {
	return mRichText.getFontStyleConfig().CharacterSize;
}

UIRichText* UIRichText::setFontSize( const Uint32& characterSize ) {
	if ( mRichText.getFontStyleConfig().CharacterSize != characterSize ) {
		if ( characterSize == 0 )
			return this;
		mRichText.getFontStyleConfig().CharacterSize = characterSize;
		mRichText.invalidate();
		mLineHeightPxDirty = true;

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();
	}
	return this;
}

const Uint32& UIRichText::getFontStyle() const {
	return mRichText.getFontStyleConfig().Style;
}

Uint32 UIRichText::getTextDecoration() const {
	Uint32 flags = mRichText.getFontStyleConfig().Style;
	flags &= ~( Text::Style::Bold | Text::Style::Italic | Text::Style::Shadow );
	return flags;
}

UIRichText* UIRichText::setTextDecoration( const Uint32& textDecoration ) {
	if ( mRichText.getFontStyleConfig().Style != textDecoration ) {
		mRichText.getFontStyleConfig().Style &= ~( Text::Underlined | Text::StrikeThrough );
		mRichText.getFontStyleConfig().Style |= textDecoration;
		mRichText.invalidate();

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();
	}
	return this;
}

UIRichText* UIRichText::setFontStyle( const Uint32& fontStyle ) {
	if ( mRichText.getFontStyleConfig().Style != fontStyle ) {
		mRichText.getFontStyleConfig().Style = fontStyle;
		mRichText.invalidate();

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();

		if ( auto* newFont = getUISceneNode()->reevaluateFontStyle(
				 mRichText.getFontStyleConfig().Font, fontStyle,
				 ( fontStyle & Text::Bold ) ? FontWeight::Bold : FontWeight::Normal ) )
			setFont( newFont );
	}
	return this;
}

FontWeight UIRichText::getFontWeight() const {
	return mRichText.getFontStyleConfig().Weight;
}

UIRichText* UIRichText::setFontWeight( const FontWeight& weight ) {
	mRichText.getFontStyleConfig().Weight = weight;

	Uint32 weightStyle = ( weight >= FontWeight::SemiBold ) ? Text::Bold : 0;
	Uint32 newStyle = ( mRichText.getFontStyleConfig().Style & ~Text::Bold ) | weightStyle;

	if ( mRichText.getFontStyleConfig().Style != newStyle ) {
		mRichText.getFontStyleConfig().Style = newStyle;
		mRichText.invalidate();

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();

		if ( auto* newFont = getUISceneNode()->reevaluateFontStyle(
				 mRichText.getFontStyleConfig().Font, newStyle, weight ) )
			setFont( newFont );
	}

	return this;
}

const Color& UIRichText::getFontColor() const {
	return mRichText.getFontStyleConfig().FontColor;
}

UIRichText* UIRichText::setFontColor( const Color& color ) {
	if ( mRichText.getFontStyleConfig().FontColor != color ) {
		mRichText.getFontStyleConfig().FontColor = color;
		mRichText.invalidate();
		updateDefaultSpansStyle();
	}
	return this;
}

const Color& UIRichText::getFontBackgroundColor() const {
	return mRichText.getFontStyleConfig().BackgroundColor;
}

UIRichText* UIRichText::setFontBackgroundColor( const Color& color ) {
	if ( mRichText.getFontStyleConfig().BackgroundColor != color ) {
		mRichText.getFontStyleConfig().BackgroundColor = color;
		mRichText.invalidate();
		updateDefaultSpansStyle();
	}
	return this;
}

const Color& UIRichText::getFontShadowColor() const {
	return mRichText.getFontStyleConfig().ShadowColor;
}

UIRichText* UIRichText::setFontShadowColor( const Color& color ) {
	if ( mRichText.getFontStyleConfig().ShadowColor != color ) {
		mRichText.getFontStyleConfig().ShadowColor = color;
		if ( mRichText.getFontStyleConfig().ShadowColor != Color::Transparent )
			mRichText.getFontStyleConfig().Style |= Text::Shadow;
		else
			mRichText.getFontStyleConfig().Style &= ~Text::Shadow;
		mRichText.invalidate();
		updateDefaultSpansStyle();
	}
	return this;
}

const Vector2f& UIRichText::getFontShadowOffset() const {
	return mRichText.getFontStyleConfig().ShadowOffset;
}

UIRichText* UIRichText::setFontShadowOffset( const Vector2f& offset ) {
	if ( mRichText.getFontStyleConfig().ShadowOffset != offset ) {
		mRichText.getFontStyleConfig().ShadowOffset = offset;
		mRichText.invalidate();
		updateDefaultSpansStyle();
	}
	return this;
}

const Float& UIRichText::getOutlineThickness() const {
	return mRichText.getFontStyleConfig().OutlineThickness;
}

UIRichText* UIRichText::setOutlineThickness( const Float& outlineThickness ) {
	if ( mRichText.getFontStyleConfig().OutlineThickness != outlineThickness ) {
		mRichText.getFontStyleConfig().OutlineThickness = outlineThickness;
		mRichText.invalidate();

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
		updateDefaultSpansStyle();
	}
	return this;
}

const Color& UIRichText::getOutlineColor() const {
	return mRichText.getFontStyleConfig().OutlineColor;
}

UIRichText* UIRichText::setOutlineColor( const Color& outlineColor ) {
	if ( mRichText.getFontStyleConfig().OutlineColor != outlineColor ) {
		mRichText.getFontStyleConfig().OutlineColor = outlineColor;
		mRichText.invalidate();
		updateDefaultSpansStyle();
	}
	return this;
}

Uint32 UIRichText::getTextAlign() const {
	return mRichText.getAlign();
}

UIRichText* UIRichText::setTextAlign( const Uint32& align ) {
	if ( mRichText.getAlign() != align ) {
		mRichText.setAlign( align );

		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
	return this;
}

UIRichText::WhiteSpaceCollapse UIRichText::getWhiteSpaceCollapse() const {
	return mWhiteSpaceCollapse;
}

void UIRichText::setWhiteSpaceCollapse( WhiteSpaceCollapse collapse ) {
	if ( mWhiteSpaceCollapse != collapse ) {
		mWhiteSpaceCollapse = collapse;
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
}

bool UIRichText::getLineWrap() const {
	return mLineWrap;
}

void UIRichText::setLineWrap( bool lineWrap ) {
	if ( mLineWrap != lineWrap ) {
		mLineWrap = lineWrap;
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
}

void UIRichText::applyWhiteSpace( std::string val ) {
	String::toLowerInPlace( val );
	String::trimInPlace( val );
	if ( val == "normal" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::Collapse );
		setLineWrap( true );
	} else if ( val == "nowrap" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::Collapse );
		setLineWrap( false );
	} else if ( val == "pre" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::Preserve );
		setLineWrap( false );
	} else if ( val == "pre-wrap" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::Preserve );
		setLineWrap( true );
	} else if ( val == "pre-line" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::PreserveBreaks );
		setLineWrap( true );
	} else if ( val == "break-spaces" ) {
		setWhiteSpaceCollapse( WhiteSpaceCollapse::BreakSpaces );
		setLineWrap( true );
	}
}

UIRichText* UIRichText::setLineHeightEq( const std::string& eq ) {
	if ( mLineHeightEq != eq ) {
		mLineHeightEq = eq;
		mLineHeightPxDirty = true;
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
	return this;
}

Float UIRichText::getLineHeightPx() const {
	if ( !mLineHeightPxDirty )
		return mLineHeightPxCache;
	mLineHeightPxDirty = false;
	if ( mLineHeightEq.empty() || mLineHeightEq == "normal" ) {
		mLineHeightPxCache = 0;
		return 0;
	}

	// Temporal hack until we support calc
	if ( mLineHeightEq.find( "calc(" ) != std::string::npos ||
		 mLineHeightEq.find( "var(" ) != std::string::npos ) {
		mLineHeightPxCache = 0;
		return 0;
	}

	bool isUnitless = !mLineHeightEq.empty();
	for ( char c : mLineHeightEq ) {
		if ( c != '-' && c != '+' && c != '.' && !String::isNumber( c, false ) ) {
			isUnitless = false;
			break;
		}
	}
	if ( isUnitless ) {
		Float multiplier;
		if ( String::fromString( multiplier, mLineHeightEq ) )
			mLineHeightPxCache = multiplier * getFontSize();
		else
			mLineHeightPxCache = 0;
	} else {
		mLineHeightPxCache = const_cast<UIRichText*>( this )->lengthFromValue(
			mLineHeightEq, CSS::PropertyRelativeTarget::FontSize, 0, 0 );
	}
	return mLineHeightPxCache;
}

UIRichText* UIRichText::setTextIndentEq( const std::string& eq ) {
	if ( mTextIndentEq != eq ) {
		mTextIndentEq = eq;
		mTextIndentPxDirty = true;
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
	return this;
}

Float UIRichText::getTextIndentPx() const {
	if ( !mTextIndentPxDirty )
		return mTextIndentPxCache;
	mTextIndentPxDirty = false;
	if ( mTextIndentEq.empty() ) {
		mTextIndentPxCache = 0;
		return 0;
	}
	mTextIndentPxCache = const_cast<UIRichText*>( this )->lengthFromValue(
		mTextIndentEq, CSS::PropertyRelativeTarget::None, 0, 0 );
	return mTextIndentPxCache;
}

const TextTransform::Value& UIRichText::getTextTransform() const {
	return mTextTransform;
}

void UIRichText::setTextTransform( const TextTransform::Value& textTransform ) {
	if ( textTransform != mTextTransform ) {
		mTextTransform = textTransform;
		notifyLayoutAttrChange();
		notifyLayoutAttrChangeParent();
	}
}

void UIRichText::loadFromXmlNode( const pugi::xml_node& node ) {
	beginAttributesTransaction();

	UIWidget::loadFromXmlNode( node );

	for ( pugi::xml_node child = node.first_child(); child; child = child.next_sibling() ) {
		if ( child.type() == pugi::node_element ) {
			if ( mTag == "pre" && String::iequals( child.name(), "code" ) ) {
				// Use a UICodeEditor for <pre><code>
				UICodeEditor* editor = UICodeEditor::NewWithTag( "code" );
				if ( editor ) {
					editor->setParent( this );
					editor->setDynamicTheming( true );
					editor->loadFromXmlNode( child );
					editor->setLayoutSizePolicy( SizePolicy::MatchParent, SizePolicy::WrapContent );
					editor->setLineWrapMode( LineWrapMode::Word );
					editor->setLineWrapType( LineWrapType::Viewport );
					editor->disableEditorFeatures( false );
					editor->setCursorVisible( false );
					editor->setAllowSelectingTextFromGutter( false );
					editor->setDisableCursorBlinkingAfterAMinuteOfInactivity( false );
					editor->setCursorBlinkTime( Time::Zero );
					editor->setShowLineNumber( false );
					editor->setShowFoldingRegion( false );
					editor->setLocked( true );

					auto langIt = mDataProperties.find( "data-language" );
					if ( langIt != mDataProperties.end() ) {
						editor->applyProperty( langIt->second );
					}
				}
			} else if ( String::iequals( child.name(), "style" ) ) {
				CSS::StyleSheetParser parser;
				std::string styleContent;
				for ( pugi::xml_node styleChild = child.first_child(); styleChild;
					  styleChild = styleChild.next_sibling() ) {
					if ( styleChild.type() == pugi::node_pcdata ||
						 styleChild.type() == pugi::node_cdata ) {
						styleContent += styleChild.value();
					}
				}

				if ( parser.loadFromString( std::string_view{ styleContent } ) ) {
					parser.getStyleSheet().setMarker( getUISceneNode()->getCurrentMarker() );
					getUISceneNode()->combineStyleSheet( parser.getStyleSheet(), false );
				}
				continue;
			} else if ( String::iequals( child.name(), "script" ) ) {
				// No plans to support it
				continue;
			} else {
				// Let parent logic load standard child widget
				UIWidget* uiwidget = UIWidgetCreator::createFromName( child.name() );

				// For not known elements it should create an HTMLUnknown element, in practice
				// an span with a the tag name used is enough
				if ( uiwidget == nullptr )
					uiwidget = UITextSpan::NewWithTag( child.name() );

				if ( uiwidget ) {
					uiwidget->setParent( this );
					uiwidget->loadFromXmlNode( child );
					uiwidget->getUIStyle()->applyInheritedProperties();

					if ( !uiwidget->loadsItsChildren() ) {
						if ( child.first_child() && !uiwidget->loadsItsChildren() ) {
							getUISceneNode()->loadNode( child.first_child(), uiwidget, 0 );
						}
					}

					uiwidget->onWidgetCreated();
				}
			}
		} else if ( child.type() == pugi::node_pcdata ) {
			String collapsed = UIRichText::collapseInternalWhitespace( child.value() );
			UITextNode* textNode = UITextNode::New();
			textNode->setParent( this );
			textNode->setText( collapsed );
		}
	}

	endAttributesTransaction();
}

void UIRichText::onSizeChange() {
	if ( getCSSPosition() == CSSPosition::Fixed ) {
		// Fixed-position elements are sized relative to the viewport. Do not
		// trigger a self-layout pass (tryUpdateLayout via
		// UIHTMLWidget::onSizeChange) nor propagate size changes upward
		// (notifyLayoutAttrChangeParent): neither ancestors nor this element
		// need to re-layout in response to a viewport-relative size change,
		// and doing so would cause unnecessary re-layout or infinite recursion.
		UIWidget::onSizeChange();
	} else {
		UIHTMLWidget::onSizeChange();
	}
	notifyLayoutAttrChange();
	if ( getCSSPosition() != CSSPosition::Fixed )
		notifyLayoutAttrChangeParent();
}

void UIRichText::onPaddingChange() {
	UIHTMLWidget::onPaddingChange();
	notifyLayoutAttrChange();
	notifyLayoutAttrChangeParent();
}

void UIRichText::onChildCountChange( Node* child, const bool& removed ) {
	UIHTMLWidget::onChildCountChange( child, removed );
	if ( !removed && child->isWidget() && child->isType( UI_TYPE_TEXTSPAN ) ) {
		static_cast<UITextSpan*>( child )->setInheritedStyle( mRichText.getFontStyleConfig() );
	}

	notifyLayoutAttrChange();
	notifyLayoutAttrChangeParent();
}

void UIRichText::onFontChanged() {
	notifyLayoutAttrChange();
	notifyLayoutAttrChangeParent();
}

void UIRichText::onFontStyleChanged() {
	notifyLayoutAttrChange();
	notifyLayoutAttrChangeParent();
}

String UIRichText::UIRichText::collapseInternalWhitespace( const String& s ) {
	String out;
	out.reserve( s.size() );
	bool inSpace = false;
	for ( size_t i = 0; i < s.size(); ++i ) {
		if ( s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r' || s[i] == '\v' ) {
			if ( !inSpace ) {
				out += ' ';
				inSpace = true;
			}
		} else {
			out += s[i];
			inSpace = false;
		}
	}
	return out;
}

static Float getAtomicInlineBoxBaseline( UIWidget* widget, const Sizef& widgetSize,
										 const Rectf& margin ) {
	Float fallbackBaseline = widgetSize.getHeight() + margin.Top + margin.Bottom;
	if ( !widget->isType( UI_TYPE_HTML_WIDGET ) )
		return fallbackBaseline;

	auto* htmlWidget = widget->asType<UIHTMLWidget>();
	auto* rt = htmlWidget->getRichTextPtr();
	if ( rt == nullptr )
		return fallbackBaseline;

	rt->updateLayout();
	const auto& lines = rt->getLines();
	for ( auto it = lines.rbegin(); it != lines.rend(); ++it ) {
		if ( !it->spans.empty() )
			return margin.Top + widget->getPixelsContentOffset().Top + it->y + it->maxAscent;
	}

	return fallbackBaseline;
}

static CSSBaselineAlignValue getWidgetBaselineAlign( UIWidget* widget ) {
	if ( widget->isType( UI_TYPE_HTML_WIDGET ) )
		return widget->asType<UIHTMLWidget>()->getBaselineAlign();
	return {};
}

static RichText::BaselineAlignValue toRichTextBaselineAlign( const CSSBaselineAlignValue& align ) {
	RichText::BaselineAlignValue out;
	out.value = align.value;
	switch ( align.type ) {
		case CSSBaselineAlignment::Sub:
			out.type = RichText::BaselineAlignment::Sub;
			break;
		case CSSBaselineAlignment::Super:
			out.type = RichText::BaselineAlignment::Super;
			break;
		case CSSBaselineAlignment::TextTop:
			out.type = RichText::BaselineAlignment::TextTop;
			break;
		case CSSBaselineAlignment::TextBottom:
			out.type = RichText::BaselineAlignment::TextBottom;
			break;
		case CSSBaselineAlignment::Middle:
			out.type = RichText::BaselineAlignment::Middle;
			break;
		case CSSBaselineAlignment::Top:
			out.type = RichText::BaselineAlignment::Top;
			break;
		case CSSBaselineAlignment::Bottom:
			out.type = RichText::BaselineAlignment::Bottom;
			break;
		case CSSBaselineAlignment::Length:
			out.type = RichText::BaselineAlignment::Length;
			break;
		case CSSBaselineAlignment::Percentage:
			out.type = RichText::BaselineAlignment::Percentage;
			break;
		case CSSBaselineAlignment::Auto:
			out.type = RichText::BaselineAlignment::Auto;
			break;
		case CSSBaselineAlignment::Baseline:
		default:
			out.type = RichText::BaselineAlignment::Baseline;
			break;
	}
	return out;
}

static RichText::InlineFloat toRichTextFloat( CSSFloat val ) {
	switch ( val ) {
		case CSSFloat::Left:
			return RichText::InlineFloat::Left;
		case CSSFloat::Right:
			return RichText::InlineFloat::Right;
		case CSSFloat::None:
		default:
			return RichText::InlineFloat::None;
	}
}

static RichText::InlineClear toRichTextClear( CSSClear val ) {
	switch ( val ) {
		case CSSClear::Left:
			return RichText::InlineClear::Left;
		case CSSClear::Right:
			return RichText::InlineClear::Right;
		case CSSClear::Both:
			return RichText::InlineClear::Both;
		case CSSClear::None:
		default:
			return RichText::InlineClear::None;
	}
}

static RichText::InlineSource toRichTextTextSource( UITextNode* node ) {
	return { RichText::InlineSourceType::TextNode, node };
}

static RichText::InlineSource toRichTextWidgetSource( UIWidget* widget ) {
	return { RichText::InlineSourceType::Widget, widget };
}

static Float getInlineBorderWidth( UIWidget* widget ) {
	if ( widget == nullptr || !widget->hasBorder() || widget->getBorder() == nullptr )
		return 0.f;

	const auto& borders = widget->getBorder()->getBorders();
	return eemax( eemax( static_cast<Float>( borders.left.width ),
						 static_cast<Float>( borders.right.width ) ),
				  eemax( static_cast<Float>( borders.top.width ),
						 static_cast<Float>( borders.bottom.width ) ) );
}

static Color getInlineBorderColor( UIWidget* widget ) {
	if ( widget == nullptr || !widget->hasBorder() || widget->getBorder() == nullptr )
		return Color::Transparent;

	const auto& borders = widget->getBorder()->getBorders();
	if ( borders.top.width > 0 )
		return borders.top.color;
	if ( borders.right.width > 0 )
		return borders.right.color;
	if ( borders.bottom.width > 0 )
		return borders.bottom.color;
	if ( borders.left.width > 0 )
		return borders.left.color;
	return Color::Transparent;
}

struct InlineBackgroundDrawable {
	Drawable* colorDrawable{ nullptr };
	Drawable* drawable{ nullptr };
	bool usesFragmentColor{ false };
};

static InlineBackgroundDrawable getInlineBackgroundDrawable( UIWidget* widget,
															 const Color& backgroundColor ) {
	if ( widget == nullptr || !( widget->getFlags() & UI_FILL_BACKGROUND ) ||
		 !widget->hasBackground() )
		return {};

	UINodeDrawable* background = widget->getBackground();
	const bool hasRoundedColorBackground =
		backgroundColor != Color::Transparent && background->getBackgroundDrawable().hasRadius();
	if ( background->hasDrawableLayers() )
		return { hasRoundedColorBackground ? &background->getBackgroundDrawable() : nullptr,
				 background, false };
	if ( background->getBackgroundColor() != Color::Transparent )
		return { nullptr, background, false };

	if ( hasRoundedColorBackground )
		return { nullptr, &background->getBackgroundDrawable(), true };

	return {};
}

static Drawable* getInlineBorderDrawable( UIWidget* widget ) {
	return widget != nullptr && widget->hasBorder() && widget->getBorder() != nullptr
			   ? widget->getBorder()
			   : nullptr;
}

void UIRichText::rebuildRichText( UILayout* container, RichText& richText, IntrinsicMode mode ) {
	richText.clear();
	if ( container->isType( UI_TYPE_RICHTEXT ) ) {
		auto* uiRt = static_cast<UIRichText*>( container );
		richText.setLineHeight( uiRt->getLineHeightPx() );
		richText.setTextIndent( uiRt->getTextIndentPx() );
		richText.setLineWrap( uiRt->getLineWrap() );
	}
	bool shouldCollapse = container->isType( UI_TYPE_RICHTEXT )
							  ? static_cast<UIRichText*>( container )->getWhiteSpaceCollapse() ==
									WhiteSpaceCollapse::Collapse
							  : true;
	bool lastSpanEndsWithSpace = false;
	Float maxWidth = 0;
	bool isInlineBlockTextSpan =
		container->isType( UI_TYPE_TEXTSPAN ) && container->asType<UITextSpan>()->isInlineBlock();
	if ( isInlineBlockTextSpan && mode == IntrinsicMode::None &&
		 container->getPixelsSize().getWidth() > 0 ) {
		maxWidth = container->getPixelsSize().getWidth() -
				   container->getPixelsContentOffset().Left -
				   container->getPixelsContentOffset().Right;
	} else if ( container->getLayoutWidthPolicy() == SizePolicy::WrapContent ) {
		maxWidth = container->getMatchParentWidth() - container->getPixelsContentOffset().Left -
				   container->getPixelsContentOffset().Right;
	} else {
		maxWidth = container->getPixelsSize().getWidth() -
				   container->getPixelsContentOffset().Left -
				   container->getPixelsContentOffset().Right;
	}

	if ( maxWidth < 0 )
		maxWidth = 0;

	Float mw = 0.f;
	if ( !container->getMaxWidthEq().empty() ) {
		mw = container->getMaxSizePx().getWidth() - container->getPixelsContentOffset().Left -
			 container->getPixelsContentOffset().Right;
		if ( mw < 0 )
			mw = 0.f;
	}

	if ( mode == IntrinsicMode::None ) {
		if ( !container->getMaxWidthEq().empty() && ( maxWidth == 0 || mw < maxWidth ) ) {
			richText.setMaxWidth( mw );
		} else {
			richText.setMaxWidth( maxWidth );
		}
	} else {
		richText.setMaxWidth( 0.f ); // Let it grow unbounded to query text bounds later
	}

	auto getEffectiveTextTransform = []( Node* node ) -> TextTransform::Value {
		while ( node ) {
			if ( node->isType( UI_TYPE_RICHTEXT ) || node->isType( UI_TYPE_TEXTSPAN ) ) {
				auto tt = node->asType<UIRichText>()->getTextTransform();
				if ( tt != TextTransform::None )
					return tt;
			}
			node = node->getParent();
		}
		return TextTransform::None;
	};

	auto applyTextTransform = []( String& text, TextTransform::Value tt ) {
		switch ( tt ) {
			case TextTransform::LowerCase:
				text = text.toLower();
				break;
			case TextTransform::UpperCase:
				text = text.toUpper();
				break;
			case TextTransform::Capitalize:
				text = text.capitalize();
				break;
			default:
				break;
		}
	};

	if ( container->isType( UI_TYPE_TEXTSPAN ) ) {
		UITextSpan* selfSpan = container->asType<UITextSpan>();
		if ( !selfSpan->getText().empty() && !selfSpan->isInline() &&
			 NULL != selfSpan->getFontStyleConfig().Font ) {
			String::View selfText = selfSpan->getText().view();
			String transformed;
			auto tt = getEffectiveTextTransform( selfSpan );
			if ( tt != TextTransform::None ) {
				transformed = selfText;
				applyTextTransform( transformed, tt );
				selfText = transformed.view();
			}
			FontStyleConfig style = selfSpan->getFontStyleConfig();
			style.BackgroundColor = Color::Transparent;
			richText.addSpan( selfText, style, Rectf::Zero, Rectf::Zero, 0, {} );
			if ( shouldCollapse )
				lastSpanEndsWithSpace = !selfText.empty() && selfText.back() == ' ';
		}
	}

	int inlineBoxDepth = 0;
	auto processNode = [&]( Node* node, auto& processNodeRef ) -> void {
		// Helper: walk up through inline ancestors to find the logical prev/next widget
		auto findLogicalPrev = []( Node* n ) -> Node* {
			while ( n ) {
				Node* sib = n->getPrevNode();
				while ( sib && sib->isTextNode() ) {
					auto* tn = sib->asType<UITextNode>();
					if ( !tn->isWhitespaceOnly() )
						return sib;
					sib = sib->getPrevNode();
				}
				if ( sib && sib->isWidget() )
					return sib;
				n = n->getParent();
				if ( !n || !n->isWidget() || !n->asType<UIWidget>()->isInlineDisplay() )
					break;
			}
			return nullptr;
		};
		auto findLogicalNext = []( Node* n ) -> Node* {
			while ( n ) {
				Node* sib = n->getNextNode();
				while ( sib && sib->isTextNode() ) {
					auto* tn = sib->asType<UITextNode>();
					if ( !tn->isWhitespaceOnly() )
						return sib;
					sib = sib->getNextNode();
				}
				if ( sib && sib->isWidget() )
					return sib;
				n = n->getParent();
				if ( !n || !n->isWidget() || !n->asType<UIWidget>()->isInlineDisplay() )
					break;
			}
			return nullptr;
		};

		if ( node->isTextNode() ) {
			UITextNode* textNode = node->asType<UITextNode>();
			String text = textNode->getText();

			Node* prev = findLogicalPrev( node );
			bool prevIsInline =
				prev && prev->isWidget() && prev->asType<UIWidget>()->isInlineDisplay();

			Node* next = findLogicalNext( node );
			bool nextIsInline =
				next && next->isWidget() && next->asType<UIWidget>()->isInlineDisplay();
			auto isFloatingOrOutOfFlow = []( Node* n ) {
				if ( n == nullptr || !n->isType( UI_TYPE_HTML_WIDGET ) )
					return false;
				auto* htmlWidget = n->asType<UIHTMLWidget>();
				return htmlWidget->getCSSFloat() != CSSFloat::None || htmlWidget->isOutOfFlow();
			};

			if ( shouldCollapse && textNode->isWhitespaceOnly() &&
				 ( !prevIsInline || isFloatingOrOutOfFlow( prev ) ) &&
				 ( !nextIsInline || isFloatingOrOutOfFlow( next ) ) ) {
				textNode->setLayoutCharCount( 0 );
				return;
			}

			// Strip leading space if prev is not inline (block boundary)
			if ( !prevIsInline && !text.empty() && text[0] == ' ' )
				text = text.substr( 1 );

			// Strip trailing space if next is not inline
			if ( !nextIsInline && !text.empty() && text.back() == ' ' )
				text = text.substr( 0, text.size() - 1 );

			if ( shouldCollapse && lastSpanEndsWithSpace && !text.empty() && text[0] == ' ' )
				text = text.substr( 1 );

			{
				auto tt = getEffectiveTextTransform( textNode );
				if ( tt != TextTransform::None )
					applyTextTransform( text, tt );
			}

			if ( text.empty() ) {
				textNode->setLayoutCharCount( 0 );
				return;
			}

			textNode->setLayoutCharCount( text.length() );

			if ( shouldCollapse )
				lastSpanEndsWithSpace = text.back() == ' ';

			FontStyleConfig style;
			if ( node->getParent()->isType( UI_TYPE_TEXTSPAN ) ) {
				style = node->getParent()->asType<UITextSpan>()->getFontStyleConfig();
			} else if ( node->getParent()->isType( UI_TYPE_RICHTEXT ) ) {
				style = node->getParent()->asType<UIRichText>()->getRichText().getFontStyleConfig();
			} else {
				style = richText.getFontStyleConfig();
			}
			if ( inlineBoxDepth > 0 )
				richText.addInlineText( text, style, Rectf::Zero, Rectf::Zero, 0, {},
										toRichTextTextSource( textNode ) );
			else
				richText.addSpan( text, style, Rectf::Zero, Rectf::Zero, 0, {},
								  toRichTextTextSource( textNode ) );
			return;
		}

		if ( !node->isWidget() || !node->isVisible() )
			return;

		UIWidget* widget = node->asType<UIWidget>();

		// Skip <head> - it must not participate in layout
		if ( widget->isType( UI_TYPE_HTML_HEAD ) )
			return;

		bool handled = false;

		if ( widget->isType( UI_TYPE_HTML_WIDGET ) && widget->asType<UIHTMLWidget>()->isInline() &&
			 widget->asType<UIHTMLWidget>()->getCSSFloat() == CSSFloat::None &&
			 !widget->asType<UIHTMLWidget>()->isOutOfFlow() ) {
			UITextSpan* span = widget->asType<UITextSpan>();
			span->setLayoutCharCount( 0 );
			Rectf margin = span->getLayoutPixelsMargin();
			Rectf padding = span->getPixelsPadding();
			Float borderWidth = getInlineBorderWidth( span );
			Color borderColor = getInlineBorderColor( span );
			bool hasOwnText = !span->getText().empty() && NULL != span->getFontStyleConfig().Font;

			Float spanLineHeight = span->getLineHeightPx();
			Float parentLineHeight = 0;
			Node* parent = span->getParent();
			if ( parent != nullptr && parent->isType( UI_TYPE_RICHTEXT ) )
				parentLineHeight = parent->asType<UIRichText>()->getLineHeightPx();
			else if ( parent != nullptr && parent->isType( UI_TYPE_TEXTSPAN ) )
				parentLineHeight = parent->asType<UITextSpan>()->getLineHeightPx();
			if ( spanLineHeight > 0 && std::abs( spanLineHeight - parentLineHeight ) <= 0.01f )
				spanLineHeight = 0;
			if ( spanLineHeight <= 0 && span->isInlineBlock() ) {
				auto& fontStyle = span->getFontStyleConfig();
				if ( fontStyle.Font )
					spanLineHeight =
						(Float)fontStyle.Font->getFontHeight( fontStyle.CharacterSize );
			}

			auto backgroundDrawable =
				getInlineBackgroundDrawable( span, span->getFontStyleConfig().BackgroundColor );
			richText.pushInlineBox(
				margin, padding, spanLineHeight,
				toRichTextBaselineAlign( span->getBaselineAlign() ),
				span->getFontStyleConfig().BackgroundColor, borderWidth, borderColor,
				span->getTextDecoration(), toRichTextWidgetSource( span ),
				backgroundDrawable.colorDrawable, backgroundDrawable.drawable,
				getInlineBorderDrawable( span ), backgroundDrawable.usesFragmentColor );
			inlineBoxDepth++;

			if ( hasOwnText ) {
				String::View spanText = span->getText().view();

				Node* prev = findLogicalPrev( node );
				bool prevIsInline =
					prev && prev->isWidget() && prev->asType<UIWidget>()->isInlineDisplay();

				Node* next = findLogicalNext( node );
				bool nextIsInline =
					next && next->isWidget() && next->asType<UIWidget>()->isInlineDisplay();

				if ( !prevIsInline && !spanText.empty() && spanText[0] == ' ' )
					spanText = spanText.substr( 1 );
				if ( !nextIsInline && !spanText.empty() && spanText.back() == ' ' )
					spanText = spanText.substr( 0, spanText.size() - 1 );

				if ( shouldCollapse && lastSpanEndsWithSpace && !spanText.empty() &&
					 spanText[0] == ' ' )
					spanText = spanText.substr( 1 );

				String transformed;
				auto tt = getEffectiveTextTransform( span );
				if ( tt != TextTransform::None ) {
					transformed = spanText;
					applyTextTransform( transformed, tt );
					spanText = transformed.view();
				}

				if ( !spanText.empty() ) {
					richText.addInlineText( spanText, span->getFontStyleConfig(), Rectf::Zero,
											Rectf::Zero, 0, {} );
					span->setLayoutCharCount( spanText.length() );
					if ( shouldCollapse )
						lastSpanEndsWithSpace = spanText.back() == ' ';
				} else {
					span->setLayoutCharCount( 0 );
				}
			}

			Node* spanChild = span->getFirstChild();
			while ( spanChild != NULL ) {
				bool isOutOfFlow = spanChild->isType( UI_TYPE_HTML_WIDGET ) &&
								   spanChild->asType<UIHTMLWidget>()->isOutOfFlow();
				if ( !isOutOfFlow )
					processNodeRef( spanChild, processNodeRef );
				spanChild = spanChild->getNextNode();
			}

			if ( shouldCollapse && span->isInlineBlock() )
				lastSpanEndsWithSpace = true;

			inlineBoxDepth--;
			richText.popInlineBox();

			handled = true;
		}

		if ( !handled ) {
			if ( widget->isType( UI_TYPE_BR ) ) {
				richText.addSpan(
					"\n", widget->asType<UILineBreak>()->getRichText().getFontStyleConfig() );
				lastSpanEndsWithSpace = false;
			} else {
				if ( widget->hasLayoutMarginAuto() )
					widget->updateLayoutMarginAuto();
				Rectf margin =
					widget->isType( UI_TYPE_HTML_WIDGET )
						? widget->asType<UIHTMLWidget>()->getNormalFlowLayoutPixelsMargin()
						: widget->getLayoutPixelsMargin();
				bool isBlock = widget->getLayoutWidthPolicy() == SizePolicy::MatchParent;
				if ( widget->isType( UI_TYPE_HTML_WIDGET ) ) {
					CSSDisplay display = widget->asType<UIHTMLWidget>()->getDisplay();
					if ( display == CSSDisplay::Inline || display == CSSDisplay::InlineBlock )
						isBlock = false;
					else if ( display != CSSDisplay::None )
						isBlock = true;
				}

				bool fillParent =
					isBlock && widget->getLayoutWidthPolicy() == SizePolicy::MatchParent;

				if ( mode == IntrinsicMode::None ) {
					if ( fillParent ) {
						if ( container->getLayoutWidthPolicy() != SizePolicy::WrapContent &&
							 container->getPixelsSize().getWidth() != 0 ) {
							Float maxSize =
								eemax( 0.f, container->getPixelsSize().getWidth() -
												container->getPixelsContentOffset().Left -
												container->getPixelsContentOffset().Right -
												margin.Left - margin.Right );
							widget->setPixelsSize( eemax( 0.f, maxSize ),
												   widget->getPixelsSize().getHeight() );
						} else {
							container->onAutoSizeChild( widget );
						}
					} else if ( widget->getLayoutWidthPolicy() == SizePolicy::WrapContent ||
								widget->getLayoutHeightPolicy() == SizePolicy::WrapContent ) {
						container->onAutoSizeChild( widget );
					}

					if ( widget->isType( UI_TYPE_TEXTSPAN ) &&
						 widget->asType<UITextSpan>()->isInlineBlock() )
						widget->asType<UITextSpan>()->updateLayout();
				}

				Sizef size;
				if ( mode == IntrinsicMode::Min ) {
					size = Sizef( widget->getMinIntrinsicWidth(), 0 );
				} else if ( mode == IntrinsicMode::Max ) {
					size = Sizef( widget->getMaxIntrinsicWidth(), 0 );
				} else {
					size = widget->getPixelsSize();
				}

				Float w = size.getWidth();
				if ( fillParent && mode == IntrinsicMode::None &&
					 container->getLayoutWidthPolicy() != SizePolicy::WrapContent &&
					 container->getPixelsSize().getWidth() != 0 ) {
					w = eemax( 0.f, container->getPixelsSize().getWidth() -
										container->getPixelsContentOffset().Left -
										container->getPixelsContentOffset().Right - margin.Left -
										margin.Right );
				}

				CSSFloat floatType = CSSFloat::None;
				CSSClear clearType = CSSClear::None;
				if ( widget->isType( UI_TYPE_HTML_WIDGET ) ) {
					floatType = widget->asType<UIHTMLWidget>()->getCSSFloat();
					clearType = widget->asType<UIHTMLWidget>()->getCSSClear();
				}
				bool isFloating = floatType != CSSFloat::None;
				bool isNormalFlowBlock = isBlock && !isFloating;
				bool isBlockFormattingContext =
					isNormalFlowBlock && widget->isType( UI_TYPE_HTML_WIDGET ) &&
					widget->asType<UIHTMLWidget>()->establishesBlockFormattingContext();
				bool shrinkToFitFloat =
					isFloating && widget->getLayoutWidthPolicy() == SizePolicy::MatchParent;

				if ( isNormalFlowBlock )
					richText.addLineBreak();

				if ( shrinkToFitFloat && mode == IntrinsicMode::None ) {
					container->onAutoSizeChild( widget );
					Float availableWidth = 0.f;
					if ( container->getPixelsSize().getWidth() > 0 ) {
						availableWidth = eemax( 0.f, container->getPixelsSize().getWidth() -
														 container->getPixelsContentOffset().Left -
														 container->getPixelsContentOffset().Right -
														 margin.Left - margin.Right );
					}
					Float preferredMin = widget->getMinIntrinsicWidth();
					Float preferred = widget->getMaxIntrinsicWidth();
					Float shrinkWidth = preferred;
					if ( availableWidth > 0 )
						shrinkWidth = eemin( eemax( preferredMin, availableWidth ), preferred );
					widget->setPixelsSize( shrinkWidth, widget->getPixelsSize().getHeight() );
					size = widget->getPixelsSize();
					w = shrinkWidth;
				}

				Sizef customSize( w + margin.Left + margin.Right,
								  size.getHeight() + margin.Top + margin.Bottom );
				richText.addCustomSize(
					customSize, toRichTextFloat( floatType ), toRichTextClear( clearType ),
					getAtomicInlineBoxBaseline( widget, size, margin ),
					toRichTextBaselineAlign( getWidgetBaselineAlign( widget ) ),
					toRichTextWidgetSource( widget ), isNormalFlowBlock, isBlockFormattingContext );

				if ( widget->isType( UI_TYPE_TEXTSPAN ) &&
					 widget->asType<UITextSpan>()->isInlineBlock() &&
					 widget->asType<UIRichText>()->getRichTextPtr() )
					widget->asType<UIRichText>()->getRichTextPtr()->updateLayout();

				if ( isNormalFlowBlock )
					richText.addLineBreak();
				else if ( widget->isType( UI_TYPE_TEXTSPAN ) &&
						  widget->asType<UITextSpan>()->isInlineBlock() &&
						  widget->asType<UIRichText>()->getRichTextPtr() &&
						  widget->asType<UIRichText>()->getRichTextPtr()->getLines().size() > 1 )
					richText.addLineBreak();

				lastSpanEndsWithSpace = false;
			}
		}
	};

	Node* child = container->getFirstChild();
	while ( NULL != child ) {
		bool isOutOfFlow =
			child->isType( UI_TYPE_HTML_WIDGET ) && child->asType<UIHTMLWidget>()->isOutOfFlow();
		if ( !isOutOfFlow )
			processNode( child, processNode );
		child = child->getNextNode();
	}
}

void UIRichText::rebuildRichText( RichText& richText, IntrinsicMode mode ) {
	rebuildRichText( this, richText, mode );
}

void UIRichText::updateDefaultSpansStyle() {
	Node* child = mChild;
	while ( NULL != child ) {
		if ( child->isWidget() && child->isType( UI_TYPE_TEXTSPAN ) ) {
			static_cast<UITextSpan*>( child )->setInheritedStyle( mRichText.getFontStyleConfig() );
		}
		child = child->getNextNode();
	}
}

Float UIRichText::getMinIntrinsicWidth() const {
	if ( mWidthPolicy == SizePolicy::Fixed ) {
		return getPropertyWidth();
	}

	UILayouter* layouter = const_cast<UIRichText*>( this )->getLayouter();
	if ( mIntrinsicWidthsDirty && layouter ) {
		layouter->computeIntrinsicWidths();
		mMinIntrinsicWidth = layouter->getMinIntrinsicWidth();
		mMaxIntrinsicWidth = layouter->getMaxIntrinsicWidth();
	} else if ( mIntrinsicWidthsDirty ) {
		RichText richText( mRichText );
		UIRichText::rebuildRichText( const_cast<UIRichText*>( this ), richText,
									 IntrinsicMode::Min );
		mMinIntrinsicWidth = richText.getMinIntrinsicWidth() + getPixelsContentOffset().Left +
							 getPixelsContentOffset().Right;
		UIRichText::rebuildRichText( const_cast<UIRichText*>( this ), richText,
									 IntrinsicMode::Max );
		mMaxIntrinsicWidth = richText.getMaxIntrinsicWidth() + getPixelsContentOffset().Left +
							 getPixelsContentOffset().Right;
		mIntrinsicWidthsDirty = false;
	}

	Float minWidth = mMinIntrinsicWidth;
	if ( !mMinWidthEq.empty() )
		minWidth = eemax( minWidth, getMinSizePx().getWidth() );
	if ( !mMaxWidthEq.empty() )
		minWidth = eemin( minWidth, getMaxSizePx().getWidth() );
	return minWidth;
}

Float UIRichText::getMaxIntrinsicWidth() const {
	if ( mWidthPolicy == SizePolicy::Fixed ) {
		return getPropertyWidth();
	}

	Float maxW = 0;
	if ( const_cast<UIRichText*>( this )->getLayouter() ) {
		maxW = const_cast<UIRichText*>( this )->getLayouter()->getMaxIntrinsicWidth();
	} else {
		if ( mIntrinsicWidthsDirty ) {
			RichText richText( mRichText );
			const_cast<UIRichText*>( this )->rebuildRichText( richText, IntrinsicMode::Min );
			mMinIntrinsicWidth = richText.getMinIntrinsicWidth() + getPixelsContentOffset().Left +
								 getPixelsContentOffset().Right;
			const_cast<UIRichText*>( this )->rebuildRichText( richText, IntrinsicMode::Max );
			mMaxIntrinsicWidth = richText.getMaxIntrinsicWidth() + getPixelsContentOffset().Left +
								 getPixelsContentOffset().Right;
			mIntrinsicWidthsDirty = false;
		}
		maxW = mMaxIntrinsicWidth;
	}

	Float maxWidth = maxW;
	if ( !mMinWidthEq.empty() )
		maxWidth = eemax( maxWidth, getMinSizePx().getWidth() );
	if ( !mMaxWidthEq.empty() )
		maxWidth = eemin( maxWidth, getMaxSizePx().getWidth() );
	return maxWidth;
}

Uint32 UIRichText::onMessage( const NodeMessage* Msg ) {
	switch ( Msg->getMsg() ) {
		case NodeMessage::LayoutAttributeChange: {
			bool packing = isPacking();
			if ( packing )
				return 1;
			if ( Msg->getSender() != this ) {
				invalidateIntrinsicSize();
				// Fixed-position children are sized relative to the viewport
				// and never affect the parent's normal-flow layout. Suppress
				// ancestor notification and parent re-layout for these; absolute
				// children are allowed because parents like body compute their
				// minHeight from absolute children.
				if ( !Msg->getSender()->isType( UI_TYPE_HTML_WIDGET ) ||
					 static_cast<const UIHTMLWidget*>( Msg->getSender() )->getCSSPosition() !=
						 CSSPosition::Fixed ) {
					notifyLayoutAttrChangeParent();
				}
			}
			if ( !Msg->getSender()->isType( UI_TYPE_HTML_WIDGET ) ||
				 static_cast<const UIHTMLWidget*>( Msg->getSender() )->getCSSPosition() !=
					 CSSPosition::Fixed ) {
				tryUpdateLayout();
			}
			return 1;
		}
		case NodeMessage::MouseDown: {
			if ( Msg->getSender()->isType( UI_TYPE_TEXTSPAN ) )
				return onMouseDown( getEventDispatcher()->getMousePos(), Msg->getFlags() );
		}
		case NodeMessage::MouseUp: {
			if ( Msg->getSender()->isType( UI_TYPE_TEXTSPAN ) ) {
				onMouseUp( getEventDispatcher()->getMousePos(), Msg->getFlags() );
				return 0;
			}
		}
		case NodeMessage::MouseClick: {
			if ( Msg->getSender()->isType( UI_TYPE_TEXTSPAN ) )
				return onMouseClick( getEventDispatcher()->getMousePos(), Msg->getFlags() );
		}
		case NodeMessage::MouseDoubleClick: {
			if ( Msg->getSender()->isType( UI_TYPE_TEXTSPAN ) )
				return onMouseDoubleClick( getEventDispatcher()->getMousePos(), Msg->getFlags() );
		}
	}

	return 0;
}

bool UIRichText::isTextSelectionEnabled() const {
	return 0 != ( mFlags & UI_TEXT_SELECTION_ENABLED );
}

void UIRichText::setTextSelectionEnabled( bool active ) {
	if ( active ) {
		mFlags |= UI_TEXT_SELECTION_ENABLED;
	} else {
		mFlags &= ~UI_TEXT_SELECTION_ENABLED;
	}
}

const Color& UIRichText::getSelectionBackColor() const {
	return mRichText.getSelectionBackColor();
}

void UIRichText::setSelectionBackColor( const Color& color ) {
	mRichText.setSelectionBackColor( color );
	invalidateDraw();
}

const Color& UIRichText::getSelectionColor() const {
	return mRichText.getSelectionColor();
}

void UIRichText::setSelectionColor( const Color& color ) {
	mRichText.setSelectionColor( color );
	invalidateDraw();
}

std::pair<Int64, Int64> UIRichText::getTextSelectionRange() const {
	return { mSelCurInit, mSelCurEnd };
}

void UIRichText::setTextSelectionRange( TextSelectionRange range ) {
	selCurInit( std::clamp( range.start, (Int64)0, mRichText.getCharacterCount() ) );
	selCurEnd( std::clamp( range.end, (Int64)0, mRichText.getCharacterCount() ) );
	onSelectionChange();
}

String UIRichText::getSelectionString() const {
	return mRichText.getSelectionString();
}

Uint32 UIRichText::onMouseDown( const Vector2i& position, const Uint32& flags ) {
	if ( NULL != getEventDispatcher() && isTextSelectionEnabled() && ( flags & EE_BUTTON_LMASK ) &&
		 ( getEventDispatcher()->getMouseDownNode() == this ||
		   inParentTreeOf( getEventDispatcher()->getMouseDownNode() ) ) ) {
		Vector2f nodePos( Vector2f( position.x, position.y ) );
		worldToNode( nodePos );
		nodePos = PixelDensity::dpToPx( nodePos ) - Vector2f( mPaddingPx.Left, mPaddingPx.Top );
		nodePos.x = eemax( 0.f, nodePos.x );
		nodePos.y = eemax( 0.f, nodePos.y );

		Int64 curPos = mRichText.findCharacterFromPos( nodePos.asInt() );

		if ( -1 != curPos ) {
			if ( !mSelecting ) {
				selCurInit( curPos );
				selCurEnd( curPos );
			} else {
				selCurInit( curPos );
			}

			onSelectionChange();
		}

		mSelecting = true;
	}

	return UIHTMLWidget::onMouseDown( position, flags );
}

Uint32 UIRichText::onMouseUp( const Vector2i& position, const Uint32& flags ) {
	if ( isTextSelectionEnabled() && ( flags & EE_BUTTON_LMASK ) ) {
		mSelecting = false;
	}

	return UIHTMLWidget::onMouseClick( position, flags );
}

Uint32 UIRichText::onMouseDoubleClick( const Vector2i& position, const Uint32& flags ) {
	return UIHTMLWidget::onMouseDoubleClick( position, flags );
}

Uint32 UIRichText::onFocusLoss() {
	UIHTMLWidget::onFocusLoss();

	selCurEnd( selCurInit() );
	onSelectionChange();

	return 1;
}

void UIRichText::onSelectionChange() {
	mRichText.setSelection( { mSelCurInit, mSelCurEnd } );
	invalidateDraw();
}

void UIRichText::selCurInit( const Int64& init ) {
	if ( mSelCurInit != init ) {
		mSelCurInit = init;
		invalidateDraw();
	}
}

void UIRichText::selCurEnd( const Int64& end ) {
	if ( mSelCurEnd != end ) {
		mSelCurEnd = end;
		invalidateDraw();
	}
}
}} // namespace EE::UI
