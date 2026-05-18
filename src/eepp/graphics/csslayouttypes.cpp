#include <eepp/core/string.hpp>
#include <eepp/ui/csslayouttypes.hpp>

namespace EE { namespace UI {

std::string CSSDisplayHelper::toString( CSSDisplay display ) {
	switch ( display ) {
		case CSSDisplay::Inline:
			return "inline";
		case CSSDisplay::InlineBlock:
			return "inline-block";
		case CSSDisplay::ListItem:
			return "list-item";
		case CSSDisplay::Flex:
			return "flex";
		case CSSDisplay::None:
			return "none";
		case CSSDisplay::Table:
			return "table";
		case CSSDisplay::TableRow:
			return "table-row";
		case CSSDisplay::TableCell:
			return "table-cell";
		case CSSDisplay::TableHead:
			return "table-header-group";
		case CSSDisplay::TableBody:
			return "table-row-group";
		case CSSDisplay::TableFooter:
			return "table-footer-group";
		case CSSDisplay::Block:
		default:
			return "block";
	}
};

CSSDisplay CSSDisplayHelper::fromString( std::string_view val ) {
	CSSDisplay display = CSSDisplay::Block;
	if ( val == "inline" )
		display = CSSDisplay::Inline;
	else if ( val == "inline-block" )
		display = CSSDisplay::InlineBlock;
	else if ( val == "list-item" )
		display = CSSDisplay::ListItem;
	else if ( val == "none" )
		display = CSSDisplay::None;
	else if ( val == "table" )
		display = CSSDisplay::Table;
	else if ( val == "table-row" )
		display = CSSDisplay::TableRow;
	else if ( val == "table-cell" )
		display = CSSDisplay::TableCell;
	else if ( val == "table-header-group" )
		display = CSSDisplay::TableHead;
	else if ( val == "table-row-group" )
		display = CSSDisplay::TableBody;
	else if ( val == "table-footer-group" )
		display = CSSDisplay::TableFooter;
	else if ( val == "flex" )
		display = CSSDisplay::Flex;
	return display;
}

std::string CSSPositionHelper::toString( CSSPosition position ) {
	switch ( position ) {
		case CSSPosition::Relative:
			return "relative";
		case CSSPosition::Absolute:
			return "absolute";
		case CSSPosition::Fixed:
			return "fixed";
		case CSSPosition::Sticky:
			return "sticky";
		case CSSPosition::Static:
		default: {
		}
	}
	return "static";
}

CSSPosition CSSPositionHelper::fromString( std::string_view val ) {
	CSSPosition position = CSSPosition::Static;
	if ( val == "relative" )
		position = CSSPosition::Relative;
	else if ( val == "absolute" )
		position = CSSPosition::Absolute;
	else if ( val == "fixed" )
		position = CSSPosition::Fixed;
	else if ( val == "sticky" )
		position = CSSPosition::Sticky;
	return position;
}

std::string CSSListStyleTypeHelper::toString( CSSListStyleType type ) {
	switch ( type ) {
		case CSSListStyleType::Disc:
			return "disc";
		case CSSListStyleType::Circle:
			return "circle";
		case CSSListStyleType::Square:
			return "square";
		case CSSListStyleType::Decimal:
			return "decimal";
		case CSSListStyleType::LowerAlpha:
			return "lower-alpha";
		case CSSListStyleType::UpperAlpha:
			return "upper-alpha";
		case CSSListStyleType::LowerRoman:
			return "lower-roman";
		case CSSListStyleType::UpperRoman:
			return "upper-roman";
		case CSSListStyleType::DisclosureClosed:
			return "disclosure-closed";
		case CSSListStyleType::DisclosureOpen:
			return "disclosure-open";
		case CSSListStyleType::None:
		default:
			return "none";
	}
}

CSSListStyleType CSSListStyleTypeHelper::fromString( std::string_view val ) {
	if ( val == "disc" )
		return CSSListStyleType::Disc;
	if ( val == "circle" )
		return CSSListStyleType::Circle;
	if ( val == "square" )
		return CSSListStyleType::Square;
	if ( val == "decimal" )
		return CSSListStyleType::Decimal;
	if ( val == "lower-alpha" )
		return CSSListStyleType::LowerAlpha;
	if ( val == "upper-alpha" )
		return CSSListStyleType::UpperAlpha;
	if ( val == "lower-roman" )
		return CSSListStyleType::LowerRoman;
	if ( val == "upper-roman" )
		return CSSListStyleType::UpperRoman;
	if ( val == "disclosure-closed" )
		return CSSListStyleType::DisclosureClosed;
	if ( val == "disclosure-open" )
		return CSSListStyleType::DisclosureOpen;
	return CSSListStyleType::None;
}

std::string CSSListStylePositionHelper::toString( CSSListStylePosition pos ) {
	return pos == CSSListStylePosition::Inside ? "inside" : "outside";
}

CSSListStylePosition CSSListStylePositionHelper::fromString( std::string_view val ) {
	if ( val == "inside" )
		return CSSListStylePosition::Inside;
	return CSSListStylePosition::Outside;
}

std::string CSSFloatHelper::toString( CSSFloat val ) {
	switch ( val ) {
		case CSSFloat::Left:
			return "left";
		case CSSFloat::Right:
			return "right";
		case CSSFloat::None:
		default:
			return "none";
	}
}

CSSFloat CSSFloatHelper::fromString( std::string_view val ) {
	if ( val == "left" )
		return CSSFloat::Left;
	if ( val == "right" )
		return CSSFloat::Right;
	return CSSFloat::None;
}

std::string CSSClearHelper::toString( CSSClear val ) {
	switch ( val ) {
		case CSSClear::Left:
			return "left";
		case CSSClear::Right:
			return "right";
		case CSSClear::Both:
			return "both";
		case CSSClear::None:
		default:
			return "none";
	}
}

CSSClear CSSClearHelper::fromString( std::string_view val ) {
	if ( val == "left" )
		return CSSClear::Left;
	if ( val == "right" )
		return CSSClear::Right;
	if ( val == "both" )
		return CSSClear::Both;
	return CSSClear::None;
}

std::string_view CSSBaselineAlignmentHelper::toString( const CSSBaselineAlignValue& val ) {
	switch ( val.type ) {
		case CSSBaselineAlignment::Sub:
			return "sub";
		case CSSBaselineAlignment::Super:
			return "super";
		case CSSBaselineAlignment::TextTop:
			return "text-top";
		case CSSBaselineAlignment::TextBottom:
			return "text-bottom";
		case CSSBaselineAlignment::Middle:
			return "middle";
		case CSSBaselineAlignment::Top:
			return "top";
		case CSSBaselineAlignment::Bottom:
			return "bottom";
		case CSSBaselineAlignment::Length:
			return "length";
		case CSSBaselineAlignment::Percentage:
			return "percentage";
		case CSSBaselineAlignment::Auto:
			return "auto";
		case CSSBaselineAlignment::Baseline:
		default:
			return "baseline";
	}
}

CSSBaselineAlignValue CSSBaselineAlignmentHelper::fromKeyword( std::string_view val ) {
	CSSBaselineAlignValue out;

	switch ( String::hashToLower( val ) ) {
		case String::hash( "auto" ):
			out.type = CSSBaselineAlignment::Auto;
			break;
		case String::hash( "sub" ):
			out.type = CSSBaselineAlignment::Sub;
			break;
		case String::hash( "super" ):
			out.type = CSSBaselineAlignment::Super;
			break;
		case String::hash( "text-top" ):
		case String::hash( "text-before-edge" ):
		case String::hash( "before-edge" ):
		case String::hash( "hanging" ):
			out.type = CSSBaselineAlignment::TextTop;
			break;
		case String::hash( "text-bottom" ):
		case String::hash( "text-after-edge" ):
		case String::hash( "after-edge" ):
		case String::hash( "mathematical" ):
			out.type = CSSBaselineAlignment::TextBottom;
			break;
		case String::hash( "middle" ):
		case String::hash( "central" ):
			out.type = CSSBaselineAlignment::Middle;
			break;
		case String::hash( "top" ):
			out.type = CSSBaselineAlignment::Top;
			break;
		case String::hash( "bottom" ):
			out.type = CSSBaselineAlignment::Bottom;
			break;
		case String::hash( "baseline" ):
		default:
			out.type = CSSBaselineAlignment::Baseline;
			break;
	}

	return out;
}

}} // namespace EE::UI
