# -*- coding: utf-8 -*-

"""
***************************************************************************
    AlgorithmsTest.py
    ---------------------
    Date                 : January 2016
    Copyright            : (C) 2016 by Matthias Kuhn
    Email                : matthias@opengis.ch
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Matthias Kuhn'
__date__ = 'January 2016'
__copyright__ = '(C) 2016, Matthias Kuhn'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = ':%H$'


import qgis  # NOQA switch sip api

import os
import yaml
import nose2
import gdal
import shutil
import glob
import hashlib
import tempfile

from osgeo.gdalconst import GA_ReadOnly
from numpy import nan_to_num
from copy import deepcopy

import processing

from processing.script.ScriptAlgorithm import ScriptAlgorithm  # NOQA

from processing.modeler.ModelerAlgorithmProvider import ModelerAlgorithmProvider  # NOQA
from processing.algs.qgis.QGISAlgorithmProvider import QGISAlgorithmProvider  # NOQA
#from processing.algs.grass7.Grass7AlgorithmProvider import Grass7AlgorithmProvider  # NOQA
#from processing.algs.gdal.GdalAlgorithmProvider import GdalAlgorithmProvider  # NOQA
#from processing.algs.saga.SagaAlgorithmProvider import SagaAlgorithmProvider  # NOQA
from processing.script.ScriptAlgorithmProvider import ScriptAlgorithmProvider  # NOQA
#from processing.preconfigured.PreconfiguredAlgorithmProvider import PreconfiguredAlgorithmProvider  # NOQA


from qgis.core import (QgsVectorLayer,
                       QgsRasterLayer,
                       QgsFeatureRequest,
                       QgsMapLayer,
                       QgsProject,
                       QgsApplication,
                       QgsProcessingContext,
                       QgsProcessingUtils,
                       QgsProcessingFeedback)

from qgis.testing import _UnexpectedSuccess

from utilities import unitTestDataPath


def processingTestDataPath():
    return os.path.join(os.path.dirname(__file__), 'testdata')


class AlgorithmsTest(object):

    def test_algorithms(self):
        """
        This is the main test function. All others will be executed based on the definitions in testdata/algorithm_tests.yaml
        """
        with open(os.path.join(processingTestDataPath(), self.test_definition_file()), 'r') as stream:
            algorithm_tests = yaml.load(stream)

        if 'tests' in algorithm_tests and algorithm_tests['tests'] is not None:
            for algtest in algorithm_tests['tests']:
                yield self.check_algorithm, algtest['name'], algtest

    def check_algorithm(self, name, defs):
        """
        Will run an algorithm definition and check if it generates the expected result
        :param name: The identifier name used in the test output heading
        :param defs: A python dict containing a test algorithm definition
        """
        self.vector_layer_params = {}
        QgsProject.instance().removeAllMapLayers()

        params = self.load_params(defs['params'])

        if defs['algorithm'].startswith('script:'):
            filePath = os.path.join(processingTestDataPath(), 'scripts', '{}.py'.format(defs['algorithm'][len('script:'):]))
            alg = ScriptAlgorithm(filePath)
            alg.initAlgorithm()
        else:
            alg = QgsApplication.processingRegistry().createAlgorithmById(defs['algorithm'])

        parameters = {}
        if isinstance(params, list):
            for param in zip(alg.parameterDefinitions(), params):
                parameters[param[0].name()] = param[1]
        else:
            for k, p in list(params.items()):
                parameters[k] = p

        for r, p in list(defs['results'].items()):
            if not 'in_place_result' in p or not p['in_place_result']:
                parameters[r] = self.load_result_param(p)

        expectFailure = False
        if 'expectedFailure' in defs:
            exec(('\n'.join(defs['expectedFailure'][:-1])), globals(), locals())
            expectFailure = eval(defs['expectedFailure'][-1])

        # ignore user setting for invalid geometry handling
        context = QgsProcessingContext()
        context.setProject(QgsProject.instance())

        if 'skipInvalid' in defs and defs['skipInvalid']:
            context.setInvalidGeometryCheck(QgsFeatureRequest.GeometrySkipInvalid)

        feedback = QgsProcessingFeedback()

        if expectFailure:
            try:
                results, ok = alg.run(parameters, context, feedback)
                self.check_results(results, context, defs['params'], defs['results'])
                if ok:
                    raise _UnexpectedSuccess
            except Exception:
                pass
        else:
            results, ok = alg.run(parameters, context, feedback)
            self.assertTrue(ok, 'params: {}, results: {}'.format(parameters, results))
            self.check_results(results, context, defs['params'], defs['results'])

    def load_params(self, params):
        """
        Loads an array of parameters
        """
        if isinstance(params, list):
            return [self.load_param(p) for p in params]
        elif isinstance(params, dict):
            return {key: self.load_param(p, key) for key, p in list(params.items())}
        else:
            return params

    def load_param(self, param, id=None):
        """
        Loads a parameter. If it's not a map, the parameter will be returned as-is. If it is a map, it will process the
        parameter based on its key `type` and return the appropriate parameter to pass to the algorithm.
        """
        try:
            if param['type'] in ('vector', 'raster', 'table'):
                return self.load_layer(id, param).id()
            elif param['type'] == 'multi':
                return [self.load_param(p) for p in param['params']]
            elif param['type'] == 'file':
                return self.filepath_from_param(param)
            elif param['type'] == 'interpolation':
                prefix = processingTestDataPath()
                tmp = ''
                for r in param['name'].split(';'):
                    v = r.split(',')
                    tmp += '{},{},{},{};'.format(os.path.join(prefix, v[0]),
                                                 v[1], v[2], v[3])
                return tmp[:-1]
        except TypeError:
            # No type specified, use whatever is there
            return param

        raise KeyError("Unknown type '{}' specified for parameter".format(param['type']))

    def load_result_param(self, param):
        """
        Loads a result parameter. Creates a temporary destination where the result should go to and returns this location
        so it can be sent to the algorithm as parameter.
        """
        if param['type'] in ['vector', 'file', 'table', 'regex']:
            outdir = tempfile.mkdtemp()
            self.cleanup_paths.append(outdir)
            if isinstance(param['name'], str):
                basename = os.path.basename(param['name'])
            else:
                basename = os.path.basename(param['name'][0])
            filepath = os.path.join(outdir, basename)
            return filepath
        elif param['type'] == 'rasterhash':
            outdir = tempfile.mkdtemp()
            self.cleanup_paths.append(outdir)
            basename = 'raster.tif'
            filepath = os.path.join(outdir, basename)
            return filepath

        raise KeyError("Unknown type '{}' specified for parameter".format(param['type']))

    def load_layers(self, id, param):
        layers = []
        if param['type'] in ('vector', 'table') and isinstance(param['name'], str):
            layers.append(self.load_layer(id, param))
        elif param['type'] in ('vector', 'table'):
            for n in param['name']:
                layer_param = deepcopy(param)
                layer_param['name'] = n
                layers.append(self.load_layer(id, layer_param))
        else:
            layers.append(self.load_layer(id, param))
        return layers

    def load_layer(self, id, param):
        """
        Loads a layer which was specified as parameter.
        """
        filepath = self.filepath_from_param(param)

        if 'in_place' in param and param['in_place']:
            # check if alg modifies layer in place
            tmpdir = tempfile.mkdtemp()
            self.cleanup_paths.append(tmpdir)
            path, file_name = os.path.split(filepath)
            base, ext = os.path.splitext(file_name)
            for file in glob.glob(os.path.join(path, '{}.*'.format(base))):
                shutil.copy(os.path.join(path, file), tmpdir)
            filepath = os.path.join(tmpdir, file_name)
            self.in_place_layers[id] = filepath

        if param['type'] in ('vector', 'table'):
            if filepath in self.vector_layer_params:
                return self.vector_layer_params[filepath]

            lyr = QgsVectorLayer(filepath, param['name'], 'ogr', False)
            self.vector_layer_params[filepath] = lyr
        elif param['type'] == 'raster':
            lyr = QgsRasterLayer(filepath, param['name'], 'gdal', False)

        self.assertTrue(lyr.isValid(), 'Could not load layer "{}" from param {}'.format(filepath, param))
        QgsProject.instance().addMapLayer(lyr)
        return lyr

    def filepath_from_param(self, param):
        """
        Creates a filepath from a param
        """
        prefix = processingTestDataPath()
        if 'location' in param and param['location'] == 'qgs':
            prefix = unitTestDataPath()

        return os.path.join(prefix, param['name'])

    def check_results(self, results, context, params, expected):
        """
        Checks if result produced by an algorithm matches with the expected specification.
        """
        for id, expected_result in list(expected.items()):
            if expected_result['type'] in ('vector', 'table'):
                if 'compare' in expected_result and not expected_result['compare']:
                    # skipping the comparison, so just make sure output is valid
                    if isinstance(results[id], QgsMapLayer):
                        result_lyr = results[id]
                    else:
                        result_lyr = QgsProcessingUtils.mapLayerFromString(results[id], context)
                    self.assertTrue(result_lyr.isValid())
                    continue

                expected_lyrs = self.load_layers(id, expected_result)
                if 'in_place_result' in expected_result:
                    result_lyr = QgsProcessingUtils.mapLayerFromString(self.in_place_layers[id], context)
                    self.assertTrue(result_lyr.isValid(), self.in_place_layers[id])
                else:
                    try:
                        results[id]
                    except KeyError as e:
                        raise KeyError('Expected result {} does not exist in {}'.format(str(e), list(results.keys())))

                    if isinstance(results[id], QgsMapLayer):
                        result_lyr = results[id]
                    else:
                        result_lyr = QgsProcessingUtils.mapLayerFromString(results[id], context)
                    self.assertTrue(result_lyr, results[id])

                compare = expected_result.get('compare', {})
                pk = expected_result.get('pk', None)

                if len(expected_lyrs) == 1:
                    self.assertLayersEqual(expected_lyrs[0], result_lyr, compare=compare, pk=pk)
                else:
                    res = False
                    for l in expected_lyrs:
                        if self.checkLayersEqual(l, result_lyr, compare=compare, pk=pk):
                            res = True
                            break
                    self.assertTrue(res, 'Could not find matching layer in expected results')

            elif 'rasterhash' == expected_result['type']:
                print("id:{} result:{}".format(id, results[id]))
                dataset = gdal.Open(results[id], GA_ReadOnly)
                dataArray = nan_to_num(dataset.ReadAsArray(0))
                strhash = hashlib.sha224(dataArray.data).hexdigest()

                if not isinstance(expected_result['hash'], str):
                    self.assertIn(strhash, expected_result['hash'])
                else:
                    self.assertEqual(strhash, expected_result['hash'])
            elif 'file' == expected_result['type']:
                expected_filepath = self.filepath_from_param(expected_result)
                result_filepath = results[id]

                self.assertFilesEqual(expected_filepath, result_filepath)
            elif 'regex' == expected_result['type']:
                with open(results[id], 'r') as file:
                    data = file.read()

                for rule in expected_result.get('rules', []):
                    self.assertRegex(data, rule)


if __name__ == '__main__':
    nose2.main()
