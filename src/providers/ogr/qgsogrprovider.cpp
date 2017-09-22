/***************************************************************************
           qgsogrprovider.cpp Data provider for OGR supported formats
                    Formerly known as qgsshapefileprovider.cpp
begin                : Oct 29, 2003
copyright            : (C) 2003 by Gary E.Sherman
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

#include "qgsogrprovider.h"
#include "qgsogrfeatureiterator.h"
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgslocalec.h"
#include "qgsfeedback.h"
#include "qgssettings.h"
#include "qgsapplication.h"
#include "qgsdataitem.h"
#include "qgsdataprovider.h"
#include "qgsfeature.h"
#include "qgsfields.h"
#include "qgsgeometry.h"
#include "qgscoordinatereferencesystem.h"
#include "qgsvectorlayerexporter.h"
#include "qgsdataitemprovider.h"
#include "qgsogrdataitems.h"
#include "qgsgeopackagedataitems.h"
#include "qgswkbtypes.h"
#include "qgis.h"


#define CPL_SUPRESS_CPLUSPLUS  //#spellok
#include <gdal.h>         // to collect version information
#include <ogr_api.h>
#include <ogr_srs_api.h>
#include <cpl_string.h>

#include <limits>

#include <QtDebug>
#include <QFile>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>
#include <QString>
#include <QTextCodec>


#ifdef Q_OS_WIN
#include <windows.h>
#endif
#ifdef Q_OS_LINUX
#include <sys/vfs.h>
#endif

// Starting with GDAL 2.2, there are 2 concepts: unset fields and null fields
// whereas previously there was only unset fields. For QGIS purposes, both
// states (unset/null) are equivalent.
#ifndef OGRNullMarker
#define OGR_F_IsFieldSetAndNotNull OGR_F_IsFieldSet
#endif

static const QString TEXT_PROVIDER_KEY = QStringLiteral( "ogr" );
static const QString TEXT_PROVIDER_DESCRIPTION =
  QStringLiteral( "OGR data provider" )
  + " (compiled against GDAL/OGR library version "
  + GDAL_RELEASE_NAME
  + ", running against GDAL/OGR library version "
  + GDALVersionInfo( "RELEASE_NAME" )
  + ')';

static OGRwkbGeometryType ogrWkbGeometryTypeFromName( const QString &typeName );

class QgsCPLErrorHandler
{
    static void CPL_STDCALL showError( CPLErr errClass, int errNo, const char *msg )
    {
      if ( errNo != OGRERR_NONE )
        QgsMessageLog::logMessage( QObject::tr( "OGR[%1] error %2: %3" ).arg( errClass ).arg( errNo ).arg( msg ), QObject::tr( "OGR" ) );
    }

  public:
    QgsCPLErrorHandler()
    {
      CPLPushErrorHandler( showError );
    }

    ~QgsCPLErrorHandler()
    {
      CPLPopErrorHandler();
    }

  private:
    QgsCPLErrorHandler( const QgsCPLErrorHandler &other );
    QgsCPLErrorHandler &operator=( const QgsCPLErrorHandler &other );

};


bool QgsOgrProvider::convertField( QgsField &field, const QTextCodec &encoding )
{
  OGRFieldType ogrType = OFTString; //default to string
  int ogrWidth = field.length();
  int ogrPrecision = field.precision();
  if ( ogrPrecision > 0 )
    ogrWidth += 1;
  switch ( field.type() )
  {
    case QVariant::LongLong:
      ogrType = OFTInteger64;
      ogrPrecision = 0;
      ogrWidth = ogrWidth > 0 && ogrWidth <= 21 ? ogrWidth : 21;
      break;

    case QVariant::String:
      ogrType = OFTString;
      if ( ogrWidth < 0 || ogrWidth > 255 )
        ogrWidth = 255;
      break;

    case QVariant::Int:
      ogrType = OFTInteger;
      ogrWidth = ogrWidth > 0 && ogrWidth <= 10 ? ogrWidth : 10;
      ogrPrecision = 0;
      break;

    case QVariant::Double:
      ogrType = OFTReal;
      break;

    case QVariant::Date:
      ogrType = OFTDate;
      break;

    case QVariant::Time:
      ogrType = OFTTime;
      break;

    case QVariant::DateTime:
      ogrType = OFTDateTime;
      break;

    default:
      return false;
  }

  field.setTypeName( encoding.toUnicode( OGR_GetFieldTypeName( ogrType ) ) );
  field.setLength( ogrWidth );
  field.setPrecision( ogrPrecision );
  return true;
}

void QgsOgrProvider::repack()
{
  if ( !mValid || ogrDriverName != QLatin1String( "ESRI Shapefile" ) || !ogrOrigLayer )
    return;

  QByteArray layerName = OGR_FD_GetName( OGR_L_GetLayerDefn( ogrOrigLayer ) );

  // run REPACK on shape files
  QByteArray sql = QByteArray( "REPACK " ) + layerName;   // don't quote the layer name as it works with spaces in the name and won't work if the name is quoted
  QgsDebugMsg( QString( "SQL: %1" ).arg( QString::fromUtf8( sql ) ) );
  CPLErrorReset();
  OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
  if ( CPLGetLastErrorType() != CE_None )
  {
    pushError( tr( "OGR[%1] error %2: %3" ).arg( CPLGetLastErrorType() ).arg( CPLGetLastErrorNo() ).arg( CPLGetLastErrorMsg() ) );
  }

  if ( mFilePath.endsWith( QLatin1String( ".shp" ), Qt::CaseInsensitive ) || mFilePath.endsWith( QLatin1String( ".dbf" ), Qt::CaseInsensitive ) )
  {
    QString packedDbf( mFilePath.left( mFilePath.size() - 4 ) + "_packed.dbf" );
    if ( QFile::exists( packedDbf ) )
    {
      QgsMessageLog::logMessage( tr( "Possible corruption after REPACK detected. %1 still exists. This may point to a permission or locking problem of the original DBF." ).arg( packedDbf ), tr( "OGR" ), QgsMessageLog::CRITICAL );

      OGR_DS_Destroy( ogrDataSource );
      ogrLayer = ogrOrigLayer = nullptr;

      ogrDataSource = QgsOgrProviderUtils::OGROpenWrapper( mFilePath.toUtf8().constData(), true, nullptr );
      if ( ogrDataSource )
      {
        if ( mLayerName.isNull() )
        {
          ogrOrigLayer = OGR_DS_GetLayer( ogrDataSource, mLayerIndex );
        }
        else
        {
          ogrOrigLayer = OGR_DS_GetLayerByName( ogrDataSource, mLayerName.toUtf8().constData() );
        }

        if ( !ogrOrigLayer )
        {
          QgsMessageLog::logMessage( tr( "Original layer could not be reopened." ), tr( "OGR" ), QgsMessageLog::CRITICAL );
          mValid = false;
        }

        ogrLayer = ogrOrigLayer;
      }
      else
      {
        QgsMessageLog::logMessage( tr( "Original datasource could not be reopened." ), tr( "OGR" ), QgsMessageLog::CRITICAL );
        mValid = false;
      }
    }

  }

  long oldcount = mFeaturesCounted;
  recalculateFeatureCount();
  if ( oldcount != mFeaturesCounted )
    emit dataChanged();
}


QgsVectorLayerExporter::ExportError QgsOgrProvider::createEmptyLayer( const QString &uri,
    const QgsFields &fields,
    QgsWkbTypes::Type wkbType,
    const QgsCoordinateReferenceSystem &srs,
    bool overwrite,
    QMap<int, int> *oldToNewAttrIdxMap,
    QString *errorMessage,
    const QMap<QString, QVariant> *options )
{
  QString encoding;
  QString driverName = QStringLiteral( "ESRI Shapefile" );
  QStringList dsOptions, layerOptions;
  QString layerName;

  if ( options )
  {
    if ( options->contains( QStringLiteral( "fileEncoding" ) ) )
      encoding = options->value( QStringLiteral( "fileEncoding" ) ).toString();

    if ( options->contains( QStringLiteral( "driverName" ) ) )
      driverName = options->value( QStringLiteral( "driverName" ) ).toString();

    if ( options->contains( QStringLiteral( "datasourceOptions" ) ) )
      dsOptions << options->value( QStringLiteral( "datasourceOptions" ) ).toStringList();

    if ( options->contains( QStringLiteral( "layerOptions" ) ) )
      layerOptions << options->value( QStringLiteral( "layerOptions" ) ).toStringList();

    if ( options->contains( QStringLiteral( "layerName" ) ) )
      layerName = options->value( QStringLiteral( "layerName" ) ).toString();
  }

  if ( oldToNewAttrIdxMap )
    oldToNewAttrIdxMap->clear();
  if ( errorMessage )
    errorMessage->clear();

  QgsVectorFileWriter::ActionOnExistingFile action( QgsVectorFileWriter::CreateOrOverwriteFile );

  bool update = false;
  if ( options && options->contains( QStringLiteral( "update" ) ) )
  {
    update = options->value( QStringLiteral( "update" ) ).toBool();
    if ( update )
    {
      if ( !overwrite && !layerName.isEmpty() )
      {
        OGRDataSourceH hDS = OGROpen( uri.toUtf8().constData(), TRUE, nullptr );
        if ( hDS )
        {
          if ( OGR_DS_GetLayerByName( hDS, layerName.toUtf8().constData() ) )
          {
            OGR_DS_Destroy( hDS );
            if ( errorMessage )
              *errorMessage += QObject::tr( "Layer %2 of %1 exists and overwrite flag is false." )
                               .arg( uri ).arg( layerName );
            return QgsVectorLayerExporter::ErrCreateDataSource;
          }
          OGR_DS_Destroy( hDS );
        }
      }
      action = QgsVectorFileWriter::CreateOrOverwriteLayer;
    }
  }

  if ( !overwrite && !update )
  {
    QFileInfo fi( uri );
    if ( fi.exists() )
    {
      if ( errorMessage )
        *errorMessage += QObject::tr( "Unable to create the datasource. %1 exists and overwrite flag is false." )
                         .arg( uri );
      return QgsVectorLayerExporter::ErrCreateDataSource;
    }
  }

  QgsVectorFileWriter *writer = new QgsVectorFileWriter(
    uri, encoding, fields, wkbType,
    srs, driverName, dsOptions, layerOptions, nullptr,
    QgsVectorFileWriter::NoSymbology, nullptr,
    layerName, action );

  QgsVectorFileWriter::WriterError error = writer->hasError();
  if ( error )
  {
    if ( errorMessage )
      *errorMessage += writer->errorMessage();

    delete writer;
    return ( QgsVectorLayerExporter::ExportError ) error;
  }

  QMap<int, int> attrIdxMap = writer->attrIdxToOgrIdx();
  delete writer;

  if ( oldToNewAttrIdxMap )
  {
    bool firstFieldIsFid = false;
    if ( !layerName.isEmpty() )
    {
      OGRDataSourceH hDS = OGROpen( uri.toUtf8().constData(), TRUE, nullptr );
      if ( hDS )
      {
        OGRLayerH hLayer = OGR_DS_GetLayerByName( hDS, layerName.toUtf8().constData() );
        if ( hLayer )
        {
          // Expose the OGR FID if it comes from a "real" column (typically GPKG)
          // and make sure that this FID column is not exposed as a regular OGR field (shouldn't happen normally)
          firstFieldIsFid = !( EQUAL( OGR_L_GetFIDColumn( hLayer ), "" ) ) &&
                            OGR_FD_GetFieldIndex( OGR_L_GetLayerDefn( hLayer ), OGR_L_GetFIDColumn( hLayer ) ) < 0 &&
                            fields.indexFromName( OGR_L_GetFIDColumn( hLayer ) ) < 0;

        }
        OGR_DS_Destroy( hDS );
      }
    }

    for ( QMap<int, int>::const_iterator attrIt = attrIdxMap.begin(); attrIt != attrIdxMap.end(); ++attrIt )
    {
      oldToNewAttrIdxMap->insert( attrIt.key(), *attrIt + ( firstFieldIsFid ? 1 : 0 ) );
    }
  }

  return QgsVectorLayerExporter::NoError;
}

static QString AnalyzeURI( QString const &uri,
                           bool &isSubLayer,
                           int &layerIndex,
                           QString &layerName,
                           QString &subsetString,
                           OGRwkbGeometryType &ogrGeometryTypeFilter )
{
  isSubLayer = false;
  layerIndex = 0;
  layerName = QString();
  subsetString = QString();
  ogrGeometryTypeFilter = wkbUnknown;

  QgsDebugMsg( "Data source uri is [" + uri + ']' );

  // try to open for update, but disable error messages to avoid a
  // message if the file is read only, because we cope with that
  // ourselves.

  // This part of the code parses the uri transmitted to the ogr provider to
  // get the options the client wants us to apply

  // If there is no | in the uri, then the uri is just the filename. The loaded
  // layer will be layer 0.

  if ( !uri.contains( '|', Qt::CaseSensitive ) )
  {
    return uri;
  }
  else
  {
    QStringList theURIParts = uri.split( '|' );
    QString filePath = theURIParts.at( 0 );

    for ( int i = 1 ; i < theURIParts.size(); i++ )
    {
      QString part = theURIParts.at( i );
      int pos = part.indexOf( '=' );
      QString field = part.left( pos );
      QString value = part.mid( pos + 1 );

      if ( field == QLatin1String( "layerid" ) )
      {
        bool ok;
        layerIndex = value.toInt( &ok );
        if ( ! ok || layerIndex < 0 )
        {
          layerIndex = -1;
        }
        else
        {
          isSubLayer = true;
        }
      }
      else if ( field == QLatin1String( "layername" ) )
      {
        layerName = value;
        isSubLayer = true;
      }

      else if ( field == QLatin1String( "subset" ) )
      {
        subsetString = value;
      }

      else if ( field == QLatin1String( "geometrytype" ) )
      {
        ogrGeometryTypeFilter = ogrWkbGeometryTypeFromName( value );
      }
    }

    return filePath;
  }

}

QgsOgrProvider::QgsOgrProvider( QString const &uri )
  : QgsVectorDataProvider( uri )
  , mFirstFieldIsFid( false )
  , ogrDataSource( nullptr )
  , mExtent( nullptr )
  , mForceRecomputeExtent( false )
  , ogrLayer( nullptr )
  , ogrOrigLayer( nullptr )
  , mLayerIndex( 0 )
  , mIsSubLayer( false )
  , mOgrGeometryTypeFilter( wkbUnknown )
  , ogrDriver( nullptr )
  , mValid( false )
  , mOGRGeomType( wkbUnknown )
  , mFeaturesCounted( QgsVectorDataProvider::Uncounted )
  , mWriteAccess( false )
  , mWriteAccessPossible( false )
  , mDynamicWriteAccess( false )
  , mShapefileMayBeCorrupted( false )
  , mUpdateModeStackDepth( 0 )
  , mCapabilities( 0 )
{
  QgsApplication::registerOgrDrivers();

  QgsSettings settings;
  CPLSetConfigOption( "SHAPE_ENCODING", settings.value( QStringLiteral( "qgis/ignoreShapeEncoding" ), true ).toBool() ? "" : nullptr );

  // make connection to the data source

  QgsDebugMsg( "Data source uri is [" + uri + ']' );

  mFilePath = AnalyzeURI( uri,
                          mIsSubLayer,
                          mLayerIndex,
                          mLayerName,
                          mSubsetString,
                          mOgrGeometryTypeFilter );

  open( OpenModeInitial );

  QList<NativeType> nativeTypes;
  nativeTypes
      << QgsVectorDataProvider::NativeType( tr( "Whole number (integer)" ), QStringLiteral( "integer" ), QVariant::Int, 0, 11 )
      << QgsVectorDataProvider::NativeType( tr( "Whole number (integer 64 bit)" ), "integer64", QVariant::LongLong, 0, 21 )
      << QgsVectorDataProvider::NativeType( tr( "Decimal number (real)" ), QStringLiteral( "double" ), QVariant::Double, 0, 20, 0, 15 )
      << QgsVectorDataProvider::NativeType( tr( "Text (string)" ), QStringLiteral( "string" ), QVariant::String, 0, 65535 )
      << QgsVectorDataProvider::NativeType( tr( "Date" ), QStringLiteral( "date" ), QVariant::Date, 8, 8 );

  // Some drivers do not support datetime type
  // Please help to fill this list
  if ( ogrDriverName != QLatin1String( "ESRI Shapefile" ) )
  {
    nativeTypes
        << QgsVectorDataProvider::NativeType( tr( "Time" ), QStringLiteral( "time" ), QVariant::Time, -1, -1 )
        << QgsVectorDataProvider::NativeType( tr( "Date & Time" ), QStringLiteral( "datetime" ), QVariant::DateTime );
  }

  setNativeTypes( nativeTypes );

  QgsOgrConnPool::instance()->ref( dataSourceUri() );
}

QgsOgrProvider::~QgsOgrProvider()
{
  QgsOgrConnPool::instance()->unref( dataSourceUri() );
  // We must also make sure to flush unusef cached connections so that
  // the file can be removed (#15137)
  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );

  // Do that as last step for final cleanup that might be prevented by
  // still opened datasets.
  close();
}

QgsAbstractFeatureSource *QgsOgrProvider::featureSource() const
{
  return new QgsOgrFeatureSource( this );
}

bool QgsOgrProvider::setSubsetString( const QString &theSQL, bool updateFeatureCount )
{
  QgsCPLErrorHandler handler;

  if ( !ogrDataSource )
    return false;

  if ( theSQL == mSubsetString && mFeaturesCounted != QgsVectorDataProvider::Uncounted )
    return true;

  OGRLayerH prevLayer = ogrLayer;
  QString prevSubsetString = mSubsetString;
  mSubsetString = theSQL;

  if ( !mSubsetString.isEmpty() )
  {

    ogrLayer = setSubsetString( ogrOrigLayer, ogrDataSource );
    if ( !ogrLayer )
    {
      pushError( tr( "OGR[%1] error %2: %3" ).arg( CPLGetLastErrorType() ).arg( CPLGetLastErrorNo() ).arg( CPLGetLastErrorMsg() ) );
      ogrLayer = prevLayer;
      mSubsetString = prevSubsetString;
      return false;
    }
  }
  else
  {
    ogrLayer = ogrOrigLayer;
  }

  if ( prevLayer != ogrOrigLayer )
  {
    OGR_DS_ReleaseResultSet( ogrDataSource, prevLayer );
  }

  QString uri = mFilePath;
  if ( !mLayerName.isNull() )
  {
    uri += QStringLiteral( "|layername=%1" ).arg( mLayerName );
  }
  else if ( mLayerIndex >= 0 )
  {
    uri += QStringLiteral( "|layerid=%1" ).arg( mLayerIndex );
  }

  if ( !mSubsetString.isEmpty() )
  {
    uri += QStringLiteral( "|subset=%1" ).arg( mSubsetString );
  }

  if ( mOgrGeometryTypeFilter != wkbUnknown )
  {
    uri += QStringLiteral( "|geometrytype=%1" ).arg( ogrWkbGeometryTypeName( mOgrGeometryTypeFilter ) );
  }

  setDataSourceUri( uri );

  OGR_L_ResetReading( ogrLayer );

  // getting the total number of features in the layer
  // TODO: This can be expensive, do we really need it!
  if ( updateFeatureCount )
  {
    recalculateFeatureCount();
  }

  // check the validity of the layer
  QgsDebugMsg( "checking validity" );
  loadFields();
  QgsDebugMsg( "Done checking validity" );

  invalidateCachedExtent( false );

  emit dataChanged();

  return true;
}

QString QgsOgrProvider::subsetString() const
{
  return mSubsetString;
}

QString QgsOgrProvider::ogrWkbGeometryTypeName( OGRwkbGeometryType type ) const
{
  QString geom;

  // GDAL 2.1 can return M/ZM geometries
#if defined(GDAL_COMPUTE_VERSION) && GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,1,0)
  if ( wkbHasM( type ) )
  {
    geom = ogrWkbGeometryTypeName( wkbFlatten( type ) );
    if ( wkbHasZ( type ) )
      geom += "Z";
    if ( wkbHasM( type ) )
      geom += "M";
    return geom;
  }
#endif

  switch ( ( long )type )
  {
    case wkbUnknown:
      geom = QStringLiteral( "Unknown" );
      break;
    case wkbPoint:
      geom = QStringLiteral( "Point" );
      break;
    case wkbLineString:
      geom = QStringLiteral( "LineString" );
      break;
    case wkbPolygon:
      geom = QStringLiteral( "Polygon" );
      break;
    case wkbMultiPoint:
      geom = QStringLiteral( "MultiPoint" );
      break;
    case wkbMultiLineString:
      geom = QStringLiteral( "MultiLineString" );
      break;
    case wkbMultiPolygon:
      geom = QStringLiteral( "MultiPolygon" );
      break;
    case wkbGeometryCollection:
      geom = QStringLiteral( "GeometryCollection" );
      break;
    case wkbCircularString:
      geom = QStringLiteral( "CircularString" );
      break;
    case wkbCompoundCurve:
      geom = QStringLiteral( "CompoundCurve" );
      break;
    case wkbCurvePolygon:
      geom = QStringLiteral( "CurvePolygon" );
      break;
    case wkbMultiCurve:
      geom = QStringLiteral( "MultiCurve" );
      break;
    case wkbMultiSurface:
      geom = QStringLiteral( "MultiSurface" );
      break;
    case wkbCircularStringZ:
      geom = QStringLiteral( "CircularStringZ" );
      break;
    case wkbCompoundCurveZ:
      geom = QStringLiteral( "CompoundCurveZ" );
      break;
    case wkbCurvePolygonZ:
      geom = QStringLiteral( "CurvePolygonZ" );
      break;
    case wkbMultiCurveZ:
      geom = QStringLiteral( "MultiCurveZ" );
      break;
    case wkbMultiSurfaceZ:
      geom = QStringLiteral( "MultiSurfaceZ" );
      break;
    case wkbNone:
      geom = QStringLiteral( "None" );
      break;
    case wkbUnknown | wkb25DBit:
      geom = QStringLiteral( "Unknown25D" );
      break;
    case wkbPoint25D:
      geom = QStringLiteral( "Point25D" );
      break;
    case wkbLineString25D:
      geom = QStringLiteral( "LineString25D" );
      break;
    case wkbPolygon25D:
      geom = QStringLiteral( "Polygon25D" );
      break;
    case wkbMultiPoint25D:
      geom = QStringLiteral( "MultiPoint25D" );
      break;
    case wkbMultiLineString25D:
      geom = QStringLiteral( "MultiLineString25D" );
      break;
    case wkbMultiPolygon25D:
      geom = QStringLiteral( "MultiPolygon25D" );
      break;
    case wkbGeometryCollection25D:
      geom = QStringLiteral( "GeometryCollection25D" );
      break;
    default:
      // Do not use ':', as it will mess with the separator used by QgsSublayersDialog::populateLayers()
      geom = QStringLiteral( "Unknown WKB (%1)" ).arg( type );
  }
  return geom;
}

static OGRwkbGeometryType ogrWkbGeometryTypeFromName( const QString &typeName )
{
  if ( typeName == QLatin1String( "Point" ) ) return wkbPoint;
  else if ( typeName == QLatin1String( "LineString" ) ) return wkbLineString;
  else if ( typeName == QLatin1String( "Polygon" ) ) return wkbPolygon;
  else if ( typeName == QLatin1String( "MultiPoint" ) ) return wkbMultiPoint;
  else if ( typeName == QLatin1String( "MultiLineString" ) ) return wkbMultiLineString;
  else if ( typeName == QLatin1String( "MultiPolygon" ) ) return wkbMultiPolygon;
  else if ( typeName == QLatin1String( "GeometryCollection" ) ) return wkbGeometryCollection;
  else if ( typeName == QLatin1String( "None" ) ) return wkbNone;
  else if ( typeName == QLatin1String( "Point25D" ) ) return wkbPoint25D;
  else if ( typeName == QLatin1String( "LineString25D" ) ) return wkbLineString25D;
  else if ( typeName == QLatin1String( "Polygon25D" ) ) return wkbPolygon25D;
  else if ( typeName == QLatin1String( "MultiPoint25D" ) ) return wkbMultiPoint25D;
  else if ( typeName == QLatin1String( "MultiLineString25D" ) ) return wkbMultiLineString25D;
  else if ( typeName == QLatin1String( "MultiPolygon25D" ) ) return wkbMultiPolygon25D;
  else if ( typeName == QLatin1String( "GeometryCollection25D" ) ) return wkbGeometryCollection25D;
  return wkbUnknown;
}

QStringList QgsOgrProvider::subLayers() const
{
  QgsDebugMsg( "Entered." );
  if ( !mValid )
  {
    return QStringList();
  }

  if ( !mSubLayerList.isEmpty() )
    return mSubLayerList;

  for ( unsigned int i = 0; i < layerCount() ; i++ )
  {
    OGRLayerH layer = OGR_DS_GetLayer( ogrDataSource, i );
    OGRFeatureDefnH fdef = OGR_L_GetLayerDefn( layer );
    QString layerName = QString::fromUtf8( OGR_FD_GetName( fdef ) );
    OGRwkbGeometryType layerGeomType = OGR_FD_GetGeomType( fdef );

    // ignore this layer if a sublayer was requested and it is not this one
    if ( mIsSubLayer &&
         ( ( !mLayerName.isNull() && layerName != mLayerName ) ||
           ( mLayerName.isNull() && mLayerIndex >= 0 && i != ( unsigned int )mLayerIndex ) ) )
    {
      QgsDebugMsg( QString( "subLayers() ignoring layer #%1 (%2)" ).arg( i ).arg( layerName ) );
      continue;
    }

    if ( !mIsSubLayer && ( layerName == QLatin1String( "layer_styles" ) ||
                           layerName == QLatin1String( "qgis_projects" ) ) )
    {
      // Ignore layer_styles (coming from QGIS styling support) and
      // qgis_projects (coming from http://plugins.qgis.org/plugins/QgisGeopackage/)
      continue;
    }

    QgsDebugMsg( QString( "id = %1 name = %2 layerGeomType = %3" ).arg( i ).arg( layerName ).arg( layerGeomType ) );

    if ( wkbFlatten( layerGeomType ) != wkbUnknown )
    {
      int layerFeatureCount = OGR_L_GetFeatureCount( layer, 0 );

      QString geom = ogrWkbGeometryTypeName( layerGeomType );

      mSubLayerList << QStringLiteral( "%1:%2:%3:%4" ).arg( i ).arg( layerName, layerFeatureCount == -1 ? tr( "Unknown" ) : QString::number( layerFeatureCount ), geom );
    }
    else
    {
      QgsDebugMsg( "Unknown geometry type, count features for each geometry type" );
      // Add virtual sublayers for supported geometry types if layer type is unknown
      // Count features for geometry types
      QMap<OGRwkbGeometryType, int> fCount;
      // TODO: avoid reading attributes, setRelevantFields cannot be called here because it is not constant
      //setRelevantFields( ogrLayer, true, QgsAttributeList() );
      OGR_L_ResetReading( layer );
      OGRFeatureH fet;
      while ( ( fet = OGR_L_GetNextFeature( layer ) ) )
      {
        OGRGeometryH geom = OGR_F_GetGeometryRef( fet );
        if ( geom )
        {
          OGRwkbGeometryType gType = ogrWkbSingleFlatten( OGR_G_GetGeometryType( geom ) );
          fCount[gType] = fCount.value( gType ) + 1;
        }
        OGR_F_Destroy( fet );
      }
      OGR_L_ResetReading( layer );
      // it may happen that there are no features in the layer, in that case add unknown type
      // to show to user that the layer exists but it is empty
      if ( fCount.isEmpty() )
      {
        fCount[wkbUnknown] = 0;
      }

      // When there are CurvePolygons, promote Polygons
      if ( fCount.contains( wkbPolygon ) && fCount.contains( wkbCurvePolygon ) )
      {
        fCount[wkbCurvePolygon] += fCount.value( wkbPolygon );
        fCount.remove( wkbPolygon );
      }
      // When there are CompoundCurves, promote LineStrings and CircularStrings
      if ( fCount.contains( wkbLineString ) && fCount.contains( wkbCompoundCurve ) )
      {
        fCount[wkbCompoundCurve] += fCount.value( wkbLineString );
        fCount.remove( wkbLineString );
      }
      if ( fCount.contains( wkbCircularString ) && fCount.contains( wkbCompoundCurve ) )
      {
        fCount[wkbCompoundCurve] += fCount.value( wkbCircularString );
        fCount.remove( wkbCircularString );
      }

      bool bIs25D = wkbHasZ( layerGeomType );
      QMap<OGRwkbGeometryType, int>::const_iterator countIt = fCount.constBegin();
      for ( ; countIt != fCount.constEnd(); ++countIt )
      {
        QString geom = ogrWkbGeometryTypeName( ( bIs25D ) ? wkbSetZ( countIt.key() ) : countIt.key() );

        QString sl = QStringLiteral( "%1:%2:%3:%4" ).arg( i ).arg( layerName ).arg( fCount.value( countIt.key() ) ).arg( geom );
        QgsDebugMsg( "sub layer: " + sl );
        mSubLayerList << sl;
      }
    }
  }

  return mSubLayerList;
}

void QgsOgrProvider::setEncoding( const QString &e )
{
#if defined(OLCStringsAsUTF8)
  QgsSettings settings;
  if ( ( ogrDriverName == QLatin1String( "ESRI Shapefile" ) && settings.value( QStringLiteral( "qgis/ignoreShapeEncoding" ), true ).toBool() ) || !OGR_L_TestCapability( ogrLayer, OLCStringsAsUTF8 ) )
  {
    QgsVectorDataProvider::setEncoding( e );
  }
  else
  {
    QgsVectorDataProvider::setEncoding( QStringLiteral( "UTF-8" ) );
  }
#else
  QgsVectorDataProvider::setEncoding( e );
#endif

  loadFields();
}

// This is reused by dataItem
OGRwkbGeometryType QgsOgrProvider::getOgrGeomType( OGRLayerH ogrLayer )
{
  OGRFeatureDefnH fdef = OGR_L_GetLayerDefn( ogrLayer );
  OGRwkbGeometryType geomType = wkbUnknown;
  if ( fdef )
  {
    geomType = OGR_FD_GetGeomType( fdef );

    // Handle wkbUnknown and its Z/M variants. QGIS has no unknown Z/M variants,
    // so just use flat wkbUnknown
    if ( wkbFlatten( geomType ) == wkbUnknown )
      geomType = wkbUnknown;

    // Some ogr drivers (e.g. GML) are not able to determine the geometry type of a layer like this.
    // In such cases, we use virtual sublayers for each geometry if the layer contains
    // multiple geometries (see subLayers) otherwise we guess geometry type from the first
    // feature that has a geometry (limit us to a few features, not the whole layer)
    if ( geomType == wkbUnknown )
    {
      geomType = wkbNone;
      OGR_L_ResetReading( ogrLayer );
      for ( int i = 0; i < 10; i++ )
      {
        OGRFeatureH nextFeature = OGR_L_GetNextFeature( ogrLayer );
        if ( !nextFeature )
          break;

        OGRGeometryH geometry = OGR_F_GetGeometryRef( nextFeature );
        if ( geometry )
        {
          geomType = OGR_G_GetGeometryType( geometry );
        }
        OGR_F_Destroy( nextFeature );
        if ( geomType != wkbNone )
          break;
      }
      OGR_L_ResetReading( ogrLayer );
    }
  }
  return geomType;
}

void QgsOgrProvider::loadFields()
{
  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
  //the attribute fields need to be read again when the encoding changes
  mAttributeFields.clear();
  mDefaultValues.clear();
  if ( !ogrLayer )
    return;

  if ( mOgrGeometryTypeFilter != wkbUnknown )
  {
    mOGRGeomType = mOgrGeometryTypeFilter;
  }
  else
  {
    mOGRGeomType = getOgrGeomType( ogrLayer );
  }
  OGRFeatureDefnH fdef = OGR_L_GetLayerDefn( ogrLayer );
  if ( fdef )
  {
    // Expose the OGR FID if it comes from a "real" column (typically GPKG)
    // and make sure that this FID column is not exposed as a regular OGR field (shouldn't happen normally)
    mFirstFieldIsFid = !( EQUAL( OGR_L_GetFIDColumn( ogrLayer ), "" ) ) &&
                       OGR_FD_GetFieldIndex( fdef, OGR_L_GetFIDColumn( ogrLayer ) ) < 0;
    if ( mFirstFieldIsFid )
    {
      mAttributeFields.append(
        QgsField(
          OGR_L_GetFIDColumn( ogrLayer ),
          QVariant::LongLong,
          QStringLiteral( "Integer64" )
        )
      );
    }

    for ( int i = 0; i < OGR_FD_GetFieldCount( fdef ); ++i )
    {
      OGRFieldDefnH fldDef = OGR_FD_GetFieldDefn( fdef, i );
      OGRFieldType ogrType = OGR_Fld_GetType( fldDef );
      QVariant::Type varType;
      switch ( ogrType )
      {
        case OFTInteger:
          varType = QVariant::Int;
          break;
        case OFTInteger64:
          varType = QVariant::LongLong;
          break;
        case OFTReal:
          varType = QVariant::Double;
          break;
        case OFTDate:
          varType = QVariant::Date;
          break;
        case OFTTime:
          varType = QVariant::Time;
          break;
        case OFTDateTime:
          varType = QVariant::DateTime;
          break;
        case OFTString:
        default:
          varType = QVariant::String; // other unsupported, leave it as a string
      }

      //TODO: fix this hack
#ifdef ANDROID
      QString name = OGR_Fld_GetNameRef( fldDef );
#else
      QString name = textEncoding()->toUnicode( OGR_Fld_GetNameRef( fldDef ) );
#endif

      if ( mAttributeFields.indexFromName( name ) != -1 )
      {

        QString tmpname = name + "_%1";
        int fix = 0;

        while ( mAttributeFields.indexFromName( name ) != -1 )
        {
          name = tmpname.arg( ++fix );
        }
      }

      int width = OGR_Fld_GetWidth( fldDef );
      int prec = OGR_Fld_GetPrecision( fldDef );
      if ( prec > 0 )
        width -= 1;

      QgsField newField = QgsField(
                            name,
                            varType,
#ifdef ANDROID
                            OGR_GetFieldTypeName( ogrType ),
#else
                            textEncoding()->toUnicode( OGR_GetFieldTypeName( ogrType ) ),
#endif
                            width, prec
                          );

      // check if field is nullable
      bool nullable = OGR_Fld_IsNullable( fldDef );
      if ( !nullable )
      {
        QgsFieldConstraints constraints;
        constraints.setConstraint( QgsFieldConstraints::ConstraintNotNull, QgsFieldConstraints::ConstraintOriginProvider );
        newField.setConstraints( constraints );
      }

      // check if field has default value
      QString defaultValue = textEncoding()->toUnicode( OGR_Fld_GetDefault( fldDef ) );
      if ( !defaultValue.isEmpty() && !OGR_Fld_IsDefaultDriverSpecific( fldDef ) )
      {
        mDefaultValues.insert( i + ( mFirstFieldIsFid ? 1 : 0 ), defaultValue );
      }

      mAttributeFields.append( newField );
    }
  }
}


QString QgsOgrProvider::storageType() const
{
  // Delegate to the driver loaded in by OGR
  return ogrDriverName;
}


void QgsOgrProvider::setRelevantFields( OGRLayerH ogrLayer, bool fetchGeometry, const QgsAttributeList &fetchAttributes )
{
  QgsOgrProviderUtils::setRelevantFields( ogrLayer, mAttributeFields.count(), fetchGeometry, fetchAttributes, mFirstFieldIsFid );
}


void QgsOgrProviderUtils::setRelevantFields( OGRLayerH ogrLayer, int fieldCount, bool fetchGeometry, const QgsAttributeList &fetchAttributes, bool firstAttrIsFid )
{
  if ( OGR_L_TestCapability( ogrLayer, OLCIgnoreFields ) )
  {
    QVector<const char *> ignoredFields;
    OGRFeatureDefnH featDefn = OGR_L_GetLayerDefn( ogrLayer );
    for ( int i = ( firstAttrIsFid ? 1 : 0 ); i < fieldCount; i++ )
    {
      if ( !fetchAttributes.contains( i ) )
      {
        // add to ignored fields
        ignoredFields.append( OGR_Fld_GetNameRef( OGR_FD_GetFieldDefn( featDefn, firstAttrIsFid ? i - 1 : i ) ) );
      }
    }

    if ( !fetchGeometry )
      ignoredFields.append( "OGR_GEOMETRY" );
    ignoredFields.append( "OGR_STYLE" ); // not used by QGIS
    ignoredFields.append( nullptr );

    OGR_L_SetIgnoredFields( ogrLayer, ignoredFields.data() );
  }
}

QgsFeatureIterator QgsOgrProvider::getFeatures( const QgsFeatureRequest &request ) const
{
  return QgsFeatureIterator( new QgsOgrFeatureIterator( static_cast<QgsOgrFeatureSource *>( featureSource() ), true, request ) );
}


unsigned char *QgsOgrProvider::getGeometryPointer( OGRFeatureH fet )
{
  OGRGeometryH geom = OGR_F_GetGeometryRef( fet );
  unsigned char *gPtr = nullptr;

  if ( !geom )
    return nullptr;

  // get the wkb representation
  gPtr = new unsigned char[OGR_G_WkbSize( geom )];

  OGR_G_ExportToWkb( geom, ( OGRwkbByteOrder ) QgsApplication::endian(), gPtr );
  return gPtr;
}


QgsRectangle QgsOgrProvider::extent() const
{
  if ( !mExtent )
  {
    mExtent = new OGREnvelope();

    // get the extent_ (envelope) of the layer
    QgsDebugMsg( "Starting get extent" );

#if defined(GDAL_COMPUTE_VERSION) && GDAL_VERSION_NUM >= GDAL_COMPUTE_VERSION(2,1,2)
    if ( mForceRecomputeExtent && mValid && ogrDriverName == "GPKG" && ogrDataSource && ogrOrigLayer )
    {
      QByteArray layerName = OGR_FD_GetName( OGR_L_GetLayerDefn( ogrOrigLayer ) );
      // works with unquoted layerName
      QByteArray sql = QByteArray( "RECOMPUTE EXTENT ON " ) + layerName;
      QgsDebugMsg( QString( "SQL: %1" ).arg( QString::fromUtf8( sql ) ) );
      OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
    }
#endif

    // TODO: This can be expensive, do we really need it!
    if ( ogrLayer == ogrOrigLayer )
    {
      OGR_L_GetExtent( ogrLayer, mExtent, true );
    }
    else
    {
      mExtent->MinX = std::numeric_limits<double>::max();
      mExtent->MinY = std::numeric_limits<double>::max();
      mExtent->MaxX = -std::numeric_limits<double>::max();
      mExtent->MaxY = -std::numeric_limits<double>::max();

      OGRFeatureH f;

      OGR_L_ResetReading( ogrLayer );
      while ( ( f = OGR_L_GetNextFeature( ogrLayer ) ) )
      {
        OGRGeometryH g = OGR_F_GetGeometryRef( f );
        if ( g )
        {
          OGREnvelope env;
          OGR_G_GetEnvelope( g, &env );

          mExtent->MinX = std::min( mExtent->MinX, env.MinX );
          mExtent->MinY = std::min( mExtent->MinY, env.MinY );
          mExtent->MaxX = std::max( mExtent->MaxX, env.MaxX );
          mExtent->MaxY = std::max( mExtent->MaxY, env.MaxY );
        }

        OGR_F_Destroy( f );
      }
      OGR_L_ResetReading( ogrLayer );
    }

    QgsDebugMsg( "Finished get extent" );
  }

  mExtentRect.set( mExtent->MinX, mExtent->MinY, mExtent->MaxX, mExtent->MaxY );
  return mExtentRect;
}

QVariant QgsOgrProvider::defaultValue( int fieldId ) const
{
  if ( fieldId < 0 || fieldId >= mAttributeFields.count() )
    return QVariant();

  QString defaultVal = mDefaultValues.value( fieldId, QString() );
  if ( defaultVal.isEmpty() )
    return QVariant();

  QVariant resultVar = defaultVal;
  if ( defaultVal == QStringLiteral( "CURRENT_TIMESTAMP" ) )
    resultVar = QDateTime::currentDateTime();
  else if ( defaultVal == QStringLiteral( "CURRENT_DATE" ) )
    resultVar = QDate::currentDate();
  else if ( defaultVal == QStringLiteral( "CURRENT_TIME" ) )
    resultVar = QTime::currentTime();
  else if ( defaultVal.startsWith( '\'' ) )
  {
    defaultVal = defaultVal.remove( 0, 1 );
    defaultVal.chop( 1 );
    defaultVal.replace( "''", "'" );
    resultVar = defaultVal;
  }

  ( void )mAttributeFields.at( fieldId ).convertCompatible( resultVar );
  return resultVar;
}

void QgsOgrProvider::updateExtents()
{
  invalidateCachedExtent( true );
}

void QgsOgrProvider::invalidateCachedExtent( bool bForceRecomputeExtent )
{
  mForceRecomputeExtent = bForceRecomputeExtent;
  delete mExtent;
  mExtent = nullptr;
}

size_t QgsOgrProvider::layerCount() const
{
  if ( !mValid )
    return 0;
  return OGR_DS_GetLayerCount( ogrDataSource );
} // QgsOgrProvider::layerCount()


/**
 * Return the feature type
 */
