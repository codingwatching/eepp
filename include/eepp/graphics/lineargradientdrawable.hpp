#ifndef EE_GRAPHICS_LINEARGRADIENTDRAWABLE_HPP
#define EE_GRAPHICS_LINEARGRADIENTDRAWABLE_HPP

#include <eepp/graphics/drawable.hpp>
#include <eepp/system/color.hpp>
#include <vector>

namespace EE { namespace Graphics {

class EE_API LinearGradientDrawable : public Drawable {
  public:
	struct ColorStop {
		Float position{ 0 }; /* 0.0 to 1.0 */
		Color color;

		ColorStop() : color( Color::White ) {}
		ColorStop( Float pos, const Color& col ) : position( pos ), color( col ) {}
	};

	static LinearGradientDrawable* New();

	LinearGradientDrawable();

	virtual Sizef getSize();

	virtual Sizef getPixelsSize();

	virtual void draw();

	virtual void draw( const Vector2f& position );

	virtual void draw( const Vector2f& position, const Sizef& size );

	virtual bool isStateful() { return false; }

	const std::vector<ColorStop>& getColorStops() const;

	void setColorStops( std::vector<ColorStop> stops );

	Float getAngle() const;

	void setAngle( Float angleDegrees );

	void setSize( const Sizef& size );

  protected:
	std::vector<ColorStop> mColorStops;
	Float mAngle{ 180.f }; /* CSS angle: 0=to top, 90=to right, 180=to bottom (default) */
	Sizef mSize;
};

}} // namespace EE::Graphics

#endif
