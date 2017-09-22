/***************************************************************************
                         qgscircularstring.cpp
                         -----------------------
    begin                : September 2014
    copyright            : (C) 2014 by Marco Hugentobler
    email                : marco at sourcepole dot ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgscircularstring.h"
#include "qgsapplication.h"
#include "qgscoordinatetransform.h"
#include "qgsgeometryutils.h"
#include "qgslinestring.h"
#include "qgsmaptopixel.h"
#include "qgspoint.h"
#include "qgswkbptr.h"
#include "qgslogger.h"
#include <QPainter>
#include <QPainterPath>

QgsCircularString::QgsCircularString(): QgsCurve()
{
  mWkbType = QgsWkbTypes::CircularString;
}

bool QgsCircularString::operator==( const QgsCurve &other ) const
{
  const QgsCircularString *otherLine = dynamic_cast< const QgsCircularString * >( &other );
  if ( !otherLine )
    return false;

  return *otherLine == *this;
}

bool QgsCircularString::operator!=( const QgsCurve &other ) const
{
  return !operator==( other );
}

QgsCircularString *QgsCircularString::clone() const
{
  return new QgsCircularString( *this );
}

void QgsCircularString::clear()
{
  mWkbType = QgsWkbTypes::CircularString;
  mX.clear();
  mY.clear();
  mZ.clear();
  mM.clear();
  clearCache();
}

QgsRectangle QgsCircularString::calculateBoundingBox() const
{
  QgsRectangle bbox;
  int nPoints = numPoints();
  for ( int i = 0; i < ( nPoints - 2 ) ; i += 2 )
  {
    if ( i == 0 )
    {
      bbox = segmentBoundingBox( QgsPoint( mX[i], mY[i] ), QgsPoint( mX[i + 1], mY[i + 1] ), QgsPoint( mX[i + 2], mY[i + 2] ) );
    }
    else
    {
      QgsRectangle segmentBox = segmentBoundingBox( QgsPoint( mX[i], mY[i] ), QgsPoint( mX[i + 1], mY[i + 1] ), QgsPoint( mX[i + 2], mY[i + 2] ) );
      bbox.combineExtentWith( segmentBox );
    }
  }

  if ( nPoints > 0 && nPoints % 2 == 0 )
  {
    if ( nPoints == 2 )
    {
      bbox.combineExtentWith( mX[ 0 ], mY[ 0 ] );
    }
    bbox.combineExtentWith( mX[ nPoints - 1 ], mY[ nPoints - 1 ] );
  }
  return bbox;
}

QgsRectangle QgsCircularString::segmentBoundingBox( const QgsPoint &pt1, const QgsPoint &pt2, const QgsPoint &pt3 )
{
  double centerX, centerY, radius;
  QgsGeometryUtils::circleCenterRadius( pt1, pt2, pt3, radius, centerX, centerY );

  double p1Angle = QgsGeometryUtils::ccwAngle( pt1.y() - centerY, pt1.x() - centerX );
  if ( p1Angle > 360 )
  {
    p1Angle -= 360;
  }
  double p2Angle = QgsGeometryUtils::ccwAngle( pt2.y() - centerY, pt2.x() - centerX );
  if ( p2Angle > 360 )
  {
    p2Angle -= 360;
  }
  double p3Angle = QgsGeometryUtils::ccwAngle( pt3.y() - centerY, pt3.x() - centerX );
  if ( p3Angle > 360 )
  {
    p3Angle -= 360;
  }

  //start point, end point and compass points in between can be on bounding box
  QgsRectangle bbox( pt1.x(), pt1.y(), pt1.x(), pt1.y() );
  bbox.combineExtentWith( pt3.x(), pt3.y() );

  QgsPointSequence compassPoints = compassPointsOnSegment( p1Angle, p2Angle, p3Angle, centerX, centerY, radius );
  QgsPointSequence::const_iterator cpIt = compassPoints.constBegin();
  for ( ; cpIt != compassPoints.constEnd(); ++cpIt )
  {
    bbox.combineExtentWith( cpIt->x(), cpIt->y() );
  }
  return bbox;
}

QgsPointSequence QgsCircularString::compassPointsOnSegment( double p1Angle, double p2Angle, double p3Angle, double centerX, double centerY, double radius )
{
  QgsPointSequence pointList;

  QgsPoint nPoint( centerX, centerY + radius );
  QgsPoint ePoint( centerX + radius, centerY );
  QgsPoint sPoint( centerX, centerY - radius );
  QgsPoint wPoint( centerX - radius, centerY );

  if ( p3Angle >= p1Angle )
  {
    if ( p2Angle > p1Angle && p2Angle < p3Angle )
    {
      if ( p1Angle <= 90 && p3Angle >= 90 )
      {
        pointList.append( nPoint );
      }
      if ( p1Angle <= 180 && p3Angle >= 180 )
      {
        pointList.append( wPoint );
      }
      if ( p1Angle <= 270 && p3Angle >= 270 )
      {
        pointList.append( sPoint );
      }
    }
    else
    {
      pointList.append( ePoint );
      if ( p1Angle >= 90 || p3Angle <= 90 )
      {
        pointList.append( nPoint );
      }
      if ( p1Angle >= 180 || p3Angle <= 180 )
      {
        pointList.append( wPoint );
      }
      if ( p1Angle >= 270 || p3Angle <= 270 )
      {
        pointList.append( sPoint );
      }
    }
  }
  else
  {
    if ( p2Angle < p1Angle && p2Angle > p3Angle )
    {
      if ( p1Angle >= 270 && p3Angle <= 270 )
      {
        pointList.append( sPoint );
      }
      if ( p1Angle >= 180 && p3Angle <= 180 )
      {
        pointList.append( wPoint );
      }
      if ( p1Angle >= 90 && p3Angle <= 90 )
      {
        pointList.append( nPoint );
      }
    }
    else
    {
      pointList.append( ePoint );
      if ( p1Angle <= 270 || p3Angle >= 270 )
      {
        pointList.append( sPoint );
      }
      if ( p1Angle <= 180 || p3Angle >= 180 )
      {
        pointList.append( wPoint );
      }
      if ( p1Angle <= 90 || p3Angle >= 90 )
      {
        pointList.append( nPoint );
      }
    }
  }
  return pointList;
}

bool QgsCircularString::fromWkb( QgsConstWkbPtr &wkbPtr )
{
  if ( !wkbPtr )
    return false;

  QgsWkbTypes::Type type = wkbPtr.readHeader();
  if ( QgsWkbTypes::flatType( type ) != QgsWkbTypes::CircularString )
  {
    return false;
  }
  clearCache();
  mWkbType = type;

  //type
  bool hasZ = is3D();
  bool hasM = isMeasure();
  int nVertices = 0;
  wkbPtr >> nVertices;
  mX.resize( nVertices );
  mY.resize( nVertices );
  hasZ ? mZ.resize( nVertices ) : mZ.clear();
  hasM ? mM.resize( nVertices ) : mM.clear();
  for ( int i = 0; i < nVertices; ++i )
  {
    wkbPtr >> mX[i];
    wkbPtr >> mY[i];
    if ( hasZ )
    {
      wkbPtr >> mZ[i];
    }
    if ( hasM )
    {
      wkbPtr >> mM[i];
    }
  }

  return true;
}

bool QgsCircularString::fromWkt( const QString &wkt )
{
  clear();

  QPair<QgsWkbTypes::Type, QString> parts = QgsGeometryUtils::wktReadBlock( wkt );

  if ( QgsWkbTypes::flatType( parts.first ) != QgsWkbTypes::CircularString )
    return false;
  mWkbType = parts.first;

  setPoints( QgsGeometryUtils::pointsFromWKT( parts.second, is3D(), isMeasure() ) );
  return true;
}

QByteArray QgsCircularString::asWkb() const
{
  int binarySize = sizeof( char ) + sizeof( quint32 ) + sizeof( quint32 );
  binarySize += numPoints() * ( 2 + is3D() + isMeasure() ) * sizeof( double );

  QByteArray wkbArray;
  wkbArray.resize( binarySize );
  QgsWkbPtr wkb( wkbArray );
  wkb << static_cast<char>( QgsApplication::endian() );
  wkb << static_cast<quint32>( wkbType() );
  QgsPointSequence pts;
  points( pts );
  QgsGeometryUtils::pointsToWKB( wkb, pts, is3D(), isMeasure() );
  return wkbArray;
}

QString QgsCircularString::asWkt( int precision ) const
{
  QString wkt = wktTypeStr() + ' ';
  QgsPointSequence pts;
  points( pts );
  wkt += QgsGeometryUtils::pointsToWKT( pts, precision, is3D(), isMeasure() );
  return wkt;
}

QDomElement QgsCircularString::asGML2( QDomDocument &doc, int precision, const QString &ns ) const
{
  // GML2 does not support curves
  QgsLineString *line = curveToLine();
  QDomElement gml = line->asGML2( doc, precision, ns );
  delete line;
  return gml;
}

QDomElement QgsCircularString::asGML3( QDomDocument &doc, int precision, const QString &ns ) const
{
  QgsPointSequence pts;
  points( pts );

  QDomElement elemCurve = doc.createElementNS( ns, QStringLiteral( "Curve" ) );
  QDomElement elemSegments = doc.createElementNS( ns, QStringLiteral( "segments" ) );
  QDomElement elemArcString = doc.createElementNS( ns, QStringLiteral( "ArcString" ) );
  elemArcString.appendChild( QgsGeometryUtils::pointsToGML3( pts, doc, precision, ns, is3D() ) );
  elemSegments.appendChild( elemArcString );
  elemCurve.appendChild( elemSegments );
  return elemCurve;
}

QString QgsCircularString::asJSON( int precision ) const
{
  // GeoJSON does not support curves
  QgsLineString *line = curveToLine();
  QString json = line->asJSON( precision );
  delete line;
  return json;
}

bool QgsCircularString::isEmpty() const
{
  return mX.isEmpty();
}

//curve interface
double QgsCircularString::length() const
{
  int nPoints = numPoints();
  double length = 0;
  for ( int i = 0; i < ( nPoints - 2 ) ; i += 2 )
  {
    length += QgsGeometryUtils::circleLength( mX[i], mY[i], mX[i + 1], mY[i + 1], mX[i + 2], mY[i + 2] );
  }
  return length;
}

QgsPoint QgsCircularString::startPoint() const
{
  if ( numPoints() < 1 )
  {
    return QgsPoint();
  }
  return pointN( 0 );
}

QgsPoint QgsCircularString::endPoint() const
{
  if ( numPoints() < 1 )
  {
    return QgsPoint();
  }
  return pointN( numPoints() - 1 );
}

QgsLineString *QgsCircularString::curveToLine( double tolerance, SegmentationToleranceType toleranceType ) const
{
  QgsLineString *line = new QgsLineString();
  QgsPointSequence points;
  int nPoints = numPoints();

  QgsDebugMsg( QString( "curveToLine input: %1" ) .arg( asWkt( 2 ) ) );
  for ( int i = 0; i < ( nPoints - 2 ) ; i += 2 )
  {
    QgsGeometryUtils::segmentizeArc( pointN( i ), pointN( i + 1 ), pointN( i + 2 ), points, tolerance, toleranceType, is3D(), isMeasure() );
  }

  line->setPoints( points );
  return line;
}

int QgsCircularString::numPoints() const
{
  return std::min( mX.size(), mY.size() );
}

QgsPoint QgsCircularString::pointN( int i ) const
{
  if ( std::min( mX.size(), mY.size() ) <= i )
  {
    return QgsPoint();
  }

  double x = mX.at( i );
  double y = mY.at( i );
  double z = 0;
  double m = 0;

  if ( is3D() )
  {
    z = mZ.at( i );
  }
  if ( isMeasure() )
  {
    m = mM.at( i );
  }

  QgsWkbTypes::Type t = QgsWkbTypes::Point;
  if ( is3D() && isMeasure() )
  {
    t = QgsWkbTypes::PointZM;
  }
  else if ( is3D() )
  {
    t = QgsWkbTypes::PointZ;
  }
  else if ( isMeasure() )
  {
    t = QgsWkbTypes::PointM;
  }
  return QgsPoint( t, x, y, z, m );
}

double QgsCircularString::xAt( int index ) const
{
  if ( index >= 0 && index < mX.size() )
    return mX.at( index );
  else
    return 0.0;
}

double QgsCircularString::yAt( int index ) const
{
  if ( index >= 0 && index < mY.size() )
    return mY.at( index );
  else
    return 0.0;
}

void QgsCircularString::points( QgsPointSequence &pts ) const
{
  pts.clear();
  int nPts = numPoints();
  for ( int i = 0; i < nPts; ++i )
  {
    pts.push_back( pointN( i ) );
  }
}

void QgsCircularString::setPoints( const QgsPointSequence &points )
{
  clearCache();

  if ( points.size() < 1 )
  {
    mWkbType = QgsWkbTypes::Unknown;
    mX.clear();
    mY.clear();
    mZ.clear();
    mM.clear();
    return;
  }

  //get wkb type from first point
  const QgsPoint &firstPt = points.at( 0 );
  bool hasZ = firstPt.is3D();
  bool hasM = firstPt.isMeasure();

  setZMTypeFromSubGeometry( &firstPt, QgsWkbTypes::CircularString );

  mX.resize( points.size() );
  mY.resize( points.size() );
  if ( hasZ )
  {
    mZ.resize( points.size() );
  }
  else
  {
    mZ.clear();
  }
  if ( hasM )
  {
    mM.resize( points.size() );
  }
  else
  {
    mM.clear();
  }

  for ( int i = 0; i < points.size(); ++i )
  {
    mX[i] = points[i].x();
    mY[i] = points[i].y();
    if ( hasZ )
    {
      mZ[i] = points[i].z();
    }
    if ( hasM )
    {
      mM[i] = points[i].m();
    }
  }
}

void QgsCircularString::draw( QPainter &p ) const
{
  QPainterPath path;
  addToPainterPath( path );
  p.drawPath( path );
}

void QgsCircularString::transform( const QgsCoordinateTransform &ct, QgsCoordinateTransform::TransformDirection d, bool transformZ )
{
  clearCache();

  double *zArray = mZ.data();

  bool hasZ = is3D();
  int nPoints = numPoints();
  bool useDummyZ = !hasZ || !transformZ;
  if ( useDummyZ )
  {
    zArray = new double[nPoints];
    for ( int i = 0; i < nPoints; ++i )
    {
      zArray[i] = 0;
    }
  }
  ct.transformCoords( nPoints, mX.data(), mY.data(), zArray, d );
  if ( useDummyZ )
  {
    delete[] zArray;
  }
}

void QgsCircularString::transform( const QTransform &t )
{
  clearCache();

  int nPoints = numPoints();
  for ( int i = 0; i < nPoints; ++i )
  {
    qreal x, y;
    t.map( mX.at( i ), mY.at( i ), &x, &y );
    mX[i] = x;
    mY[i] = y;
  }
}

#if 0
void QgsCircularString::clip( const QgsRectangle &rect )
{
  //todo...
}
#endif

void QgsCircularString::addToPainterPath( QPainterPath &path ) const
{
  int nPoints = numPoints();
  if ( nPoints < 1 )
  {
    return;
  }

  if ( path.isEmpty() || path.currentPosition() != QPointF( mX[0], mY[0] ) )
  {
    path.moveTo( QPointF( mX[0], mY[0] ) );
  }

  for ( int i = 0; i < ( nPoints - 2 ) ; i += 2 )
  {
    QgsPointSequence pt;
    QgsGeometryUtils::segmentizeArc( QgsPoint( mX[i], mY[i] ), QgsPoint( mX[i + 1], mY[i + 1] ), QgsPoint( mX[i + 2], mY[i + 2] ), pt );
    for ( int j = 1; j < pt.size(); ++j )
    {
      path.lineTo( pt.at( j ).x(), pt.at( j ).y() );
    }
    //arcTo( path, QPointF( mX[i], mY[i] ), QPointF( mX[i + 1], mY[i + 1] ), QPointF( mX[i + 2], mY[i + 2] ) );
  }

  //if number of points is even, connect to last point with straight line (even though the circular string is not valid)
  if ( nPoints % 2 == 0 )
  {
    path.lineTo( mX[ nPoints - 1 ], mY[ nPoints - 1 ] );
  }
}

void QgsCircularString::arcTo( QPainterPath &path, QPointF pt1, QPointF pt2, QPointF pt3 )
{
  double centerX, centerY, radius;
  QgsGeometryUtils::circleCenterRadius( QgsPoint( pt1.x(), pt1.y() ), QgsPoint( pt2.x(), pt2.y() ), QgsPoint( pt3.x(), pt3.y() ),
                                        radius, centerX, centerY );

  double p1Angle = QgsGeometryUtils::ccwAngle( pt1.y() - centerY, pt1.x() - centerX );
  double sweepAngle = QgsGeometryUtils::sweepAngle( centerX, centerY, pt1.x(), pt1.y(), pt2.x(), pt2.y(), pt3.x(), pt3.y() );

  double diameter = 2 * radius;
  path.arcTo( centerX - radius, centerY - radius, diameter, diameter, p1Angle, sweepAngle );
}

void QgsCircularString::drawAsPolygon( QPainter &p ) const
{
  draw( p );
}

bool QgsCircularString::insertVertex( QgsVertexId position, const QgsPoint &vertex )
{
  if ( position.vertex > mX.size() || position.vertex < 1 )
  {
    return false;
  }

  mX.insert( position.vertex, vertex.x() );
  mY.insert( position.vertex, vertex.y() );
  if ( is3D() )
  {
    mZ.insert( position.vertex, vertex.z() );
  }
  if ( isMeasure() )
  {
    mM.insert( position.vertex, vertex.m() );
  }

  bool vertexNrEven = ( position.vertex % 2 == 0 );
  if ( vertexNrEven )
  {
    insertVertexBetween( position.vertex - 2, position.vertex - 1, position.vertex );
  }
  else
  {
    insertVertexBetween( position.vertex, position.vertex + 1, position.vertex - 1 );
  }
  clearCache(); //set bounding box invalid
  return true;
}

bool QgsCircularString::moveVertex( QgsVertexId position, const QgsPoint &newPos )
{
  if ( position.vertex < 0 || position.vertex >= mX.size() )
  {
    return false;
  }

  mX[position.vertex] = newPos.x();
  mY[position.vertex] = newPos.y();
  if ( is3D() && newPos.is3D() )
  {
    mZ[position.vertex] = newPos.z();
  }
  if ( isMeasure() && newPos.isMeasure() )
  {
    mM[position.vertex] = newPos.m();
  }
  clearCache(); //set bounding box invalid
  return true;
}

bool QgsCircularString::deleteVertex( QgsVertexId position )
{
  int nVertices = this->numPoints();
  if ( nVertices < 4 ) //circular string must have at least 3 vertices
  {
    clear();
    return true;
  }
  if ( position.vertex < 0 || position.vertex > ( nVertices - 1 ) )
  {
    return false;
  }

  if ( position.vertex < ( nVertices - 2 ) )
  {
    //remove this and the following vertex
    deleteVertex( position.vertex + 1 );
    deleteVertex( position.vertex );
  }
  else //remove this and the preceding vertex
  {
    deleteVertex( position.vertex );
    deleteVertex( position.vertex - 1 );
  }

  clearCache(); //set bounding box invalid
  return true;
}

void QgsCircularString::deleteVertex( int i )
{
  mX.remove( i );
  mY.remove( i );
  if ( is3D() )
  {
    mZ.remove( i );
  }
  if ( isMeasure() )
  {
    mM.remove( i );
  }
  clearCache();
}

double QgsCircularString::closestSegment( const QgsPoint &pt, QgsPoint &segmentPt,  QgsVertexId &vertexAfter, bool *leftOf, double epsilon ) const
{
  Q_UNUSED( epsilon );
  double minDist = std::numeric_limits<double>::max();
  QgsPoint minDistSegmentPoint;
  QgsVertexId minDistVertexAfter;
  bool minDistLeftOf = false;

  double currentDist = 0.0;

  int nPoints = numPoints();
  for ( int i = 0; i < ( nPoints - 2 ) ; i += 2 )
  {
    currentDist = closestPointOnArc( mX[i], mY[i], mX[i + 1], mY[i + 1], mX[i + 2], mY[i + 2], pt, segmentPt, vertexAfter, leftOf, epsilon );
    if ( currentDist < minDist )
    {
      minDist = currentDist;
      minDistSegmentPoint = segmentPt;
      minDistVertexAfter.vertex = vertexAfter.vertex + i;
      if ( leftOf )
      {
        minDistLeftOf = *leftOf;
      }
    }
  }

  if ( minDist == std::numeric_limits<double>::max() )
    return -1; // error: no segments

  segmentPt = minDistSegmentPoint;
  vertexAfter = minDistVertexAfter;
  vertexAfter.part = 0;
  vertexAfter.ring = 0;
  if ( leftOf )
  {
    *leftOf = minDistLeftOf;
  }
  return minDist;
}

bool QgsCircularString::pointAt( int node, QgsPoint &point, QgsVertexId::VertexType &type ) const
{
  if ( node >= numPoints() )
  {
    return false;
  }
  point = pointN( node );
  type = ( node % 2 == 0 ) ? QgsVertexId::SegmentVertex : QgsVertexId::CurveVertex;
  return true;
}

void QgsCircularString::sumUpArea( double &sum ) const
{
  int maxIndex = numPoints() - 1;

  for ( int i = 0; i < maxIndex; i += 2 )
  {
    QgsPoint p1( mX[i], mY[i] );
    QgsPoint p2( mX[i + 1], mY[i + 1] );
    QgsPoint p3( mX[i + 2], mY[i + 2] );

    //segment is a full circle, p2 is the center point
    if ( p1 == p3 )
    {
      double r2 = QgsGeometryUtils::sqrDistance2D( p1, p2 ) / 4.0;
      sum += M_PI * r2;
      continue;
    }

    sum += 0.5 * ( mX[i] * mY[i + 2] - mY[i] * mX[i + 2] );

    //calculate area between circle and chord, then sum / subtract from total area
    double midPointX = ( p1.x() + p3.x() ) / 2.0;
    double midPointY = ( p1.y() + p3.y() ) / 2.0;

    double radius, centerX, centerY;
    QgsGeometryUtils::circleCenterRadius( p1, p2, p3, radius, centerX, centerY );

    double d = std::sqrt( QgsGeometryUtils::sqrDistance2D( QgsPoint( centerX, centerY ), QgsPoint( midPointX, midPointY ) ) );
    double r2 = radius * radius;

    if ( d > radius )
    {
      //d cannot be greater than radius, something must be wrong...
      continue;
    }

    bool circlePointLeftOfLine = QgsGeometryUtils::leftOfLine( p2.x(), p2.y(), p1.x(), p1.y(), p3.x(), p3.y() ) < 0;
    bool centerPointLeftOfLine = QgsGeometryUtils::leftOfLine( centerX, centerY, p1.x(), p1.y(), p3.x(), p3.y() ) < 0;

    double cov = 0.5 - d * std::sqrt( r2 - d * d ) / ( M_PI * r2 ) - 1 / M_PI * std::asin( d / radius );
    double circleChordArea = 0;
    if ( circlePointLeftOfLine == centerPointLeftOfLine )
    {
      circleChordArea = M_PI * r2 * ( 1 - cov );
    }
    else
    {
      circleChordArea = M_PI * r2 * cov;
    }

    if ( !circlePointLeftOfLine )
    {
      sum += circleChordArea;
    }
    else
    {
      sum -= circleChordArea;
    }
  }
}

double QgsCircularString::closestPointOnArc( double x1, double y1, double x2, double y2, double x3, double y3,
    const QgsPoint &pt, QgsPoint &segmentPt,  QgsVertexId &vertexAfter, bool *leftOf, double epsilon )
{
  double radius, centerX, centerY;
  QgsPoint pt1( x1, y1 );
  QgsPoint pt2( x2, y2 );
  QgsPoint pt3( x3, y3 );

  QgsGeometryUtils::circleCenterRadius( pt1, pt2, pt3, radius, centerX, centerY );
  double angle = QgsGeometryUtils::ccwAngle( pt.y() - centerY, pt.x() - centerX );
  double angle1 = QgsGeometryUtils::ccwAngle( pt1.y() - centerY, pt1.x() - centerX );
  double angle2 = QgsGeometryUtils::ccwAngle( pt2.y() - centerY, pt2.x() - centerX );
  double angle3 = QgsGeometryUtils::ccwAngle( pt3.y() - centerY, pt3.x() - centerX );

  bool clockwise = QgsGeometryUtils::circleClockwise( angle1, angle2, angle3 );

  if ( QgsGeometryUtils::angleOnCircle( angle, angle1, angle2, angle3 ) )
  {
    //get point on line center -> pt with distance radius
    segmentPt = QgsGeometryUtils::pointOnLineWithDistance( QgsPoint( centerX, centerY ), pt, radius );

    //vertexAfter
    vertexAfter.vertex = QgsGeometryUtils::circleAngleBetween( angle, angle1, angle2, clockwise ) ? 1 : 2;
  }
  else
  {
    double distPtPt1 = QgsGeometryUtils::sqrDistance2D( pt, pt1 );
    double distPtPt3 = QgsGeometryUtils::sqrDistance2D( pt, pt3 );
    segmentPt = ( distPtPt1 <= distPtPt3 ) ? pt1 : pt3;
    vertexAfter.vertex = ( distPtPt1 <= distPtPt3 ) ? 1 : 2;
  }

  double sqrDistance = QgsGeometryUtils::sqrDistance2D( segmentPt, pt );
  //prevent rounding errors if the point is directly on the segment
  if ( qgsDoubleNear( sqrDistance, 0.0, epsilon ) )
  {
    segmentPt.setX( pt.x() );
    segmentPt.setY( pt.y() );
    sqrDistance = 0.0;
  }

  if ( leftOf )
  {
    *leftOf = clockwise ? sqrDistance > radius : sqrDistance < radius;
  }

  return sqrDistance;
}

void QgsCircularString::insertVertexBetween( int after, int before, int pointOnCircle )
{
  double xAfter = mX.at( after );
  double yAfter = mY.at( after );
  double xBefore = mX.at( before );
  double yBefore = mY.at( before );
  double xOnCircle = mX.at( pointOnCircle );
  double yOnCircle = mY.at( pointOnCircle );

  double radius, centerX, centerY;
  QgsGeometryUtils::circleCenterRadius( QgsPoint( xAfter, yAfter ), QgsPoint( xBefore, yBefore ), QgsPoint( xOnCircle, yOnCircle ), radius, centerX, centerY );

  double x = ( xAfter + xBefore ) / 2.0;
  double y = ( yAfter + yBefore ) / 2.0;

  QgsPoint newVertex = QgsGeometryUtils::pointOnLineWithDistance( QgsPoint( centerX, centerY ), QgsPoint( x, y ), radius );
  mX.insert( before, newVertex.x() );
  mY.insert( before, newVertex.y() );

  if ( is3D() )
  {
    mZ.insert( before, ( mZ[after] + mZ[before] ) / 2.0 );
  }
  if ( isMeasure() )
  {
    mM.insert( before, ( mM[after] + mM[before] ) / 2.0 );
  }
  clearCache();
}

double QgsCircularString::vertexAngle( QgsVertexId vId ) const
{
  int before = vId.vertex - 1;
  int vertex = vId.vertex;
  int after = vId.vertex + 1;

  if ( vId.vertex % 2 != 0 ) // a curve vertex
  {
    if ( vId.vertex >= 1 && vId.vertex < numPoints() - 1 )
    {
      return QgsGeometryUtils::circleTangentDirection( QgsPoint( mX[vertex], mY[vertex] ), QgsPoint( mX[before], mY[before] ),
             QgsPoint( mX[vertex], mY[vertex] ), QgsPoint( mX[after], mY[after] ) );
    }
  }
  else //a point vertex
  {
    if ( vId.vertex == 0 )
    {
      return QgsGeometryUtils::circleTangentDirection( QgsPoint( mX[0], mY[0] ), QgsPoint( mX[0], mY[0] ),
             QgsPoint( mX[1], mY[1] ), QgsPoint( mX[2], mY[2] ) );
    }
    if ( vId.vertex >= numPoints() - 1 )
    {
      if ( numPoints() < 3 )
      {
        return 0.0;
      }
      int a = numPoints() - 3;
      int b = numPoints() - 2;
      int c = numPoints() - 1;
      return QgsGeometryUtils::circleTangentDirection( QgsPoint( mX[c], mY[c] ), QgsPoint( mX[a], mY[a] ),
             QgsPoint( mX[b], mY[b] ), QgsPoint( mX[c], mY[c] ) );
    }
    else
    {
      if ( vId.vertex + 2 > numPoints() - 1 )
      {
        return 0.0;
      }

      int vertex1 = vId.vertex - 2;
      int vertex2 = vId.vertex - 1;
      int vertex3 = vId.vertex;
      double angle1 = QgsGeometryUtils::circleTangentDirection( QgsPoint( mX[vertex3], mY[vertex3] ),
                      QgsPoint( mX[vertex1], mY[vertex1] ), QgsPoint( mX[vertex2], mY[vertex2] ), QgsPoint( mX[vertex3], mY[vertex3] ) );
      int vertex4 = vId.vertex + 1;
      int vertex5 = vId.vertex + 2;
      double angle2 = QgsGeometryUtils::circleTangentDirection( QgsPoint( mX[vertex3], mY[vertex3] ),
                      QgsPoint( mX[vertex3], mY[vertex3] ), QgsPoint( mX[vertex4], mY[vertex4] ), QgsPoint( mX[vertex5], mY[vertex5] ) );
      return QgsGeometryUtils::averageAngle( angle1, angle2 );
    }
  }
  return 0.0;
}

QgsCircularString *QgsCircularString::reversed() const
{
  QgsCircularString *copy = clone();
  std::reverse( copy->mX.begin(), copy->mX.end() );
  std::reverse( copy->mY.begin(), copy->mY.end() );
  if ( is3D() )
  {
    std::reverse( copy->mZ.begin(), copy->mZ.end() );
  }
  if ( isMeasure() )
  {
    std::reverse( copy->mM.begin(), copy->mM.end() );
  }
  return copy;
}

bool QgsCircularString::addZValue( double zValue )
{
  if ( QgsWkbTypes::hasZ( mWkbType ) )
    return false;

  clearCache();
  mWkbType = QgsWkbTypes::addZ( mWkbType );

  int nPoints = numPoints();
  mZ.clear();
  mZ.reserve( nPoints );
  for ( int i = 0; i < nPoints; ++i )
  {
    mZ << zValue;
  }
  return true;
}

bool QgsCircularString::addMValue( double mValue )
{
  if ( QgsWkbTypes::hasM( mWkbType ) )
    return false;

  clearCache();
  mWkbType = QgsWkbTypes::addM( mWkbType );

  int nPoints = numPoints();
  mM.clear();
  mM.reserve( nPoints );
  for ( int i = 0; i < nPoints; ++i )
  {
    mM << mValue;
  }
  return true;
}

bool QgsCircularString::dropZValue()
{
  if ( !QgsWkbTypes::hasZ( mWkbType ) )
    return false;

  clearCache();

  mWkbType = QgsWkbTypes::dropZ( mWkbType );
  mZ.clear();
  return true;
}

bool QgsCircularString::dropMValue()
{
  if ( !QgsWkbTypes::hasM( mWkbType ) )
    return false;

  clearCache();

  mWkbType = QgsWkbTypes::dropM( mWkbType );
  mM.clear();
  return true;
}
