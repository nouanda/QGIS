# -*- coding: utf-8 -*-
"""QGIS Unit tests for QgsGeometry.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Tim Sutton'
__date__ = '20/08/2012'
__copyright__ = 'Copyright 2012, The QGIS Project'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import os
import csv
import math

from qgis.core import (
    QgsGeometry,
    QgsVectorLayer,
    QgsFeature,
    QgsPointXY,
    QgsPoint,
    QgsCircularString,
    QgsCompoundCurve,
    QgsCurvePolygon,
    QgsGeometryCollection,
    QgsLineString,
    QgsMultiCurve,
    QgsMultiLineString,
    QgsMultiPointV2,
    QgsMultiPolygonV2,
    QgsMultiSurface,
    QgsPolygonV2,
    QgsCoordinateTransform,
    QgsRectangle,
    QgsWkbTypes
)

from qgis.testing import (
    start_app,
    unittest,
)

from utilities import (
    compareWkt,
    unitTestDataPath,
    writeShape
)

# Convenience instances in case you may need them not used in this test

start_app()
TEST_DATA_DIR = unitTestDataPath()


class TestQgsGeometry(unittest.TestCase):

    def testBool(self):
        """ Test boolean evaluation of QgsGeometry """
        g = QgsGeometry()
        self.assertFalse(g)
        myWKT = 'Point (10 10)'
        g = QgsGeometry.fromWkt(myWKT)
        self.assertTrue(g)
        g.setGeometry(None)
        self.assertFalse(g)

    def testIsEmpty(self):
        """
        the bulk of these tests are in testqgsgeometry.cpp for each QgsAbstractGeometry subclass
        this test just checks the QgsGeometry wrapper
        """
        g = QgsGeometry()
        self.assertTrue(g.isEmpty())
        g = QgsGeometry.fromWkt('Point(10 10 )')
        self.assertFalse(g.isEmpty())
        g = QgsGeometry.fromWkt('MultiPoint ()')
        self.assertTrue(g.isEmpty())

    def testWktPointLoading(self):
        myWKT = 'Point (10 10)'
        myGeometry = QgsGeometry.fromWkt(myWKT)
        self.assertEqual(myGeometry.wkbType(), QgsWkbTypes.Point)

    def testWktMultiPointLoading(self):
        # Standard format
        wkt = 'MultiPoint ((10 15),(20 30))'
        geom = QgsGeometry.fromWkt(wkt)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.MultiPoint, ('Expected:\n%s\nGot:\n%s\n' % (QgsWkbTypes.Point, geom.type())))
        self.assertEqual(geom.geometry().numGeometries(), 2)
        self.assertEqual(geom.geometry().geometryN(0).x(), 10)
        self.assertEqual(geom.geometry().geometryN(0).y(), 15)
        self.assertEqual(geom.geometry().geometryN(1).x(), 20)
        self.assertEqual(geom.geometry().geometryN(1).y(), 30)

        # Check MS SQL format
        wkt = 'MultiPoint (11 16, 21 31)'
        geom = QgsGeometry.fromWkt(wkt)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.MultiPoint, ('Expected:\n%s\nGot:\n%s\n' % (QgsWkbTypes.Point, geom.type())))
        self.assertEqual(geom.geometry().numGeometries(), 2)
        self.assertEqual(geom.geometry().geometryN(0).x(), 11)
        self.assertEqual(geom.geometry().geometryN(0).y(), 16)
        self.assertEqual(geom.geometry().geometryN(1).x(), 21)
        self.assertEqual(geom.geometry().geometryN(1).y(), 31)

    def testFromPoint(self):
        myPoint = QgsGeometry.fromPoint(QgsPointXY(10, 10))
        self.assertEqual(myPoint.wkbType(), QgsWkbTypes.Point)

    def testFromMultiPoint(self):
        myMultiPoint = QgsGeometry.fromMultiPoint([
            (QgsPointXY(0, 0)), (QgsPointXY(1, 1))])
        self.assertEqual(myMultiPoint.wkbType(), QgsWkbTypes.MultiPoint)

    def testFromLine(self):
        myLine = QgsGeometry.fromPolyline([QgsPointXY(1, 1), QgsPointXY(2, 2)])
        self.assertEqual(myLine.wkbType(), QgsWkbTypes.LineString)

    def testFromMultiLine(self):
        myMultiPolyline = QgsGeometry.fromMultiPolyline(
            [[QgsPointXY(0, 0), QgsPointXY(1, 1)], [QgsPointXY(0, 1), QgsPointXY(2, 1)]])
        self.assertEqual(myMultiPolyline.wkbType(), QgsWkbTypes.MultiLineString)

    def testFromPolygon(self):
        myPolygon = QgsGeometry.fromPolygon(
            [[QgsPointXY(1, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)]])
        self.assertEqual(myPolygon.wkbType(), QgsWkbTypes.Polygon)

    def testFromMultiPolygon(self):
        myMultiPolygon = QgsGeometry.fromMultiPolygon([
            [[QgsPointXY(1, 1),
                QgsPointXY(2, 2),
                QgsPointXY(1, 2),
                QgsPointXY(1, 1)]],
            [[QgsPointXY(2, 2),
                QgsPointXY(3, 3),
                QgsPointXY(3, 1),
                QgsPointXY(2, 2)]]
        ])
        self.assertEqual(myMultiPolygon.wkbType(), QgsWkbTypes.MultiPolygon)

    def testReferenceGeometry(self):
        """ Test parsing a whole range of valid reference wkt formats and variants, and checking
        expected values such as length, area, centroids, bounding boxes, etc of the resultant geometry.
        Note the bulk of this test data was taken from the PostGIS WKT test data """

        with open(os.path.join(TEST_DATA_DIR, 'geom_data.csv'), 'r') as f:
            reader = csv.DictReader(f)
            for i, row in enumerate(reader):

                # test that geometry can be created from WKT
                geom = QgsGeometry.fromWkt(row['wkt'])
                if row['valid_wkt']:
                    assert geom, "WKT conversion {} failed: could not create geom:\n{}\n".format(i + 1, row['wkt'])
                else:
                    assert not geom, "Corrupt WKT {} was incorrectly converted to geometry:\n{}\n".format(i + 1, row['wkt'])
                    continue

                # test exporting to WKT results in expected string
                result = geom.exportToWkt()
                exp = row['valid_wkt']
                assert compareWkt(result, exp, 0.000001), "WKT conversion {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result)

                # test num points in geometry
                exp_nodes = int(row['num_points'])
                self.assertEqual(geom.geometry().nCoordinates(), exp_nodes, "Node count {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp_nodes, geom.geometry().nCoordinates()))

                # test num geometries in collections
                exp_geometries = int(row['num_geometries'])
                try:
                    self.assertEqual(geom.geometry().numGeometries(), exp_geometries, "Geometry count {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp_geometries, geom.geometry().numGeometries()))
                except:
                    # some geometry types don't have numGeometries()
                    assert exp_geometries <= 1, "Geometry count {}:  Expected:\n{} geometries but could not call numGeometries()\n".format(i + 1, exp_geometries)

                # test count of rings
                exp_rings = int(row['num_rings'])
                try:
                    self.assertEqual(geom.geometry().numInteriorRings(), exp_rings, "Ring count {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp_rings, geom.geometry().numInteriorRings()))
                except:
                    # some geometry types don't have numInteriorRings()
                    assert exp_rings <= 1, "Ring count {}:  Expected:\n{} rings but could not call numInteriorRings()\n{}".format(i + 1, exp_rings, geom.geometry())

                # test isClosed
                exp = (row['is_closed'] == '1')
                try:
                    self.assertEqual(geom.geometry().isClosed(), exp, "isClosed {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, True, geom.geometry().isClosed()))
                except:
                    # some geometry types don't have isClosed()
                    assert not exp, "isClosed {}:  Expected:\n isClosed() but could not call isClosed()\n".format(i + 1)

                # test geometry centroid
                exp = row['centroid']
                result = geom.centroid().exportToWkt()
                assert compareWkt(result, exp, 0.00001), "Centroid {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result)

                # test bounding box limits
                bbox = geom.geometry().boundingBox()
                exp = float(row['x_min'])
                result = bbox.xMinimum()
                self.assertAlmostEqual(result, exp, 5, "Min X {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))
                exp = float(row['y_min'])
                result = bbox.yMinimum()
                self.assertAlmostEqual(result, exp, 5, "Min Y {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))
                exp = float(row['x_max'])
                result = bbox.xMaximum()
                self.assertAlmostEqual(result, exp, 5, "Max X {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))
                exp = float(row['y_max'])
                result = bbox.yMaximum()
                self.assertAlmostEqual(result, exp, 5, "Max Y {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))

                # test area calculation
                exp = float(row['area'])
                result = geom.geometry().area()
                self.assertAlmostEqual(result, exp, 5, "Area {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))

                # test length calculation
                exp = float(row['length'])
                result = geom.geometry().length()
                self.assertAlmostEqual(result, exp, 5, "Length {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))

                # test perimeter calculation
                exp = float(row['perimeter'])
                result = geom.geometry().perimeter()
                self.assertAlmostEqual(result, exp, 5, "Perimeter {}: mismatch Expected:\n{}\nGot:\n{}\n".format(i + 1, exp, result))

    def testIntersection(self):
        myLine = QgsGeometry.fromPolyline([
            QgsPointXY(0, 0),
            QgsPointXY(1, 1),
            QgsPointXY(2, 2)])
        myPoint = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        intersectionGeom = QgsGeometry.intersection(myLine, myPoint)
        self.assertEqual(intersectionGeom.wkbType(), QgsWkbTypes.Point)

        layer = QgsVectorLayer("Point", "intersection", "memory")
        assert layer.isValid(), "Failed to create valid point memory layer"

        provider = layer.dataProvider()

        ft = QgsFeature()
        ft.setGeometry(intersectionGeom)
        provider.addFeatures([ft])

        self.assertEqual(layer.featureCount(), 1)

    def testBuffer(self):
        myPoint = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        bufferGeom = myPoint.buffer(10, 5)
        self.assertEqual(bufferGeom.wkbType(), QgsWkbTypes.Polygon)
        myTestPoint = QgsGeometry.fromPoint(QgsPointXY(3, 3))
        self.assertTrue(bufferGeom.intersects(myTestPoint))

    def testContains(self):
        myPoly = QgsGeometry.fromPolygon(
            [[QgsPointXY(0, 0),
              QgsPointXY(2, 0),
              QgsPointXY(2, 2),
              QgsPointXY(0, 2),
              QgsPointXY(0, 0)]])
        myPoint = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        self.assertTrue(QgsGeometry.contains(myPoly, myPoint))

    def testTouches(self):
        myLine = QgsGeometry.fromPolyline([
            QgsPointXY(0, 0),
            QgsPointXY(1, 1),
            QgsPointXY(2, 2)])
        myPoly = QgsGeometry.fromPolygon([[
            QgsPointXY(0, 0),
            QgsPointXY(1, 1),
            QgsPointXY(2, 0),
            QgsPointXY(0, 0)]])
        touchesGeom = QgsGeometry.touches(myLine, myPoly)
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     ("True", touchesGeom))
        assert touchesGeom, myMessage

    def testOverlaps(self):
        myPolyA = QgsGeometry.fromPolygon([[
            QgsPointXY(0, 0),
            QgsPointXY(1, 3),
            QgsPointXY(2, 0),
            QgsPointXY(0, 0)]])
        myPolyB = QgsGeometry.fromPolygon([[
            QgsPointXY(0, 0),
            QgsPointXY(2, 0),
            QgsPointXY(2, 2),
            QgsPointXY(0, 2),
            QgsPointXY(0, 0)]])
        overlapsGeom = QgsGeometry.overlaps(myPolyA, myPolyB)
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     ("True", overlapsGeom))
        assert overlapsGeom, myMessage

    def testWithin(self):
        myLine = QgsGeometry.fromPolyline([
            QgsPointXY(0.5, 0.5),
            QgsPointXY(1, 1),
            QgsPointXY(1.5, 1.5)
        ])
        myPoly = QgsGeometry.fromPolygon([[
            QgsPointXY(0, 0),
            QgsPointXY(2, 0),
            QgsPointXY(2, 2),
            QgsPointXY(0, 2),
            QgsPointXY(0, 0)]])
        withinGeom = QgsGeometry.within(myLine, myPoly)
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     ("True", withinGeom))
        assert withinGeom, myMessage

    def testEquals(self):
        myPointA = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        myPointB = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        equalsGeom = QgsGeometry.equals(myPointA, myPointB)
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     ("True", equalsGeom))
        assert equalsGeom, myMessage

    def testCrosses(self):
        myLine = QgsGeometry.fromPolyline([
            QgsPointXY(0, 0),
            QgsPointXY(1, 1),
            QgsPointXY(3, 3)])
        myPoly = QgsGeometry.fromPolygon([[
            QgsPointXY(1, 0),
            QgsPointXY(2, 0),
            QgsPointXY(2, 2),
            QgsPointXY(1, 2),
            QgsPointXY(1, 0)]])
        crossesGeom = QgsGeometry.crosses(myLine, myPoly)
        myMessage = ('Expected:\n%s\nGot:\n%s\n' %
                     ("True", crossesGeom))
        assert crossesGeom, myMessage

    def testSimplifyIssue4189(self):
        """Test we can simplify a complex geometry.

        Note: there is a ticket related to this issue here:
        https://issues.qgis.org/issues/4189

        Backstory: Ole Nielson pointed out an issue to me
        (Tim Sutton) where simplify ftools was dropping
        features. This test replicates that issues.

        Interestingly we could replicate the issue in PostGIS too:
         - doing straight simplify returned no feature
         - transforming to UTM49, then simplify with e.g. 200 threshold is OK
         - as above with 500 threshold drops the feature

         pgsql2shp -f /tmp/dissolve500.shp gis 'select *,
           transform(simplify(transform(geom,32649),500), 4326) as
           simplegeom from dissolve;'
        """
        with open(os.path.join(unitTestDataPath('wkt'), 'simplify_error.wkt'), 'rt') as myWKTFile:
            myWKT = myWKTFile.readline()
        # print myWKT
        myGeometry = QgsGeometry().fromWkt(myWKT)
        assert myGeometry is not None
        myStartLength = len(myWKT)
        myTolerance = 0.00001
        mySimpleGeometry = myGeometry.simplify(myTolerance)
        myEndLength = len(mySimpleGeometry.exportToWkt())
        myMessage = 'Before simplify: %i\nAfter simplify: %i\n : Tolerance %e' % (
            myStartLength, myEndLength, myTolerance)
        myMinimumLength = len('Polygon(())')
        assert myEndLength > myMinimumLength, myMessage

    def testClipping(self):
        """Test that we can clip geometries using other geometries."""
        myMemoryLayer = QgsVectorLayer(
            ('LineString?crs=epsg:4326&field=name:string(20)&index=yes'),
            'clip-in',
            'memory')

        assert myMemoryLayer is not None, 'Provider not initialized'
        myProvider = myMemoryLayer.dataProvider()
        assert myProvider is not None

        myFeature1 = QgsFeature()
        myFeature1.setGeometry(QgsGeometry.fromPolyline([
            QgsPointXY(10, 10),
            QgsPointXY(20, 10),
            QgsPointXY(30, 10),
            QgsPointXY(40, 10),
        ]))
        myFeature1.setAttributes(['Johny'])

        myFeature2 = QgsFeature()
        myFeature2.setGeometry(QgsGeometry.fromPolyline([
            QgsPointXY(10, 10),
            QgsPointXY(20, 20),
            QgsPointXY(30, 30),
            QgsPointXY(40, 40),
        ]))
        myFeature2.setAttributes(['Be'])

        myFeature3 = QgsFeature()
        myFeature3.setGeometry(QgsGeometry.fromPolyline([
            QgsPointXY(10, 10),
            QgsPointXY(10, 20),
            QgsPointXY(10, 30),
            QgsPointXY(10, 40),
        ]))

        myFeature3.setAttributes(['Good'])

        myResult, myFeatures = myProvider.addFeatures(
            [myFeature1, myFeature2, myFeature3])
        assert myResult
        self.assertEqual(len(myFeatures), 3)

        myClipPolygon = QgsGeometry.fromPolygon([[
            QgsPointXY(20, 20),
            QgsPointXY(20, 30),
            QgsPointXY(30, 30),
            QgsPointXY(30, 20),
            QgsPointXY(20, 20),
        ]])
        print(('Clip: %s' % myClipPolygon.exportToWkt()))
        writeShape(myMemoryLayer, 'clipGeometryBefore.shp')
        fit = myProvider.getFeatures()
        myFeatures = []
        myFeature = QgsFeature()
        while fit.nextFeature(myFeature):
            myGeometry = myFeature.geometry()
            if myGeometry.intersects(myClipPolygon):
                # Adds nodes where the clip and the line intersec
                myCombinedGeometry = myGeometry.combine(myClipPolygon)
                # Gives you the areas inside the clip
                mySymmetricalGeometry = myGeometry.symDifference(
                    myCombinedGeometry)
                # Gives you areas outside the clip area
                # myDifferenceGeometry = myCombinedGeometry.difference(
                #    myClipPolygon)
                # print 'Original: %s' % myGeometry.exportToWkt()
                # print 'Combined: %s' % myCombinedGeometry.exportToWkt()
                # print 'Difference: %s' % myDifferenceGeometry.exportToWkt()
                print(('Symmetrical: %s' % mySymmetricalGeometry.exportToWkt()))

                myExpectedWkt = 'Polygon ((20 20, 20 30, 30 30, 30 20, 20 20))'

                # There should only be one feature that intersects this clip
                # poly so this assertion should work.
                assert compareWkt(myExpectedWkt,
                                  mySymmetricalGeometry.exportToWkt())

                myNewFeature = QgsFeature()
                myNewFeature.setAttributes(myFeature.attributes())
                myNewFeature.setGeometry(mySymmetricalGeometry)
                myFeatures.append(myNewFeature)

        myNewMemoryLayer = QgsVectorLayer(
            ('LineString?crs=epsg:4326&field=name:string(20)&index=yes'),
            'clip-out',
            'memory')
        myNewProvider = myNewMemoryLayer.dataProvider()
        myResult, myFeatures = myNewProvider.addFeatures(myFeatures)
        self.assertTrue(myResult)
        self.assertEqual(len(myFeatures), 1)

        writeShape(myNewMemoryLayer, 'clipGeometryAfter.shp')

    def testClosestVertex(self):
        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4 !
        # |
        # 1-+-+-+-+-0 !
        polyline = QgsGeometry.fromPolyline(
            [QgsPointXY(5, 0), QgsPointXY(0, 0), QgsPointXY(0, 4), QgsPointXY(5, 4), QgsPointXY(5, 1), QgsPointXY(1, 1), QgsPointXY(1, 3), QgsPointXY(4, 3), QgsPointXY(4, 2), QgsPointXY(2, 2)]
        )

        (point, atVertex, beforeVertex, afterVertex, dist) = polyline.closestVertex(QgsPointXY(6, 1))
        self.assertEqual(point, QgsPointXY(5, 1))
        self.assertEqual(beforeVertex, 3)
        self.assertEqual(atVertex, 4)
        self.assertEqual(afterVertex, 5)
        self.assertEqual(dist, 1)

        (dist, minDistPoint, afterVertex) = polyline.closestSegmentWithContext(QgsPointXY(6, 2))
        self.assertEqual(dist, 1)
        self.assertEqual(minDistPoint, QgsPointXY(5, 2))
        self.assertEqual(afterVertex, 4)

        (point, atVertex, beforeVertex, afterVertex, dist) = polyline.closestVertex(QgsPointXY(6, 0))
        self.assertEqual(point, QgsPointXY(5, 0))
        self.assertEqual(beforeVertex, -1)
        self.assertEqual(atVertex, 0)
        self.assertEqual(afterVertex, 1)
        self.assertEqual(dist, 1)

        (dist, minDistPoint, afterVertex) = polyline.closestSegmentWithContext(QgsPointXY(6, 0))
        self.assertEqual(dist, 1)
        self.assertEqual(minDistPoint, QgsPointXY(5, 0))
        self.assertEqual(afterVertex, 1)

        (point, atVertex, beforeVertex, afterVertex, dist) = polyline.closestVertex(QgsPointXY(0, -1))
        self.assertEqual(point, QgsPointXY(0, 0))
        self.assertEqual(beforeVertex, 0)
        self.assertEqual(atVertex, 1)
        self.assertEqual(afterVertex, 2)
        self.assertEqual(dist, 1)

        (dist, minDistPoint, afterVertex) = polyline.closestSegmentWithContext(QgsPointXY(0, 1))
        self.assertEqual(dist, 0)
        self.assertEqual(minDistPoint, QgsPointXY(0, 1))
        self.assertEqual(afterVertex, 2)

        #   2-3 6-+-7 !
        #   | | |   |
        # 0-1 4 5   8-9
        polyline = QgsGeometry.fromMultiPolyline(
            [
                [QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 0), ],
                [QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(5, 1), QgsPointXY(5, 0), QgsPointXY(6, 0), ]
            ]
        )
        (point, atVertex, beforeVertex, afterVertex, dist) = polyline.closestVertex(QgsPointXY(5, 2))
        self.assertEqual(point, QgsPointXY(5, 1))
        self.assertEqual(beforeVertex, 6)
        self.assertEqual(atVertex, 7)
        self.assertEqual(afterVertex, 8)
        self.assertEqual(dist, 1)

        (dist, minDistPoint, afterVertex) = polyline.closestSegmentWithContext(QgsPointXY(7, 0))
        self.assertEqual(dist, 1)
        self.assertEqual(minDistPoint, QgsPointXY(6, 0))
        self.assertEqual(afterVertex, 9)

        # 5---4
        # |!  |
        # | 2-3
        # | |
        # 0-1
        polygon = QgsGeometry.fromPolygon(
            [[
                QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0),
            ]]
        )
        (point, atVertex, beforeVertex, afterVertex, dist) = polygon.closestVertex(QgsPointXY(0.7, 1.1))
        self.assertEqual(point, QgsPointXY(1, 1))
        self.assertEqual(beforeVertex, 1)
        self.assertEqual(atVertex, 2)
        self.assertEqual(afterVertex, 3)
        assert abs(dist - 0.1) < 0.00001, "Expected: %f; Got:%f" % (dist, 0.1)

        (dist, minDistPoint, afterVertex) = polygon.closestSegmentWithContext(QgsPointXY(0.7, 1.1))
        self.assertEqual(afterVertex, 2)
        self.assertEqual(minDistPoint, QgsPointXY(1, 1))
        exp = 0.3 ** 2 + 0.1 ** 2
        assert abs(dist - exp) < 0.00001, "Expected: %f; Got:%f" % (exp, dist)

        # 3-+-+-2
        # |     |
        # + 8-7 +
        # | |!| |
        # + 5-6 +
        # |     |
        # 0-+-+-1
        polygon = QgsGeometry.fromPolygon(
            [
                [QgsPointXY(0, 0), QgsPointXY(3, 0), QgsPointXY(3, 3), QgsPointXY(0, 3), QgsPointXY(0, 0)],
                [QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)],
            ]
        )
        (point, atVertex, beforeVertex, afterVertex, dist) = polygon.closestVertex(QgsPointXY(1.1, 1.9))
        self.assertEqual(point, QgsPointXY(1, 2))
        self.assertEqual(beforeVertex, 7)
        self.assertEqual(atVertex, 8)
        self.assertEqual(afterVertex, 9)
        assert abs(dist - 0.02) < 0.00001, "Expected: %f; Got:%f" % (dist, 0.02)

        (dist, minDistPoint, afterVertex) = polygon.closestSegmentWithContext(QgsPointXY(1.2, 1.9))
        self.assertEqual(afterVertex, 8)
        self.assertEqual(minDistPoint, QgsPointXY(1.2, 2))
        exp = 0.01
        assert abs(dist - exp) < 0.00001, "Expected: %f; Got:%f" % (exp, dist)

        # 5-+-4 0-+-9
        # |   | |   |
        # 6 2-3 1-2!+
        # | |     | |
        # 0-1     7-8
        polygon = QgsGeometry.fromMultiPolygon(
            [
                [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0), ]],
                [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0), ]]
            ]
        )
        (point, atVertex, beforeVertex, afterVertex, dist) = polygon.closestVertex(QgsPointXY(4.1, 1.1))
        self.assertEqual(point, QgsPointXY(4, 1))
        self.assertEqual(beforeVertex, 11)
        self.assertEqual(atVertex, 12)
        self.assertEqual(afterVertex, 13)
        assert abs(dist - 0.02) < 0.00001, "Expected: %f; Got:%f" % (dist, 0.02)

        (dist, minDistPoint, afterVertex) = polygon.closestSegmentWithContext(QgsPointXY(4.1, 1.1))
        self.assertEqual(afterVertex, 12)
        self.assertEqual(minDistPoint, QgsPointXY(4, 1))
        exp = 0.02
        assert abs(dist - exp) < 0.00001, "Expected: %f; Got:%f" % (exp, dist)

    def testAdjacentVertex(self):
        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4 !
        # |
        # 1-+-+-+-+-0 !
        polyline = QgsGeometry.fromPolyline(
            [QgsPointXY(5, 0), QgsPointXY(0, 0), QgsPointXY(0, 4), QgsPointXY(5, 4), QgsPointXY(5, 1), QgsPointXY(1, 1), QgsPointXY(1, 3), QgsPointXY(4, 3), QgsPointXY(4, 2), QgsPointXY(2, 2)]
        )

        # don't crash
        (before, after) = polyline.adjacentVertices(-100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        for i in range(0, 10):
            (before, after) = polyline.adjacentVertices(i)
            if i == 0:
                self.assertEqual(before == -1 and after, 1, "Expected (0,1), Got:(%d,%d)" % (before, after))
            elif i == 9:
                self.assertEqual(before == i - 1 and after, -1, "Expected (0,1), Got:(%d,%d)" % (before, after))
            else:
                self.assertEqual(before == i - 1 and after, i + 1, "Expected (0,1), Got:(%d,%d)" % (before, after))

        (before, after) = polyline.adjacentVertices(100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        #   2-3 6-+-7
        #   | | |   |
        # 0-1 4 5   8-9
        polyline = QgsGeometry.fromMultiPolyline(
            [
                [QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 0), ],
                [QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(5, 1), QgsPointXY(5, 0), QgsPointXY(6, 0), ]
            ]
        )

        (before, after) = polyline.adjacentVertices(-100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        for i in range(0, 10):
            (before, after) = polyline.adjacentVertices(i)

            if i == 0 or i == 5:
                self.assertEqual(before == -1 and after, i + 1, "Expected (-1,%d), Got:(%d,%d)" % (i + 1, before, after))
            elif i == 4 or i == 9:
                self.assertEqual(before == i - 1 and after, -1, "Expected (%d,-1), Got:(%d,%d)" % (i - 1, before, after))
            else:
                self.assertEqual(before == i - 1 and after, i + 1, "Expected (%d,%d), Got:(%d,%d)" % (i - 1, i + 1, before, after))

        (before, after) = polyline.adjacentVertices(100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        # 5---4
        # |   |
        # | 2-3
        # | |
        # 0-1
        polygon = QgsGeometry.fromPolygon(
            [[
                QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0),
            ]]
        )

        (before, after) = polygon.adjacentVertices(-100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        for i in range(0, 7):
            (before, after) = polygon.adjacentVertices(i)

            if i == 0 or i == 6:
                self.assertEqual(before == 5 and after, 1, "Expected (5,1), Got:(%d,%d)" % (before, after))
            else:
                self.assertEqual(before == i - 1 and after, i + 1, "Expected (%d,%d), Got:(%d,%d)" % (i - 1, i + 1, before, after))

        (before, after) = polygon.adjacentVertices(100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        # 3-+-+-2
        # |     |
        # + 8-7 +
        # | | | |
        # + 5-6 +
        # |     |
        # 0-+-+-1
        polygon = QgsGeometry.fromPolygon(
            [
                [QgsPointXY(0, 0), QgsPointXY(3, 0), QgsPointXY(3, 3), QgsPointXY(0, 3), QgsPointXY(0, 0)],
                [QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)],
            ]
        )

        (before, after) = polygon.adjacentVertices(-100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        for i in range(0, 8):
            (before, after) = polygon.adjacentVertices(i)

            if i == 0 or i == 4:
                self.assertEqual(before == 3 and after, 1, "Expected (3,1), Got:(%d,%d)" % (before, after))
            elif i == 5:
                self.assertEqual(before == 8 and after, 6, "Expected (2,0), Got:(%d,%d)" % (before, after))
            else:
                self.assertEqual(before == i - 1 and after, i + 1, "Expected (%d,%d), Got:(%d,%d)" % (i - 1, i + 1, before, after))

        (before, after) = polygon.adjacentVertices(100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        # 5-+-4 0-+-9
        # |   | |   |
        # | 2-3 1-2 |
        # | |     | |
        # 0-1     7-8
        polygon = QgsGeometry.fromMultiPolygon(
            [
                [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0), ]],
                [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0), ]]
            ]
        )

        (before, after) = polygon.adjacentVertices(-100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

        for i in range(0, 14):
            (before, after) = polygon.adjacentVertices(i)

            if i == 0 or i == 6:
                self.assertEqual(before == 5 and after, 1, "Expected (5,1), Got:(%d,%d)" % (before, after))
            elif i == 7 or i == 13:
                self.assertEqual(before == 12 and after, 8, "Expected (12,8), Got:(%d,%d)" % (before, after))
            else:
                self.assertEqual(before == i - 1 and after, i + 1, "Expected (%d,%d), Got:(%d,%d)" % (i - 1, i + 1, before, after))

        (before, after) = polygon.adjacentVertices(100)
        self.assertEqual(before == -1 and after, -1, "Expected (-1,-1), Got:(%d,%d)" % (before, after))

    def testVertexAt(self):
        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4 !
        # |
        # 1-+-+-+-+-0 !
        points = [QgsPointXY(5, 0), QgsPointXY(0, 0), QgsPointXY(0, 4), QgsPointXY(5, 4), QgsPointXY(5, 1), QgsPointXY(1, 1), QgsPointXY(1, 3), QgsPointXY(4, 3), QgsPointXY(4, 2), QgsPointXY(2, 2)]
        polyline = QgsGeometry.fromPolyline(points)

        for i in range(0, len(points)):
            self.assertEqual(QgsPoint(points[i]), polyline.vertexAt(i), "Mismatch at %d" % i)

        #   2-3 6-+-7
        #   | | |   |
        # 0-1 4 5   8-9
        points = [
            [QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 0), ],
            [QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(5, 1), QgsPointXY(5, 0), QgsPointXY(6, 0), ]
        ]
        polyline = QgsGeometry.fromMultiPolyline(points)

        p = polyline.vertexAt(-100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        p = polyline.vertexAt(100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        i = 0
        for j in range(0, len(points)):
            for k in range(0, len(points[j])):
                self.assertEqual(QgsPoint(points[j][k]), polyline.vertexAt(i), "Mismatch at %d / %d,%d" % (i, j, k))
                i += 1

        # 5---4
        # |   |
        # | 2-3
        # | |
        # 0-1
        points = [[
            QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0),
        ]]
        polygon = QgsGeometry.fromPolygon(points)

        p = polygon.vertexAt(-100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        p = polygon.vertexAt(100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        i = 0
        for j in range(0, len(points)):
            for k in range(0, len(points[j])):
                self.assertEqual(QgsPoint(points[j][k]), polygon.vertexAt(i), "Mismatch at %d / %d,%d" % (i, j, k))
                i += 1

        # 3-+-+-2
        # |     |
        # + 8-7 +
        # | | | |
        # + 5-6 +
        # |     |
        # 0-+-+-1
        points = [
            [QgsPointXY(0, 0), QgsPointXY(3, 0), QgsPointXY(3, 3), QgsPointXY(0, 3), QgsPointXY(0, 0)],
            [QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)],
        ]
        polygon = QgsGeometry.fromPolygon(points)

        p = polygon.vertexAt(-100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        p = polygon.vertexAt(100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        i = 0
        for j in range(0, len(points)):
            for k in range(0, len(points[j])):
                self.assertEqual(QgsPoint(points[j][k]), polygon.vertexAt(i), "Mismatch at %d / %d,%d" % (i, j, k))
                i += 1

        # 5-+-4 0-+-9
        # |   | |   |
        # | 2-3 1-2 |
        # | |     | |
        # 0-1     7-8
        points = [
            [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0), ]],
            [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0), ]]
        ]

        polygon = QgsGeometry.fromMultiPolygon(points)

        p = polygon.vertexAt(-100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        p = polygon.vertexAt(100)
        self.assertEqual(p, QgsPoint(0, 0), "Expected 0,0, Got {}.{}".format(p.x(), p.y()))

        i = 0
        for j in range(0, len(points)):
            for k in range(0, len(points[j])):
                for l in range(0, len(points[j][k])):
                    p = polygon.vertexAt(i)
                    self.assertEqual(QgsPoint(points[j][k][l]), p, "Got {},{} Expected {} at {} / {},{},{}".format(p.x(), p.y(), points[j][k][l].toString(), i, j, k, l))
                    i += 1

    def testMultipoint(self):
        # #9423
        points = [QgsPointXY(10, 30), QgsPointXY(40, 20), QgsPointXY(30, 10), QgsPointXY(20, 10)]
        wkt = "MultiPoint ((10 30),(40 20),(30 10),(20 10))"
        multipoint = QgsGeometry.fromWkt(wkt)
        assert multipoint.isMultipart(), "Expected MultiPoint to be multipart"
        self.assertEqual(multipoint.wkbType(), QgsWkbTypes.MultiPoint, "Expected wkbType to be WKBMultipoint")
        i = 0
        for p in multipoint.asMultiPoint():
            self.assertEqual(p, points[i], "Expected %s at %d, got %s" % (points[i].toString(), i, p.toString()))
            i += 1

        multipoint = QgsGeometry.fromWkt("MultiPoint ((5 5))")
        self.assertEqual(multipoint.vertexAt(0), QgsPoint(5, 5), "MULTIPOINT fromWkt failed")

        assert multipoint.insertVertex(4, 4, 0), "MULTIPOINT insert 4,4 at 0 failed"
        expwkt = "MultiPoint ((4 4),(5 5))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert multipoint.insertVertex(7, 7, 2), "MULTIPOINT append 7,7 at 2 failed"
        expwkt = "MultiPoint ((4 4),(5 5),(7 7))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert multipoint.insertVertex(6, 6, 2), "MULTIPOINT append 6,6 at 2 failed"
        expwkt = "MultiPoint ((4 4),(5 5),(6 6),(7 7))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert not multipoint.deleteVertex(4), "MULTIPOINT delete at 4 unexpectedly succeeded"
        assert not multipoint.deleteVertex(-1), "MULTIPOINT delete at -1 unexpectedly succeeded"

        assert multipoint.deleteVertex(1), "MULTIPOINT delete at 1 failed"
        expwkt = "MultiPoint ((4 4),(6 6),(7 7))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert multipoint.deleteVertex(2), "MULTIPOINT delete at 2 failed"
        expwkt = "MultiPoint ((4 4),(6 6))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert multipoint.deleteVertex(0), "MULTIPOINT delete at 2 failed"
        expwkt = "MultiPoint ((6 6))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        multipoint = QgsGeometry.fromWkt("MultiPoint ((5 5))")
        self.assertEqual(multipoint.vertexAt(0), QgsPoint(5, 5), "MultiPoint fromWkt failed")

    def testMoveVertex(self):
        multipoint = QgsGeometry.fromWkt("MultiPoint ((5 0),(0 0),(0 4),(5 4),(5 1),(1 1),(1 3),(4 3),(4 2),(2 2))")

        # try moving invalid vertices
        assert not multipoint.moveVertex(9, 9, -1), "move vertex succeeded when it should have failed"
        assert not multipoint.moveVertex(9, 9, 10), "move vertex succeeded when it should have failed"
        assert not multipoint.moveVertex(9, 9, 11), "move vertex succeeded when it should have failed"

        for i in range(0, 10):
            assert multipoint.moveVertex(i + 1, -1 - i, i), "move vertex %d failed" % i
        expwkt = "MultiPoint ((1 -1),(2 -2),(3 -3),(4 -4),(5 -5),(6 -6),(7 -7),(8 -8),(9 -9),(10 -10))"
        wkt = multipoint.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4 !
        # |
        # 1-+-+-+-+-0 !
        polyline = QgsGeometry.fromWkt("LineString (5 0, 0 0, 0 4, 5 4, 5 1, 1 1, 1 3, 4 3, 4 2, 2 2)")

        # try moving invalid vertices
        assert not polyline.moveVertex(9, 9, -1), "move vertex succeeded when it should have failed"
        assert not polyline.moveVertex(9, 9, 10), "move vertex succeeded when it should have failed"
        assert not polyline.moveVertex(9, 9, 11), "move vertex succeeded when it should have failed"

        assert polyline.moveVertex(5.5, 4.5, 3), "move vertex failed"
        expwkt = "LineString (5 0, 0 0, 0 4, 5.5 4.5, 5 1, 1 1, 1 3, 4 3, 4 2, 2 2)"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 5-+-4
        # |   |
        # 6 2-3
        # | |
        # 0-1
        polygon = QgsGeometry.fromWkt("Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))")

        assert not polygon.moveVertex(3, 4, -10), "move vertex unexpectedly succeeded"
        assert not polygon.moveVertex(3, 4, 7), "move vertex unexpectedly succeeded"
        assert not polygon.moveVertex(3, 4, 8), "move vertex unexpectedly succeeded"

        assert polygon.moveVertex(1, 2, 0), "move vertex failed"
        expwkt = "Polygon ((1 2, 1 0, 1 1, 2 1, 2 2, 0 2, 1 2))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.moveVertex(3, 4, 3), "move vertex failed"
        expwkt = "Polygon ((1 2, 1 0, 1 1, 3 4, 2 2, 0 2, 1 2))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.moveVertex(2, 3, 6), "move vertex failed"
        expwkt = "Polygon ((2 3, 1 0, 1 1, 3 4, 2 2, 0 2, 2 3))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 5-+-4 0-+-9
        # |   | |   |
        # 6 2-3 1-2!+
        # | |     | |
        # 0-1     7-8
        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")

        assert not polygon.moveVertex(3, 4, -10), "move vertex unexpectedly succeeded"
        assert not polygon.moveVertex(3, 4, 14), "move vertex unexpectedly succeeded"
        assert not polygon.moveVertex(3, 4, 15), "move vertex unexpectedly succeeded"

        assert polygon.moveVertex(6, 2, 9), "move vertex failed"
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 6 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.moveVertex(1, 2, 0), "move vertex failed"
        expwkt = "MultiPolygon (((1 2, 1 0, 1 1, 2 1, 2 2, 0 2, 1 2)),((4 0, 5 0, 6 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.moveVertex(2, 1, 7), "move vertex failed"
        expwkt = "MultiPolygon (((1 2, 1 0, 1 1, 2 1, 2 2, 0 2, 1 2)),((2 1, 5 0, 6 2, 3 2, 3 1, 4 1, 2 1)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

    def testDeleteVertex(self):
        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4
        # |
        # 1-+-+-+-+-0
        polyline = QgsGeometry.fromWkt("LineString (5 0, 0 0, 0 4, 5 4, 5 1, 1 1, 1 3, 4 3, 4 2, 2 2)")
        assert polyline.deleteVertex(3), "Delete vertex 5 4 failed"
        expwkt = "LineString (5 0, 0 0, 0 4, 5 1, 1 1, 1 3, 4 3, 4 2, 2 2)"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert not polyline.deleteVertex(-5), "Delete vertex -5 unexpectedly succeeded"
        assert not polyline.deleteVertex(100), "Delete vertex 100 unexpectedly succeeded"

        #   2-3 6-+-7
        #   | | |   |
        # 0-1 4 5   8-9
        polyline = QgsGeometry.fromWkt("MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 0),(3 0, 3 1, 5 1, 5 0, 6 0))")
        assert polyline.deleteVertex(5), "Delete vertex 5 failed"
        expwkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 0), (3 1, 5 1, 5 0, 6 0))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert not polyline.deleteVertex(-100), "Delete vertex -100 unexpectedly succeeded"
        assert not polyline.deleteVertex(100), "Delete vertex 100 unexpectedly succeeded"

        assert polyline.deleteVertex(0), "Delete vertex 0 failed"
        expwkt = "MultiLineString ((1 0, 1 1, 2 1, 2 0), (3 1, 5 1, 5 0, 6 0))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polyline = QgsGeometry.fromWkt("MultiLineString ((0 0, 1 0, 1 1, 2 1,2 0),(3 0, 3 1, 5 1, 5 0, 6 0))")
        for i in range(4):
            assert polyline.deleteVertex(5), "Delete vertex 5 failed"
        expwkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 0))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 5---4
        # |   |
        # | 2-3
        # | |
        # 0-1
        polygon = QgsGeometry.fromWkt("Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))")

        assert polygon.deleteVertex(2), "Delete vertex 2 failed"
        expwkt = "Polygon ((0 0, 1 0, 2 1, 2 2, 0 2, 0 0))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.deleteVertex(0), "Delete vertex 0 failed"
        expwkt = "Polygon ((1 0, 2 1, 2 2, 0 2, 1 0))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.deleteVertex(4), "Delete vertex 4 failed"
        # "Polygon ((2 1, 2 2, 0 2, 2 1))" #several possibilities are correct here
        expwkt = "Polygon ((0 2, 2 1, 2 2, 0 2))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert not polygon.deleteVertex(-100), "Delete vertex -100 unexpectedly succeeded"
        assert not polygon.deleteVertex(100), "Delete vertex 100 unexpectedly succeeded"

        # 5-+-4 0-+-9
        # |   | |   |
        # 6 2-3 1-2 +
        # | |     | |
        # 0-1     7-8
        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")
        assert polygon.deleteVertex(9), "Delete vertex 5 2 failed"
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.deleteVertex(0), "Delete vertex 0 failed"
        expwkt = "MultiPolygon (((1 0, 1 1, 2 1, 2 2, 0 2, 1 0)),((4 0, 5 0, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert polygon.deleteVertex(6), "Delete vertex 6 failed"
        expwkt = "MultiPolygon (((1 0, 1 1, 2 1, 2 2, 0 2, 1 0)),((5 0, 3 2, 3 1, 4 1, 5 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")
        for i in range(4):
            assert polygon.deleteVertex(0), "Delete vertex 0 failed"

        expwkt = "MultiPolygon (((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 3-+-+-+-+-+-+-+-+-2
        # |                 |
        # + 8-7 3-2 8-7 3-2 +
        # | | | | | | | | | |
        # + 5-6 0-1 5-6 0-1 +
        # |                 |
        # 0-+-+-+-+---+-+-+-1
        polygon = QgsGeometry.fromWkt("Polygon ((0 0, 9 0, 9 3, 0 3, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1),(3 1, 4 1, 4 2, 3 2, 3 1),(5 1, 6 1, 6 2, 5 2, 5 1),(7 1, 8 1, 8 2, 7 2, 7 1))")
        #                                         0   1    2    3    4     5    6    7    8    9     10   11   12   13   14    15   16   17   18   19    20  21   22   23   24

        for i in range(2):
            assert polygon.deleteVertex(16), "Delete vertex 16 failed" % i

        expwkt = "Polygon ((0 0, 9 0, 9 3, 0 3, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1),(3 1, 4 1, 4 2, 3 2, 3 1),(7 1, 8 1, 8 2, 7 2, 7 1))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        for i in range(3):
            for j in range(2):
                assert polygon.deleteVertex(5), "Delete vertex 5 failed" % i

        expwkt = "Polygon ((0 0, 9 0, 9 3, 0 3, 0 0))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # Remove whole outer ring, inner ring should become outer
        polygon = QgsGeometry.fromWkt("Polygon ((0 0, 9 0, 9 3, 0 3, 0 0),(1 1, 2 1, 2 2, 1 2, 1 1))")
        for i in range(2):
            assert polygon.deleteVertex(0), "Delete vertex 16 failed" % i

        expwkt = "Polygon ((1 1, 2 1, 2 2, 1 2, 1 1))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

    def testInsertVertex(self):
        linestring = QgsGeometry.fromWkt("LineString(1 0, 2 0)")

        assert linestring.insertVertex(0, 0, 0), "Insert vertex 0 0 at 0 failed"
        expwkt = "LineString (0 0, 1 0, 2 0)"
        wkt = linestring.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert linestring.insertVertex(1.5, 0, 2), "Insert vertex 1.5 0 at 2 failed"
        expwkt = "LineString (0 0, 1 0, 1.5 0, 2 0)"
        wkt = linestring.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        assert not linestring.insertVertex(3, 0, 5), "Insert vertex 3 0 at 5 should have failed"

        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")
        assert polygon.insertVertex(0, 0, 8), "Insert vertex 0 0 at 8 failed"
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 0 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")
        assert polygon.insertVertex(0, 0, 7), "Insert vertex 0 0 at 7 failed"
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((0 0, 4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 0 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

    def testTranslate(self):
        point = QgsGeometry.fromWkt("Point (1 1)")
        self.assertEqual(point.translate(1, 2), 0, "Translate failed")
        expwkt = "Point (2 3)"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        point = QgsGeometry.fromWkt("MultiPoint ((1 1),(2 2),(3 3))")
        self.assertEqual(point.translate(1, 2), 0, "Translate failed")
        expwkt = "MultiPoint ((2 3),(3 4),(4 5))"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        linestring = QgsGeometry.fromWkt("LineString (1 0, 2 0)")
        self.assertEqual(linestring.translate(1, 2), 0, "Translate failed")
        expwkt = "LineString (2 2, 3 2)"
        wkt = linestring.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polygon = QgsGeometry.fromWkt("MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))")
        self.assertEqual(polygon.translate(1, 2), 0, "Translate failed")
        expwkt = "MultiPolygon (((1 2, 2 2, 2 3, 3 3, 3 4, 1 4, 1 2)),((5 2, 6 2, 6 2, 4 4, 4 3, 5 3, 5 2)))"
        wkt = polygon.exportToWkt()

        ct = QgsCoordinateTransform()

        point = QgsGeometry.fromWkt("Point (1 1)")
        self.assertEqual(point.transform(ct), 0, "Translate failed")
        expwkt = "Point (1 1)"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        point = QgsGeometry.fromWkt("MultiPoint ((1 1),(2 2),(3 3))")
        self.assertEqual(point.transform(ct), 0, "Translate failed")
        expwkt = "MultiPoint ((1 1),(2 2),(3 3))"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        linestring = QgsGeometry.fromWkt("LineString (1 0, 2 0)")
        self.assertEqual(linestring.transform(ct), 0, "Translate failed")
        expwkt = "LineString (1 0, 2 0)"
        wkt = linestring.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polygon = QgsGeometry.fromWkt("MultiPolygon(((0 0,1 0,1 1,2 1,2 2,0 2,0 0)),((4 0,5 0,5 2,3 2,3 1,4 1,4 0)))")
        self.assertEqual(polygon.transform(ct), 0, "Translate failed")
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()

    def testExtrude(self):
        # test with empty geometry
        g = QgsGeometry()
        self.assertTrue(g.extrude(1, 2).isNull())

        points = [QgsPointXY(1, 2), QgsPointXY(3, 2), QgsPointXY(4, 3)]
        line = QgsGeometry.fromPolyline(points)
        expected = QgsGeometry.fromWkt('Polygon ((1 2, 3 2, 4 3, 5 5, 4 4, 2 4, 1 2))')
        self.assertEqual(line.extrude(1, 2).exportToWkt(), expected.exportToWkt())

        points2 = [[QgsPointXY(1, 2), QgsPointXY(3, 2)], [QgsPointXY(4, 3), QgsPointXY(8, 3)]]
        multiline = QgsGeometry.fromMultiPolyline(points2)
        expected = QgsGeometry.fromWkt('MultiPolygon (((1 2, 3 2, 4 4, 2 4, 1 2)),((4 3, 8 3, 9 5, 5 5, 4 3)))')
        self.assertEqual(multiline.extrude(1, 2).exportToWkt(), expected.exportToWkt())

    def testNearestPoint(self):
        # test with empty geometries
        g1 = QgsGeometry()
        g2 = QgsGeometry()
        self.assertTrue(g1.nearestPoint(g2).isNull())
        g1 = QgsGeometry.fromWkt('LineString( 1 1, 5 1, 5 5 )')
        self.assertTrue(g1.nearestPoint(g2).isNull())
        self.assertTrue(g2.nearestPoint(g1).isNull())

        g2 = QgsGeometry.fromWkt('Point( 6 3 )')
        expWkt = 'Point( 5 3 )'
        wkt = g1.nearestPoint(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))
        expWkt = 'Point( 6 3 )'
        wkt = g2.nearestPoint(g1).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

        g1 = QgsGeometry.fromWkt('Polygon ((1 1, 5 1, 5 5, 1 5, 1 1))')
        g2 = QgsGeometry.fromWkt('Point( 6 3 )')
        expWkt = 'Point( 5 3 )'
        wkt = g1.nearestPoint(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

        expWkt = 'Point( 6 3 )'
        wkt = g2.nearestPoint(g1).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))
        g2 = QgsGeometry.fromWkt('Point( 2 3 )')
        expWkt = 'Point( 2 3 )'
        wkt = g1.nearestPoint(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

    def testShortestLine(self):
        # test with empty geometries
        g1 = QgsGeometry()
        g2 = QgsGeometry()
        self.assertTrue(g1.shortestLine(g2).isNull())
        g1 = QgsGeometry.fromWkt('LineString( 1 1, 5 1, 5 5 )')
        self.assertTrue(g1.shortestLine(g2).isNull())
        self.assertTrue(g2.shortestLine(g1).isNull())

        g2 = QgsGeometry.fromWkt('Point( 6 3 )')
        expWkt = 'LineString( 5 3, 6 3 )'
        wkt = g1.shortestLine(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))
        expWkt = 'LineString( 6 3, 5 3 )'
        wkt = g2.shortestLine(g1).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

        g1 = QgsGeometry.fromWkt('Polygon ((1 1, 5 1, 5 5, 1 5, 1 1))')
        g2 = QgsGeometry.fromWkt('Point( 6 3 )')
        expWkt = 'LineString( 5 3, 6 3 )'
        wkt = g1.shortestLine(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

        expWkt = 'LineString( 6 3, 5 3 )'
        wkt = g2.shortestLine(g1).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))
        g2 = QgsGeometry.fromWkt('Point( 2 3 )')
        expWkt = 'LineString( 2 3, 2 3 )'
        wkt = g1.shortestLine(g2).exportToWkt()
        self.assertTrue(compareWkt(expWkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt))

    def testBoundingBox(self):
        # 2-+-+-+-+-3
        # |         |
        # + 6-+-+-7 +
        # | |     | |
        # + + 9-+-8 +
        # | |       |
        # ! 5-+-+-+-4 !
        # |
        # 1-+-+-+-+-0 !
        points = [QgsPointXY(5, 0), QgsPointXY(0, 0), QgsPointXY(0, 4), QgsPointXY(5, 4), QgsPointXY(5, 1), QgsPointXY(1, 1), QgsPointXY(1, 3), QgsPointXY(4, 3), QgsPointXY(4, 2), QgsPointXY(2, 2)]
        polyline = QgsGeometry.fromPolyline(points)
        expbb = QgsRectangle(0, 0, 5, 4)
        bb = polyline.boundingBox()
        self.assertEqual(expbb, bb, "Expected:\n%s\nGot:\n%s\n" % (expbb.toString(), bb.toString()))

        #   2-3 6-+-7
        #   | | |   |
        # 0-1 4 5   8-9
        points = [
            [QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 0), ],
            [QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(5, 1), QgsPointXY(5, 0), QgsPointXY(6, 0), ]
        ]
        polyline = QgsGeometry.fromMultiPolyline(points)
        expbb = QgsRectangle(0, 0, 6, 1)
        bb = polyline.boundingBox()
        self.assertEqual(expbb, bb, "Expected:\n%s\nGot:\n%s\n" % (expbb.toString(), bb.toString()))

        # 5---4
        # |   |
        # | 2-3
        # | |
        # 0-1
        points = [[
            QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0),
        ]]
        polygon = QgsGeometry.fromPolygon(points)
        expbb = QgsRectangle(0, 0, 2, 2)
        bb = polygon.boundingBox()
        self.assertEqual(expbb, bb, "Expected:\n%s\nGot:\n%s\n" % (expbb.toString(), bb.toString()))

        # 3-+-+-2
        # |     |
        # + 8-7 +
        # | | | |
        # + 5-6 +
        # |     |
        # 0-+-+-1
        points = [
            [QgsPointXY(0, 0), QgsPointXY(3, 0), QgsPointXY(3, 3), QgsPointXY(0, 3), QgsPointXY(0, 0)],
            [QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)],
        ]
        polygon = QgsGeometry.fromPolygon(points)
        expbb = QgsRectangle(0, 0, 3, 3)
        bb = polygon.boundingBox()
        self.assertEqual(expbb, bb, "Expected:\n%s\nGot:\n%s\n" % (expbb.toString(), bb.toString()))

        # 5-+-4 0-+-9
        # |   | |   |
        # | 2-3 1-2 |
        # | |     | |
        # 0-1     7-8
        points = [
            [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0), ]],
            [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0), ]]
        ]

        polygon = QgsGeometry.fromMultiPolygon(points)
        expbb = QgsRectangle(0, 0, 5, 2)
        bb = polygon.boundingBox()
        self.assertEqual(expbb, bb, "Expected:\n%s\nGot:\n%s\n" % (expbb.toString(), bb.toString()))

        # NULL
        points = []
        line = QgsGeometry.fromPolyline(points)
        assert line.boundingBox().isNull()

    def testCollectGeometry(self):
        # collect points
        geometries = [QgsGeometry.fromPoint(QgsPointXY(0, 0)), QgsGeometry.fromPoint(QgsPointXY(1, 1))]
        geometry = QgsGeometry.collectGeometry(geometries)
        expwkt = "MultiPoint ((0 0), (1 1))"
        wkt = geometry.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # collect lines
        points = [
            [QgsPointXY(0, 0), QgsPointXY(1, 0)],
            [QgsPointXY(2, 0), QgsPointXY(3, 0)]
        ]
        geometries = [QgsGeometry.fromPolyline(points[0]), QgsGeometry.fromPolyline(points[1])]
        geometry = QgsGeometry.collectGeometry(geometries)
        expwkt = "MultiLineString ((0 0, 1 0), (2 0, 3 0))"
        wkt = geometry.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # collect polygons
        points = [
            [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(0, 1), QgsPointXY(0, 0)]],
            [[QgsPointXY(2, 0), QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(2, 1), QgsPointXY(2, 0)]]
        ]
        geometries = [QgsGeometry.fromPolygon(points[0]), QgsGeometry.fromPolygon(points[1])]
        geometry = QgsGeometry.collectGeometry(geometries)
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 0 1, 0 0)),((2 0, 3 0, 3 1, 2 1, 2 0)))"
        wkt = geometry.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # test empty list
        geometries = []
        geometry = QgsGeometry.collectGeometry(geometries)
        assert geometry.isNull(), "Expected geometry to be empty"

        # check that the resulting geometry is multi
        geometry = QgsGeometry.collectGeometry([QgsGeometry.fromWkt('Point (0 0)')])
        assert geometry.isMultipart(), "Expected collected geometry to be multipart"

    def testAddPart(self):
        # add a part to a multipoint
        points = [QgsPointXY(0, 0), QgsPointXY(1, 0)]

        point = QgsGeometry.fromPoint(points[0])
        self.assertEqual(point.addPoints([points[1]]), 0)
        expwkt = "MultiPoint ((0 0), (1 0))"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # test adding a part with Z values
        point = QgsGeometry.fromPoint(points[0])
        point.geometry().addZValue(4.0)
        self.assertEqual(point.addPointsV2([QgsPoint(points[1][0], points[1][1], 3.0, wkbType=QgsWkbTypes.PointZ)]), 0)
        expwkt = "MultiPointZ ((0 0 4), (1 0 3))"
        wkt = point.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        #   2-3 6-+-7
        #   | | |   |
        # 0-1 4 5   8-9
        points = [
            [QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 0), ],
            [QgsPointXY(3, 0), QgsPointXY(3, 1), QgsPointXY(5, 1), QgsPointXY(5, 0), QgsPointXY(6, 0), ]
        ]

        polyline = QgsGeometry.fromPolyline(points[0])
        self.assertEqual(polyline.addPoints(points[1][0:1]), QgsGeometry.InvalidInput, "addPoints with one point line unexpectedly succeeded.")
        self.assertEqual(polyline.addPoints(points[1][0:2]), QgsGeometry.Success, "addPoints with two point line failed.")
        expwkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 0), (3 0, 3 1))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        polyline = QgsGeometry.fromPolyline(points[0])
        self.assertEqual(polyline.addPoints(points[1]), QgsGeometry.Success, "addPoints with %d point line failed." % len(points[1]))
        expwkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 0), (3 0, 3 1, 5 1, 5 0, 6 0))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # test adding a part with Z values
        polyline = QgsGeometry.fromPolyline(points[0])
        polyline.geometry().addZValue(4.0)
        points2 = [QgsPoint(p[0], p[1], 3.0, wkbType=QgsWkbTypes.PointZ) for p in points[1]]
        self.assertEqual(polyline.addPointsV2(points2), QgsGeometry.Success)
        expwkt = "MultiLineStringZ ((0 0 4, 1 0 4, 1 1 4, 2 1 4, 2 0 4),(3 0 3, 3 1 3, 5 1 3, 5 0 3, 6 0 3))"
        wkt = polyline.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # 5-+-4 0-+-9
        # |   | |   |
        # | 2-3 1-2 |
        # | |     | |
        # 0-1     7-8
        points = [
            [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0), ]],
            [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0), ]]
        ]

        polygon = QgsGeometry.fromPolygon(points[0])

        self.assertEqual(polygon.addPoints(points[1][0][0:1]), QgsGeometry.InvalidInput, "addPoints with one point ring unexpectedly succeeded.")
        self.assertEqual(polygon.addPoints(points[1][0][0:2]), QgsGeometry.InvalidInput, "addPoints with two point ring unexpectedly succeeded.")
        self.assertEqual(polygon.addPoints(points[1][0][0:3]), QgsGeometry.InvalidInput, "addPoints with unclosed three point ring unexpectedly succeeded.")
        self.assertEqual(polygon.addPoints([QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(4, 0)]), QgsGeometry.InvalidInput, "addPoints with 'closed' three point ring unexpectedly succeeded.")

        self.assertEqual(polygon.addPoints(points[1][0]), QgsGeometry.Success, "addPoints failed")
        expwkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        mp = QgsGeometry.fromMultiPolygon(points[:1])
        p = QgsGeometry.fromPolygon(points[1])

        self.assertEqual(mp.addPartGeometry(p), QgsGeometry.Success)
        wkt = mp.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        mp = QgsGeometry.fromMultiPolygon(points[:1])
        mp2 = QgsGeometry.fromMultiPolygon(points[1:])
        self.assertEqual(mp.addPartGeometry(mp2), QgsGeometry.Success)
        wkt = mp.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # test adding a part with Z values
        polygon = QgsGeometry.fromPolygon(points[0])
        polygon.geometry().addZValue(4.0)
        points2 = [QgsPoint(pi[0], pi[1], 3.0, wkbType=QgsWkbTypes.PointZ) for pi in points[1][0]]
        self.assertEqual(polygon.addPointsV2(points2), QgsGeometry.Success)
        expwkt = "MultiPolygonZ (((0 0 4, 1 0 4, 1 1 4, 2 1 4, 2 2 4, 0 2 4, 0 0 4)),((4 0 3, 5 0 3, 5 2 3, 3 2 3, 3 1 3, 4 1 3, 4 0 3)))"
        wkt = polygon.exportToWkt()
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

        # Test adding parts to empty geometry, should become first part
        empty = QgsGeometry()
        # if not default type specified, addPart should fail
        result = empty.addPoints([QgsPointXY(4, 0)])
        assert result != QgsGeometry.Success, 'Got return code {}'.format(result)
        result = empty.addPoints([QgsPointXY(4, 0)], QgsWkbTypes.PointGeometry)
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiPoint ((4 0))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)
        result = empty.addPoints([QgsPointXY(5, 1)])
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiPoint ((4 0),(5 1))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)
        # next try with lines
        empty = QgsGeometry()
        result = empty.addPoints(points[0][0], QgsWkbTypes.LineGeometry)
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)
        result = empty.addPoints(points[1][0])
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0),(4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)
        # finally try with polygons
        empty = QgsGeometry()
        result = empty.addPoints(points[0][0], QgsWkbTypes.PolygonGeometry)
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)
        result = empty.addPoints(points[1][0])
        self.assertEqual(result, QgsGeometry.Success, 'Got return code {}'.format(result))
        wkt = empty.exportToWkt()
        expwkt = 'MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))'
        assert compareWkt(expwkt, wkt), "Expected:\n%s\nGot:\n%s\n" % (expwkt, wkt)

    def testConvertToType(self):
        # 5-+-4 0-+-9  13-+-+-12
        # |   | |   |  |       |
        # | 2-3 1-2 |  + 18-17 +
        # | |     | |  | |   | |
        # 0-1     7-8  + 15-16 +
        #              |       |
        #              10-+-+-11
        points = [
            [[QgsPointXY(0, 0), QgsPointXY(1, 0), QgsPointXY(1, 1), QgsPointXY(2, 1), QgsPointXY(2, 2), QgsPointXY(0, 2), QgsPointXY(0, 0)], ],
            [[QgsPointXY(4, 0), QgsPointXY(5, 0), QgsPointXY(5, 2), QgsPointXY(3, 2), QgsPointXY(3, 1), QgsPointXY(4, 1), QgsPointXY(4, 0)], ],
            [[QgsPointXY(10, 0), QgsPointXY(13, 0), QgsPointXY(13, 3), QgsPointXY(10, 3), QgsPointXY(10, 0)], [QgsPointXY(11, 1), QgsPointXY(12, 1), QgsPointXY(12, 2), QgsPointXY(11, 2), QgsPointXY(11, 1)]]
        ]
        # ####### TO POINT ########
        # POINT TO POINT
        point = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        wkt = point.convertToType(QgsWkbTypes.PointGeometry, False).exportToWkt()
        expWkt = "Point (1 1)"
        assert compareWkt(expWkt, wkt), "convertToType failed: from point to point. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # POINT TO MultiPoint
        wkt = point.convertToType(QgsWkbTypes.PointGeometry, True).exportToWkt()
        expWkt = "MultiPoint ((1 1))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from point to multipoint. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # LINE TO MultiPoint
        line = QgsGeometry.fromPolyline(points[0][0])
        wkt = line.convertToType(QgsWkbTypes.PointGeometry, True).exportToWkt()
        expWkt = "MultiPoint ((0 0),(1 0),(1 1),(2 1),(2 2),(0 2),(0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from line to multipoint. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MULTILINE TO MultiPoint
        multiLine = QgsGeometry.fromMultiPolyline(points[2])
        wkt = multiLine.convertToType(QgsWkbTypes.PointGeometry, True).exportToWkt()
        expWkt = "MultiPoint ((10 0),(13 0),(13 3),(10 3),(10 0),(11 1),(12 1),(12 2),(11 2),(11 1))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multiline to multipoint. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # Polygon TO MultiPoint
        polygon = QgsGeometry.fromPolygon(points[0])
        wkt = polygon.convertToType(QgsWkbTypes.PointGeometry, True).exportToWkt()
        expWkt = "MultiPoint ((0 0),(1 0),(1 1),(2 1),(2 2),(0 2),(0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from poylgon to multipoint. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPolygon TO MultiPoint
        multiPolygon = QgsGeometry.fromMultiPolygon(points)
        wkt = multiPolygon.convertToType(QgsWkbTypes.PointGeometry, True).exportToWkt()
        expWkt = "MultiPoint ((0 0),(1 0),(1 1),(2 1),(2 2),(0 2),(0 0),(4 0),(5 0),(5 2),(3 2),(3 1),(4 1),(4 0),(10 0),(13 0),(13 3),(10 3),(10 0),(11 1),(12 1),(12 2),(11 2),(11 1))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipoylgon to multipoint. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # ####### TO LINE ########
        # POINT TO LINE
        point = QgsGeometry.fromPoint(QgsPointXY(1, 1))
        self.assertFalse(point.convertToType(QgsWkbTypes.LineGeometry, False)), "convertToType with a point should return a null geometry"
        # MultiPoint TO LINE
        multipoint = QgsGeometry.fromMultiPoint(points[0][0])
        wkt = multipoint.convertToType(QgsWkbTypes.LineGeometry, False).exportToWkt()
        expWkt = "LineString (0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipoint to line. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPoint TO MULTILINE
        multipoint = QgsGeometry.fromMultiPoint(points[0][0])
        wkt = multipoint.convertToType(QgsWkbTypes.LineGeometry, True).exportToWkt()
        expWkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipoint to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MULTILINE (which has a single part) TO LINE
        multiLine = QgsGeometry.fromMultiPolyline(points[0])
        wkt = multiLine.convertToType(QgsWkbTypes.LineGeometry, False).exportToWkt()
        expWkt = "LineString (0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multiline to line. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # LINE TO MULTILINE
        line = QgsGeometry.fromPolyline(points[0][0])
        wkt = line.convertToType(QgsWkbTypes.LineGeometry, True).exportToWkt()
        expWkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from line to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # Polygon TO LINE
        polygon = QgsGeometry.fromPolygon(points[0])
        wkt = polygon.convertToType(QgsWkbTypes.LineGeometry, False).exportToWkt()
        expWkt = "LineString (0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)"
        assert compareWkt(expWkt, wkt), "convertToType failed: from polygon to line. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # Polygon TO MULTILINE
        polygon = QgsGeometry.fromPolygon(points[0])
        wkt = polygon.convertToType(QgsWkbTypes.LineGeometry, True).exportToWkt()
        expWkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from polygon to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # Polygon with ring TO MULTILINE
        polygon = QgsGeometry.fromPolygon(points[2])
        wkt = polygon.convertToType(QgsWkbTypes.LineGeometry, True).exportToWkt()
        expWkt = "MultiLineString ((10 0, 13 0, 13 3, 10 3, 10 0), (11 1, 12 1, 12 2, 11 2, 11 1))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from polygon with ring to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPolygon (which has a single part) TO LINE
        multiPolygon = QgsGeometry.fromMultiPolygon([points[0]])
        wkt = multiPolygon.convertToType(QgsWkbTypes.LineGeometry, False).exportToWkt()
        expWkt = "LineString (0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipolygon to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPolygon TO MULTILINE
        multiPolygon = QgsGeometry.fromMultiPolygon(points)
        wkt = multiPolygon.convertToType(QgsWkbTypes.LineGeometry, True).exportToWkt()
        expWkt = "MultiLineString ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0), (4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0), (10 0, 13 0, 13 3, 10 3, 10 0), (11 1, 12 1, 12 2, 11 2, 11 1))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipolygon to multiline. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # ####### TO Polygon ########
        # MultiPoint TO Polygon
        multipoint = QgsGeometry.fromMultiPoint(points[0][0])
        wkt = multipoint.convertToType(QgsWkbTypes.PolygonGeometry, False).exportToWkt()
        expWkt = "Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipoint to polygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPoint TO MultiPolygon
        multipoint = QgsGeometry.fromMultiPoint(points[0][0])
        wkt = multipoint.convertToType(QgsWkbTypes.PolygonGeometry, True).exportToWkt()
        expWkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multipoint to multipolygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # LINE TO Polygon
        line = QgsGeometry.fromPolyline(points[0][0])
        wkt = line.convertToType(QgsWkbTypes.PolygonGeometry, False).exportToWkt()
        expWkt = "Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from line to polygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # LINE ( 3 vertices, with first = last ) TO Polygon
        line = QgsGeometry.fromPolyline([QgsPointXY(1, 1), QgsPointXY(0, 0), QgsPointXY(1, 1)])
        self.assertFalse(line.convertToType(QgsWkbTypes.PolygonGeometry, False), "convertToType to polygon of a 3 vertices lines with first and last vertex identical should return a null geometry")
        # MULTILINE ( with a part of 3 vertices, with first = last ) TO MultiPolygon
        multiline = QgsGeometry.fromMultiPolyline([points[0][0], [QgsPointXY(1, 1), QgsPointXY(0, 0), QgsPointXY(1, 1)]])
        self.assertFalse(multiline.convertToType(QgsWkbTypes.PolygonGeometry, True), "convertToType to polygon of a 3 vertices lines with first and last vertex identical should return a null geometry")
        # LINE TO MultiPolygon
        line = QgsGeometry.fromPolyline(points[0][0])
        wkt = line.convertToType(QgsWkbTypes.PolygonGeometry, True).exportToWkt()
        expWkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from line to multipolygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MULTILINE (which has a single part) TO Polygon
        multiLine = QgsGeometry.fromMultiPolyline(points[0])
        wkt = multiLine.convertToType(QgsWkbTypes.PolygonGeometry, False).exportToWkt()
        expWkt = "Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multiline to polygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MULTILINE TO MultiPolygon
        multiLine = QgsGeometry.fromMultiPolyline([points[0][0], points[1][0]])
        wkt = multiLine.convertToType(QgsWkbTypes.PolygonGeometry, True).exportToWkt()
        expWkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)),((4 0, 5 0, 5 2, 3 2, 3 1, 4 1, 4 0)))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multiline to multipolygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # Polygon TO MultiPolygon
        polygon = QgsGeometry.fromPolygon(points[0])
        wkt = polygon.convertToType(QgsWkbTypes.PolygonGeometry, True).exportToWkt()
        expWkt = "MultiPolygon (((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0)))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from polygon to multipolygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # MultiPolygon (which has a single part) TO Polygon
        multiPolygon = QgsGeometry.fromMultiPolygon([points[0]])
        wkt = multiPolygon.convertToType(QgsWkbTypes.PolygonGeometry, False).exportToWkt()
        expWkt = "Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))"
        assert compareWkt(expWkt, wkt), "convertToType failed: from multiline to polygon. Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testRegression13053(self):
        """ See https://issues.qgis.org/issues/13053 """
        p = QgsGeometry.fromWkt('MULTIPOLYGON(((62.0 18.0, 62.0 19.0, 63.0 19.0, 63.0 18.0, 62.0 18.0)), ((63.0 19.0, 63.0 20.0, 64.0 20.0, 64.0 19.0, 63.0 19.0)))')
        assert p is not None

        expWkt = 'MultiPolygon (((62 18, 62 19, 63 19, 63 18, 62 18)),((63 19, 63 20, 64 20, 64 19, 63 19)))'
        wkt = p.exportToWkt()
        assert compareWkt(expWkt, wkt), "testRegression13053 failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testRegression13055(self):
        """ See https://issues.qgis.org/issues/13055
            Testing that invalid WKT with z values but not using PolygonZ is still parsed
            by QGIS.
        """
        p = QgsGeometry.fromWkt('Polygon((0 0 0, 0 1 0, 1 1 0, 0 0 0 ))')
        assert p is not None

        expWkt = 'PolygonZ ((0 0 0, 0 1 0, 1 1 0, 0 0 0 ))'
        wkt = p.exportToWkt()
        assert compareWkt(expWkt, wkt), "testRegression13055 failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testRegression13274(self):
        """ See https://issues.qgis.org/issues/13274
            Testing that two combined linestrings produce another line string if possible
        """
        a = QgsGeometry.fromWkt('LineString (0 0, 1 0)')
        b = QgsGeometry.fromWkt('LineString (1 0, 2 0)')
        c = a.combine(b)

        expWkt = 'LineString (0 0, 1 0, 2 0)'
        wkt = c.exportToWkt()
        assert compareWkt(expWkt, wkt), "testRegression13274 failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testReshape(self):
        """ Test geometry reshaping """
        g = QgsGeometry.fromWkt('Polygon ((0 0, 1 0, 1 1, 0 1, 0 0))')
        g.reshapeGeometry(QgsLineString([QgsPoint(0, 1.5), QgsPoint(1.5, 0)]))
        expWkt = 'Polygon ((0.5 1, 0 1, 0 0, 1 0, 1 0.5, 0.5 1))'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # Test reshape a geometry involving the first/last vertex (https://issues.qgis.org/issues/14443)
        g.reshapeGeometry(QgsLineString([QgsPoint(0.5, 1), QgsPoint(0, 0.5)]))

        expWkt = 'Polygon ((0 0.5, 0 0, 1 0, 1 0.5, 0.5 1, 0 0.5))'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # Test reshape a line from first/last vertex
        g = QgsGeometry.fromWkt('LineString (0 0, 5 0, 5 1)')
        # extend start
        self.assertEqual(g.reshapeGeometry(QgsLineString([QgsPoint(0, 0), QgsPoint(-1, 0)])), 0)
        expWkt = 'LineString (-1 0, 0 0, 5 0, 5 1)'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # extend end
        self.assertEqual(g.reshapeGeometry(QgsLineString([QgsPoint(5, 1), QgsPoint(10, 1), QgsPoint(10, 2)])), 0)
        expWkt = 'LineString (-1 0, 0 0, 5 0, 5 1, 10 1, 10 2)'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test with reversed lines
        g = QgsGeometry.fromWkt('LineString (0 0, 5 0, 5 1)')
        # extend start
        self.assertEqual(g.reshapeGeometry(QgsLineString([QgsPoint(-1, 0), QgsPoint(0, 0)])), 0)
        expWkt = 'LineString (-1 0, 0 0, 5 0, 5 1)'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # extend end
        self.assertEqual(g.reshapeGeometry(QgsLineString([QgsPoint(10, 2), QgsPoint(10, 1), QgsPoint(5, 1)])), 0)
        expWkt = 'LineString (-1 0, 0 0, 5 0, 5 1, 10 1, 10 2)'
        wkt = g.exportToWkt()
        assert compareWkt(expWkt, wkt), "testReshape failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testConvertToMultiType(self):
        """ Test converting geometries to multi type """
        point = QgsGeometry.fromWkt('Point (1 2)')
        assert point.convertToMultiType()
        expWkt = 'MultiPoint ((1 2))'
        wkt = point.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of MultiPoint
        assert point.convertToMultiType()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        line = QgsGeometry.fromWkt('LineString (1 0, 2 0)')
        assert line.convertToMultiType()
        expWkt = 'MultiLineString ((1 0, 2 0))'
        wkt = line.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of MultiLineString
        assert line.convertToMultiType()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        poly = QgsGeometry.fromWkt('Polygon ((1 0, 2 0, 2 1, 1 1, 1 0))')
        assert poly.convertToMultiType()
        expWkt = 'MultiPolygon (((1 0, 2 0, 2 1, 1 1, 1 0)))'
        wkt = poly.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of MultiPolygon
        assert poly.convertToMultiType()
        assert compareWkt(expWkt, wkt), "testConvertToMultiType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testConvertToSingleType(self):
        """ Test converting geometries to single type """
        point = QgsGeometry.fromWkt('MultiPoint ((1 2),(2 3))')
        assert point.convertToSingleType()
        expWkt = 'Point (1 2)'
        wkt = point.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of Point
        assert point.convertToSingleType()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        line = QgsGeometry.fromWkt('MultiLineString ((1 0, 2 0),(2 3, 4 5))')
        assert line.convertToSingleType()
        expWkt = 'LineString (1 0, 2 0)'
        wkt = line.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of LineString
        assert line.convertToSingleType()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        poly = QgsGeometry.fromWkt('MultiPolygon (((1 0, 2 0, 2 1, 1 1, 1 0)),((2 3,2 4, 3 4, 3 3, 2 3)))')
        assert poly.convertToSingleType()
        expWkt = 'Polygon ((1 0, 2 0, 2 1, 1 1, 1 0))'
        wkt = poly.exportToWkt()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)
        # test conversion of Polygon
        assert poly.convertToSingleType()
        assert compareWkt(expWkt, wkt), "testConvertToSingleType failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testAddZValue(self):
        """ Test adding z dimension to geometries """

        # circular string
        geom = QgsGeometry.fromWkt('CircularString (1 5, 6 2, 7 3)')
        assert geom.geometry().addZValue(2)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.CircularStringZ)
        expWkt = 'CircularStringZ (1 5 2, 6 2 2, 7 3 2)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to CircularString failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # compound curve
        geom = QgsGeometry.fromWkt('CompoundCurve ((5 3, 5 13),CircularString (5 13, 7 15, 9 13),(9 13, 9 3),CircularString (9 3, 7 1, 5 3))')
        assert geom.geometry().addZValue(2)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.CompoundCurveZ)
        expWkt = 'CompoundCurveZ ((5 3 2, 5 13 2),CircularStringZ (5 13 2, 7 15 2, 9 13 2),(9 13 2, 9 3 2),CircularStringZ (9 3 2, 7 1 2, 5 3 2))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to CompoundCurve failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # curve polygon
        geom = QgsGeometry.fromWkt('Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))')
        assert geom.geometry().addZValue(3)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.PolygonZ)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.PolygonZ)
        expWkt = 'PolygonZ ((0 0 3, 1 0 3, 1 1 3, 2 1 3, 2 2 3, 0 2 3, 0 0 3))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to CurvePolygon failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # geometry collection
        geom = QgsGeometry.fromWkt('MultiPoint ((1 2),(2 3))')
        assert geom.geometry().addZValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.MultiPointZ)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.MultiPointZ)
        expWkt = 'MultiPointZ ((1 2 4),(2 3 4))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to GeometryCollection failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # LineString
        geom = QgsGeometry.fromWkt('LineString (1 2, 2 3)')
        assert geom.geometry().addZValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.LineStringZ)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.LineStringZ)
        expWkt = 'LineStringZ (1 2 4, 2 3 4)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to LineString failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # Point
        geom = QgsGeometry.fromWkt('Point (1 2)')
        assert geom.geometry().addZValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.PointZ)
        self.assertEqual(geom.wkbType(), QgsWkbTypes.PointZ)
        expWkt = 'PointZ (1 2 4)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addZValue to Point failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testAddMValue(self):
        """ Test adding m dimension to geometries """

        # circular string
        geom = QgsGeometry.fromWkt('CircularString (1 5, 6 2, 7 3)')
        assert geom.geometry().addMValue(2)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.CircularStringM)
        expWkt = 'CircularStringM (1 5 2, 6 2 2, 7 3 2)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to CircularString failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # compound curve
        geom = QgsGeometry.fromWkt('CompoundCurve ((5 3, 5 13),CircularString (5 13, 7 15, 9 13),(9 13, 9 3),CircularString (9 3, 7 1, 5 3))')
        assert geom.geometry().addMValue(2)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.CompoundCurveM)
        expWkt = 'CompoundCurveM ((5 3 2, 5 13 2),CircularStringM (5 13 2, 7 15 2, 9 13 2),(9 13 2, 9 3 2),CircularStringM (9 3 2, 7 1 2, 5 3 2))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to CompoundCurve failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # curve polygon
        geom = QgsGeometry.fromWkt('Polygon ((0 0, 1 0, 1 1, 2 1, 2 2, 0 2, 0 0))')
        assert geom.geometry().addMValue(3)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.PolygonM)
        expWkt = 'PolygonM ((0 0 3, 1 0 3, 1 1 3, 2 1 3, 2 2 3, 0 2 3, 0 0 3))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to CurvePolygon failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # geometry collection
        geom = QgsGeometry.fromWkt('MultiPoint ((1 2),(2 3))')
        assert geom.geometry().addMValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.MultiPointM)
        expWkt = 'MultiPointM ((1 2 4),(2 3 4))'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to GeometryCollection failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # LineString
        geom = QgsGeometry.fromWkt('LineString (1 2, 2 3)')
        assert geom.geometry().addMValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.LineStringM)
        expWkt = 'LineStringM (1 2 4, 2 3 4)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to LineString failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

        # Point
        geom = QgsGeometry.fromWkt('Point (1 2)')
        assert geom.geometry().addMValue(4)
        self.assertEqual(geom.geometry().wkbType(), QgsWkbTypes.PointM)
        expWkt = 'PointM (1 2 4)'
        wkt = geom.exportToWkt()
        assert compareWkt(expWkt, wkt), "addMValue to Point failed: mismatch Expected:\n%s\nGot:\n%s\n" % (expWkt, wkt)

    def testDistanceToVertex(self):
        """ Test distanceToVertex calculation """
        g = QgsGeometry()
        self.assertEqual(g.distanceToVertex(0), -1)

        g = QgsGeometry.fromWkt('LineString ()')
        self.assertEqual(g.distanceToVertex(0), -1)

        g = QgsGeometry.fromWkt('Polygon ((0 0, 1 0, 1 1, 0 1, 0 0))')
        self.assertEqual(g.distanceToVertex(0), 0)
        self.assertEqual(g.distanceToVertex(1), 1)
        self.assertEqual(g.distanceToVertex(2), 2)
        self.assertEqual(g.distanceToVertex(3), 3)
        self.assertEqual(g.distanceToVertex(4), 4)
        self.assertEqual(g.distanceToVertex(5), -1)

    def testTypeInformation(self):
        """ Test type information """
        types = [
            (QgsCircularString, "CircularString", QgsWkbTypes.CircularString),
            (QgsCompoundCurve, "CompoundCurve", QgsWkbTypes.CompoundCurve),
            (QgsCurvePolygon, "CurvePolygon", QgsWkbTypes.CurvePolygon),
            (QgsGeometryCollection, "GeometryCollection", QgsWkbTypes.GeometryCollection),
            (QgsLineString, "LineString", QgsWkbTypes.LineString),
            (QgsMultiCurve, "MultiCurve", QgsWkbTypes.MultiCurve),
            (QgsMultiLineString, "MultiLineString", QgsWkbTypes.MultiLineString),
            (QgsMultiPointV2, "MultiPoint", QgsWkbTypes.MultiPoint),
            (QgsMultiPolygonV2, "MultiPolygon", QgsWkbTypes.MultiPolygon),
            (QgsMultiSurface, "MultiSurface", QgsWkbTypes.MultiSurface),
            (QgsPoint, "Point", QgsWkbTypes.Point),
            (QgsPolygonV2, "Polygon", QgsWkbTypes.Polygon),
        ]

        for geomtype in types:
            geom = geomtype[0]()
            self.assertEqual(geom.geometryType(), geomtype[1])
            self.assertEqual(geom.wkbType(), geomtype[2])
            geom.clear()
            self.assertEqual(geom.geometryType(), geomtype[1])
            self.assertEqual(geom.wkbType(), geomtype[2])
            clone = geom.clone()
            self.assertEqual(clone.geometryType(), geomtype[1])
            self.assertEqual(clone.wkbType(), geomtype[2])

    def testRelates(self):
        """ Test relationships between geometries. Note the bulk of these tests were taken from the PostGIS relate testdata """
        with open(os.path.join(TEST_DATA_DIR, 'relates_data.csv'), 'r') as d:
            for i, t in enumerate(d):
                test_data = t.strip().split('|')
                geom1 = QgsGeometry.fromWkt(test_data[0])
                assert geom1, "Relates {} failed: could not create geom:\n{}\n".format(i + 1, test_data[0])
                geom2 = QgsGeometry.fromWkt(test_data[1])
                assert geom2, "Relates {} failed: could not create geom:\n{}\n".format(i + 1, test_data[1])
                result = QgsGeometry.createGeometryEngine(geom1.geometry()).relate(geom2.geometry())
                exp = test_data[2]
                self.assertEqual(result, exp, "Relates {} failed: mismatch Expected:\n{}\nGot:\n{}\nGeom1:\n{}\nGeom2:\n{}\n".format(i + 1, exp, result, test_data[0], test_data[1]))

    def testWkbTypes(self):
        """ Test QgsWkbTypes methods """

        # test singleType method
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.Point), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PointZ), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PointM), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PointZM), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPoint), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPointZ), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPointM), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPointZM), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.LineString), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.LineStringM), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiLineString), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.Polygon), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PolygonZ), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PolygonM), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPolygon), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.GeometryCollection), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CircularString), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularStringZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiCurve), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiCurveM), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiSurface), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.Point25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.singleType(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.Polygon25D)

        # test multiType method
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.Point), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PointZ), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PointM), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PointZM), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.LineString), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.LineStringZ), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.LineStringM), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.LineStringZM), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.Polygon), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PolygonZ), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PolygonM), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.PolygonZM), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollectionZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CircularString), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CircularStringZ), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CircularStringM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CircularStringZM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CompoundCurve), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CurvePolygon), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.Point25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.LineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.Polygon25D), QgsWkbTypes.MultiPolygon25D)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.multiType(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon25D)

        # test flatType method
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.Point), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PointZ), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PointM), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PointZM), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.LineString), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.LineStringM), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.Polygon), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PolygonZ), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PolygonM), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.PolygonZM), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CircularString), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.Point25D), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.flatType(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon)

        # test geometryType method
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.Unknown), QgsWkbTypes.UnknownGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.Point), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PointZ), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PointM), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PointZM), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPoint), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPointZ), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPointM), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPointZM), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.LineString), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.LineStringM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiLineString), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.Polygon), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PolygonZ), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PolygonM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPolygon), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.GeometryCollection), QgsWkbTypes.UnknownGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.UnknownGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.UnknownGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.UnknownGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CircularString), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CircularStringZ), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CircularStringM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CircularStringZM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CompoundCurve), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CurvePolygon), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiCurve), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiCurveM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiSurface), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.NoGeometry), QgsWkbTypes.NullGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.Point25D), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.LineString25D), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.Polygon25D), QgsWkbTypes.PolygonGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.PointGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.LineGeometry)
        self.assertEqual(QgsWkbTypes.geometryType(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.PolygonGeometry)

        # test displayString method
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.Unknown), 'Unknown')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.Point), 'Point')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PointZ), 'PointZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PointM), 'PointM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PointZM), 'PointZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPoint), 'MultiPoint')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPointZ), 'MultiPointZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPointM), 'MultiPointM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPointZM), 'MultiPointZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.LineString), 'LineString')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.LineStringZ), 'LineStringZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.LineStringM), 'LineStringM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.LineStringZM), 'LineStringZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiLineString), 'MultiLineString')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiLineStringZ), 'MultiLineStringZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiLineStringM), 'MultiLineStringM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiLineStringZM), 'MultiLineStringZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.Polygon), 'Polygon')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PolygonZ), 'PolygonZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PolygonM), 'PolygonM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.PolygonZM), 'PolygonZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPolygon), 'MultiPolygon')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPolygonZ), 'MultiPolygonZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPolygonM), 'MultiPolygonM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPolygonZM), 'MultiPolygonZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.GeometryCollection), 'GeometryCollection')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.GeometryCollectionZ), 'GeometryCollectionZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.GeometryCollectionM), 'GeometryCollectionM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.GeometryCollectionZM), 'GeometryCollectionZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CircularString), 'CircularString')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CircularStringZ), 'CircularStringZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CircularStringM), 'CircularStringM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CircularStringZM), 'CircularStringZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CompoundCurve), 'CompoundCurve')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CompoundCurveZ), 'CompoundCurveZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CompoundCurveM), 'CompoundCurveM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CompoundCurveZM), 'CompoundCurveZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CurvePolygon), 'CurvePolygon')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CurvePolygonZ), 'CurvePolygonZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CurvePolygonM), 'CurvePolygonM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.CurvePolygonZM), 'CurvePolygonZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiCurve), 'MultiCurve')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiCurveZ), 'MultiCurveZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiCurveM), 'MultiCurveM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiCurveZM), 'MultiCurveZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiSurface), 'MultiSurface')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiSurfaceZ), 'MultiSurfaceZ')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiSurfaceM), 'MultiSurfaceM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiSurfaceZM), 'MultiSurfaceZM')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.NoGeometry), 'NoGeometry')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.Point25D), 'Point25D')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.LineString25D), 'LineString25D')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.Polygon25D), 'Polygon25D')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPoint25D), 'MultiPoint25D')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiLineString25D), 'MultiLineString25D')
        self.assertEqual(QgsWkbTypes.displayString(QgsWkbTypes.MultiPolygon25D), 'MultiPolygon25D')

        # test parseType method
        self.assertEqual(QgsWkbTypes.parseType('point( 1 2 )'), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.parseType('POINT( 1 2 )'), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.parseType('   point    ( 1 2 )   '), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.parseType('point'), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.parseType('LINE STRING( 1 2, 3 4 )'), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.parseType('POINTZ( 1 2 )'), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.parseType('POINT z m'), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.parseType('bad'), QgsWkbTypes.Unknown)

        # test wkbDimensions method
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.Unknown), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.Point), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PointZ), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PointM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PointZM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPoint), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPointZ), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPointM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPointZM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.LineString), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.LineStringZ), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.LineStringM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.LineStringZM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiLineString), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiLineStringZ), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiLineStringM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiLineStringZM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.Polygon), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PolygonZ), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PolygonM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.PolygonZM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPolygon), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPolygonZ), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPolygonM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPolygonZM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.GeometryCollection), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.GeometryCollectionZ), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.GeometryCollectionM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.GeometryCollectionZM), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CircularString), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CircularStringZ), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CircularStringM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CircularStringZM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CompoundCurve), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CompoundCurveZ), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CompoundCurveM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CompoundCurveZM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CurvePolygon), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CurvePolygonZ), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CurvePolygonM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.CurvePolygonZM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiCurve), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiCurveZ), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiCurveM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiCurveZM), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiSurface), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiSurfaceZ), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiSurfaceM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiSurfaceZM), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.NoGeometry), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.Point25D), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.LineString25D), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.Polygon25D), 2)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPoint25D), 0)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiLineString25D), 1)
        self.assertEqual(QgsWkbTypes.wkbDimensions(QgsWkbTypes.MultiPolygon25D), 2)

        # test coordDimensions method
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.Unknown), 0)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.Point), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PointZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PointM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PointZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPoint), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPointZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPointM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPointZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.LineString), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.LineStringZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.LineStringM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.LineStringZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiLineString), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiLineStringZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiLineStringM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiLineStringZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.Polygon), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PolygonZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PolygonM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.PolygonZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPolygon), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPolygonZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPolygonM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPolygonZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.GeometryCollection), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.GeometryCollectionZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.GeometryCollectionM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.GeometryCollectionZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CircularString), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CircularStringZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CircularStringM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CircularStringZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CompoundCurve), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CompoundCurveZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CompoundCurveM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CompoundCurveZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CurvePolygon), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CurvePolygonZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CurvePolygonM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.CurvePolygonZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiCurve), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiCurveZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiCurveM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiCurveZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiSurface), 2)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiSurfaceZ), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiSurfaceM), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiSurfaceZM), 4)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.NoGeometry), 0)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.Point25D), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.LineString25D), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.Polygon25D), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPoint25D), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiLineString25D), 3)
        self.assertEqual(QgsWkbTypes.coordDimensions(QgsWkbTypes.MultiPolygon25D), 3)

        # test isSingleType methods
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.Unknown)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.Point)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.LineString)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.Polygon)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPoint)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiLineString)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPolygon)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.GeometryCollection)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CircularString)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CompoundCurve)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CurvePolygon)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiCurve)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiSurface)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.NoGeometry)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PointZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.LineStringZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PolygonZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPointZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiLineStringZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPolygonZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.GeometryCollectionZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CircularStringZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CompoundCurveZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CurvePolygonZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiCurveZ)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiSurfaceZ)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PointM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.LineStringM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PolygonM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPointM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiLineStringM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPolygonM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.GeometryCollectionM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CircularStringM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CompoundCurveM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CurvePolygonM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiCurveM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiSurfaceM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PointZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.LineStringZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.PolygonZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPointZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiLineStringZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPolygonZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.GeometryCollectionZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CircularStringZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CompoundCurveZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.CurvePolygonZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiCurveZM)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiSurfaceZM)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.Point25D)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.LineString25D)
        assert QgsWkbTypes.isSingleType(QgsWkbTypes.Polygon25D)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPoint25D)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiLineString25D)
        assert not QgsWkbTypes.isSingleType(QgsWkbTypes.MultiPolygon25D)

        # test isMultiType methods
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.Unknown)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.Point)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.LineString)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.Polygon)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPoint)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiLineString)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPolygon)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.GeometryCollection)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CircularString)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CompoundCurve)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CurvePolygon)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiCurve)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiSurface)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.NoGeometry)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PointZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.LineStringZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PolygonZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPointZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiLineStringZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPolygonZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.GeometryCollectionZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CircularStringZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CompoundCurveZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CurvePolygonZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiCurveZ)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiSurfaceZ)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PointM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.LineStringM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PolygonM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPointM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiLineStringM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPolygonM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.GeometryCollectionM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CircularStringM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CompoundCurveM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CurvePolygonM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiCurveM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiSurfaceM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PointZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.LineStringZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.PolygonZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPointZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiLineStringZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPolygonZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.GeometryCollectionZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CircularStringZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CompoundCurveZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.CurvePolygonZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiCurveZM)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiSurfaceZM)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.Point25D)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.LineString25D)
        assert not QgsWkbTypes.isMultiType(QgsWkbTypes.Polygon25D)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPoint25D)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiLineString25D)
        assert QgsWkbTypes.isMultiType(QgsWkbTypes.MultiPolygon25D)

        # test isCurvedType methods
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.Unknown)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.Point)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.LineString)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.Polygon)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPoint)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiLineString)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPolygon)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.GeometryCollection)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CircularString)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CompoundCurve)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CurvePolygon)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiCurve)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiSurface)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.NoGeometry)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PointZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.LineStringZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PolygonZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPointZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiLineStringZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPolygonZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.GeometryCollectionZ)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CircularStringZ)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CompoundCurveZ)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CurvePolygonZ)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiCurveZ)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiSurfaceZ)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PointM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.LineStringM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PolygonM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPointM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiLineStringM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPolygonM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.GeometryCollectionM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CircularStringM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CompoundCurveM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CurvePolygonM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiCurveM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiSurfaceM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PointZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.LineStringZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.PolygonZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPointZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiLineStringZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPolygonZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.GeometryCollectionZM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CircularStringZM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CompoundCurveZM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.CurvePolygonZM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiCurveZM)
        assert QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiSurfaceZM)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.Point25D)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.LineString25D)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.Polygon25D)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPoint25D)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiLineString25D)
        assert not QgsWkbTypes.isCurvedType(QgsWkbTypes.MultiPolygon25D)

        # test hasZ methods
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.Unknown)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.Point)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.LineString)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.Polygon)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiPoint)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiLineString)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiPolygon)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.GeometryCollection)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CircularString)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CompoundCurve)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CurvePolygon)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiCurve)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiSurface)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.NoGeometry)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.PointZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.LineStringZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.PolygonZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPointZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiLineStringZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPolygonZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.GeometryCollectionZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CircularStringZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CompoundCurveZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CurvePolygonZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiCurveZ)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiSurfaceZ)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.PointM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.LineStringM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.PolygonM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiPointM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiLineStringM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiPolygonM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.GeometryCollectionM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CircularStringM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CompoundCurveM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.CurvePolygonM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiCurveM)
        assert not QgsWkbTypes.hasZ(QgsWkbTypes.MultiSurfaceM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.PointZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.LineStringZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.PolygonZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPointZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiLineStringZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPolygonZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.GeometryCollectionZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CircularStringZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CompoundCurveZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.CurvePolygonZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiCurveZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiSurfaceZM)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.Point25D)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.LineString25D)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.Polygon25D)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPoint25D)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiLineString25D)
        assert QgsWkbTypes.hasZ(QgsWkbTypes.MultiPolygon25D)

        # test hasM methods
        assert not QgsWkbTypes.hasM(QgsWkbTypes.Unknown)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.Point)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.LineString)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.Polygon)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPoint)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiLineString)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPolygon)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.GeometryCollection)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CircularString)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CompoundCurve)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CurvePolygon)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiCurve)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiSurface)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.NoGeometry)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.PointZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.LineStringZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.PolygonZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPointZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiLineStringZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPolygonZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.GeometryCollectionZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CircularStringZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CompoundCurveZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.CurvePolygonZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiCurveZ)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiSurfaceZ)
        assert QgsWkbTypes.hasM(QgsWkbTypes.PointM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.LineStringM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.PolygonM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiPointM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiLineStringM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiPolygonM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.GeometryCollectionM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CircularStringM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CompoundCurveM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CurvePolygonM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiCurveM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiSurfaceM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.PointZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.LineStringZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.PolygonZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiPointZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiLineStringZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiPolygonZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.GeometryCollectionZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CircularStringZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CompoundCurveZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.CurvePolygonZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiCurveZM)
        assert QgsWkbTypes.hasM(QgsWkbTypes.MultiSurfaceZM)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.Point25D)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.LineString25D)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.Polygon25D)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPoint25D)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiLineString25D)
        assert not QgsWkbTypes.hasM(QgsWkbTypes.MultiPolygon25D)

        # test adding z dimension to types
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.Point), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PointZ), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PointM), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PointZM), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.LineString), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.LineStringM), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.Polygon), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PolygonZ), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PolygonM), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollectionZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollectionZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CircularString), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularStringZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.Point25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.addZ(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon25D)

        # test to25D
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.Point), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PointZ), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PointM), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PointZM), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.LineString), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.LineStringM), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.Polygon), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PolygonZ), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PolygonM), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.PolygonZM), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.GeometryCollection), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CircularString), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CircularStringZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CircularStringM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CircularStringZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CompoundCurve), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CurvePolygon), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiCurve), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiCurveM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiSurface), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.Point25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.to25D(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon25D)

        # test adding m dimension to types
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.Point), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PointZ), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PointM), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PointZM), QgsWkbTypes.PointZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPointZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.LineString), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.LineStringM), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.Polygon), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PolygonZ), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PolygonM), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollectionZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollectionZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CircularString), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularStringZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurveZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygonZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurveZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurfaceZM)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        # can't be added to these types
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.Point25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.addM(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon25D)

        # test dropping z dimension from types
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.Point), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PointZ), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PointM), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PointZM), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.LineString), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.LineStringM), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.Polygon), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PolygonZ), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PolygonM), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CircularString), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.Point25D), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.dropZ(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon)

        # test dropping m dimension from types
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.Unknown), QgsWkbTypes.Unknown)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.Point), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PointZ), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PointM), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PointZM), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPoint), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPointZ), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPointM), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPointZM), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.LineString), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.LineStringZ), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.LineStringM), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.LineStringZM), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiLineString), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiLineStringZ), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiLineStringM), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiLineStringZM), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.Polygon), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PolygonZ), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PolygonM), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.PolygonZM), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPolygon), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPolygonZ), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPolygonM), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPolygonZM), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.GeometryCollection), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.GeometryCollectionZ), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.GeometryCollectionM), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.GeometryCollectionZM), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CircularString), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CircularStringZ), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CircularStringM), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CircularStringZM), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CompoundCurve), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CompoundCurveZ), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CompoundCurveM), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CompoundCurveZM), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CurvePolygon), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CurvePolygonZ), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CurvePolygonM), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.CurvePolygonZM), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiCurve), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiCurveZ), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiCurveM), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiCurveZM), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiSurface), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiSurfaceZ), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiSurfaceM), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiSurfaceZM), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.NoGeometry), QgsWkbTypes.NoGeometry)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.Point25D), QgsWkbTypes.Point25D)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.LineString25D), QgsWkbTypes.LineString25D)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.Polygon25D), QgsWkbTypes.Polygon25D)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPoint25D), QgsWkbTypes.MultiPoint25D)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiLineString25D), QgsWkbTypes.MultiLineString25D)
        self.assertEqual(QgsWkbTypes.dropM(QgsWkbTypes.MultiPolygon25D), QgsWkbTypes.MultiPolygon25D)

        # Test QgsWkbTypes.zmType
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Point, False, False), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Point, True, False), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Point, False, True), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Point, True, True), QgsWkbTypes.PointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZ, False, False), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZ, True, False), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZ, False, True), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZ, True, True), QgsWkbTypes.PointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointM, False, False), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointM, True, False), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointM, False, True), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointM, True, True), QgsWkbTypes.PointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZM, False, False), QgsWkbTypes.Point)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZM, True, False), QgsWkbTypes.PointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZM, False, True), QgsWkbTypes.PointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PointZM, True, True), QgsWkbTypes.PointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineString, False, False), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineString, True, False), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineString, False, True), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineString, True, True), QgsWkbTypes.LineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZ, False, False), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZ, True, False), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZ, False, True), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZ, True, True), QgsWkbTypes.LineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringM, False, False), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringM, True, False), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringM, False, True), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringM, True, True), QgsWkbTypes.LineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZM, False, False), QgsWkbTypes.LineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZM, True, False), QgsWkbTypes.LineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZM, False, True), QgsWkbTypes.LineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.LineStringZM, True, True), QgsWkbTypes.LineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Polygon, False, False), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Polygon, True, False), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Polygon, False, True), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.Polygon, True, True), QgsWkbTypes.PolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZ, False, False), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZ, True, False), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZ, False, True), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZ, True, True), QgsWkbTypes.PolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonM, False, False), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonM, True, False), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonM, False, True), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonM, True, True), QgsWkbTypes.PolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZM, False, False), QgsWkbTypes.Polygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZM, True, False), QgsWkbTypes.PolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZM, False, True), QgsWkbTypes.PolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.PolygonZM, True, True), QgsWkbTypes.PolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPoint, False, False), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPoint, True, False), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPoint, False, True), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPoint, True, True), QgsWkbTypes.MultiPointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZ, False, False), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZ, True, False), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZ, False, True), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZ, True, True), QgsWkbTypes.MultiPointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointM, False, False), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointM, True, False), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointM, False, True), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointM, True, True), QgsWkbTypes.MultiPointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZM, False, False), QgsWkbTypes.MultiPoint)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZM, True, False), QgsWkbTypes.MultiPointZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZM, False, True), QgsWkbTypes.MultiPointM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPointZM, True, True), QgsWkbTypes.MultiPointZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineString, False, False), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineString, True, False), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineString, False, True), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineString, True, True), QgsWkbTypes.MultiLineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZ, False, False), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZ, True, False), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZ, False, True), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZ, True, True), QgsWkbTypes.MultiLineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringM, False, False), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringM, True, False), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringM, False, True), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringM, True, True), QgsWkbTypes.MultiLineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZM, False, False), QgsWkbTypes.MultiLineString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZM, True, False), QgsWkbTypes.MultiLineStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZM, False, True), QgsWkbTypes.MultiLineStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiLineStringZM, True, True), QgsWkbTypes.MultiLineStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygon, False, False), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygon, True, False), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygon, False, True), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygon, True, True), QgsWkbTypes.MultiPolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZ, False, False), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZ, True, False), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZ, False, True), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZ, True, True), QgsWkbTypes.MultiPolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonM, False, False), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonM, True, False), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonM, False, True), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonM, True, True), QgsWkbTypes.MultiPolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZM, False, False), QgsWkbTypes.MultiPolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZM, True, False), QgsWkbTypes.MultiPolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZM, False, True), QgsWkbTypes.MultiPolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiPolygonZM, True, True), QgsWkbTypes.MultiPolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollection, False, False), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollection, True, False), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollection, False, True), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollection, True, True), QgsWkbTypes.GeometryCollectionZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZ, False, False), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZ, True, False), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZ, False, True), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZ, True, True), QgsWkbTypes.GeometryCollectionZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionM, False, False), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionM, True, False), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionM, False, True), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionM, True, True), QgsWkbTypes.GeometryCollectionZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZM, False, False), QgsWkbTypes.GeometryCollection)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZM, True, False), QgsWkbTypes.GeometryCollectionZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZM, False, True), QgsWkbTypes.GeometryCollectionM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.GeometryCollectionZM, True, True), QgsWkbTypes.GeometryCollectionZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularString, False, False), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularString, True, False), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularString, False, True), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularString, True, True), QgsWkbTypes.CircularStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZ, False, False), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZ, True, False), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZ, False, True), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZ, True, True), QgsWkbTypes.CircularStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringM, False, False), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringM, True, False), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringM, False, True), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringM, True, True), QgsWkbTypes.CircularStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZM, False, False), QgsWkbTypes.CircularString)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZM, True, False), QgsWkbTypes.CircularStringZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZM, False, True), QgsWkbTypes.CircularStringM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CircularStringZM, True, True), QgsWkbTypes.CircularStringZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurve, False, False), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurve, True, False), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurve, False, True), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurve, True, True), QgsWkbTypes.CompoundCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZ, False, False), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZ, True, False), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZ, False, True), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZ, True, True), QgsWkbTypes.CompoundCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveM, False, False), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveM, True, False), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveM, False, True), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveM, True, True), QgsWkbTypes.CompoundCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZM, False, False), QgsWkbTypes.CompoundCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZM, True, False), QgsWkbTypes.CompoundCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZM, False, True), QgsWkbTypes.CompoundCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CompoundCurveZM, True, True), QgsWkbTypes.CompoundCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurve, False, False), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurve, True, False), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurve, False, True), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurve, True, True), QgsWkbTypes.MultiCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZ, False, False), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZ, True, False), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZ, False, True), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZ, True, True), QgsWkbTypes.MultiCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveM, False, False), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveM, True, False), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveM, False, True), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveM, True, True), QgsWkbTypes.MultiCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZM, False, False), QgsWkbTypes.MultiCurve)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZM, True, False), QgsWkbTypes.MultiCurveZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZM, False, True), QgsWkbTypes.MultiCurveM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiCurveZM, True, True), QgsWkbTypes.MultiCurveZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygon, False, False), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygon, True, False), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygon, False, True), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygon, True, True), QgsWkbTypes.CurvePolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZ, False, False), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZ, True, False), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZ, False, True), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZ, True, True), QgsWkbTypes.CurvePolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonM, False, False), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonM, True, False), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonM, False, True), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonM, True, True), QgsWkbTypes.CurvePolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZM, False, False), QgsWkbTypes.CurvePolygon)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZM, True, False), QgsWkbTypes.CurvePolygonZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZM, False, True), QgsWkbTypes.CurvePolygonM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.CurvePolygonZM, True, True), QgsWkbTypes.CurvePolygonZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurface, False, False), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurface, True, False), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurface, False, True), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurface, True, True), QgsWkbTypes.MultiSurfaceZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZ, False, False), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZ, True, False), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZ, False, True), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZ, True, True), QgsWkbTypes.MultiSurfaceZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceM, False, False), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceM, True, False), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceM, False, True), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceM, True, True), QgsWkbTypes.MultiSurfaceZM)

        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZM, False, False), QgsWkbTypes.MultiSurface)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZM, True, False), QgsWkbTypes.MultiSurfaceZ)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZM, False, True), QgsWkbTypes.MultiSurfaceM)
        self.assertEqual(QgsWkbTypes.zmType(QgsWkbTypes.MultiSurfaceZM, True, True), QgsWkbTypes.MultiSurfaceZM)

    def testDeleteVertexCircularString(self):

        wkt = "CircularString ((0 0,1 1,2 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(0)
        self.assertEqual(geom.exportToWkt(), QgsCircularString().asWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(0)
        expected_wkt = "CircularString (2 0, 3 -1, 4 0)"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        expected_wkt = "CircularString (0 0, 3 -1, 4 0)"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(2)
        expected_wkt = "CircularString (0 0, 1 1, 4 0)"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(3)
        expected_wkt = "CircularString (0 0, 1 1, 4 0)"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(4)
        expected_wkt = "CircularString (0 0,1 1,2 0)"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CircularString ((0 0,1 1,2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert not geom.deleteVertex(-1)
        assert not geom.deleteVertex(5)

    def testDeleteVertexCompoundCurve(self):

        wkt = "CompoundCurve ((0 0,1 1))"
        geom = QgsGeometry.fromWkt(wkt)
        assert not geom.deleteVertex(-1)
        assert not geom.deleteVertex(2)
        assert geom.deleteVertex(0)
        self.assertEqual(geom.exportToWkt(), QgsCompoundCurve().asWkt())

        wkt = "CompoundCurve ((0 0,1 1))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        self.assertEqual(geom.exportToWkt(), QgsCompoundCurve().asWkt())

        wkt = "CompoundCurve ((0 0,1 1),(1 1,2 2))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(0)
        expected_wkt = "CompoundCurve ((1 1,2 2))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve ((0 0,1 1),(1 1,2 2))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        expected_wkt = "CompoundCurve ((0 0,2 2))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve ((0 0,1 1),(1 1,2 2))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(2)
        expected_wkt = "CompoundCurve ((0 0,1 1))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve ((0 0,1 1),CircularString(1 1,2 0,1 -1))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        expected_wkt = "CompoundCurve ((0 0,1 -1))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve (CircularString(0 0,1 1,2 0),CircularString(2 0,3 -1,4 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(2)
        expected_wkt = "CompoundCurve ((0 0, 4 0))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(4)
        expected_wkt = "CompoundCurve (CircularString (0 0, 1 1, 2 0),(2 0, 0 0))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve ((-1 0,0 0),CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        expected_wkt = "CompoundCurve ((-1 0, 2 0),CircularString (2 0, 1.5 -0.5, 1 -1))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CompoundCurve (CircularString(-1 -1,-1.5 -0.5,-2 0,-1 1,0 0),CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(4)
        expected_wkt = "CompoundCurve (CircularString (-1 -1, -1.5 -0.5, -2 0),(-2 0, 2 0),CircularString (2 0, 1.5 -0.5, 1 -1))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

    def testDeleteVertexCurvePolygon(self):

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0),(2 0,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert not geom.deleteVertex(-1)
        assert not geom.deleteVertex(4)
        assert geom.deleteVertex(0)
        self.assertEqual(geom.exportToWkt(), QgsCurvePolygon().asWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0),(2 0,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        self.assertEqual(geom.exportToWkt(), QgsCurvePolygon().asWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0),(2 0,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(2)
        self.assertEqual(geom.exportToWkt(), QgsCurvePolygon().asWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0),(2 0,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(3)
        self.assertEqual(geom.exportToWkt(), QgsCurvePolygon().asWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(0)
        expected_wkt = "CurvePolygon (CompoundCurve (CircularString (2 0, 1.5 -0.5, 1 -1),(1 -1, 2 0)))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(1)
        expected_wkt = "CurvePolygon (CompoundCurve (CircularString (0 0, 1.5 -0.5, 1 -1),(1 -1, 0 0)))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(2)
        expected_wkt = "CurvePolygon (CompoundCurve (CircularString (0 0, 1 1, 1 -1),(1 -1, 0 0)))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(3)
        expected_wkt = "CurvePolygon (CompoundCurve (CircularString (0 0, 1 1, 1 -1),(1 -1, 0 0)))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

        wkt = "CurvePolygon (CompoundCurve (CircularString(0 0,1 1,2 0,1.5 -0.5,1 -1),(1 -1,0 0)))"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom.deleteVertex(4)
        expected_wkt = "CurvePolygon (CompoundCurve (CircularString (0 0, 1 1, 2 0),(2 0, 0 0)))"
        self.assertEqual(geom.exportToWkt(), QgsGeometry.fromWkt(expected_wkt).exportToWkt())

    def testSingleSidedBuffer(self):

        wkt = "LineString( 0 0, 10 0)"
        geom = QgsGeometry.fromWkt(wkt)
        out = geom.singleSidedBuffer(1, 8, QgsGeometry.SideLeft)
        result = out.exportToWkt()
        expected_wkt = "Polygon ((10 0, 0 0, 0 1, 10 1, 10 0))"
        self.assertTrue(compareWkt(result, expected_wkt, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(expected_wkt, result))

        wkt = "LineString( 0 0, 10 0)"
        geom = QgsGeometry.fromWkt(wkt)
        out = geom.singleSidedBuffer(1, 8, QgsGeometry.SideRight)
        result = out.exportToWkt()
        expected_wkt = "Polygon ((0 0, 10 0, 10 -1, 0 -1, 0 0))"
        self.assertTrue(compareWkt(result, expected_wkt, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(expected_wkt, result))

        wkt = "LineString( 0 0, 10 0, 10 10)"
        geom = QgsGeometry.fromWkt(wkt)
        out = geom.singleSidedBuffer(1, 8, QgsGeometry.SideRight, QgsGeometry.JoinStyleMiter)
        result = out.exportToWkt()
        expected_wkt = "Polygon ((0 0, 10 0, 10 10, 11 10, 11 -1, 0 -1, 0 0))"
        self.assertTrue(compareWkt(result, expected_wkt, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(expected_wkt, result))

        wkt = "LineString( 0 0, 10 0, 10 10)"
        geom = QgsGeometry.fromWkt(wkt)
        out = geom.singleSidedBuffer(1, 8, QgsGeometry.SideRight, QgsGeometry.JoinStyleBevel)
        result = out.exportToWkt()
        expected_wkt = "Polygon ((0 0, 10 0, 10 10, 11 10, 11 0, 10 -1, 0 -1, 0 0))"
        self.assertTrue(compareWkt(result, expected_wkt, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(expected_wkt, result))

    def testMisc(self):

        # Test that we cannot add a CurvePolygon in a MultiPolygon
        multipolygon = QgsMultiPolygonV2()
        cp = QgsCurvePolygon()
        cp.fromWkt("CurvePolygon ((0 0,0 1,1 1,0 0))")
        assert not multipolygon.addGeometry(cp)

        # Test that importing an invalid WKB (a MultiPolygon with a CurvePolygon) fails
        geom = QgsGeometry.fromWkt('MultiSurface(((0 0,0 1,1 1,0 0)), CurvePolygon ((0 0,0 1,1 1,0 0)))')
        wkb = geom.exportToWkb()
        wkb = bytearray(wkb)
        if wkb[1] == QgsWkbTypes.MultiSurface:
            wkb[1] = QgsWkbTypes.MultiPolygon
        elif wkb[1 + 4] == QgsWkbTypes.MultiSurface:
            wkb[1 + 4] = QgsWkbTypes.MultiPolygon
        else:
            self.assertTrue(False)
        geom = QgsGeometry()
        geom.fromWkb(wkb)
        self.assertEqual(geom.exportToWkt(), QgsMultiPolygonV2().asWkt())

        # Test that fromWkt() on a GeometryCollection works with all possible geometries
        wkt = "GeometryCollection( "
        wkt += "Point(0 1)"
        wkt += ","
        wkt += "LineString(0 0,0 1)"
        wkt += ","
        wkt += "Polygon ((0 0,1 1,1 0,0 0))"
        wkt += ","
        wkt += "CurvePolygon ((0 0,1 1,1 0,0 0))"
        wkt += ","
        wkt += "CircularString (0 0,1 1,2 0)"
        wkt += ","
        wkt += "CompoundCurve ((0 0,0 1))"
        wkt += ","
        wkt += "MultiPoint ((0 0))"
        wkt += ","
        wkt += "MultiLineString((0 0,0 1))"
        wkt += ","
        wkt += "MultiCurve((0 0,0 1))"
        wkt += ","
        wkt += "MultiPolygon (((0 0,1 1,1 0,0 0)))"
        wkt += ","
        wkt += "MultiSurface (((0 0,1 1,1 0,0 0)))"
        wkt += ","
        wkt += "GeometryCollection (Point(0 0))"
        wkt += ")"
        geom = QgsGeometry.fromWkt(wkt)
        assert geom is not None
        wkb1 = geom.exportToWkb()
        geom = QgsGeometry()
        geom.fromWkb(wkb1)
        wkb2 = geom.exportToWkb()
        self.assertEqual(wkb1, wkb2)

    def testMergeLines(self):
        """ test merging linestrings """

        # not a (multi)linestring
        geom = QgsGeometry.fromWkt('Point(1 2)')
        result = geom.mergeLines()
        self.assertTrue(result.isNull())

        # linestring should be returned intact
        geom = QgsGeometry.fromWkt('LineString(0 0, 10 10)')
        result = geom.mergeLines().exportToWkt()
        exp = 'LineString(0 0, 10 10)'
        self.assertTrue(compareWkt(result, exp, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multilinestring
        geom = QgsGeometry.fromWkt('MultiLineString((0 0, 10 10),(10 10, 20 20))')
        result = geom.mergeLines().exportToWkt()
        exp = 'LineString(0 0, 10 10, 20 20)'
        self.assertTrue(compareWkt(result, exp, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        geom = QgsGeometry.fromWkt('MultiLineString((0 0, 10 10),(12 2, 14 4),(10 10, 20 20))')
        result = geom.mergeLines().exportToWkt()
        exp = 'MultiLineString((0 0, 10 10, 20 20),(12 2, 14 4))'
        self.assertTrue(compareWkt(result, exp, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        geom = QgsGeometry.fromWkt('MultiLineString((0 0, 10 10),(12 2, 14 4))')
        result = geom.mergeLines().exportToWkt()
        exp = 'MultiLineString((0 0, 10 10),(12 2, 14 4))'
        self.assertTrue(compareWkt(result, exp, 0.00001), "Merge lines: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testLineLocatePoint(self):
        """ test QgsGeometry.lineLocatePoint() """

        # not a linestring
        point = QgsGeometry.fromWkt('Point(1 2)')
        self.assertEqual(point.lineLocatePoint(point), -1)

        # not a point
        linestring = QgsGeometry.fromWkt('LineString(0 0, 10 0)')
        self.assertEqual(linestring.lineLocatePoint(linestring), -1)

        # valid
        self.assertEqual(linestring.lineLocatePoint(point), 1)
        point = QgsGeometry.fromWkt('Point(9 -2)')
        self.assertEqual(linestring.lineLocatePoint(point), 9)

        # circular string
        geom = QgsGeometry.fromWkt('CircularString (1 5, 6 2, 7 3)')
        point = QgsGeometry.fromWkt('Point(9 -2)')
        self.assertAlmostEqual(geom.lineLocatePoint(point), 7.377, places=3)

    def testInterpolateAngle(self):
        """ test QgsGeometry.interpolateAngle() """

        empty = QgsGeometry()
        # just test no crash
        self.assertEqual(empty.interpolateAngle(5), 0)

        # not a linestring
        point = QgsGeometry.fromWkt('Point(1 2)')
        # no meaning, just test no crash!
        self.assertEqual(point.interpolateAngle(5), 0)

        # linestring
        linestring = QgsGeometry.fromWkt('LineString(0 0, 10 0, 20 10, 20 20, 10 20)')
        self.assertAlmostEqual(linestring.interpolateAngle(0), math.radians(90), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(5), math.radians(90), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(10), math.radians(67.5), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(15), math.radians(45), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(25), math.radians(0), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(35), math.radians(270), places=3)

        # test first and last points in a linestring - angle should be angle of
        # first/last segment
        linestring = QgsGeometry.fromWkt('LineString(20 0, 10 0, 10 -10)')
        self.assertAlmostEqual(linestring.interpolateAngle(0), math.radians(270), places=3)
        self.assertAlmostEqual(linestring.interpolateAngle(20), math.radians(180), places=3)

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((0 0, 10 0, 20 10, 20 20, 10 20, 0 0))')
        self.assertAlmostEqual(polygon.interpolateAngle(5), math.radians(90), places=3)
        self.assertAlmostEqual(polygon.interpolateAngle(10), math.radians(67.5), places=3)
        self.assertAlmostEqual(polygon.interpolateAngle(15), math.radians(45), places=3)
        self.assertAlmostEqual(polygon.interpolateAngle(25), math.radians(0), places=3)
        self.assertAlmostEqual(polygon.interpolateAngle(35), math.radians(270), places=3)

        # test first/last vertex in polygon
        polygon = QgsGeometry.fromWkt('Polygon((0 0, 10 0, 10 10, 0 10, 0 0))')
        self.assertAlmostEqual(polygon.interpolateAngle(0), math.radians(135), places=3)
        self.assertAlmostEqual(polygon.interpolateAngle(40), math.radians(135), places=3)

        # circular string
        geom = QgsGeometry.fromWkt('CircularString (1 5, 6 2, 7 3)')
        self.assertAlmostEqual(geom.interpolateAngle(5), 1.6919, places=3)

    def testInterpolate(self):
        """ test QgsGeometry.interpolate() """

        empty = QgsGeometry()
        # just test no crash
        self.assertFalse(empty.interpolate(5))

        # not a linestring
        point = QgsGeometry.fromWkt('Point(1 2)')  # NOQA
        # no meaning, just test no crash!
        self.assertFalse(empty.interpolate(5))

        # linestring
        linestring = QgsGeometry.fromWkt('LineString(0 0, 10 0, 10 10)')
        exp = 'Point(5 0)'
        result = linestring.interpolate(5).exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001), "Interpolate: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((0 0, 10 0, 10 10, 20 20, 10 20, 0 0))')  # NOQA
        exp = 'Point(10 5)'
        result = linestring.interpolate(15).exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Interpolate: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testAngleAtVertex(self):
        """ test QgsGeometry.angleAtVertex """

        empty = QgsGeometry()
        # just test no crash
        self.assertEqual(empty.angleAtVertex(0), 0)

        # not a linestring
        point = QgsGeometry.fromWkt('Point(1 2)')
        # no meaning, just test no crash!
        self.assertEqual(point.angleAtVertex(0), 0)

        # linestring
        linestring = QgsGeometry.fromWkt('LineString(0 0, 10 0, 20 10, 20 20, 10 20)')
        self.assertAlmostEqual(linestring.angleAtVertex(1), math.radians(67.5), places=3)
        self.assertAlmostEqual(linestring.angleAtVertex(2), math.radians(22.5), places=3)
        self.assertAlmostEqual(linestring.angleAtVertex(3), math.radians(315.0), places=3)
        self.assertAlmostEqual(linestring.angleAtVertex(5), 0, places=3)
        self.assertAlmostEqual(linestring.angleAtVertex(-1), 0, places=3)

        # test first and last points in a linestring - angle should be angle of
        # first/last segment
        linestring = QgsGeometry.fromWkt('LineString(20 0, 10 0, 10 -10)')
        self.assertAlmostEqual(linestring.angleAtVertex(0), math.radians(270), places=3)
        self.assertAlmostEqual(linestring.angleAtVertex(2), math.radians(180), places=3)

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((0 0, 10 0, 10 10, 0 10, 0 0))')
        self.assertAlmostEqual(polygon.angleAtVertex(0), math.radians(135.0), places=3)
        self.assertAlmostEqual(polygon.angleAtVertex(1), math.radians(45.0), places=3)
        self.assertAlmostEqual(polygon.angleAtVertex(2), math.radians(315.0), places=3)
        self.assertAlmostEqual(polygon.angleAtVertex(3), math.radians(225.0), places=3)
        self.assertAlmostEqual(polygon.angleAtVertex(4), math.radians(135.0), places=3)

    def testExtendLine(self):
        """ test QgsGeometry.extendLine """

        empty = QgsGeometry()
        self.assertFalse(empty.extendLine(1, 2))

        # not a linestring
        point = QgsGeometry.fromWkt('Point(1 2)')
        self.assertFalse(point.extendLine(1, 2))

        # linestring
        linestring = QgsGeometry.fromWkt('LineString(0 0, 1 0, 1 1)')
        extended = linestring.extendLine(1, 2)
        exp = 'LineString(-1 0, 1 0, 1 3)'
        result = extended.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Extend line: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multilinestring
        multilinestring = QgsGeometry.fromWkt('MultiLineString((0 0, 1 0, 1 1),(11 11, 11 10, 10 10))')
        extended = multilinestring.extendLine(1, 2)
        exp = 'MultiLineString((-1 0, 1 0, 1 3),(11 12, 11 10, 8 10))'
        result = extended.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Extend line: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testRemoveRings(self):
        empty = QgsGeometry()
        self.assertFalse(empty.removeInteriorRings())

        # not a polygon
        point = QgsGeometry.fromWkt('Point(1 2)')
        self.assertFalse(point.removeInteriorRings())

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((0 0, 1 0, 1 1, 0 0),(0.1 0.1, 0.2 0.1, 0.2 0.2, 0.1 0.1))')
        removed = polygon.removeInteriorRings()
        exp = 'Polygon((0 0, 1 0, 1 1, 0 0))'
        result = removed.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Extend line: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multipolygon
        multipolygon = QgsGeometry.fromWkt('MultiPolygon(((0 0, 1 0, 1 1, 0 0),(0.1 0.1, 0.2 0.1, 0.2 0.2, 0.1 0.1)),'
                                           '((10 0, 11 0, 11 1, 10 0),(10.1 10.1, 10.2 0.1, 10.2 0.2, 10.1 0.1)))')
        removed = multipolygon.removeInteriorRings()
        exp = 'MultiPolygon(((0 0, 1 0, 1 1, 0 0)),((10 0, 11 0, 11 1, 10 0)))'
        result = removed.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Extend line: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testMinimumOrientedBoundingBox(self):
        empty = QgsGeometry()
        bbox, area, angle, width, height = empty.orientedMinimumBoundingBox()
        self.assertFalse(bbox)

        # not a useful geometry
        point = QgsGeometry.fromWkt('Point(1 2)')
        bbox, area, angle, width, height = point.orientedMinimumBoundingBox()
        self.assertFalse(bbox)

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((-0.1 -1.3, 2.1 1, 3 2.8, 6.7 0.2, 3 -1.8, 0.3 -2.7, -0.1 -1.3))')
        bbox, area, angle, width, height = polygon.orientedMinimumBoundingBox()
        exp = 'Polygon ((-0.94905660 -1.571698, 2.3817055 -4.580453, 6.7000000 0.1999999, 3.36923 3.208754, -0.949056 -1.57169))'

        result = bbox.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "Oriented MBBR: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        self.assertAlmostEqual(area, 28.9152, places=3)
        self.assertAlmostEqual(angle, 42.0922, places=3)
        self.assertAlmostEqual(width, 4.4884, places=3)
        self.assertAlmostEqual(height, 6.4420, places=3)

    def testOrthogonalize(self):
        empty = QgsGeometry()
        o = empty.orthogonalize()
        self.assertFalse(o)

        # not a useful geometry
        point = QgsGeometry.fromWkt('Point(1 2)')
        o = point.orthogonalize()
        self.assertFalse(o)

        # polygon
        polygon = QgsGeometry.fromWkt('Polygon((-0.699 0.892, -0.703 0.405, -0.022 0.361, 0.014 0.851, -0.699 0.892))')
        o = polygon.orthogonalize()
        exp = 'Polygon ((-0.69899999999999995 0.89200000000000002, -0.72568713635737736 0.38414056283699533, -0.00900222326098143 0.34648000752227009, 0.01768491457044956 0.85433944198378253, -0.69899999999999995 0.89200000000000002))'
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "orthogonalize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # polygon with ring
        polygon = QgsGeometry.fromWkt('Polygon ((-0.698 0.892, -0.702 0.405, -0.022 0.360, 0.014 0.850, -0.698 0.892),(-0.619 0.777, -0.619 0.574, -0.515 0.567, -0.517 0.516, -0.411 0.499, -0.379 0.767, -0.619 0.777),(-0.322 0.506, -0.185 0.735, -0.046 0.428, -0.322 0.506))')
        o = polygon.orthogonalize()
        exp = 'Polygon ((-0.69799999999999995 0.89200000000000002, -0.72515703079591087 0.38373993222914216, -0.00901577368860811 0.34547552423418099, 0.01814125858957143 0.85373558928902782, -0.69799999999999995 0.89200000000000002),(-0.61899999999999999 0.77700000000000002, -0.63403125159063511 0.56020458713735533, -0.53071476068518508 0.55304126003523246, -0.5343108192220235 0.5011754225601015, -0.40493624158682306 0.49220537936424585, -0.3863089084840608 0.76086661681561074, -0.61899999999999999 0.77700000000000002),(-0.32200000000000001 0.50600000000000001, -0.185 0.73499999999999999, -0.046 0.42799999999999999, -0.32200000000000001 0.50600000000000001))'
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "orthogonalize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multipolygon

        polygon = QgsGeometry.fromWkt('MultiPolygon(((-0.550 -1.553, -0.182 -0.954, -0.182 -0.954, 0.186 -1.538, -0.550 -1.553)),'
                                      '((0.506 -1.376, 0.433 -1.081, 0.765 -0.900, 0.923 -1.132, 0.923 -1.391, 0.506 -1.376)))')
        o = polygon.orthogonalize()
        exp = 'MultiPolygon (((-0.55000000000000004 -1.55299999999999994, -0.182 -0.95399999999999996, -0.182 -0.95399999999999996, 0.186 -1.53800000000000003, -0.55000000000000004 -1.55299999999999994)),((0.50600000000000001 -1.37599999999999989, 0.34888970623957499 -1.04704644438350125, 0.78332709454235683 -0.83955640656085295, 0.92300000000000004 -1.1319999999999999, 0.91737248858460974 -1.38514497083566535, 0.50600000000000001 -1.37599999999999989)))'
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "orthogonalize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # line
        line = QgsGeometry.fromWkt('LineString (-1.07445631048298162 -0.91619958829825165, 0.04022568180912156 -0.95572731852137571, 0.04741254184968957 -0.61794489661467789, 0.68704308546024517 -0.66106605685808595)')
        o = line.orthogonalize()
        exp = 'LineString (-1.07445631048298162 -0.91619958829825165, 0.04812855116470245 -0.96433184892270418, 0.06228000950284909 -0.63427853851139493, 0.68704308546024517 -0.66106605685808595)'
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "orthogonalize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testPolygonize(self):
        o = QgsGeometry.polygonize([])
        self.assertFalse(o)
        empty = QgsGeometry()
        o = QgsGeometry.polygonize([empty])
        self.assertFalse(o)
        line = QgsGeometry.fromWkt('LineString()')
        o = QgsGeometry.polygonize([line])
        self.assertFalse(o)

        l1 = QgsGeometry.fromWkt("LINESTRING (100 180, 20 20, 160 20, 100 180)")
        l2 = QgsGeometry.fromWkt("LINESTRING (100 180, 80 60, 120 60, 100 180)")
        o = QgsGeometry.polygonize([l1, l2])
        exp = "GeometryCollection(POLYGON ((100 180, 160 20, 20 20, 100 180), (100 180, 80 60, 120 60, 100 180)),POLYGON ((100 180, 120 60, 80 60, 100 180)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "polygonize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        lines = [QgsGeometry.fromWkt('LineString(0 0, 1 1)'),
                 QgsGeometry.fromWkt('LineString(0 0, 0 1)'),
                 QgsGeometry.fromWkt('LineString(0 1, 1 1)'),
                 QgsGeometry.fromWkt('LineString(1 1, 1 0)'),
                 QgsGeometry.fromWkt('LineString(1 0, 0 0)'),
                 QgsGeometry.fromWkt('LineString(5 5, 6 6)'),
                 QgsGeometry.fromWkt('Point(0, 0)')]
        o = QgsGeometry.polygonize(lines)
        exp = "GeometryCollection (Polygon ((0 0, 1 1, 1 0, 0 0)),Polygon ((1 1, 0 0, 0 1, 1 1)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "polygonize: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testDelaunayTriangulation(self):
        empty = QgsGeometry()
        o = empty.delaunayTriangulation()
        self.assertFalse(o)
        line = QgsGeometry.fromWkt('LineString()')
        o = line.delaunayTriangulation()
        self.assertFalse(o)

        input = QgsGeometry.fromWkt("MULTIPOINT ((10 10), (10 20), (20 20))")
        o = input.delaunayTriangulation()
        exp = "GEOMETRYCOLLECTION (POLYGON ((10 20, 10 10, 20 20, 10 20)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.delaunayTriangulation(0, True)
        exp = "MultiLineString ((10 20, 20 20),(10 10, 10 20),(10 10, 20 20))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("MULTIPOINT ((50 40), (140 70), (80 100), (130 140), (30 150), (70 180), (190 110), (120 20))")
        o = input.delaunayTriangulation()
        exp = "GEOMETRYCOLLECTION (POLYGON ((30 150, 50 40, 80 100, 30 150)), POLYGON ((30 150, 80 100, 70 180, 30 150)), POLYGON ((70 180, 80 100, 130 140, 70 180)), POLYGON ((70 180, 130 140, 190 110, 70 180)), POLYGON ((190 110, 130 140, 140 70, 190 110)), POLYGON ((190 110, 140 70, 120 20, 190 110)), POLYGON ((120 20, 140 70, 80 100, 120 20)), POLYGON ((120 20, 80 100, 50 40, 120 20)), POLYGON ((80 100, 140 70, 130 140, 80 100)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.delaunayTriangulation(0, True)
        exp = "MultiLineString ((70 180, 190 110),(30 150, 70 180),(30 150, 50 40),(50 40, 120 20),(120 20, 190 110),(120 20, 140 70),(140 70, 190 110),(130 140, 140 70),(130 140, 190 110),(70 180, 130 140),(80 100, 130 140),(70 180, 80 100),(30 150, 80 100),(50 40, 80 100),(80 100, 120 20),(80 100, 140 70))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((10 10), (10 20), (20 20), (20 10), (20 0), (10 0), (0 0), (0 10), (0 20))")
        o = input.delaunayTriangulation()
        exp = "GEOMETRYCOLLECTION (POLYGON ((0 20, 0 10, 10 10, 0 20)), POLYGON ((0 20, 10 10, 10 20, 0 20)), POLYGON ((10 20, 10 10, 20 10, 10 20)), POLYGON ((10 20, 20 10, 20 20, 10 20)), POLYGON ((10 0, 20 0, 10 10, 10 0)), POLYGON ((10 0, 10 10, 0 10, 10 0)), POLYGON ((10 0, 0 10, 0 0, 10 0)), POLYGON ((10 10, 20 0, 20 10, 10 10)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.delaunayTriangulation(0, True)
        exp = "MultiLineString ((10 20, 20 20),(0 20, 10 20),(0 10, 0 20),(0 0, 0 10),(0 0, 10 0),(10 0, 20 0),(20 0, 20 10),(20 10, 20 20),(10 20, 20 10),(10 10, 20 10),(10 10, 10 20),(0 20, 10 10),(0 10, 10 10),(10 0, 10 10),(0 10, 10 0),(10 10, 20 0))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt(
            "POLYGON ((42 30, 41.96 29.61, 41.85 29.23, 41.66 28.89, 41.41 28.59, 41.11 28.34, 40.77 28.15, 40.39 28.04, 40 28, 39.61 28.04, 39.23 28.15, 38.89 28.34, 38.59 28.59, 38.34 28.89, 38.15 29.23, 38.04 29.61, 38 30, 38.04 30.39, 38.15 30.77, 38.34 31.11, 38.59 31.41, 38.89 31.66, 39.23 31.85, 39.61 31.96, 40 32, 40.39 31.96, 40.77 31.85, 41.11 31.66, 41.41 31.41, 41.66 31.11, 41.85 30.77, 41.96 30.39, 42 30))")
        o = input.delaunayTriangulation(0, True)
        exp = "MULTILINESTRING ((41.66 31.11, 41.85 30.77), (41.41 31.41, 41.66 31.11), (41.11 31.66, 41.41 31.41), (40.77 31.85, 41.11 31.66), (40.39 31.96, 40.77 31.85), (40 32, 40.39 31.96), (39.61 31.96, 40 32), (39.23 31.85, 39.61 31.96), (38.89 31.66, 39.23 31.85), (38.59 31.41, 38.89 31.66), (38.34 31.11, 38.59 31.41), (38.15 30.77, 38.34 31.11), (38.04 30.39, 38.15 30.77), (38 30, 38.04 30.39), (38 30, 38.04 29.61), (38.04 29.61, 38.15 29.23), (38.15 29.23, 38.34 28.89), (38.34 28.89, 38.59 28.59), (38.59 28.59, 38.89 28.34), (38.89 28.34, 39.23 28.15), (39.23 28.15, 39.61 28.04), (39.61 28.04, 40 28), (40 28, 40.39 28.04), (40.39 28.04, 40.77 28.15), (40.77 28.15, 41.11 28.34), (41.11 28.34, 41.41 28.59), (41.41 28.59, 41.66 28.89), (41.66 28.89, 41.85 29.23), (41.85 29.23, 41.96 29.61), (41.96 29.61, 42 30), (41.96 30.39, 42 30), (41.85 30.77, 41.96 30.39), (41.66 31.11, 41.96 30.39), (41.41 31.41, 41.96 30.39), (41.41 28.59, 41.96 30.39), (41.41 28.59, 41.41 31.41), (38.59 28.59, 41.41 28.59), (38.59 28.59, 41.41 31.41), (38.59 28.59, 38.59 31.41), (38.59 31.41, 41.41 31.41), (38.59 31.41, 39.61 31.96), (39.61 31.96, 41.41 31.41), (39.61 31.96, 40.39 31.96), (40.39 31.96, 41.41 31.41), (40.39 31.96, 41.11 31.66), (38.04 30.39, 38.59 28.59), (38.04 30.39, 38.59 31.41), (38.04 30.39, 38.34 31.11), (38.04 29.61, 38.59 28.59), (38.04 29.61, 38.04 30.39), (39.61 28.04, 41.41 28.59), (38.59 28.59, 39.61 28.04), (38.89 28.34, 39.61 28.04), (40.39 28.04, 41.41 28.59), (39.61 28.04, 40.39 28.04), (41.96 29.61, 41.96 30.39), (41.41 28.59, 41.96 29.61), (41.66 28.89, 41.96 29.61), (40.39 28.04, 41.11 28.34), (38.04 29.61, 38.34 28.89), (38.89 31.66, 39.61 31.96))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt(
            "POLYGON ((0 0, 0 200, 180 200, 180 0, 0 0), (20 180, 160 180, 160 20, 152.625 146.75, 20 180), (30 160, 150 30, 70 90, 30 160))")
        o = input.delaunayTriangulation(0, True)
        exp = "MultiLineString ((0 200, 180 200),(0 0, 0 200),(0 0, 180 0),(180 0, 180 200),(152.625 146.75, 180 0),(152.625 146.75, 180 200),(152.625 146.75, 160 180),(160 180, 180 200),(0 200, 160 180),(20 180, 160 180),(0 200, 20 180),(20 180, 30 160),(0 200, 30 160),(0 0, 30 160),(30 160, 70 90),(0 0, 70 90),(70 90, 150 30),(0 0, 150 30),(150 30, 160 20),(0 0, 160 20),(160 20, 180 0),(152.625 146.75, 160 20),(150 30, 152.625 146.75),(70 90, 152.625 146.75),(30 160, 152.625 146.75),(30 160, 160 180))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((10 10 1), (10 20 2), (20 20 3), (20 10 1.5), (20 0 2.5), (10 0 3.5), (0 0 0), (0 10 .5), (0 20 .25))")
        o = input.delaunayTriangulation()
        exp = "GeometryCollection (PolygonZ ((0 20 0.25, 0 10 0.5, 10 10 1, 0 20 0.25)),PolygonZ ((0 20 0.25, 10 10 1, 10 20 2, 0 20 0.25)),PolygonZ ((10 20 2, 10 10 1, 20 10 1.5, 10 20 2)),PolygonZ ((10 20 2, 20 10 1.5, 20 20 3, 10 20 2)),PolygonZ ((10 0 3.5, 20 0 2.5, 10 10 1, 10 0 3.5)),PolygonZ ((10 0 3.5, 10 10 1, 0 10 0.5, 10 0 3.5)),PolygonZ ((10 0 3.5, 0 10 0.5, 0 0 0, 10 0 3.5)),PolygonZ ((10 10 1, 20 0 2.5, 20 10 1.5, 10 10 1)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.delaunayTriangulation(0, True)
        exp = "MultiLineStringZ ((10 20 2, 20 20 3),(0 20 0.25, 10 20 2),(0 10 0.5, 0 20 0.25),(0 0 0, 0 10 0.5),(0 0 0, 10 0 3.5),(10 0 3.5, 20 0 2.5),(20 0 2.5, 20 10 1.5),(20 10 1.5, 20 20 3),(10 20 2, 20 10 1.5),(10 10 1, 20 10 1.5),(10 10 1, 10 20 2),(0 20 0.25, 10 10 1),(0 10 0.5, 10 10 1),(10 0 3.5, 10 10 1),(0 10 0.5, 10 0 3.5),(10 10 1, 20 0 2.5))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt(
            "MULTIPOINT((-118.3964065 56.0557),(-118.396406 56.0475),(-118.396407 56.04),(-118.3968 56))")
        o = input.delaunayTriangulation(0.001, True)
        exp = "MULTILINESTRING ((-118.3964065 56.0557, -118.396406 56.0475), (-118.396407 56.04, -118.396406 56.0475), (-118.3968 56, -118.396407 56.04))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testVoronoi(self):
        empty = QgsGeometry()
        o = empty.voronoiDiagram()
        self.assertFalse(o)
        line = QgsGeometry.fromWkt('LineString()')
        o = line.voronoiDiagram()
        self.assertFalse(o)

        input = QgsGeometry.fromWkt("MULTIPOINT ((150 200))")
        o = input.voronoiDiagram()
        self.assertTrue(o.isEmpty())

        input = QgsGeometry.fromWkt("MULTIPOINT ((150 200), (180 270), (275 163))")
        o = input.voronoiDiagram()
        exp = "GeometryCollection (Polygon ((170.02400000000000091 38, 25 38, 25 295, 221.20588235294115975 210.91176470588234793, 170.02400000000000091 38)),Polygon ((400 369.65420560747662648, 400 38, 170.02400000000000091 38, 221.20588235294115975 210.91176470588234793, 400 369.65420560747662648)),Polygon ((25 295, 25 395, 400 395, 400 369.65420560747662648, 221.20588235294115975 210.91176470588234793, 25 295)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("MULTIPOINT ((280 300), (420 330), (380 230), (320 160))")
        o = input.voronoiDiagram()
        exp = "GeometryCollection (Polygon ((110 175.71428571428572241, 110 500, 310.35714285714283278 500, 353.515625 298.59375, 306.875 231.96428571428572241, 110 175.71428571428572241)),Polygon ((590 204, 590 -10, 589.16666666666662877 -10, 306.875 231.96428571428572241, 353.515625 298.59375, 590 204)),Polygon ((589.16666666666662877 -10, 110 -10, 110 175.71428571428572241, 306.875 231.96428571428572241, 589.16666666666662877 -10)),Polygon ((310.35714285714283278 500, 590 500, 590 204, 353.515625 298.59375, 310.35714285714283278 500)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("MULTIPOINT ((320 170), (366 246), (530 230), (530 300), (455 277), (490 160))")
        o = input.voronoiDiagram()
        exp = "GeometryCollection (Polygon ((392.35294117647055145 -50, 110 -50, 110 349.02631578947364233, 405.31091180866962986 170.28550074738416242, 392.35294117647055145 -50)),Polygon ((740 63.57142857142859071, 740 -50, 392.35294117647055145 -50, 405.31091180866962986 170.28550074738416242, 429.91476778570188344 205.76082797008174907, 470.12061711079945781 217.78821879382888937, 740 63.57142857142859071)),Polygon ((110 349.02631578947364233, 110 510, 323.94382022471910432 510, 429.91476778570188344 205.76082797008174907, 405.31091180866962986 170.28550074738416242, 110 349.02631578947364233)),Polygon ((323.94382022471910432 510, 424.57333333333326664 510, 499.70666666666664923 265, 470.12061711079945781 217.78821879382888937, 429.91476778570188344 205.76082797008174907, 323.94382022471910432 510)),Polygon ((740 265, 740 63.57142857142859071, 470.12061711079945781 217.78821879382888937, 499.70666666666664923 265, 740 265)),Polygon ((424.57333333333326664 510, 740 510, 740 265, 499.70666666666664923 265, 424.57333333333326664 510)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("MULTIPOINT ((280 200), (406 285), (580 280), (550 190), (370 190), (360 90), (480 110), (440 160), (450 180), (480 180), (460 160), (360 210), (360 220), (370 210), (375 227))")
        o = input.voronoiDiagram()
        exp = "GeometryCollection (Polygon ((-20 -102.27272727272726627, -20 585, 111.94841269841269593 585, 293.54906542056073704 315.803738317756995, 318.75 215, 323.2352941176470722 179.1176470588235361, 319.39560439560437999 144.560439560439562, -20 -102.27272727272726627)),Polygon ((365 200, 365 215, 369.40909090909093493 219.40909090909090651, 414.21192052980131848 206.23178807947019209, 411.875 200, 365 200)),Polygon ((365 215, 365 200, 323.2352941176470722 179.1176470588235361, 318.75 215, 365 215)),Polygon ((471.66666666666674246 -210, -20 -210, -20 -102.27272727272726627, 319.39560439560437999 144.560439560439562, 388.97260273972602818 137.60273972602738013, 419.55882352941176805 102.64705882352942012, 471.66666666666674246 -210)),Polygon ((411.875 200, 410.29411764705884025 187.35294117647057988, 388.97260273972602818 137.60273972602738013, 319.39560439560437999 144.560439560439562, 323.2352941176470722 179.1176470588235361, 365 200, 411.875 200)),Polygon ((410.29411764705884025 187.35294117647057988, 411.875 200, 414.21192052980131848 206.23178807947019209, 431.62536593766145643 234.0192009643533595, 465 248.00476190476189231, 465 175, 450 167.5, 410.29411764705884025 187.35294117647057988)),Polygon ((293.54906542056073704 315.803738317756995, 339.65007656967839011 283.17840735068909908, 369.40909090909093493 219.40909090909090651, 365 215, 318.75 215, 293.54906542056073704 315.803738317756995)),Polygon ((111.94841269841269593 585, 501.69252873563215189 585, 492.56703910614521646 267.43296089385472669, 465 248.00476190476189231, 431.62536593766145643 234.0192009643533595, 339.65007656967839011 283.17840735068909908, 293.54906542056073704 315.803738317756995, 111.94841269841269593 585)),Polygon ((369.40909090909093493 219.40909090909090651, 339.65007656967839011 283.17840735068909908, 431.62536593766145643 234.0192009643533595, 414.21192052980131848 206.23178807947019209, 369.40909090909093493 219.40909090909090651)),Polygon ((388.97260273972602818 137.60273972602738013, 410.29411764705884025 187.35294117647057988, 450 167.5, 450 127, 419.55882352941176805 102.64705882352942012, 388.97260273972602818 137.60273972602738013)),Polygon ((465 175, 465 248.00476190476189231, 492.56703910614521646 267.43296089385472669, 505 255, 520.71428571428566556 145, 495 145, 465 175)),Polygon ((880 -169.375, 880 -210, 471.66666666666674246 -210, 419.55882352941176805 102.64705882352942012, 450 127, 495 145, 520.71428571428566556 145, 880 -169.375)),Polygon ((465 175, 495 145, 450 127, 450 167.5, 465 175)),Polygon ((501.69252873563215189 585, 880 585, 880 130.00000000000005684, 505 255, 492.56703910614521646 267.43296089385472669, 501.69252873563215189 585)),Polygon ((880 130.00000000000005684, 880 -169.375, 520.71428571428566556 145, 505 255, 880 130.00000000000005684)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((100 200), (105 202), (110 200), (140 230), (210 240), (220 190), (170 170), (170 260), (213 245), (220 190))")
        o = input.voronoiDiagram(QgsGeometry(), 6)
        exp = "GeometryCollection (Polygon ((77.1428571428571388 50, -20 50, -20 380, -3.75 380, 105 235, 105 115, 77.1428571428571388 50)),Polygon ((247 50, 77.1428571428571388 50, 105 115, 145 195, 178.33333333333334281 211.66666666666665719, 183.51851851851853326 208.70370370370369528, 247 50)),Polygon ((-3.75 380, 20.00000000000000711 380, 176.66666666666665719 223.33333333333334281, 178.33333333333334281 211.66666666666665719, 145 195, 105 235, -3.75 380)),Polygon ((105 115, 105 235, 145 195, 105 115)),Polygon ((20.00000000000000711 380, 255 380, 176.66666666666665719 223.33333333333334281, 20.00000000000000711 380)),Polygon ((255 380, 340 380, 340 240, 183.51851851851853326 208.70370370370369528, 178.33333333333334281 211.66666666666665719, 176.66666666666665719 223.33333333333334281, 255 380)),Polygon ((340 240, 340 50, 247 50, 183.51851851851853326 208.70370370370369528, 340 240)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((170 270), (177 275), (190 230), (230 250), (210 290), (240 280), (240 250))")
        o = input.voronoiDiagram(QgsGeometry(), 10)
        exp = "GeometryCollection (Polygon ((100 210, 100 360, 150 360, 200 260, 100 210)),Polygon ((150 360, 250 360, 220 270, 200 260, 150 360)),Polygon ((247 160, 100 160, 100 210, 200 260, 235 190, 247 160)),Polygon ((220 270, 235 265, 235 190, 200 260, 220 270)),Polygon ((250 360, 310 360, 310 265, 235 265, 220 270, 250 360)),Polygon ((310 265, 310 160, 247 160, 235 190, 235 265, 310 265)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((155 271), (150 360), (260 360), (271 265), (280 260), (270 370), (154 354), (150 260))")
        o = input.voronoiDiagram(QgsGeometry(), 100)
        exp = "GeometryCollection (Polygon ((215 130, 20 130, 20 310, 205 310, 215 299, 215 130)),Polygon ((205 500, 410 500, 410 338, 215 299, 205 310, 205 500)),Polygon ((20 310, 20 500, 205 500, 205 310, 20 310)),Polygon ((410 338, 410 130, 215 130, 215 299, 410 338)))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "delaunay: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testDensifyByCount(self):

        empty = QgsGeometry()
        o = empty.densifyByCount(4)
        self.assertFalse(o)

        # point
        input = QgsGeometry.fromWkt("PointZ( 1 2 3 )")
        o = input.densifyByCount(100)
        exp = "PointZ( 1 2 3 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((155 271), (150 360), (260 360), (271 265), (280 260), (270 370), (154 354), (150 260))")
        o = input.densifyByCount(100)
        exp = "MULTIPOINT ((155 271), (150 360), (260 360), (271 265), (280 260), (270 370), (154 354), (150 260))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # line
        input = QgsGeometry.fromWkt("LineString( 0 0, 10 0, 10 10 )")
        o = input.densifyByCount(0)
        exp = "LineString( 0 0, 10 0, 10 10 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.densifyByCount(1)
        exp = "LineString( 0 0, 5 0, 10 0, 10 5, 10 10 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.densifyByCount(3)
        exp = "LineString( 0 0, 2.5 0, 5 0, 7.5 0, 10 0, 10 2.5, 10 5, 10 7.5, 10 10 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringZ( 0 0 1, 10 0 2, 10 10 0)")
        o = input.densifyByCount(1)
        exp = "LineStringZ( 0 0 1, 5 0 1.5, 10 0 2, 10 5 1, 10 10 0 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringM( 0 0 0, 10 0 2, 10 10 0)")
        o = input.densifyByCount(1)
        exp = "LineStringM( 0 0 0, 5 0 1, 10 0 2, 10 5 1, 10 10 0 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringZM( 0 0 1 10, 10 0 2 8, 10 10 0 4)")
        o = input.densifyByCount(1)
        exp = "LineStringZM( 0 0 1 10, 5 0 1.5 9, 10 0 2 8, 10 5 1 6, 10 10 0 4 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # polygon
        input = QgsGeometry.fromWkt("Polygon(( 0 0, 10 0, 10 10, 0 0 ))")
        o = input.densifyByCount(0)
        exp = "Polygon(( 0 0, 10 0, 10 10, 0 0 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("PolygonZ(( 0 0 1, 10 0 2, 10 10 0, 0 0 1 ))")
        o = input.densifyByCount(1)
        exp = "PolygonZ(( 0 0 1, 5 0 1.5, 10 0 2, 10 5 1, 10 10 0, 5 5 0.5, 0 0 1 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("PolygonZM(( 0 0 1 4, 10 0 2 6, 10 10 0 8, 0 0 1 4 ))")
        o = input.densifyByCount(1)
        exp = "PolygonZM(( 0 0 1 4, 5 0 1.5 5, 10 0 2 6, 10 5 1 7, 10 10 0 8, 5 5 0.5 6, 0 0 1 4 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        # (not strictly valid, but shouldn't matter!
        input = QgsGeometry.fromWkt("PolygonZM(( 0 0 1 4, 10 0 2 6, 10 10 0 8, 0 0 1 4 ), ( 0 0 1 4, 10 0 2 6, 10 10 0 8, 0 0 1 4 ) )")
        o = input.densifyByCount(1)
        exp = "PolygonZM(( 0 0 1 4, 5 0 1.5 5, 10 0 2 6, 10 5 1 7, 10 10 0 8, 5 5 0.5 6, 0 0 1 4 ),( 0 0 1 4, 5 0 1.5 5, 10 0 2 6, 10 5 1 7, 10 10 0 8, 5 5 0.5 6, 0 0 1 4 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multi line
        input = QgsGeometry.fromWkt("MultiLineString(( 0 0, 5 0, 10 0, 10 5, 10 10), (20 0, 25 0, 30 0, 30 5, 30 10 ) )")
        o = input.densifyByCount(1)
        exp = "MultiLineString(( 0 0, 2.5 0, 5 0, 7.5 0, 10 0, 10 2.5, 10 5, 10 7.5, 10 10 ),( 20 0, 22.5 0, 25 0, 27.5 0, 30 0, 30 2.5, 30 5, 30 7.5, 30 10 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # multipolygon
        input = QgsGeometry.fromWkt("MultiPolygonZ(((  0 0 1, 10 0 2, 10 10 0, 0 0 1)),((  0 0 1, 10 0 2, 10 10 0, 0 0 1 )))")
        o = input.densifyByCount(1)
        exp = "MultiPolygonZ((( 0 0 1, 5 0 1.5, 10 0 2, 10 5 1, 10 10 0, 5 5 0.5, 0 0 1 )),(( 0 0 1, 5 0 1.5, 10 0 2, 10 5 1, 10 10 0, 5 5 0.5, 0 0 1 )))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testDensifyByDistance(self):
        empty = QgsGeometry()
        o = empty.densifyByDistance(4)
        self.assertFalse(o)

        # point
        input = QgsGeometry.fromWkt("PointZ( 1 2 3 )")
        o = input.densifyByDistance(0.1)
        exp = "PointZ( 1 2 3 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt(
            "MULTIPOINT ((155 271), (150 360), (260 360), (271 265), (280 260), (270 370), (154 354), (150 260))")
        o = input.densifyByDistance(0.1)
        exp = "MULTIPOINT ((155 271), (150 360), (260 360), (271 265), (280 260), (270 370), (154 354), (150 260))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # line
        input = QgsGeometry.fromWkt("LineString( 0 0, 10 0, 10 10 )")
        o = input.densifyByDistance(100)
        exp = "LineString( 0 0, 10 0, 10 10 )"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        o = input.densifyByDistance(3)
        exp = "LineString (0 0, 2.5 0, 5 0, 7.5 0, 10 0, 10 2.5, 10 5, 10 7.5, 10 10)"

        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringZ( 0 0 1, 10 0 2, 10 10 0)")
        o = input.densifyByDistance(6)
        exp = "LineStringZ (0 0 1, 5 0 1.5, 10 0 2, 10 5 1, 10 10 0)"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringM( 0 0 0, 10 0 2, 10 10 0)")
        o = input.densifyByDistance(3)
        exp = "LineStringM (0 0 0, 2.5 0 0.5, 5 0 1, 7.5 0 1.5, 10 0 2, 10 2.5 1.5, 10 5 1, 10 7.5 0.5, 10 10 0)"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))
        input = QgsGeometry.fromWkt("LineStringZM( 0 0 1 10, 10 0 2 8, 10 10 0 4)")
        o = input.densifyByDistance(6)
        exp = "LineStringZM (0 0 1 10, 5 0 1.5 9, 10 0 2 8, 10 5 1 6, 10 10 0 4)"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        # polygon
        input = QgsGeometry.fromWkt("Polygon(( 0 0, 20 0, 20 20, 0 0 ))")
        o = input.densifyByDistance(110)
        exp = "Polygon(( 0 0, 20 0, 20 20, 0 0 ))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

        input = QgsGeometry.fromWkt("PolygonZ(( 0 0 1, 20 0 2, 20 20 0, 0 0 1 ))")
        o = input.densifyByDistance(6)
        exp = "PolygonZ ((0 0 1, 5 0 1.25, 10 0 1.5, 15 0 1.75, 20 0 2, 20 5 1.5, 20 10 1, 20 15 0.5, 20 20 0, 16 16 0.2, 12 12 0.4, 8 8 0.6, 4 4 0.8, 0 0 1))"
        result = o.exportToWkt()
        self.assertTrue(compareWkt(result, exp, 0.00001),
                        "densify by count: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testCentroid(self):
        tests = [["POINT(10 0)", "POINT(10 0)"],
                 ["POINT(10 10)", "POINT(10 10)"],
                 ["MULTIPOINT((10 10), (20 20) )", "POINT(15 15)"],
                 [" MULTIPOINT((10 10), (20 20), (10 20), (20 10))", "POINT(15 15)"],
                 ["LINESTRING(10 10, 20 20)", "POINT(15 15)"],
                 ["LINESTRING(0 0, 10 0)", "POINT(5 0 )"],
                 ["LINESTRING (10 10, 10 10)", "POINT (10 10)"], # zero length line
                 ["MULTILINESTRING ((10 10, 10 10), (20 20, 20 20))", "POINT (15 15)"], # zero length multiline
                 ["LINESTRING (60 180, 120 100, 180 180)", "POINT (120 140)"],
                 ["LINESTRING (80 0, 80 120, 120 120, 120 0))", "POINT (100 68.57142857142857)"],
                 ["MULTILINESTRING ((0 0, 0 100), (100 0, 100 100))", "POINT (50 50)"],
                 [" MULTILINESTRING ((0 0, 0 200, 200 200, 200 0, 0 0),(60 180, 20 180, 20 140, 60 140, 60 180))", "POINT (90 110)"],
                 ["MULTILINESTRING ((20 20, 60 60),(20 -20, 60 -60),(-20 -20, -60 -60),(-20 20, -60 60),(-80 0, 0 80, 80 0, 0 -80, -80 0),(-40 20, -40 -20),(-20 40, 20 40),(40 20, 40 -20),(20 -40, -20 -40))", "POINT (0 0)"],
                 ["POLYGON((0 0, 10 0, 10 10, 0 10, 0 0))", "POINT (5 5)"],
                 ["POLYGON ((40 160, 160 160, 160 40, 40 40, 40 160))", "POINT (100 100)"],
                 ["POLYGON ((0 200, 200 200, 200 0, 0 0, 0 200), (20 180, 80 180, 80 20, 20 20, 20 180))", "POINT (115.78947368421052 100)"],
                 ["POLYGON ((0 0, 0 200, 200 200, 200 0, 0 0),(60 180, 20 180, 20 140, 60 140, 60 180))", "POINT (102.5 97.5)"],
                 ["POLYGON ((0 0, 0 200, 200 200, 200 0, 0 0),(60 180, 20 180, 20 140, 60 140, 60 180),(180 60, 140 60, 140 20, 180 20, 180 60))", "POINT (100 100)"],
                 ["MULTIPOLYGON (((0 40, 0 140, 140 140, 140 120, 20 120, 20 40, 0 40)),((0 0, 0 20, 120 20, 120 100, 140 100, 140 0, 0 0)))", "POINT (70 70)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((0 200, 20 180, 20 140, 60 140, 200 0, 0 0, 0 200)),POLYGON ((200 200, 0 200, 20 180, 60 180, 60 140, 200 0, 200 200)))", "POINT (102.5 97.5)"],
                 ["GEOMETRYCOLLECTION (LINESTRING (80 0, 80 120, 120 120, 120 0),MULTIPOINT ((20 60), (40 80), (60 60)))", "POINT (100 68.57142857142857)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((0 40, 40 40, 40 0, 0 0, 0 40)),LINESTRING (80 0, 80 80, 120 40))", "POINT (20 20)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((0 40, 40 40, 40 0, 0 0, 0 40)),LINESTRING (80 0, 80 80, 120 40),MULTIPOINT ((20 60), (40 80), (60 60)))", "POINT (20 20)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((10 10, 10 10, 10 10, 10 10)),LINESTRING (20 20, 30 30))", "POINT (25 25)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((10 10, 10 10, 10 10, 10 10)),LINESTRING (20 20, 20 20))", "POINT (15 15)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((10 10, 10 10, 10 10, 10 10)),LINESTRING (20 20, 20 20),MULTIPOINT ((20 10), (10 20)) )", "POINT (15 15)"],
                 # ["GEOMETRYCOLLECTION (POLYGON ((10 10, 10 10, 10 10, 10 10)),LINESTRING (20 20, 20 20),POINT EMPTY )","POINT (15 15)"],
                 # ["GEOMETRYCOLLECTION (POLYGON ((10 10, 10 10, 10 10, 10 10)),LINESTRING EMPTY,POINT EMPTY )","POINT (10 10)"],
                 ["GEOMETRYCOLLECTION (POLYGON ((20 100, 20 -20, 60 -20, 60 100, 20 100)),POLYGON ((-20 60, 100 60, 100 20, -20 20, -20 60)))", "POINT (40 40)"],
                 ["POLYGON ((40 160, 160 160, 160 160, 40 160, 40 160))", "POINT (100 160)"],
                 ["POLYGON ((10 10, 100 100, 100 100, 10 10))", "POINT (55 55)"],
                 # ["POLYGON EMPTY","POINT EMPTY"],
                 # ["MULTIPOLYGON(EMPTY,((0 0,1 0,1 1,0 1, 0 0)))","POINT (0.5 0.5)"],
                 ["POLYGON((56.528666666700 25.2101666667,56.529000000000 25.2105000000,56.528833333300 25.2103333333,56.528666666700 25.2101666667))", "POINT (56.52883333335 25.21033333335)"],
                 ["POLYGON((56.528666666700 25.2101666667,56.529000000000 25.2105000000,56.528833333300 25.2103333333,56.528666666700 25.2101666667))", "POINT (56.528833 25.210333)"]
                 ]
        for t in tests:
            input = QgsGeometry.fromWkt(t[0])
            o = input.centroid()
            exp = t[1]
            result = o.exportToWkt()
            self.assertTrue(compareWkt(result, exp, 0.00001),
                            "centroid: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

            # QGIS native algorithms are bad!
            if False:
                result = QgsGeometry(input.geometry().centroid()).exportToWkt()
                self.assertTrue(compareWkt(result, exp, 0.00001),
                                "centroid: mismatch using QgsAbstractGeometry methods Input {} \n Expected:\n{}\nGot:\n{}\n".format(t[0], exp, result))

    def testCompare(self):
        lp = [QgsPointXY(1, 1), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)]
        lp2 = [QgsPointXY(1, 1.0000001), QgsPointXY(2, 2), QgsPointXY(1, 2), QgsPointXY(1, 1)]
        self.assertTrue(QgsGeometry.compare(lp, lp))  # line-line
        self.assertTrue(QgsGeometry.compare([lp], [lp]))  # pylygon-polygon
        self.assertTrue(QgsGeometry.compare([[lp]], [[lp]]))  # multipyolygon-multipolygon
        # handling empty values
        self.assertFalse(QgsGeometry.compare(None, None))
        self.assertFalse(QgsGeometry.compare(lp, []))  # line-line
        self.assertFalse(QgsGeometry.compare([lp], [[]]))  # pylygon-polygon
        self.assertFalse(QgsGeometry.compare([[lp]], [[[]]]))  # multipolygon-multipolygon
        # tolerance
        self.assertFalse(QgsGeometry.compare(lp, lp2))
        self.assertTrue(QgsGeometry.compare(lp, lp2, 1e-6))

    def testPoint(self):
        point = QgsPoint(1, 2)
        self.assertEqual(point.wkbType(), QgsWkbTypes.Point)
        self.assertEqual(point.x(), 1)
        self.assertEqual(point.y(), 2)

        point = QgsPoint(1, 2, wkbType=QgsWkbTypes.Point)
        self.assertEqual(point.wkbType(), QgsWkbTypes.Point)
        self.assertEqual(point.x(), 1)
        self.assertEqual(point.y(), 2)

        point_z = QgsPoint(1, 2, 3)
        self.assertEqual(point_z.wkbType(), QgsWkbTypes.PointZ)
        self.assertEqual(point_z.x(), 1)
        self.assertEqual(point_z.y(), 2)
        self.assertEqual(point_z.z(), 3)

        point_z = QgsPoint(1, 2, 3, 4, wkbType=QgsWkbTypes.PointZ)
        self.assertEqual(point_z.wkbType(), QgsWkbTypes.PointZ)
        self.assertEqual(point_z.x(), 1)
        self.assertEqual(point_z.y(), 2)
        self.assertEqual(point_z.z(), 3)

        point_m = QgsPoint(1, 2, m=3)
        self.assertEqual(point_m.wkbType(), QgsWkbTypes.PointM)
        self.assertEqual(point_m.x(), 1)
        self.assertEqual(point_m.y(), 2)
        self.assertEqual(point_m.m(), 3)

        point_zm = QgsPoint(1, 2, 3, 4)
        self.assertEqual(point_zm.wkbType(), QgsWkbTypes.PointZM)
        self.assertEqual(point_zm.x(), 1)
        self.assertEqual(point_zm.y(), 2)
        self.assertEqual(point_zm.z(), 3)
        self.assertEqual(point_zm.m(), 4)

    def testSubdivide(self):
        tests = [["LINESTRING (1 1,1 9,9 9,9 1)", 8, "MULTILINESTRING ((1 1,1 9,9 9,9 1))"],
                 ["Point (1 1)", 8, "MultiPoint ((1 1))"],
                 ["GeometryCollection ()", 8, "GeometryCollection ()"],
                 ["LINESTRING (1 1,1 2,1 3,1 4,1 5,1 6,1 7,1 8,1 9)", 8, "MultiLineString ((1 1, 1 2, 1 3, 1 4, 1 5),(1 5, 1 6, 1 7, 1 8, 1 9))"],
                 ["LINESTRING(0 0, 100 100, 150 150)", 8, 'MultiLineString ((0 0, 100 100, 150 150))'],
                 ['POLYGON((132 10,119 23,85 35,68 29,66 28,49 42,32 56,22 64,32 110,40 119,36 150,57 158,75 171,92 182,114 184,132 186,146 178,176 184,179 162,184 141,190 122,190 100,185 79,186 56,186 52,178 34,168 18,147 13,132 10))',
                  10, None],
                 ["LINESTRING (1 1,1 2,1 3,1 4,1 5,1 6,1 7,1 8,1 9)", 1,
                  "MultiLineString ((1 1, 1 2, 1 3, 1 4, 1 5),(1 5, 1 6, 1 7, 1 8, 1 9))"],
                 ["LINESTRING (1 1,1 2,1 3,1 4,1 5,1 6,1 7,1 8,1 9)", 16,
                  "MultiLineString ((1 1, 1 2, 1 3, 1 4, 1 5, 1 6, 1 7, 1 8, 1 9))"],
                 ["POLYGON ((0 0, 0 200, 200 200, 200 0, 0 0),(60 180, 20 180, 20 140, 60 140, 60 180),(180 60, 140 60, 140 20, 180 20, 180 60))", 8, "MultiPolygon (((0 0, 0 100, 100 100, 100 0, 0 0)),((100 0, 100 50, 140 50, 140 20, 150 20, 150 0, 100 0)),((150 0, 150 20, 180 20, 180 50, 200 50, 200 0, 150 0)),((100 50, 100 100, 150 100, 150 60, 140 60, 140 50, 100 50)),((150 60, 150 100, 200 100, 200 50, 180 50, 180 60, 150 60)),((0 100, 0 150, 20 150, 20 140, 50 140, 50 100, 0 100)),((50 100, 50 140, 60 140, 60 150, 100 150, 100 100, 50 100)),((0 150, 0 200, 50 200, 50 180, 20 180, 20 150, 0 150)),((50 180, 50 200, 100 200, 100 150, 60 150, 60 180, 50 180)),((100 100, 100 200, 200 200, 200 100, 100 100)))"],
                 ["POLYGON((132 10,119 23,85 35,68 29,66 28,49 42,32 56,22 64,32 110,40 119,36 150, 57 158,75 171,92 182,114 184,132 186,146 178,176 184,179 162,184 141,190 122,190 100,185 79,186 56,186 52,178 34,168 18,147 13,132 10))", 10, None]
                 ]
        for t in tests:
            input = QgsGeometry.fromWkt(t[0])
            o = input.subdivide(t[1])
            # make sure area is unchanged
            self.assertAlmostEqual(input.area(), o.area(), 5)
            max_points = 999999
            for p in range(o.geometry().numGeometries()):
                part = o.geometry().geometryN(p)
                self.assertLessEqual(part.nCoordinates(), max(t[1], 8))

            if t[2]:
                exp = t[2]
                result = o.exportToWkt()
                self.assertTrue(compareWkt(result, exp, 0.00001),
                                "clipped: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))

    def testClipped(self):
        tests = [["LINESTRING (1 1,1 9,9 9,9 1)", QgsRectangle(0, 0, 10, 10), "LINESTRING (1 1,1 9,9 9,9 1)"],
                 ["LINESTRING (-1 -9,-1 11,9 11)", QgsRectangle(0, 0, 10, 10), "GEOMETRYCOLLECTION ()"],
                 ["LINESTRING (-1 5,5 5,9 9)", QgsRectangle(0, 0, 10, 10), "LINESTRING (0 5,5 5,9 9)"],
                 ["LINESTRING (5 5,8 5,12 5)", QgsRectangle(0, 0, 10, 10), "LINESTRING (5 5,8 5,10 5)"],
                 ["LINESTRING (5 -1,5 5,1 2,-3 2,1 6)", QgsRectangle(0, 0, 10, 10), "MULTILINESTRING ((5 0,5 5,1 2,0 2),(0 5,1 6))"],
                 ["LINESTRING (0 3,0 5,0 7)", QgsRectangle(0, 0, 10, 10), "GEOMETRYCOLLECTION ()"],
                 ["LINESTRING (0 3,0 5,-1 7)", QgsRectangle(0, 0, 10, 10), "GEOMETRYCOLLECTION ()"],
                 ["LINESTRING (0 3,0 5,2 7)", QgsRectangle(0, 0, 10, 10), "LINESTRING (0 5,2 7)"],
                 ["LINESTRING (2 1,0 0,1 2)", QgsRectangle(0, 0, 10, 10), "LINESTRING (2 1,0 0,1 2)"],
                 ["LINESTRING (3 3,0 3,0 5,2 7)", QgsRectangle(0, 0, 10, 10), "MULTILINESTRING ((3 3,0 3),(0 5,2 7))"],
                 ["LINESTRING (5 5,10 5,20 5)", QgsRectangle(0, 0, 10, 10), "LINESTRING (5 5,10 5)"],
                 ["LINESTRING (3 3,0 6,3 9)", QgsRectangle(0, 0, 10, 10), "LINESTRING (3 3,0 6,3 9)"],
                 ["POLYGON ((5 5,5 6,6 6,6 5,5 5))", QgsRectangle(0, 0, 10, 10), "POLYGON ((5 5,5 6,6 6,6 5,5 5))"],
                 ["POLYGON ((15 15,15 16,16 16,16 15,15 15))", QgsRectangle(0, 0, 10, 10), "GEOMETRYCOLLECTION ()"],
                 ["POLYGON ((-1 -1,-1 11,11 11,11 -1,-1 -1))", QgsRectangle(0, 0, 10, 10), "Polygon ((0 0, 0 10, 10 10, 10 0, 0 0))"],
                 ["POLYGON ((-1 -1,-1 5,5 5,5 -1,-1 -1))", QgsRectangle(0, 0, 10, 10), "Polygon ((0 0, 0 5, 5 5, 5 0, 0 0))"],
                 ["POLYGON ((-2 -2,-2 5,5 5,5 -2,-2 -2), (3 3,4 4,4 2,3 3))", QgsRectangle(0, 0, 10, 10), "Polygon ((0 0, 0 5, 5 5, 5 0, 0 0),(3 3, 4 4, 4 2, 3 3))"]
                 ]
        for t in tests:
            input = QgsGeometry.fromWkt(t[0])
            o = input.clipped(t[1])
            exp = t[2]
            result = o.exportToWkt()
            self.assertTrue(compareWkt(result, exp, 0.00001),
                            "clipped: mismatch Expected:\n{}\nGot:\n{}\n".format(exp, result))


if __name__ == '__main__':
    unittest.main()
