# -*- coding: utf-8 -*-
"""QGIS Unit tests for the OGR/GPKG provider.

.. note:: This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
"""
__author__ = 'Even Rouault'
__date__ = '2016-04-21'
__copyright__ = 'Copyright 2016, Even Rouault'
# This will get replaced with a git SHA1 when you do a git archive
__revision__ = '$Format:%H$'

import qgis  # NOQA

import os
import tempfile
import shutil
from osgeo import gdal, ogr

from qgis.core import QgsVectorLayer, QgsVectorLayerExporter, QgsFeature, QgsGeometry, QgsRectangle, QgsSettings
from qgis.PyQt.QtCore import QCoreApplication
from qgis.testing import start_app, unittest


def GDAL_COMPUTE_VERSION(maj, min, rev):
    return ((maj) * 1000000 + (min) * 10000 + (rev) * 100)


class ErrorReceiver():

    def __init__(self):
        self.msg = None

    def receiveError(self, msg):
        self.msg = msg


class TestPyQgsOGRProviderGpkg(unittest.TestCase):

    @classmethod
    def setUpClass(cls):
        """Run before all tests"""

        QCoreApplication.setOrganizationName("QGIS_Test")
        QCoreApplication.setOrganizationDomain("TestPyQgsOGRProviderGpkg.com")
        QCoreApplication.setApplicationName("TestPyQgsOGRProviderGpkg")
        QgsSettings().clear()
        start_app()

        # Create test layer
        cls.basetestpath = tempfile.mkdtemp()

    @classmethod
    def tearDownClass(cls):
        """Run after all tests"""
        shutil.rmtree(cls.basetestpath, True)

        QgsSettings().clear()

    def testSingleToMultiPolygonPromotion(self):

        tmpfile = os.path.join(self.basetestpath, 'testSingleToMultiPolygonPromotion.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        ds.CreateLayer('test', geom_type=ogr.wkbMultiPolygon)
        ds = None

        vl = QgsVectorLayer('{}|layerid=0'.format(tmpfile), 'test', 'ogr')
        f = QgsFeature()
        f.setGeometry(QgsGeometry.fromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
        vl.dataProvider().addFeatures([f])
        got = [feat for feat in vl.getFeatures()][0]
        got_geom = got.geometry()
        reference = QgsGeometry.fromWkt('MultiPolygon (((0 0, 0 1, 1 1, 0 0)))')
        # The geometries must be binarily identical
        self.assertEqual(got_geom.exportToWkb(), reference.exportToWkb(), 'Expected {}, got {}'.format(reference.exportToWkt(), got_geom.exportToWkt()))

    def testCurveGeometryType(self):

        tmpfile = os.path.join(self.basetestpath, 'testCurveGeometryType.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        ds.CreateLayer('test', geom_type=ogr.wkbCurvePolygon)
        ds = None

        vl = QgsVectorLayer('{}'.format(tmpfile), 'test', 'ogr')
        self.assertEqual(vl.dataProvider().subLayers(), ['0:test:0:CurvePolygon'])
        f = QgsFeature()
        f.setGeometry(QgsGeometry.fromWkt('POLYGON ((0 0,0 1,1 1,0 0))'))
        vl.dataProvider().addFeatures([f])
        got = [feat for feat in vl.getFeatures()][0]
        got_geom = got.geometry()
        reference = QgsGeometry.fromWkt('CurvePolygon (((0 0, 0 1, 1 1, 0 0)))')
        # The geometries must be binarily identical
        self.assertEqual(got_geom.exportToWkb(), reference.exportToWkb(), 'Expected {}, got {}'.format(reference.exportToWkt(), got_geom.exportToWkt()))

    def internalTestBug15351(self, orderClosing):

        tmpfile = os.path.join(self.basetestpath, 'testBug15351.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
        f = None
        ds = None

        vl = QgsVectorLayer(u'{}'.format(tmpfile), u'test', u'ogr')
        self.assertTrue(vl.startEditing())
        self.assertTrue(vl.changeGeometry(1, QgsGeometry.fromWkt('Point (3 50)')))

        # Iterate over features (will open a new OGR connection), but do not
        # close the iterator for now
        it = vl.getFeatures()
        f = QgsFeature()
        it.nextFeature(f)

        if orderClosing == 'closeIter_commit_closeProvider':
            it = None

        # Commit changes
        cbk = ErrorReceiver()
        vl.dataProvider().raiseError.connect(cbk.receiveError)
        self.assertTrue(vl.commitChanges())
        self.assertIsNone(cbk.msg)

        # Close layer and iterator in different orders
        if orderClosing == 'closeIter_commit_closeProvider':
            vl = None
        elif orderClosing == 'commit_closeProvider_closeIter':
            vl = None
            it = None
        else:
            assert orderClosing == 'commit_closeIter_closeProvider'
            it = None
            vl = None

        # Test that we succeeded restoring default journal mode, and we
        # are not let in WAL mode.
        ds = ogr.Open(tmpfile)
        lyr = ds.ExecuteSQL('PRAGMA journal_mode')
        f = lyr.GetNextFeature()
        res = f.GetField(0)
        ds.ReleaseResultSet(lyr)
        ds = None
        self.assertEqual(res, 'delete')

    # We need GDAL 2.0 to issue PRAGMA journal_mode
    # Note: for that case, we don't strictly need turning on WAL
    def testBug15351_closeIter_commit_closeProvider(self):
        self.internalTestBug15351('closeIter_commit_closeProvider')

    # We need GDAL 2.0 to issue PRAGMA journal_mode
    def testBug15351_commit_closeProvider_closeIter(self):
        self.internalTestBug15351('commit_closeProvider_closeIter')

    # We need GDAL 2.0 to issue PRAGMA journal_mode
    def testBug15351_commit_closeIter_closeProvider(self):
        self.internalTestBug15351('commit_closeIter_closeProvider')

    @unittest.skip(int(gdal.VersionInfo('VERSION_NUM')) < GDAL_COMPUTE_VERSION(2, 1, 2))
    def testGeopackageExtentUpdate(self):
        ''' test https://issues.qgis.org/issues/15273 '''
        tmpfile = os.path.join(self.basetestpath, 'testGeopackageExtentUpdate.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
        lyr.CreateFeature(f)
        f = None
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 0.5)'))
        lyr.CreateFeature(f)
        f = None
        gdal.ErrorReset()
        ds.ExecuteSQL('RECOMPUTE EXTENT ON test')
        has_error = gdal.GetLastErrorMsg() != ''
        ds = None
        if has_error:
            print('Too old GDAL trunk version. Please update')
            return

        vl = QgsVectorLayer(u'{}'.format(tmpfile), u'test', u'ogr')

        # Test moving a geometry that touches the bbox
        self.assertTrue(vl.startEditing())
        self.assertTrue(vl.changeGeometry(1, QgsGeometry.fromWkt('Point (0.5 0)')))
        self.assertTrue(vl.commitChanges())
        reference = QgsGeometry.fromRect(QgsRectangle(0.5, 0.0, 1.0, 1.0))
        provider_extent = QgsGeometry.fromRect(vl.extent())
        self.assertTrue(QgsGeometry.compare(provider_extent.asPolygon()[0], reference.asPolygon()[0], 0.00001),
                        provider_extent.asPolygon()[0])

        # Test deleting a geometry that touches the bbox
        self.assertTrue(vl.startEditing())
        self.assertTrue(vl.deleteFeature(2))
        self.assertTrue(vl.commitChanges())
        reference = QgsGeometry.fromRect(QgsRectangle(0.5, 0.0, 1.0, 0.5))
        provider_extent = QgsGeometry.fromRect(vl.extent())
        self.assertTrue(QgsGeometry.compare(provider_extent.asPolygon()[0], reference.asPolygon()[0], 0.00001),
                        provider_extent.asPolygon()[0])

    def testSelectSubsetString(self):

        tmpfile = os.path.join(self.basetestpath, 'testSelectSubsetString.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbMultiPolygon)
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['foo'] = 'bar'
        lyr.CreateFeature(f)
        f = None
        f = ogr.Feature(lyr.GetLayerDefn())
        f['foo'] = 'baz'
        lyr.CreateFeature(f)
        f = None
        ds = None

        vl = QgsVectorLayer('{}|layerid=0'.format(tmpfile), 'test', 'ogr')
        vl.setSubsetString("SELECT fid, foo FROM test WHERE foo = 'baz'")
        got = [feat for feat in vl.getFeatures()]
        self.assertEqual(len(got), 1)

    def testStyle(self):

        # First test with invalid URI
        vl = QgsVectorLayer('/idont/exist.gpkg', 'test', 'ogr')

        self.assertFalse(vl.dataProvider().isSaveAndLoadStyleToDatabaseSupported())

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, -1)
        self.assertEqual(idlist, [])
        self.assertEqual(namelist, [])
        self.assertEqual(desclist, [])
        self.assertNotEqual(errmsg, "")

        qml, errmsg = vl.getStyleFromDatabase("1")
        self.assertEqual(qml, "")
        self.assertNotEqual(errmsg, "")

        qml, success = vl.loadNamedStyle('/idont/exist.gpkg')
        self.assertFalse(success)

        errorMsg = vl.saveStyleToDatabase("name", "description", False, "")
        self.assertNotEqual(errorMsg, "")

        # Now with valid URI
        tmpfile = os.path.join(self.basetestpath, 'testStyle.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbMultiPolygon)
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['foo'] = 'bar'
        lyr.CreateFeature(f)
        f = None
        lyr = ds.CreateLayer('test2', geom_type=ogr.wkbMultiPolygon)
        lyr.CreateField(ogr.FieldDefn('foo', ogr.OFTString))
        f = ogr.Feature(lyr.GetLayerDefn())
        f['foo'] = 'bar'
        lyr.CreateFeature(f)
        f = None
        ds = None

        vl = QgsVectorLayer('{}|layername=test'.format(tmpfile), 'test', 'ogr')
        self.assertTrue(vl.isValid())

        vl2 = QgsVectorLayer('{}|layername=test2'.format(tmpfile), 'test2', 'ogr')
        self.assertTrue(vl2.isValid())

        self.assertTrue(vl.dataProvider().isSaveAndLoadStyleToDatabaseSupported())

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, 0)
        self.assertEqual(idlist, [])
        self.assertEqual(namelist, [])
        self.assertEqual(desclist, [])
        self.assertNotEqual(errmsg, "")

        qml, errmsg = vl.getStyleFromDatabase("not_existing")
        self.assertEqual(qml, "")
        self.assertNotEqual(errmsg, "")

        qml, success = vl.loadNamedStyle('{}|layerid=0'.format(tmpfile))
        self.assertFalse(success)

        errorMsg = vl.saveStyleToDatabase("name", "description", False, "")
        self.assertEqual(errorMsg, "")

        qml, errmsg = vl.getStyleFromDatabase("not_existing")
        self.assertEqual(qml, "")
        self.assertNotEqual(errmsg, "")

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, 1)
        self.assertEqual(errmsg, "")
        self.assertEqual(idlist, ['1'])
        self.assertEqual(namelist, ['name'])
        self.assertEqual(desclist, ['description'])

        qml, errmsg = vl.getStyleFromDatabase("100")
        self.assertEqual(qml, "")
        self.assertNotEqual(errmsg, "")

        qml, errmsg = vl.getStyleFromDatabase("1")
        self.assertTrue(qml.startswith('<!DOCTYPE qgis'), qml)
        self.assertEqual(errmsg, "")

        # Try overwrite it but simulate answer no
        settings = QgsSettings()
        settings.setValue("/qgis/overwriteStyle", False)
        errorMsg = vl.saveStyleToDatabase("name", "description_bis", False, "")
        self.assertNotEqual(errorMsg, "")

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, 1)
        self.assertEqual(errmsg, "")
        self.assertEqual(idlist, ['1'])
        self.assertEqual(namelist, ['name'])
        self.assertEqual(desclist, ['description'])

        # Try overwrite it and simulate answer yes
        settings = QgsSettings()
        settings.setValue("/qgis/overwriteStyle", True)
        errorMsg = vl.saveStyleToDatabase("name", "description_bis", False, "")
        self.assertEqual(errorMsg, "")

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, 1)
        self.assertEqual(errmsg, "")
        self.assertEqual(idlist, ['1'])
        self.assertEqual(namelist, ['name'])
        self.assertEqual(desclist, ['description_bis'])

        errorMsg = vl2.saveStyleToDatabase("name_test2", "description_test2", True, "")
        self.assertEqual(errorMsg, "")

        errorMsg = vl.saveStyleToDatabase("name2", "description2", True, "")
        self.assertEqual(errorMsg, "")

        errorMsg = vl.saveStyleToDatabase("name3", "description3", True, "")
        self.assertEqual(errorMsg, "")

        related_count, idlist, namelist, desclist, errmsg = vl.listStylesInDatabase()
        self.assertEqual(related_count, 3)
        self.assertEqual(errmsg, "")
        self.assertEqual(idlist, ['1', '3', '4', '2'])
        self.assertEqual(namelist, ['name', 'name2', 'name3', 'name_test2'])
        self.assertEqual(desclist, ['description_bis', 'description2', 'description3', 'name_test2'])

        # Check that layers_style table is not list in subLayers()
        vl = QgsVectorLayer(tmpfile, 'test', 'ogr')
        sublayers = vl.dataProvider().subLayers()
        self.assertEqual(len(sublayers), 2, sublayers)

    def testDisablewalForSqlite3(self):
        ''' Test disabling walForSqlite3 setting '''
        QgsSettings().setValue("/qgis/walForSqlite3", False)

        tmpfile = os.path.join(self.basetestpath, 'testDisablewalForSqlite3.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('test', geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('attr0', ogr.OFTInteger))
        lyr.CreateField(ogr.FieldDefn('attr1', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
        f = None
        ds = None

        vl = QgsVectorLayer(u'{}'.format(tmpfile), u'test', u'ogr')

        # Test that we are using default delete mode and not WAL
        ds = ogr.Open(tmpfile)
        lyr = ds.ExecuteSQL('PRAGMA journal_mode')
        f = lyr.GetNextFeature()
        res = f.GetField(0)
        ds.ReleaseResultSet(lyr)
        ds = None
        self.assertEqual(res, 'delete')

        self.assertTrue(vl.startEditing())
        feature = next(vl.getFeatures())
        self.assertTrue(vl.changeAttributeValue(feature.id(), 1, 1001))

        # Commit changes
        cbk = ErrorReceiver()
        vl.dataProvider().raiseError.connect(cbk.receiveError)
        self.assertTrue(vl.commitChanges())
        self.assertIsNone(cbk.msg)
        vl = None

        QgsSettings().setValue("/qgis/walForSqlite3", None)

    def testSimulatedDBManagerImport(self):
        uri = 'point?field=f1:int'
        uri += '&field=f2:double(6,4)'
        uri += '&field=f3:string(20)'
        lyr = QgsVectorLayer(uri, "x", "memory")
        self.assertTrue(lyr.isValid())
        f = QgsFeature(lyr.fields())
        f['f1'] = 1
        f['f2'] = 123.456
        f['f3'] = '12345678.90123456789'
        f2 = QgsFeature(lyr.fields())
        f2['f1'] = 2
        lyr.dataProvider().addFeatures([f, f2])

        tmpfile = os.path.join(self.basetestpath, 'testSimulatedDBManagerImport.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        ds = None
        options = {}
        options['update'] = True
        options['driverName'] = 'GPKG'
        options['layerName'] = 'my_out_table'
        err = QgsVectorLayerExporter.exportLayer(lyr, tmpfile, "ogr", lyr.crs(), False, options)
        self.assertEqual(err[0], QgsVectorLayerExporter.NoError,
                         'unexpected import error {0}'.format(err))
        lyr = QgsVectorLayer(tmpfile + "|layername=my_out_table", "y", "ogr")
        self.assertTrue(lyr.isValid())
        features = lyr.getFeatures()
        f = next(features)
        self.assertEqual(f['f1'], 1)
        self.assertEqual(f['f2'], 123.456)
        self.assertEqual(f['f3'], '12345678.90123456789')
        f = next(features)
        self.assertEqual(f['f1'], 2)
        features = None

        # Test overwriting without overwrite option
        err = QgsVectorLayerExporter.exportLayer(lyr, tmpfile, "ogr", lyr.crs(), False, options)
        self.assertEqual(err[0], QgsVectorLayerExporter.ErrCreateDataSource)

        # Test overwriting
        lyr = QgsVectorLayer(uri, "x", "memory")
        self.assertTrue(lyr.isValid())
        f = QgsFeature(lyr.fields())
        f['f1'] = 3
        lyr.dataProvider().addFeatures([f])
        options['overwrite'] = True
        err = QgsVectorLayerExporter.exportLayer(lyr, tmpfile, "ogr", lyr.crs(), False, options)
        self.assertEqual(err[0], QgsVectorLayerExporter.NoError,
                         'unexpected import error {0}'.format(err))
        lyr = QgsVectorLayer(tmpfile + "|layername=my_out_table", "y", "ogr")
        self.assertTrue(lyr.isValid())
        features = lyr.getFeatures()
        f = next(features)
        self.assertEqual(f['f1'], 3)
        features = None

    def testGeopackageTwoLayerEdition(self):
        ''' test https://issues.qgis.org/issues/17034 '''
        tmpfile = os.path.join(self.basetestpath, 'testGeopackageTwoLayerEdition.gpkg')
        ds = ogr.GetDriverByName('GPKG').CreateDataSource(tmpfile)
        lyr = ds.CreateLayer('layer1', geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('attr', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(0 0)'))
        lyr.CreateFeature(f)
        f = None
        lyr = ds.CreateLayer('layer2', geom_type=ogr.wkbPoint)
        lyr.CreateField(ogr.FieldDefn('attr', ogr.OFTInteger))
        f = ogr.Feature(lyr.GetLayerDefn())
        f.SetGeometry(ogr.CreateGeometryFromWkt('POINT(1 1)'))
        lyr.CreateFeature(f)
        f = None
        ds = None

        vl1 = QgsVectorLayer(u'{}'.format(tmpfile) + "|layername=layer1", u'layer1', u'ogr')
        vl2 = QgsVectorLayer(u'{}'.format(tmpfile) + "|layername=layer2", u'layer2', u'ogr')

        # Edit vl1, vl2 multiple times
        self.assertTrue(vl1.startEditing())
        self.assertTrue(vl2.startEditing())
        self.assertTrue(vl1.changeGeometry(1, QgsGeometry.fromWkt('Point (2 2)')))
        self.assertTrue(vl2.changeGeometry(1, QgsGeometry.fromWkt('Point (3 3)')))
        self.assertTrue(vl1.commitChanges())
        self.assertTrue(vl2.commitChanges())

        self.assertTrue(vl1.startEditing())
        self.assertTrue(vl2.startEditing())
        self.assertTrue(vl1.changeAttributeValue(1, 1, 100))
        self.assertTrue(vl2.changeAttributeValue(1, 1, 101))
        self.assertTrue(vl1.commitChanges())
        self.assertTrue(vl2.commitChanges())

        self.assertTrue(vl1.startEditing())
        self.assertTrue(vl2.startEditing())
        self.assertTrue(vl1.changeGeometry(1, QgsGeometry.fromWkt('Point (4 4)')))
        self.assertTrue(vl2.changeGeometry(1, QgsGeometry.fromWkt('Point (5 5)')))
        self.assertTrue(vl1.commitChanges())
        self.assertTrue(vl2.commitChanges())

        vl1 = None
        vl2 = None

        # Check everything is as expected after re-opening
        vl1 = QgsVectorLayer(u'{}'.format(tmpfile) + "|layername=layer1", u'layer1', u'ogr')
        vl2 = QgsVectorLayer(u'{}'.format(tmpfile) + "|layername=layer2", u'layer2', u'ogr')

        got = [feat for feat in vl1.getFeatures()][0]
        got_geom = got.geometry()
        self.assertEqual(got['attr'], 100)
        reference = QgsGeometry.fromWkt('Point (4 4)')
        self.assertEqual(got_geom.exportToWkb(), reference.exportToWkb(), 'Expected {}, got {}'.format(reference.exportToWkt(), got_geom.exportToWkt()))

        got = [feat for feat in vl2.getFeatures()][0]
        got_geom = got.geometry()
        self.assertEqual(got['attr'], 101)
        reference = QgsGeometry.fromWkt('Point (5 5)')
        self.assertEqual(got_geom.exportToWkb(), reference.exportToWkb(), 'Expected {}, got {}'.format(reference.exportToWkt(), got_geom.exportToWkt()))


if __name__ == '__main__':
    unittest.main()
