/***************************************************************************
                         qgscircle.cpp
                         --------------
    begin                : March 2017
    copyright            : (C) 2017 by Loîc Bartoletti
    email                : lbartoletti at tuxfamily dot org
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscircle.h"
#include "qgslinestring.h"
#include "qgsgeometryutils.h"
#include "qgstriangle.h"

#include <memory>

QgsCircle::QgsCircle() :
  QgsEllipse( QgsPoint(), 0.0, 0.0, 0.0 )
{

}

QgsCircle::QgsCircle( const QgsPoint &center, double radius, double azimuth ) :
  QgsEllipse( center, radius, radius, azimuth )
{

}

QgsCircle QgsCircle::from2Points( const QgsPoint &pt1, const QgsPoint &pt2 )
{
  QgsPoint center = QgsGeometryUtils::midpoint( pt1, pt2 );
  double azimuth = QgsGeometryUtils::lineAngle( pt1.x(), pt1.y(), pt2.x(), pt2.y() ) * 180.0 / M_PI;
  double radius = pt1.distance( pt2 );

  return QgsCircle( center, radius, azimuth );
}

static bool isPerpendicular( const QgsPoint &pt1, const QgsPoint &pt2, const QgsPoint &pt3, double epsilon )
{
  // check the given point are perpendicular to x or y axis

  double yDelta_a = pt2.y() - pt1.y();
  double xDelta_a = pt2.x() - pt1.x();
  double yDelta_b = pt3.y() - pt2.y();
  double xDelta_b = pt3.x() - pt2.x();

  if ( ( std::fabs( xDelta_a ) <= epsilon ) && ( std::fabs( yDelta_b ) <= epsilon ) )
  {
    return false;
  }

  if ( std::fabs( yDelta_a ) <= epsilon )
  {
    return true;
  }
  else if ( std::fabs( yDelta_b ) <= epsilon )
  {
    return true;
  }
  else if ( std::fabs( xDelta_a ) <= epsilon )
  {
    return true;
  }
  else if ( std::fabs( xDelta_b ) <= epsilon )
  {
    return true;
  }

  return false;

}

QgsCircle QgsCircle::from3Points( const QgsPoint &pt1, const QgsPoint &pt2, const QgsPoint &pt3, double epsilon )
{
  QgsPoint p1, p2, p3;

  if ( !isPerpendicular( pt1, pt2, pt3, epsilon ) )
  {
    p1 = pt1;
    p2 = pt2;
    p3 = pt3;
  }
  else if ( !isPerpendicular( pt1, pt3, pt2, epsilon ) )
  {
    p1 = pt1;
    p2 = pt3;
    p3 = pt2;
  }
  else if ( !isPerpendicular( pt2, pt1, pt3, epsilon ) )
  {
    p1 = pt2;
    p2 = pt1;
    p3 = pt3;
  }
  else if ( !isPerpendicular( pt2, pt3, pt1, epsilon ) )
  {
    p1 = pt2;
    p2 = pt3;
    p3 = pt1;
  }
  else if ( !isPerpendicular( pt3, pt2, pt1, epsilon ) )
  {
    p1 = pt3;
    p2 = pt2;
    p3 = pt1;
  }
  else if ( !isPerpendicular( pt3, pt1, pt2, epsilon ) )
  {
    p1 = pt3;
    p2 = pt1;
    p3 = pt2;
  }
  else
  {
    return QgsCircle();
  }
  QgsPoint center = QgsPoint();
  double radius = -0.0;
  // Paul Bourke's algorithm
  double yDelta_a = p2.y() - p1.y();
  double xDelta_a = p2.x() - p1.x();
  double yDelta_b = p3.y() - p2.y();
  double xDelta_b = p3.x() - p2.x();

  if ( qgsDoubleNear( xDelta_a, 0.0, epsilon ) || qgsDoubleNear( xDelta_b, 0.0, epsilon ) )
  {
    return QgsCircle();
  }

  double aSlope = yDelta_a / xDelta_a;
  double bSlope = yDelta_b / xDelta_b;

  if ( ( std::fabs( xDelta_a ) <= epsilon ) && ( std::fabs( yDelta_b ) <= epsilon ) )
  {
    center.setX( 0.5 * ( p2.x() + p3.x() ) );
    center.setY( 0.5 * ( p1.y() + p2.y() ) );
    radius = center.distance( pt1 );

    return QgsCircle( center, radius );
  }

  if ( std::fabs( aSlope - bSlope ) <= epsilon )
  {
    return QgsCircle();
  }

  center.setX(
    ( aSlope * bSlope * ( p1.y() - p3.y() ) +
      bSlope * ( p1.x() + p2.x() ) -
      aSlope * ( p2.x() + p3.x() ) ) /
    ( 2.0 * ( bSlope - aSlope ) )
  );
  center.setY(
    -1.0 * ( center.x() - ( p1.x() + p2.x() ) / 2.0 ) /
    aSlope + ( p1.y() + p2.y() ) / 2.0
  );

  radius = center.distance( p1 );

  return QgsCircle( center, radius );
}

QgsCircle QgsCircle::fromCenterDiameter( const QgsPoint &center, double diameter, double azimuth )
{
  return QgsCircle( center, diameter / 2.0, azimuth );
}

QgsCircle QgsCircle::fromCenterPoint( const QgsPoint &center, const QgsPoint &pt1 )
{
  double azimuth = QgsGeometryUtils::lineAngle( center.x(), center.y(), pt1.x(), pt1.y() ) * 180.0 / M_PI;
  return QgsCircle( center, center.distance( pt1 ), azimuth );
}

QgsCircle QgsCircle::from3Tangents( const QgsPoint &pt1_tg1, const QgsPoint &pt2_tg1, const QgsPoint &pt1_tg2, const QgsPoint &pt2_tg2, const QgsPoint &pt1_tg3, const QgsPoint &pt2_tg3, double epsilon )
{
  QgsPoint p1, p2, p3;
  QgsGeometryUtils::segmentIntersection( pt1_tg1, pt2_tg1, pt1_tg2, pt2_tg2, p1, epsilon );
  QgsGeometryUtils::segmentIntersection( pt1_tg1, pt2_tg1, pt1_tg3, pt2_tg3, p2, epsilon );
  QgsGeometryUtils::segmentIntersection( pt1_tg2, pt2_tg2, pt1_tg3, pt2_tg3, p3, epsilon );

  return QgsTriangle( p1, p2, p3 ).inscribedCircle();
}

QgsCircle QgsCircle::fromExtent( const QgsPoint &pt1, const QgsPoint &pt2 )
{
  double delta_x = std::fabs( pt1.x() - pt2.x() );
  double delta_y = std::fabs( pt1.x() - pt2.y() );
  if ( !qgsDoubleNear( delta_x, delta_y ) )
  {
    return QgsCircle();
  }

  return QgsCircle( QgsGeometryUtils::midpoint( pt1, pt2 ), delta_x / 2.0, 0 );
}

double QgsCircle::area() const
{
  return M_PI * mSemiMajorAxis * mSemiMajorAxis;
}

double QgsCircle::perimeter() const
{
  return 2.0 * M_PI * mSemiMajorAxis;
}

QVector<QgsPoint> QgsCircle::northQuadrant() const
{
  QVector<QgsPoint> quad;
  quad.append( QgsPoint( mCenter.x(), mCenter.y() + mSemiMajorAxis ) );
  quad.append( QgsPoint( mCenter.x() + mSemiMajorAxis, mCenter.y() ) );
  quad.append( QgsPoint( mCenter.x(), mCenter.y() - mSemiMajorAxis ) );
  quad.append( QgsPoint( mCenter.x() - mSemiMajorAxis, mCenter.y() ) );

  return quad;
}

QgsCircularString *QgsCircle::toCircularString( bool oriented ) const
{
  std::unique_ptr<QgsCircularString> circString( new QgsCircularString() );
  QgsPointSequence points;
  QVector<QgsPoint> quad;
  if ( oriented )
  {
    quad = quadrant();
  }
  else
  {
    quad = northQuadrant();
  }
  quad.append( quad.at( 0 ) );
  for ( QVector<QgsPoint>::const_iterator it = quad.constBegin(); it != quad.constEnd(); ++it )
  {
    points.append( *it );
  }
  circString->setPoints( points );

  return circString.release();
}

QgsRectangle QgsCircle::boundingBox() const
{
  return QgsRectangle( mCenter.x() - mSemiMajorAxis, mCenter.y() - mSemiMajorAxis, mCenter.x() + mSemiMajorAxis, mCenter.y() + mSemiMajorAxis );
}

QString QgsCircle::toString( int pointPrecision, int radiusPrecision, int azimuthPrecision ) const
{
  QString rep;
  if ( isEmpty() )
    rep = QStringLiteral( "Empty" );
  else
    rep = QStringLiteral( "Circle (Center: %1, Radius: %2, Azimuth: %3)" )
          .arg( mCenter.asWkt( pointPrecision ), 0, 's' )
          .arg( qgsDoubleToString( mSemiMajorAxis, radiusPrecision ), 0, 'f' )
          .arg( qgsDoubleToString( mAzimuth, azimuthPrecision ), 0, 'f' );

  return rep;

}
