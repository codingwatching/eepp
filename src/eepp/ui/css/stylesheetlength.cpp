#include <eepp/core/string.hpp>
#include <eepp/graphics/font.hpp>
#include <eepp/graphics/pixeldensity.hpp>
#include <eepp/math/math.hpp>
#include <eepp/system/luapattern.hpp>
#include <eepp/ui/css/stylesheetlength.hpp>

using namespace EE::Graphics;
using namespace std::literals;

namespace EE { namespace UI { namespace CSS {

enum UnitHashes : String::HashType {
	Percentage = String::hash( "%" ),
	In = String::hash( "in" ),
	Cm = String::hash( "cm" ),
	Mm = String::hash( "mm" ),
	Em = String::hash( "em" ),
	Ex = String::hash( "ex" ),
	Pt = String::hash( "pt" ),
	Pc = String::hash( "pc" ),
	Px = String::hash( "px" ),
	Dpi = String::hash( "dpi" ),
	Dp = String::hash( "dp" ),
	Dpcm = String::hash( "dpcm" ),
	Vw = String::hash( "vw" ),
	Vh = String::hash( "vh" ),
	Vmin = String::hash( "vmin" ),
	Vmax = String::hash( "vmax" ),
	Rem = String::hash( "rem" ),
	Dprd = String::hash( "dprd" ),
	Dpru = String::hash( "dpru" ),
	Dpr = String::hash( "dpr" ),
	Ch = String::hash( "ch" ),
};

enum PercentagePositions : String::HashType {
	Center = String::hash( "center" ),
	Left = String::hash( "left" ),
	Right = String::hash( "right" ),
	Top = String::hash( "top" ),
	Bottom = String::hash( "bottom" ),
	None = 0,
};

static std::string positionToPercentage( const PercentagePositions& pos ) {
	switch ( pos ) {
		case Center:
			return "50%";
		case Left:
		case Top:
			return "0%";
		case Right:
		case Bottom:
			return "100%";
		default:
		case None:
			return "";
	}
}

static PercentagePositions isPercentagePosition( const String::HashType& strHash ) {
	switch ( strHash ) {
		case PercentagePositions::Center:
			return PercentagePositions::Center;
		case PercentagePositions::Left:
			return PercentagePositions::Left;
		case PercentagePositions::Right:
			return PercentagePositions::Right;
		case PercentagePositions::Top:
			return PercentagePositions::Top;
		case PercentagePositions::Bottom:
			return PercentagePositions::Bottom;
	}
	return PercentagePositions::None;
}

StyleSheetLength::Unit StyleSheetLength::unitFromString( std::string_view unitStr ) {
	switch ( String::hashToLower( unitStr ) ) {
		case UnitHashes::Percentage:
			return Unit::Percentage;
		case UnitHashes::Dp:
			return Unit::Dp;
		case UnitHashes::Px:
			return Unit::Px;
		case UnitHashes::In:
			return Unit::In;
		case UnitHashes::Cm:
			return Unit::Cm;
		case UnitHashes::Mm:
			return Unit::Mm;
		case UnitHashes::Em:
			return Unit::Em;
		case UnitHashes::Ex:
			return Unit::Ex;
		case UnitHashes::Pt:
			return Unit::Pt;
		case UnitHashes::Pc:
			return Unit::Pc;
		case UnitHashes::Dpi:
			return Unit::Dpi;
		case UnitHashes::Dpcm:
			return Unit::Dpcm;
		case UnitHashes::Vw:
			return Unit::Vw;
		case UnitHashes::Vh:
			return Unit::Vh;
		case UnitHashes::Vmin:
			return Unit::Vmin;
		case UnitHashes::Vmax:
			return Unit::Vmax;
		case UnitHashes::Rem:
			return Unit::Rem;
		case UnitHashes::Dprd:
			return Unit::Dprd;
		case UnitHashes::Dpru:
			return Unit::Dpru;
		case UnitHashes::Dpr:
			return Unit::Dpr;
		case UnitHashes::Ch:
			return Unit::Ch;
	}
	return Unit::Dp;
}

std::string StyleSheetLength::unitToString( const StyleSheetLength::Unit& unit ) {
	switch ( unit ) {
		case Unit::Percentage:
			return "%";
		case Unit::Dp:
			return "dp";
		case Unit::Px:
			return "px";
		case Unit::In:
			return "in";
		case Unit::Cm:
			return "cm";
		case Unit::Mm:
			return "mm";
		case Unit::Em:
			return "em";
		case Unit::Ex:
			return "ex";
		case Unit::Pt:
			return "pt";
		case Unit::Pc:
			return "pc";
		case Unit::Dpi:
			return "dpi";
		case Unit::Dpcm:
			return "dpcm";
		case Unit::Vw:
			return "vw";
		case Unit::Vh:
			return "vh";
		case Unit::Vmin:
			return "vmin";
		case Unit::Vmax:
			return "vmax";
		case Unit::Rem:
			return "rem";
		case Unit::Dprd:
			return "dprd";
		case Unit::Dpru:
			return "dpru";
		case Unit::Dpr:
			return "dpr";
		case Unit::Ch:
			return "ch";
	}
	return "px";
}

bool StyleSheetLength::isLength( std::string_view unitStr ) {
	if ( unitStr.empty() )
		return false;

	size_t pos = 0;
	if ( unitStr[pos] == '-' || unitStr[pos] == '+' )
		pos++;

	bool hasDigit = false;
	bool hasDot = false;
	while ( pos < unitStr.size() ) {
		char c = unitStr[pos];
		if ( c >= '0' && c <= '9' ) {
			hasDigit = true;
			pos++;
		} else if ( c == '.' && !hasDot ) {
			hasDot = true;
			pos++;
		} else {
			break;
		}
	}

	if ( !hasDigit )
		return false;

	if ( pos < unitStr.size() && ( unitStr[pos] == 'e' || unitStr[pos] == 'E' ) ) {
		size_t expPos = pos + 1;
		if ( expPos < unitStr.size() && ( unitStr[expPos] == '-' || unitStr[expPos] == '+' ) )
			expPos++;
		bool hasExpDigit = false;
		while ( expPos < unitStr.size() && unitStr[expPos] >= '0' && unitStr[expPos] <= '9' ) {
			hasExpDigit = true;
			expPos++;
		}
		if ( hasExpDigit )
			pos = expPos;
	}

	std::string_view unit = unitStr.substr( pos );
	if ( unit.empty() )
		return true;

	auto unitType = unitFromString( unit );
	return unitType != StyleSheetLength::Unit::Px || unit == "px";
}

bool StyleSheetLength::isPercentage( std::string_view val ) {
	return !val.empty() && val.back() == '%';
}

StyleSheetLength::StyleSheetLength() : mUnit( Px ), mValue( 0 ) {}

StyleSheetLength::StyleSheetLength( const Float& val, const StyleSheetLength::Unit& unit ) :
	mUnit( unit ), mValue( val ) {}

StyleSheetLength::StyleSheetLength( const std::string& val, const Float& defaultValue ) :
	StyleSheetLength( fromString( val, defaultValue ) ) {}

StyleSheetLength::StyleSheetLength( const StyleSheetLength& val ) {
	mUnit = val.mUnit;
	mValue = val.mValue;
}

void StyleSheetLength::setValue( const Float& val, const StyleSheetLength::Unit& units ) {
	mValue = val;
	mUnit = units;
}

const Float& StyleSheetLength::getValue() const {
	return mValue;
}

const StyleSheetLength::Unit& StyleSheetLength::getUnit() const {
	return mUnit;
}

Float StyleSheetLength::asPixels( const Float& parentSize, const Sizef& viewSize,
								  const Float& displayDpi, const Float& elFontSize,
								  const Float& globalFontSize, Graphics::Font* font ) const {
	Float ret = 0;

	// CSS dictates a base 96 DPI for logical pixels.
	// We multiply by the device pixel ratio to get actual physical pixels on screen.
	const Float CSS_DPI = 96.f * PixelDensity::getPixelDensity();

	switch ( mUnit ) {
		case Unit::Percentage:
			ret = parentSize * mValue / 100.f;
			break;
		case Unit::Dp:
			ret = PixelDensity::dpToPx( mValue );
			break;
		case Unit::Dpr:
			return round( PixelDensity::dpToPx( mValue ) );
		case Unit::Dprd:
			ret = Math::roundDown( PixelDensity::dpToPx( mValue ) );
			break;
		case Unit::Dpru:
			ret = Math::roundUp( PixelDensity::dpToPx( mValue ) );
			break;
		case Unit::Em:
			ret = Math::round( mValue * elFontSize );
			break;
		case Unit::Ex:
			if ( font && elFontSize > 0 ) {
				Glyph glyph =
					font->getGlyph( 'x', static_cast<unsigned int>( elFontSize ), false, false );
				Float xHeight = eemax( 0.f, -glyph.bounds.Top );
				if ( xHeight <= 0 )
					xHeight = elFontSize * 0.5f;
				ret = Math::round( mValue * xHeight );
			} else {
				ret = Math::round( mValue * elFontSize * 0.5f );
			}
			break;
		case Unit::Ch:
			if ( font && elFontSize > 0 ) {
				Glyph glyph =
					font->getGlyph( '0', static_cast<unsigned int>( elFontSize ), false, false );
				ret = Math::round( mValue * eemax( 0.f, glyph.advance ) );
			} else {
				ret = Math::round( mValue * elFontSize * 0.5f );
			}
			break;
		case Unit::Pt:
			ret = mValue * CSS_DPI / 72.f;
			break;
		case Unit::Pc:
			ret = ( mValue * CSS_DPI / 72.f ) * 12.f;
			break;
		case Unit::In:
			ret = mValue * CSS_DPI;
			break;
		case Unit::Cm:
			ret = ( mValue * CSS_DPI ) / 2.54f;
			break;
		case Unit::Mm:
			ret = ( mValue * CSS_DPI ) / 25.4f;
			break;
		case Unit::Vw:
			ret = viewSize.getWidth() * mValue / 100.f;
			break;
		case Unit::Vh:
			ret = viewSize.getHeight() * mValue / 100.f;
			break;
		case Unit::Vmin:
			ret = eemin( viewSize.getHeight(), viewSize.getWidth() ) * mValue / 100.f;
			break;
		case Unit::Vmax:
			ret = eemax( viewSize.getHeight(), viewSize.getWidth() ) * mValue / 100.f;
			break;
		case Unit::Rem:
			ret = globalFontSize * mValue;
			break;
		case Unit::Dpi:
		case Unit::Dpcm:
			ret = (int)( mValue * 2.54 );
			break;
		case Unit::Px:
		default:
			ret = mValue;
			break;
	}
	return ret;
}

Float StyleSheetLength::asDp( const Float& parentSize, const Sizef& viewSize,
							  const Float& displayDpi, const Float& elFontSize,
							  const Float& globalFontSize, Graphics::Font* font ) const {
	return PixelDensity::pxToDp(
		asPixels( parentSize, viewSize, displayDpi, elFontSize, globalFontSize, font ) );
}

bool StyleSheetLength::operator==( const StyleSheetLength& length ) const {
	return mValue == length.mValue && mUnit == length.mUnit;
}

bool StyleSheetLength::operator!=( const StyleSheetLength& length ) const {
	return !( *this == length );
}

StyleSheetLength& StyleSheetLength::operator=( const Float& val ) {
	mValue = val;
	mUnit = Unit::Px;
	return *this;
}

StyleSheetLength& StyleSheetLength::operator=( const StyleSheetLength& val ) {
	mUnit = val.mUnit;
	mValue = val.mValue;
	return *this;
}

StyleSheetLength StyleSheetLength::fromString( const std::string& str, const Float& defaultValue,
											   bool pxAsDp ) {
	PercentagePositions isPercentage = isPercentagePosition( String::hashToLower( str ) );
	if ( PercentagePositions::None != isPercentage )
		return fromString( positionToPercentage( isPercentage ), defaultValue );

	StyleSheetLength length;
	length.setValue( defaultValue, Unit::Px );
	std::string num;
	std::string unit;

	for ( std::size_t i = 0; i < str.size(); i++ ) {
		char c = str[i];
		if ( String::isNumber( c, true, true ) || ( '-' == c && i == 0 ) ||
			 ( '+' == c && i == 0 ) ) {
			num += c;
		} else {
			unit = str.substr( i );
			break;
		}
	}

	if ( !num.empty() ) {
		Float val = 0;
		while ( !num.empty() && !String::fromString( val, num ) ) {
			unit = num.back() + unit;
			num.pop_back();
		}
		if ( !num.empty() )
			length.setValue( val, unitFromString( unit ) );
	}

	if ( pxAsDp && length.getUnit() == Unit::Px )
		length.mUnit = Unit::Dp;

	return length;
}

std::string StyleSheetLength::toString() const {
	std::string res( String::format( "%.2f", mValue ) );
	String::replace( res, ",", "." );
	String::numberCleanInPlace( res );
	res += unitToString( mUnit );
	return res;
}

}}} // namespace EE::UI::CSS
