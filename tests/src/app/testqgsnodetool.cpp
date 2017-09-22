/***************************************************************************
     testqgsnodetool.cpp
     ----------------------
    Date                 : 2017-03-01
    Copyright            : (C) 2017 by Martin Dobias
    Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgstest.h"

#include "qgsadvanceddigitizingdockwidget.h"
#include "qgsgeometry.h"
#include "qgsmapcanvas.h"
#include "qgsmapcanvassnappingutils.h"
#include "qgsproject.h"
#include "qgsvectorlayer.h"

#include "nodetool/qgsnodetool.h"

bool operator==( const QgsGeometry &g1, const QgsGeometry &g2 )
{
  if ( g1.isNull() && g2.isNull() )
    return true;
  else
    return g1.isGeosEqual( g2 );
}

namespace QTest
{
  // pretty printing of geometries in comparison tests
  template<> char *toString( const QgsGeometry &geom )
  {
    QByteArray ba = geom.exportToWkt().toAscii();
    return qstrdup( ba.data() );
  }
}

/** \ingroup UnitTests
 * This is a unit test for the node tool
 */
class TestQgsNodeTool : public QObject
{
    Q_OBJECT
  public:
    TestQgsNodeTool();

  private slots:
    void initTestCase();// will be called before the first testfunction is executed.
    void cleanupTestCase();// will be called after the last testfunction was executed.

    void testMoveVertex();
    void testMoveEdge();
    void testAddVertex();
    void testAddVertexAtEndpoint();
    void testDeleteVertex();
    void testMoveMultipleVertices();
    void testMoveVertexTopo();
    void testDeleteVertexTopo();

  private:
    QPoint mapToScreen( double mapX, double mapY )
    {
      QgsPointXY pt = mCanvas->mapSettings().mapToPixel().transform( mapX, mapY );
      return QPoint( std::round( pt.x() ), std::round( pt.y() ) );
    }

    void mouseMove( double mapX, double mapY )
    {
      QgsMapMouseEvent e( mCanvas, QEvent::MouseMove, mapToScreen( mapX, mapY ) );
      mNodeTool->cadCanvasMoveEvent( &e );
    }

    void mousePress( double mapX, double mapY, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::KeyboardModifiers() )
    {
      QgsMapMouseEvent e1( mCanvas, QEvent::MouseButtonPress, mapToScreen( mapX, mapY ), button, button, stateKey );
      mNodeTool->cadCanvasPressEvent( &e1 );
    }

    void mouseRelease( double mapX, double mapY, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::KeyboardModifiers() )
    {
      QgsMapMouseEvent e2( mCanvas, QEvent::MouseButtonRelease, mapToScreen( mapX, mapY ), button, Qt::MouseButton(), stateKey );
      mNodeTool->cadCanvasReleaseEvent( &e2 );
    }

    void mouseClick( double mapX, double mapY, Qt::MouseButton button, Qt::KeyboardModifiers stateKey = Qt::KeyboardModifiers() )
    {
      mousePress( mapX, mapY, button, stateKey );
      mouseRelease( mapX, mapY, button, stateKey );
    }

    void keyClick( int key )
    {
      QKeyEvent e1( QEvent::KeyPress, key, Qt::KeyboardModifiers() );
      mNodeTool->keyPressEvent( &e1 );

      QKeyEvent e2( QEvent::KeyRelease, key, Qt::KeyboardModifiers() );
      mNodeTool->keyReleaseEvent( &e2 );
    }

  private:
    QgsMapCanvas *mCanvas = nullptr;
    QgsAdvancedDigitizingDockWidget *mAdvancedDigitizingDockWidget = nullptr;
    QgsNodeTool *mNodeTool = nullptr;
    QgsVectorLayer *mLayerLine = nullptr;
    QgsVectorLayer *mLayerPolygon = nullptr;
    QgsVectorLayer *mLayerPoint = nullptr;
    QgsFeatureId mFidLineF1 = 0;
    QgsFeatureId mFidPolygonF1 = 0;
    QgsFeatureId mFidPointF1 = 0;
};

TestQgsNodeTool::TestQgsNodeTool()
  : mCanvas( nullptr )
{
}