QgsWkbTypes::Type QgsOgrProvider::wkbType() const
{
  QgsWkbTypes::Type wkb = static_cast<QgsWkbTypes::Type>( mOGRGeomType );
  if ( ogrDriverName == QLatin1String( "ESRI Shapefile" ) && ( wkb == QgsWkbTypes::LineString || wkb == QgsWkbTypes::Polygon ) )
  {
    wkb = QgsWkbTypes::multiType( wkb );
  }
  return wkb;
}

/**
 * Return the feature count
 */
long QgsOgrProvider::featureCount() const
{
  return mFeaturesCounted;
}


QgsFields QgsOgrProvider::fields() const
{
  return mAttributeFields;
}


//TODO - add sanity check for shape file layers, to include checking to
//       see if the .shp, .dbf, .shx files are all present and the layer
//       actually has features
bool QgsOgrProvider::isValid() const
{
  return mValid;
}

// Drivers may be more tolerant than we really wish (e.g. GeoPackage driver
// may accept any geometry type)
OGRGeometryH QgsOgrProvider::ConvertGeometryIfNecessary( OGRGeometryH hGeom )
{
  if ( hGeom == nullptr )
    return hGeom;
  OGRwkbGeometryType layerGeomType = OGR_L_GetGeomType( ogrLayer );
  OGRwkbGeometryType flattenLayerGeomType = wkbFlatten( layerGeomType );
  OGRwkbGeometryType geomType = OGR_G_GetGeometryType( hGeom );
  OGRwkbGeometryType flattenGeomType = wkbFlatten( geomType );

  if ( flattenLayerGeomType == wkbUnknown || flattenLayerGeomType == flattenGeomType )
  {
    return hGeom;
  }
  if ( flattenLayerGeomType == wkbMultiPolygon && flattenGeomType == wkbPolygon )
  {
    return OGR_G_ForceToMultiPolygon( hGeom );
  }
  if ( flattenLayerGeomType == wkbMultiLineString && flattenGeomType == wkbLineString )
  {
    return OGR_G_ForceToMultiLineString( hGeom );
  }

  return OGR_G_ForceTo( hGeom, layerGeomType, nullptr );
}

