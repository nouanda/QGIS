/***************************************************************************
                         qgsprocessingutils.cpp
                         ------------------------
    begin                : April 2017
    copyright            : (C) 2017 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsprocessingutils.h"
#include "qgsproject.h"
#include "qgssettings.h"
#include "qgsexception.h"
#include "qgsprocessingcontext.h"
#include "qgsvectorlayerexporter.h"
#include "qgsvectorfilewriter.h"
#include "qgsmemoryproviderutils.h"
#include "qgsprocessingparameters.h"
#include "qgsprocessingalgorithm.h"
#include "qgsvectorlayerfeatureiterator.h"

QList<QgsRasterLayer *> QgsProcessingUtils::compatibleRasterLayers( QgsProject *project, bool sort )
{
  if ( !project )
    return QList<QgsRasterLayer *>();

  QList<QgsRasterLayer *> layers;
  Q_FOREACH ( QgsRasterLayer *l, project->layers<QgsRasterLayer *>() )
  {
    if ( canUseLayer( l ) )
      layers << l;
  }

  if ( sort )
  {
    std::sort( layers.begin(), layers.end(), []( const QgsRasterLayer * a, const QgsRasterLayer * b ) -> bool
    {
      return QString::localeAwareCompare( a->name(), b->name() ) < 0;
    } );
  }
  return layers;
}

QList<QgsVectorLayer *> QgsProcessingUtils::compatibleVectorLayers( QgsProject *project, const QList<QgsWkbTypes::GeometryType> &geometryTypes, bool sort )
{
  if ( !project )
    return QList<QgsVectorLayer *>();

  QList<QgsVectorLayer *> layers;
  Q_FOREACH ( QgsVectorLayer *l, project->layers<QgsVectorLayer *>() )
  {
    if ( canUseLayer( l, geometryTypes ) )
      layers << l;
  }

  if ( sort )
  {
    std::sort( layers.begin(), layers.end(), []( const QgsVectorLayer * a, const QgsVectorLayer * b ) -> bool
    {
      return QString::localeAwareCompare( a->name(), b->name() ) < 0;
    } );
  }
  return layers;
}

QList<QgsMapLayer *> QgsProcessingUtils::compatibleLayers( QgsProject *project, bool sort )
{
  if ( !project )
    return QList<QgsMapLayer *>();

  QList<QgsMapLayer *> layers;
  Q_FOREACH ( QgsRasterLayer *rl, compatibleRasterLayers( project, false ) )
    layers << rl;
  Q_FOREACH ( QgsVectorLayer *vl, compatibleVectorLayers( project, QList< QgsWkbTypes::GeometryType >(), false ) )
    layers << vl;

  if ( sort )
  {
    std::sort( layers.begin(), layers.end(), []( const QgsMapLayer * a, const QgsMapLayer * b ) -> bool
    {
      return QString::localeAwareCompare( a->name(), b->name() ) < 0;
    } );
  }
  return layers;
}

QgsMapLayer *QgsProcessingUtils::mapLayerFromStore( const QString &string, QgsMapLayerStore *store )
{
  if ( !store || string.isEmpty() )
    return nullptr;

  QList< QgsMapLayer * > layers = store->mapLayers().values();

  layers.erase( std::remove_if( layers.begin(), layers.end(), []( QgsMapLayer * layer )
  {
    switch ( layer->type() )
    {
      case QgsMapLayer::VectorLayer:
        return !canUseLayer( qobject_cast< QgsVectorLayer * >( layer ) );
      case QgsMapLayer::RasterLayer:
        return !canUseLayer( qobject_cast< QgsRasterLayer * >( layer ) );
      case QgsMapLayer::PluginLayer:
        return true;
    }
    return true;
  } ), layers.end() );

  Q_FOREACH ( QgsMapLayer *l, layers )
  {
    if ( l->id() == string )
      return l;
  }
  Q_FOREACH ( QgsMapLayer *l, layers )
  {
    if ( l->name() == string )
      return l;
  }
  Q_FOREACH ( QgsMapLayer *l, layers )
  {
    if ( normalizeLayerSource( l->source() ) == normalizeLayerSource( string ) )
      return l;
  }
  return nullptr;
}

///@cond PRIVATE
class ProjectionSettingRestorer
{
  public:

    ProjectionSettingRestorer()
    {
      QgsSettings settings;
      previousSetting = settings.value( QStringLiteral( "/Projections/defaultBehavior" ) ).toString();
      settings.setValue( QStringLiteral( "/Projections/defaultBehavior" ), QString() );
    }

    ~ProjectionSettingRestorer()
    {
      QgsSettings settings;
      settings.setValue( QStringLiteral( "/Projections/defaultBehavior" ), previousSetting );
    }

    QString previousSetting;
};
///@endcond PRIVATE

QgsMapLayer *QgsProcessingUtils::loadMapLayerFromString( const QString &string )
{
  if ( QFileInfo::exists( string ) )
  {
    // TODO - remove when there is a cleaner way to block the unknown projection dialog!
    ProjectionSettingRestorer restorer;
    ( void )restorer; // no warnings

    QFileInfo fi( string );
    QString name = fi.baseName();

    // brute force attempt to load a matching layer
    std::unique_ptr< QgsVectorLayer > layer( new QgsVectorLayer( string, name, QStringLiteral( "ogr" ), false ) );
    if ( layer->isValid() )
    {
      return layer.release();
    }
    std::unique_ptr< QgsRasterLayer > rasterLayer( new QgsRasterLayer( string, name, QStringLiteral( "gdal" ), false ) );
    if ( rasterLayer->isValid() )
    {
      return rasterLayer.release();
    }
  }
  return nullptr;
}

QgsMapLayer *QgsProcessingUtils::mapLayerFromString( const QString &string, QgsProcessingContext &context, bool allowLoadingNewLayers )
{
  if ( string.isEmpty() )
    return nullptr;

  // prefer project layers
  QgsMapLayer *layer = nullptr;
  if ( context.project() )
  {
    QgsMapLayer *layer = mapLayerFromStore( string, context.project()->layerStore() );
    if ( layer )
      return layer;
  }

  layer = mapLayerFromStore( string, context.temporaryLayerStore() );
  if ( layer )
    return layer;

  if ( !allowLoadingNewLayers )
    return nullptr;

  layer = loadMapLayerFromString( string );
  if ( layer )
  {
    context.temporaryLayerStore()->addMapLayer( layer );
    return layer;
  }
  else
  {
    return nullptr;
  }
}

QgsProcessingFeatureSource *QgsProcessingUtils::variantToSource( const QVariant &value, QgsProcessingContext &context, const QVariant &fallbackValue )
{
  QVariant val = value;
  bool selectedFeaturesOnly = false;
  if ( val.canConvert<QgsProcessingFeatureSourceDefinition>() )
  {
    // input is a QgsProcessingFeatureSourceDefinition - get extra properties from it
    QgsProcessingFeatureSourceDefinition fromVar = qvariant_cast<QgsProcessingFeatureSourceDefinition>( val );
    selectedFeaturesOnly = fromVar.selectedFeaturesOnly;
    val = fromVar.source;
  }

  if ( QgsVectorLayer *layer = qobject_cast< QgsVectorLayer * >( qvariant_cast<QObject *>( val ) ) )
  {
    return new QgsProcessingFeatureSource( layer, context );
  }

  QString layerRef;
  if ( val.canConvert<QgsProperty>() )
  {
    layerRef = val.value< QgsProperty >().valueAsString( context.expressionContext(), fallbackValue.toString() );
  }
  else if ( !val.isValid() || val.toString().isEmpty() )
  {
    // fall back to default
    if ( QgsVectorLayer *layer = qobject_cast< QgsVectorLayer * >( qvariant_cast<QObject *>( fallbackValue ) ) )
    {
      return new QgsProcessingFeatureSource( layer, context );
    }

    layerRef = fallbackValue.toString();
  }
  else
  {
    layerRef = val.toString();
  }

  if ( layerRef.isEmpty() )
    return nullptr;

  QgsVectorLayer *vl = qobject_cast< QgsVectorLayer *>( QgsProcessingUtils::mapLayerFromString( layerRef, context ) );
  if ( !vl )
    return nullptr;

  if ( selectedFeaturesOnly )
  {
    return new QgsProcessingFeatureSource( new QgsVectorLayerSelectedFeatureSource( vl ), context, true );
  }
  else
  {
    return new QgsProcessingFeatureSource( vl, context );
  }
}

bool QgsProcessingUtils::canUseLayer( const QgsRasterLayer *layer )
{
  // only gdal file-based layers
  return layer && layer->providerType() == QStringLiteral( "gdal" );
}

bool QgsProcessingUtils::canUseLayer( const QgsVectorLayer *layer, const QList<QgsWkbTypes::GeometryType> &geometryTypes )
{
  return layer &&
         ( geometryTypes.isEmpty() || geometryTypes.contains( layer->geometryType() ) );
}

QString QgsProcessingUtils::normalizeLayerSource( const QString &source )
{
  QString normalized = source;
  normalized.replace( '\\', '/' );
  normalized.replace( '"', "'" );
  return normalized.trimmed();
}

void parseDestinationString( QString &destination, QString &providerKey, QString &uri, QString &format, QMap<QString, QVariant> &options )
{
  QRegularExpression splitRx( "^(.{3,}):(.*)$" );
  QRegularExpressionMatch match = splitRx.match( destination );
  if ( match.hasMatch() )
  {
    providerKey = match.captured( 1 );
    if ( providerKey == QStringLiteral( "postgis" ) ) // older processing used "postgis" instead of "postgres"
    {
      providerKey = QStringLiteral( "postgres" );
    }
    uri = match.captured( 2 );
  }
  else
  {
    providerKey = QStringLiteral( "ogr" );
    QRegularExpression splitRx( "^(.*)\\.(.*?)$" );
    QRegularExpressionMatch match = splitRx.match( destination );
    QString extension;
    if ( match.hasMatch() )
    {
      extension = match.captured( 2 );
      format = QgsVectorFileWriter::driverForExtension( extension );
    }

    if ( format.isEmpty() )
    {
      format = QStringLiteral( "ESRI Shapefile" );
      destination = destination + QStringLiteral( ".shp" );
    }

    options.insert( QStringLiteral( "driverName" ), format );
    uri = destination;
  }
}

QgsFeatureSink *QgsProcessingUtils::createFeatureSink( QString &destination, QgsProcessingContext &context, const QgsFields &fields, QgsWkbTypes::Type geometryType, const QgsCoordinateReferenceSystem &crs, const QVariantMap &createOptions )
{
  QVariantMap options = createOptions;
  if ( !options.contains( QStringLiteral( "fileEncoding" ) ) )
  {
    // no destination encoding specified, use default
    options.insert( QStringLiteral( "fileEncoding" ), context.defaultEncoding().isEmpty() ? QStringLiteral( "system" ) : context.defaultEncoding() );
  }

  if ( destination.isEmpty() || destination.startsWith( QStringLiteral( "memory:" ) ) )
  {
    // memory provider cannot be used with QgsVectorLayerImport - so create layer manually
    std::unique_ptr< QgsVectorLayer > layer( QgsMemoryProviderUtils::createMemoryLayer( destination, fields, geometryType, crs ) );
    if ( !layer )
      return nullptr;

    if ( !layer->isValid() )
    {
      return nullptr;
    }

    // update destination to layer ID
    destination = layer->id();

    // this is a factory, so we need to return a proxy
    std::unique_ptr< QgsProxyFeatureSink > sink( new QgsProxyFeatureSink( layer->dataProvider() ) );
    context.temporaryLayerStore()->addMapLayer( layer.release() );

    return sink.release();
  }
  else
  {
    QString providerKey;
    QString uri;
    QString format;
    parseDestinationString( destination, providerKey, uri, format, options );

    if ( providerKey == "ogr" )
    {
      // use QgsVectorFileWriter for OGR destinations instead of QgsVectorLayerImport, as that allows
      // us to use any OGR format which supports feature addition
      QString finalFileName;
      QgsVectorFileWriter *writer = new QgsVectorFileWriter( destination, options.value( QStringLiteral( "fileEncoding" ) ).toString(), fields, geometryType, crs, format, QgsVectorFileWriter::defaultDatasetOptions( format ),
          QgsVectorFileWriter::defaultLayerOptions( format ), &finalFileName );
      destination = finalFileName;
      return writer;
    }
    else
    {
      //create empty layer
      std::unique_ptr< QgsVectorLayerExporter > exporter( new QgsVectorLayerExporter( uri, providerKey, fields, geometryType, crs, false, options ) );
      if ( exporter->errorCode() )
        return nullptr;

      // use destination string as layer name (eg "postgis:..." )
      std::unique_ptr< QgsVectorLayer > layer( new QgsVectorLayer( uri, destination, providerKey ) );
      // update destination to layer ID
      destination = layer->id();
      context.temporaryLayerStore()->addMapLayer( layer.release() );
      return exporter.release();
    }
  }
  return nullptr;
}

void QgsProcessingUtils::createFeatureSinkPython( QgsFeatureSink **sink, QString &destination, QgsProcessingContext &context, const QgsFields &fields, QgsWkbTypes::Type geometryType, const QgsCoordinateReferenceSystem &crs, const QVariantMap &options )
{
  *sink = createFeatureSink( destination, context, fields, geometryType, crs, options );
}


QgsRectangle QgsProcessingUtils::combineLayerExtents( const QList<QgsMapLayer *> layers, const QgsCoordinateReferenceSystem &crs )
{
  QgsRectangle extent;
  Q_FOREACH ( QgsMapLayer *layer, layers )
  {
    if ( !layer )
      continue;

    if ( crs.isValid() )
    {
      //transform layer extent to target CRS
      QgsCoordinateTransform ct( layer->crs(), crs );
      try
      {
        QgsRectangle reprojExtent = ct.transformBoundingBox( layer->extent() );
        extent.combineExtentWith( reprojExtent );
      }
      catch ( QgsCsException & )
      {
        // can't reproject... what to do here? hmmm?
        // let's ignore this layer for now, but maybe we should just use the original extent?
      }
    }
    else
    {
      extent.combineExtentWith( layer->extent() );
    }

  }
  return extent;
}

QVariant QgsProcessingUtils::generateIteratingDestination( const QVariant &input, const QVariant &id, QgsProcessingContext &context )
{
  if ( !input.isValid() )
    return QStringLiteral( "memory:%1" ).arg( id.toString() );

  if ( input.canConvert<QgsProcessingOutputLayerDefinition>() )
  {
    QgsProcessingOutputLayerDefinition fromVar = qvariant_cast<QgsProcessingOutputLayerDefinition>( input );
    QVariant newSink = generateIteratingDestination( fromVar.sink, id, context );
    fromVar.sink = QgsProperty::fromValue( newSink );
    return fromVar;
  }
  else if ( input.canConvert<QgsProperty>() )
  {
    QString res = input.value< QgsProperty>().valueAsString( context.expressionContext() );
    return generateIteratingDestination( res, id, context );
  }
  else
  {
    QString res = input.toString();
    if ( res.startsWith( QStringLiteral( "memory:" ) ) )
    {
      return res + '_' + id.toString();
    }
    else
    {
      // assume a filename type output for now
      // TODO - uris?
      int lastIndex = res.lastIndexOf( '.' );
      return res.left( lastIndex ) + '_' + id.toString() + res.mid( lastIndex );
    }
  }
}

QString QgsProcessingUtils::tempFolder()
{
  static QString sFolder;
  static QMutex sMutex;
  sMutex.lock();
  if ( sFolder.isEmpty() )
  {
    QString subPath = QUuid::createUuid().toString().remove( '-' ).remove( '{' ).remove( '}' );
    sFolder = QDir::tempPath() + QStringLiteral( "/processing_" ) + subPath;
    if ( !QDir( sFolder ).exists() )
      QDir().mkpath( sFolder );
  }
  sMutex.unlock();
  return sFolder;
}

QString QgsProcessingUtils::generateTempFilename( const QString &basename )
{
  QString subPath = QUuid::createUuid().toString().remove( '-' ).remove( '{' ).remove( '}' );
  QString path = tempFolder() + '/' + subPath;
  if ( !QDir( path ).exists() ) //make sure the directory exists - it shouldn't, but lets be safe...
  {
    QDir tmpDir;
    tmpDir.mkdir( path );
  }
  return path + '/' + basename;
}

QString QgsProcessingUtils::formatHelpMapAsHtml( const QVariantMap &map, const QgsProcessingAlgorithm *algorithm )
{
  auto getText = [map]( const QString & key )->QString
  {
    if ( map.contains( key ) )
      return map.value( key ).toString();
    return QString();
  };

  QString s = QObject::tr( "<html><body><h2>Algorithm description</h2>\n " );
  s += QStringLiteral( "<p>" ) + getText( QStringLiteral( "ALG_DESC" ) ) + QStringLiteral( "</p>\n" );
  s +=  QObject::tr( "<h2>Input parameters</h2>\n" );

  Q_FOREACH ( const QgsProcessingParameterDefinition *def, algorithm->parameterDefinitions() )
  {
    s += QStringLiteral( "<h3>" ) + def->description() + QStringLiteral( "</h3>\n" );
    s += QStringLiteral( "<p>" ) + getText( def->name() ) + QStringLiteral( "</p>\n" );
  }
  s += QObject::tr( "<h2>Outputs</h2>\n" );
  Q_FOREACH ( const QgsProcessingOutputDefinition *def, algorithm->outputDefinitions() )
  {
    s += QStringLiteral( "<h3>" ) + def->description() + QStringLiteral( "</h3>\n" );
    s += QStringLiteral( "<p>" ) + getText( def->name() ) + QStringLiteral( "</p>\n" );
  }
  s += "<br>";
  s += QObject::tr( "<p align=\"right\">Algorithm author: %1</p>" ).arg( getText( QStringLiteral( "ALG_CREATOR" ) ) );
  s += QObject::tr( "<p align=\"right\">Help author: %1</p>" ).arg( getText( QStringLiteral( "ALG_HELP_CREATOR" ) ) );
  s += QObject::tr( "<p align=\"right\">Algorithm version: %1</p>" ).arg( getText( QStringLiteral( "ALG_VERSION" ) ) );
  s += QStringLiteral( "</body></html>" );
  return s;
}

QString QgsProcessingUtils::convertToCompatibleFormat( const QgsVectorLayer *vl, bool selectedFeaturesOnly, const QString &baseName, const QStringList &compatibleFormats, const QString &preferredFormat, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  bool requiresTranslation = selectedFeaturesOnly;
  if ( !selectedFeaturesOnly )
  {
    QFileInfo fi( vl->source() );
    requiresTranslation = !compatibleFormats.contains( fi.suffix(), Qt::CaseInsensitive );
  }

  if ( requiresTranslation )
  {
    QString temp = QgsProcessingUtils::generateTempFilename( baseName + '.' + preferredFormat );

    QgsVectorFileWriter writer( temp, context.defaultEncoding(),
                                vl->fields(), vl->wkbType(), vl->crs(), QgsVectorFileWriter::driverForExtension( preferredFormat ) );
    QgsFeature f;
    QgsFeatureIterator it;
    if ( selectedFeaturesOnly )
      it = vl->getSelectedFeatures();
    else
      it = vl->getFeatures();

    while ( it.nextFeature( f ) )
    {
      if ( feedback->isCanceled() )
        return QString();
      writer.addFeature( f, QgsFeatureSink::FastInsert );
    }
    return temp;
  }
  else
  {
    return vl->source();
  }
}


//
// QgsProcessingFeatureSource
//

QgsProcessingFeatureSource::QgsProcessingFeatureSource( QgsFeatureSource *originalSource, const QgsProcessingContext &context, bool ownsOriginalSource )
  : mSource( originalSource )
  , mOwnsSource( ownsOriginalSource )
  , mInvalidGeometryCheck( context.invalidGeometryCheck() )
  , mInvalidGeometryCallback( context.invalidGeometryCallback() )
  , mTransformErrorCallback( context.transformErrorCallback() )
{}

QgsProcessingFeatureSource::~QgsProcessingFeatureSource()
{
  if ( mOwnsSource )
    delete mSource;
}

QgsFeatureIterator QgsProcessingFeatureSource::getFeatures( const QgsFeatureRequest &request, Flags flags ) const
{
  QgsFeatureRequest req( request );
  req.setTransformErrorCallback( mTransformErrorCallback );

  if ( flags & FlagSkipGeometryValidityChecks )
    req.setInvalidGeometryCheck( QgsFeatureRequest::GeometryNoCheck );
  else
  {
    req.setInvalidGeometryCheck( mInvalidGeometryCheck );
    req.setInvalidGeometryCallback( mInvalidGeometryCallback );
  }

  return mSource->getFeatures( req );
}

QgsFeatureIterator QgsProcessingFeatureSource::getFeatures( const QgsFeatureRequest &request ) const
{
  QgsFeatureRequest req( request );
  req.setInvalidGeometryCheck( mInvalidGeometryCheck );
  req.setInvalidGeometryCallback( mInvalidGeometryCallback );
  req.setTransformErrorCallback( mTransformErrorCallback );
  return mSource->getFeatures( req );
}

QgsCoordinateReferenceSystem QgsProcessingFeatureSource::sourceCrs() const
{
  return mSource->sourceCrs();
}

QgsFields QgsProcessingFeatureSource::fields() const
{
  return mSource->fields();
}

QgsWkbTypes::Type QgsProcessingFeatureSource::wkbType() const
{
  return mSource->wkbType();
}

long QgsProcessingFeatureSource::featureCount() const
{
  return mSource->featureCount();
}

QString QgsProcessingFeatureSource::sourceName() const
{
  return mSource->sourceName();

}

QSet<QVariant> QgsProcessingFeatureSource::uniqueValues( int fieldIndex, int limit ) const
{
  return mSource->uniqueValues( fieldIndex, limit );
}

QVariant QgsProcessingFeatureSource::minimumValue( int fieldIndex ) const
{
  return mSource->minimumValue( fieldIndex );
}

QVariant QgsProcessingFeatureSource::maximumValue( int fieldIndex ) const
{
  return mSource->maximumValue( fieldIndex );
}
