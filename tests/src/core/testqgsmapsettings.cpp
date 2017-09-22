/***************************************************************************
     testqgsmapsettings.cpp
     --------------------------------------
    Date                 : Tue  6 Feb 2015
    Copyright            : (C) 2014 by Sandro Santilli
    Email                : strk@keybit.net
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
#include <math.h>

//header for class being tested
#include "qgsexpression.h"
#include "qgsexpressioncontext.h"
#include "qgsrectangle.h"
#include "qgsmapsettings.h"
#include "qgspointxy.h"
#include "qgslogger.h"
#include "qgsapplication.h"
#include "qgsmaplayerlistutils.h"
#include "qgsvectorlayer.h"

class TestQgsMapSettings: public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase();
    void cleanupTestCase();
    void testDefaults();
    void visibleExtent();
    void mapUnitsPerPixel();
    void visiblePolygon();
    void testIsLayerVisible();
    void testMapLayerListUtils();
    void testXmlReadWrite();

  private:
    QString toString( const QPolygonF &p, int decimalPlaces = 2 ) const;
};

void TestQgsMapSettings::initTestCase()
{
  QgsApplication::init();
  QgsApplication::initQgis();
  QgsApplication::showSettings();
}

void TestQgsMapSettings::cleanupTestCase()
{
  QgsApplication::exitQgis();
}

QString TestQgsMapSettings::toString( const QPolygonF &p, int dec ) const
{
  QString s;
  const char *sep = "";
  double r = std::pow( 10.0, dec );
  for ( int i = 0; i < p.size(); ++i )
  {
    s += QStringLiteral( "%1%2 %3" ).arg( sep )
         .arg( int( p[i].x() * r ) / r )
         .arg( int( p[i].y() * r ) / r );
    sep = ",";
  }

  return s;
}

void TestQgsMapSettings::testDefaults()
{
  QgsMapSettings ms;
  QCOMPARE( ms.destinationCrs(), QgsCoordinateReferenceSystem() );
}

void TestQgsMapSettings::visibleExtent()
{
  QgsMapSettings ms;

  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );
  ms.setOutputSize( QSize( 100, 50 ) );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-50,0 : 150,100" ) );

  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );
  ms.setOutputSize( QSize( 100, 100 ) );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "0,0 : 100,100" ) );

  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );
  ms.setOutputSize( QSize( 50, 100 ) );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "0,-50 : 100,150" ) );

  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );
  ms.setOutputSize( QSize( 50, 100 ) );
  ms.setRotation( 90 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-50,0 : 150,100" ) );
  ms.setRotation( -90 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-50,0 : 150,100" ) );

  ms.setExtent( QgsRectangle( 0, 0, 100, 50 ) );
  ms.setOutputSize( QSize( 50, 100 ) );
  ms.setRotation( 0 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "0,-75 : 100,125" ) );
  ms.setRotation( 90 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-50,-25 : 150,75" ) );
  ms.setRotation( -90 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-50,-25 : 150,75" ) );
  ms.setRotation( 45 );
  QCOMPARE( ms.visibleExtent().toString( 0 ), QString( "-56,-81 : 156,131" ) );
}

void TestQgsMapSettings::mapUnitsPerPixel()
{
  QgsMapSettings ms;
  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );

  ms.setOutputSize( QSize( 100, 50 ) );
  QCOMPARE( ms.mapUnitsPerPixel(), 2.0 );

  ms.setOutputSize( QSize( 100, 100 ) );
  QCOMPARE( ms.mapUnitsPerPixel(), 1.0 );

  ms.setOutputSize( QSize( 50, 100 ) );
  QCOMPARE( ms.mapUnitsPerPixel(), 2.0 );

  ms.setOutputSize( QSize( 5000, 1000 ) );
  QCOMPARE( ms.mapUnitsPerPixel(), 0.1 );

  ms.setOutputSize( QSize( 1000, 500 ) );
  QCOMPARE( ms.mapUnitsPerPixel(), 0.2 );
}

void TestQgsMapSettings::visiblePolygon()
{
  QgsMapSettings ms;

  ms.setExtent( QgsRectangle( 0, 0, 100, 100 ) );
  ms.setOutputSize( QSize( 100, 50 ) );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "-50 100,150 100,150 0,-50 0" ) );

  ms.setExtent( QgsRectangle( 0, -50, 100, 0 ) );
  ms.setOutputSize( QSize( 100, 50 ) );
  ms.setRotation( 90 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "25 -75,25 25,75 25,75 -75" ) );
  ms.setRotation( -90 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "75 25,75 -75,25 -75,25 25" ) );
  ms.setRotation( 30 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "-5.8 -28.34,80.8 21.65,105.8 -21.65,19.19 -71.65" ) );
  ms.setRotation( -30 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "19.19 21.65,105.8 -28.34,80.8 -71.65,-5.8 -21.65" ) );
  ms.setRotation( 45 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "-3.03 -42.67,67.67 28.03,103.03 -7.32,32.32 -78.03" ) );
  ms.setRotation( -45 );
  QCOMPARE( toString( ms.visiblePolygon() ),
            QString( "32.32 28.03,103.03 -42.67,67.67 -78.03,-3.03 -7.32" ) );
}

void TestQgsMapSettings::testIsLayerVisible()
{
  QgsVectorLayer *vlA = new QgsVectorLayer( "Point", "a", "memory" );
  QgsVectorLayer *vlB = new QgsVectorLayer( "Point", "b", "memory" );

  QList<QgsMapLayer *> layers;
  layers << vlA << vlB;

  QgsMapSettings ms;
  ms.setLayers( layers );
  QgsExpressionContext context;
  context << QgsExpressionContextUtils::mapSettingsScope( ms );

  // test checking for visible layer by id
  QgsExpression e( QString( "is_layer_visible( '%1' )" ).arg( vlA-> id() ) );
  QVariant r = e.evaluate( &context );
  QCOMPARE( r.toBool(), true );

  // test checking for visible layer by name
  QgsExpression e2( QString( "is_layer_visible( '%1' )" ).arg( vlB-> name() ) );
  r = e2.evaluate( &context );
  QCOMPARE( r.toBool(), true );

  // test checking for non-existent layer
  QgsExpression e3( QString( "is_layer_visible( 'non matching name' )" ) );
  r = e3.evaluate( &context );
  QCOMPARE( r.toBool(), false );

  delete vlA;
  delete vlB;
}

void TestQgsMapSettings::testMapLayerListUtils()
{
  QgsVectorLayer *vlA = new QgsVectorLayer( "Point", "a", "memory" );
  QgsVectorLayer *vlB = new QgsVectorLayer( "Point", "b", "memory" );

  QList<QgsMapLayer *> listRawSource;
  listRawSource << vlA << vlB;

  QgsMapLayer *l = _qgis_findLayer( listRawSource, QStringLiteral( "a" ) );
  QCOMPARE( l, vlA );

  l = _qgis_findLayer( listRawSource, QStringLiteral( "z" ) );
  QCOMPARE( !l, true );

  QgsWeakMapLayerPointerList listQPointer = _qgis_listRawToQPointer( listRawSource );

  QCOMPARE( listQPointer.count(), 2 );
  QCOMPARE( listQPointer[0].data(), vlA );
  QCOMPARE( listQPointer[1].data(), vlB );

  QList<QgsMapLayer *> listRaw = _qgis_listQPointerToRaw( listQPointer );

  QCOMPARE( listRaw.count(), 2 );
  QCOMPARE( listRaw[0], vlA );
  QCOMPARE( listRaw[1], vlB );

  QStringList listIDs = _qgis_listQPointerToIDs( listQPointer );

  QCOMPARE( listIDs.count(), 2 );
  QCOMPARE( listIDs[0], vlA->id() );
  QCOMPARE( listIDs[1], vlB->id() );

  // now delete one layer!
  // QPointer to vlA must get invalidated
  delete vlA;

  QCOMPARE( listQPointer.count(), 2 );  // still two items but one is invalid

  QList<QgsMapLayer *> listRaw2 = _qgis_listQPointerToRaw( listQPointer );

  QCOMPARE( listRaw2.count(), 1 );
  QCOMPARE( listRaw2[0], vlB );

  QStringList listIDs2 = _qgis_listQPointerToIDs( listQPointer );

  QCOMPARE( listIDs2.count(), 1 );
  QCOMPARE( listIDs2[0], vlB->id() );

  delete vlB;
}

void TestQgsMapSettings::testXmlReadWrite()
{
  //create a test dom element
  QDomImplementation DomImplementation;
  QDomDocumentType documentType =
    DomImplementation.createDocumentType(
      "qgis", "http://mrcc.com/qgis.dtd", "SYSTEM" );
  QDomDocument doc( documentType );
  QDomElement element = doc.createElement( "s" );

  //create a map settings object
  QgsMapSettings s1;
  s1.setDestinationCrs( QgsCoordinateReferenceSystem( QStringLiteral( "EPSG:3111" ) ) );

  //write to xml
  s1.writeXml( element, doc );

  // read a copy from xml
  QgsMapSettings s2;
  s2.readXml( element );

  QCOMPARE( s2.destinationCrs().authid(), QStringLiteral( "EPSG:3111" ) );

  // test writing a map settings without a valid CRS
  element = doc.createElement( "s" );
  s1.setDestinationCrs( QgsCoordinateReferenceSystem() );
  s1.writeXml( element, doc );
  s2.readXml( element );
  QVERIFY( !s2.destinationCrs().isValid() );
}

QGSTEST_MAIN( TestQgsMapSettings )
#include "testqgsmapsettings.moc"
