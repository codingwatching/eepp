#include <eepp/ui/css/stylesheetproperty.hpp>
#include <eepp/ui/flexlayouter.hpp>
#include <eepp/ui/uihtmlwidget.hpp>
#include <eepp/ui/uilayout.hpp>
#include <eepp/ui/uirichtext.hpp>
#include <eepp/ui/uistyle.hpp>
#include <eepp/ui/uitextnode.hpp>
#include <eepp/ui/uitextspan.hpp>

namespace EE { namespace UI {

void FlexLayouter::collectFlexItems( SmallVector<FlexItem, 16>& items ) {
	items.clear();

	Node* child = mContainer->getFirstChild();
	while ( child ) {
		if ( !child->isWidget() ) {
			child = child->getNextNode();
			continue;
		}

		UIWidget* widget = child->asType<UIWidget>();

		if ( !widget->isVisible() ) {
			child = child->getNextNode();
			continue;
		}

		if ( widget->isType( UI_TYPE_TEXTNODE ) ) {
			auto* textNode = widget->asType<UITextNode>();
			if ( textNode && textNode->isWhitespaceOnly() ) {
				child = child->getNextNode();
				continue;
			}
		}

		if ( widget->isType( UI_TYPE_HTML_WIDGET ) ) {
			auto* htmlWidget = widget->asType<UIHTMLWidget>();

			if ( htmlWidget->getDisplay() == CSSDisplay::None || htmlWidget->isOutOfFlow() ) {
				child = child->getNextNode();
				continue;
			}
		}

		FlexItem item;
		item.widget = widget;
		readItemStyle( widget, item );

		items.push_back( item );
		child = child->getNextNode();
	}

	std::sort( items.begin(), items.end(),
			   []( const FlexItem& a, const FlexItem& b ) { return a.order < b.order; } );
}

FlexLayouter::Axis FlexLayouter::getMainAxis( CSSFlexDirection direction ) const {
	switch ( direction ) {
		case CSSFlexDirection::Row:
			return { true, false };
		case CSSFlexDirection::RowReverse:
			return { true, true };
		case CSSFlexDirection::Column:
			return { false, false };
		case CSSFlexDirection::ColumnReverse:
			return { false, true };
	}
	return { true, false };
}

FlexLayouter::Axis FlexLayouter::getCrossAxis( CSSFlexDirection direction ) const {
	switch ( direction ) {
		case CSSFlexDirection::Row:
		case CSSFlexDirection::RowReverse:
			return { false, false };
		case CSSFlexDirection::Column:
		case CSSFlexDirection::ColumnReverse:
			return { true, false };
	}
	return { false, false };
}

void FlexLayouter::readContainerStyle( CSSFlexDirection& direction, CSSFlexWrap& wrap,
									   CSSJustifyContent& justify, CSSAlignItems& alignItems,
									   CSSAlignContent& alignContent, Float& columnGap,
									   Float& rowGap ) {
	if ( !mContainer->isType( UI_TYPE_HTML_WIDGET ) )
		return;

	auto* widget = mContainer->asType<UIHTMLWidget>();
	direction = widget->getFlexDirection();
	wrap = widget->getFlexWrap();
	justify = widget->getJustifyContent();
	alignItems = widget->getAlignItems();
	alignContent = widget->getAlignContent();

	columnGap = mContainer->lengthFromValue(
		widget->getColumnGap(), CSS::PropertyRelativeTarget::ContainingBlockWidth, 0.f );
	rowGap = mContainer->lengthFromValue( widget->getRowGap(),
										  CSS::PropertyRelativeTarget::ContainingBlockWidth, 0.f );
}

void FlexLayouter::readItemStyle( UIWidget* child, FlexItem& item ) {
	if ( !child->isType( UI_TYPE_HTML_WIDGET ) )
		return;

	auto* widget = child->asType<UIHTMLWidget>();
	item.flexGrow = widget->getFlexGrow();
	item.flexShrink = widget->getFlexShrink();

	std::string val = widget->getFlexBasis();
	String::toLowerInPlace( val );
	if ( val == "auto" || val == "content" ) {
		item.flexBasisAuto = true;
		item.flexBasisValue = 0.f;
		item.flexBasisIsPercentage = false;
	} else {
		item.flexBasisAuto = false;
		item.flexBasisIsPercentage = val.find( '%' ) != std::string::npos;
		item.flexBasisValue = mContainer->lengthFromValue(
			val, CSS::PropertyRelativeTarget::ContainingBlockWidth, 0.f );
	}

	item.alignSelf = widget->getAlignSelf();
	item.order = widget->getOrder();
}

Float FlexLayouter::resolveFlexBasis( UIWidget* child, CSSFlexDirection, Float flexBasisValue,
									  bool flexBasisAuto, const Axis& mainAxis ) {
	if ( flexBasisAuto ) {
		Float intrinsic = 0.f;
		if ( mainAxis.horizontal && child->getLayoutWidthPolicy() == SizePolicy::Fixed &&
			 child->getUIStyle() ) {
			const auto* wprop = child->getUIStyle()->getProperty( PropertyId::Width );
			if ( wprop )
				return child->lengthFromValue( *wprop );
		}
		if ( !mainAxis.horizontal && child->getLayoutHeightPolicy() == SizePolicy::Fixed &&
			 child->getUIStyle() ) {
			const auto* hprop = child->getUIStyle()->getProperty( PropertyId::Height );
			if ( hprop )
				return child->lengthFromValue( *hprop );
		}

		if ( mainAxis.horizontal )
			intrinsic = child->getPixelsSize().getWidth();
		else
			intrinsic = child->getPixelsSize().getHeight();

		return intrinsic;
	}
	return flexBasisValue;
}

Float FlexLayouter::getItemMainSize( UIWidget* child, const Axis& mainAxis ) const {
	return mainAxis.horizontal ? child->getPixelsSize().getWidth()
							   : child->getPixelsSize().getHeight();
}

Float FlexLayouter::getItemCrossSize( UIWidget* child, const Axis& crossAxis ) const {
	return crossAxis.horizontal ? child->getPixelsSize().getWidth()
								: child->getPixelsSize().getHeight();
}

void FlexLayouter::measureFlexItems( const Axis& mainAxis, const Axis& crossAxis,
									 const Float containerCrossSize, const Float containerWidth,
									 const Float containerHeight, const Rectf& containerPadding,
									 bool indefiniteMainSize, bool indefiniteCrossSize ) {
	for ( auto& item : mItems ) {
		Float tentativeCrossSize = containerCrossSize;
		if ( indefiniteCrossSize ) {
			tentativeCrossSize = 0.f;
		} else if ( tentativeCrossSize <= 0.f && crossAxis.horizontal ) {
			tentativeCrossSize = containerWidth - containerPadding.Left - containerPadding.Right;
		} else if ( tentativeCrossSize <= 0.f && !crossAxis.horizontal ) {
			tentativeCrossSize = containerHeight - containerPadding.Top - containerPadding.Bottom;
		}
		if ( tentativeCrossSize < 0.f )
			tentativeCrossSize = 0.f;

		if ( crossAxis.horizontal && tentativeCrossSize > 0.f &&
			 item.widget->getLayoutWidthPolicy() != SizePolicy::Fixed )
			item.widget->setInternalPixelsWidth( tentativeCrossSize );
		if ( !crossAxis.horizontal && tentativeCrossSize > 0.f &&
			 item.widget->getLayoutHeightPolicy() != SizePolicy::Fixed )
			item.widget->setInternalPixelsHeight( tentativeCrossSize );

		SizePolicy oldMainPolicy = mainAxis.horizontal ? item.widget->getLayoutWidthPolicy()
													   : item.widget->getLayoutHeightPolicy();
		if ( oldMainPolicy == SizePolicy::MatchParent ) {
			if ( mainAxis.horizontal )
				item.widget->setLayoutWidthPolicy( SizePolicy::WrapContent );
			else
				item.widget->setLayoutHeightPolicy( SizePolicy::WrapContent );
		}

		if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) ) {
			auto* childHtml = item.widget->asType<UIHTMLWidget>();
			auto* layouter = childHtml->getLayouter();
			if ( layouter && !layouter->isPacking() && layouter != this ) {
				childHtml->updateLayout();
			}
		}

