/***************************************************************************
                          qgsmaplayer.h  -  description
                             -------------------
    begin                : Fri Jun 28 2002
    copyright            : (C) 2002 by Gary E.Sherman
    email                : sherman at mrcc.com
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSMAPLAYER_H
#define QGSMAPLAYER_H

#include "qgis_core.h"
#include <QDateTime>
#include <QDomNode>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QUndoStack>
#include <QVariant>

#include "qgis.h"
#include "qgserror.h"
#include "qgsobjectcustomproperties.h"
#include "qgsrectangle.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsrendercontext.h"
#include "qgsmaplayerdependency.h"
#include "qgslayermetadata.h"

class QgsDataProvider;
class QgsMapLayerLegend;
class QgsMapLayerRenderer;
class QgsMapLayerStyleManager;
class QgsReadWriteContext;
class QgsProject;

class QDomDocument;
class QKeyEvent;
class QPainter;

/** \ingroup core
 * Base class for all map layer types.
 * This is the base class for all map layer types (vector, raster).
 */
class CORE_EXPORT QgsMapLayer : public QObject
{
    Q_OBJECT

    Q_PROPERTY( QString name READ name WRITE setName NOTIFY nameChanged )
    Q_PROPERTY( int autoRefreshInterval READ autoRefreshInterval WRITE setAutoRefreshInterval NOTIFY autoRefreshIntervalChanged )
    Q_PROPERTY( QgsLayerMetadata metadata READ metadata WRITE setMetadata NOTIFY metadataChanged )

#ifdef SIP_RUN
    SIP_CONVERT_TO_SUBCLASS_CODE
    QgsMapLayer *layer = qobject_cast<QgsMapLayer *>( sipCpp );

    sipType = 0;

    if ( layer )
    {
      switch ( layer->type() )
      {
        case QgsMapLayer::VectorLayer:
          sipType = sipType_QgsVectorLayer;
          break;
        case QgsMapLayer::RasterLayer:
          sipType = sipType_QgsRasterLayer;
          break;
        case QgsMapLayer::PluginLayer:
          sipType = sipType_QgsPluginLayer;
          break;
        default:
          sipType = nullptr;
          break;
      }
    }
    SIP_END
#endif

  public:

    //! Types of layers that can be added to a map
    enum LayerType
    {
      VectorLayer,
      RasterLayer,
      PluginLayer
    };

    /** Constructor for QgsMapLayer
     * \param type layer type
     * \param name display name for the layer
     * \param source datasource of layer
     */
    QgsMapLayer( QgsMapLayer::LayerType type = VectorLayer, const QString &name = QString(), const QString &source = QString() );

    virtual ~QgsMapLayer();

    //! QgsMapLayer cannot be copied
    QgsMapLayer( QgsMapLayer const & ) = delete;
    //! QgsMapLayer cannot be copied
    QgsMapLayer &operator=( QgsMapLayer const & ) = delete;

    /** Returns a new instance equivalent to this one except for the id which
     *  is still unique.
     * \returns a new layer instance
     * \since QGIS 3.0
     */
    virtual QgsMapLayer *clone() const = 0;

    /** Returns the type of the layer.
     */
    QgsMapLayer::LayerType type() const;

    //! Returns the layer's unique ID, which is used to access this layer from QgsProject.
    QString id() const;

    /**
     * Set the display \a name of the layer.
     * \since QGIS 2.16
     * \see name()
     */
    void setName( const QString &name );

    /** Returns the display name of the layer.
     * \returns the layer name
     * \see setName()
     */
    QString name() const;

    /**
     * Returns the layer's data provider.
     */
    virtual QgsDataProvider *dataProvider();

    /**
     * Returns the layer's data provider in a const-correct manner
     * \note not available in Python bindings
     */
    virtual const QgsDataProvider *dataProvider() const SIP_SKIP;

    /** Returns the original name of the layer.
     */
    QString originalName() const;

    /** Sets the short name of the layer
     *  used by QGIS Server to identify the layer.
     * \returns the layer short name
     * \see shortName()
     */
    void setShortName( const QString &shortName ) { mShortName = shortName; }

    /** Returns the short name of the layer
     *  used by QGIS Server to identify the layer.
     * \see setShortName()
     */
    QString shortName() const { return mShortName; }

    /** Sets the title of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \see title()
     */
    void setTitle( const QString &title ) { mTitle = title; }

