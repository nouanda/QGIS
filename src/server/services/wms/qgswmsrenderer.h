/***************************************************************************
                              qgswmsrendrer.h
                              -------------------
  begin                : May 14, 2006
  copyright            : (C) 2006 by Marco Hugentobler
                         (C) 2017 by David Marteau
  email                : marco dot hugentobler at karto dot baug dot ethz dot ch
                         david dot marteau at 3liz dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSWMSRENDERER_H
#define QGSWMSRENDERER_H

#include "qgswmsconfigparser.h"
#include "qgsserversettings.h"
#include "qgswmsparameters.h"
#include <QDomDocument>
#include <QMap>
#include <QPair>
#include <QString>
#include <map>

class QgsCapabilitiesCache;
class QgsCoordinateReferenceSystem;
class QgsComposition;
class QgsConfigParser;
class QgsFeature;
class QgsFeatureRenderer;
class QgsMapLayer;
class QgsMapSettings;
class QgsPointXY;
class QgsRasterLayer;
class QgsRasterRenderer;
class QgsRectangle;
class QgsRenderContext;
class QgsVectorLayer;
class QgsSymbol;
class QgsSymbol;
class QgsAccessControl;
class QgsDxfExport;
class QgsLayerTreeModel;
class QgsLayerTree;

class QColor;
class QFile;
class QFont;
class QImage;
class QPaintDevice;
class QPainter;
class QStandardItem;

/** This class handles requestsi that share rendering:
 *
 * This includes
 *  - getfeatureinfo
 *  - getmap
 *  - getlegendgraphics
 *  - getprint
 */

// These requests share common methods: putting them into a single helper class is
// more practical than spitting everything in a more functional way.

namespace QgsWms
{

  class QgsRenderer
  {
    public:

      /** Constructor. Does _NOT_ take ownership of
          QgsConfigParser and QgsCapabilitiesCache*/
      QgsRenderer( QgsServerInterface *serverIface,
                   const QgsProject *project,
                   const QgsServerRequest::Parameters &parameters,
                   QgsWmsConfigParser *parser );

      ~QgsRenderer();

      /** Returns the map legend as an image (or a null pointer in case of error). The caller takes ownership
      of the image object*/
      QImage *getLegendGraphics();

      typedef QSet<QString> SymbolSet;
      typedef QHash<QgsVectorLayer *, SymbolSet> HitTest;

      /** Returns the map as an image (or a null pointer in case of error). The caller takes ownership
      of the image object). If an instance to existing hit test structure is passed, instead of rendering
      it will fill the structure with symbols that would be used for rendering */
      QImage *getMap( HitTest *hitTest = nullptr );

      /** Identical to getMap( HitTest* hitTest ) and updates the map settings actually used.
        \since QGIS 3.0 */
      QImage *getMap( QgsMapSettings &mapSettings, HitTest *hitTest = nullptr );

      /** Returns the map as DXF data
       \param options: extracted from the FORMAT_OPTIONS parameter
       \returns the map as DXF data
       \since QGIS 3.0*/
      QgsDxfExport getDxf( const QMap<QString, QString> &options );

      /** Returns printed page as binary
        \param formatString out: format of the print output (e.g. pdf, svg, png, ...)
        \returns printed page as binary or 0 in case of error*/
      QByteArray *getPrint( const QString &formatString );

      /** Creates an xml document that describes the result of the getFeatureInfo request.
       * May throw an exception
       */
      QByteArray *getFeatureInfo( const QString &version = "1.3.0" );

    private:

      // Init the restricted layers with nicknames
      void initRestrictedLayers();

      // Build and returns highlight layers
      QList<QgsMapLayer *> highlightLayers();

      // Init a map with nickname for layers' project
      void initNicknameLayers();

      // Return the nickname of the layer (short name, id or name according to
      // the project configuration)
      QString layerNickname( const QgsMapLayer &layer ) const;

      // Return true if the layer has to be displayed according to he current
      // scale
      bool layerScaleVisibility( const QgsMapLayer &layer, double scaleDenominator ) const;

      // Remove unwanted layers (restricted, not visible, etc)
      void removeUnwantedLayers( QList<QgsMapLayer *> &layers, double scaleDenominator = -1 ) const;

      // Remove non identifiable layers (restricted, not visible, etc)
      void removeNonIdentifiableLayers( QList<QgsMapLayer *> &layers ) const;

      // Rendering step for layers
      QPainter *layersRendering( const QgsMapSettings &mapSettings, QImage &image, HitTest *hitTest = nullptr ) const;

      // Rendering step for annotations
      void annotationsRendering( QPainter *painter ) const;

      // Return a list of layers stylized with LAYERS/STYLES parameters
      QList<QgsMapLayer *> stylizedLayers( const QList<QgsWmsParametersLayer> &params ) const;

