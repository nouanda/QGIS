/***************************************************************************
  qgsmapsettings.cpp
  --------------------------------------
  Date                 : December 2013
  Copyright            : (C) 2013 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsmapsettings.h"

#include "qgsscalecalculator.h"
#include "qgsmaprendererjob.h"
#include "qgsmaptopixel.h"
#include "qgslogger.h"

#include "qgsmessagelog.h"
#include "qgsmaplayer.h"
#include "qgsmaplayerlistutils.h"
#include "qgsproject.h"
#include "qgsxmlutils.h"
#include "qgsexception.h"
#include "qgsgeometry.h"

Q_GUI_EXPORT extern int qt_defaultDpiX();


QgsMapSettings::QgsMapSettings()
  : mDpi( qt_defaultDpiX() ) // DPI that will be used by default for QImage instances
  , mSize( QSize( 0, 0 ) )
  , mExtent()
  , mRotation( 0.0 )
  , mMagnificationFactor( 1.0 )
  , mDestCRS()
  , mDatumTransformStore( mDestCRS )
  , mBackgroundColor( Qt::white )
  , mSelectionColor( Qt::yellow )
  , mFlags( Antialiasing | UseAdvancedEffects | DrawLabeling | DrawSelection )
  , mImageFormat( QImage::Format_ARGB32_Premultiplied )
  , mSegmentationTolerance( M_PI_2 / 90 )
  , mSegmentationToleranceType( QgsAbstractGeometry::MaximumAngle )
  , mValid( false )
  , mVisibleExtent()
  , mMapUnitsPerPixel( 1 )
  , mScale( 1 )
{
  mScaleCalculator.setMapUnits( QgsUnitTypes::DistanceUnknownUnit );

  updateDerived();
}

void QgsMapSettings::setMagnificationFactor( double factor )
{
  double ratio = mMagnificationFactor / factor;

  mMagnificationFactor = factor;

  double rot = rotation();
  setRotation( 0.0 );

  QgsRectangle ext = visibleExtent();
  ext.scale( ratio );

  mRotation = rot;
  mExtent = ext;
  mDpi = mDpi / ratio;

  QgsDebugMsg( QString( "Magnification factor: %1  dpi: %2  ratio: %3" ).arg( factor ).arg( mDpi ).arg( ratio ) );

  updateDerived();
}

double QgsMapSettings::magnificationFactor() const
{
  return mMagnificationFactor;
}

QgsRectangle QgsMapSettings::extent() const
{
  return mExtent;
}

void QgsMapSettings::setExtent( const QgsRectangle &extent, bool magnified )
{
  QgsRectangle magnifiedExtent = extent;

  if ( !magnified )
    magnifiedExtent.scale( 1 / mMagnificationFactor );

  mExtent = magnifiedExtent;

  updateDerived();
}

double QgsMapSettings::rotation() const
{
  return mRotation;
}

void QgsMapSettings::setRotation( double degrees )
{
  if ( qgsDoubleNear( mRotation, degrees ) )
    return;

  mRotation = degrees;

  // TODO: update extent while keeping scale ?
  updateDerived();
}


void QgsMapSettings::updateDerived()
{
  QgsRectangle extent = mExtent;

  if ( extent.isEmpty() || !extent.isFinite() )
  {
    mValid = false;
    return;
  }

  // Don't allow zooms where the current extent is so small that it
  // can't be accurately represented using a double (which is what
  // currentExtent uses). Excluding 0 avoids a divide by zero and an
  // infinite loop when rendering to a new canvas. Excluding extents
  // greater than 1 avoids doing unnecessary calculations.

  // The scheme is to compare the width against the mean x coordinate
  // (and height against mean y coordinate) and only allow zooms where
  // the ratio indicates that there is more than about 12 significant
  // figures (there are about 16 significant figures in a double).

  if ( extent.width()  > 0 &&
       extent.height() > 0 &&
       extent.width()  < 1 &&
       extent.height() < 1 )
  {
    // Use abs() on the extent to avoid the case where the extent is
    // symmetrical about 0.
    double xMean = ( std::fabs( extent.xMinimum() ) + std::fabs( extent.xMaximum() ) ) * 0.5;
    double yMean = ( std::fabs( extent.yMinimum() ) + std::fabs( extent.yMaximum() ) ) * 0.5;

    double xRange = extent.width() / xMean;
    double yRange = extent.height() / yMean;

    static const double MIN_PROPORTION = 1e-12;
    if ( xRange < MIN_PROPORTION || yRange < MIN_PROPORTION )
    {
      mValid = false;
      return;
    }
  }

  double myHeight = mSize.height();
  double myWidth = mSize.width();

  if ( !myWidth || !myHeight )
  {
    mValid = false;
    return;
  }

  // calculate the translation and scaling parameters
  double mapUnitsPerPixelY = mExtent.height() / myHeight;
  double mapUnitsPerPixelX = mExtent.width() / myWidth;
  mMapUnitsPerPixel = mapUnitsPerPixelY > mapUnitsPerPixelX ? mapUnitsPerPixelY : mapUnitsPerPixelX;

  // calculate the actual extent of the mapCanvas
  double dxmin = mExtent.xMinimum(), dxmax = mExtent.xMaximum(),
         dymin = mExtent.yMinimum(), dymax = mExtent.yMaximum(), whitespace;

  if ( mapUnitsPerPixelY > mapUnitsPerPixelX )
  {
    whitespace = ( ( myWidth * mMapUnitsPerPixel ) - mExtent.width() ) * 0.5;
    dxmin -= whitespace;
    dxmax += whitespace;
  }
  else
  {
    whitespace = ( ( myHeight * mMapUnitsPerPixel ) - mExtent.height() ) * 0.5;
    dymin -= whitespace;
    dymax += whitespace;
  }

  mVisibleExtent.set( dxmin, dymin, dxmax, dymax );

  // update the scale
  mScaleCalculator.setDpi( mDpi );
  mScale = mScaleCalculator.calculate( mVisibleExtent, mSize.width() );

  mMapToPixel.setParameters( mapUnitsPerPixel(),
                             visibleExtent().center().x(),
                             visibleExtent().center().y(),
                             outputSize().width(),
                             outputSize().height(),
                             mRotation );

#if 1 // set visible extent taking rotation in consideration
  if ( mRotation )
  {
    QgsPointXY p1 = mMapToPixel.toMapCoordinates( QPoint( 0, 0 ) );
    QgsPointXY p2 = mMapToPixel.toMapCoordinates( QPoint( 0, myHeight ) );
    QgsPointXY p3 = mMapToPixel.toMapCoordinates( QPoint( myWidth, 0 ) );
    QgsPointXY p4 = mMapToPixel.toMapCoordinates( QPoint( myWidth, myHeight ) );
    dxmin = std::min( p1.x(), std::min( p2.x(), std::min( p3.x(), p4.x() ) ) );
    dymin = std::min( p1.y(), std::min( p2.y(), std::min( p3.y(), p4.y() ) ) );
    dxmax = std::max( p1.x(), std::max( p2.x(), std::max( p3.x(), p4.x() ) ) );
    dymax = std::max( p1.y(), std::max( p2.y(), std::max( p3.y(), p4.y() ) ) );
    mVisibleExtent.set( dxmin, dymin, dxmax, dymax );
  }
#endif

  QgsDebugMsg( QString( "Map units per pixel (x,y) : %1, %2" ).arg( qgsDoubleToString( mapUnitsPerPixelX ), qgsDoubleToString( mapUnitsPerPixelY ) ) );
  QgsDebugMsg( QString( "Pixmap dimensions (x,y) : %1, %2" ).arg( qgsDoubleToString( mSize.width() ), qgsDoubleToString( mSize.height() ) ) );
  QgsDebugMsg( QString( "Extent dimensions (x,y) : %1, %2" ).arg( qgsDoubleToString( mExtent.width() ), qgsDoubleToString( mExtent.height() ) ) );
  QgsDebugMsg( mExtent.toString() );
  QgsDebugMsg( QString( "Adjusted map units per pixel (x,y) : %1, %2" ).arg( qgsDoubleToString( mVisibleExtent.width() / myWidth ), qgsDoubleToString( mVisibleExtent.height() / myHeight ) ) );
  QgsDebugMsg( QString( "Recalced pixmap dimensions (x,y) : %1, %2" ).arg( qgsDoubleToString( mVisibleExtent.width() / mMapUnitsPerPixel ), qgsDoubleToString( mVisibleExtent.height() / mMapUnitsPerPixel ) ) );
  QgsDebugMsg( QString( "Scale (assuming meters as map units) = 1:%1" ).arg( qgsDoubleToString( mScale ) ) );
  QgsDebugMsg( QString( "Rotation: %1 degrees" ).arg( mRotation ) );

  mValid = true;
}


QSize QgsMapSettings::outputSize() const
{
  return mSize;
}

void QgsMapSettings::setOutputSize( QSize size )
{
  mSize = size;

  updateDerived();
}

double QgsMapSettings::outputDpi() const
{
  return mDpi;
}

void QgsMapSettings::setOutputDpi( double dpi )
{
  mDpi = dpi;

  updateDerived();
}


QStringList QgsMapSettings::layerIds() const
{
  return _qgis_listQPointerToIDs( mLayers );
}


QList<QgsMapLayer *> QgsMapSettings::layers() const
{
  return _qgis_listQPointerToRaw( mLayers );
}

void QgsMapSettings::setLayers( const QList<QgsMapLayer *> &layers )
{
  mLayers = _qgis_listRawToQPointer( layers );
}

QMap<QString, QString> QgsMapSettings::layerStyleOverrides() const
{
  return mLayerStyleOverrides;
}

void QgsMapSettings::setLayerStyleOverrides( const QMap<QString, QString> &overrides )
{
  mLayerStyleOverrides = overrides;
}

void QgsMapSettings::setDestinationCrs( const QgsCoordinateReferenceSystem &crs )
{
  mDestCRS = crs;
  mDatumTransformStore.setDestinationCrs( crs );
  mScaleCalculator.setMapUnits( crs.mapUnits() );
  // Since the map units have changed, force a recalculation of the scale.
  updateDerived();
}

QgsCoordinateReferenceSystem QgsMapSettings::destinationCrs() const
{
  return mDestCRS;
}

bool QgsMapSettings::setEllipsoid( const QString &ellipsoid )
{
  QgsEllipsoidUtils::EllipsoidParameters params = QgsEllipsoidUtils::ellipsoidParameters( ellipsoid );
  if ( !params.valid )
  {
    return false;
  }
  else
  {
    mEllipsoid = ellipsoid;
    return true;
  }
}

void QgsMapSettings::setFlags( QgsMapSettings::Flags flags )
{
  mFlags = flags;
}

void QgsMapSettings::setFlag( QgsMapSettings::Flag flag, bool on )
{
  if ( on )
    mFlags |= flag;
  else
    mFlags &= ~flag;
}

QgsMapSettings::Flags QgsMapSettings::flags() const
{
  return mFlags;
}

bool QgsMapSettings::testFlag( QgsMapSettings::Flag flag ) const
{
  return mFlags.testFlag( flag );
}

QgsUnitTypes::DistanceUnit QgsMapSettings::mapUnits() const
{
  return mScaleCalculator.mapUnits();
}


bool QgsMapSettings::hasValidSettings() const
{
  return mValid;
}

QgsRectangle QgsMapSettings::visibleExtent() const
{
  return mVisibleExtent;
}

QPolygonF QgsMapSettings::visiblePolygon() const
{
  QPolygonF poly;

  const QSize &sz = outputSize();
  const QgsMapToPixel &m2p = mapToPixel();

  poly << m2p.toMapCoordinatesF( 0,          0 ).toQPointF();
  poly << m2p.toMapCoordinatesF( sz.width(), 0 ).toQPointF();
  poly << m2p.toMapCoordinatesF( sz.width(), sz.height() ).toQPointF();
  poly << m2p.toMapCoordinatesF( 0,          sz.height() ).toQPointF();

  return poly;
}

double QgsMapSettings::mapUnitsPerPixel() const
{
  return mMapUnitsPerPixel;
}

double QgsMapSettings::scale() const
{
  return mScale;
}


QgsCoordinateTransform QgsMapSettings::layerTransform( const QgsMapLayer *layer ) const
{
  return mDatumTransformStore.transformation( layer );
}


double QgsMapSettings::layerToMapUnits( const QgsMapLayer *layer, const QgsRectangle &referenceExtent ) const
{
  QgsRectangle extent = referenceExtent.isEmpty() ? layer->extent() : referenceExtent;
  QgsPointXY l1( extent.xMinimum(), extent.yMinimum() );
  QgsPointXY l2( extent.xMaximum(), extent.yMaximum() );
  double distLayerUnits = std::sqrt( l1.sqrDist( l2 ) );
  QgsPointXY m1 = layerToMapCoordinates( layer, l1 );
  QgsPointXY m2 = layerToMapCoordinates( layer, l2 );
  double distMapUnits = std::sqrt( m1.sqrDist( m2 ) );
  return distMapUnits / distLayerUnits;
}


QgsRectangle QgsMapSettings::layerExtentToOutputExtent( const QgsMapLayer *layer, QgsRectangle extent ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
    {
      QgsDebugMsg( QString( "sourceCrs = " + ct.sourceCrs().authid() ) );
      QgsDebugMsg( QString( "destCRS = " + ct.destinationCrs().authid() ) );
      QgsDebugMsg( QString( "extent = " + extent.toString() ) );
      extent = ct.transformBoundingBox( extent );
    }
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  QgsDebugMsg( QString( "proj extent = " + extent.toString() ) );

  return extent;
}


QgsRectangle QgsMapSettings::outputExtentToLayerExtent( const QgsMapLayer *layer, QgsRectangle extent ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
    {
      QgsDebugMsg( QString( "sourceCrs = " + ct.sourceCrs().authid() ) );
      QgsDebugMsg( QString( "destCRS = " + ct.destinationCrs().authid() ) );
      QgsDebugMsg( QString( "extent = " + extent.toString() ) );
      extent = ct.transformBoundingBox( extent, QgsCoordinateTransform::ReverseTransform );
    }
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  QgsDebugMsg( QString( "proj extent = " + extent.toString() ) );

  return extent;
}


QgsPointXY QgsMapSettings::layerToMapCoordinates( const QgsMapLayer *layer, QgsPointXY point ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
      point = ct.transform( point, QgsCoordinateTransform::ForwardTransform );
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  return point;
}


QgsRectangle QgsMapSettings::layerToMapCoordinates( const QgsMapLayer *layer, QgsRectangle rect ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
      rect = ct.transform( rect, QgsCoordinateTransform::ForwardTransform );
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  return rect;
}


QgsPointXY QgsMapSettings::mapToLayerCoordinates( const QgsMapLayer *layer, QgsPointXY point ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
      point = ct.transform( point, QgsCoordinateTransform::ReverseTransform );
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  return point;
}


QgsRectangle QgsMapSettings::mapToLayerCoordinates( const QgsMapLayer *layer, QgsRectangle rect ) const
{
  try
  {
    QgsCoordinateTransform ct = layerTransform( layer );
    if ( ct.isValid() )
      rect = ct.transform( rect, QgsCoordinateTransform::ReverseTransform );
  }
  catch ( QgsCsException &cse )
  {
    QgsMessageLog::logMessage( QStringLiteral( "Transform error caught: %1" ).arg( cse.what() ), QStringLiteral( "CRS" ) );
  }

  return rect;
}



QgsRectangle QgsMapSettings::fullExtent() const
{
  QgsDebugMsg( "called." );

  // reset the map canvas extent since the extent may now be smaller
  // We can't use a constructor since QgsRectangle normalizes the rectangle upon construction
  QgsRectangle fullExtent;
  fullExtent.setMinimal();

  // iterate through the map layers and test each layers extent
  // against the current min and max values
  QgsDebugMsg( QString( "Layer count: %1" ).arg( mLayers.count() ) );
  Q_FOREACH ( const QgsWeakMapLayerPointer &layerPtr, mLayers )
  {
    if ( QgsMapLayer *lyr = layerPtr.data() )
    {
      QgsDebugMsg( "Updating extent using " + lyr->name() );
      QgsDebugMsg( "Input extent: " + lyr->extent().toString() );

      if ( lyr->extent().isNull() )
        continue;

      // Layer extents are stored in the coordinate system (CS) of the
      // layer. The extent must be projected to the canvas CS
      QgsRectangle extent = layerExtentToOutputExtent( lyr, lyr->extent() );

      QgsDebugMsg( "Output extent: " + extent.toString() );
      fullExtent.combineExtentWith( extent );
    }
  }

  if ( fullExtent.width() == 0.0 || fullExtent.height() == 0.0 )
  {
    // If all of the features are at the one point, buffer the
    // rectangle a bit. If they are all at zero, do something a bit
    // more crude.

    if ( fullExtent.xMinimum() == 0.0 && fullExtent.xMaximum() == 0.0 &&
         fullExtent.yMinimum() == 0.0 && fullExtent.yMaximum() == 0.0 )
    {
      fullExtent.set( -1.0, -1.0, 1.0, 1.0 );
    }
    else
    {
      const double padFactor = 1e-8;
      double widthPad = fullExtent.xMinimum() * padFactor;
      double heightPad = fullExtent.yMinimum() * padFactor;
      double xmin = fullExtent.xMinimum() - widthPad;
      double xmax = fullExtent.xMaximum() + widthPad;
      double ymin = fullExtent.yMinimum() - heightPad;
      double ymax = fullExtent.yMaximum() + heightPad;
      fullExtent.set( xmin, ymin, xmax, ymax );
    }
  }

  QgsDebugMsg( "Full extent: " + fullExtent.toString() );
  return fullExtent;
}


void QgsMapSettings::readXml( QDomNode &node )
{
  // set destination CRS
  QgsCoordinateReferenceSystem srs;
  QDomNode srsNode = node.namedItem( QStringLiteral( "destinationsrs" ) );
  if ( !srsNode.isNull() )
  {
    srs.readXml( srsNode );
  }
  setDestinationCrs( srs );

  // set extent
  QDomNode extentNode = node.namedItem( QStringLiteral( "extent" ) );
  QgsRectangle aoi = QgsXmlUtils::readRectangle( extentNode.toElement() );
  setExtent( aoi );

  // set rotation
  QDomNode rotationNode = node.namedItem( QStringLiteral( "rotation" ) );
  QString rotationVal = rotationNode.toElement().text();
  if ( ! rotationVal.isEmpty() )
  {
    double rot = rotationVal.toDouble();
    setRotation( rot );
  }

  //render map tile
  QDomElement renderMapTileElem = node.firstChildElement( QStringLiteral( "rendermaptile" ) );
  if ( !renderMapTileElem.isNull() )
  {
    setFlag( QgsMapSettings::RenderMapTile, renderMapTileElem.text() == QLatin1String( "1" ) );
  }

  mDatumTransformStore.readXml( node );
}



void QgsMapSettings::writeXml( QDomNode &node, QDomDocument &doc )
{
  // units
  node.appendChild( QgsXmlUtils::writeMapUnits( mapUnits(), doc ) );

  // Write current view extents
  node.appendChild( QgsXmlUtils::writeRectangle( extent(), doc ) );

  // Write current view rotation
  QDomElement rotNode = doc.createElement( QStringLiteral( "rotation" ) );
  rotNode.appendChild(
    doc.createTextNode( qgsDoubleToString( rotation() ) )
  );
  node.appendChild( rotNode );

  // destination CRS
  if ( mDestCRS.isValid() )
  {
    QDomElement srsNode = doc.createElement( QStringLiteral( "destinationsrs" ) );
    node.appendChild( srsNode );
    mDestCRS.writeXml( srsNode, doc );
  }

  //render map tile
  QDomElement renderMapTileElem = doc.createElement( QStringLiteral( "rendermaptile" ) );
  QDomText renderMapTileText = doc.createTextNode( testFlag( QgsMapSettings::RenderMapTile ) ? "1" : "0" );
  renderMapTileElem.appendChild( renderMapTileText );
  node.appendChild( renderMapTileElem );

  mDatumTransformStore.writeXml( node, doc );
}
