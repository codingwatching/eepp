#include <algorithm>
#include <cmath>

#include <eepp/graphics/globalbatchrenderer.hpp>
#include <eepp/math/math.hpp>
#include <eepp/ui/lineargradientdrawable.hpp>

namespace EE { namespace UI {

LinearGradientDrawable* LinearGradientDrawable::New() {
	return eeNew( LinearGradientDrawable, () );
}

LinearGradientDrawable* LinearGradientDrawable::NewRepeating() {
	return eeNew( LinearGradientDrawable, ( Graphics::Drawable::REPEATINGLINEARGRADIENT ) );
}

LinearGradientDrawable::LinearGradientDrawable( Graphics::Drawable::Type drawableType ) :
	Drawable( drawableType ) {}

Sizef LinearGradientDrawable::getSize() {
	return mSize;
}

Sizef LinearGradientDrawable::getPixelsSize() {
	return mSize;
}

void LinearGradientDrawable::draw() {
	draw( mPosition, mSize );
}

void LinearGradientDrawable::draw( const Vector2f& position ) {
	draw( position, mSize );
}

void LinearGradientDrawable::draw( const Vector2f& position, const Sizef& size ) {
	if ( mColorStops.size() < 2 || size.getWidth() <= 0 || size.getHeight() <= 0 )
		return;

	Float w = size.getWidth();
	Float h = size.getHeight();

	Float aRad = mAngle * EE_PI / 180.f;
	Float dx = std::sin( aRad );
	Float dy = -std::cos( aRad );

	Float cx = w * 0.5f;
	Float cy = h * 0.5f;

	Float tVals[4];
	int validCount = 0;

	auto addT = [&]( Float t, Float px, Float py ) {
		if ( !std::isfinite( t ) )
			return;
		if ( px >= -0.001f && px <= w + 0.001f && py >= -0.001f && py <= h + 0.001f ) {
			tVals[validCount++] = t;
		}
	};

	if ( std::abs( dx ) > 0.0001f ) {
		Float tLeft = -cx / dx;
		addT( tLeft, 0, cy + tLeft * dy );
		Float tRight = ( w - cx ) / dx;
		addT( tRight, w, cy + tRight * dy );
	}
	if ( std::abs( dy ) > 0.0001f ) {
		Float tTop = -cy / dy;
		addT( tTop, cx + tTop * dx, 0 );
		Float tBottom = ( h - cy ) / dy;
		addT( tBottom, cx + tBottom * dx, h );
	}

	if ( validCount < 2 )
		return;

	Float tMin = tVals[0];
	Float tMax = tVals[0];
	for ( int i = 1; i < validCount; i++ ) {
		if ( tVals[i] < tMin )
			tMin = tVals[i];
		if ( tVals[i] > tMax )
			tMax = tVals[i];
	}

	if ( std::abs( tMax - tMin ) < 0.0001f )
		return;

	Float txLen = tMax - tMin;

	// Normalize all stops to [0,1] using the gradient-line pixel length
	std::vector<ColorStop> stops;
	stops.reserve( mColorStops.size() );
	for ( const auto& s : mColorStops )
		stops.push_back(
			ColorStop( s.getNormalized( txLen ), s.color, CSS::StyleSheetLength::Percentage ) );

	std::sort( stops.begin(), stops.end(),
			   []( const ColorStop& a, const ColorStop& b ) { return a.value < b.value; } );


	Float px = -dy;
	Float py = dx;

	BatchRenderer* sBR = GlobalBatchRenderer::instance();
	sBR->setTexture( NULL );
	sBR->setBlendMode( BlendMode::Alpha() );
	sBR->quadsBegin();

	if ( isRepeating() ) {
		Float firstPos = stops.front().value;
		Float lastPos = stops.back().value;
		Float patternLen = lastPos - firstPos;
		if ( patternLen < 0.0001f ) {
			sBR->draw();
			return;
		}

		int rStart = (int)std::floor( ( -firstPos ) / patternLen );
		int rEnd = (int)std::ceil( ( 1.f - lastPos ) / patternLen );
		const int MAX_REPEATS = 128;
		if ( rEnd - rStart > MAX_REPEATS )
			rEnd = rStart + MAX_REPEATS;

		for ( int r = rStart; r <= rEnd; r++ ) {
			Float repeatOff = (Float)r * patternLen;

			for ( size_t i = 0; i + 1 < stops.size(); i++ ) {
				Float p0 = stops[i].value + repeatOff;
				Float p1 = stops[i + 1].value + repeatOff;

				if ( p1 <= 0.f || p0 >= 1.f )
					continue;

				Float clip0 = eemax( 0.f, p0 );
				Float clip1 = eemin( 1.f, p1 );
				if ( clip1 - clip0 < 0.0001f )
					continue;

				Float bw = p1 - p0;
				Float frac0 = ( clip0 - p0 ) / bw;
				Float frac1 = ( clip1 - p0 ) / bw;
				const Color& sc0 = stops[i].color;
				const Color& sc1 = stops[i + 1].color;
				Color cc0(
					(Uint8)( (Float)sc0.r + frac0 * (Float)( sc1.r - sc0.r ) ),
					(Uint8)( (Float)sc0.g + frac0 * (Float)( sc1.g - sc0.g ) ),
					(Uint8)( (Float)sc0.b + frac0 * (Float)( sc1.b - sc0.b ) ),
					(Uint8)( (Float)sc0.a + frac0 * (Float)( sc1.a - sc0.a ) ) );
				Color cc1(
					(Uint8)( (Float)sc0.r + frac1 * (Float)( sc1.r - sc0.r ) ),
					(Uint8)( (Float)sc0.g + frac1 * (Float)( sc1.g - sc0.g ) ),
					(Uint8)( (Float)sc0.b + frac1 * (Float)( sc1.b - sc0.b ) ),
					(Uint8)( (Float)sc0.a + frac1 * (Float)( sc1.a - sc0.a ) ) );

				Float tc0 = tMin + clip0 * txLen;
				Float tc1 = tMin + clip1 * txLen;
				Float gx0 = cx + tc0 * dx;
				Float gy0 = cy + tc0 * dy;
				Float gx1 = cx + tc1 * dx;
				Float gy1 = cy + tc1 * dy;

				Float ptVals0[4];
				int ptCount0 = 0;
				auto addPT0 = [&]( Float pt, Float qx, Float qy ) {
					if ( !std::isfinite( pt ) )
						return;
					if ( qx >= -0.001f && qx <= w + 0.001f && qy >= -0.001f && qy <= h + 0.001f )
						ptVals0[ptCount0++] = pt;
				};
				if ( std::abs( px ) > 0.0001f ) {
					Float ptLeft = -gx0 / px;
					addPT0( ptLeft, 0, gy0 + ptLeft * py );
					Float ptRight = ( w - gx0 ) / px;
					addPT0( ptRight, w, gy0 + ptRight * py );
				}
				if ( std::abs( py ) > 0.0001f ) {
					Float ptTop = -gy0 / py;
					addPT0( ptTop, gx0 + ptTop * px, 0 );
					Float ptBottom = ( h - gy0 ) / py;
					addPT0( ptBottom, gx0 + ptBottom * px, h );
				}
				if ( ptCount0 < 2 )
					continue;
				Float ptMin0 = ptVals0[0], ptMax0 = ptVals0[0];
				for ( int j = 1; j < ptCount0; j++ ) {
					if ( ptVals0[j] < ptMin0 )
						ptMin0 = ptVals0[j];
					if ( ptVals0[j] > ptMax0 )
						ptMax0 = ptVals0[j];
				}
				Vector2f a0( gx0 + ptMin0 * px + position.x, gy0 + ptMin0 * py + position.y );
				Vector2f b0( gx0 + ptMax0 * px + position.x, gy0 + ptMax0 * py + position.y );

				Float ptVals1[4];
				int ptCount1 = 0;
				auto addPT1 = [&]( Float pt, Float qx, Float qy ) {
					if ( !std::isfinite( pt ) )
						return;
					if ( qx >= -0.001f && qx <= w + 0.001f && qy >= -0.001f && qy <= h + 0.001f )
						ptVals1[ptCount1++] = pt;
				};
				if ( std::abs( px ) > 0.0001f ) {
					Float ptLeft = -gx1 / px;
					addPT1( ptLeft, 0, gy1 + ptLeft * py );
					Float ptRight = ( w - gx1 ) / px;
					addPT1( ptRight, w, gy1 + ptRight * py );
				}
				if ( std::abs( py ) > 0.0001f ) {
					Float ptTop = -gy1 / py;
					addPT1( ptTop, gx1 + ptTop * px, 0 );
					Float ptBottom = ( h - gy1 ) / py;
					addPT1( ptBottom, gx1 + ptBottom * px, h );
				}
				if ( ptCount1 < 2 )
					continue;
				Float ptMin1 = ptVals1[0], ptMax1 = ptVals1[0];
				for ( int j = 1; j < ptCount1; j++ ) {
					if ( ptVals1[j] < ptMin1 )
						ptMin1 = ptVals1[j];
					if ( ptVals1[j] > ptMax1 )
						ptMax1 = ptVals1[j];
				}
				Vector2f a1( gx1 + ptMin1 * px + position.x, gy1 + ptMin1 * py + position.y );
				Vector2f b1( gx1 + ptMax1 * px + position.x, gy1 + ptMax1 * py + position.y );

				Color fc0 = ( mColor.a == 255 ) ? cc0 : Color( cc0 ).blendAlpha( mColor.a );
				Color fc1 = ( mColor.a == 255 ) ? cc1 : Color( cc1 ).blendAlpha( mColor.a );
				sBR->quadsSetColorFree( fc0, fc1, fc1, fc0 );
				sBR->batchQuadFree( a0.x, a0.y, a1.x, a1.y, b1.x, b1.y, b0.x, b0.y );
			}
		}
	} else {
		struct PerpSeg {
			Vector2f a;
			Vector2f b;
		};
		std::vector<PerpSeg> segments;
		segments.reserve( stops.size() );

		for ( size_t i = 0; i < stops.size(); i++ ) {
			PerpSeg seg;
			Float t = tMin + stops[i].value * txLen;
			Float gx = cx + t * dx;
			Float gy = cy + t * dy;

			Float ptVals[4];
			int ptCount = 0;

			auto addPT = [&]( Float pt, Float qx, Float qy ) {
				if ( !std::isfinite( pt ) )
					return;
				if ( qx >= -0.001f && qx <= w + 0.001f && qy >= -0.001f && qy <= h + 0.001f ) {
					ptVals[ptCount++] = pt;
				}
			};

			if ( std::abs( px ) > 0.0001f ) {
				Float ptLeft = -gx / px;
				addPT( ptLeft, 0, gy + ptLeft * py );
				Float ptRight = ( w - gx ) / px;
				addPT( ptRight, w, gy + ptRight * py );
			}
			if ( std::abs( py ) > 0.0001f ) {
				Float ptTop = -gy / py;
				addPT( ptTop, gx + ptTop * px, 0 );
				Float ptBottom = ( h - gy ) / py;
				addPT( ptBottom, gx + ptBottom * px, h );
			}

			if ( ptCount < 2 )
				return;

			Float ptMin = ptVals[0];
			Float ptMax = ptVals[0];
			for ( int j = 1; j < ptCount; j++ ) {
				if ( ptVals[j] < ptMin )
					ptMin = ptVals[j];
				if ( ptVals[j] > ptMax )
					ptMax = ptVals[j];
			}

			seg.a = Vector2f( gx + ptMin * px + position.x, gy + ptMin * py + position.y );
			seg.b = Vector2f( gx + ptMax * px + position.x, gy + ptMax * py + position.y );
			segments.push_back( seg );
		}

		for ( size_t i = 0; i < stops.size() - 1; i++ ) {
			const Color& c0 = stops[i].color;
			const Color& c1 = stops[i + 1].color;

			Color fc0 = ( mColor.a == 255 ) ? c0 : Color( c0 ).blendAlpha( mColor.a );
			Color fc1 = ( mColor.a == 255 ) ? c1 : Color( c1 ).blendAlpha( mColor.a );

			sBR->quadsSetColorFree( fc0, fc1, fc1, fc0 );

			const PerpSeg& s0 = segments[i];
			const PerpSeg& s1 = segments[i + 1];

			sBR->batchQuadFree( s0.a.x, s0.a.y, s1.a.x, s1.a.y, s1.b.x, s1.b.y, s0.b.x, s0.b.y );
		}
	}

	sBR->draw();
}

const std::vector<LinearGradientDrawable::ColorStop>&
LinearGradientDrawable::getColorStops() const {
	return mColorStops;
}

void LinearGradientDrawable::setColorStops( std::vector<ColorStop> stops ) {
	mColorStops = std::move( stops );
}

Float LinearGradientDrawable::getAngle() const {
	return mAngle;
}

void LinearGradientDrawable::setAngle( Float angleDegrees ) {
	mAngle = angleDegrees;
}

void LinearGradientDrawable::setSize( const Sizef& size ) {
	mSize = size;
}

bool LinearGradientDrawable::isRepeating() const {
	return mDrawableType == Graphics::Drawable::REPEATINGLINEARGRADIENT;
}

}} // namespace EE::UI
