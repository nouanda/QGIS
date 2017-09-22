/***************************************************************************
     testqgspointlocator.cpp
     --------------------------------------
    Date                 : November 2014
    Copyright            : (C) 2014 by Martin Dobias
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
#include <QObject>
#include <QString>

#include "qgsapplication.h"
#include "qgsvectorlayer.h"
#include "qgsvectordataprovider.h"
#include "qgsgeometry.h"
#include "qgsproject.h"
#include "qgspointlocator.h"
#include "qgspolygon.h"


struct FilterExcludePoint : public QgsPointLocator::MatchFilter
{
  explicit FilterExcludePoint( const QgsPointXY &p ) : mPoint( p ) {}

  bool acceptMatch( const QgsPointLocator::Match &match ) { return match.point() != mPoint; }

  QgsPointXY mPoint;
};

struct FilterExcludeEdge : public QgsPointLocator::MatchFilter
{
  FilterExcludeEdge( const QgsPointXY &p1, const QgsPointXY &p2 )
    : mP1( p1 )
    , mP2( p2 )
  {}

  bool acceptMatch( const QgsPointLocator::Match &match )
  {
    QgsPointXY p1, p2;
    match.edgePoints( p1, p2 );
    return !( p1 == mP1 && p2 == mP2 ) && !( p1 == mP2 && p2 == mP1 );
  }

  QgsPointXY mP1, mP2;
};


class TestQgsPointLocator : public QObject
{
    Q_OBJECT
  public:
    TestQgsPointLocator()
      : mVL( 0 )
    {}

  private:
    QgsVectorLayer *mVL = nullptr;

  private slots:

    void initTestCase()
    {
      QgsApplication::init();
      QgsApplication::initQgis();
      // Will make sure the settings dir with the style file for color ramp is created
      QgsApplication::createDatabase();
      QgsApplication::showSettings();

      // vector layer with a triangle:
      // (0,1) +---+ (1,1)
      //        \  |
      //         \ |
      //          \|
      //           + (1,0)
      mVL = new QgsVectorLayer( QStringLiteral( "Polygon" ), QStringLiteral( "x" ), QStringLiteral( "memory" ) );
      QgsFeature ff( 0 );
      QgsPolygon polygon;
      QgsPolyline polyline;
      polyline << QgsPointXY( 0, 1 ) << QgsPointXY( 1, 0 ) << QgsPointXY( 1, 1 ) << QgsPointXY( 0, 1 );
      polygon << polyline;
      QgsGeometry ffGeom = QgsGeometry::fromPolygon( polygon );
      ff.setGeometry( ffGeom );
      QgsFeatureList flist;
      flist << ff;
      mVL->dataProvider()->addFeatures( flist );

      QgsProject::instance()->addMapLayer( mVL );
    }

    void cleanupTestCase()
    {
      QgsApplication::exitQgis();
    }

    void testNearestVertex()
    {
      QgsPointLocator loc( mVL );
      QgsPointXY pt( 2, 2 );
      QgsPointLocator::Match m = loc.nearestVertex( pt, 999 );
      QVERIFY( m.isValid() );
      QVERIFY( m.hasVertex() );
      QCOMPARE( m.layer(), mVL );
      QCOMPARE( m.featureId(), ( QgsFeatureId )1 );
      QCOMPARE( m.point(), QgsPointXY( 1, 1 ) );
      QCOMPARE( m.distance(), std::sqrt( 2.0 ) );
      QCOMPARE( m.vertexIndex(), 2 );
    }

    void testNearestEdge()
    {
      QgsPointLocator loc( mVL );
      QgsPointXY pt( 1.1, 0.5 );
      QgsPointLocator::Match m = loc.nearestEdge( pt, 999 );
      QVERIFY( m.isValid() );
      QVERIFY( m.hasEdge() );
      QCOMPARE( m.layer(), mVL );
      QCOMPARE( m.featureId(), ( QgsFeatureId )1 );
      QCOMPARE( m.point(), QgsPointXY( 1, 0.5 ) );
      QCOMPARE( m.distance(), 0.1 );
      QCOMPARE( m.vertexIndex(), 1 );
      QgsPointXY pt1, pt2;
      m.edgePoints( pt1, pt2 );
      QCOMPARE( pt1, QgsPointXY( 1, 0 ) );
      QCOMPARE( pt2, QgsPointXY( 1, 1 ) );
    }

    void testPointInPolygon()
    {
      QgsPointLocator loc( mVL );
      QgsPointLocator::MatchList mValid = loc.pointInPolygon( QgsPointXY( 0.8, 0.8 ) );
      QCOMPARE( mValid.count(), 1 );
      QgsPointLocator::Match m = mValid[0];
      QVERIFY( m.isValid() );
      QVERIFY( m.hasArea() );
      QCOMPARE( m.layer(), mVL );
      QCOMPARE( m.featureId(), ( QgsFeatureId )1 );

      QgsPointLocator::MatchList mInvalid = loc.pointInPolygon( QgsPointXY( 0, 0 ) );
      QCOMPARE( mInvalid.count(), 0 );
    }

#if 0 // verticesInRect() not implemented
    void testVerticesInRect()
    {
      QgsPointLocator loc( mVL );
      QgsPointLocator::MatchList lst = loc.verticesInRect( QgsPointXY( 1, 0 ), 2 );
      QCOMPARE( lst.count(), 4 );
      QCOMPARE( lst[0].point(), QgsPointXY( 1, 0 ) );
      QCOMPARE( lst[0].distance(), 0. );
      QCOMPARE( lst[1].point(), QgsPointXY( 1, 1 ) );
      QCOMPARE( lst[1].distance(), 1. );
      QCOMPARE( lst[2].point(), QgsPointXY( 0, 1 ) );
      QCOMPARE( lst[2].distance(), std::sqrt( 2 ) );

      QgsPointLocator::MatchList lst2 = loc.verticesInRect( QgsPointXY( 1, 0 ), 1 );
      QCOMPARE( lst2.count(), 2 );

      // test match filtering
      FilterExcludePoint myFilter( QgsPointXY( 1, 0 ) );
      QgsPointLocator::MatchList lst3 = loc.verticesInRect( QgsPointXY( 1, 0 ), 1, &myFilter );
      QCOMPARE( lst3.count(), 1 );
      QCOMPARE( lst3[0].point(), QgsPointXY( 1, 1 ) );
    }
#endif

    void testEdgesInTolerance()
    {
      QgsPointLocator loc( mVL );
      QgsPointLocator::MatchList lst = loc.edgesInRect( QgsPointXY( 0, 0 ), 2 );
      QCOMPARE( lst.count(), 3 );

      QgsPointLocator::MatchList lst2 = loc.edgesInRect( QgsPointXY( 0, 0 ), 0.9 );
      QCOMPARE( lst2.count(), 1 );

      // test match filtering
      FilterExcludeEdge myFilter( QgsPointXY( 1, 0 ), QgsPointXY( 0, 1 ) );
      QgsPointLocator::MatchList lst3 = loc.edgesInRect( QgsPointXY( 0, 0 ), 2, &myFilter );
      QCOMPARE( lst3.count(), 2 );
    }


    void testLayerUpdates()
    {

      QgsPointLocator loc( mVL );

      QgsPointLocator::Match mAddV0 = loc.nearestVertex( QgsPointXY( 12, 12 ), 999 );
      QVERIFY( mAddV0.isValid() );
      QCOMPARE( mAddV0.point(), QgsPointXY( 1, 1 ) );

      mVL->startEditing();

      // add a new feature
      QgsFeature ff( 0 );
      QgsPolygon polygon;
      QgsPolyline polyline;
      polyline << QgsPointXY( 10, 11 ) << QgsPointXY( 11, 10 ) << QgsPointXY( 11, 11 ) << QgsPointXY( 10, 11 );
      polygon << polyline;
      QgsGeometry ffGeom = QgsGeometry::fromPolygon( polygon ) ;
      ff.setGeometry( ffGeom );
      QgsFeatureList flist;
      flist << ff;
      bool resA = mVL->addFeature( ff );
      QVERIFY( resA );

      // verify it is added in the point locator
      QgsPointLocator::Match mAddV = loc.nearestVertex( QgsPointXY( 12, 12 ), 999 );
      QVERIFY( mAddV.isValid() );
      QCOMPARE( mAddV.point(), QgsPointXY( 11, 11 ) );
      QgsPointLocator::Match mAddE = loc.nearestEdge( QgsPointXY( 11.1, 10.5 ), 999 );
      QVERIFY( mAddE.isValid() );
      QCOMPARE( mAddE.point(), QgsPointXY( 11, 10.5 ) );
      QgsPointLocator::MatchList mAddA = loc.pointInPolygon( QgsPointXY( 10.8, 10.8 ) );
      QVERIFY( mAddA.count() == 1 );

      // change geometry
      QgsGeometry *newGeom = new QgsGeometry( ff.geometry() );
      newGeom->moveVertex( 10, 10, 2 ); // change 11,11 to 10,10
      mVL->changeGeometry( ff.id(), *newGeom );
      delete newGeom;

      // verify it is changed in the point locator
      QgsPointLocator::Match mChV = loc.nearestVertex( QgsPointXY( 12, 12 ), 999 );
      QVERIFY( mChV.isValid() );
      QVERIFY( mChV.point() != QgsPointXY( 11, 11 ) ); // that point does not exist anymore
      mChV = loc.nearestVertex( QgsPointXY( 9, 9 ), 999 );
      QVERIFY( mChV.isValid() );
      QVERIFY( mChV.point() == QgsPointXY( 10, 10 ) ); // updated point

      // delete feature
      bool resD = mVL->deleteFeature( ff.id() );
      QVERIFY( resD );

      // verify it is deleted from the point locator
      QgsPointLocator::Match mDelV = loc.nearestVertex( QgsPointXY( 12, 12 ), 999 );
      QVERIFY( mDelV.isValid() );
      QCOMPARE( mDelV.point(), QgsPointXY( 1, 1 ) );

      mVL->rollBack();
    }

    void testExtent()
    {
      QgsRectangle bbox1( 10, 10, 11, 11 ); // out of layer's bounds
      QgsPointLocator loc1( mVL, QgsCoordinateReferenceSystem(), &bbox1 );

      QgsPointLocator::Match m1 = loc1.nearestVertex( QgsPointXY( 2, 2 ), 999 );
      QVERIFY( !m1.isValid() );

      QgsRectangle bbox2( 0, 0, 1, 1 ); // in layer's bounds
      QgsPointLocator loc2( mVL, QgsCoordinateReferenceSystem(), &bbox2 );

      QgsPointLocator::Match m2 = loc2.nearestVertex( QgsPointXY( 2, 2 ), 999 );
      QVERIFY( m2.isValid() );
      QCOMPARE( m2.point(), QgsPointXY( 1, 1 ) );
    }

    void testNullGeometries()
    {
      QgsVectorLayer *vlNullGeom = new QgsVectorLayer( "Polygon", "x", "memory" );
      QgsFeature ff( 0 );
      ff.setGeometry( QgsGeometry() );
      QgsFeatureList flist;
      flist << ff;
      vlNullGeom->dataProvider()->addFeatures( flist );

      QgsPointLocator loc( vlNullGeom, QgsCoordinateReferenceSystem(), nullptr );

      QgsPointLocator::Match m1 = loc.nearestVertex( QgsPointXY( 2, 2 ), std::numeric_limits<double>::max() );
      QVERIFY( !m1.isValid() );

      QgsPointLocator::Match m2 = loc.nearestEdge( QgsPointXY( 2, 2 ), std::numeric_limits<double>::max() );
      QVERIFY( !m2.isValid() );

      delete vlNullGeom;
    }

    void testEmptyGeometries()
    {
      QgsVectorLayer *vlEmptyGeom = new QgsVectorLayer( "Polygon", "x", "memory" );
      QgsFeature ff( 0 );
      QgsGeometry g;
      g.setGeometry( new QgsPolygonV2() );
      ff.setGeometry( g );
      QgsFeatureList flist;
      flist << ff;
      vlEmptyGeom->dataProvider()->addFeatures( flist );

      QgsPointLocator loc( vlEmptyGeom, QgsCoordinateReferenceSystem(), nullptr );

      QgsPointLocator::Match m1 = loc.nearestVertex( QgsPointXY( 2, 2 ), std::numeric_limits<double>::max() );
      QVERIFY( !m1.isValid() );

      QgsPointLocator::Match m2 = loc.nearestEdge( QgsPointXY( 2, 2 ), std::numeric_limits<double>::max() );
      QVERIFY( !m2.isValid() );

      delete vlEmptyGeom;
    }
};

QGSTEST_MAIN( TestQgsPointLocator )

#include "testqgspointlocator.moc"
