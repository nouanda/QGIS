# -*- coding: utf-8 -*-

"""
***************************************************************************
    ParameterGuiUtils.py
    ---------------------
    Date                 : June 2017
    Copyright            : (C) 2017 by Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
***************************************************************************
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
***************************************************************************
"""


__author__ = 'Nyall Dawson'
__date__ = 'June 2017'
__copyright__ = '(C) 2017, Nyall Dawson'

# This will get replaced with a git SHA1 when you do a git archive

__revision__ = '$Format:%H$'

from qgis.core import (QgsProcessing,
                       QgsProviderRegistry,
                       QgsProcessingFeatureSourceDefinition,
                       QgsVectorFileWriter)
from qgis.PyQt.QtCore import QCoreApplication
from processing.tools import dataobjects


def tr(string, context=''):
    if context == '':
        context = 'Processing'
    return QCoreApplication.translate(context, string)


def getFileFilter(param):
    """
    Returns a suitable file filter pattern for the specified parameter definition
    :param param:
    :return:
    """
    if param.type() == 'multilayer':
        if param.layerType() == QgsProcessing.TypeRaster:
            exts = dataobjects.getSupportedOutputRasterLayerExtensions()
        elif param.layerType() == QgsProcessing.TypeFile:
            return tr('All files (*.*)', 'QgsProcessingParameterMultipleLayers')
        else:
            exts = QgsVectorFileWriter.supportedFormatExtensions()
        for i in range(len(exts)):
            exts[i] = tr('{0} files (*.{1})', 'QgsProcessingParameterMultipleLayers').format(exts[i].upper(), exts[i].lower())
        return tr('All files (*.*)') + ';;' + ';;'.join(exts)
    elif param.type() == 'raster':
        return QgsProviderRegistry.instance().fileRasterFilters()
    elif param.type() == 'rasterDestination':
        exts = dataobjects.getSupportedOutputRasterLayerExtensions()
        for i in range(len(exts)):
            exts[i] = tr('{0} files (*.{1})', 'QgsProcessingParameterRasterDestination').format(exts[i].upper(), exts[i].lower())
        return tr('All files (*.*)') + ';;' + ';;'.join(exts)
    elif param.type() == 'table':
        exts = ['csv', 'dbf']
        for i in range(len(exts)):
            exts[i] = tr('{0} files (*.{1})', 'ParameterTable').format(exts[i].upper(), exts[i].lower())
        return tr('All files (*.*)') + ';;' + ';;'.join(exts)
    elif param.type() == 'sink':
        exts = QgsVectorFileWriter.supportedFormatExtensions()
        for i in range(len(exts)):
            exts[i] = tr('{0} files (*.{1})', 'ParameterVector').format(exts[i].upper(), exts[i].lower())
        return tr('All files (*.*)') + ';;' + ';;'.join(exts)
    elif param.type() == 'source':
        return QgsProviderRegistry.instance().fileVectorFilters()
    elif param.type() == 'vector':
        return QgsProviderRegistry.instance().fileVectorFilters()
    elif param.type() == 'fileOut':
        return tr('All files (*.*)') + ';;' + param.fileFilter()

    return ''