    /** Returns the title of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \returns the layer title
     * \see setTitle()
     */
    QString title() const { return mTitle; }

    /** Sets the abstract of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \returns the layer abstract
     * \see abstract()
     */
    void setAbstract( const QString &abstract ) { mAbstract = abstract; }

    /** Returns the abstract of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \returns the layer abstract
     * \see setAbstract()
     */
    QString abstract() const { return mAbstract; }

    /** Sets the keyword list of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \returns the layer keyword list
     * \see keywordList()
     */
    void setKeywordList( const QString &keywords ) { mKeywordList = keywords; }

    /** Returns the keyword list of the layer
     *  used by QGIS Server in GetCapabilities request.
     * \returns the layer keyword list
     * \see setKeywordList()
     */
    QString keywordList() const { return mKeywordList; }

    /* Layer dataUrl information */

    /** Sets the DataUrl of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  DataUrl is a a link to the underlying data represented by a particular layer.
     * \returns the layer DataUrl
     * \see dataUrl()
     */
    void setDataUrl( const QString &dataUrl ) { mDataUrl = dataUrl; }

    /** Returns the DataUrl of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  DataUrl is a a link to the underlying data represented by a particular layer.
     * \returns the layer DataUrl
     * \see setDataUrl()
     */
    QString dataUrl() const { return mDataUrl; }

    /** Sets the DataUrl format of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  DataUrl is a a link to the underlying data represented by a particular layer.
     * \returns the layer DataUrl format
     * \see dataUrlFormat()
     */
    void setDataUrlFormat( const QString &dataUrlFormat ) { mDataUrlFormat = dataUrlFormat; }

    /** Returns the DataUrl format of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  DataUrl is a a link to the underlying data represented by a particular layer.
     * \returns the layer DataUrl format
     * \see setDataUrlFormat()
     */
    QString dataUrlFormat() const { return mDataUrlFormat; }

    /* Layer attribution information */

    /** Sets the attribution of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  Attribution indicates the provider of a layer or collection of layers.
     * \returns the layer attribution
     * \see attribution()
     */
    void setAttribution( const QString &attrib ) { mAttribution = attrib; }

    /** Returns the attribution of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  Attribution indicates the provider of a layer or collection of layers.
     * \returns the layer attribution
     * \see setAttribution()
     */
    QString attribution() const { return mAttribution; }

    /** Sets the attribution URL of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  Attribution indicates the provider of a layer or collection of layers.
     * \returns the layer attribution URL
     * \see attributionUrl()
     */
    void setAttributionUrl( const QString &attribUrl ) { mAttributionUrl = attribUrl; }

    /** Returns the attribution URL of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  Attribution indicates the provider of a layer or collection of layers.
     * \returns the layer attribution URL
     * \see setAttributionUrl()
     */
    QString attributionUrl() const { return mAttributionUrl; }

    /* Layer metadataUrl information */

    /** Sets the metadata URL of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  MetadataUrl is a a link to the detailed, standardized metadata about the data.
     * \returns the layer metadata URL
     * \see metadataUrl()
     */
    void setMetadataUrl( const QString &metaUrl ) { mMetadataUrl = metaUrl; }

    /** Returns the metadata URL of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  MetadataUrl is a a link to the detailed, standardized metadata about the data.
     * \returns the layer metadata URL
     * \see setMetadataUrl()
     */
    QString metadataUrl() const { return mMetadataUrl; }

    /** Set the metadata type of the layer
     *  used by QGIS Server in GetCapabilities request
     *  MetadataUrlType indicates the standard to which the metadata complies.
     * \returns the layer metadata type
     * \see metadataUrlType()
     */
    void setMetadataUrlType( const QString &metaUrlType ) { mMetadataUrlType = metaUrlType; }

    /** Returns the metadata type of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  MetadataUrlType indicates the standard to which the metadata complies.
     * \returns the layer metadata type
     * \see setMetadataUrlType()
     */
    QString metadataUrlType() const { return mMetadataUrlType; }

    /** Sets the metadata format of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  MetadataUrlType indicates how the metadata is structured.
     * \returns the layer metadata format
     * \see metadataUrlFormat()
     */
    void setMetadataUrlFormat( const QString &metaUrlFormat ) { mMetadataUrlFormat = metaUrlFormat; }

    /** Returns the metadata format of the layer
     *  used by QGIS Server in GetCapabilities request.
     *  MetadataUrlType indicates how the metadata is structured.
     * \returns the layer metadata format
     * \see setMetadataUrlFormat()
     */
    QString metadataUrlFormat() const { return mMetadataUrlFormat; }