		if ( oldMainPolicy == SizePolicy::MatchParent ) {
			if ( mainAxis.horizontal )
				item.widget->setLayoutWidthPolicy( oldMainPolicy );
			else
				item.widget->setLayoutHeightPolicy( oldMainPolicy );
		}

		// Resolve margins along the flex axes.
		// Auto margins on the main axis are handled by the flex algorithm
		// (CSS Flexbox §8.1), so we treat them as 0 here.
		Rectf margin = item.widget->getLayoutPixelsMargin();
		if ( mainAxis.horizontal ) {
			item.marginMainStart = item.widget->hasLayoutMarginLeftAuto() ? 0.f : margin.Left;
			item.marginMainEnd = item.widget->hasLayoutMarginRightAuto() ? 0.f : margin.Right;
			item.marginCrossStart = margin.Top;
			item.marginCrossEnd = margin.Bottom;
			item.hasAutoMarginCrossStart = item.widget->hasLayoutMarginTopAuto();
			item.hasAutoMarginCrossEnd = item.widget->hasLayoutMarginBottomAuto();
		} else {
			item.marginMainStart = item.widget->hasLayoutMarginTopAuto() ? 0.f : margin.Top;
			item.marginMainEnd = item.widget->hasLayoutMarginBottomAuto() ? 0.f : margin.Bottom;
			item.marginCrossStart = margin.Left;
			item.marginCrossEnd = margin.Right;
			item.hasAutoMarginCrossStart = item.widget->hasLayoutMarginLeftAuto();
			item.hasAutoMarginCrossEnd = item.widget->hasLayoutMarginRightAuto();
		}

		// Zero out cross-axis auto margins since they absorb free space during
		// cross-axis alignment and should not contribute to line cross sizing.
		if ( item.hasAutoMarginCrossStart )
			item.marginCrossStart = 0.f;
		if ( item.hasAutoMarginCrossEnd )
			item.marginCrossEnd = 0.f;

		item.targetMainSize = resolveFlexBasis( item.widget, mDirection, item.flexBasisValue,
												item.flexBasisAuto, mainAxis );

		// CSS Flexbox §9.2: percentage flex-basis against indefinite main size behaves as auto
		if ( indefiniteMainSize && item.flexBasisIsPercentage ) {
			item.targetMainSize = getItemMainSize( item.widget, mainAxis );
		}

		if ( mainAxis.horizontal && item.widget->getLayoutWidthPolicy() == SizePolicy::Fixed &&
			 item.widget->getUIStyle() ) {
			const auto* wprop = item.widget->getUIStyle()->getProperty( PropertyId::Width );
			if ( wprop )
				item.targetMainSize = item.widget->lengthFromValue( *wprop );
		} else if ( !mainAxis.horizontal &&
					item.widget->getLayoutHeightPolicy() == SizePolicy::Fixed &&
					item.widget->getUIStyle() ) {
			const auto* hprop = item.widget->getUIStyle()->getProperty( PropertyId::Height );
			if ( hprop )
				item.targetMainSize = item.widget->lengthFromValue( *hprop );
		}

		// Compute min/max main size (CSS §4.5: min-width/min-height default is auto for flex items)
		if ( mainAxis.horizontal ) {
			if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) &&
				 item.widget->asType<UIHTMLWidget>()->getLayouter() )
				item.minMainSize =
					item.widget->asType<UIHTMLWidget>()->getLayouter()->getMinIntrinsicWidth();
			else
				item.minMainSize = item.widget->getPixelsSize().getWidth();
			if ( item.minMainSize < 0.f )
				item.minMainSize = 0.f;

			// Per §4.5: if the main-axis overflow is scrollable (non-visible),
			// the automatic minimum size is zero, not the content-based minimum.
			if ( item.widget->getUIStyle() ) {
				const auto* ov = item.widget->getUIStyle()->getProperty( PropertyId::Overflow );
				if ( ov ) {
					std::string val = ov->asString();
					String::toLowerInPlace( val );
					if ( val != "visible" )
						item.minMainSize = 0.f;
				}
			}

