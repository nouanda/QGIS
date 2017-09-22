/***************************************************************************
    qgsmapmouseevent.cpp  -  mouse event in map coordinates and ability to snap
    ----------------------
    begin                : October 2014
    copyright            : (C) Denis Rouzaud
    email                : denis.rouzaud@gmail.com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/


#include "qgsmapmouseevent.h"
#include "qgsmapcanvas.h"

#include "qgssnappingutils.h"
#include "qgssnappingconfig.h"

/// @cond PRIVATE
struct EdgesOnlyFilter : public QgsPointLocator::MatchFilter
{
  bool acceptMatch( const QgsPointLocator::Match &m ) override { return m.hasEdge(); }
};
/// @endcond

QgsMapMouseEvent::QgsMapMouseEvent( QgsMapCanvas *mapCanvas, QMouseEvent *event )
  : QMouseEvent( event->type(), event->pos(), event->button(), event->buttons(), event->modifiers() )
  , mHasCachedSnapResult( false )
  , mOriginalMapPoint( mapCanvas ? mapCanvas->mapSettings().mapToPixel().toMapCoordinates( event->pos() ) : QgsPointXY() )
  , mMapPoint( mOriginalMapPoint )
  , mPixelPoint( event->pos() )
  , mMapCanvas( mapCanvas )
{
}

QgsMapMouseEvent::QgsMapMouseEvent( QgsMapCanvas *mapCanvas, QEvent::Type type, QPoint pos, Qt::MouseButton button, Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers )
  : QMouseEvent( type, pos, button, buttons, modifiers )
  , mHasCachedSnapResult( false )
  , mOriginalMapPoint( mapCanvas ? mapCanvas->mapSettings().mapToPixel().toMapCoordinates( pos ) : QgsPointXY() )
  , mMapPoint( mOriginalMapPoint )
  , mPixelPoint( pos )
  , mMapCanvas( mapCanvas )
{
}

QgsPointXY QgsMapMouseEvent::snapPoint()
{
  // Use cached result
  if ( mHasCachedSnapResult )
    return mMapPoint;

  mHasCachedSnapResult = true;

  QgsSnappingUtils *snappingUtils = mMapCanvas->snappingUtils();
  mSnapMatch = snappingUtils->snapToMap( mMapPoint );

  if ( mSnapMatch.isValid() )
  {
    mMapPoint = mSnapMatch.point();
    mPixelPoint = mapToPixelCoordinates( mMapPoint );
  }
  else
  {
    mMapPoint = mOriginalMapPoint;
    mPixelPoint = pos();
  }

  return mMapPoint;
}

QList<QgsPointXY> QgsMapMouseEvent::snapSegment( bool *snapped, bool allLayers ) const
{
  QList<QgsPointXY> segment;
  QgsPointXY pt1, pt2;

  // If there's a cached snapping result we use it
  if ( mHasCachedSnapResult && mSnapMatch.hasEdge() )
  {
    mSnapMatch.edgePoints( pt1, pt2 );
    segment << pt1 << pt2;
  }
  else
  {
    QgsPointLocator::Match match;
    if ( !allLayers )
    {
      // run snapToMap with only segments
      EdgesOnlyFilter filter;
      match = mMapCanvas->snappingUtils()->snapToMap( mOriginalMapPoint, &filter );
    }
    else
    {
      // run snapToMap with only edges on all layers
      QgsSnappingUtils *snappingUtils = mMapCanvas->snappingUtils();

      QgsSnappingConfig canvasConfig = snappingUtils->config();
      QgsSnappingConfig localConfig = snappingUtils->config();

      localConfig.setMode( QgsSnappingConfig::AllLayers );
      localConfig.setType( QgsSnappingConfig::Segment );
      snappingUtils->setConfig( localConfig );

      match = snappingUtils->snapToMap( mOriginalMapPoint );

      snappingUtils->setConfig( canvasConfig );
    }
    if ( match.isValid() && match.hasEdge() )
    {
      match.edgePoints( pt1, pt2 );
      segment << pt1 << pt2;
    }
  }

  if ( snapped )
  {
    *snapped = segment.count() == 2;
  }

  return segment;
}

void QgsMapMouseEvent::setMapPoint( const QgsPointXY &point )
{
  mMapPoint = point;
  mPixelPoint = mapToPixelCoordinates( point );
}

QPoint QgsMapMouseEvent::mapToPixelCoordinates( const QgsPointXY &point )
{
  double x = point.x(), y = point.y();

  mMapCanvas->mapSettings().mapToPixel().transformInPlace( x, y );

  return QPoint( std::round( x ), std::round( y ) );
}