    /** Set the blending mode used for rendering a layer.
     * \param blendMode new blending mode
     * \see blendMode()
    */
    void setBlendMode( QPainter::CompositionMode blendMode );

    /** Returns the current blending mode for a layer.
     * \see setBlendMode()
    */
    QPainter::CompositionMode blendMode() const;

    //! Returns if this layer is read only.
    bool readOnly() const { return isReadOnly(); }

    /** Synchronises with changes in the datasource
        */
    virtual void reload() {}

    /** Return new instance of QgsMapLayerRenderer that will be used for rendering of given context
     * \since QGIS 2.4
     */
    virtual QgsMapLayerRenderer *createMapRenderer( QgsRenderContext &rendererContext ) = 0 SIP_FACTORY;

    //! Returns the extent of the layer.
    virtual QgsRectangle extent() const;

    /** Return the status of the layer. An invalid layer is one which has a bad datasource
     * or other problem. Child classes set this flag when initialized.
     * \returns true if the layer is valid and can be accessed
     */
    bool isValid() const;

    /** Gets a version of the internal layer definition that has sensitive
      *  bits removed (for example, the password). This function should
      * be used when displaying the source name for general viewing.
      * \see source()
     */
    QString publicSource() const;

    /** Returns the source for the layer. This source may contain usernames, passwords
     * and other sensitive information.
     * \see publicSource()
     */
    QString source() const;

    /**
     * Returns the sublayers of this layer.
     * (Useful for providers that manage their own layers, such as WMS).
     */
    virtual QStringList subLayers() const;

    /**
     * Reorders the *previously selected* sublayers of this layer from bottom to top.
     * (Useful for providers that manage their own layers, such as WMS).
     */
    virtual void setLayerOrder( const QStringList &layers );

    /** Set the visibility of the given sublayer name.
     * \param name sublayer name
     * \param visible sublayer visibility
    */
    virtual void setSubLayerVisibility( const QString &name, bool visible );

    //! Returns true if the layer can be edited.
    virtual bool isEditable() const;

    /** Returns true if the layer is considered a spatial layer, ie it has some form of geometry associated with it.
     * \since QGIS 2.16
     */
    virtual bool isSpatial() const;

    /** Sets state from Dom document
       \param layerElement The Dom element corresponding to ``maplayer'' tag
       \param context writing context (e.g. for conversion between relative and absolute paths)
       \note

       The Dom node corresponds to a Dom document project file XML element read
       by QgsProject.

       This, in turn, calls readXml(), which is over-rideable by sub-classes so
       that they can read their own specific state from the given Dom node.

       Invoked by QgsProject::read().

       \returns true if successful
     */
    bool readLayerXml( const QDomElement &layerElement, const QgsReadWriteContext &context );

    /** Stores state in Dom node
     * \param layerElement is a Dom element corresponding to ``maplayer'' tag
     * \param document is a the dom document being written
     * \param context reading context (e.g. for conversion between relative and absolute paths)
     * \note
     *
     * The Dom node corresponds to a Dom document project file XML element to be
     * written by QgsProject.
     *
     * This, in turn, calls writeXml(), which is over-rideable by sub-classes so
     * that they can write their own specific state to the given Dom node.
     *
     * Invoked by QgsProject::write().
     *
     * \returns true if successful
     */
    bool writeLayerXml( QDomElement &layerElement, QDomDocument &document, const QgsReadWriteContext &context ) const;

    /** Returns list of all keys within custom properties. Properties are stored in a map and saved in project file.
     * \see customProperty()
     * \since QGIS 3.0
     */
    QStringList customPropertyKeys() const;

    /** Set a custom property for layer. Properties are stored in a map and saved in project file.
     * \see customProperty()
     * \see removeCustomProperty()
     */
    void setCustomProperty( const QString &key, const QVariant &value );

    /** Read a custom property from layer. Properties are stored in a map and saved in project file.
     * \see setCustomProperty()
    */
    QVariant customProperty( const QString &value, const QVariant &defaultValue = QVariant() ) const;

    /** Set custom properties for layer. Current properties are dropped.
     * \since QGIS 3.0
     */
    void setCustomProperties( const QgsObjectCustomProperties &properties );