			// Clamp by explicit min-width/max-width CSS properties
			if ( item.widget->getUIStyle() ) {
				const auto* minW = item.widget->getUIStyle()->getProperty( PropertyId::MinWidth );
				if ( minW ) {
					Float explicitMin = item.widget->lengthFromValue( *minW );
					if ( explicitMin > item.minMainSize )
						item.minMainSize = explicitMin;
				}
				const auto* maxW = item.widget->getUIStyle()->getProperty( PropertyId::MaxWidth );
				if ( maxW )
					item.maxMainSize = item.widget->lengthFromValue( *maxW );
				else
					item.maxMainSize = std::numeric_limits<Float>::max();
			}
		} else {
			item.minMainSize = item.widget->getPixelsSize().getHeight();
			if ( item.minMainSize < 0.f )
				item.minMainSize = 0.f;

			// Per §4.5: if the main-axis overflow is scrollable (non-visible),
			// the automatic minimum size is zero.
			if ( item.widget->getUIStyle() ) {
				const auto* ov = item.widget->getUIStyle()->getProperty( PropertyId::Overflow );
				if ( ov ) {
					std::string val = ov->asString();
					String::toLowerInPlace( val );
					if ( val != "visible" )
						item.minMainSize = 0.f;
				}
			}

			// Clamp by explicit min-height/max-height CSS properties
			if ( item.widget->getUIStyle() ) {
				const auto* minH = item.widget->getUIStyle()->getProperty( PropertyId::MinHeight );
				if ( minH ) {
					Float explicitMin = item.widget->lengthFromValue( *minH );
					if ( explicitMin > item.minMainSize )
						item.minMainSize = explicitMin;
				}
				const auto* maxH = item.widget->getUIStyle()->getProperty( PropertyId::MaxHeight );
				if ( maxH )
					item.maxMainSize = item.widget->lengthFromValue( *maxH );
				else
					item.maxMainSize = std::numeric_limits<Float>::max();
			}
		}
	}
}

void FlexLayouter::generateFlexLines( SmallVector<FlexLayouter::FlexLine, 8>& lines,
									  const Float containerMainSize, const Float columnGap ) {
	lines.clear();

	SmallVector<size_t, 16> remaining;
	for ( size_t i = 0; i < mItems.size(); i++ )
		remaining.push_back( i );

	while ( !remaining.empty() ) {
		FlexLine line;
		Float usedMain = 0.f;

		SmallVector<size_t, 16> nextRemaining;

		for ( size_t idx : remaining ) {
			const auto& item = mItems[idx];
			Float itemOuterSize = item.targetMainSize + item.marginMainStart + item.marginMainEnd;
			Float gap = line.itemIndices.empty() ? 0.f : columnGap;

			if ( mWrap == CSSFlexWrap::NoWrap ||
				 ( usedMain + gap + itemOuterSize <= containerMainSize &&
				   usedMain + gap + itemOuterSize >= 0.f ) ) {
				line.itemIndices.push_back( idx );
				usedMain += gap + itemOuterSize;
			} else {
				nextRemaining.push_back( idx );
			}
		}

		// Per CSS Flexbox §9.4, every flex line must contain at least one
		// flex item. If the container is narrower than any single item,
		// force the first remaining item into this line to avoid an
		// infinite loop.
		if ( line.itemIndices.empty() && !remaining.empty() ) {
			line.itemIndices.push_back( remaining.front() );
			nextRemaining.clear();
			for ( size_t i = 1; i < remaining.size(); i++ )
				nextRemaining.push_back( remaining[i] );
		}

		lines.push_back( std::move( line ) );
		remaining = std::move( nextRemaining );

		if ( mWrap == CSSFlexWrap::NoWrap )
			break;
	}

	if ( mWrap == CSSFlexWrap::WrapReverse )
		std::reverse( lines.begin(), lines.end() );
}

static CSSAlignSelf resolveAlignSelf( CSSAlignSelf alignSelf, CSSAlignItems alignItems ) {
	if ( alignSelf == CSSAlignSelf::Auto ) {
		switch ( alignItems ) {
			case CSSAlignItems::FlexStart:
				return CSSAlignSelf::FlexStart;
			case CSSAlignItems::FlexEnd:
				return CSSAlignSelf::FlexEnd;
			case CSSAlignItems::Center:
				return CSSAlignSelf::Center;
			case CSSAlignItems::Baseline:
				return CSSAlignSelf::Baseline;
			case CSSAlignItems::Stretch:
				return CSSAlignSelf::Stretch;
		}
	}
	return alignSelf;
}