bool QgsOgrProvider::addFeaturePrivate( QgsFeature &f, Flags flags )
{
  bool returnValue = true;
  OGRFeatureDefnH fdef = OGR_L_GetLayerDefn( ogrLayer );
  OGRFeatureH feature = OGR_F_Create( fdef );

  if ( f.hasGeometry() )
  {
    QByteArray wkb( f.geometry().exportToWkb() );
    OGRGeometryH geom = nullptr;

    if ( !wkb.isEmpty() )
    {
      if ( OGR_G_CreateFromWkb( reinterpret_cast<unsigned char *>( const_cast<char *>( wkb.constData() ) ), nullptr, &geom, wkb.length() ) != OGRERR_NONE )
      {
        pushError( tr( "OGR error creating wkb for feature %1: %2" ).arg( f.id() ).arg( CPLGetLastErrorMsg() ) );
        return false;
      }

      geom = ConvertGeometryIfNecessary( geom );

      OGR_F_SetGeometryDirectly( feature, geom );
    }
  }

  QgsAttributes attrs = f.attributes();

  QgsLocaleNumC l;

  int qgisAttId = ( mFirstFieldIsFid ) ? 1 : 0;
  // If the first attribute is the FID and the user has set it, then use it
  if ( mFirstFieldIsFid && attrs.count() > 0 )
  {
    QVariant attrFid = attrs.at( 0 );
    if ( !attrFid.isNull() )
    {
      bool ok = false;
      qlonglong id = attrFid.toLongLong( &ok );
      if ( ok )
      {
        OGR_F_SetFID( feature, static_cast<GIntBig>( id ) );
      }
    }
  }

  //add possible attribute information
  for ( int ogrAttId = 0; qgisAttId < attrs.count(); ++qgisAttId, ++ogrAttId )
  {
    // don't try to set field from attribute map if it's not present in layer
    if ( ogrAttId >= OGR_FD_GetFieldCount( fdef ) )
      continue;

    //if(!s.isEmpty())
    // continue;
    //
    OGRFieldDefnH fldDef = OGR_FD_GetFieldDefn( fdef, ogrAttId );
    OGRFieldType type = OGR_Fld_GetType( fldDef );

    QVariant attrVal = attrs.at( qgisAttId );
    if ( attrVal.isNull() || ( type != OFTString && attrVal.toString().isEmpty() ) )
    {
// Starting with GDAL 2.2, there are 2 concepts: unset fields and null fields
// whereas previously there was only unset fields. For a GeoJSON output,
// leaving a field unset will cause it to not appear at all in the output
// feature.
// When all features of a layer have a field unset, this would cause the
// field to not be present at all in the output, and thus on reading to
// have disappeared. #16812
#ifdef OGRNullMarker
      OGR_F_SetFieldNull( feature, ogrAttId );
#else
      OGR_F_UnsetField( feature, ogrAttId );
#endif
    }
    else
    {
      switch ( type )
      {
        case OFTInteger:
          OGR_F_SetFieldInteger( feature, ogrAttId, attrVal.toInt() );
          break;


        case OFTInteger64:
          OGR_F_SetFieldInteger64( feature, ogrAttId, attrVal.toLongLong() );
          break;

        case OFTReal:
          OGR_F_SetFieldDouble( feature, ogrAttId, attrVal.toDouble() );
          break;

        case OFTDate:
          OGR_F_SetFieldDateTime( feature, ogrAttId,
                                  attrVal.toDate().year(),
                                  attrVal.toDate().month(),
                                  attrVal.toDate().day(),
                                  0, 0, 0,
                                  0 );
          break;

        case OFTTime:
          OGR_F_SetFieldDateTime( feature, ogrAttId,
                                  0, 0, 0,
                                  attrVal.toTime().hour(),
                                  attrVal.toTime().minute(),
                                  attrVal.toTime().second(),
                                  0 );
          break;

        case OFTDateTime:
          OGR_F_SetFieldDateTime( feature, ogrAttId,
                                  attrVal.toDateTime().date().year(),
                                  attrVal.toDateTime().date().month(),
                                  attrVal.toDateTime().date().day(),
                                  attrVal.toDateTime().time().hour(),
                                  attrVal.toDateTime().time().minute(),
                                  attrVal.toDateTime().time().second(),
                                  0 );
          break;

        case OFTString:
          QgsDebugMsg( QString( "Writing string attribute %1 with %2, encoding %3" )
                       .arg( qgisAttId )
                       .arg( attrVal.toString(),
                             textEncoding()->name().data() ) );
          OGR_F_SetFieldString( feature, ogrAttId, textEncoding()->fromUnicode( attrVal.toString() ).constData() );
          break;

        default:
          QgsMessageLog::logMessage( tr( "type %1 for attribute %2 not found" ).arg( type ).arg( qgisAttId ), tr( "OGR" ) );
          break;
      }
    }
  }

  if ( OGR_L_CreateFeature( ogrLayer, feature ) != OGRERR_NONE )
  {
    pushError( tr( "OGR error creating feature %1: %2" ).arg( f.id() ).arg( CPLGetLastErrorMsg() ) );
    returnValue = false;
  }
  else if ( !( flags & QgsFeatureSink::FastInsert ) )
  {
    QgsFeatureId id = static_cast<QgsFeatureId>( OGR_F_GetFID( feature ) );
    if ( id >= 0 )
    {
      f.setId( id );

      if ( mFirstFieldIsFid && attrs.count() > 0 )
      {
        f.setAttribute( 0, id );
      }
    }
  }
  OGR_F_Destroy( feature );

  return returnValue;
}


bool QgsOgrProvider::addFeatures( QgsFeatureList &flist, Flags flags )
{
  if ( !doInitialActionsForEdition() )
    return false;

  setRelevantFields( ogrLayer, true, attributeIndexes() );

  const bool inTransaction = startTransaction();

  bool returnvalue = true;
  for ( QgsFeatureList::iterator it = flist.begin(); it != flist.end(); ++it )
  {
    if ( !addFeaturePrivate( *it, flags ) )
    {
      returnvalue = false;
    }
  }

  if ( inTransaction )
  {
    commitTransaction();
  }

  if ( !syncToDisc() )
  {
    returnvalue = false;
  }

  recalculateFeatureCount();

  if ( returnvalue )
    clearMinMaxCache();

  return returnvalue;
}