    /** Remove a custom property from layer. Properties are stored in a map and saved in project file.
     * \see setCustomProperty()
     */
    void removeCustomProperty( const QString &key );

    /** Get current status error. This error describes some principal problem
     *  for which layer cannot work and thus is not valid. It is not last error
     *  after accessing data by draw() etc.
     */
    virtual QgsError error() const;

    /** Returns the layer's spatial reference system.
    \since QGIS 1.4
     */
    QgsCoordinateReferenceSystem crs() const;

    //! Sets layer's spatial reference system
    void setCrs( const QgsCoordinateReferenceSystem &srs, bool emitSignal = true );

    //! A convenience function to (un)capitalize the layer name
    static QString capitalizeLayerName( const QString &name );

    /** Retrieve the style URI for this layer
     * (either as a .qml file on disk or as a
     * record in the users style table in their personal qgis.db)
     * \returns a QString with the style file name
     * \see also loadNamedStyle () and saveNamedStyle ();
     */
    virtual QString styleURI() const;

    /** Retrieve the default style for this layer if one
     * exists (either as a .qml file on disk or as a
     * record in the users style table in their personal qgis.db)
     * \param resultFlag a reference to a flag that will be set to false if
     * we did not manage to load the default style.
     * \returns a QString with any status messages
     * \see also loadNamedStyle ();
     */
    virtual QString loadDefaultStyle( bool &resultFlag SIP_OUT );

    /** Retrieve a named style for this layer if one
     * exists (either as a .qml file on disk or as a
     * record in the users style table in their personal qgis.db)
     * \param uri - the file name or other URI for the
     * style file. First an attempt will be made to see if this
     * is a file and load that, if that fails the qgis.db styles
     * table will be consulted to see if there is a style who's
     * key matches the URI.
     * \param resultFlag a reference to a flag that will be set to false if
     * we did not manage to load the default style.
     * \returns a QString with any status messages
     * \see also loadDefaultStyle ();
     */
    virtual QString loadNamedStyle( const QString &uri, bool &resultFlag SIP_OUT );

    /** Retrieve a named style for this layer from a sqlite database.
     * \param db path to sqlite database
     * \param uri uri for table
     * \param qml will be set to QML style content from database
     * \returns true if style was successfully loaded
     */
    virtual bool loadNamedStyleFromDatabase( const QString &db, const QString &uri, QString &qml SIP_OUT );

    /**
     * Import the properties of this layer from a QDomDocument
     * \param doc source QDomDocument
     * \param errorMsg this QString will be initialized on error
     * during the execution of readSymbology
     * \returns true on success
     * \since QGIS 2.8
     */
    virtual bool importNamedStyle( QDomDocument &doc, QString &errorMsg SIP_OUT );

    /**
     * Export the properties of this layer as named style in a QDomDocument
     * \param doc the target QDomDocument
     * \param errorMsg this QString will be initialized on error
     * during the execution of writeSymbology
     */
    virtual void exportNamedStyle( QDomDocument &doc, QString &errorMsg ) const;


    /**
     * Export the properties of this layer as SLD style in a QDomDocument
     * \param doc the target QDomDocument
     * \param errorMsg this QString will be initialized on error
     * during the execution of writeSymbology
     */
    virtual void exportSldStyle( QDomDocument &doc, QString &errorMsg ) const;

    /** Save the properties of this layer as the default style
     * (either as a .qml file on disk or as a
     * record in the users style table in their personal qgis.db)
     * \param resultFlag a reference to a flag that will be set to false if
     * we did not manage to save the default style.
     * \returns a QString with any status messages
     * \see loadNamedStyle() and \see saveNamedStyle()
     */
    virtual QString saveDefaultStyle( bool &resultFlag SIP_OUT );

    /** Save the properties of this layer as a named style
     * (either as a .qml file on disk or as a
     * record in the users style table in their personal qgis.db)
     * \param uri the file name or other URI for the
     * style file. First an attempt will be made to see if this
     * is a file and save to that, if that fails the qgis.db styles
     * table will be used to create a style entry who's
     * key matches the URI.
     * \param resultFlag a reference to a flag that will be set to false if
     * we did not manage to save the default style.
     * \returns a QString with any status messages
     * \see saveDefaultStyle()
     */
    virtual QString saveNamedStyle( const QString &uri, bool &resultFlag SIP_OUT );