void FlexLayouter::resolveFlexibleLengths( FlexLine& line, const Float containerMainSize,
										   const Float columnGap ) {
	if ( line.itemIndices.empty() )
		return;

	size_t itemCount = line.itemIndices.size();
	Float totalGaps = ( itemCount > 1 ) ? (Float)( itemCount - 1 ) * columnGap : 0.f;

	// Save original flex base sizes and compute totals
	SmallVector<Float, 16> baseSizes;
	Float totalFlexGrow = 0.f;
	Float totalFlexShrink = 0.f;
	for ( auto idx : line.itemIndices ) {
		baseSizes.push_back( mItems[idx].targetMainSize );
		totalFlexGrow += mItems[idx].flexGrow;
		totalFlexShrink += mItems[idx].flexShrink;
	}

	// Reset frozen state for all items on this line
	for ( auto idx : line.itemIndices )
		mItems[idx].frozen = false;

	// Compute initial free space using original flex base sizes
	Float totalOuter = 0.f;
	for ( size_t i = 0; i < itemCount; ++i ) {
		size_t idx = line.itemIndices[i];
		totalOuter += baseSizes[i] + mItems[idx].marginMainStart + mItems[idx].marginMainEnd;
	}
	Float freeSpace = containerMainSize - totalOuter - totalGaps;

	bool isGrow = freeSpace > 0.f && totalFlexGrow > 0.f;
	bool isShrink = freeSpace < 0.f && totalFlexShrink > 0.f;

	if ( freeSpace == 0.f || ( !isGrow && !isShrink ) ) {
		// No free space to distribute, or no flexing possible — just clamp
		for ( auto idx : line.itemIndices ) {
			auto& item = mItems[idx];
			if ( item.targetMainSize < item.minMainSize )
				item.targetMainSize = item.minMainSize;
			if ( item.targetMainSize > item.maxMainSize )
				item.targetMainSize = item.maxMainSize;
		}
		return;
	}

	// Freeze items with zero flex factor in the current direction
	for ( auto idx : line.itemIndices ) {
		if ( isGrow ? ( mItems[idx].flexGrow <= 0.f ) : ( mItems[idx].flexShrink <= 0.f ) )
			mItems[idx].frozen = true;
	}

	// Reset targetMainSize to base sizes before the first distribution
	for ( size_t i = 0; i < itemCount; ++i )
		mItems[line.itemIndices[i]].targetMainSize = baseSizes[i];

	// Helper: compute remaining free space using frozen items' clamped sizes
	// and unfrozen items' original flex base sizes.
	auto computeRemaining = [&]() -> Float {
		Float sum = totalGaps;
		for ( size_t i = 0; i < itemCount; ++i ) {
			size_t idx = line.itemIndices[i];
			const auto& item = mItems[idx];
			if ( item.frozen )
				sum += item.targetMainSize + item.marginMainStart + item.marginMainEnd;
			else
				sum += baseSizes[i] + item.marginMainStart + item.marginMainEnd;
		}
		return containerMainSize - sum;
	};

	// Iterative distribution loop (max 100 iterations as safety)
	for ( int iter = 0; iter < 100; ++iter ) {
		Float remaining = computeRemaining();
		if ( remaining == 0.f )
			break;

		if ( remaining > 0.f ) {
			// --- Grow distribution ---
			Float curGrow = 0.f;
			for ( auto idx : line.itemIndices )
				if ( !mItems[idx].frozen )
					curGrow += mItems[idx].flexGrow;
			if ( curGrow <= 0.f )
				break;

			Float used = 0.f;
			for ( size_t i = 0; i < itemCount; ++i ) {
				size_t idx = line.itemIndices[i];
				auto& item = mItems[idx];
				if ( item.frozen || item.flexGrow <= 0.f )
					continue;
				Float add = ( remaining * item.flexGrow ) / curGrow;
				item.targetMainSize = baseSizes[i] + add;
				used += add;
			}
			if ( curGrow > 0.f && ( remaining - used ) > 0.5f ) {
				for ( auto idx : line.itemIndices ) {
					if ( !mItems[idx].frozen && mItems[idx].flexGrow > 0.f ) {
						mItems[idx].targetMainSize += ( remaining - used );
						break;
					}
				}
			}
		} else {
			// --- Shrink distribution ---
			Float deficit = -remaining;
			Float totalScaledShrink = 0.f;
			for ( size_t i = 0; i < itemCount; ++i ) {
				size_t idx = line.itemIndices[i];
				if ( !mItems[idx].frozen && mItems[idx].flexShrink > 0.f )
					totalScaledShrink += mItems[idx].flexShrink * baseSizes[i];
			}
			if ( totalScaledShrink <= 0.f )
				break;

			Float used = 0.f;
			for ( size_t i = 0; i < itemCount; ++i ) {
				size_t idx = line.itemIndices[i];
				auto& item = mItems[idx];
				if ( item.frozen || item.flexShrink <= 0.f )
					continue;
				Float shrink = deficit * ( item.flexShrink * baseSizes[i] ) / totalScaledShrink;
				Float newMain = baseSizes[i] - shrink;
				if ( newMain < 0.f )
					newMain = 0.f;
				item.targetMainSize = newMain;
				used += baseSizes[i] - item.targetMainSize;
			}
			if ( ( deficit - used ) > 0.5f ) {
				for ( auto idx : line.itemIndices ) {
					if ( !mItems[idx].frozen && mItems[idx].targetMainSize > 0.f ) {
						mItems[idx].targetMainSize -= ( deficit - used );
						break;
					}
				}
			}
		}

		// Clamp each unfrozen item and freeze those that hit min/max
		bool anyViolation = false;
		for ( size_t i = 0; i < itemCount; ++i ) {
			size_t idx = line.itemIndices[i];
			auto& item = mItems[idx];
			if ( item.frozen )
				continue;
			if ( item.targetMainSize < item.minMainSize ) {
				item.targetMainSize = item.minMainSize;
				item.frozen = true;
				anyViolation = true;
			} else if ( item.targetMainSize > item.maxMainSize ) {
				item.targetMainSize = item.maxMainSize;
				item.frozen = true;
				anyViolation = true;
			}
		}

		if ( !anyViolation )
			break;
	}
}

void FlexLayouter::alignMainAxis( FlexLine& line, const Float containerMainSize,
								  const Float columnGap ) {
	if ( line.itemIndices.empty() )
		return;

	size_t itemCount = line.itemIndices.size();
	Float totalItemMain = 0.f;
	for ( size_t idx : line.itemIndices ) {
		const auto& item = mItems[idx];
		totalItemMain += item.targetMainSize + item.marginMainStart + item.marginMainEnd;
	}

	Float totalGap = ( itemCount > 1 ) ? (Float)( itemCount - 1 ) * columnGap : 0.f;
	Float freeSpaceMain = containerMainSize - totalItemMain - totalGap;

	Float mainOffset = 0.f;
	Float spacing = columnGap;

	switch ( mJustify ) {
		case CSSJustifyContent::FlexEnd:
			mainOffset = freeSpaceMain;
			break;
		case CSSJustifyContent::Center:
			mainOffset = freeSpaceMain * 0.5f;
			break;
		case CSSJustifyContent::SpaceBetween:
			if ( freeSpaceMain > 0.f && itemCount > 1 )
				spacing = columnGap + freeSpaceMain / (Float)( itemCount - 1 );
			else
				mainOffset = 0.f;
			break;
		case CSSJustifyContent::SpaceAround:
			if ( freeSpaceMain > 0.f && itemCount > 0 ) {
				spacing = columnGap + freeSpaceMain / (Float)itemCount;
				mainOffset = spacing * 0.5f;
			}
			break;
		case CSSJustifyContent::SpaceEvenly:
			if ( freeSpaceMain > 0.f && itemCount > 0 ) {
				Float slot = columnGap + freeSpaceMain / (Float)( itemCount + 1 );
				mainOffset = slot;
				spacing = slot;
			}
			break;
		case CSSJustifyContent::FlexStart:
		default:
			mainOffset = 0.f;
			break;
	}

	Float pos = mainOffset;
	for ( size_t idx : line.itemIndices ) {
		const auto& item = mItems[idx];
		mItems[idx].mainPos = pos + item.marginMainStart;
		pos += item.targetMainSize + item.marginMainStart + item.marginMainEnd + spacing;
	}
}