//runs before all tests
void TestQgsNodeTool::initTestCase()
{
  qDebug() << "TestQgisAppClipboard::initTestCase()";
  // init QGIS's paths - true means that all path will be inited from prefix
  QgsApplication::init();
  QgsApplication::initQgis();

  // Set up the QSettings environment
  QCoreApplication::setOrganizationName( QStringLiteral( "QGIS" ) );
  QCoreApplication::setOrganizationDomain( QStringLiteral( "qgis.org" ) );
  QCoreApplication::setApplicationName( QStringLiteral( "QGIS-TEST" ) );

  mCanvas = new QgsMapCanvas();

  mCanvas->setDestinationCrs( QgsCoordinateReferenceSystem( "EPSG:27700" ) );

  mAdvancedDigitizingDockWidget = new QgsAdvancedDigitizingDockWidget( mCanvas );

  // make testing layers
  mLayerLine = new QgsVectorLayer( "LineString?crs=EPSG:27700", "layer line", "memory" );
  QVERIFY( mLayerLine->isValid() );
  mLayerPolygon = new QgsVectorLayer( "Polygon?crs=EPSG:27700", "layer polygon", "memory" );
  QVERIFY( mLayerPolygon->isValid() );
  mLayerPoint = new QgsVectorLayer( "Point?crs=EPSG:27700", "layer point", "memory" );
  QVERIFY( mLayerPoint->isValid() );
  QgsProject::instance()->addMapLayers( QList<QgsMapLayer *>() << mLayerLine << mLayerPolygon << mLayerPoint );

  QgsPolyline line1;
  line1 << QgsPointXY( 2, 1 ) << QgsPointXY( 1, 1 ) << QgsPointXY( 1, 3 );
  QgsFeature lineF1;
  lineF1.setGeometry( QgsGeometry::fromPolyline( line1 ) );

  QgsPolygon polygon1;
  QgsPolyline polygon1exterior;
  polygon1exterior << QgsPointXY( 4, 1 ) << QgsPointXY( 7, 1 ) << QgsPointXY( 7, 4 ) << QgsPointXY( 4, 4 ) << QgsPointXY( 4, 1 );
  polygon1 << polygon1exterior;
  QgsFeature polygonF1;
  polygonF1.setGeometry( QgsGeometry::fromPolygon( polygon1 ) );

  QgsFeature pointF1;
  pointF1.setGeometry( QgsGeometry::fromPoint( QgsPointXY( 2, 3 ) ) );

  mLayerLine->startEditing();
  mLayerLine->addFeature( lineF1 );
  mFidLineF1 = lineF1.id();
  QCOMPARE( mLayerLine->featureCount(), ( long )1 );

  mLayerPolygon->startEditing();
  mLayerPolygon->addFeature( polygonF1 );
  mFidPolygonF1 = polygonF1.id();
  QCOMPARE( mLayerPolygon->featureCount(), ( long )1 );

  mLayerPoint->startEditing();
  mLayerPoint->addFeature( pointF1 );
  mFidPointF1 = pointF1.id();
  QCOMPARE( mLayerPoint->featureCount(), ( long )1 );

  // just one added feature in each undo stack
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 1 );
  QCOMPARE( mLayerPoint->undoStack()->index(), 1 );

  mCanvas->setFrameStyle( QFrame::NoFrame );
  mCanvas->resize( 512, 512 );
  mCanvas->setExtent( QgsRectangle( 0, 0, 8, 8 ) );
  mCanvas->show(); // to make the canvas resize
  mCanvas->hide();
  QCOMPARE( mCanvas->mapSettings().outputSize(), QSize( 512, 512 ) );
  QCOMPARE( mCanvas->mapSettings().visibleExtent(), QgsRectangle( 0, 0, 8, 8 ) );

  mCanvas->setLayers( QList<QgsMapLayer *>() << mLayerLine << mLayerPolygon << mLayerPoint );

  // TODO: set up snapping

  mCanvas->setSnappingUtils( new QgsMapCanvasSnappingUtils( mCanvas, this ) );

  // create node tool
  mNodeTool = new QgsNodeTool( mCanvas, mAdvancedDigitizingDockWidget );

  mCanvas->setMapTool( mNodeTool );
}

//runs after all tests
void TestQgsNodeTool::cleanupTestCase()
{
  delete mNodeTool;
  delete mAdvancedDigitizingDockWidget;
  delete mCanvas;
  QgsApplication::exitQgis();
}