    /** Saves the properties of this layer to an SLD format file.
     * \param uri uri of destination for exported SLD file.
     * \param resultFlag a reference to a flag that will be set to false if
     * the SLD file could not be generated
     * \returns a string with any status or error messages
     * \see loadSldStyle()
     */
    virtual QString saveSldStyle( const QString &uri, bool &resultFlag ) const;

    /** Attempts to style the layer using the formatting from an SLD type file.
     * \param uri uri of source SLD file
     * \param resultFlag a reference to a flag that will be set to false if
     * the SLD file could not be loaded
     * \returns a string with any status or error messages
     * \see saveSldStyle()
     */
    virtual QString loadSldStyle( const QString &uri, bool &resultFlag );

    virtual bool readSld( const QDomNode &node, QString &errorMessage )
    { Q_UNUSED( node ); errorMessage = QStringLiteral( "Layer type %1 not supported" ).arg( type() ); return false; }



    /** Read the symbology for the current layer from the Dom node supplied.
     * \param node node that will contain the symbology definition for this layer.
     * \param errorMessage reference to string that will be updated with any error messages
     * \param context reading context (used for transform from relative to absolute paths)
     * \returns true in case of success.
     */
    virtual bool readSymbology( const QDomNode &node, QString &errorMessage, const QgsReadWriteContext &context ) = 0;

    /** Read the style for the current layer from the Dom node supplied.
     * \param node node that will contain the style definition for this layer.
     * \param errorMessage reference to string that will be updated with any error messages
     * \param context reading context (used for transform from relative to absolute paths)
     * \returns true in case of success.
     * \since QGIS 2.16
     * \note To be implemented in subclasses. Default implementation does nothing and returns false.
     */
    virtual bool readStyle( const QDomNode &node, QString &errorMessage, const QgsReadWriteContext &context );

    /** Write the symbology for the layer into the docment provided.
     *  \param node the node that will have the style element added to it.
     *  \param doc the document that will have the QDomNode added.
     *  \param errorMessage reference to string that will be updated with any error messages
     *  \param context writing context (used for transform from absolute to relative paths)
     *  \returns true in case of success.
     */
    virtual bool writeSymbology( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsReadWriteContext &context ) const = 0;

    /** Write just the style information for the layer into the document
     *  \param node the node that will have the style element added to it.
     *  \param doc the document that will have the QDomNode added.
     *  \param errorMessage reference to string that will be updated with any error messages
     *  \param context writing context (used for transform from absolute to relative paths)
     *  \returns true in case of success.
     *  \since QGIS 2.16
     *  \note To be implemented in subclasses. Default implementation does nothing and returns false.
     */
    virtual bool writeStyle( QDomNode &node, QDomDocument &doc, QString &errorMessage, const QgsReadWriteContext &context ) const;

    //! Return pointer to layer's undo stack
    QUndoStack *undoStack();

    /** Return pointer to layer's style undo stack
     *  \since QGIS 2.16
     */
    QUndoStack *undoStackStyles();

    /**
     * Sets the URL for the layer's legend.
    */
    void setLegendUrl( const QString &legendUrl ) { mLegendUrl = legendUrl; }

    /**
     * Returns the URL for the layer's legend.
     */
    QString legendUrl() const { return mLegendUrl; }

    /**
     * Sets the format for a URL based layer legend.
     */
    void setLegendUrlFormat( const QString &legendUrlFormat ) { mLegendUrlFormat = legendUrlFormat; }

    /**
     * Returns the format for a URL based layer legend.
     */
    QString legendUrlFormat() const { return mLegendUrlFormat; }

    /**
     * Assign a legend controller to the map layer. The object will be responsible for providing legend items.
     * \param legend Takes ownership of the object. Can be null pointer
     * \since QGIS 2.6
     */
    void setLegend( QgsMapLayerLegend *legend SIP_TRANSFER );

    /**
     * Can be null.
     * \since QGIS 2.6
     */
    QgsMapLayerLegend *legend() const;

    /**
     * Get access to the layer's style manager. Style manager allows switching between multiple styles.
     * \since QGIS 2.8
     */
    QgsMapLayerStyleManager *styleManager() const;

    /** Tests whether the layer should be visible at the specified \a scale.
     *  The \a scale value indicates the scale denominator, e.g. 1000.0 for a 1:1000 map.
     * \returns true if the layer is visible at the given scale.
     * \since QGIS 2.16
     * \see minimumScale()
     * \see maximumScale()
     * \see hasScaleBasedVisibility()
     */
    bool isInScaleRange( double scale ) const;