void FlexLayouter::resolveCrossSizes( FlexLine& line, const Axis& crossAxis,
									  const Axis& mainAxis ) {
	if ( line.itemIndices.empty() ) {
		line.crossSize = 0.f;
		return;
	}

	// Set each item's main-axis size to its resolved targetMainSize so that
	// getItemCrossSize reflects the correct cross size for the final width.
	for ( size_t idx : line.itemIndices ) {
		auto& item = mItems[idx];
		if ( mainAxis.horizontal )
			item.widget->setInternalPixelsWidth( item.targetMainSize );
		else
			item.widget->setInternalPixelsHeight( item.targetMainSize );
		if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) ) {
			auto* htmlWidget = item.widget->asType<UIHTMLWidget>();
			if ( htmlWidget->getLayouter() && htmlWidget->getLayouter() != this )
				htmlWidget->updateLayout();
		}
	}

	Float maxCross = 0.f;
	for ( size_t idx : line.itemIndices ) {
		auto& item = mItems[idx];
		item.crossSize = getItemCrossSize( item.widget, crossAxis );
		item.outerCrossSize = item.crossSize + item.marginCrossStart + item.marginCrossEnd;
		maxCross = eemax( maxCross, item.outerCrossSize );
	}
	line.crossSize = maxCross;
}

void FlexLayouter::alignCrossAxis( const SmallVector<FlexLine, 8>& lines,
								   const Float containerCrossSize, const Float rowGap,
								   const Axis& crossAxis ) {
	if ( lines.empty() )
		return;

	Float totalLinesCross = 0.f;
	for ( const auto& line : lines )
		totalLinesCross += line.crossSize;

	size_t lineCount = lines.size();
	Float totalRowGap = ( lineCount > 1 ) ? (Float)( lineCount - 1 ) * rowGap : 0.f;
	Float freeSpaceCross = containerCrossSize - totalLinesCross - totalRowGap;

	Float crossOffset = 0.f;
	Float addPerLine = 0.f;
	Float lineGap = rowGap;

	if ( lineCount > 1 ) {
		Float fsc = freeSpaceCross;
		switch ( mAlignContent ) {
			case CSSAlignContent::FlexEnd:
				crossOffset = freeSpaceCross;
				break;
			case CSSAlignContent::Center:
				crossOffset = freeSpaceCross * 0.5f;
				break;
			case CSSAlignContent::SpaceBetween:
				if ( fsc > 0.f && lineCount > 1 )
					lineGap = fsc / (Float)( lineCount - 1 );
				break;
			case CSSAlignContent::SpaceAround:
				if ( fsc > 0.f ) {
					lineGap = fsc / (Float)lineCount;
					crossOffset = lineGap * 0.5f;
				}
				break;
			case CSSAlignContent::SpaceEvenly:
				if ( fsc > 0.f ) {
					lineGap = fsc / (Float)( lineCount + 1 );
					crossOffset = lineGap;
				}
				break;
			case CSSAlignContent::Stretch:
				if ( fsc > 0.f && lineCount > 0 )
					addPerLine = fsc / (Float)lineCount;
				break;
			case CSSAlignContent::FlexStart:
			default:
				break;
		}
	}

	Float pos = crossOffset;
	for ( size_t li = 0; li < lines.size(); li++ ) {
		FlexLine& line = const_cast<FlexLine&>( lines[li] );
		line.crossPos = pos;

		Float lineCrossSize = line.crossSize + addPerLine;
		if ( lineCrossSize < line.crossSize )
			lineCrossSize = line.crossSize;
		if ( lines.size() == 1 && containerCrossSize > 0.f )
			lineCrossSize = containerCrossSize;
		if ( lineCrossSize > containerCrossSize && containerCrossSize > 0.f )
			lineCrossSize = containerCrossSize;

		for ( size_t idx : line.itemIndices ) {
			auto& item = mItems[idx];

			// Cross-axis auto margins (§8.1): absorb free space before align-self.
			if ( item.hasAutoMarginCrossStart || item.hasAutoMarginCrossEnd ) {
				Float free = lineCrossSize - item.outerCrossSize;
				if ( item.hasAutoMarginCrossStart && item.hasAutoMarginCrossEnd ) {
					item.crossPos = pos + free * 0.5f + item.marginCrossStart;
				} else if ( item.hasAutoMarginCrossStart ) {
					item.crossPos = pos + free + item.marginCrossStart;
				} else {
					item.crossPos = pos + item.marginCrossStart;
				}
				continue;
			}

			CSSAlignSelf resolved = resolveAlignSelf( item.alignSelf, mAlignItems );

			Float itemFreeSpace = lineCrossSize - item.outerCrossSize;
			switch ( resolved ) {
				case CSSAlignSelf::FlexStart:
					item.crossPos = pos + item.marginCrossStart;
					break;
				case CSSAlignSelf::FlexEnd:
					item.crossPos = pos + itemFreeSpace + item.marginCrossStart;
					break;
				case CSSAlignSelf::Center:
					item.crossPos = pos + itemFreeSpace * 0.5f + item.marginCrossStart;
					break;
				case CSSAlignSelf::Stretch:
					item.crossPos = pos + item.marginCrossStart;
					if ( !( crossAxis.horizontal
								? ( item.widget->getLayoutWidthPolicy() == SizePolicy::Fixed )
								: ( item.widget->getLayoutHeightPolicy() == SizePolicy::Fixed ) ) )
						item.crossSize =
							lineCrossSize - item.marginCrossStart - item.marginCrossEnd;
					break;
				case CSSAlignSelf::Baseline:
					item.crossPos = pos + item.marginCrossStart;
					break;
				case CSSAlignSelf::Auto:
				default:
					item.crossPos = pos + item.marginCrossStart;
					break;
			}
		}

		pos += lineCrossSize + lineGap;
	}
}