      // Return a list of layers stylized with SLD parameter
      QList<QgsMapLayer *> sldStylizedLayers( const QString &sld ) const;

      // Set layer opacity
      void setLayerOpacity( QgsMapLayer *layer, int opacity ) const;

      // Set layer filter
      void setLayerFilter( QgsMapLayer *layer, const QStringList &filter ) const;

      // Set layer python filter
      void setLayerAccessControlFilter( QgsMapLayer *layer ) const;

      // Set layer selection
      void setLayerSelection( QgsMapLayer *layer, const QStringList &fids ) const;

      // Combine map extent with layer extent
      void updateExtent( const QgsMapLayer *layer, QgsMapSettings &mapSettings ) const;

      // Scale image with WIDTH/HEIGHT if necessary
      QImage *scaleImage( const QImage *image ) const;

      // Check layer read permissions
      void checkLayerReadPermissions( QgsMapLayer *layer ) const;

      // Build a layer tree model for legend
      QgsLayerTreeModel *buildLegendTreeModel( const QList<QgsMapLayer *> &layers, double scaleDenominator, QgsLayerTree &rootGroup );

      // Returns default dots per mm
      qreal dotsPerMm() const;

      /** Initializes WMS layers and configures rendering.
       * \param layersList out: list with WMS layer names
       * \param stylesList out: list with WMS style names
       * \param layerIdList out: list with QGIS layer ids
       * \returns image configured. The calling function takes ownership of the image
       * may throw an exception
       */
      QImage *initializeRendering( QStringList &layersList, QStringList &stylesList, QStringList &layerIdList, QgsMapSettings &mapSettings );

      /** Creates a QImage from the HEIGHT and WIDTH parameters
       * \param width image width (or -1 if width should be taken from WIDTH wms parameter)
       * \param height image height (or -1 if height should be taken from HEIGHT wms parameter)
       * \param useBbox flag to indicate if the BBOX has to be used to adapt aspect ratio
       * \returns a non null pointer
       * may throw an exception
       */
      QImage *createImage( int width = -1, int height = -1, bool useBbox = true ) const;

      /** Configures mapSettings to the parameters
       * HEIGHT, WIDTH, BBOX, CRS.
       * \param paintDevice the device that is used for painting (for dpi)
       * may throw an exception
       */
      void configureMapSettings( const QPaintDevice *paintDevice, QgsMapSettings &mapSettings ) const;

      /**
       * If the parameter SLD exists, mSLDParser is configured appropriately. The lists are
       * set to the layer and style names according to the SLD
       * may throw an exception
       */
      void initializeSLDParser( QStringList &layersList, QStringList &stylesList );

      QDomDocument featureInfoDocument( QList<QgsMapLayer *> &layers, const QgsMapSettings &mapSettings,
                                        const QImage *outputImage, const QString &version ) const;

      /** Appends feature info xml for the layer to the layer element of the feature info dom document
      \param featureBBox the bounding box of the selected features in output CRS
      \returns true in case of success*/
      bool featureInfoFromVectorLayer( QgsVectorLayer *layer,
                                       const QgsPointXY *infoPoint,
                                       int nFeatures,
                                       QDomDocument &infoDocument,
                                       QDomElement &layerElement,
                                       const QgsMapSettings &mapSettings,
                                       QgsRenderContext &renderContext,
                                       const QString &version,
                                       QgsRectangle *featureBBox = nullptr,
                                       QgsGeometry *filterGeom = nullptr ) const;
      //! Appends feature info xml for the layer to the layer element of the dom document
      bool featureInfoFromRasterLayer( QgsRasterLayer *layer,
                                       const QgsMapSettings &mapSettings,
                                       const QgsPointXY *infoPoint,
                                       QDomDocument &infoDocument,
                                       QDomElement &layerElement,
                                       const QString &version ) const;

      /** Creates a layer set and returns a stringlist with layer ids that can be passed to a renderer. Usually used in conjunction with readLayersAndStyles
         \param scaleDenominator Filter out layer if scale based visibility does not match (or use -1 if no scale restriction)*/
      QStringList layerSet( const QStringList &layersList, const QStringList &stylesList, const QgsCoordinateReferenceSystem &destCRS, double scaleDenominator = -1 ) const;

      //! Record which symbols would be used if the map was in the current configuration of renderer. This is useful for content-based legend
      void runHitTest( const QgsMapSettings &mapSettings, HitTest &hitTest ) const;
      //! Record which symbols within one layer would be rendered with the given renderer context
      void runHitTestLayer( QgsVectorLayer *vl, SymbolSet &usedSymbols, QgsRenderContext &context ) const;