    /**
     * Returns the minimum map scale (i.e. most "zoomed out" scale) at which the layer will be visible.
     * The scale value indicates the scale denominator, e.g. 1000.0 for a 1:1000 map.
     * A scale of 0 indicates no minimum scale visibility.
     * \note Scale based visibility is only used if setScaleBasedVisibility() is set to true.
     * \see setMinimumScale()
     * \see maximumScale()
     * \see hasScaleBasedVisibility()
     * \see isInScaleRange()
     */
    double minimumScale() const;

    /**
     * Returns the maximum map scale (i.e. most "zoomed in" scale) at which the layer will be visible.
     * The scale value indicates the scale denominator, e.g. 1000.0 for a 1:1000 map.
     * A scale of 0 indicates no maximum scale visibility.
     * \note Scale based visibility is only used if setScaleBasedVisibility() is set to true.
     * \see setMaximumScale()
     * \see minimumScale()
     * \see hasScaleBasedVisibility()
     * \see isInScaleRange()
     */
    double maximumScale() const;

    /** Returns whether scale based visibility is enabled for the layer.
     * \returns true if scale based visibility is enabled
     * \see minimumScale()
     * \see maximumScale()
     * \see setScaleBasedVisibility()
     * \see isInScaleRange()
     */
    bool hasScaleBasedVisibility() const;

    /**
     * Returns true if auto refresh is enabled for the layer.
     * \since QGIS 3.0
     * \see autoRefreshInterval()
     * \see setAutoRefreshEnabled()
     */
    bool hasAutoRefreshEnabled() const;

    /**
     * Returns the auto refresh interval (in milliseconds). Note that
     * auto refresh is only active when hasAutoRefreshEnabled() is true.
     * \since QGIS 3.0
     * \see autoRefreshEnabled()
     * \see setAutoRefreshInterval()
     */
    int autoRefreshInterval() const;

    /**
     * Sets the auto refresh interval (in milliseconds) for the layer. This
     * will cause the layer to be automatically redrawn on a matching interval.
     * Note that auto refresh must be enabled by calling setAutoRefreshEnabled().
     *
     * Note that auto refresh triggers deferred repaints of the layer. Any map
     * canvas must be refreshed separately in order to view the refreshed layer.
     * \since QGIS 3.0
     * \see autoRefreshInterval()
     * \see setAutoRefreshEnabled()
     */
    void setAutoRefreshInterval( int interval );

    /**
     * Sets whether auto refresh is enabled for the layer.
     * \since QGIS 3.0
     * \see hasAutoRefreshEnabled()
     * \see setAutoRefreshInterval()
     */
    void setAutoRefreshEnabled( bool enabled );

    /**
     * Returns a reference to the layer's metadata store.
     * \since QGIS 3.0
     * \see setMetadata()
     * \see metadataChanged()
     */
    virtual const QgsLayerMetadata &metadata() const;

    /**
     * Sets the layer's \a metadata store.
     * \since QGIS 3.0
     * \see metadata()
     * \see metadataChanged()
     */
    virtual void setMetadata( const QgsLayerMetadata &metadata );

    /**
     * Obtain a formatted HTML string containing assorted metadata for this layer.
     * \since QGIS 3.0
     */
    virtual QString htmlMetadata() const;

    //! Time stamp of data source in the moment when data/metadata were loaded by provider
    virtual QDateTime timestamp() const;

    /**
     * Gets the list of dependencies. This includes data dependencies set by the user (\see setDataDependencies)
     * as well as dependencies given by the provider
     *
     * \returns a set of QgsMapLayerDependency
     * \since QGIS 3.0
     */
    virtual QSet<QgsMapLayerDependency> dependencies() const;

  public slots:

    /**
     * Sets the minimum map \a scale (i.e. most "zoomed out" scale) at which the layer will be visible.
     * The \a scale value indicates the scale denominator, e.g. 1000.0 for a 1:1000 map.
     * A \a scale of 0 indicates no minimum scale visibility.
     * \note Scale based visibility is only used if setScaleBasedVisibility() is set to true.
     * \see minimumScale()
     * \see setMaximumScale()
     * \see setScaleBasedVisibility()
     */
    void setMinimumScale( double scale );