void FlexLayouter::applyLayout( const SmallVector<FlexLine, 8>& lines, const Axis& mainAxis,
								const Axis& crossAxis, const Rectf& containerPadding,
								const Float containerWidth, const Float containerHeight,
								const SizePolicy widthPolicy, const SizePolicy heightPolicy ) {
	Float contentW = 0.f;
	Float contentH = 0.f;

	for ( const auto& line : lines ) {
		for ( size_t idx : line.itemIndices ) {
			auto& item = mItems[idx];
			Float widgetX, widgetY, widgetW, widgetH;

			if ( mainAxis.horizontal ) {
				if ( mainAxis.reversed ) {
					widgetX = containerWidth - containerPadding.Right - item.mainPos -
							  item.targetMainSize;
				} else {
					widgetX = containerPadding.Left + item.mainPos;
				}
				widgetY = containerPadding.Top + item.crossPos;
				widgetW = item.targetMainSize;
				widgetH = item.crossSize;
				contentW = eemax( contentW,
								  widgetX + widgetW + item.marginMainEnd - containerPadding.Left );
				contentH = eemax( contentH,
								  widgetY + widgetH + item.marginCrossEnd - containerPadding.Top );
			} else {
				if ( mainAxis.reversed ) {
					widgetY = containerHeight - containerPadding.Bottom - item.mainPos -
							  item.targetMainSize;
				} else {
					widgetY = containerPadding.Top + item.mainPos;
				}
				widgetX = containerPadding.Left + item.crossPos;
				widgetW = item.crossSize;
				widgetH = item.targetMainSize;
				contentW = eemax( contentW,
								  widgetX + widgetW + item.marginCrossEnd - containerPadding.Left );
				contentH = eemax( contentH,
								  widgetY + widgetH + item.marginMainEnd - containerPadding.Top );
			}

			item.widget->setPixelsPosition( widgetX, widgetY );
			item.widget->setPixelsSize( widgetW, widgetH );

			if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) ) {
				auto* childHtml = item.widget->asType<UIHTMLWidget>();
				if ( childHtml->getLayouter() && childHtml->getLayouter() != this ) {
					auto* childLayouter = childHtml->getLayouter();
					if ( !childLayouter->isPacking() )
						childHtml->updateLayout();
				}
			}
		}
	}

	Float totW = containerWidth;
	Float totH = containerHeight;

	if ( widthPolicy == SizePolicy::WrapContent )
		totW = containerPadding.Left + contentW + containerPadding.Right;
	else if ( widthPolicy != SizePolicy::Fixed )
		totW = mContainer->getParent() ? mContainer->getParent()->getPixelsSize().getWidth() : totW;

	if ( heightPolicy == SizePolicy::WrapContent )
		totH = containerPadding.Top + contentH + containerPadding.Bottom;

	mContainer->setInternalPixelsWidth( totW );
	mContainer->setInternalPixelsHeight( totH );
}

