/***************************************************************************
                              qgsshadoweffect.cpp
                              -------------------
    begin                : December 2014
    copyright            : (C) 2014 Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsshadoweffect.h"
#include "qgsimageoperation.h"
#include "qgssymbollayerutils.h"
#include "qgsunittypes.h"

QgsShadowEffect::QgsShadowEffect()
  : QgsPaintEffect()
  , mBlurLevel( 10 )
  , mOffsetAngle( 135 )
  , mOffsetDist( 2.0 )
  , mOffsetUnit( QgsUnitTypes::RenderMillimeters )
  , mColor( Qt::black )
  , mBlendMode( QPainter::CompositionMode_Multiply )
{

}

void QgsShadowEffect::draw( QgsRenderContext &context )
{
  if ( !source() || !enabled() || !context.painter() )
    return;

  QImage colorisedIm = sourceAsImage( context )->copy();

  QPainter *painter = context.painter();
  painter->save();
  painter->setCompositionMode( mBlendMode );

  if ( !exteriorShadow() )
  {
    //inner shadow, first invert the opacity. The color does not matter since we will
    //be replacing it anyway
    colorisedIm.invertPixels( QImage::InvertRgba );
  }

  QgsImageOperation::overlayColor( colorisedIm, mColor );
  QgsImageOperation::stackBlur( colorisedIm, mBlurLevel );

  double offsetDist = context.convertToPainterUnits( mOffsetDist, mOffsetUnit, mOffsetMapUnitScale );

  double   angleRad = mOffsetAngle * M_PI / 180; // to radians
  QPointF transPt( -offsetDist * std::cos( angleRad + M_PI / 2 ),
                   -offsetDist * std::sin( angleRad + M_PI / 2 ) );

  //transparency, scale
  QgsImageOperation::multiplyOpacity( colorisedIm, mOpacity );

  if ( !exteriorShadow() )
  {
    //inner shadow, do a bit of painter juggling
    QImage innerShadowIm( colorisedIm.width(), colorisedIm.height(), QImage::Format_ARGB32 );
    innerShadowIm.fill( Qt::transparent );
    QPainter imPainter( &innerShadowIm );

    //draw shadow at offset
    imPainter.drawImage( transPt.x(), transPt.y(), colorisedIm );

    //restrict shadow so it's only drawn on top of original image
    imPainter.setCompositionMode( QPainter::CompositionMode_DestinationIn );
    imPainter.drawImage( 0, 0, *sourceAsImage( context ) );
    imPainter.end();

    painter->drawImage( imageOffset( context ), innerShadowIm );
  }
  else
  {
    painter->drawImage( imageOffset( context ) + transPt, colorisedIm );
  }
  painter->restore();
}

QgsStringMap QgsShadowEffect::properties() const
{
  QgsStringMap props;
  props.insert( QStringLiteral( "enabled" ), mEnabled ? "1" : "0" );
  props.insert( QStringLiteral( "draw_mode" ), QString::number( int( mDrawMode ) ) );
  props.insert( QStringLiteral( "blend_mode" ), QString::number( int( mBlendMode ) ) );
  props.insert( QStringLiteral( "opacity" ), QString::number( mOpacity ) );
  props.insert( QStringLiteral( "blur_level" ), QString::number( mBlurLevel ) );
  props.insert( QStringLiteral( "offset_angle" ), QString::number( mOffsetAngle ) );
  props.insert( QStringLiteral( "offset_distance" ), QString::number( mOffsetDist ) );
  props.insert( QStringLiteral( "offset_unit" ), QgsUnitTypes::encodeUnit( mOffsetUnit ) );
  props.insert( QStringLiteral( "offset_unit_scale" ), QgsSymbolLayerUtils::encodeMapUnitScale( mOffsetMapUnitScale ) );
  props.insert( QStringLiteral( "color" ), QgsSymbolLayerUtils::encodeColor( mColor ) );
  return props;
}

void QgsShadowEffect::readProperties( const QgsStringMap &props )
{
  bool ok;
  QPainter::CompositionMode mode = static_cast< QPainter::CompositionMode >( props.value( QStringLiteral( "blend_mode" ) ).toInt( &ok ) );
  if ( ok )
  {
    mBlendMode = mode;
  }
  if ( props.contains( QStringLiteral( "transparency" ) ) )
  {
    double transparency = props.value( QStringLiteral( "transparency" ) ).toDouble( &ok );
    if ( ok )
    {
      mOpacity = 1.0 - transparency;
    }
  }
  else
  {
    double opacity = props.value( QStringLiteral( "opacity" ) ).toDouble( &ok );
    if ( ok )
    {
      mOpacity = opacity;
    }
  }
  mEnabled = props.value( QStringLiteral( "enabled" ), QStringLiteral( "1" ) ).toInt();
  mDrawMode = static_cast< QgsPaintEffect::DrawMode >( props.value( QStringLiteral( "draw_mode" ), QStringLiteral( "2" ) ).toInt() );
  int level = props.value( QStringLiteral( "blur_level" ) ).toInt( &ok );
  if ( ok )
  {
    mBlurLevel = level;
  }
  int angle = props.value( QStringLiteral( "offset_angle" ) ).toInt( &ok );
  if ( ok )
  {
    mOffsetAngle = angle;
  }
  double distance = props.value( QStringLiteral( "offset_distance" ) ).toDouble( &ok );
  if ( ok )
  {
    mOffsetDist = distance;
  }
  mOffsetUnit = QgsUnitTypes::decodeRenderUnit( props.value( QStringLiteral( "offset_unit" ) ) );
  mOffsetMapUnitScale = QgsSymbolLayerUtils::decodeMapUnitScale( props.value( QStringLiteral( "offset_unit_scale" ) ) );
  if ( props.contains( QStringLiteral( "color" ) ) )
  {
    mColor = QgsSymbolLayerUtils::decodeColor( props.value( QStringLiteral( "color" ) ) );
  }
}

QRectF QgsShadowEffect::boundingRect( const QRectF &rect, const QgsRenderContext &context ) const
{
  //offset distance
  double spread = context.convertToPainterUnits( mOffsetDist, mOffsetUnit, mOffsetMapUnitScale );
  //plus possible extension due to blur, with a couple of extra pixels thrown in for safety
  spread += mBlurLevel * 2 + 10;
  return rect.adjusted( -spread, -spread, spread, spread );
}


//
// QgsDropShadowEffect
//

QgsPaintEffect *QgsDropShadowEffect::create( const QgsStringMap &map )
{
  QgsDropShadowEffect *effect = new QgsDropShadowEffect();
  effect->readProperties( map );
  return effect;
}

QgsDropShadowEffect::QgsDropShadowEffect()
  : QgsShadowEffect()
{

}

QString QgsDropShadowEffect::type() const
{
  return QStringLiteral( "dropShadow" );
}

QgsDropShadowEffect *QgsDropShadowEffect::clone() const
{
  return new QgsDropShadowEffect( *this );
}

bool QgsDropShadowEffect::exteriorShadow() const
{
  return true;
}


//
// QgsInnerShadowEffect
//

QgsPaintEffect *QgsInnerShadowEffect::create( const QgsStringMap &map )
{
  QgsInnerShadowEffect *effect = new QgsInnerShadowEffect();
  effect->readProperties( map );
  return effect;
}

QgsInnerShadowEffect::QgsInnerShadowEffect()
  : QgsShadowEffect()
{

}

QString QgsInnerShadowEffect::type() const
{
  return QStringLiteral( "innerShadow" );
}

QgsInnerShadowEffect *QgsInnerShadowEffect::clone() const
{
  return new QgsInnerShadowEffect( *this );
}

bool QgsInnerShadowEffect::exteriorShadow() const
{
  return false;
}