    /**
     * Sets the maximum map \a scale (i.e. most "zoomed in" scale) at which the layer will be visible.
     * The \a scale value indicates the scale denominator, e.g. 1000.0 for a 1:1000 map.
     * A \a scale of 0 indicates no maximum scale visibility.
     * \note Scale based visibility is only used if setScaleBasedVisibility() is set to true.
     * \see maximumScale()
     * \see setMinimumScale()
     * \see setScaleBasedVisibility()
     */
    void setMaximumScale( double scale );

    /** Sets whether scale based visibility is enabled for the layer.
     * \param enabled set to true to enable scale based visibility
     * \see setMinimumScale
     * \see setMaximumScale
     * \see scaleBasedVisibility
     */
    void setScaleBasedVisibility( const bool enabled );

    /**
     * Will advise the map canvas (and any other interested party) that this layer requires to be repainted.
     * Will emit a repaintRequested() signal.
     * If \a deferredUpdate is true then the layer will only be repainted when the canvas is next
     * re-rendered, and will not trigger any canvas redraws itself.
     *
     * \note in 2.6 function moved from vector/raster subclasses to QgsMapLayer
     */
    void triggerRepaint( bool deferredUpdate = false );

    /** Triggers an emission of the styleChanged() signal.
     * \since QGIS 2.16
     */
    void emitStyleChanged();

    /**
     * Sets the list of dependencies.
     * \see dependencies()
     *
     * \param layers set of QgsMapLayerDependency. Only user-defined dependencies will be added
     * \returns false if a dependency cycle has been detected
     * \since QGIS 3.0
     */
    virtual bool setDependencies( const QSet<QgsMapLayerDependency> &layers );

  signals:

    //! Emit a signal with status (e.g. to be caught by QgisApp and display a msg on status bar)
    void statusChanged( const QString &status );

    /**
     * Emitted when the name has been changed
     *
     * \since QGIS 2.16
     */
    void nameChanged();

    //! Emit a signal that layer's CRS has been reset
    void crsChanged();

    /** By emitting this signal the layer tells that either appearance or content have been changed
     * and any view showing the rendered layer should refresh itself.
     * If \a deferredUpdate is true then the layer will only be repainted when the canvas is next
     * re-rendered, and will not trigger any canvas redraws itself.
     */
    void repaintRequested( bool deferredUpdate = false );

    //! This is used to send a request that any mapcanvas using this layer update its extents
    void recalculateExtents() const;

    //! Data of layer changed
    void dataChanged();

    //! Signal emitted when the blend mode is changed, through QgsMapLayer::setBlendMode()
    void blendModeChanged( QPainter::CompositionMode blendMode );

    /** Signal emitted when renderer is changed.
     * \see styleChanged()
    */
    void rendererChanged();

    /** Signal emitted whenever a change affects the layer's style. Ie this may be triggered
     * by renderer changes, label style changes, or other style changes such as blend
     * mode or layer opacity changes.
     * \since QGIS 2.16
     * \see rendererChanged()
    */
    void styleChanged();

    /**
     * Signal emitted when legend of the layer has changed
     * \since QGIS 2.6
     */
    void legendChanged();

    /**
     * Emitted whenever the configuration is changed. The project listens to this signal
     * to be marked as dirty.
     */
    void configChanged();

    /**
     * Emitted when dependencies are changed.
     */
    void dependenciesChanged();

    /**
     * Emitted in the destructor when the layer is about to be deleted,
     * but it is still in a perfectly valid state: the last chance for
     * other pieces of code for some cleanup if they use the layer.
     * \since QGIS 3.0
     */
    void willBeDeleted();

    /**
     * Emitted when the auto refresh interval changes.
     * \see setAutoRefreshInterval()
     * \since QGIS 3.0
     */
    void autoRefreshIntervalChanged( int interval );

    /**
     * Emitted when the layer's metadata is changed.
     * \see setMetadata()
     * \see metadata()
     * \since QGIS 3.0
     */
    void metadataChanged();

  protected:

    /** Copies attributes like name, short name, ... into another layer.
     * \param layer The copy recipient
     * \since QGIS 3.0
     */
    void clone( QgsMapLayer *layer ) const;

    //! Set the extent
    virtual void setExtent( const QgsRectangle &rect );

    //! Set whether layer is valid or not - should be used in constructor.
    void setValid( bool valid );

    /** Called by readLayerXML(), used by children to read state specific to them from
     *  project files.
     */
    virtual bool readXml( const QDomNode &layer_node, const QgsReadWriteContext &context );