void FlexLayouter::updateLayout() {
	if ( !mContainer->isType( UI_TYPE_HTML_WIDGET ) )
		return;

	auto* widget = mContainer->asType<UIHTMLWidget>();
	if ( widget->isInline() || mPacking )
		return;

	mPacking = true;

	mContainer->beginAttributesTransaction();
	setMatchParentIfNeededVerticalGrowth();

	readContainerStyle( mDirection, mWrap, mJustify, mAlignItems, mAlignContent, mColumnGap,
						mRowGap );

	const Rectf containerPadding = mContainer->getPixelsContentOffset();

	const SizePolicy widthPolicy = mContainer->getLayoutWidthPolicy();
	const SizePolicy heightPolicy = mContainer->getLayoutHeightPolicy();

	Float containerWidth = mContainer->getPixelsSize().getWidth();
	Float containerHeight = mContainer->getPixelsSize().getHeight();

	if ( widthPolicy == SizePolicy::Fixed && mContainer->getUIStyle() ) {
		const auto* wprop = mContainer->getUIStyle()->getProperty( PropertyId::Width );
		if ( wprop ) {
			Float rawWidth = mContainer->lengthFromValue( *wprop );
			containerWidth =
				mContainer->fitMinMaxSizePx( Sizef( rawWidth, containerHeight ) ).getWidth();
		}
	}

	if ( heightPolicy == SizePolicy::Fixed && mContainer->getUIStyle() ) {
		const auto* hprop = mContainer->getUIStyle()->getProperty( PropertyId::Height );
		if ( hprop ) {
			Float rawHeight = mContainer->lengthFromValue( *hprop );
			containerHeight =
				mContainer->fitMinMaxSizePx( Sizef( containerWidth, rawHeight ) ).getHeight();
		}
	}

	Axis mainAxis = getMainAxis( mDirection );
	Axis crossAxis = getCrossAxis( mDirection );

	const Float totalPaddingMain = mainAxis.horizontal
									   ? containerPadding.Left + containerPadding.Right
									   : containerPadding.Top + containerPadding.Bottom;
	const Float totalPaddingCross = crossAxis.horizontal
										? containerPadding.Left + containerPadding.Right
										: containerPadding.Top + containerPadding.Bottom;

	Float containerMainSize = mainAxis.horizontal ? containerWidth : containerHeight;
	Float containerCrossSize = crossAxis.horizontal ? containerWidth : containerHeight;

	if ( widthPolicy != SizePolicy::Fixed && mainAxis.horizontal )
		containerMainSize -= totalPaddingMain;
	if ( heightPolicy != SizePolicy::Fixed && !mainAxis.horizontal )
		containerMainSize -= totalPaddingMain;

	if ( widthPolicy != SizePolicy::Fixed && crossAxis.horizontal )
		containerCrossSize -= totalPaddingCross;
	if ( heightPolicy != SizePolicy::Fixed && !crossAxis.horizontal )
		containerCrossSize -= totalPaddingCross;

	if ( containerMainSize < 0.f )
		containerMainSize = 0.f;
	if ( containerCrossSize < 0.f )
		containerCrossSize = 0.f;

	// Detect indefinite cross size: WrapContent in the cross axis direction
	bool indefiniteCrossSize = ( crossAxis.horizontal && widthPolicy == SizePolicy::WrapContent ) ||
							   ( !crossAxis.horizontal && heightPolicy == SizePolicy::WrapContent );

	// When cross size is indefinite, zero out containerCrossSize so that
	// the flex algorithm sizes lines to content rather than using a stale
	// container size from a previous layout pass.
	if ( indefiniteCrossSize )
		containerCrossSize = 0.f;

	collectFlexItems( mItems );

	if ( mItems.empty() ) {
		// If the container is a UITextSpan with its own text content (e.g. an <a> with
		// display:flex containing only text), process the text via RichText so the
		// container gets a proper content-based size.
		bool hasTextContent = false;
		if ( mContainer->isType( UI_TYPE_TEXTSPAN ) ) {
			auto* textSpan = mContainer->asType<UITextSpan>();
			auto* rt = textSpan->getRichTextPtr();
			if ( rt && !textSpan->getText().empty() &&
				 textSpan->getFontStyleConfig().Font != nullptr ) {
				hasTextContent = true;
				UIRichText::rebuildRichText( textSpan, *rt );
				rt->updateLayout();
				Float textW = rt->getSize().getWidth();
				Float textH = rt->getSize().getHeight();

				if ( widthPolicy == SizePolicy::WrapContent )
					mContainer->setInternalPixelsWidth( textW + containerPadding.Left +
														containerPadding.Right );
				else if ( widthPolicy == SizePolicy::Fixed )
					mContainer->setInternalPixelsWidth( containerWidth );
				else
					mContainer->setInternalPixelsWidth(
						mContainer->getParent()
							? mContainer->getParent()->getPixelsSize().getWidth()
							: 0 );

				if ( heightPolicy == SizePolicy::WrapContent )
					mContainer->setInternalPixelsHeight( textH + containerPadding.Top +
														 containerPadding.Bottom );
				else if ( heightPolicy == SizePolicy::Fixed )
					mContainer->setInternalPixelsHeight( containerHeight );
			}
		}

		if ( !hasTextContent ) {
			Float totW = containerPadding.Left + containerPadding.Right;
			Float totH = containerPadding.Top + containerPadding.Bottom;

			if ( widthPolicy == SizePolicy::WrapContent )
				mContainer->setInternalPixelsWidth( totW );
			else if ( widthPolicy == SizePolicy::Fixed )
				mContainer->setInternalPixelsWidth( containerWidth );
			else
				mContainer->setInternalPixelsWidth(
					mContainer->getParent() ? mContainer->getParent()->getPixelsSize().getWidth()
											: 0 );

			if ( heightPolicy == SizePolicy::WrapContent )
				mContainer->setInternalPixelsHeight( totH );
			else if ( heightPolicy == SizePolicy::Fixed )
				mContainer->setInternalPixelsHeight( containerHeight );
		}

		mContainer->endAttributesTransaction();
		mPacking = false;
		return;
	}

	// Detect indefinite main size: WrapContent in the main axis direction
	bool indefiniteMainSize = ( mainAxis.horizontal && widthPolicy == SizePolicy::WrapContent ) ||
							  ( !mainAxis.horizontal && heightPolicy == SizePolicy::WrapContent );

	measureFlexItems( mainAxis, crossAxis, containerCrossSize, containerWidth, containerHeight,
					  containerPadding, indefiniteMainSize, indefiniteCrossSize );

	SmallVector<FlexLine, 8> lines;

	if ( indefiniteMainSize ) {
		// Compute container main size from item outer sizes (CSS §9.3)
		Float naturalMainSize = 0.f;
		for ( auto& item : mItems )
			naturalMainSize += item.targetMainSize + item.marginMainStart + item.marginMainEnd;
		if ( mItems.size() > 1 )
			naturalMainSize += (Float)( mItems.size() - 1 ) * mColumnGap;
		containerMainSize = naturalMainSize;
	}

	generateFlexLines( lines, containerMainSize, mColumnGap );

	for ( auto& line : lines ) {
		if ( !indefiniteMainSize )
			resolveFlexibleLengths( line, containerMainSize, mColumnGap );
		alignMainAxis( line, containerMainSize, mColumnGap );
		resolveCrossSizes( line, crossAxis, mainAxis );
	}

	alignCrossAxis( lines, containerCrossSize, mRowGap, crossAxis );

	applyLayout( lines, mainAxis, crossAxis, containerPadding, containerWidth, containerHeight,
				 widthPolicy, heightPolicy );

	mContainer->endAttributesTransaction();
	mPacking = false;
}