void TestQgsNodeTool::testMoveVertex()
{
  QCOMPARE( mCanvas->mapSettings().outputSize(), QSize( 512, 512 ) );
  QCOMPARE( mCanvas->mapSettings().visibleExtent(), QgsRectangle( 0, 0, 8, 8 ) );

  // move vertex of linestring

  mouseClick( 2, 1, Qt::LeftButton );
  mouseClick( 2, 2, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 2, 1 1, 1 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  mouseClick( 1, 1, Qt::LeftButton );
  mouseClick( 0.5, 0.5, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 0.5 0.5, 1 3)" ) );

  mLayerLine->undoStack()->undo();

  // move point

  mouseClick( 2, 3, Qt::LeftButton );
  mouseClick( 1, 4, Qt::LeftButton );

  QCOMPARE( mLayerPoint->undoStack()->index(), 2 );
  QCOMPARE( mLayerPoint->getFeature( mFidPointF1 ).geometry(), QgsGeometry::fromWkt( "POINT(1 4)" ) );

  mLayerPoint->undoStack()->undo();

  QCOMPARE( mLayerPoint->getFeature( mFidPointF1 ).geometry(), QgsGeometry::fromWkt( "POINT(2 3)" ) );

  // move vertex of polygon

  mouseClick( 4, 1, Qt::LeftButton );
  mouseClick( 5, 2, Qt::LeftButton );

  QCOMPARE( mLayerPolygon->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((5 2, 7 1, 7 4, 4 4, 5 2))" ) );

  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  mouseClick( 4, 4, Qt::LeftButton );
  mouseClick( 5, 5, Qt::LeftButton );

  QCOMPARE( mLayerPolygon->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 5 5, 4 1))" ) );

  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  // cancel moving of a linestring with right mouse button
  mouseClick( 2, 1, Qt::LeftButton );
  mouseClick( 2, 2, Qt::RightButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // clicks somewhere away from features - should do nothing
  mouseClick( 2, 2, Qt::LeftButton );
  mouseClick( 2, 4, Qt::LeftButton );

  // no other unexpected changes happened
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 1 );
  QCOMPARE( mLayerPoint->undoStack()->index(), 1 );
}

void TestQgsNodeTool::testMoveEdge()
{
  // move edge of linestring

  mouseClick( 1.2, 1, Qt::LeftButton );
  mouseClick( 1.2, 2, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 2, 1 2, 1 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // move edge of polygon

  mouseClick( 5, 1, Qt::LeftButton );
  mouseClick( 6, 1, Qt::LeftButton );

  QCOMPARE( mLayerPolygon->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((5 1, 8 1, 7 4, 4 4, 5 1))" ) );

  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  // no other unexpected changes happened
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 1 );
  QCOMPARE( mLayerPoint->undoStack()->index(), 1 );
}


void TestQgsNodeTool::testAddVertex()
{
  // add vertex in linestring

  mouseClick( 1.5, 1, Qt::LeftButton );
  mouseClick( 1.5, 2, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1.5 2, 1 1, 1 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // add vertex in polygon

  mouseClick( 4, 2.5, Qt::LeftButton );
  mouseClick( 3, 2.5, Qt::LeftButton );

  QCOMPARE( mLayerPolygon->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 3 2.5, 4 1))" ) );

  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  // no other unexpected changes happened
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 1 );
  QCOMPARE( mLayerPoint->undoStack()->index(), 1 );
}


void TestQgsNodeTool::testAddVertexAtEndpoint()
{
  // offset of the endpoint marker - currently set as 15px away from the last node in direction of the line
  double offsetInMapUnits = 15 * mCanvas->mapSettings().mapUnitsPerPixel();

  // add vertex at the end

  mouseMove( 1, 3 ); // first we need to move to the vertex
  mouseClick( 1, 3 + offsetInMapUnits, Qt::LeftButton );
  mouseClick( 2, 3, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3, 2 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // add vertex at the start

  mouseMove( 2, 1 ); // first we need to move to the vertex
  mouseClick( 2 + offsetInMapUnits, 1, Qt::LeftButton );
  mouseClick( 2, 2, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 2, 2 1, 1 1, 1 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );
}


void TestQgsNodeTool::testDeleteVertex()
{
  // delete vertex in linestring

  mouseClick( 1, 1, Qt::LeftButton );
  keyClick( Qt::Key_Delete );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 3)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // delete vertex in polygon

  mouseClick( 7, 4, Qt::LeftButton );
  keyClick( Qt::Key_Delete );

  QCOMPARE( mLayerPolygon->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 4 4, 4 1))" ) );

  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  // delete vertex in point - deleting its geometry

  mouseClick( 2, 3, Qt::LeftButton );
  keyClick( Qt::Key_Delete );

  QCOMPARE( mLayerPoint->undoStack()->index(), 2 );
  QCOMPARE( mLayerPoint->getFeature( mFidPointF1 ).geometry(), QgsGeometry() );

  mLayerPoint->undoStack()->undo();

  QCOMPARE( mLayerPoint->getFeature( mFidPointF1 ).geometry(), QgsGeometry::fromWkt( "POINT(2 3)" ) );

  // delete a vertex by dragging a selection rect

  mousePress( 0.5, 2.5, Qt::LeftButton );
  mouseMove( 1.5, 3.5 );
  mouseRelease( 1.5, 3.5, Qt::LeftButton );
  keyClick( Qt::Key_Delete );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );

  // no other unexpected changes happened
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 1 );
  QCOMPARE( mLayerPoint->undoStack()->index(), 1 );
}