    /** Called by writeLayerXML(), used by children to write state specific to them to
     *  project files.
     */
    virtual bool writeXml( QDomNode &layer_node, QDomDocument &document, const QgsReadWriteContext &context ) const;


    /** Read custom properties from project file.
      \param layerNode note to read from
      \param keyStartsWith reads only properties starting with the specified string (or all if the string is empty)*/
    void readCustomProperties( const QDomNode &layerNode, const QString &keyStartsWith = QString() );

    //! Write custom properties to project file.
    void writeCustomProperties( QDomNode &layerNode, QDomDocument &doc ) const;

    //! Read style manager's configuration (if any). To be called by subclasses.
    void readStyleManager( const QDomNode &layerNode );
    //! Write style manager's configuration (if exists). To be called by subclasses.
    void writeStyleManager( QDomNode &layerNode, QDomDocument &doc ) const;

#ifndef SIP_RUN
#if 0
    //! Debugging member - invoked when a connect() is made to this object
    void connectNotify( const char *signal ) override;
#endif
#endif

    //! Add error message
    void appendError( const QgsErrorMessage &error ) { mError.append( error );}
    //! Set error message
    void setError( const QgsError &error ) { mError = error;}

    //! Extent of the layer
    mutable QgsRectangle mExtent;

    //! Indicates if the layer is valid and can be drawn
    bool mValid;

    //! Data source description string, varies by layer type
    QString mDataSource;

    //! Name of the layer - used for display
    QString mLayerName;

    /** Original name of the layer
     */
    QString mLayerOrigName;

    QString mShortName;
    QString mTitle;

    //! Description of the layer
    QString mAbstract;
    QString mKeywordList;

    //! DataUrl of the layer
    QString mDataUrl;
    QString mDataUrlFormat;

    //! Attribution of the layer
    QString mAttribution;
    QString mAttributionUrl;

    //! MetadataUrl of the layer
    QString mMetadataUrl;
    QString mMetadataUrlType;
    QString mMetadataUrlFormat;

    //! WMS legend
    QString mLegendUrl;
    QString mLegendUrlFormat;

    //! \brief Error
    QgsError mError;

    //! List of layers that may modify this layer on modification
    QSet<QgsMapLayerDependency> mDependencies;

    //! Checks whether a new set of dependencies will introduce a cycle
    bool hasDependencyCycle( const QSet<QgsMapLayerDependency> &layers ) const;

  private:

    /**
     * This method returns true by default but can be overwritten to specify
     * that a certain layer is writable.
     */
    virtual bool isReadOnly() const;

    /** Layer's spatial reference system.
        private to make sure setCrs must be used and crsChanged() is emitted */
    QgsCoordinateReferenceSystem mCRS;

    //! Unique ID of this layer - used to refer to this layer in map layer registry
    QString mID;

    //! Type of the layer (e.g., vector, raster)
    QgsMapLayer::LayerType mLayerType;

    //! Blend mode for the layer
    QPainter::CompositionMode mBlendMode;

    //! Tag for embedding additional information
    QString mTag;

    //! Minimum scale denominator at which this layer should be displayed
    double mMinScale;
    //! Maximum scale denominator at which this layer should be displayed
    double mMaxScale;
    //! A flag that tells us whether to use the above vars to restrict layer visibility
    bool mScaleBasedVisibility;

    //! Collection of undoable operations for this layer. *
    QUndoStack mUndoStack;

    QUndoStack mUndoStackStyles;

    //! Layer's persistent storage of additional properties (may be used by plugins)
    QgsObjectCustomProperties mCustomProperties;

    //! Controller of legend items of this layer
    QgsMapLayerLegend *mLegend = nullptr;

    //! Manager of multiple styles available for a layer (may be null)
    QgsMapLayerStyleManager *mStyleManager = nullptr;

    //! Timer for triggering automatic refreshes of the layer
    QTimer mRefreshTimer;

    QgsLayerMetadata mMetadata;

};

Q_DECLARE_METATYPE( QgsMapLayer * )

#ifndef SIP_RUN

/**
 * Weak pointer for QgsMapLayer
 * \since QGIS 3.0
 * \note not available in Python bindings
 */
typedef QPointer< QgsMapLayer > QgsWeakMapLayerPointer;

/**
 * A list of weak pointers to QgsMapLayers.
 * \since QGIS 3.0
 * \note not available in Python bindings
 */
typedef QList< QgsWeakMapLayerPointer > QgsWeakMapLayerPointerList;
#endif

#endif
