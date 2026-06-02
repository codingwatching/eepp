#include <algorithm>
#include <cmath>
#include <eepp/graphics/globalbatchrenderer.hpp>
#include <eepp/graphics/lineargradientdrawable.hpp>
#include <eepp/math/math.hpp>

namespace EE { namespace Graphics {

LinearGradientDrawable* LinearGradientDrawable::New() {
	return eeNew( LinearGradientDrawable, () );
}

LinearGradientDrawable::LinearGradientDrawable() : Drawable( LINEARGRADIENT ) {}

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

	// CSS angle: 0° = up, 90° = right, 180° = down (default)
	Float aRad = mAngle * EE_PI / 180.f;
	Float dx = std::sin( aRad );
	Float dy = -std::cos( aRad );

	// Gradient line passes through box center
	Float cx = w * 0.5f;
	Float cy = h * 0.5f;

	// Find gradient line intersections with box boundaries
	Float tVals[4];
	int validCount = 0;

	auto addT = [&]( Float t, Float px, Float py ) {
		if ( !std::isfinite( t ) )
			return;
		// Clamp check point to box
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

	// Perpendicular direction (rotated 90° clockwise in screen coords)
	Float px = -dy;
	Float py = dx;

	BatchRenderer* sBR = GlobalBatchRenderer::instance();
	sBR->setTexture( NULL );
	sBR->setBlendMode( BlendMode::Alpha() );
	sBR->quadsBegin();

	// Pre-compute position on gradient line: center + t * dir
	Float txLen = tMax - tMin;

	// Pre-compute perpendicular intersections for all stops
	struct PerpSeg {
		Vector2f a;
		Vector2f b;
		Float gradPos; // t value along gradient line
	};
	std::vector<PerpSeg> segments;
	segments.reserve( mColorStops.size() );

	for ( size_t i = 0; i < mColorStops.size(); i++ ) {
		PerpSeg seg;
		Float t = tMin + mColorStops[i].position * txLen;
		seg.gradPos = t;
		Float gx = cx + t * dx;
		Float gy = cy + t * dy;

		// Find perpendicular line intersections with the box
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

	// Render quads between consecutive stops
	for ( size_t i = 0; i < mColorStops.size() - 1; i++ ) {
		const Color& c0 = mColorStops[i].color;
		const Color& c1 = mColorStops[i + 1].color;

		Color fc0 = ( mColor.a == 255 ) ? c0 : Color( c0 ).blendAlpha( mColor.a );
		Color fc1 = ( mColor.a == 255 ) ? c1 : Color( c1 ).blendAlpha( mColor.a );

		sBR->quadsSetColorFree( fc0, fc1, fc1, fc0 );

		const PerpSeg& s0 = segments[i];
		const PerpSeg& s1 = segments[i + 1];

		sBR->batchQuadFree( s0.a.x, s0.a.y, s1.a.x, s1.a.y, s1.b.x, s1.b.y, s0.b.x, s0.b.y );
	}

	sBR->draw();
}

const std::vector<LinearGradientDrawable::ColorStop>&
LinearGradientDrawable::getColorStops() const {
	return mColorStops;
}

void LinearGradientDrawable::setColorStops( std::vector<ColorStop> stops ) {
	mColorStops = std::move( stops );
	if ( mColorStops.size() >= 2 ) {
		std::sort(
			mColorStops.begin(), mColorStops.end(),
			[]( const ColorStop& a, const ColorStop& b ) { return a.position < b.position; } );
		mColorStops.front().position = 0.f;
		mColorStops.back().position = 1.f;
	}
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

}} // namespace EE::Graphics
