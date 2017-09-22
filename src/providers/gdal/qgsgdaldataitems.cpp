/***************************************************************************
    qgsgdaldataitems.cpp
    ---------------------
    begin                : October 2011
    copyright            : (C) 2011 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgsgdaldataitems.h"
#include "qgsgdalprovider.h"
#include "qgslogger.h"
#include "qgssettings.h"

#include <QFileInfo>

// defined in qgsgdalprovider.cpp
void buildSupportedRasterFileFilterAndExtensions( QString &fileFiltersString, QStringList &extensions, QStringList &wildcards );


QgsGdalLayerItem::QgsGdalLayerItem( QgsDataItem *parent,
                                    QString name, QString path, QString uri,
                                    QStringList *sublayers )
  : QgsLayerItem( parent, name, path, uri, QgsLayerItem::Raster, QStringLiteral( "gdal" ) )
{
  mToolTip = uri;
  // save sublayers for subsequent access
  // if there are sublayers, set populated=false so item can be populated on demand
  if ( sublayers && !sublayers->isEmpty() )
  {
    mSublayers = *sublayers;
    // We have sublayers: we are able to create children!
    mCapabilities |= Fertile;
    setState( NotPopulated );
  }
  else
    setState( Populated );

  GDALAllRegister();
  GDALDatasetH hDS = GDALOpen( mPath.toUtf8().constData(), GA_Update );

  if ( hDS )
  {
    mCapabilities |= SetCrs;
    GDALClose( hDS );
  }
}


bool QgsGdalLayerItem::setCrs( const QgsCoordinateReferenceSystem &crs )
{
  GDALDatasetH hDS = GDALOpen( mPath.toUtf8().constData(), GA_Update );
  if ( !hDS )
    return false;

  QString wkt = crs.toWkt();
  if ( GDALSetProjection( hDS, wkt.toLocal8Bit().data() ) != CE_None )
  {
    GDALClose( hDS );
    QgsDebugMsg( "Could not set CRS" );
    return false;
  }

  GDALClose( hDS );
  return true;
}

QVector<QgsDataItem *> QgsGdalLayerItem::createChildren()
{
  QgsDebugMsgLevel( "Entered, path=" + path(), 3 );
  QVector<QgsDataItem *> children;

  // get children from sublayers
  if ( !mSublayers.isEmpty() )
  {
    QgsDataItem *childItem = nullptr;
    QgsDebugMsgLevel( QString( "got %1 sublayers" ).arg( mSublayers.count() ), 3 );
    for ( int i = 0; i < mSublayers.count(); i++ )
    {
      QString name = mSublayers[i];
      // if netcdf/hdf use all text after filename
      // for hdf4 it would be best to get description, because the subdataset_index is not very practical
      if ( name.startsWith( QLatin1String( "netcdf" ), Qt::CaseInsensitive ) ||
           name.startsWith( QLatin1String( "hdf" ), Qt::CaseInsensitive ) )
        name = name.mid( name.indexOf( mPath ) + mPath.length() + 1 );
      else
      {
        // remove driver name and file name and initial ':'
        name.remove( name.split( ':' )[0] + ':' );
        name.remove( mPath );
      }
      // remove any : or " left over
      if ( name.startsWith( ':' ) ) name.remove( 0, 1 );
      if ( name.startsWith( '\"' ) ) name.remove( 0, 1 );
      if ( name.endsWith( ':' ) ) name.chop( 1 );
      if ( name.endsWith( '\"' ) ) name.chop( 1 );

      childItem = new QgsGdalLayerItem( this, name, mSublayers[i], mSublayers[i] );
      if ( childItem )
      {
        children.append( childItem );
      }
    }
  }

  return children;
}

QString QgsGdalLayerItem::layerName() const
{
  QFileInfo info( name() );
  if ( info.suffix() == QLatin1String( "gz" ) )
    return info.baseName();
  else
    return info.completeBaseName();
}

// ---------------------------------------------------------------------------

static QString sFilterString;
static QStringList sExtensions = QStringList();
static QStringList sWildcards = QStringList();
static QMutex sBuildingFilters;

QGISEXTERN int dataCapabilities()
{
  return QgsDataProvider::File | QgsDataProvider::Dir | QgsDataProvider::Net;
}

QGISEXTERN QgsDataItem *dataItem( QString path, QgsDataItem *parentItem )
{
  if ( path.isEmpty() )
    return nullptr;

  QgsDebugMsgLevel( "thePath = " + path, 2 );

  // zip settings + info
  QgsSettings settings;
  QString scanZipSetting = settings.value( QStringLiteral( "qgis/scanZipInBrowser2" ), "basic" ).toString();
  QString vsiPrefix = QgsZipItem::vsiPrefix( path );
  bool is_vsizip = ( vsiPrefix == QLatin1String( "/vsizip/" ) );
  bool is_vsigzip = ( vsiPrefix == QLatin1String( "/vsigzip/" ) );
  bool is_vsitar = ( vsiPrefix == QLatin1String( "/vsitar/" ) );

  // should we check ext. only?
  // check if scanItemsInBrowser2 == extension or parent dir in scanItemsFastScanUris
  // TODO - do this in dir item, but this requires a way to inform which extensions are supported by provider
  // maybe a callback function or in the provider registry?
  bool scanExtSetting = false;
  if ( ( settings.value( QStringLiteral( "qgis/scanItemsInBrowser2" ),
                         "extension" ).toString() == QLatin1String( "extension" ) ) ||
       ( parentItem && settings.value( QStringLiteral( "qgis/scanItemsFastScanUris" ),
                                       QStringList() ).toStringList().contains( parentItem->path() ) ) ||
       ( ( is_vsizip || is_vsitar ) && parentItem && parentItem->parent() &&
         settings.value( QStringLiteral( "qgis/scanItemsFastScanUris" ),
                         QStringList() ).toStringList().contains( parentItem->parent()->path() ) ) )
  {
    scanExtSetting = true;
  }

  // get suffix, removing .gz if present
  QString tmpPath = path; //path used for testing, not for layer creation
  if ( is_vsigzip )
    tmpPath.chop( 3 );
  QFileInfo info( tmpPath );
  QString suffix = info.suffix().toLower();
  // extract basename with extension
  info.setFile( path );
  QString name = info.fileName();

  QgsDebugMsgLevel( "path= " + path + " tmpPath= " + tmpPath + " name= " + name
                    + " suffix= " + suffix + " vsiPrefix= " + vsiPrefix, 3 );

  // allow only normal files or VSIFILE items to continue
  if ( !info.isFile() && vsiPrefix == QLatin1String( "" ) )
    return nullptr;

  // get supported extensions
  if ( sExtensions.isEmpty() )
  {
    // this code may be executed by more threads at once!
    // use a mutex to make sure this does not happen (so there's no crash on start)
    QMutexLocker locker( &sBuildingFilters );
    if ( sExtensions.isEmpty() )
    {
      buildSupportedRasterFileFilterAndExtensions( sFilterString, sExtensions, sWildcards );
      QgsDebugMsgLevel( "extensions: " + sExtensions.join( " " ), 2 );
      QgsDebugMsgLevel( "wildcards: " + sWildcards.join( " " ), 2 );
    }
  }

  // skip *.aux.xml files (GDAL auxiliary metadata files),
  // *.shp.xml files (ESRI metadata) and *.tif.xml files (TIFF metadata)
  // unless that extension is in the list (*.xml might be though)
  if ( path.endsWith( QLatin1String( ".aux.xml" ), Qt::CaseInsensitive ) &&
       !sExtensions.contains( QStringLiteral( "aux.xml" ) ) )
    return nullptr;
  if ( path.endsWith( QLatin1String( ".shp.xml" ), Qt::CaseInsensitive ) &&
       !sExtensions.contains( QStringLiteral( "shp.xml" ) ) )
    return nullptr;
  if ( path.endsWith( QLatin1String( ".tif.xml" ), Qt::CaseInsensitive ) &&
       !sExtensions.contains( QStringLiteral( "tif.xml" ) ) )
    return nullptr;

  // Filter files by extension
  if ( !sExtensions.contains( suffix ) )
  {
    bool matches = false;
    Q_FOREACH ( const QString &wildcard, sWildcards )
    {
      QRegExp rx( wildcard, Qt::CaseInsensitive, QRegExp::Wildcard );
      if ( rx.exactMatch( info.fileName() ) )
      {
        matches = true;
        break;
      }
    }
    if ( !matches )
      return nullptr;
  }

  // fix vsifile path and name
  if ( vsiPrefix != QLatin1String( "" ) )
  {
    // add vsiPrefix to path if needed
    if ( !path.startsWith( vsiPrefix ) )
      path = vsiPrefix + path;
    // if this is a /vsigzip/path_to_zip.zip/file_inside_zip remove the full path from the name
    // no need to change the name I believe
#if 0
    if ( ( is_vsizip || is_vsitar ) && ( path != vsiPrefix + parentItem->path() ) )
    {
      name = path;
      name = name.replace( vsiPrefix + parentItem->path() + '/', "" );
    }
#endif
  }

  // return item without testing if:
  // scanExtSetting
  // or zipfile and scan zip == "Basic scan"
  if ( scanExtSetting ||
       ( ( is_vsizip || is_vsitar ) && scanZipSetting == QLatin1String( "basic" ) ) )
  {
    // if this is a VRT file make sure it is raster VRT to avoid duplicates
    if ( suffix == QLatin1String( "vrt" ) )
    {
      // do not print errors, but write to debug
      CPLPushErrorHandler( CPLQuietErrorHandler );
      CPLErrorReset();
      if ( ! GDALIdentifyDriver( path.toUtf8().constData(), nullptr ) )
      {
        QgsDebugMsgLevel( "Skipping VRT file because root is not a GDAL VRT", 2 );
        CPLPopErrorHandler();
        return nullptr;
      }
      CPLPopErrorHandler();
    }
    // add the item
    QStringList sublayers;
    QgsDebugMsgLevel( QString( "adding item name=%1 path=%2" ).arg( name, path ), 2 );
    QgsLayerItem *item = new QgsGdalLayerItem( parentItem, name, path, path, &sublayers );
    if ( item )
      return item;
  }

  // test that file is valid with GDAL
  GDALAllRegister();
  // do not print errors, but write to debug
  CPLPushErrorHandler( CPLQuietErrorHandler );
  CPLErrorReset();
  GDALDatasetH hDS = GDALOpen( path.toUtf8().constData(), GA_ReadOnly );
  CPLPopErrorHandler();

  if ( ! hDS )
  {
    QgsDebugMsg( QString( "GDALOpen error # %1 : %2 " ).arg( CPLGetLastErrorNo() ).arg( CPLGetLastErrorMsg() ) );
    return nullptr;
  }

  QStringList sublayers = QgsGdalProvider::subLayers( hDS );

  GDALClose( hDS );

  QgsDebugMsgLevel( "GdalDataset opened " + path, 2 );

  QgsLayerItem *item = new QgsGdalLayerItem( parentItem, name, path, path,
      &sublayers );

  return item;
}