void FlexLayouter::computeIntrinsicWidths() {
	if ( !mIntrinsicWidthsDirty )
		return;

	if ( !mContainer->isType( UI_TYPE_HTML_WIDGET ) )
		return;

	if ( mContainer->getLayoutWidthPolicy() == SizePolicy::Fixed ) {
		mIntrinsicWidthsDirty = false;
		return;
	}

	readContainerStyle( mDirection, mWrap, mJustify, mAlignItems, mAlignContent, mColumnGap,
						mRowGap );

	Axis mainAxis = getMainAxis( mDirection );

	SmallVector<FlexItem, 16> items;
	collectFlexItems( items );

	if ( items.empty() ) {
		const Rectf containerPadding = mContainer->getPixelsContentOffset();
		mMaxIntrinsicWidth = containerPadding.Left + containerPadding.Right;
		mMinIntrinsicWidth = containerPadding.Left + containerPadding.Right;
		mIntrinsicWidthsDirty = false;
		return;
	}

	for ( auto& item : items ) {
		item.targetMainSize = resolveFlexBasis( item.widget, mDirection, item.flexBasisValue,
												item.flexBasisAuto, mainAxis );

		Rectf margin = item.widget->getLayoutPixelsMargin();
		if ( mainAxis.horizontal ) {
			item.marginMainStart = item.widget->hasLayoutMarginLeftAuto() ? 0.f : margin.Left;
			item.marginMainEnd = item.widget->hasLayoutMarginRightAuto() ? 0.f : margin.Right;
			item.marginCrossStart = margin.Top;
			item.marginCrossEnd = margin.Bottom;
		} else {
			item.marginMainStart = item.widget->hasLayoutMarginTopAuto() ? 0.f : margin.Top;
			item.marginMainEnd = item.widget->hasLayoutMarginBottomAuto() ? 0.f : margin.Bottom;
			item.marginCrossStart = margin.Left;
			item.marginCrossEnd = margin.Right;
		}
	}

	const Rectf containerPadding = mContainer->getPixelsContentOffset();

	if ( mWrap == CSSFlexWrap::NoWrap ) {
		Float maxContentMain = 0.f;
		for ( auto& item : items )
			maxContentMain += item.targetMainSize + item.marginMainStart + item.marginMainEnd;
		if ( items.size() > 1 )
			maxContentMain += (Float)( items.size() - 1 ) * mColumnGap;

		if ( mainAxis.horizontal ) {
			mMaxIntrinsicWidth = maxContentMain + containerPadding.Left + containerPadding.Right;

			// Min-content = largest single item's min-content contribution
			Float maxMinContent = 0.f;
			for ( auto& item : items ) {
				Float minContent = item.targetMainSize;
				if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) &&
					 item.widget->asType<UIHTMLWidget>()->getLayouter() ) {
					minContent = eemax( minContent, item.widget->asType<UIHTMLWidget>()
														->getLayouter()
														->getMinIntrinsicWidth() );
				} else {
					minContent = eemax( minContent, item.widget->getPixelsSize().getWidth() );
				}
				if ( minContent < 0.f )
					minContent = 0.f;
				maxMinContent =
					eemax( maxMinContent, minContent + item.marginMainStart + item.marginMainEnd );
			}
			mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
		} else {
			Float maxItemWidth = 0.f;
			for ( auto& item : items )
				maxItemWidth = eemax( maxItemWidth, item.targetMainSize + item.marginCrossStart +
														item.marginCrossEnd );
			mMaxIntrinsicWidth = maxItemWidth + containerPadding.Left + containerPadding.Right;

			Float maxMinContent = 0.f;
			for ( auto& item : items ) {
				Float minContent = item.targetMainSize;
				if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) &&
					 item.widget->asType<UIHTMLWidget>()->getLayouter() ) {
					minContent = eemax( minContent, item.widget->asType<UIHTMLWidget>()
														->getLayouter()
														->getMinIntrinsicWidth() );
				} else {
					minContent = eemax( minContent, item.widget->getPixelsSize().getWidth() );
				}
				if ( minContent < 0.f )
					minContent = 0.f;
				maxMinContent = eemax( maxMinContent,
									   minContent + item.marginCrossStart + item.marginCrossEnd );
			}
			mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
		}
	} else {
		if ( mainAxis.horizontal ) {
			Float maxLineWidth = 0.f;
			Float lineSum = 0.f;
			for ( auto& item : items ) {
				lineSum += ( lineSum > 0.f ? mColumnGap : 0.f ) + item.targetMainSize +
						   item.marginMainStart + item.marginMainEnd;
			}
			maxLineWidth = lineSum;
			mMaxIntrinsicWidth = maxLineWidth + containerPadding.Left + containerPadding.Right;

			// Min-content width for multi-line containers = largest single item's
			// min-content contribution (each line can be as narrow as its widest item).
			Float maxMinContent = 0.f;
			for ( auto& item : items ) {
				Float minContent = item.targetMainSize;
				if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) &&
					 item.widget->asType<UIHTMLWidget>()->getLayouter() ) {
					minContent = eemax( minContent, item.widget->asType<UIHTMLWidget>()
														->getLayouter()
														->getMinIntrinsicWidth() );
				} else {
					minContent = eemax( minContent, item.widget->getPixelsSize().getWidth() );
				}
				if ( minContent < 0.f )
					minContent = 0.f;
				maxMinContent =
					eemax( maxMinContent, minContent + item.marginMainStart + item.marginMainEnd );
			}
			mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
		} else {
			Float maxItemWidth = 0.f;
			for ( auto& item : items )
				maxItemWidth = eemax( maxItemWidth, item.targetMainSize + item.marginCrossStart +
														item.marginCrossEnd );
			mMaxIntrinsicWidth = maxItemWidth + containerPadding.Left + containerPadding.Right;

			Float maxMinContent = 0.f;
			for ( auto& item : items ) {
				Float minContent = item.targetMainSize;
				if ( item.widget->isType( UI_TYPE_HTML_WIDGET ) &&
					 item.widget->asType<UIHTMLWidget>()->getLayouter() ) {
					minContent = eemax( minContent, item.widget->asType<UIHTMLWidget>()
														->getLayouter()
														->getMinIntrinsicWidth() );
				} else {
					minContent = eemax( minContent, item.widget->getPixelsSize().getWidth() );
				}
				if ( minContent < 0.f )
					minContent = 0.f;
				maxMinContent = eemax( maxMinContent,
									   minContent + item.marginCrossStart + item.marginCrossEnd );
			}
			mMinIntrinsicWidth = maxMinContent + containerPadding.Left + containerPadding.Right;
		}
	}

	mIntrinsicWidthsDirty = false;
}

Float FlexLayouter::getMinIntrinsicWidth() {
	computeIntrinsicWidths();
	return mMinIntrinsicWidth;
}

Float FlexLayouter::getMaxIntrinsicWidth() {
	computeIntrinsicWidths();
	return mMaxIntrinsicWidth;
}

}} // namespace EE::UI