      /** Apply filter (subset) strings from the request to the layers. Example: '&FILTER=<layer1>:"AND property > 100",<layer2>:"AND bla = 'hallo!'" '
       * \param layerList list of layer IDs to filter
       * \param originalFilters hash of layer ID to original filter string
       * \note It is strongly recommended that this method be called alongside use of QgsOWSServerFilterRestorer
       * to ensure that the original filters are always correctly restored, regardless of whether exceptions
       * are thrown or functions are terminated early.
       */
      void applyRequestedLayerFilters( const QStringList &layerList, QgsMapSettings &mapSettings, QHash<QgsMapLayer *, QString> &originalFilters ) const;

#ifdef HAVE_SERVER_PYTHON_PLUGINS

      /** Apply filter strings from the access control to the layers.
       * \param layerList layers to filter
       * \param originalLayerFilters the original layers filter dictionary
       */
      void applyAccessControlLayersFilters( const QStringList &layerList, QHash<QgsMapLayer *, QString> &originalLayerFilters ) const;
#endif

      /** Tests if a filter sql string is allowed (safe)
        \returns true in case of success, false if string seems unsafe*/
      bool testFilterStringSafety( const QString &filter ) const;
      //! Helper function for filter safety test. Groups stringlist to merge entries starting/ending with quotes
      static void groupStringList( QStringList &list, const QString &groupString );

      /** Select vector features with ids specified in parameter SELECTED, e.g. ...&SELECTED=layer1:1,2,9;layer2:3,5,10&...
        \returns list with layer ids where selections have been created*/
      QStringList applyFeatureSelections( const QStringList &layerList ) const;
      //! Clear all feature selections in the given layers
      void clearFeatureSelections( const QStringList &layerIds ) const;

      //! Applies opacity on layer/group level
      void applyOpacities( const QStringList &layerList, QList< QPair< QgsVectorLayer *, QgsFeatureRenderer *> > &vectorRenderers,
                           QList< QPair< QgsRasterLayer *, QgsRasterRenderer * > > &rasterRenderers,
                           QList< QPair< QgsVectorLayer *, double > > &labelTransparencies,
                           QList< QPair< QgsVectorLayer *, double > > &labelBufferTransparencies
                         );

      //! Restore original opacities
      void restoreOpacities( QList< QPair <QgsVectorLayer *, QgsFeatureRenderer *> > &vectorRenderers,
                             QList< QPair < QgsRasterLayer *, QgsRasterRenderer * > > &rasterRenderers,
                             QList< QPair< QgsVectorLayer *, double > > &labelTransparencies,
                             QList< QPair< QgsVectorLayer *, double > > &labelBufferTransparencies );

      /** Checks WIDTH/HEIGHT values against MaxWidth and MaxHeight
        \returns true if width/height values are okay*/
      bool checkMaximumWidthHeight() const;

      //! Converts a feature info xml document to SIA2045 norm
      void convertFeatureInfoToSia2045( QDomDocument &doc ) const;

      //! Converts a feature info xml document to HTML
      QByteArray convertFeatureInfoToHtml( const QDomDocument &doc ) const;

      //! Converts a feature info xml document to Text
      QByteArray convertFeatureInfoToText( const QDomDocument &doc ) const;

      QDomElement createFeatureGML(
        QgsFeature *feat,
        QgsVectorLayer *layer,
        QDomDocument &doc,
        QgsCoordinateReferenceSystem &crs,
        const QgsMapSettings &mapSettings,
        const QString &typeName,
        bool withGeom,
        int version,
        QStringList *attributes = nullptr ) const;

      //! Replaces attribute value with ValueRelation or ValueRelation if defined. Otherwise returns the original value
      static QString replaceValueMapAndRelation( QgsVectorLayer *vl, int idx, const QString &attributeVal );
      //! Gets layer search rectangle (depending on request parameter, layer type, map and layer crs)
      QgsRectangle featureInfoSearchRect( QgsVectorLayer *ml, const QgsMapSettings &ms, const QgsRenderContext &rct, const QgsPointXY &infoPoint ) const;


    private:

      const QgsServerRequest::Parameters &mParameters;

      bool mOwnsConfigParser; //delete config parser after request (e.g. sent SLD)

      //! Map containing the WMS parameters
      QgsWmsConfigParser    *mConfigParser = nullptr;

      //! The access control helper
      QgsAccessControl *mAccessControl = nullptr;

      const QgsServerSettings &mSettings;
      const QgsProject *mProject = nullptr;
      QgsWmsParameters mWmsParameters;
      QStringList mRestrictedLayers;
      QMap<QString, QgsMapLayer *> mNicknameLayers;

    public:

      //! Return the image quality to use for getMap request
      int getImageQuality() const;

      //! Return precision to use for GetFeatureInfo request
      int getWMSPrecision() const;

  };

} // namespace QgsWms

#endif