void TestQgsNodeTool::testMoveMultipleVertices()
{
  // select two vertices
  mousePress( 0.5, 0.5, Qt::LeftButton );
  mouseMove( 1.5, 3.5 );
  mouseRelease( 1.5, 3.5, Qt::LeftButton );

  // move them by -1,-1
  mouseClick( 1, 1, Qt::LeftButton );
  mouseClick( 0, 0, Qt::LeftButton );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 0 0, 0 2)" ) );

  mLayerLine->undoStack()->undo();
  QCOMPARE( mLayerLine->undoStack()->index(), 1 );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );
}

void TestQgsNodeTool::testMoveVertexTopo()
{
  // test moving of vertices of two features at once

  QgsProject::instance()->setTopologicalEditing( true );

  // connect linestring with polygon at point (2, 1)
  mouseClick( 4, 1, Qt::LeftButton );
  mouseClick( 2, 1, Qt::LeftButton );

  // move shared node of linestring and polygon
  mouseClick( 2, 1, Qt::LeftButton );
  mouseClick( 3, 3, Qt::LeftButton );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(3 3, 1 1, 1 3)" ) );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((3 3, 7 1, 7 4, 4 4, 3 3))" ) );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 3 );  // one more move of node from earlier
  mLayerLine->undoStack()->undo();
  mLayerPolygon->undoStack()->undo();
  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  QgsProject::instance()->setTopologicalEditing( false );
}

void TestQgsNodeTool::testDeleteVertexTopo()
{
  // test deletion of vertices with topological editing enabled

  QgsProject::instance()->setTopologicalEditing( true );

  // connect linestring with polygon at point (2, 1)
  mouseClick( 4, 1, Qt::LeftButton );
  mouseClick( 2, 1, Qt::LeftButton );

  // move shared node of linestring and polygon
  mouseClick( 2, 1, Qt::LeftButton );
  keyClick( Qt::Key_Delete );

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(1 1, 1 3)" ) );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((7 1, 7 4, 4 4, 7 1))" ) );

  QCOMPARE( mLayerLine->undoStack()->index(), 2 );
  QCOMPARE( mLayerPolygon->undoStack()->index(), 3 );  // one more move of node from earlier
  mLayerLine->undoStack()->undo();
  mLayerPolygon->undoStack()->undo();
  mLayerPolygon->undoStack()->undo();

  QCOMPARE( mLayerLine->getFeature( mFidLineF1 ).geometry(), QgsGeometry::fromWkt( "LINESTRING(2 1, 1 1, 1 3)" ) );
  QCOMPARE( mLayerPolygon->getFeature( mFidPolygonF1 ).geometry(), QgsGeometry::fromWkt( "POLYGON((4 1, 7 1, 7 4, 4 4, 4 1))" ) );

  QgsProject::instance()->setTopologicalEditing( false );
}

QGSTEST_MAIN( TestQgsNodeTool )
#include "testqgsnodetool.moc"
