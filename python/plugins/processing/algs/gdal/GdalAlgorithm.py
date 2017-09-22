# -*- coding: utf-8 -*-

"""
***************************************************************************
    GdalAlgorithm.py
    ---------------------
    Date                 : August 2012
    Copyright            : (C) 2012 by Victor Olaya
    Email                : volayaf at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""

__author__ = 'Victor Olaya'
__date__ = 'August 2012'
__copyright__ = '(C) 2012, Victor Olaya'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

import os
import re

from qgis.PyQt.QtCore import QUrl, QCoreApplication

from qgis.core import (QgsApplication,
                       QgsVectorFileWriter,
                       QgsProcessingAlgorithm)

from processing.algs.gdal.GdalAlgorithmDialog import GdalAlgorithmDialog
from processing.algs.gdal.GdalUtils import GdalUtils

pluginPath = os.path.normpath(os.path.join(
    os.path.split(os.path.dirname(__file__))[0], os.pardir))


class GdalAlgorithm(QgsProcessingAlgorithm):

    def __init__(self):
        super().__init__()
        self.output_values = {}

    def icon(self):
        return QgsApplication.getThemeIcon("/providerGdal.svg")

    def svgIconPath(self):
        return QgsApplication.iconPath("providerGdal.svg")

    def createInstance(self, config={}):
        return self.__class__()

    def createCustomParametersWidget(self, parent):
        return GdalAlgorithmDialog(self)

    def getConsoleCommands(self, parameters, context, feedback):
        return None

    def getOgrCompatibleSource(self, parameter_name, parameters, context, feedback):
        """
        Interprets a parameter as an OGR compatible source and layer name
        """
        input_layer = self.parameterAsVectorLayer(parameters, parameter_name, context)
        ogr_data_path = None
        ogr_layer_name = None
        if input_layer is None:
            # parameter is not a vector layer - try to convert to a source compatible with OGR
            # and extract selection if required
            ogr_data_path = self.parameterAsCompatibleSourceLayerPath(parameters, parameter_name, context,
                                                                      QgsVectorFileWriter.supportedFormatExtensions(),
                                                                      feedback=feedback)
            ogr_layer_name = GdalUtils.ogrLayerName(ogr_data_path)
        elif input_layer.dataProvider().name() == 'ogr':
            # parameter is a vector layer, with OGR data provider
            # so extract selection if required
            ogr_data_path = self.parameterAsCompatibleSourceLayerPath(parameters, parameter_name, context,
                                                                      QgsVectorFileWriter.supportedFormatExtensions(),
                                                                      feedback=feedback)
            ogr_layer_name = GdalUtils.ogrLayerName(input_layer.dataProvider().dataSourceUri())
        else:
            # vector layer, but not OGR - get OGR compatible path
            # TODO - handle "selected features only" mode!!
            ogr_data_path = GdalUtils.ogrConnectionString(input_layer.dataProvider().dataSourceUri(), context)[1:-1]
            ogr_layer_name = GdalUtils.ogrLayerName(input_layer.dataProvider().dataSourceUri())
        return ogr_data_path, ogr_layer_name

    def setOutputValue(self, name, value):
        self.output_values[name] = value

    def processAlgorithm(self, parameters, context, feedback):
        commands = self.getConsoleCommands(parameters, context, feedback)
        GdalUtils.runGdal(commands, feedback)

        # auto generate outputs
        results = {}
        for o in self.outputDefinitions():
            if o.name() in parameters:
                results[o.name()] = parameters[o.name()]
        for k, v in self.output_values.items():
            results[k] = v

        return results

    def helpUrl(self):
        helpPath = GdalUtils.gdalHelpPath()
        if helpPath == '':
            return None

        if os.path.exists(helpPath):
            return QUrl.fromLocalFile(os.path.join(helpPath, '{}.html'.format(self.commandName()))).toString()
        else:
            return helpPath + '{}.html'.format(self.commandName())

    def commandName(self):
        parameters = {}
        for output in self.outputs:
            output.setValue("dummy")
        for param in self.parameterDefinitions():
            parameters[param.name()] = "1"
        name = self.getConsoleCommands(parameters)[0]
        if name.endswith(".py"):
            name = name[:-3]
        return name

    def tr(self, string, context=''):
        if context == '':
            context = self.__class__.__name__
        return QCoreApplication.translate(context, string)