bool QgsOgrProvider::addAttributes( const QList<QgsField> &attributes )
{
  if ( !doInitialActionsForEdition() )
    return false;

  if ( ogrDriverName == QLatin1String( "MapInfo File" ) )
  {
    // adding attributes in mapinfo requires to be able to delete the .dat file
    // so drop any cached connections.
    QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
  }

  bool returnvalue = true;

  QMap< QString, QgsField > mapFieldNameToOriginalField;

  for ( QList<QgsField>::const_iterator iter = attributes.constBegin(); iter != attributes.constEnd(); ++iter )
  {
    mapFieldNameToOriginalField[ iter->name()] = *iter;

    OGRFieldType type;

    switch ( iter->type() )
    {
      case QVariant::Int:
        type = OFTInteger;
        break;
      case QVariant::LongLong:
      {
        const char *pszDataTypes = GDALGetMetadataItem( ogrDriver, GDAL_DMD_CREATIONFIELDDATATYPES, NULL );
        if ( pszDataTypes && strstr( pszDataTypes, "Integer64" ) )
          type = OFTInteger64;
        else
        {
          type = OFTReal;
        }
        break;
      }
      case QVariant::Double:
        type = OFTReal;
        break;
      case QVariant::Date:
        type = OFTDate;
        break;
      case QVariant::Time:
        type = OFTTime;
        break;
      case QVariant::DateTime:
        type = OFTDateTime;
        break;
      case QVariant::String:
        type = OFTString;
        break;
      default:
        pushError( tr( "type %1 for field %2 not found" ).arg( iter->typeName(), iter->name() ) );
        returnvalue = false;
        continue;
    }

    OGRFieldDefnH fielddefn = OGR_Fld_Create( textEncoding()->fromUnicode( iter->name() ).constData(), type );
    int width = iter->length();
    if ( iter->precision() )
      width += 1;
    OGR_Fld_SetWidth( fielddefn, width );
    OGR_Fld_SetPrecision( fielddefn, iter->precision() );

    if ( OGR_L_CreateField( ogrLayer, fielddefn, true ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error creating field %1: %2" ).arg( iter->name(), CPLGetLastErrorMsg() ) );
      returnvalue = false;
    }
    OGR_Fld_Destroy( fielddefn );
  }
  loadFields();

  // The check in QgsVectorLayerEditBuffer::commitChanges() is questionable with
  // real-world drivers that might only be able to satisfy request only partially.
  // So to avoid erroring out, patch field type, width and precision to match
  // what was requested.
  // For example in case of Integer64->Real mapping so that QVariant::LongLong is
  // still returned to the caller
  // Or if a field width was specified but not strictly enforced by the driver (#15614)
  for ( QMap< QString, QgsField >::const_iterator it = mapFieldNameToOriginalField.constBegin();
        it != mapFieldNameToOriginalField.constEnd(); ++it )
  {
    int idx = mAttributeFields.lookupField( it.key() );
    if ( idx >= 0 )
    {
      mAttributeFields[ idx ].setType( it->type() );
      mAttributeFields[ idx ].setLength( it->length() );
      mAttributeFields[ idx ].setPrecision( it->precision() );
    }
  }

  return returnvalue;
}

bool QgsOgrProvider::deleteAttributes( const QgsAttributeIds &attributes )
{
  if ( !doInitialActionsForEdition() )
    return false;

  bool res = true;
  QList<int> attrsLst = attributes.toList();
  // sort in descending order
  std::sort( attrsLst.begin(), attrsLst.end(), std::greater<int>() );
  Q_FOREACH ( int attr, attrsLst )
  {
    if ( mFirstFieldIsFid )
    {
      if ( attr == 0 )
      {
        pushError( tr( "Cannot delete feature id column" ) );
        res = false;
        break;
      }
      else
      {
        --attr;
      }
    }
    if ( OGR_L_DeleteField( ogrLayer, attr ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error deleting field %1: %2" ).arg( attr ).arg( CPLGetLastErrorMsg() ) );
      res = false;
    }
  }
  loadFields();
  return res;
}

bool QgsOgrProvider::renameAttributes( const QgsFieldNameMap &renamedAttributes )
{
  if ( !doInitialActionsForEdition() )
    return false;

  QgsFieldNameMap::const_iterator renameIt = renamedAttributes.constBegin();
  bool result = true;
  for ( ; renameIt != renamedAttributes.constEnd(); ++renameIt )
  {
    int fieldIndex = renameIt.key();
    if ( fieldIndex < 0 || fieldIndex >= mAttributeFields.count() )
    {
      pushError( tr( "Invalid attribute index" ) );
      result = false;
      continue;
    }
    if ( mAttributeFields.indexFromName( renameIt.value() ) >= 0 )
    {
      //field name already in use
      pushError( tr( "Error renaming field %1: name '%2' already exists" ).arg( fieldIndex ).arg( renameIt.value() ) );
      result = false;
      continue;
    }
    int ogrFieldIndex = fieldIndex;
    if ( mFirstFieldIsFid )
    {
      ogrFieldIndex -= 1;
      if ( ogrFieldIndex < 0 )
      {
        pushError( tr( "Invalid attribute index" ) );
        result = false;
        continue;
      }
    }

    //type does not matter, it will not be used
    OGRFieldDefnH fld = OGR_Fld_Create( textEncoding()->fromUnicode( renameIt.value() ), OFTReal );
    if ( OGR_L_AlterFieldDefn( ogrLayer, ogrFieldIndex, fld, ALTER_NAME_FLAG ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error renaming field %1: %2" ).arg( fieldIndex ).arg( CPLGetLastErrorMsg() ) );
      result = false;
    }
    OGR_Fld_Destroy( fld );
  }
  loadFields();
  return result;
}

bool QgsOgrProvider::startTransaction()
{
  bool inTransaction = false;
  if ( OGR_L_TestCapability( ogrLayer, OLCTransactions ) )
  {
    // A transaction might already be active, so be robust on failed
    // StartTransaction.
    CPLPushErrorHandler( CPLQuietErrorHandler );
    inTransaction = ( OGR_L_StartTransaction( ogrLayer ) == OGRERR_NONE );
    CPLPopErrorHandler();
  }
  return inTransaction;
}


bool QgsOgrProvider::commitTransaction()
{
  if ( OGR_L_CommitTransaction( ogrLayer ) != OGRERR_NONE )
  {
    pushError( tr( "OGR error committing transaction: %1" ).arg( CPLGetLastErrorMsg() ) );
    return false;
  }
  return true;
}


bool QgsOgrProvider::changeAttributeValues( const QgsChangedAttributesMap &attr_map )
{
  if ( !doInitialActionsForEdition() )
    return false;

  if ( attr_map.isEmpty() )
    return true;

  clearMinMaxCache();

  setRelevantFields( ogrLayer, true, attributeIndexes() );

  const bool inTransaction = startTransaction();

  for ( QgsChangedAttributesMap::const_iterator it = attr_map.begin(); it != attr_map.end(); ++it )
  {
    QgsFeatureId fid = it.key();

    if ( FID_TO_NUMBER( fid ) > std::numeric_limits<long>::max() )
    {
      pushError( tr( "OGR error on feature %1: id too large" ).arg( fid ) );
      continue;
    }

    const QgsAttributeMap &attr = it.value();
    if ( attr.isEmpty() )
      continue;

    OGRFeatureH of = OGR_L_GetFeature( ogrLayer, static_cast<long>( FID_TO_NUMBER( fid ) ) );
    if ( !of )
    {
      pushError( tr( "Feature %1 for attribute update not found." ).arg( fid ) );
      continue;
    }
    OGR_L_ResetReading( ogrLayer ); // needed for SQLite-based to clear iterator

    QgsLocaleNumC l;

    for ( QgsAttributeMap::const_iterator it2 = attr.begin(); it2 != attr.end(); ++it2 )
    {
      int f = it2.key();
      if ( mFirstFieldIsFid )
      {
        if ( f == 0 )
        {
          if ( it2->toLongLong() != fid )
          {
            pushError( tr( "Changing feature id of feature %1 is not allowed." ).arg( fid ) );
            continue;
          }
        }
        else
        {
          --f;
        }
      }

      OGRFieldDefnH fd = OGR_F_GetFieldDefnRef( of, f );
      if ( !fd )
      {
        pushError( tr( "Field %1 of feature %2 doesn't exist." ).arg( f ).arg( fid ) );
        continue;
      }

      OGRFieldType type = OGR_Fld_GetType( fd );

      if ( it2->isNull() || ( type != OFTString && it2->toString().isEmpty() ) )
      {
// Starting with GDAL 2.2, there are 2 concepts: unset fields and null fields
// whereas previously there was only unset fields. For a GeoJSON output,
// leaving a field unset will cause it to not appear at all in the output
// feature.
// When all features of a layer have a field unset, this would cause the
// field to not be present at all in the output, and thus on reading to
// have disappeared. #16812
#ifdef OGRNullMarker
        OGR_F_SetFieldNull( of, f );
#else
        OGR_F_UnsetField( of, f );
#endif
      }
      else
      {

        switch ( type )
        {
          case OFTInteger:
            OGR_F_SetFieldInteger( of, f, it2->toInt() );
            break;
          case OFTInteger64:
            OGR_F_SetFieldInteger64( of, f, it2->toLongLong() );
            break;
          case OFTReal:
            OGR_F_SetFieldDouble( of, f, it2->toDouble() );
            break;
          case OFTDate:
            OGR_F_SetFieldDateTime( of, f,
                                    it2->toDate().year(),
                                    it2->toDate().month(),
                                    it2->toDate().day(),
                                    0, 0, 0,
                                    0 );
            break;
          case OFTTime:
            OGR_F_SetFieldDateTime( of, f,
                                    0, 0, 0,
                                    it2->toTime().hour(),
                                    it2->toTime().minute(),
                                    it2->toTime().second(),
                                    0 );
            break;
          case OFTDateTime:
            OGR_F_SetFieldDateTime( of, f,
                                    it2->toDateTime().date().year(),
                                    it2->toDateTime().date().month(),
                                    it2->toDateTime().date().day(),
                                    it2->toDateTime().time().hour(),
                                    it2->toDateTime().time().minute(),
                                    it2->toDateTime().time().second(),
                                    0 );
            break;
          case OFTString:
            OGR_F_SetFieldString( of, f, textEncoding()->fromUnicode( it2->toString() ).constData() );
            break;
          default:
            pushError( tr( "Type %1 of attribute %2 of feature %3 unknown." ).arg( type ).arg( fid ).arg( f ) );
            break;
        }
      }
    }

    if ( OGR_L_SetFeature( ogrLayer, of ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error setting feature %1: %2" ).arg( fid ).arg( CPLGetLastErrorMsg() ) );
    }

    OGR_F_Destroy( of );
  }

  if ( inTransaction )
  {
    commitTransaction();
  }

  if ( OGR_L_SyncToDisk( ogrLayer ) != OGRERR_NONE )
  {
    pushError( tr( "OGR error syncing to disk: %1" ).arg( CPLGetLastErrorMsg() ) );
  }
  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
  return true;
}

bool QgsOgrProvider::changeGeometryValues( const QgsGeometryMap &geometry_map )
{
  if ( !doInitialActionsForEdition() )
    return false;

  setRelevantFields( ogrLayer, true, attributeIndexes() );

  const bool inTransaction = startTransaction();

  for ( QgsGeometryMap::const_iterator it = geometry_map.constBegin(); it != geometry_map.constEnd(); ++it )
  {
    if ( FID_TO_NUMBER( it.key() ) > std::numeric_limits<long>::max() )
    {
      pushError( tr( "OGR error on feature %1: id too large" ).arg( it.key() ) );
      continue;
    }

    OGRFeatureH theOGRFeature = OGR_L_GetFeature( ogrLayer, static_cast<long>( FID_TO_NUMBER( it.key() ) ) );
    if ( !theOGRFeature )
    {
      pushError( tr( "OGR error changing geometry: feature %1 not found" ).arg( it.key() ) );
      continue;
    }
    OGR_L_ResetReading( ogrLayer ); // needed for SQLite-based to clear iterator

    OGRGeometryH newGeometry = nullptr;
    QByteArray wkb = it->exportToWkb();
    // We might receive null geometries. It is OK, but don't go through the
    // OGR_G_CreateFromWkb() route then
    if ( !wkb.isEmpty() )
    {
      //create an OGRGeometry
      if ( OGR_G_CreateFromWkb( reinterpret_cast<unsigned char *>( const_cast<char *>( wkb.constData() ) ),
                                OGR_L_GetSpatialRef( ogrLayer ),
                                &newGeometry,
                                wkb.length() ) != OGRERR_NONE )
      {
        pushError( tr( "OGR error creating geometry for feature %1: %2" ).arg( it.key() ).arg( CPLGetLastErrorMsg() ) );
        OGR_G_DestroyGeometry( newGeometry );
        newGeometry = nullptr;
        OGR_F_Destroy( theOGRFeature );
        continue;
      }

      if ( !newGeometry )
      {
        pushError( tr( "OGR error in feature %1: geometry is null" ).arg( it.key() ) );
        OGR_F_Destroy( theOGRFeature );
        continue;
      }

      newGeometry = ConvertGeometryIfNecessary( newGeometry );
    }

    //set the new geometry
    if ( OGR_F_SetGeometryDirectly( theOGRFeature, newGeometry ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error setting geometry of feature %1: %2" ).arg( it.key() ).arg( CPLGetLastErrorMsg() ) );
      // Shouldn't happen normally. If it happens, ownership of the geometry
      // may be not really well defined, so better not destroy it, but just
      // the feature.
      OGR_F_Destroy( theOGRFeature );
      continue;
    }


    if ( OGR_L_SetFeature( ogrLayer, theOGRFeature ) != OGRERR_NONE )
    {
      pushError( tr( "OGR error setting feature %1: %2" ).arg( it.key() ).arg( CPLGetLastErrorMsg() ) );
      OGR_F_Destroy( theOGRFeature );
      continue;
    }
    mShapefileMayBeCorrupted = true;

    invalidateCachedExtent( true );

    OGR_F_Destroy( theOGRFeature );
  }

  if ( inTransaction )
  {
    commitTransaction();
  }

  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
  return syncToDisc();
}

bool QgsOgrProvider::createSpatialIndex()
{
  if ( !doInitialActionsForEdition() )
    return false;

  if ( ogrDriverName != QLatin1String( "ESRI Shapefile" ) )
    return false;

  QByteArray layerName = OGR_FD_GetName( OGR_L_GetLayerDefn( ogrOrigLayer ) );

  if ( ogrDataSource )
  {
    QByteArray sql = "CREATE SPATIAL INDEX ON " + quotedIdentifier( layerName );  // quote the layer name so spaces are handled
    QgsDebugMsg( QString( "SQL: %1" ).arg( QString::fromUtf8( sql ) ) );
    OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), OGR_L_GetSpatialFilter( ogrOrigLayer ), nullptr );
  }

  QFileInfo fi( mFilePath );     // to get the base name
  //find out, if the .qix file is there
  QFile indexfile( fi.path().append( '/' ).append( fi.completeBaseName() ).append( ".qix" ) );
  return indexfile.exists();
}

bool QgsOgrProvider::createAttributeIndex( int field )
{
  if ( !doInitialActionsForEdition() )
    return false;

  QByteArray quotedLayerName = quotedIdentifier( OGR_FD_GetName( OGR_L_GetLayerDefn( ogrOrigLayer ) ) );
  QByteArray dropSql = "DROP INDEX ON " + quotedLayerName;
  OGR_DS_ExecuteSQL( ogrDataSource, dropSql.constData(), OGR_L_GetSpatialFilter( ogrOrigLayer ), nullptr );
  QByteArray createSql = "CREATE INDEX ON " + quotedLayerName + " USING " + textEncoding()->fromUnicode( fields().at( field ).name() );
  OGR_DS_ExecuteSQL( ogrDataSource, createSql.constData(), OGR_L_GetSpatialFilter( ogrOrigLayer ), nullptr );

  QFileInfo fi( mFilePath );     // to get the base name
  //find out, if the .idm file is there
  QFile indexfile( fi.path().append( '/' ).append( fi.completeBaseName() ).append( ".idm" ) );
  return indexfile.exists();
}

bool QgsOgrProvider::deleteFeatures( const QgsFeatureIds &id )
{
  if ( !doInitialActionsForEdition() )
    return false;

  const bool inTransaction = startTransaction();

  bool returnvalue = true;
  for ( QgsFeatureIds::const_iterator it = id.begin(); it != id.end(); ++it )
  {
    if ( !deleteFeature( *it ) )
    {
      returnvalue = false;
    }
  }

  if ( inTransaction )
  {
    commitTransaction();
  }

  if ( !syncToDisc() )
  {
    returnvalue = false;
  }

  recalculateFeatureCount();

  clearMinMaxCache();

  invalidateCachedExtent( true );

  return returnvalue;
}

bool QgsOgrProvider::deleteFeature( QgsFeatureId id )
{
  if ( !doInitialActionsForEdition() )
    return false;

  if ( FID_TO_NUMBER( id ) > std::numeric_limits<long>::max() )
  {
    pushError( tr( "OGR error on feature %1: id too large" ).arg( id ) );
    return false;
  }

  if ( OGR_L_DeleteFeature( ogrLayer, FID_TO_NUMBER( id ) ) != OGRERR_NONE )
  {
    pushError( tr( "OGR error deleting feature %1: %2" ).arg( id ).arg( CPLGetLastErrorMsg() ) );
    return false;
  }

  mShapefileMayBeCorrupted = true;

  return true;
}

bool QgsOgrProvider::doInitialActionsForEdition()
{
  if ( !mValid )
    return false;

  if ( !mWriteAccess && mWriteAccessPossible && mDynamicWriteAccess )
  {
    QgsDebugMsg( "Enter update mode implictly" );
    if ( !enterUpdateMode() )
      return false;
  }

  return true;
}

QgsVectorDataProvider::Capabilities QgsOgrProvider::capabilities() const
{
  return mCapabilities;
}

void QgsOgrProvider::computeCapabilities()
{
  QgsVectorDataProvider::Capabilities ability = 0;

  // collect abilities reported by OGR
  if ( ogrLayer )
  {
    // Whilst the OGR documentation (e.g. at
    // http://www.gdal.org/ogr/classOGRLayer.html#a17) states "The capability
    // codes that can be tested are represented as strings, but #defined
    // constants exists to ensure correct spelling", we always use strings
    // here.  This is because older versions of OGR don't always have all
    // the #defines we want to test for here.

    if ( OGR_L_TestCapability( ogrLayer, "RandomRead" ) )
      // true if the GetFeature() method works *efficiently* for this layer.
      // TODO: Perhaps influence if QGIS caches into memory
      //       (vs read from disk every time) based on this setting.
    {
      // the latter flag is here just for compatibility
      ability |= QgsVectorDataProvider::SelectAtId;
    }

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "SequentialWrite" ) )
      // true if the CreateFeature() method works for this layer.
    {
      ability |= QgsVectorDataProvider::AddFeatures;
    }

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "DeleteFeature" ) )
      // true if this layer can delete its features
    {
      ability |= DeleteFeatures;
    }

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "RandomWrite" ) )
      // true if the SetFeature() method is operational on this layer.
    {
      // TODO According to http://shapelib.maptools.org/ (Shapefile C Library V1.2)
      // TODO "You can't modify the vertices of existing structures".
      // TODO Need to work out versions of shapelib vs versions of GDAL/OGR
      // TODO And test appropriately.

      ability |= ChangeAttributeValues;
      ability |= ChangeGeometries;
    }

#if 0
    if ( OGR_L_TestCapability( ogrLayer, "FastSpatialFilter" ) )
      // true if this layer implements spatial filtering efficiently.
      // Layers that effectively read all features, and test them with the
      // OGRFeature intersection methods should return false.
      // This can be used as a clue by the application whether it should build
      // and maintain it's own spatial index for features in this layer.
    {
      // TODO: Perhaps use as a clue by QGIS whether it should build and maintain it's own spatial index for features in this layer.
    }

    if ( OGR_L_TestCapability( ogrLayer, "FastFeatureCount" ) )
      // true if this layer can return a feature count
      // (via OGRLayer::GetFeatureCount()) efficiently ... ie. without counting
      // the features. In some cases this will return true until a spatial
      // filter is installed after which it will return false.
    {
      // TODO: Perhaps use as a clue by QGIS whether it should spawn a thread to count features.
    }

    if ( OGR_L_TestCapability( ogrLayer, "FastGetExtent" ) )
      // true if this layer can return its data extent
      // (via OGRLayer::GetExtent()) efficiently ... ie. without scanning
      // all the features. In some cases this will return true until a
      // spatial filter is installed after which it will return false.
    {
      // TODO: Perhaps use as a clue by QGIS whether it should spawn a thread to calculate extent.
    }

    if ( OGR_L_TestCapability( ogrLayer, "FastSetNextByIndex" ) )
      // true if this layer can perform the SetNextByIndex() call efficiently.
    {
      // No use required for this QGIS release.
    }
#endif

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "CreateField" ) )
    {
      ability |= AddAttributes;
    }

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "DeleteField" ) )
    {
      ability |= DeleteAttributes;
    }

    if ( mWriteAccessPossible && OGR_L_TestCapability( ogrLayer, "AlterFieldDefn" ) )
    {
      ability |= RenameAttributes;
    }

#if defined(OLCStringsAsUTF8)
    if ( !OGR_L_TestCapability( ogrLayer, OLCStringsAsUTF8 ) )
    {
      ability |= SelectEncoding;
    }
#else
    ability |= SelectEncoding;
#endif

    // OGR doesn't handle shapefiles without attributes, ie. missing DBFs well, fixes #803
    if ( ogrDriverName == QLatin1String( "ESRI Shapefile" ) )
    {
      ability |= CreateSpatialIndex;
      ability |= CreateAttributeIndex;

      if ( mAttributeFields.size() == 0 )
      {
        QgsMessageLog::logMessage( tr( "Shapefiles without attribute are considered read-only." ), tr( "OGR" ) );
        ability &= ~( AddFeatures | DeleteFeatures | ChangeAttributeValues | AddAttributes | DeleteAttributes );
      }

      if ( ( ability & ChangeAttributeValues ) == 0 )
      {
        // on readonly shapes OGR reports that it can delete features although it can't RandomWrite
        ability &= ~( AddAttributes | DeleteFeatures );
      }
    }

    /* Curve geometries are available in some drivers starting with GDAL 2.0 */
    if ( OGR_L_TestCapability( ogrLayer, "CurveGeometries" ) )
    {
      ability |= CircularGeometries;
    }
  }

  mCapabilities = ability;
}


QString QgsOgrProvider::name() const
{
  return TEXT_PROVIDER_KEY;
} // QgsOgrProvider::name()


QString  QgsOgrProvider::description() const
{
  return TEXT_PROVIDER_DESCRIPTION;
} //  QgsOgrProvider::description()


/**

  Convenience function for readily creating file filters.

  Given a long name for a file filter and a regular expression, return
  a file filter string suitable for use in a QFileDialog::OpenFiles()
  call.  The regular express, glob, will have both all lower and upper
  case versions added.

  @note

  Copied from qgisapp.cpp.

  @todo XXX This should probably be generalized and moved to a standard
            utility type thingy.

*/
static QString createFileFilter_( QString const &longName, QString const &glob )
{
  // return longName + " [OGR] (" + glob.toLower() + ' ' + glob.toUpper() + ");;";
  return longName + " (" + glob.toLower() + ' ' + glob.toUpper() + ");;";
} // createFileFilter_


