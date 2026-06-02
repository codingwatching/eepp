#ifndef EE_GRAPHICS_RADIALGRADIENTDRAWABLE_HPP
#define EE_GRAPHICS_RADIALGRADIENTDRAWABLE_HPP

#include <eepp/graphics/drawable.hpp>
#include <eepp/system/color.hpp>
#include <vector>

namespace EE { namespace Graphics {

class EE_API RadialGradientDrawable : public Drawable {
  public:
	struct ColorStop {
		Float position{ 0 };
		Color color;

		ColorStop() : color( Color::White ) {}
		ColorStop( Float pos, const Color& col ) : position( pos ), color( col ) {}
	};

	enum ShapeType { CIRCLE, ELLIPSE };
	enum Extent { CLOSEST_SIDE, FARTHEST_SIDE, CLOSEST_CORNER, FARTHEST_CORNER };

	static RadialGradientDrawable* New();
	static RadialGradientDrawable* NewRepeating();

	RadialGradientDrawable( Drawable::Type drawableType = RADIALGRADIENT );

	virtual Sizef getSize();

	virtual Sizef getPixelsSize();

	virtual void draw();

	virtual void draw( const Vector2f& position );

	virtual void draw( const Vector2f& position, const Sizef& size );

	virtual bool isStateful() { return false; }

	const std::vector<ColorStop>& getColorStops() const;

	void setColorStops( std::vector<ColorStop> stops );

	ShapeType getShape() const;

	void setShape( ShapeType shape );

	Extent getExtent() const;

	void setExtent( Extent extent );

	const Vector2f& getCenter() const;

	void setCenter( const Vector2f& centerNormalized );

	void setSize( const Sizef& size );

	bool isRepeating() const;

  protected:
	std::vector<ColorStop> mColorStops;
	ShapeType mShape{ CIRCLE };
	Extent mExtent{ FARTHEST_CORNER };
	Vector2f mCenter{ 0.5f, 0.5f };
	Sizef mSize;
};

}} // namespace EE::Graphics

#endif