QString createFilters( const QString &type )
{
  //! Database drivers available
  static QString sDatabaseDrivers;
  //! Protocol drivers available
  static QString sProtocolDrivers;
  //! File filters
  static QString sFileFilters;
  //! Directory drivers
  static QString sDirectoryDrivers;
  //! Extensions
  static QStringList sExtensions;
  //! Wildcards
  static QStringList sWildcards;

  // if we've already built the supported vector string, just return what
  // we've already built

  if ( sFileFilters.isEmpty() || sFileFilters.isNull() )
  {
    // register ogr plugins
    QgsApplication::registerOgrDrivers();

    // first get the GDAL driver manager
    OGRSFDriverH driver;          // current driver
    QString driverName;           // current driver name

    // Grind through all the drivers and their respective metadata.
    // We'll add a file filter for those drivers that have a file
    // extension defined for them; the others, welll, even though
    // theoreticaly we can open those files because there exists a
    // driver for them, the user will have to use the "All Files" to
    // open datasets with no explicitly defined file name extension.
    QgsDebugMsg( QString( "Driver count: %1" ).arg( OGRGetDriverCount() ) );

    for ( int i = 0; i < OGRGetDriverCount(); ++i )
    {
      driver = OGRGetDriver( i );

      Q_CHECK_PTR( driver ); // NOLINT

      if ( !driver )
      {
        QgsMessageLog::logMessage( QObject::tr( "Unable to get driver %1" ).arg( i ), QObject::tr( "OGR" ) );
        continue;
      }

      driverName = OGR_Dr_GetName( driver );

      if ( driverName.startsWith( QLatin1String( "AVCBin" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "Arc/Info Binary Coverage" ) + ",AVCBin;";
      }
      else if ( driverName.startsWith( QLatin1String( "AVCE00" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Arc/Info ASCII Coverage" ), QStringLiteral( "*.e00" ) );
        sExtensions << QStringLiteral( "e00" );
      }
      else if ( driverName.startsWith( QLatin1String( "BNA" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Atlas BNA" ), QStringLiteral( "*.bna" ) );
        sExtensions << QStringLiteral( "bna" );
      }
      else if ( driverName.startsWith( QLatin1String( "CSV" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Comma Separated Value" ), QStringLiteral( "*.csv" ) );
        sExtensions << QStringLiteral( "csv" );
      }
      else if ( driverName.startsWith( QObject::tr( "DODS" ) ) )
      {
        sProtocolDrivers += QLatin1String( "DODS/OPeNDAP,DODS;" );
      }
      else if ( driverName.startsWith( QObject::tr( "CouchDB" ) ) )
      {
        sProtocolDrivers += QLatin1String( "CouchDB;" );
      }
      else if ( driverName.startsWith( QLatin1String( "FileGDB" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "ESRI FileGDB" ) + ",FileGDB;";
      }
      else if ( driverName.startsWith( QLatin1String( "PGeo" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "ESRI Personal GeoDatabase" ) + ",PGeo;";
#ifdef Q_OS_WIN
        sFileFilters += createFileFilter_( QObject::tr( "ESRI Personal GeoDatabase" ), "*.mdb" );
        sExtensions << "mdb";
#endif
      }
      else if ( driverName.startsWith( QLatin1String( "SDE" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "ESRI ArcSDE" ) + ",SDE;";
      }
      else if ( driverName.startsWith( QLatin1String( "ESRI" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "ESRI Shapefiles" ), QStringLiteral( "*.shp" ) );
        sExtensions << QStringLiteral( "shp" ) << QStringLiteral( "dbf" );
      }
      else if ( driverName.startsWith( QObject::tr( "FMEObjects Gateway" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "FMEObjects Gateway" ), QStringLiteral( "*.fdd" ) );
        sExtensions << QStringLiteral( "fdd" );
      }
      else if ( driverName.startsWith( QLatin1String( "GeoJSON" ) ) )
      {
        sProtocolDrivers += QLatin1String( "GeoJSON,GeoJSON;" );
        sFileFilters += createFileFilter_( QObject::tr( "GeoJSON" ), QStringLiteral( "*.geojson" ) );
        sExtensions << QStringLiteral( "geojson" );
      }
      else if ( driverName.startsWith( QLatin1String( "GeoRSS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "GeoRSS" ), QStringLiteral( "*.xml" ) );
        sExtensions << QStringLiteral( "xml" );
      }
      else if ( driverName.startsWith( QLatin1String( "GML" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Geography Markup Language [GML]" ), QStringLiteral( "*.gml" ) );
        sExtensions << QStringLiteral( "gml" );
      }
      else if ( driverName.startsWith( QLatin1String( "GMT" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Generic Mapping Tools [GMT]" ), QStringLiteral( "*.gmt" ) );
        sExtensions << QStringLiteral( "gmt" );
      }
      else if ( driverName.startsWith( QLatin1String( "GPX" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "GPS eXchange Format [GPX]" ), QStringLiteral( "*.gpx" ) );
        sExtensions << QStringLiteral( "gpx" );
      }
      else if ( driverName.startsWith( QLatin1String( "GPKG" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "GeoPackage" ), QStringLiteral( "*.gpkg" ) );
        sExtensions << QStringLiteral( "gpkg" );
      }
      else if ( driverName.startsWith( QLatin1String( "GRASS" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "Grass Vector" ) + ",GRASS;";
      }
      else if ( driverName.startsWith( QLatin1String( "IDB" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "Informix DataBlade" ) + ",IDB;";
      }
      else if ( driverName.startsWith( QLatin1String( "Interlis 1" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "INTERLIS 1" ), QStringLiteral( "*.itf *.xml *.ili" ) );
        sExtensions << QStringLiteral( "itf" ) << QStringLiteral( "xml" ) << QStringLiteral( "ili" );
      }
      else if ( driverName.startsWith( QLatin1String( "Interlis 2" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "INTERLIS 2" ), QStringLiteral( "*.xtf *.xml *.ili" ) );
        sExtensions << QStringLiteral( "xtf" ) << QStringLiteral( "xml" ) << QStringLiteral( "ili" );
      }
      else if ( driverName.startsWith( QLatin1String( "Ingres" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "Ingres" ) + ",Ingres;";
      }
      else if ( driverName.startsWith( QLatin1String( "KML" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Keyhole Markup Language [KML]" ), QStringLiteral( "*.kml *.kmz" ) );
        sExtensions << QStringLiteral( "kml" ) << QStringLiteral( "kmz" );
      }
      else if ( driverName.startsWith( QLatin1String( "MapInfo File" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Mapinfo File" ), QStringLiteral( "*.mif *.tab" ) );
        sExtensions << QStringLiteral( "mif" ) << QStringLiteral( "tab" );
      }
      else if ( driverName.startsWith( QLatin1String( "DGN" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Microstation DGN" ), QStringLiteral( "*.dgn" ) );
        sExtensions << QStringLiteral( "dgn" );
      }
      else if ( driverName.startsWith( QLatin1String( "MySQL" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "MySQL" ) + ",MySQL;";
      }
      else if ( driverName.startsWith( QLatin1String( "MSSQL" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "MSSQL" ) + ",MSSQL;";
      }
      else if ( driverName.startsWith( QLatin1String( "OCI" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "Oracle Spatial" ) + ",OCI;";
      }
      else if ( driverName.startsWith( QLatin1String( "ODBC" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "ODBC" ) + ",ODBC;";
      }
      else if ( driverName.startsWith( QLatin1String( "OGDI" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "OGDI Vectors" ) + ",OGDI;";
      }
      else if ( driverName.startsWith( QLatin1String( "OpenFileGDB" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "OpenFileGDB" ) + ",OpenFileGDB;";
      }
      else if ( driverName.startsWith( QLatin1String( "PostgreSQL" ) ) )
      {
        sDatabaseDrivers += QObject::tr( "PostgreSQL" ) + ",PostgreSQL;";
      }
      else if ( driverName.startsWith( QLatin1String( "S57" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "S-57 Base file" ),
                                           QStringLiteral( "*.000" ) );
        sExtensions << QStringLiteral( "000" );
      }
      else if ( driverName.startsWith( QLatin1String( "SDTS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Spatial Data Transfer Standard [SDTS]" ),
                                           QStringLiteral( "*catd.ddf" ) );
        sWildcards << QStringLiteral( "*catd.ddf" );
      }
      else if ( driverName.startsWith( QLatin1String( "SOSI" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Systematic Organization of Spatial Information [SOSI]" ), QStringLiteral( "*.sos" ) );
        sExtensions << QStringLiteral( "sos" );
      }
      else if ( driverName.startsWith( QLatin1String( "SQLite" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "SQLite/SpatiaLite" ), QStringLiteral( "*.sqlite *.db *.sqlite3 *.db3 *.s3db *.sl3" ) );
        sExtensions << QStringLiteral( "sqlite" ) << QStringLiteral( "db" ) << QStringLiteral( "sqlite3" ) << QStringLiteral( "db3" ) << QStringLiteral( "s3db" ) << QStringLiteral( "sl3" );
      }
      else if ( driverName.startsWith( QLatin1String( "SXF" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Storage and eXchange Format" ), QStringLiteral( "*.sxf" ) );
        sExtensions << QStringLiteral( "sxf" );
      }
      else if ( driverName.startsWith( QLatin1String( "UK .NTF" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "UK. NTF2" ) + ",UK. NTF;";
      }
      else if ( driverName.startsWith( QLatin1String( "TIGER" ) ) )
      {
        sDirectoryDrivers += QObject::tr( "U.S. Census TIGER/Line" ) + ",TIGER;";
      }
      else if ( driverName.startsWith( QLatin1String( "VRT" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "VRT - Virtual Datasource" ),
                                           QStringLiteral( "*.vrt *.ovf" ) );
        sExtensions << QStringLiteral( "vrt" ) << QStringLiteral( "ovf" );
      }
      else if ( driverName.startsWith( QLatin1String( "XPlane" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "X-Plane/Flightgear" ),
                                           QStringLiteral( "apt.dat nav.dat fix.dat awy.dat" ) );
        sWildcards << QStringLiteral( "apt.dat" ) << QStringLiteral( "nav.dat" ) << QStringLiteral( "fix.dat" ) << QStringLiteral( "awy.dat" );
      }
      else if ( driverName.startsWith( QLatin1String( "Geoconcept" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Geoconcept" ), QStringLiteral( "*.gxt *.txt" ) );
        sExtensions << QStringLiteral( "gxt" ) << QStringLiteral( "txt" );
      }
      else if ( driverName.startsWith( QLatin1String( "DXF" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "AutoCAD DXF" ), QStringLiteral( "*.dxf" ) );
        sExtensions << QStringLiteral( "dxf" );
      }
      else if ( driverName.startsWith( QLatin1String( "ODS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Open Document Spreadsheet" ), QStringLiteral( "*.ods" ) );
        sExtensions << QStringLiteral( "ods" );
      }
      else if ( driverName.startsWith( QLatin1String( "XLSX" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "MS Office Open XML spreadsheet" ), QStringLiteral( "*.xlsx" ) );
        sExtensions << QStringLiteral( "xlsx" );
      }
      else if ( driverName.endsWith( QLatin1String( "XLS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "MS Excel format" ), QStringLiteral( "*.xls" ) );
        sExtensions << QStringLiteral( "xls" );
      }
      else if ( driverName.startsWith( QLatin1String( "EDIGEO" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "EDIGEO" ), QStringLiteral( "*.thf" ) );
        sExtensions << QStringLiteral( "thf" );
      }
      else if ( driverName.startsWith( QLatin1String( "NAS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "NAS - ALKIS" ), QStringLiteral( "*.xml" ) );
        sExtensions << QStringLiteral( "xml" );
      }
      else if ( driverName.startsWith( QLatin1String( "WAsP" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "WAsP" ), QStringLiteral( "*.map" ) );
        sExtensions << QStringLiteral( "map" );
      }
      else if ( driverName.startsWith( QLatin1String( "PCIDSK" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "PCI Geomatics Database File" ), QStringLiteral( "*.pix" ) );
        sExtensions << QStringLiteral( "pix" );
      }
      else if ( driverName.startsWith( QLatin1String( "GPSTrackMaker" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "GPSTrackMaker" ), QStringLiteral( "*.gtm *.gtz" ) );
        sExtensions << QStringLiteral( "gtm" ) << QStringLiteral( "gtz" );
      }
      else if ( driverName.startsWith( QLatin1String( "VFK" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Czech Cadastral Exchange Data Format" ), QStringLiteral( "*.vfk" ) );
        sExtensions << QStringLiteral( "vfk" );
      }
      else if ( driverName.startsWith( QLatin1String( "OSM" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "OpenStreetMap" ), QStringLiteral( "*.osm *.pbf" ) );
        sExtensions << QStringLiteral( "osm" ) << QStringLiteral( "pbf" );
      }
      else if ( driverName.startsWith( QLatin1String( "SUA" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Special Use Airspace Format" ), QStringLiteral( "*.sua" ) );
        sExtensions << QStringLiteral( "sua" );
      }
      else if ( driverName.startsWith( QLatin1String( "OpenAir" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "OpenAir Special Use Airspace Format" ), QStringLiteral( "*.txt" ) );
        sExtensions << QStringLiteral( "txt" );
      }
      else if ( driverName.startsWith( QLatin1String( "PDS" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Planetary Data Systems TABLE" ), QStringLiteral( "*.xml" ) );
        sExtensions << QStringLiteral( "xml" );
      }
      else if ( driverName.startsWith( QLatin1String( "HTF" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Hydrographic Transfer Format" ), QStringLiteral( "*.htf" ) );
        sExtensions << QStringLiteral( "htf" );
      }
      else if ( driverName.startsWith( QLatin1String( "SVG" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Scalable Vector Graphics" ), QStringLiteral( "*.svg" ) );
        sExtensions << QStringLiteral( "svg" );
      }
      else if ( driverName.startsWith( QLatin1String( "ARCGEN" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Arc/Info Generate" ), QStringLiteral( "*.gen" ) );
        sExtensions << QStringLiteral( "gen" );
      }
      else if ( driverName.startsWith( QLatin1String( "PDF" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "Geospatial PDF" ), QStringLiteral( "*.pdf" ) );
        sExtensions << QStringLiteral( "pdf" );
      }
      else if ( driverName.startsWith( QLatin1String( "SEGY" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "SEG-Y" ), QStringLiteral( "*.sgy *.segy" ) );
        sExtensions << QStringLiteral( "sgy" ) << QStringLiteral( "segy" );
      }
      else if ( driverName.startsWith( QLatin1String( "SEGUKOOA" ) ) )
      {
        sFileFilters += createFileFilter_( QObject::tr( "SEG-P1" ), QStringLiteral( "*.seg *.seg1 *.sp1" ) );
        sFileFilters += createFileFilter_( QObject::tr( "UKOOA P1/90" ), QStringLiteral( "*.uko *.ukooa" ) );
        sExtensions << QStringLiteral( "seg" ) << QStringLiteral( "seg1" ) << QStringLiteral( "sp1" ) << QStringLiteral( "uko" ) << QStringLiteral( "ukooa" );
      }
      else
      {
        // NOP, we don't know anything about the current driver
        // with regards to a proper file filter string
        QgsDebugMsg( QString( "Unknown driver %1 for file filters." ).arg( driverName ) );
      }

    }                          // each loaded OGR driver

    // sort file filters alphabetically
    QgsDebugMsg( "myFileFilters: " + sFileFilters );
    QStringList filters = sFileFilters.split( QStringLiteral( ";;" ), QString::SkipEmptyParts );
    filters.sort();
    sFileFilters = filters.join( QStringLiteral( ";;" ) ) + ";;";
    QgsDebugMsg( "myFileFilters: " + sFileFilters );

    // VSIFileHandler (.zip and .gz files) - second
    //   see http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
    // Requires GDAL>=1.6.0 with libz support, let's assume we have it.
    // This does not work for some file types, see VSIFileHandler doc.
    QgsSettings settings;
    if ( settings.value( QStringLiteral( "qgis/scanZipInBrowser2" ), "basic" ).toString() != QLatin1String( "no" ) )
    {
      sFileFilters.prepend( createFileFilter_( QObject::tr( "GDAL/OGR VSIFileHandler" ), QStringLiteral( "*.zip *.gz *.tar *.tar.gz *.tgz" ) ) );
      sExtensions << QStringLiteral( "zip" ) << QStringLiteral( "gz" ) << QStringLiteral( "tar" ) << QStringLiteral( "tar.gz" ) << QStringLiteral( "tgz" );

    }

    // can't forget the default case - first
    sFileFilters.prepend( QObject::tr( "All files" ) + " (*);;" );

    // cleanup
    if ( sFileFilters.endsWith( QLatin1String( ";;" ) ) ) sFileFilters.chop( 2 );

    QgsDebugMsg( "myFileFilters: " + sFileFilters );
  }

  if ( type == QLatin1String( "file" ) )
  {
    return sFileFilters;
  }
  if ( type == QLatin1String( "database" ) )
  {
    return sDatabaseDrivers;
  }
  if ( type == QLatin1String( "protocol" ) )
  {
    return sProtocolDrivers;
  }
  if ( type == QLatin1String( "directory" ) )
  {
    return sDirectoryDrivers;
  }
  if ( type == QLatin1String( "extensions" ) )
  {
    return sExtensions.join( QStringLiteral( "|" ) );
  }
  if ( type == QLatin1String( "wildcards" ) )
  {
    return sWildcards.join( QStringLiteral( "|" ) );
  }
  else
  {
    return QLatin1String( "" );
  }
}


QGISEXTERN QString fileVectorFilters()
{
  return createFilters( QStringLiteral( "file" ) );
}

QString QgsOgrProvider::fileVectorFilters() const
{
  return createFilters( QStringLiteral( "file" ) );
}

QGISEXTERN QString databaseDrivers()
{
  return createFilters( QStringLiteral( "database" ) );
}

QString QgsOgrProvider::databaseDrivers() const
{
  return createFilters( QStringLiteral( "database" ) );
}

QGISEXTERN QString protocolDrivers()
{
  return createFilters( QStringLiteral( "protocol" ) );
}

QString QgsOgrProvider::protocolDrivers() const
{
  return createFilters( QStringLiteral( "protocol" ) );
}

QGISEXTERN QString directoryDrivers()
{
  return  createFilters( QStringLiteral( "directory" ) );
}

QString QgsOgrProvider::directoryDrivers() const
{
  return  createFilters( QStringLiteral( "directory" ) );
}

QGISEXTERN QStringList fileExtensions()
{
  return  createFilters( QStringLiteral( "extensions" ) ).split( '|' );
}

QGISEXTERN QStringList wildcards()
{
  return  createFilters( QStringLiteral( "wildcards" ) ).split( '|' );
}


/**
 * Class factory to return a pointer to a newly created
 * QgsOgrProvider object
 */
QGISEXTERN QgsOgrProvider *classFactory( const QString *uri )
{
  return new QgsOgrProvider( *uri );
}



/** Required key function (used to map the plugin to a data store type)
*/
QGISEXTERN QString providerKey()
{
  return TEXT_PROVIDER_KEY;
}


/**
 * Required description function
 */
QGISEXTERN QString description()
{
  return TEXT_PROVIDER_DESCRIPTION;
}

/**
 * Required isProvider function. Used to determine if this shared library
 * is a data provider plugin
 */

QGISEXTERN bool isProvider()
{
  return true;
}

/** Creates an empty data source
@param uri location to store the file(s)
@param format data format (e.g. "ESRI Shapefile"
@param vectortype point/line/polygon or multitypes
@param attributes a list of name/type pairs for the initial attributes
@return true in case of success*/
QGISEXTERN bool createEmptyDataSource( const QString &uri,
                                       const QString &format,
                                       const QString &encoding,
                                       QgsWkbTypes::Type vectortype,
                                       const QList< QPair<QString, QString> > &attributes,
                                       const QgsCoordinateReferenceSystem &srs = QgsCoordinateReferenceSystem() )
{
  QgsDebugMsg( QString( "Creating empty vector layer with format: %1" ).arg( format ) );

  OGRSFDriverH driver;
  QgsApplication::registerOgrDrivers();
  driver = OGRGetDriverByName( format.toLatin1() );
  if ( !driver )
  {
    return false;
  }

  QString driverName = OGR_Dr_GetName( driver );

  if ( driverName == QLatin1String( "ESRI Shapefile" ) )
  {
    if ( !uri.endsWith( QLatin1String( ".shp" ), Qt::CaseInsensitive ) )
    {
      QgsDebugMsg( QString( "uri %1 doesn't end with .shp" ).arg( uri ) );
      return false;
    }

    // check for duplicate fieldnames
    QSet<QString> fieldNames;
    QList<QPair<QString, QString> >::const_iterator fldIt;
    for ( fldIt = attributes.begin(); fldIt != attributes.end(); ++fldIt )
    {
      QString name = fldIt->first.left( 10 );
      if ( fieldNames.contains( name ) )
      {
        QgsMessageLog::logMessage( QObject::tr( "Duplicate field (10 significant characters): %1" ).arg( name ), QObject::tr( "OGR" ) );
        return false;
      }
      fieldNames << name;
    }

    QgsVectorFileWriter::deleteShapeFile( uri );
  }
  else
  {
    QFile::remove( uri );
  }

  OGRDataSourceH dataSource;
  dataSource = OGR_Dr_CreateDataSource( driver, uri.toUtf8().constData(), nullptr );
  if ( !dataSource )
  {
    QgsMessageLog::logMessage( QObject::tr( "Creating the data source %1 failed: %2" ).arg( uri, QString::fromUtf8( CPLGetLastErrorMsg() ) ), QObject::tr( "OGR" ) );
    return false;
  }

  //consider spatial reference system
  OGRSpatialReferenceH reference = nullptr;

  QgsCoordinateReferenceSystem mySpatialRefSys;
  if ( srs.isValid() )
  {
    mySpatialRefSys = srs;
  }
  else
  {
    mySpatialRefSys.validate();
  }

  QString myWkt = mySpatialRefSys.toWkt();

  if ( !myWkt.isNull()  &&  myWkt.length() != 0 )
  {
    reference = OSRNewSpatialReference( myWkt.toLocal8Bit().data() );
  }

  // Map the qgis geometry type to the OGR geometry type
  OGRwkbGeometryType OGRvectortype = wkbUnknown;
  switch ( vectortype )
  {
    case QgsWkbTypes::Point:
      OGRvectortype = wkbPoint;
      break;
    case QgsWkbTypes::Point25D:
      OGRvectortype = wkbPoint25D;
      break;
    case QgsWkbTypes::LineString:
      OGRvectortype = wkbLineString;
      break;
    case QgsWkbTypes::LineString25D:
      OGRvectortype = wkbLineString25D;
      break;
    case QgsWkbTypes::Polygon:
      OGRvectortype = wkbPolygon;
      break;
    case QgsWkbTypes::Polygon25D:
      OGRvectortype = wkbPolygon25D;
      break;
    case QgsWkbTypes::MultiPoint:
      OGRvectortype = wkbMultiPoint;
      break;
    case QgsWkbTypes::MultiPoint25D:
      OGRvectortype = wkbMultiPoint25D;
      break;
    case QgsWkbTypes::MultiLineString:
      OGRvectortype = wkbMultiLineString;
      break;
    case QgsWkbTypes::MultiLineString25D:
      OGRvectortype = wkbMultiLineString25D;
      break;
    case QgsWkbTypes::MultiPolygon:
      OGRvectortype = wkbMultiPolygon;
      break;
    case QgsWkbTypes::MultiPolygon25D:
      OGRvectortype = wkbMultiPolygon25D;
      break;
    default:
    {
      QgsMessageLog::logMessage( QObject::tr( "Unknown vector type of %1" ).arg( ( int )( vectortype ) ), QObject::tr( "OGR" ) );
      return false;
    }
  }

  char **papszOptions = nullptr;
  if ( driverName == QLatin1String( "ESRI Shapefile" ) )
  {
    papszOptions = CSLSetNameValue( papszOptions, "ENCODING", QgsVectorFileWriter::convertCodecNameForEncodingOption( encoding ).toLocal8Bit().data() );
    // OGR Shapefile fails to create fields if given encoding is not supported by its side
    // so disable encoding conversion of OGR Shapefile layer
    CPLSetConfigOption( "SHAPE_ENCODING", "" );
  }

  OGRLayerH layer;
  layer = OGR_DS_CreateLayer( dataSource, QFileInfo( uri ).completeBaseName().toUtf8().constData(), reference, OGRvectortype, papszOptions );
  CSLDestroy( papszOptions );

  QgsSettings settings;
  if ( !settings.value( QStringLiteral( "qgis/ignoreShapeEncoding" ), true ).toBool() )
  {
    CPLSetConfigOption( "SHAPE_ENCODING", nullptr );
  }

  if ( !layer )
  {
    QgsMessageLog::logMessage( QObject::tr( "Creation of OGR data source %1 failed: %2" ).arg( uri, QString::fromUtf8( CPLGetLastErrorMsg() ) ), QObject::tr( "OGR" ) );
    return false;
  }

  //create the attribute fields

  QTextCodec *codec = QTextCodec::codecForName( encoding.toLocal8Bit().data() );
  if ( !codec )
  {
    // fall back to "System" codec
    codec = QTextCodec::codecForLocale();
    Q_ASSERT( codec );
  }

  for ( QList<QPair<QString, QString> >::const_iterator it = attributes.begin(); it != attributes.end(); ++it )
  {
    QStringList fields = it->second.split( ';' );

    if ( fields.isEmpty() )
      continue;

    int width = fields.size() > 1 ? fields[1].toInt() : -1;
    int precision = fields.size() > 2 ? fields[2].toInt() : -1;
    if ( precision > 0 )
      width += 1;

    OGRFieldDefnH field;
    if ( fields[0] == QLatin1String( "Real" ) )
    {
      if ( width < 0 )
        width = 32;
      if ( precision < 0 )
        precision = 3;

      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTReal );
      OGR_Fld_SetWidth( field, width );
      OGR_Fld_SetPrecision( field, precision );
    }
    else if ( fields[0] == QLatin1String( "Integer" ) )
    {
      if ( width < 0 || width > 10 )
        width = 10;

      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTInteger );
      // limit to 10.  otherwise OGR sets it to 11 and recognizes as OFTDouble later
      OGR_Fld_SetWidth( field, width );
    }
    else if ( fields[0] == QLatin1String( "String" ) )
    {
      if ( width < 0 || width > 255 )
        width = 255;

      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTString );
      OGR_Fld_SetWidth( field, width );
    }
    else if ( fields[0] == QLatin1String( "Date" ) )
    {
      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTDate );
    }
    else if ( fields[0] == QLatin1String( "Time" ) )
    {
      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTTime );
    }
    else if ( fields[0] == QLatin1String( "DateTime" ) )
    {
      field = OGR_Fld_Create( codec->fromUnicode( it->first ).constData(), OFTDateTime );
    }
    else
    {
      QgsMessageLog::logMessage( QObject::tr( "field %1 with unsupported type %2 skipped" ).arg( it->first, fields[0] ), QObject::tr( "OGR" ) );
      continue;
    }

    if ( OGR_L_CreateField( layer, field, true ) != OGRERR_NONE )
    {
      QgsMessageLog::logMessage( QObject::tr( "creation of field %1 failed" ).arg( it->first ), QObject::tr( "OGR" ) );
    }
  }

  OGR_DS_Destroy( dataSource );

  if ( driverName == QLatin1String( "ESRI Shapefile" ) )
  {
    QString layerName = uri.left( uri.indexOf( QLatin1String( ".shp" ), Qt::CaseInsensitive ) );
    QFile prjFile( layerName + ".qpj" );
    if ( prjFile.open( QIODevice::WriteOnly | QIODevice::Truncate ) )
    {
      QTextStream prjStream( &prjFile );
      prjStream << myWkt.toLocal8Bit().data() << endl;
      prjFile.close();
    }
    else
    {
      QgsMessageLog::logMessage( QObject::tr( "Couldn't create file %1.qpj" ).arg( layerName ), QObject::tr( "OGR" ) );
    }
  }

  QgsDebugMsg( QString( "GDAL Version number %1" ).arg( GDAL_VERSION_NUM ) );
  if ( reference )
  {
    OSRRelease( reference );
  }
  return true;
}


QGISEXTERN QList< QgsDataItemProvider * > *dataItemProviders()
{
  QList< QgsDataItemProvider * > *providers = new QList< QgsDataItemProvider * >();
  *providers << new QgsGeoPackageDataItemProvider;
  return providers;
}

QgsCoordinateReferenceSystem QgsOgrProvider::crs() const
{
  QgsDebugMsg( "Entering." );

  QgsCoordinateReferenceSystem srs;
  if ( !mValid )
    return srs;

  if ( ogrDriver )
  {
    QString driverName = OGR_Dr_GetName( ogrDriver );

    if ( driverName == QLatin1String( "ESRI Shapefile" ) )
    {
      QString layerName = mFilePath.left( mFilePath.indexOf( QLatin1String( ".shp" ), Qt::CaseInsensitive ) );
      QFile prjFile( layerName + ".qpj" );
      if ( prjFile.open( QIODevice::ReadOnly ) )
      {
        QTextStream prjStream( &prjFile );
        QString myWktString = prjStream.readLine();
        prjFile.close();

        srs = QgsCoordinateReferenceSystem::fromWkt( myWktString.toUtf8().constData() );
        if ( srs.isValid() )
          return srs;
      }
    }
  }

  // add towgs84 parameter
  QgsCoordinateReferenceSystem::setupESRIWktFix();

  OGRSpatialReferenceH mySpatialRefSys = OGR_L_GetSpatialRef( ogrLayer );
  if ( mySpatialRefSys )
  {
    // get the proj4 text
    char *pszProj4 = nullptr;
    OSRExportToProj4( mySpatialRefSys, &pszProj4 );
    QgsDebugMsg( pszProj4 );
    CPLFree( pszProj4 );

    char *pszWkt = nullptr;
    OSRExportToWkt( mySpatialRefSys, &pszWkt );

    srs = QgsCoordinateReferenceSystem::fromWkt( pszWkt );
    CPLFree( pszWkt );
  }
  else
  {
    QgsDebugMsg( "no spatial reference found" );
  }

  return srs;
}

QSet<QVariant> QgsOgrProvider::uniqueValues( int index, int limit ) const
{
  QSet<QVariant> uniqueValues;

  if ( !mValid || index < 0 || index >= mAttributeFields.count() )
    return uniqueValues;

  QgsField fld = mAttributeFields.at( index );
  if ( fld.name().isNull() )
  {
    return uniqueValues; //not a provider field
  }

  QByteArray sql = "SELECT DISTINCT " + quotedIdentifier( textEncoding()->fromUnicode( fld.name() ) );
  sql += " FROM " + quotedIdentifier( OGR_FD_GetName( OGR_L_GetLayerDefn( ogrLayer ) ) );

  if ( !mSubsetString.isEmpty() )
  {
    sql += " WHERE " + textEncoding()->fromUnicode( mSubsetString );
  }

  sql += " ORDER BY " + textEncoding()->fromUnicode( fld.name() ) + " ASC";  // quoting of fieldname produces a syntax error

  QgsDebugMsg( QString( "SQL: %1" ).arg( textEncoding()->toUnicode( sql ) ) );
  OGRLayerH l = OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
  if ( !l )
  {
    QgsDebugMsg( "Failed to execute SQL" );
    return QgsVectorDataProvider::uniqueValues( index, limit );
  }

  OGRFeatureH f;
  while ( ( f = OGR_L_GetNextFeature( l ) ) )
  {
    uniqueValues << ( OGR_F_IsFieldSetAndNotNull( f, 0 ) ? convertValue( fld.type(), textEncoding()->toUnicode( OGR_F_GetFieldAsString( f, 0 ) ) ) : QVariant( fld.type() ) );
    OGR_F_Destroy( f );

    if ( limit >= 0 && uniqueValues.size() >= limit )
      break;
  }

  OGR_DS_ReleaseResultSet( ogrDataSource, l );
  return uniqueValues;
}

QStringList QgsOgrProvider::uniqueStringsMatching( int index, const QString &substring, int limit, QgsFeedback *feedback ) const
{
  QStringList results;

  if ( !mValid || index < 0 || index >= mAttributeFields.count() )
    return results;

  QgsField fld = mAttributeFields.at( index );
  if ( fld.name().isNull() )
  {
    return results; //not a provider field
  }

  QByteArray sql = "SELECT DISTINCT " + quotedIdentifier( textEncoding()->fromUnicode( fld.name() ) );
  sql += " FROM " + quotedIdentifier( OGR_FD_GetName( OGR_L_GetLayerDefn( ogrLayer ) ) );

  sql += " WHERE " + quotedIdentifier( textEncoding()->fromUnicode( fld.name() ) ) + " LIKE '%" +  textEncoding()->fromUnicode( substring ) + "%'";

  if ( !mSubsetString.isEmpty() )
  {
    sql += " AND (" + textEncoding()->fromUnicode( mSubsetString ) + ')';
  }

  sql += " ORDER BY " + textEncoding()->fromUnicode( fld.name() ) + " ASC";  // quoting of fieldname produces a syntax error

  QgsDebugMsg( QString( "SQL: %1" ).arg( textEncoding()->toUnicode( sql ) ) );
  OGRLayerH l = OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
  if ( !l )
  {
    QgsDebugMsg( "Failed to execute SQL" );
    return QgsVectorDataProvider::uniqueStringsMatching( index, substring, limit, feedback );
  }

  OGRFeatureH f;
  while ( ( f = OGR_L_GetNextFeature( l ) ) )
  {
    if ( OGR_F_IsFieldSetAndNotNull( f, 0 ) )
      results << textEncoding()->toUnicode( OGR_F_GetFieldAsString( f, 0 ) );
    OGR_F_Destroy( f );

    if ( ( limit >= 0 && results.size() >= limit ) || ( feedback && feedback->isCanceled() ) )
      break;
  }

  OGR_DS_ReleaseResultSet( ogrDataSource, l );
  return results;
}

QVariant QgsOgrProvider::minimumValue( int index ) const
{
  if ( !mValid || index < 0 || index >= mAttributeFields.count() )
  {
    return QVariant();
  }
  QgsField fld = mAttributeFields.at( index );

  // Don't quote column name (see https://trac.osgeo.org/gdal/ticket/5799#comment:9)
  QByteArray sql = "SELECT MIN(" + textEncoding()->fromUnicode( fld.name() );
  sql += ") FROM " + quotedIdentifier( OGR_FD_GetName( OGR_L_GetLayerDefn( ogrLayer ) ) );

  if ( !mSubsetString.isEmpty() )
  {
    sql += " WHERE " + textEncoding()->fromUnicode( mSubsetString );
  }

  OGRLayerH l = OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
  if ( !l )
  {
    QgsDebugMsg( QString( "Failed to execute SQL: %1" ).arg( textEncoding()->toUnicode( sql ) ) );
    return QgsVectorDataProvider::minimumValue( index );
  }

  OGRFeatureH f = OGR_L_GetNextFeature( l );
  if ( !f )
  {
    OGR_DS_ReleaseResultSet( ogrDataSource, l );
    return QVariant();
  }

  QVariant value = OGR_F_IsFieldSetAndNotNull( f, 0 ) ? convertValue( fld.type(), textEncoding()->toUnicode( OGR_F_GetFieldAsString( f, 0 ) ) ) : QVariant( fld.type() );
  OGR_F_Destroy( f );

  OGR_DS_ReleaseResultSet( ogrDataSource, l );

  return value;
}

QVariant QgsOgrProvider::maximumValue( int index ) const
{
  if ( !mValid || index < 0 || index >= mAttributeFields.count() )
  {
    return QVariant();
  }
  QgsField fld = mAttributeFields.at( index );

  // Don't quote column name (see https://trac.osgeo.org/gdal/ticket/5799#comment:9)
  QByteArray sql = "SELECT MAX(" + textEncoding()->fromUnicode( fld.name() );
  sql += ") FROM " + quotedIdentifier( OGR_FD_GetName( OGR_L_GetLayerDefn( ogrLayer ) ) );

  if ( !mSubsetString.isEmpty() )
  {
    sql += " WHERE " + textEncoding()->fromUnicode( mSubsetString );
  }

  OGRLayerH l = OGR_DS_ExecuteSQL( ogrDataSource, sql.constData(), nullptr, nullptr );
  if ( !l )
  {
    QgsDebugMsg( QString( "Failed to execute SQL: %1" ).arg( textEncoding()->toUnicode( sql ) ) );
    return QgsVectorDataProvider::maximumValue( index );
  }

  OGRFeatureH f = OGR_L_GetNextFeature( l );
  if ( !f )
  {
    OGR_DS_ReleaseResultSet( ogrDataSource, l );
    return QVariant();
  }

  QVariant value = OGR_F_IsFieldSetAndNotNull( f, 0 ) ? convertValue( fld.type(), textEncoding()->toUnicode( OGR_F_GetFieldAsString( f, 0 ) ) ) : QVariant( fld.type() );
  OGR_F_Destroy( f );

  OGR_DS_ReleaseResultSet( ogrDataSource, l );

  return value;
}

QByteArray QgsOgrProvider::quotedIdentifier( const QByteArray &field ) const
{
  return QgsOgrProviderUtils::quotedIdentifier( field, ogrDriverName );
}

void QgsOgrProvider::forceReload()
{
  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
}

OGRDataSourceH QgsOgrProviderUtils::OGROpenWrapper( const char *pszPath, bool bUpdate, OGRSFDriverH *phDriver )
{
  CPLErrorReset();
  OGRSFDriverH hDriver = nullptr;
  OGRDataSourceH hDS = OGROpen( pszPath, bUpdate, &hDriver );
  if ( phDriver )
    *phDriver = hDriver;
  if ( !hDS )
    return nullptr;

  return hDS;
}

static bool IsLocalFile( const QString &path )
{
  QString dirName( QFileInfo( path ).absolutePath() );
  // Start with the OS specific methods since the QT >= 5.4 method just
  // return a string and not an enumerated type.
#if defined(Q_OS_WIN)
  if ( dirName.startsWith( "\\\\" ) )
    return false;
  if ( dirName.length() >= 3 && dirName[1] == ':' &&
       ( dirName[2] == '\\' || dirName[2] == '/' ) )
  {
    dirName.resize( 3 );
    return GetDriveType( dirName.toAscii().constData() ) != DRIVE_REMOTE;
  }
  return true;
#elif defined(Q_OS_LINUX)
  struct statfs sStatFS;
  if ( statfs( dirName.toAscii().constData(), &sStatFS ) == 0 )
  {
    // Codes from http://man7.org/linux/man-pages/man2/statfs.2.html
    if ( sStatFS.f_type == 0x6969 /* NFS */ ||
         sStatFS.f_type == 0x517b /* SMB */ ||
         sStatFS.f_type == 0xff534d42 /* CIFS */ )
    {
      return false;
    }
  }
  return true;
#elif QT_VERSION >= QT_VERSION_CHECK(5, 4, 0)
  QStorageInfo info( dirName );
  QString fileSystem( info.fileSystemType() );
  QgsDebugMsg( QString( "Filesystem for %1 is %2" ).arg( path ).arg( fileSystem ) );
  return path != "nfs" && path != "smbfs";
#else
  return true;
#endif
}

void QgsOgrProviderUtils::OGRDestroyWrapper( OGRDataSourceH ogrDataSource )
{
  if ( !ogrDataSource )
    return;
  OGRSFDriverH ogrDriver = OGR_DS_GetDriver( ogrDataSource );
  QString ogrDriverName = OGR_Dr_GetName( ogrDriver );
  QString datasetName( QString::fromUtf8( OGR_DS_GetName( ogrDataSource ) ) );
  if ( ogrDriverName == QLatin1String( "GPKG" ) &&
       IsLocalFile( datasetName ) &&
       !CPLGetConfigOption( "OGR_SQLITE_JOURNAL", NULL ) )
  {
    // We need to reset all iterators on layers, otherwise we will not
    // be able to change journal_mode.
    int layerCount = OGR_DS_GetLayerCount( ogrDataSource );
    for ( int i = 0; i < layerCount; i ++ )
    {
      OGR_L_ResetReading( OGR_DS_GetLayer( ogrDataSource, i ) );
    }

    CPLPushErrorHandler( CPLQuietErrorHandler );
    QgsDebugMsg( "GPKG: Trying to return to delete mode" );
    bool bSuccess = false;
    OGRLayerH hSqlLyr = OGR_DS_ExecuteSQL( ogrDataSource,
                                           "PRAGMA journal_mode = delete",
                                           NULL, NULL );
    if ( hSqlLyr != NULL )
    {
      OGRFeatureH hFeat = OGR_L_GetNextFeature( hSqlLyr );
      if ( hFeat != NULL )
      {
        const char *pszRet = OGR_F_GetFieldAsString( hFeat, 0 );
        bSuccess = EQUAL( pszRet, "delete" );
        QgsDebugMsg( QString( "Return: %1" ).arg( pszRet ) );
        OGR_F_Destroy( hFeat );
      }
    }
    else if ( CPLGetLastErrorType() != CE_None )
    {
      QgsDebugMsg( QString( "Return: %1" ).arg( CPLGetLastErrorMsg() ) );
    }
    OGR_DS_ReleaseResultSet( ogrDataSource, hSqlLyr );
    CPLPopErrorHandler();
    OGR_DS_Destroy( ogrDataSource );

    // This may have not worked if the file was opened in read-only mode,
    // so retry in update mode
    if ( !bSuccess )
    {
      QgsDebugMsg( "GPKG: Trying again" );
      CPLSetThreadLocalConfigOption( "OGR_SQLITE_JOURNAL", "DELETE" );
      ogrDataSource = OGROpen( datasetName.toUtf8().constData(), TRUE, NULL );
      CPLSetThreadLocalConfigOption( "OGR_SQLITE_JOURNAL", NULL );
      if ( ogrDataSource )
      {
#ifdef QGISDEBUG
        CPLPushErrorHandler( CPLQuietErrorHandler );
        OGRLayerH hSqlLyr = OGR_DS_ExecuteSQL( ogrDataSource,
                                               "PRAGMA journal_mode",
                                               NULL, NULL );
        CPLPopErrorHandler();
        if ( hSqlLyr != NULL )
        {
          OGRFeatureH hFeat = OGR_L_GetNextFeature( hSqlLyr );
          if ( hFeat != NULL )
          {
            const char *pszRet = OGR_F_GetFieldAsString( hFeat, 0 );
            QgsDebugMsg( QString( "Return: %1" ).arg( pszRet ) );
            OGR_F_Destroy( hFeat );
          }
          OGR_DS_ReleaseResultSet( ogrDataSource, hSqlLyr );
        }
#endif
        OGR_DS_Destroy( ogrDataSource );
      }
    }
  }
  else
  {
    OGR_DS_Destroy( ogrDataSource );
  }
}

QByteArray QgsOgrProviderUtils::quotedIdentifier( QByteArray field, const QString &ogrDriverName )
{
  if ( ogrDriverName == QLatin1String( "MySQL" ) )
  {
    field.replace( '\\', "\\\\" );
    field.replace( '`', "``" );
    return field.prepend( '`' ).append( '`' );
  }
  else
  {
    field.replace( '\\', "\\\\" );
    field.replace( '"', "\\\"" );
    field.replace( '\'', "\\'" );
    return field.prepend( '\"' ).append( '\"' );
  }
}

QString QgsOgrProviderUtils::quotedValue( const QVariant &value )
{
  if ( value.isNull() )
    return QStringLiteral( "NULL" );

  switch ( value.type() )
  {
    case QVariant::Int:
    case QVariant::LongLong:
    case QVariant::Double:
      return value.toString();

    case QVariant::Bool:
      //OGR does not support boolean literals
      return value.toBool() ? "1" : "0";

    default:
    case QVariant::String:
      QString v = value.toString();
      v.replace( '\'', QLatin1String( "''" ) );
      if ( v.contains( '\\' ) )
        return v.replace( '\\', QLatin1String( "\\\\" ) ).prepend( "E'" ).append( '\'' );
      else
        return v.prepend( '\'' ).append( '\'' );
  }
}

bool QgsOgrProvider::syncToDisc()
{
  //for shapefiles, remove spatial index files and create a new index
  QgsOgrConnPool::instance()->unref( dataSourceUri() );
  bool shapeIndex = false;
  if ( ogrDriverName == QLatin1String( "ESRI Shapefile" ) )
  {
    QString sbnIndexFile;
    QFileInfo fi( mFilePath );
    int suffixLength = fi.suffix().length();
    sbnIndexFile = mFilePath;
    sbnIndexFile.chop( suffixLength );
    sbnIndexFile.append( "sbn" );

    if ( QFile::exists( sbnIndexFile ) )
    {
      shapeIndex = true;
      close();
      QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
      QFile::remove( sbnIndexFile );
      open( OpenModeSameAsCurrent );
      if ( !mValid )
        return false;
    }
  }

  if ( OGR_L_SyncToDisk( ogrLayer ) != OGRERR_NONE )
  {
    pushError( tr( "OGR error syncing to disk: %1" ).arg( CPLGetLastErrorMsg() ) );
  }

  if ( mShapefileMayBeCorrupted )
    repack();

  mShapefileMayBeCorrupted = false;

  QgsOgrConnPool::instance()->ref( dataSourceUri() );
  if ( shapeIndex )
  {
    return createSpatialIndex();
  }

  return true;
}

void QgsOgrProvider::recalculateFeatureCount()
{
  if ( !ogrLayer )
  {
    mFeaturesCounted = QgsVectorDataProvider::Uncounted;
    return;
  }

  OGRGeometryH filter = OGR_L_GetSpatialFilter( ogrLayer );
  if ( filter )
  {
    filter = OGR_G_Clone( filter );
    OGR_L_SetSpatialFilter( ogrLayer, nullptr );
  }

  // feature count returns number of features within current spatial filter
  // so we remove it if there's any and then put it back
  if ( mOgrGeometryTypeFilter == wkbUnknown )
  {
    mFeaturesCounted = OGR_L_GetFeatureCount( ogrLayer, true );
    if ( mFeaturesCounted == -1 )
    {
      mFeaturesCounted = QgsVectorDataProvider::UnknownCount;
    }
  }
  else
  {
    mFeaturesCounted = 0;
    OGR_L_ResetReading( ogrLayer );
    setRelevantFields( ogrLayer, true, QgsAttributeList() );
    OGR_L_ResetReading( ogrLayer );
    OGRFeatureH fet;
    const OGRwkbGeometryType flattenGeomTypeFilter =
      QgsOgrProvider::ogrWkbSingleFlatten( mOgrGeometryTypeFilter );
    while ( ( fet = OGR_L_GetNextFeature( ogrLayer ) ) )
    {
      OGRGeometryH geom = OGR_F_GetGeometryRef( fet );
      if ( geom )
      {
        OGRwkbGeometryType gType = OGR_G_GetGeometryType( geom );
        gType = QgsOgrProvider::ogrWkbSingleFlatten( gType );
        if ( gType == flattenGeomTypeFilter ) mFeaturesCounted++;
      }
      OGR_F_Destroy( fet );
    }
    OGR_L_ResetReading( ogrLayer );

  }

  if ( filter )
  {
    OGR_L_SetSpatialFilter( ogrLayer, filter );
  }

  QgsOgrConnPool::instance()->invalidateConnections( dataSourceUri() );
}

bool QgsOgrProvider::doesStrictFeatureTypeCheck() const
{
  // FIXME probably other drivers too...
  return ogrDriverName != QLatin1String( "ESRI Shapefile" ) || ( mOGRGeomType == wkbPoint || mOGRGeomType == wkbPoint25D );
}

OGRwkbGeometryType QgsOgrProvider::ogrWkbSingleFlatten( OGRwkbGeometryType type )
{
  type = wkbFlatten( type );
  switch ( type )
  {
    case wkbMultiPoint:
      return wkbPoint;
    case wkbMultiLineString:
      return wkbLineString;
    case wkbMultiPolygon:
      return wkbPolygon;
    case wkbMultiCurve:
      return wkbCompoundCurve;
    case wkbMultiSurface:
      return wkbCurvePolygon;
    default:
      return type;
  }
}

OGRLayerH QgsOgrProvider::setSubsetString( OGRLayerH layer, OGRDataSourceH ds )
{
  return QgsOgrProviderUtils::setSubsetString( layer, ds, textEncoding(), mSubsetString );
}

OGRLayerH QgsOgrProviderUtils::setSubsetString( OGRLayerH layer, OGRDataSourceH ds, QTextCodec *encoding, const QString &subsetString )
{
  QByteArray layerName = OGR_FD_GetName( OGR_L_GetLayerDefn( layer ) );
  OGRSFDriverH ogrDriver = OGR_DS_GetDriver( ds );
  QString ogrDriverName = OGR_Dr_GetName( ogrDriver );

  if ( ogrDriverName == QLatin1String( "ODBC" ) ) //the odbc driver does not like schema names for subset
  {
    QString layerNameString = encoding->toUnicode( layerName );
    int dotIndex = layerNameString.indexOf( '.' );
    if ( dotIndex > 1 )
    {
      QString modifiedLayerName = layerNameString.right( layerNameString.size() - dotIndex - 1 );
      layerName = encoding->fromUnicode( modifiedLayerName );
    }
  }
  QByteArray sql;
  if ( subsetString.startsWith( QLatin1String( "SELECT " ), Qt::CaseInsensitive ) )
    sql = encoding->fromUnicode( subsetString );
  else
  {
    sql = "SELECT * FROM " + quotedIdentifier( layerName, ogrDriverName );
    sql += " WHERE " + encoding->fromUnicode( subsetString );
  }

  QgsDebugMsg( QString( "SQL: %1" ).arg( encoding->toUnicode( sql ) ) );
  return OGR_DS_ExecuteSQL( ds, sql.constData(), nullptr, nullptr );
}

void QgsOgrProvider::open( OpenMode mode )
{
  bool openReadOnly = false;

  // Try to open using VSIFileHandler
  //   see http://trac.osgeo.org/gdal/wiki/UserDocs/ReadInZip
  QString vsiPrefix = QgsZipItem::vsiPrefix( dataSourceUri() );
  if ( vsiPrefix != QLatin1String( "" ) )
  {
    // GDAL>=1.8.0 has write support for zip, but read and write operations
    // cannot be interleaved, so for now just use read-only.
    openReadOnly = true;
    if ( !mFilePath.startsWith( vsiPrefix ) )
    {
      mFilePath = vsiPrefix + mFilePath;
      setDataSourceUri( mFilePath );
    }
    QgsDebugMsg( QString( "Trying %1 syntax, mFilePath= %2" ).arg( vsiPrefix, mFilePath ) );
  }

  QgsDebugMsg( "mFilePath: " + mFilePath );
  QgsDebugMsg( "mLayerIndex: " + QString::number( mLayerIndex ) );
  QgsDebugMsg( "mLayerName: " + mLayerName );
  QgsDebugMsg( "mSubsetString: " + mSubsetString );
  CPLSetConfigOption( "OGR_ORGANIZE_POLYGONS", "ONLY_CCW" );  // "SKIP" returns MULTIPOLYGONs for multiringed POLYGONs
  CPLSetConfigOption( "GPX_ELE_AS_25D", "YES" );  // use GPX elevation as z values

  if ( mFilePath.startsWith( QLatin1String( "MySQL:" ) ) && !mLayerName.isEmpty() && !mFilePath.endsWith( ",tables=" + mLayerName ) )
  {
    mFilePath += ",tables=" + mLayerName;
  }

  if ( mode == OpenModeForceReadOnly )
    openReadOnly = true;
  else if ( mode == OpenModeSameAsCurrent && !mWriteAccess )
    openReadOnly = true;

  // first try to open in update mode (unless specified otherwise)
  if ( !openReadOnly )
  {
    if ( QFileInfo( mFilePath ).suffix().compare( QLatin1String( "gpkg" ), Qt::CaseInsensitive ) == 0 &&
         IsLocalFile( mFilePath ) &&
         !CPLGetConfigOption( "OGR_SQLITE_JOURNAL", NULL ) &&
         QgsSettings().value( QStringLiteral( "qgis/walForSqlite3" ), true ).toBool() )
    {
      // For GeoPackage, we force opening of the file in WAL (Write Ahead Log)
      // mode so as to avoid readers blocking writer(s), and vice-versa.
      // https://www.sqlite.org/wal.html
      // But only do that on a local file since WAL is advertized not to work
      // on network shares
      CPLSetThreadLocalConfigOption( "OGR_SQLITE_JOURNAL", "WAL" );
    }
    ogrDataSource = QgsOgrProviderUtils::OGROpenWrapper( mFilePath.toUtf8().constData(), true, &ogrDriver );
    CPLSetThreadLocalConfigOption( "OGR_SQLITE_JOURNAL", NULL );
  }

  mValid = false;
  if ( ogrDataSource )
  {
    mWriteAccess = true;
    mWriteAccessPossible = true;
  }
  else
  {
    mWriteAccess = false;
    if ( !openReadOnly )
    {
      QgsDebugMsg( "OGR failed to opened in update mode, trying in read-only mode" );
    }

    // try to open read-only
    ogrDataSource = QgsOgrProviderUtils::OGROpenWrapper( mFilePath.toUtf8().constData(), false, &ogrDriver );
  }

  if ( ogrDataSource )
  {
    QgsDebugMsg( "OGR opened using Driver " + QString( OGR_Dr_GetName( ogrDriver ) ) );

    ogrDriverName = OGR_Dr_GetName( ogrDriver );

    // We get the layer which was requested by the uri. The layername
    // has precedence over the layerid if both are given.
    if ( mLayerName.isNull() )
    {
      ogrOrigLayer = OGR_DS_GetLayer( ogrDataSource, mLayerIndex );
    }
    else
    {
      ogrOrigLayer = OGR_DS_GetLayerByName( ogrDataSource, mLayerName.toUtf8().constData() );
    }

    ogrLayer = ogrOrigLayer;
    if ( ogrLayer )
    {
      // check that the initial encoding setting is fit for this layer
      setEncoding( encoding() );

      mValid = setSubsetString( mSubsetString );
      if ( mValid )
      {
        if ( mode == OpenModeInitial )
        {
          computeCapabilities();
        }
        QgsDebugMsg( "Data source is valid" );
      }
      else
      {
        QgsMessageLog::logMessage( tr( "Data source is invalid (%1)" ).arg( QString::fromUtf8( CPLGetLastErrorMsg() ) ), tr( "OGR" ) );
      }
    }
    else
    {
      QgsMessageLog::logMessage( tr( "Data source is invalid, no layer found (%1)" ).arg( QString::fromUtf8( CPLGetLastErrorMsg() ) ), tr( "OGR" ) );
    }
  }
  else
  {
    QgsMessageLog::logMessage( tr( "Data source is invalid (%1)" ).arg( QString::fromUtf8( CPLGetLastErrorMsg() ) ), tr( "OGR" ) );
  }

  // For shapefiles or MapInfo .tab, so as to allow concurrent opening between
  // QGIS and MapInfo, we go back to read-only mode for now.
  // We limit to those drivers as re-opening is relatively cheap (other drivers
  // like GeoJSON might do full content ingestion for example)
  if ( mValid && mode == OpenModeInitial && mWriteAccess &&
       ( ogrDriverName == QLatin1String( "ESRI Shapefile" ) || ogrDriverName == QLatin1String( "MapInfo File" ) ) )
  {
    OGR_DS_Destroy( ogrDataSource );
    ogrLayer = ogrOrigLayer = nullptr;
    mValid = false;

#if defined(GDAL_COMPUTE_VERSION)
    // In the case where we deal with a shapefile, it is possible that it has
    // pre-existing holes in the DBF (see #15407), so if using a GDAL version
    // recent enough to have reliable packing, do a packing at the first edit
    // action.
    if ( ogrDriverName == QLatin1String( "ESRI Shapefile" )  &&
         atoi( GDALVersionInfo( "VERSION_NUM" ) ) >= GDAL_COMPUTE_VERSION( 2, 1, 2 ) )
    {
      mShapefileMayBeCorrupted = true;
    }
#endif

    ogrDataSource = QgsOgrProviderUtils::OGROpenWrapper( mFilePath.toUtf8().constData(), false, &ogrDriver );

    mWriteAccess = false;

    if ( ogrDataSource )
    {
      // We get the layer which was requested by the uri. The layername
      // has precedence over the layerid if both are given.
      if ( mLayerName.isNull() )
      {
        ogrOrigLayer = OGR_DS_GetLayer( ogrDataSource, mLayerIndex );
      }
      else
      {
        ogrOrigLayer = OGR_DS_GetLayerByName( ogrDataSource, mLayerName.toUtf8().constData() );
      }

      ogrLayer = ogrOrigLayer;
    }
    if ( ogrLayer )
    {
      mValid = true;
      mDynamicWriteAccess = true;

      if ( !mSubsetString.isEmpty() )
      {
        int featuresCountedBackup = mFeaturesCounted;
        mFeaturesCounted = -1;
        mValid = setSubsetString( mSubsetString, false );
        mFeaturesCounted = featuresCountedBackup;
      }
    }
  }

  // For debug/testing purposes
  if ( !mValid )
    setProperty( "_debug_open_mode", "invalid" );
  else if ( mWriteAccess )
    setProperty( "_debug_open_mode", "read-write" );
  else
    setProperty( "_debug_open_mode", "read-only" );
}

void QgsOgrProvider::close()
{
  if ( ogrLayer != ogrOrigLayer )
  {
    OGR_DS_ReleaseResultSet( ogrDataSource, ogrLayer );
  }

  if ( ogrDataSource )
  {
    QgsOgrProviderUtils::OGRDestroyWrapper( ogrDataSource );
  }
  ogrDataSource = nullptr;
  ogrLayer = nullptr;
  ogrOrigLayer = nullptr;
  mValid = false;
  setProperty( "_debug_open_mode", "invalid" );

  invalidateCachedExtent( false );
}

void QgsOgrProvider::reloadData()
{
  forceReload();
  close();
  open( OpenModeSameAsCurrent );
  if ( !mValid )
    pushError( tr( "Cannot reopen datasource %1" ).arg( dataSourceUri() ) );
}

bool QgsOgrProvider::enterUpdateMode()
{
  if ( !mWriteAccessPossible )
  {
    return false;
  }
  if ( mWriteAccess )
  {
    ++mUpdateModeStackDepth;
    return true;
  }
  if ( mUpdateModeStackDepth == 0 )
  {
    Q_ASSERT( mDynamicWriteAccess );
    QgsDebugMsg( QString( "Reopening %1 in update mode" ).arg( dataSourceUri() ) );
    close();
    open( OpenModeForceUpdate );
    if ( !ogrDataSource || !mWriteAccess )
    {
      QgsMessageLog::logMessage( tr( "Cannot reopen datasource %1 in update mode" ).arg( dataSourceUri() ), tr( "OGR" ) );
      pushError( tr( "Cannot reopen datasource %1 in update mode" ).arg( dataSourceUri() ) );
      return false;
    }
  }
  ++mUpdateModeStackDepth;
  return true;
}

bool QgsOgrProvider::leaveUpdateMode()
{
  if ( !mWriteAccessPossible )
  {
    return false;
  }
  --mUpdateModeStackDepth;
  if ( mUpdateModeStackDepth < 0 )
  {
    QgsMessageLog::logMessage( tr( "Unbalanced call to leaveUpdateMode() w.r.t. enterUpdateMode()" ), tr( "OGR" ) );
    mUpdateModeStackDepth = 0;
    return false;
  }
  if ( !mDynamicWriteAccess )
  {
    return true;
  }
  if ( mUpdateModeStackDepth == 0 )
  {
    QgsDebugMsg( QString( "Reopening %1 in read-only mode" ).arg( dataSourceUri() ) );
    close();
    open( OpenModeForceReadOnly );
    if ( !ogrDataSource )
    {
      QgsMessageLog::logMessage( tr( "Cannot reopen datasource %1 in read-only mode" ).arg( dataSourceUri() ), tr( "OGR" ) );
      pushError( tr( "Cannot reopen datasource %1 in read-only mode" ).arg( dataSourceUri() ) );
      return false;
    }
  }
  return true;
}

bool QgsOgrProvider::isSaveAndLoadStyleToDatabaseSupported() const
{
  // We could potentially extend support for styling to other drivers
  // with multiple layer support.
  return ogrDriverName == QLatin1String( "GPKG" ) ||
         ogrDriverName == QLatin1String( "SQLite" );
}

// ---------------------------------------------------------------------------

static
OGRDataSourceH LoadDataSourceAndLayer( const QString &uri,
                                       OGRLayerH &hUserLayer,
                                       QString &errCause )
{
  hUserLayer = nullptr;
  bool isSubLayer;
  int layerIndex;
  QString layerName;
  QString subsetString;
  OGRwkbGeometryType ogrGeometryType;
  QString filePath = AnalyzeURI( uri,
                                 isSubLayer,
                                 layerIndex,
                                 layerName,
                                 subsetString,
                                 ogrGeometryType );

  OGRDataSourceH hDS = QgsOgrProviderUtils::OGROpenWrapper( filePath.toUtf8().constData(), true, nullptr );
  if ( !hDS )
  {
    QgsDebugMsg( "Connection to database failed.." );
    errCause = QObject::tr( "Connection to database failed" );
    return nullptr;
  }

  if ( !layerName.isEmpty() )
  {
    hUserLayer = OGR_DS_GetLayerByName( hDS, layerName.toUtf8().constData() );
    if ( !hUserLayer )
    {
      errCause = QObject::tr( "Cannot find layer %1." ).arg( layerName );
      QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
      return nullptr;
    }
  }
  else
  {
    hUserLayer = OGR_DS_GetLayer( hDS, layerIndex );
    if ( !hUserLayer )
    {
      errCause = QObject::tr( "Cannot find layer %1." ).arg( layerIndex );
      QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
      return nullptr;
    }
  }

  return hDS;
}


QGISEXTERN bool saveStyle( const QString &uri, const QString &qmlStyle, const QString &sldStyle,
                           const QString &styleName, const QString &styleDescription,
                           const QString &uiFileContent, bool useAsDefault, QString &errCause )
{
  OGRLayerH hUserLayer = nullptr;
  OGRDataSourceH hDS = LoadDataSourceAndLayer( uri, hUserLayer, errCause );
  if ( !hDS )
    return false;

  // check if layer_styles table already exist
  OGRLayerH hLayer = OGR_DS_GetLayerByName( hDS, "layer_styles" );
  if ( !hLayer )
  {
    // if not create it
    // Note: we use the same schema as in the SpatiaLite and postgre providers
    //for cross interoperability

    char **options = nullptr;
    // TODO: might need change if other drivers than GPKG / SQLite
    options = CSLSetNameValue( options, "FID", "id" );
    hLayer = OGR_DS_CreateLayer( hDS, "layer_styles", nullptr, wkbNone, options );
    CSLDestroy( options );
    if ( !hLayer )
    {
      errCause = QObject::tr( "Unable to save layer style. It's not possible to create the destination table on the database." );
      QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
      return false;
    }
    bool ok = true;
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "f_table_catalog", OFTString );
      OGR_Fld_SetWidth( fld, 256 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "f_table_schema", OFTString );
      OGR_Fld_SetWidth( fld, 256 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "f_table_name", OFTString );
      OGR_Fld_SetWidth( fld, 256 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "f_geometry_column", OFTString );
      OGR_Fld_SetWidth( fld, 256 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "styleName", OFTString );
      OGR_Fld_SetWidth( fld, 30 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "styleQML", OFTString );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "styleSLD", OFTString );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "useAsDefault", OFTInteger );
      OGR_Fld_SetSubType( fld, OFSTBoolean );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "description", OFTString );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "owner", OFTString );
      OGR_Fld_SetWidth( fld, 30 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "ui", OFTString );
      OGR_Fld_SetWidth( fld, 30 );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    {
      OGRFieldDefnH fld = OGR_Fld_Create( "update_time", OFTDateTime );
      OGR_Fld_SetDefault( fld, "CURRENT_TIMESTAMP" );
      ok &= OGR_L_CreateField( hLayer, fld, true ) == OGRERR_NONE;
      OGR_Fld_Destroy( fld );
    }
    if ( !ok )
    {
      errCause = QObject::tr( "Unable to save layer style. It's not possible to create the destination table on the database." );
      QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
      return false;
    }
  }

  QString realStyleName =
    styleName.isEmpty() ? QString( OGR_L_GetName( hUserLayer ) ) : styleName;

  OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn( hLayer );

  if ( useAsDefault )
  {
    QString oldDefaultQuery = QStringLiteral( "useAsDefault = 1 AND f_table_schema=''"
                              " AND f_table_name=%1"
                              " AND f_geometry_column=%2" )
                              .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetName( hUserLayer ) ) ) )
                              .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetGeometryColumn( hUserLayer ) ) ) );
    OGR_L_SetAttributeFilter( hLayer, oldDefaultQuery.toUtf8().constData() );
    OGRFeatureH hFeature = OGR_L_GetNextFeature( hLayer );
    if ( hFeature )
    {
      OGR_F_SetFieldInteger( hFeature,
                             OGR_FD_GetFieldIndex( hLayerDefn, "useAsDefault" ),
                             0 );
      bool ok = OGR_L_SetFeature( hLayer, hFeature ) == 0;
      OGR_F_Destroy( hFeature );
      if ( !ok )
      {
        QgsDebugMsg( "Could not unset previous useAsDefault style" );
      }
    }
  }

  QString checkQuery = QStringLiteral( "f_table_schema=''"
                                       " AND f_table_name=%1"
                                       " AND f_geometry_column=%2"
                                       " AND styleName=%3" )
                       .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetName( hUserLayer ) ) ) )
                       .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetGeometryColumn( hUserLayer ) ) ) )
                       .arg( QgsOgrProviderUtils::quotedValue( realStyleName ) );
  OGR_L_SetAttributeFilter( hLayer, checkQuery.toUtf8().constData() );
  OGR_L_ResetReading( hLayer );
  OGRFeatureH hFeature = OGR_L_GetNextFeature( hLayer );
  bool bNew = true;

  if ( hFeature != NULL )
  {
    QgsSettings settings;
    // Only used in tests. Do not define it for interactive implication
    QVariant overwriteStyle = settings.value( QStringLiteral( "qgis/overwriteStyle" ) );
    if ( ( !overwriteStyle.isNull() && !overwriteStyle.toBool() ) ||
         ( overwriteStyle.isNull() &&
           QMessageBox::question( nullptr, QObject::tr( "Save style in database" ),
                                  QObject::tr( "A style named \"%1\" already exists in the database for this layer. Do you want to overwrite it?" )
                                  .arg( realStyleName ),
                                  QMessageBox::Yes | QMessageBox::No ) == QMessageBox::No ) )
    {
      errCause = QObject::tr( "Operation aborted" );
      OGR_F_Destroy( hFeature );
      QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
      return false;
    }
    bNew = false;
  }
  else
  {
    hFeature = OGR_F_Create( hLayerDefn );
    OGR_F_SetFieldString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "f_table_catalog" ),
                          "" );
    OGR_F_SetFieldString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "f_table_schema" ),
                          "" );
    OGR_F_SetFieldString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "f_table_name" ),
                          OGR_L_GetName( hUserLayer ) );
    OGR_F_SetFieldString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "f_geometry_column" ),
                          OGR_L_GetGeometryColumn( hUserLayer ) );
    OGR_F_SetFieldString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "styleName" ),
                          realStyleName.toUtf8().constData() );
    if ( !uiFileContent.isEmpty() )
    {
      OGR_F_SetFieldString( hFeature,
                            OGR_FD_GetFieldIndex( hLayerDefn, "ui" ),
                            uiFileContent.toUtf8().constData() );
    }
  }
  OGR_F_SetFieldString( hFeature,
                        OGR_FD_GetFieldIndex( hLayerDefn, "styleQML" ),
                        qmlStyle.toUtf8().constData() );
  OGR_F_SetFieldString( hFeature,
                        OGR_FD_GetFieldIndex( hLayerDefn, "styleSLD" ),
                        sldStyle.toUtf8().constData() );
  OGR_F_SetFieldInteger( hFeature,
                         OGR_FD_GetFieldIndex( hLayerDefn, "useAsDefault" ),
                         useAsDefault ? 1 : 0 );
  OGR_F_SetFieldString( hFeature,
                        OGR_FD_GetFieldIndex( hLayerDefn, "description" ),
                        ( styleDescription.isEmpty() ? QDateTime::currentDateTime().toString() : styleDescription ).toUtf8().constData() );
  OGR_F_SetFieldString( hFeature,
                        OGR_FD_GetFieldIndex( hLayerDefn, "owner" ),
                        "" );

  bool bFeatureOK;
  if ( bNew )
    bFeatureOK = OGR_L_CreateFeature( hLayer, hFeature ) == OGRERR_NONE;
  else
    bFeatureOK = OGR_L_SetFeature( hLayer, hFeature ) == OGRERR_NONE;

  OGR_F_Destroy( hFeature );

  QgsOgrProviderUtils::OGRDestroyWrapper( hDS );

  if ( !bFeatureOK )
  {
    QgsMessageLog::logMessage( QObject::tr( "Error updating style" ) );
    errCause = QObject::tr( "Error looking for style. The query was logged" );
    return false;
  }

  return true;
}


QGISEXTERN QString loadStyle( const QString &uri, QString &errCause )
{
  OGRLayerH hUserLayer = nullptr;
  OGRDataSourceH hDS = LoadDataSourceAndLayer( uri, hUserLayer, errCause );
  if ( !hDS )
    return "";

  // check if layer_styles table already exist
  OGRLayerH hLayer = OGR_DS_GetLayerByName( hDS, "layer_styles" );
  if ( !hLayer )
  {
    errCause = QObject::tr( "Cannot find layer_styles layer" );
    QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
    return "";
  }

  QString selectQmlQuery = QStringLiteral( "f_table_schema=''"
                           " AND f_table_name=%1"
                           " AND f_geometry_column=%2"
                           " ORDER BY CASE WHEN useAsDefault THEN 1 ELSE 2 END"
                           ",update_time DESC LIMIT 1" )
                           .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetName( hUserLayer ) ) ) )
                           .arg( QgsOgrProviderUtils::quotedValue( QString( OGR_L_GetGeometryColumn( hUserLayer ) ) ) );
  OGR_L_SetAttributeFilter( hLayer, selectQmlQuery.toUtf8().constData() );
  OGR_L_ResetReading( hLayer );
  OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn( hLayer );
  QString styleQML;
  qlonglong moreRecentTimestamp = 0;
  while ( true )
  {
    OGRFeatureH hFeat = OGR_L_GetNextFeature( hLayer );
    if ( !hFeat )
      break;
    if ( OGR_F_GetFieldAsInteger( hFeat, OGR_FD_GetFieldIndex( hLayerDefn, "useAsDefault" ) ) )
    {
      styleQML = QString::fromUtf8(
                   OGR_F_GetFieldAsString( hFeat, OGR_FD_GetFieldIndex( hLayerDefn, "styleQML" ) ) );
      OGR_F_Destroy( hFeat );
      break;
    }

    int  year, month, day, hour, minute, second, TZ;
    OGR_F_GetFieldAsDateTime( hFeat, OGR_FD_GetFieldIndex( hLayerDefn, "update_time" ),
                              &year, &month, &day, &hour, &minute, &second, &TZ );
    qlonglong ts = second + minute * 60 + hour * 3600 + day * 24 * 3600 +
                   ( qlonglong )month * 31 * 24 * 3600 + ( qlonglong )year * 12 * 31 * 24 * 3600;
    if ( ts > moreRecentTimestamp )
    {
      moreRecentTimestamp = ts;
      styleQML = QString::fromUtf8(
                   OGR_F_GetFieldAsString( hFeat, OGR_FD_GetFieldIndex( hLayerDefn, "styleQML" ) ) );

    }
    OGR_F_Destroy( hFeat );
  }

  QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
  return styleQML;
}

QGISEXTERN int listStyles( const QString &uri, QStringList &ids, QStringList &names,
                           QStringList &descriptions, QString &errCause )
{
  OGRLayerH hUserLayer = nullptr;
  OGRDataSourceH hDS = LoadDataSourceAndLayer( uri, hUserLayer, errCause );
  if ( !hDS )
    return -1;

  // check if layer_styles table already exist
  OGRLayerH hLayer = OGR_DS_GetLayerByName( hDS, "layer_styles" );
  if ( !hLayer || OGR_L_GetFeatureCount( hLayer, TRUE ) == 0 )
  {
    QgsMessageLog::logMessage( QObject::tr( "No styles available on DB" ) );
    errCause = QObject::tr( "No styles available on DB" );
    QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
    return 0;
  }

  OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn( hLayer );

  OGR_L_ResetReading( hLayer );

  QList<qlonglong> listTimestamp;
  QMap<int, QString> mapIdToStyleName;
  QMap<int, QString> mapIdToDescription;
  QMap<qlonglong, QList<int> > mapTimestampToId;
  int numberOfRelatedStyles = 0;
  while ( true )
  {
    OGRFeatureH hFeature = OGR_L_GetNextFeature( hLayer );
    if ( !hFeature )
      break;

    QString tableName( QString::fromUtf8(
                         OGR_F_GetFieldAsString( hFeature,
                             OGR_FD_GetFieldIndex( hLayerDefn, "f_table_name" ) ) ) );
    QString geometryColumn( QString::fromUtf8(
                              OGR_F_GetFieldAsString( hFeature,
                                  OGR_FD_GetFieldIndex( hLayerDefn, "f_geometry_column" ) ) ) );
    QString styleName( QString::fromUtf8(
                         OGR_F_GetFieldAsString( hFeature,
                             OGR_FD_GetFieldIndex( hLayerDefn, "styleName" ) ) ) );
    QString description( QString::fromUtf8(
                           OGR_F_GetFieldAsString( hFeature,
                               OGR_FD_GetFieldIndex( hLayerDefn, "description" ) ) ) );
    int fid = static_cast<int>( OGR_F_GetFID( hFeature ) );
    if ( tableName == QString::fromUtf8( OGR_L_GetName( hUserLayer ) ) &&
         geometryColumn == QString::fromUtf8( OGR_L_GetGeometryColumn( hUserLayer ) ) )
    {
      // Append first all related styles
      QString id( QString( "%1" ).arg( fid ) );
      ids.append( id );
      names.append( styleName );
      descriptions.append( description );
      ++ numberOfRelatedStyles;
    }
    else
    {
      int  year, month, day, hour, minute, second, TZ;
      OGR_F_GetFieldAsDateTime( hFeature, OGR_FD_GetFieldIndex( hLayerDefn, "update_time" ),
                                &year, &month, &day, &hour, &minute, &second, &TZ );
      qlonglong ts = second + minute * 60 + hour * 3600 + day * 24 * 3600 +
                     ( qlonglong )month * 31 * 24 * 3600 + ( qlonglong )year * 12 * 31 * 24 * 3600;

      listTimestamp.append( ts );
      mapIdToStyleName[fid] = styleName;
      mapIdToDescription[fid] = styleName;
      mapTimestampToId[ts].append( fid );
    }

    OGR_F_Destroy( hFeature );
  }

  std::sort( listTimestamp.begin(), listTimestamp.end() );
  // Sort from most recent to least recent
  for ( int i = listTimestamp.size() - 1; i >= 0; i-- )
  {
    const QList<int> &listId = mapTimestampToId[listTimestamp[i]];
    for ( int j = 0; j < listId.size(); j++ )
    {
      int fid = listId[j];
      QString id( QString( "%1" ).arg( fid ) );
      ids.append( id );
      names.append( mapIdToStyleName[fid] );
      descriptions.append( mapIdToDescription[fid] );
    }
  }

  QgsOgrProviderUtils::OGRDestroyWrapper( hDS );

  return numberOfRelatedStyles;
}

QGISEXTERN QString getStyleById( const QString &uri, QString styleId, QString &errCause )
{
  OGRLayerH hUserLayer = nullptr;
  OGRDataSourceH hDS = LoadDataSourceAndLayer( uri, hUserLayer, errCause );
  if ( !hDS )
    return "";

  // check if layer_styles table already exist
  OGRLayerH hLayer = OGR_DS_GetLayerByName( hDS, "layer_styles" );
  if ( !hLayer )
  {
    errCause = QObject::tr( "Cannot find layer_styles layer" );
    QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
    return "";
  }

  bool ok;
  int id = styleId.toInt( &ok );
  if ( !ok )
  {
    errCause = QObject::tr( "Invalid style identifier" );
    QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
    return "";
  }

  OGRFeatureH hFeature = OGR_L_GetFeature( hLayer, id );
  if ( !hFeature )
  {
    errCause = QObject::tr( "No style corresponding to style identifier" );
    QgsOgrProviderUtils::OGRDestroyWrapper( hDS );
    return "";
  }

  OGRFeatureDefnH hLayerDefn = OGR_L_GetLayerDefn( hLayer );
  QString styleQML( QString::fromUtf8(
                      OGR_F_GetFieldAsString( hFeature,
                          OGR_FD_GetFieldIndex( hLayerDefn, "styleQML" ) ) ) );

  OGR_F_Destroy( hFeature );

  QgsOgrProviderUtils::OGRDestroyWrapper( hDS );

  return styleQML;
}


// ---------------------------------------------------------------------------

QGISEXTERN QgsVectorLayerExporter::ExportError createEmptyLayer(
  const QString &uri,
  const QgsFields &fields,
  QgsWkbTypes::Type wkbType,
  const QgsCoordinateReferenceSystem &srs,
  bool overwrite,
  QMap<int, int> *oldToNewAttrIdxMap,
  QString *errorMessage,
  const QMap<QString, QVariant> *options )
{
  return QgsOgrProvider::createEmptyLayer(
           uri, fields, wkbType, srs, overwrite,
           oldToNewAttrIdxMap, errorMessage, options
         );
}

QGISEXTERN void cleanupProvider()
{
  QgsOgrConnPool::cleanupInstance();
  // NOTE: QgsApplication takes care of
  // calling OGRCleanupAll();
}



QGISEXTERN bool deleteLayer( const QString &uri, QString &errCause )
{
  bool isSubLayer;
  int layerIndex;
  QString layerName;
  QString subsetString;
  OGRwkbGeometryType ogrGeometryType;
  QString filePath = AnalyzeURI( uri,
                                 isSubLayer,
                                 layerIndex,
                                 layerName,
                                 subsetString,
                                 ogrGeometryType );

  OGRDataSourceH hDS = GDALOpenEx( filePath.toLocal8Bit().data(), GDAL_OF_RASTER | GDAL_OF_VECTOR | GDAL_OF_UPDATE, NULL, NULL, NULL );
  if ( hDS  && ( ! layerName.isEmpty() || layerIndex != -1 ) )
  {
    if ( layerIndex == -1 )
    {
      for ( int i = 0; i < GDALDatasetGetLayerCount( hDS ); i++ )
      {
        OGRLayerH hL = GDALDatasetGetLayer( hDS, i );
        if ( layerName == QString( OGR_L_GetName( hL ) ) )
        {
          layerIndex = i;
        }
      }
    }
    OGRErr error = GDALDatasetDeleteLayer( hDS, layerIndex );
    switch ( error )
    {
      case OGRERR_NOT_ENOUGH_DATA:
        errCause = QObject::tr( "Not enough data to deserialize" );
        break;
      case OGRERR_NOT_ENOUGH_MEMORY:
        errCause = QObject::tr( "Not enough memory" );
        break;
      case OGRERR_UNSUPPORTED_GEOMETRY_TYPE:
        errCause = QObject::tr( "Unsupported geometry type" );
        break;
      case OGRERR_UNSUPPORTED_OPERATION:
        errCause = QObject::tr( "Unsupported operation" );
        break;
      case OGRERR_CORRUPT_DATA:
        errCause = QObject::tr( "Corrupt data" );
        break;
      case OGRERR_FAILURE:
        errCause = QObject::tr( "Failure" );
        break;
      case OGRERR_UNSUPPORTED_SRS:
        errCause = QObject::tr( "Unsupported SRS" );
        break;
      case OGRERR_INVALID_HANDLE:
        errCause = QObject::tr( "Invalid handle" );
        break;
      case OGRERR_NON_EXISTING_FEATURE:
        errCause = QObject::tr( "Non existing feature" );
        break;
      default:
      case OGRERR_NONE:
        errCause = QObject::tr( "Success" );
        break;
    }
    errCause = QObject::tr( "GDAL result code: %s" ).arg( errCause );
    return error == OGRERR_NONE;
  }
  // This should never happen:
  errCause = QObject::tr( "Layer not found: %s" ).arg( uri );
  return false;
}
