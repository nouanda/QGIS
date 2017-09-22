/***************************************************************************
                          qgspluginmanager.cpp  -  description
                             -------------------
    begin                : Someday 2003
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

#include <cmath>

#include <QApplication>
#include <QFileDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QLibrary>
#include <QStandardItem>
#include <QPushButton>
#include <QRegExp>
#include <QSortFilterProxyModel>
#include <QActionGroup>
#include <QTextStream>
#include <QTimer>
#include <QDesktopServices>

#include "qgis.h"
#include "qgisapp.h"
#include "qgsapplication.h"
#include "qgsconfig.h"
#include "qgsproviderregistry.h"
#include "qgspluginregistry.h"
#include "qgspluginsortfilterproxymodel.h"
#include "qgspythonrunner.h"
#include "qgspluginmanager.h"
#include "qgisplugin.h"
#include "qgslogger.h"
#include "qgspluginitemdelegate.h"
#include "qgssettings.h"
#ifdef WITH_BINDINGS
#include "qgspythonutils.h"
#endif

// Do we need this?
// #define TESTLIB
#ifdef TESTLIB
// This doesn't work on windows and causes problems with plugins
// on OS X (the code doesn't cause a problem but including dlfcn.h
// renders plugins unloadable)
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACX)
#include <dlfcn.h>
#endif
#endif


QgsPluginManager::QgsPluginManager( QWidget *parent, bool pluginsAreEnabled, Qt::WindowFlags fl )
  : QgsOptionsDialogBase( QStringLiteral( "PluginManager" ), parent, fl )
{
  // initialize pointer
  mPythonUtils = nullptr;

  setupUi( this );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsPluginManager::showHelp );

  // QgsOptionsDialogBase handles saving/restoring of geometry, splitter and current tab states,
  // switching vertical tabs between icon/text to icon-only modes (splitter collapsed to left),
  // and connecting QDialogButtonBox's accepted/rejected signals to dialog's accept/reject slots
  initOptionsBase( true );

  // Don't let QgsOptionsDialogBase to narrow the vertical tab list widget
  mOptListWidget->setMaximumWidth( 16777215 );

  // Restiore UI state for widgets not handled by QgsOptionsDialogBase
  QgsSettings settings;
  mPluginsDetailsSplitter->restoreState( settings.value( QStringLiteral( "Windows/PluginManager/secondSplitterState" ) ).toByteArray() );

  // load translated description strings from qgspluginmanager_texts
  initTabDescriptions();

  // set internal variable
  mPluginsAreEnabled = pluginsAreEnabled;

  // Init models
  mModelPlugins = new QStandardItemModel( 0, 1 );
  mModelProxy = new QgsPluginSortFilterProxyModel( this );
  mModelProxy->setSourceModel( mModelPlugins );
  mModelProxy->setSortCaseSensitivity( Qt::CaseInsensitive );
  mModelProxy->setSortRole( Qt::DisplayRole );
  mModelProxy->setDynamicSortFilter( true );
  mModelProxy->sort( 0, Qt::AscendingOrder );
  vwPlugins->setModel( mModelProxy );
  vwPlugins->setItemDelegate( new QgsPluginItemDelegate( vwPlugins ) );
  vwPlugins->setFocus();

  // Preset widgets
  leFilter->setFocus( Qt::MouseFocusReason );
  wvDetails->page()->setLinkDelegationPolicy( QWebPage::DelegateAllLinks );

  // Don't restore the last used tab from QgsSettings
  mOptionsListWidget->setCurrentRow( 0 );

  // Connect other signals
  connect( mOptionsListWidget, &QListWidget::currentRowChanged, this, &QgsPluginManager::setCurrentTab );
  connect( vwPlugins->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsPluginManager::currentPluginChanged );
  connect( mModelPlugins, &QStandardItemModel::itemChanged, this, &QgsPluginManager::pluginItemChanged );
  // Force setting the status filter (if the active tab was 0, the setCurrentRow( 0 ) above doesn't take any action)
  setCurrentTab( 0 );

  // Hide widgets only suitable with Python support enabled (they will be uncovered back in setPythonUtils)
  buttonUpgradeAll->hide();
  buttonInstall->hide();
  buttonUninstall->hide();
  frameSettings->setHidden( true );

  voteRating->hide();
  voteLabel->hide();
  voteSlider->hide();
  voteSubmit->hide();
#ifndef WITH_QTWEBKIT
  connect( voteSubmit, SIGNAL( clicked() ), this, SLOT( submitVote() ) );
#endif

  // Init the message bar instance
  msgBar = new QgsMessageBar( this );
  msgBar->setSizePolicy( QSizePolicy::Minimum, QSizePolicy::Fixed );
  vlayoutRightColumn->insertWidget( 0, msgBar );
}



QgsPluginManager::~QgsPluginManager()
{
  delete mModelProxy;
  delete mModelPlugins;

  QgsSettings settings;
  settings.setValue( QStringLiteral( "Windows/PluginManager/secondSplitterState" ), mPluginsDetailsSplitter->saveState() );
}



void QgsPluginManager::setPythonUtils( QgsPythonUtils *pythonUtils )
{
  mPythonUtils = pythonUtils;

  // Now enable Python support:
  // Show and preset widgets only suitable when Python support active
  buttonUpgradeAll->show();
  buttonInstall->show();
  buttonUninstall->show();
  frameSettings->setHidden( false );
  labelNoPython->setHidden( true );
  buttonRefreshRepos->setEnabled( false );
  buttonEditRep->setEnabled( false );
  buttonDeleteRep->setEnabled( false );

  // Add context menu to the plugins list view
  QAction *actionSortByName = new QAction( tr( "sort by name" ), vwPlugins );
  QAction *actionSortByDownloads = new QAction( tr( "sort by downloads" ), vwPlugins );
  QAction *actionSortByVote = new QAction( tr( "sort by vote" ), vwPlugins );
  QAction *actionSortByStatus = new QAction( tr( "sort by status" ), vwPlugins );
  actionSortByName->setCheckable( true );
  actionSortByDownloads->setCheckable( true );
  actionSortByVote->setCheckable( true );
  actionSortByStatus->setCheckable( true );
  QActionGroup *group = new QActionGroup( vwPlugins );
  actionSortByName->setActionGroup( group );
  actionSortByDownloads->setActionGroup( group );
  actionSortByVote->setActionGroup( group );
  actionSortByStatus->setActionGroup( group );
  actionSortByName->setChecked( true );
  vwPlugins->addAction( actionSortByName );
  vwPlugins->addAction( actionSortByDownloads );
  vwPlugins->addAction( actionSortByVote );
  vwPlugins->addAction( actionSortByStatus );
  vwPlugins->setContextMenuPolicy( Qt::ActionsContextMenu );
  connect( actionSortByName, &QAction::triggered, mModelProxy, &QgsPluginSortFilterProxyModel::sortPluginsByName );
  connect( actionSortByDownloads, &QAction::triggered, mModelProxy, &QgsPluginSortFilterProxyModel::sortPluginsByDownloads );
  connect( actionSortByVote, &QAction::triggered, mModelProxy, &QgsPluginSortFilterProxyModel::sortPluginsByVote );
  connect( actionSortByStatus, &QAction::triggered, mModelProxy, &QgsPluginSortFilterProxyModel::sortPluginsByStatus );

  // get the QgsSettings group from the installer
  QString settingsGroup;
  QgsPythonRunner::eval( QStringLiteral( "pyplugin_installer.instance().exportSettingsGroup()" ), settingsGroup );

  // Initialize list of allowed checking intervals
  mCheckingOnStartIntervals << 0 << 1 << 3 << 7 << 14 << 30;

  // Initialize the "Settings" tab widgets
  QgsSettings settings;
  if ( settings.value( settingsGroup + "/checkOnStart", false ).toBool() )
  {
    ckbCheckUpdates->setChecked( true );
  }

  if ( settings.value( settingsGroup + "/allowExperimental", false ).toBool() )
  {
    ckbExperimental->setChecked( true );
  }

  if ( settings.value( settingsGroup + "/allowDeprecated", false ).toBool() )
  {
    ckbDeprecated->setChecked( true );
  }


  int interval = settings.value( settingsGroup + "/checkOnStartInterval", "" ).toInt();
  int indx = mCheckingOnStartIntervals.indexOf( interval ); // if none found, just use -1 index.
  comboInterval->setCurrentIndex( indx );
}



void QgsPluginManager::loadPlugin( const QString &id )
{
  const QMap<QString, QString> *plugin = pluginMetadata( id );

  if ( ! plugin )
  {
    return;
  }

  QApplication::setOverrideCursor( Qt::WaitCursor );

  QgsPluginRegistry *pRegistry = QgsPluginRegistry::instance();
  QString library = plugin->value( QStringLiteral( "library" ) );
  if ( plugin->value( QStringLiteral( "pythonic" ) ) == QLatin1String( "true" ) )
  {
    library = plugin->value( QStringLiteral( "id" ) );
    QgsDebugMsg( "Loading Python plugin: " + library );
    pRegistry->loadPythonPlugin( library );
  }
  else // C++ plugin
  {
    QgsDebugMsg( "Loading C++ plugin: " + library );
    pRegistry->loadCppPlugin( library );
  }

  QgsDebugMsg( "Plugin loaded: " + library );
  QApplication::restoreOverrideCursor();
}



void QgsPluginManager::unloadPlugin( const QString &id )
{
  const QMap<QString, QString> *plugin = pluginMetadata( id );

  if ( ! plugin )
  {
    return;
  }

  QgsPluginRegistry *pRegistry = QgsPluginRegistry::instance();
  QString library = plugin->value( QStringLiteral( "library" ) );

  if ( plugin->value( QStringLiteral( "pythonic" ) ) == QLatin1String( "true" ) )
  {
    library = plugin->value( QStringLiteral( "id" ) );
    QgsDebugMsg( "Unloading Python plugin: " + library );
    pRegistry->unloadPythonPlugin( library );
  }
  else // C++ plugin
  {
    QgsDebugMsg( "Unloading C++ plugin: " + library );
    pRegistry->unloadCppPlugin( library );
  }
}



void QgsPluginManager::savePluginState( QString id, bool state )
{
  const QMap<QString, QString> *plugin = pluginMetadata( id );
  if ( ! plugin )
  {
    return;
  }

  QgsSettings settings;
  if ( plugin->value( QStringLiteral( "pythonic" ) ) == QLatin1String( "true" ) )
  {
    // Python plugin
    settings.setValue( "/PythonPlugins/" + id, state );
  }
  else
  {
    // C++ plugin
    // Trim "cpp:" prefix from cpp plugin id
    id = id.mid( 4 );
    settings.setValue( "/Plugins/" + id, state );
  }
}



void QgsPluginManager::getCppPluginsMetadata()
{
  QString sharedLibExtension;
#if defined(Q_OS_WIN) || defined(__CYGWIN__)
  sharedLibExtension = "*.dll";
#else
  sharedLibExtension = QStringLiteral( "*.so*" );
#endif

  // check all libs in the current ans user plugins directories, and get name and descriptions
  // First, the qgis install directory/lib (this info is available from the provider registry so we use it here)
  QgsProviderRegistry *pr = QgsProviderRegistry::instance();
  QStringList myPathList( pr->libraryDirectory().path() );

  QgsSettings settings;
  QString myPaths = settings.value( QStringLiteral( "plugins/searchPathsForPlugins" ), "" ).toString();
  if ( !myPaths.isEmpty() )
  {
    myPathList.append( myPaths.split( '|' ) );
  }

  for ( int j = 0; j < myPathList.size(); ++j )
  {
    QString myPluginDir = myPathList.at( j );
    QDir pluginDir( myPluginDir, sharedLibExtension, QDir::Name | QDir::IgnoreCase, QDir::Files | QDir::NoSymLinks );

    if ( pluginDir.count() == 0 )
    {
      QMessageBox::information( this, tr( "No Plugins" ), tr( "No QGIS plugins found in %1" ).arg( myPluginDir ) );
      return;
    }

    for ( uint i = 0; i < pluginDir.count(); i++ )
    {
      QString lib = QStringLiteral( "%1/%2" ).arg( myPluginDir, pluginDir[i] );

#ifdef TESTLIB
      // This doesn't work on windows and causes problems with plugins
      // on OS X (the code doesn't cause a problem but including dlfcn.h
      // renders plugins unloadable)
#if !defined(Q_OS_WIN) && !defined(Q_OS_MACX)
      // test code to help debug loading problems
      // This doesn't work on windows and causes problems with plugins
      // on OS X (the code doesn't cause a problem but including dlfcn.h
      // renders plugins unloadable)

      //void *handle = dlopen( (const char *) lib, RTLD_LAZY);
      void *handle = dlopen( lib.toLocal8Bit().data(), RTLD_LAZY | RTLD_GLOBAL );
      if ( !handle )
      {
        QgsDebugMsg( "Error in dlopen: " );
        QgsDebugMsg( dlerror() );
      }
      else
      {
        QgsDebugMsg( "dlopen succeeded for " + lib );
        dlclose( handle );
      }
#endif //#ifndef Q_OS_WIN && Q_OS_MACX
#endif //#ifdef TESTLIB

      QgsDebugMsg( "Examining: " + lib );
      QLibrary *myLib = new QLibrary( lib );
      bool loaded = myLib->load();
      if ( !loaded )
      {
        QgsDebugMsg( QString( "Failed to load: %1 (%2)" ).arg( myLib->fileName(), myLib->errorString() ) );
        delete myLib;
        continue;
      }

      QgsDebugMsg( "Loaded library: " + myLib->fileName() );

      // Don't bother with libraries that are providers
      //if(!myLib->resolve( "isProvider" ) )

      //MH: Replaced to allow for plugins that are linked to providers
      //type is only used in non-provider plugins
      if ( !myLib->resolve( "type" ) )
      {
        delete myLib;
        continue;
      }

      // resolve the metadata from plugin
      name_t *pName = ( name_t * ) cast_to_fptr( myLib->resolve( "name" ) );
      description_t *pDesc = ( description_t * ) cast_to_fptr( myLib->resolve( "description" ) );
      category_t *pCat = ( category_t * ) cast_to_fptr( myLib->resolve( "category" ) );
      version_t *pVersion = ( version_t * ) cast_to_fptr( myLib->resolve( "version" ) );
      icon_t *pIcon = ( icon_t * ) cast_to_fptr( myLib->resolve( "icon" ) );
      experimental_t *pExperimental = ( experimental_t * ) cast_to_fptr( myLib->resolve( "experimental" ) );

      // show the values (or lack of) for each function
      if ( pName )
      {
        QgsDebugMsg( "Plugin name: " + pName() );
      }
      else
      {
        QgsDebugMsg( "Plugin name not returned when queried" );
      }
      if ( pDesc )
      {
        QgsDebugMsg( "Plugin description: " + pDesc() );
      }
      else
      {
        QgsDebugMsg( "Plugin description not returned when queried" );
      }
      if ( pCat )
      {
        QgsDebugMsg( "Plugin category: " + pCat() );
      }
      else
      {
        QgsDebugMsg( "Plugin category not returned when queried" );
      }
      if ( pVersion )
      {
        QgsDebugMsg( "Plugin version: " + pVersion() );
      }
      else
      {
        QgsDebugMsg( "Plugin version not returned when queried" );
      }
      if ( pIcon )
      {
        QgsDebugMsg( "Plugin icon: " + pIcon() );
      }

      if ( !pName || !pDesc || !pVersion )
      {
        QgsDebugMsg( "Failed to get name, description, or type for " + myLib->fileName() );
        delete myLib;
        continue;
      }

      // Add "cpp:" prefix in case of two: Python and C++ plugins with the same name
      QString baseName = "cpp:" + QFileInfo( lib ).baseName();

      QMap<QString, QString> metadata;
      metadata[QStringLiteral( "id" )] = baseName;
      metadata[QStringLiteral( "name" )] = pName();
      metadata[QStringLiteral( "description" )] = pDesc();
      metadata[QStringLiteral( "category" )] = ( pCat ? pCat() : tr( "Plugins" ) );
      metadata[QStringLiteral( "version_installed" )] = pVersion();
      metadata[QStringLiteral( "icon" )] = ( pIcon ? pIcon() : QString() );
      metadata[QStringLiteral( "library" )] = myLib->fileName();
      metadata[QStringLiteral( "pythonic" )] = QStringLiteral( "false" );
      metadata[QStringLiteral( "installed" )] = QStringLiteral( "true" );
      metadata[QStringLiteral( "readonly" )] = QStringLiteral( "true" );
      metadata[QStringLiteral( "status" )] = QStringLiteral( "orphan" );
      metadata[QStringLiteral( "experimental" )] = ( pExperimental ? pExperimental() : QString() );
      mPlugins.insert( baseName, metadata );

      delete myLib;
    }
  }
}



QStandardItem *QgsPluginManager::createSpacerItem( const QString &text, const QString &value )
{
  QStandardItem *mySpacerltem = new QStandardItem( text );
  mySpacerltem->setData( value, PLUGIN_STATUS_ROLE );
  mySpacerltem->setData( "status", SPACER_ROLE );
  mySpacerltem->setEnabled( false );
  mySpacerltem->setEditable( false );
  QFont font = mySpacerltem->font();
  font.setBold( true );
  mySpacerltem->setFont( font );
  mySpacerltem->setTextAlignment( Qt::AlignHCenter );
  return mySpacerltem;
}



void QgsPluginManager::reloadModelData()
{
  mModelPlugins->clear();

  if ( !mCurrentlyDisplayedPlugin.isEmpty() )
  {
    wvDetails->setHtml( QLatin1String( "" ) );
    buttonInstall->setEnabled( false );
    buttonUninstall->setEnabled( false );
  }

  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( ! it->value( QStringLiteral( "id" ) ).isEmpty() )
    {
      QString baseName = it->value( QStringLiteral( "id" ) );
      QString pluginName = it->value( QStringLiteral( "name" ) );
      QString description = it->value( QStringLiteral( "description" ) );
      QString author = it->value( QStringLiteral( "author_name" ) );
      QString iconPath = it->value( QStringLiteral( "icon" ) );
      QString status = it->value( QStringLiteral( "status" ) );
      QString error = it->value( QStringLiteral( "error" ) );

      QStandardItem *mypDetailItem = new QStandardItem( pluginName.left( 32 ) );

      mypDetailItem->setData( baseName, PLUGIN_BASE_NAME_ROLE );
      mypDetailItem->setData( status, PLUGIN_STATUS_ROLE );
      mypDetailItem->setData( error, PLUGIN_ERROR_ROLE );
      mypDetailItem->setData( description, PLUGIN_DESCRIPTION_ROLE );
      mypDetailItem->setData( author, PLUGIN_AUTHOR_ROLE );
      mypDetailItem->setData( it->value( QStringLiteral( "tags" ) ), PLUGIN_TAGS_ROLE );
      mypDetailItem->setData( it->value( QStringLiteral( "downloads" ) ).rightJustified( 10, '0' ), PLUGIN_DOWNLOADS_ROLE );
      mypDetailItem->setData( it->value( QStringLiteral( "zip_repository" ) ), PLUGIN_REPOSITORY_ROLE );
      mypDetailItem->setData( it->value( QStringLiteral( "average_vote" ) ), PLUGIN_VOTE_ROLE );
      mypDetailItem->setData( it->value( QStringLiteral( "trusted" ) ), PLUGIN_TRUSTED_ROLE );

      if ( QFileInfo( iconPath ).isFile() )
      {
        mypDetailItem->setData( QPixmap( iconPath ), Qt::DecorationRole );
      }
      else
      {
        mypDetailItem->setData( QPixmap( QgsApplication::defaultThemePath() + "/propertyicons/plugin.svg" ), Qt::DecorationRole );
      }

      mypDetailItem->setEditable( false );

      // Set checkable if the plugin is installed and not disabled due to incompatibility.
      // Broken plugins are checkable to to allow disabling them
      mypDetailItem->setCheckable( it->value( QStringLiteral( "installed" ) ) == QLatin1String( "true" ) && it->value( QStringLiteral( "error" ) ) != QLatin1String( "incompatible" ) );

      // Set ckeckState depending on the plugin is loaded or not.
      // Initially mark all unchecked, then overwrite state of loaded ones with checked.
      // Only do it with installed plugins, not not initialize checkboxes of not installed plugins at all.
      if ( it->value( QStringLiteral( "installed" ) ) == QLatin1String( "true" ) )
      {
        mypDetailItem->setCheckState( Qt::Unchecked );
      }

      if ( isPluginEnabled( it->value( QStringLiteral( "id" ) ) ) )
      {
        mypDetailItem->setCheckState( Qt::Checked );
      }

      // Add items to model
      mModelPlugins->appendRow( mypDetailItem );

      // Repaint the details view if the currently displayed data are changed.
      if ( baseName == mCurrentlyDisplayedPlugin )
      {
        showPluginDetails( mypDetailItem );
      }
    }
  }

#ifdef WITH_BINDINGS
  // Add spacers for sort by status
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    // TODO: implement better sort method instead of these dummy -Z statuses
    mModelPlugins->appendRow( createSpacerItem( tr( "Only locally available", "category: plugins that are only locally available" ), QStringLiteral( "orphanZ" ) ) );
    if ( hasReinstallablePlugins() )
      mModelPlugins->appendRow( createSpacerItem( tr( "Reinstallable", "category: plugins that are installed and available" ), QStringLiteral( "installedZ" ) ) );
    if ( hasUpgradeablePlugins() )
      mModelPlugins->appendRow( createSpacerItem( tr( "Upgradeable", "category: plugins that are installed and there is a newer version available" ), QStringLiteral( "upgradeableZ" ) ) );
    if ( hasNewerPlugins() )
      mModelPlugins->appendRow( createSpacerItem( tr( "Downgradeable", "category: plugins that are installed and there is an OLDER version available" ), QStringLiteral( "newerZ" ) ) );
    if ( hasAvailablePlugins() )
      mModelPlugins->appendRow( createSpacerItem( tr( "Installable", "category: plugins that are available for installation" ), QStringLiteral( "not installedZ" ) ) );
  }
#endif

  updateWindowTitle();

  buttonUpgradeAll->setEnabled( hasUpgradeablePlugins() );

  // Disable tabs that are empty because of no suitable plugins in the model.
  mOptionsListWidget->item( PLUGMAN_TAB_NOT_INSTALLED )->setHidden( ! hasAvailablePlugins() );
  mOptionsListWidget->item( PLUGMAN_TAB_UPGRADEABLE )->setHidden( ! hasUpgradeablePlugins() );
  mOptionsListWidget->item( PLUGMAN_TAB_NEW )->setHidden( ! hasNewPlugins() );
  mOptionsListWidget->item( PLUGMAN_TAB_INVALID )->setHidden( ! hasInvalidPlugins() );
}



void QgsPluginManager::pluginItemChanged( QStandardItem *item )
{
  QString id = item->data( PLUGIN_BASE_NAME_ROLE ).toString();

  if ( item->checkState() )
  {
    if ( mPluginsAreEnabled && ! isPluginEnabled( id ) )
    {
      QgsDebugMsg( " Loading plugin: " + id );
      loadPlugin( id );
    }
    else
    {
      // only enable the plugin, as we're in --noplugins mode
      QgsDebugMsg( " Enabling plugin: " + id );
      savePluginState( id, true );
    }
  }
  else if ( ! item->checkState() )
  {
    QgsDebugMsg( " Unloading plugin: " + id );
    unloadPlugin( id );
  }
}



void QgsPluginManager::showPluginDetails( QStandardItem *item )
{
  const QMap<QString, QString> *metadata = pluginMetadata( item->data( PLUGIN_BASE_NAME_ROLE ).toString() );

  if ( !metadata )
  {
    return;
  }

  QString html = QLatin1String( "" );
  html += "<style>"
          "  body, table {"
          "    padding:0px;"
          "    margin:0px;"
          "    font-family:verdana;"
          "    font-size: 10pt;"
          "  }"
          "  div#votes {"
          "    width:360px;"
          "    margin-left:98px;"
          "    padding-top:3px;"
          "  }"
          "</style>";

  if ( !metadata->value( QStringLiteral( "plugin_id" ) ).isEmpty() )
  {
#ifdef WITH_QTWEBKIT
    html += QString(
              "<style>"
              "  div#stars_bg {"
              "    background-image: url('qrc:/images/themes/default/stars_empty.png');"
              "    width:92px;"
              "    height:16px;"
              "  }"
              "  div#stars {"
              "    background-image: url('qrc:/images/themes/default/stars_full.png');"
              "    width:%1px;"
              "    height:16px;"
              "  }"
              "</style>" ).arg( metadata->value( QStringLiteral( "average_vote" ) ).toFloat() / 5 * 92 );
    html += QString(
              "<script>"
              "  var plugin_id=%1;"
              "  var vote=0;"
              "  function ready()"
              "  {"
              "    document.getElementById('stars_bg').onmouseover=save_vote;"
              "    document.getElementById('stars_bg').onmouseout=restore_vote;"
              "    document.getElementById('stars_bg').onmousemove=change_vote;"
              "    document.getElementById('stars_bg').onclick=send_vote;"
              "  };"
              "    "
              "  function save_vote(e)"
              "  {"
              "    vote = document.getElementById('stars').style.width"
              "  }"
              "   "
              "  function restore_vote(e)"
              "  {"
              "    document.getElementById('stars').style.width = vote;"
              "  }"
              "   "
              "  function change_vote(e)"
              "  {"
              "    var length = e.x - document.getElementById('stars').getBoundingClientRect().left;"
              "    max = document.getElementById('stars_bg').getBoundingClientRect().right;"
              "    if ( length <= max ) document.getElementById('stars').style.width = length + 'px';"
              "  }"
              "   "
              "  function send_vote(e)"
              "  {"
              "    save_vote();"
              "    result = Number(vote.replace('px',''));"
              "    if (!result) return;"
              "    result = Math.floor(result/92*5)+1;"
              "    document.getElementById('send_vote_trigger').href='rpc2://plugin.vote/'+plugin_id+'/'+result;"
              "    ev=document.createEvent('MouseEvents');"
              "    ev.initEvent('click', false, true);"
              "    document.getElementById('send_vote_trigger').dispatchEvent(ev);"
              "  }"
              "</script>" ).arg( metadata->value( QStringLiteral( "plugin_id" ) ) );
#else
    voteRating->show();
    voteLabel->show();
    voteSlider->show();
    voteSubmit->show();
    QgsDebugMsg( QString( "vote slider:%1" ).arg( std::round( metadata->value( "average_vote" ).toFloat() ) ) );
    voteSlider->setValue( std::round( metadata->value( "average_vote" ).toFloat() ) );
    mCurrentPluginId = metadata->value( "plugin_id" ).toInt();
  }
  else
  {
    voteRating->hide();
    voteLabel->hide();
    voteSlider->hide();
    voteSubmit->hide();
    mCurrentPluginId = -1;
#endif
  }

#ifdef WITH_QTWEBKIT
  html += QLatin1String( "<body onload='ready()'>" );
#else
  html += "<body>";
#endif


  // First prepare message box(es)
  if ( ! metadata->value( QStringLiteral( "error" ) ).isEmpty() )
  {
    QString errorMsg;
    if ( metadata->value( QStringLiteral( "error" ) ) == QLatin1String( "incompatible" ) )
    {
      errorMsg = QStringLiteral( "<b>%1</b><br/>%2" ).arg( tr( "This plugin is incompatible with this version of QGIS" ), tr( "Plugin designed for QGIS %1", "compatible QGIS version(s)" ).arg( metadata->value( QStringLiteral( "error_details" ) ) ) );
    }
    else if ( metadata->value( QStringLiteral( "error" ) ) == QLatin1String( "dependent" ) )
    {
      errorMsg = QStringLiteral( "<b>%1:</b><br/>%2" ).arg( tr( "This plugin requires a missing module" ), metadata->value( QStringLiteral( "error_details" ) ) );
    }
    else
    {
      errorMsg = QStringLiteral( "<b>%1</b><br/>%2" ).arg( tr( "This plugin is broken" ), metadata->value( QStringLiteral( "error_details" ) ) );
    }
    html += QString( "<table bgcolor=\"#FFFF88\" cellspacing=\"2\" cellpadding=\"6\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#CC0000\">%1</td></tr>"
                     "</table>" ).arg( errorMsg );
  }

  if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "upgradeable" ) )
  {
    html += QString( "<table bgcolor=\"#FFFFAA\" cellspacing=\"2\" cellpadding=\"6\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#880000\"><b>%1</b></td></tr>"
                     "</table>" ).arg( tr( "There is a new version available" ) );
  }

  if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "new" ) )
  {
    html += QString( "<table bgcolor=\"#CCFFCC\" cellspacing=\"2\" cellpadding=\"6\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#008800\"><b>%1</b></td></tr>"
                     "</table>" ).arg( tr( "This is a new plugin" ) );
  }

  if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "newer" ) )
  {
    html += QString( "<table bgcolor=\"#FFFFCC\" cellspacing=\"2\" cellpadding=\"6\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#550000\"><b>%1</b></td></tr>"
                     "</table>" ).arg( tr( "Installed version of this plugin is higher than any version found in repository" ) );
  }

  if ( metadata->value( QStringLiteral( "experimental" ) ) == QLatin1String( "true" ) )
  {
    html += QString( "<table bgcolor=\"#EEEEBB\" cellspacing=\"2\" cellpadding=\"2\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#660000\">"
                     "    <img src=\"qrc:/images/themes/default/pluginExperimental.png\" width=\"32\"><b>%1</b>"
                     "  </td></tr>"
                     "</table>" ).arg( tr( "This plugin is experimental" ) );
  }

  if ( metadata->value( QStringLiteral( "deprecated" ) ) == QLatin1String( "true" ) )
  {
    html += QString( "<table bgcolor=\"#EEBBCC\" cellspacing=\"2\" cellpadding=\"2\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#660000\">"
                     "    <img src=\"qrc:/images/themes/default/pluginDeprecated.png\" width=\"32\"><b>%1</b>"
                     "  </td></tr>"
                     "</table>" ).arg( tr( "This plugin is deprecated" ) );
  }

  if ( metadata->value( QStringLiteral( "trusted" ) ) == QLatin1String( "true" ) )
  {
    html += QString( "<table bgcolor=\"#90EE90\" cellspacing=\"2\" cellpadding=\"2\" width=\"100%\">"
                     "  <tr><td width=\"100%\" style=\"color:#660000\">"
                     "    <img src=\"qrc:/images/themes/default/mIconSuccess.svg\" width=\"32\"><b>%1</b>"
                     "  </td></tr>"
                     "</table>" ).arg( tr( "This plugin is trusted" ) );
  }

  // Now the metadata

  html += QLatin1String( "<table cellspacing=\"4\" width=\"100%\"><tr><td>" );

  QString iconPath = metadata->value( QStringLiteral( "icon" ) );
  if ( QFileInfo( iconPath ).isFile() )
  {
    if ( iconPath.contains( QLatin1String( ":/" ) ) )
    {
      iconPath = "qrc" + iconPath;
    }
    else
    {
      iconPath = "file://" + iconPath;
    }
    html += QStringLiteral( "<img src=\"%1\" style=\"float:right;max-width:64px;max-height:64px;\">" ).arg( iconPath );
  }

  html += QStringLiteral( "<h1>%1</h1>" ).arg( metadata->value( QStringLiteral( "name" ) ) );

  html += QStringLiteral( "<h3>%1</h3>" ).arg( metadata->value( QStringLiteral( "description" ) ) );

  if ( ! metadata->value( QStringLiteral( "about" ) ).isEmpty() )
  {
    QString about = metadata->value( QStringLiteral( "about" ) );
    html += about.replace( '\n', QLatin1String( "<br/>" ) );
  }

  html += QLatin1String( "<br/><br/>" );

  QString votes;
#ifndef WITH_QTWEBKIT
  votes += tr( "Average rating %1" ).arg( metadata->value( "average_vote" ).toFloat(), 0, 'f', 1 );
#endif
  if ( ! metadata->value( QStringLiteral( "rating_votes" ) ).isEmpty() )
  {
    if ( !votes.isEmpty() )
      votes += QLatin1String( ", " );
    votes += tr( "%1 rating vote(s)" ).arg( metadata->value( QStringLiteral( "rating_votes" ) ) );
  }
  if ( ! metadata->value( QStringLiteral( "downloads" ) ).isEmpty() )
  {
    if ( !votes.isEmpty() )
      votes += QLatin1String( ", " );
    votes += tr( "%1 downloads" ).arg( metadata->value( QStringLiteral( "downloads" ) ) );
  }

#ifdef WITH_QTWEBKIT
  html += QLatin1String( "<div id='stars_bg'/><div id='stars'/>" );
  html += QLatin1String( "<div id='votes'>" );
  html += votes;
  html += QLatin1String( "</div>" );
  html += QLatin1String( "<div><a id='send_vote_trigger'/></div>" );
#else
  voteRating->setText( votes );
#endif
  html += QLatin1String( "</td></tr><tr><td>" );
  html += QLatin1String( "<br/>" );

  if ( ! metadata->value( QStringLiteral( "category" ) ).isEmpty() )
  {
    html += QStringLiteral( "%1: %2 <br/>" ).arg( tr( "Category" ), metadata->value( QStringLiteral( "category" ) ) );
  }
  if ( ! metadata->value( QStringLiteral( "tags" ) ).isEmpty() )
  {
    html += QStringLiteral( "%1: %2 <br/>" ).arg( tr( "Tags" ), metadata->value( QStringLiteral( "tags" ) ) );
  }
  if ( ! metadata->value( QStringLiteral( "homepage" ) ).isEmpty() || ! metadata->value( QStringLiteral( "tracker" ) ).isEmpty() || ! metadata->value( QStringLiteral( "code_repository" ) ).isEmpty() )
  {
    html += QStringLiteral( "%1: " ).arg( tr( "More info" ) );
    if ( ! metadata->value( QStringLiteral( "homepage" ) ).isEmpty() )
    {
      html += QStringLiteral( "<a href='%1'>%2</a> &nbsp; " ).arg( metadata->value( QStringLiteral( "homepage" ) ), tr( "homepage" ) );
    }
    if ( ! metadata->value( QStringLiteral( "tracker" ) ).isEmpty() )
    {
      html += QStringLiteral( "<a href='%1'>%2</a> &nbsp; " ).arg( metadata->value( QStringLiteral( "tracker" ) ), tr( "bug_tracker" ) );
    }
    if ( ! metadata->value( QStringLiteral( "code_repository" ) ).isEmpty() )
    {
      html += QStringLiteral( "<a href='%1'>%2</a>" ).arg( metadata->value( QStringLiteral( "code_repository" ) ), tr( "code_repository" ) );
    }
    html += QLatin1String( "<br/>" );
  }
  html += QLatin1String( "<br/>" );

  if ( ! metadata->value( QStringLiteral( "author_email" ) ).isEmpty() )
  {
    html += QStringLiteral( "%1: <a href='mailto:%2'>%3</a>" ).arg( tr( "Author" ), metadata->value( QStringLiteral( "author_email" ) ), metadata->value( QStringLiteral( "author_name" ) ) );
    html += QLatin1String( "<br/><br/>" );
  }
  else if ( ! metadata->value( QStringLiteral( "author_name" ) ).isEmpty() )
  {
    html += QStringLiteral( "%1: %2" ).arg( tr( "Author" ), metadata->value( QStringLiteral( "author_name" ) ) );
    html += QLatin1String( "<br/><br/>" );
  }

  if ( ! metadata->value( QStringLiteral( "version_installed" ) ).isEmpty() )
  {
    QString ver = metadata->value( QStringLiteral( "version_installed" ) );
    if ( ver == QLatin1String( "-1" ) ) ver = '?';
    html += tr( "Installed version: %1 (in %2)<br/>" ).arg( ver, metadata->value( QStringLiteral( "library" ) ) );
  }
  if ( ! metadata->value( QStringLiteral( "version_available" ) ).isEmpty() )
  {
    html += tr( "Available version: %1 (in %2)<br/>" ).arg( metadata->value( QStringLiteral( "version_available" ) ), metadata->value( QStringLiteral( "zip_repository" ) ) );
  }

  if ( ! metadata->value( QStringLiteral( "changelog" ) ).isEmpty() )
  {
    html += QLatin1String( "<br/>" );
    QString changelog = tr( "changelog:<br/>%1 <br/>" ).arg( metadata->value( QStringLiteral( "changelog" ) ) );
    html += changelog.replace( '\n', QLatin1String( "<br/>" ) );
  }

  html += QLatin1String( "</td></tr></table>" );

  html += QLatin1String( "</body>" );

  wvDetails->setHtml( html );

  // Set buttonInstall text (and sometimes focus)
  buttonInstall->setDefault( false );
  if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "upgradeable" ) )
  {
    buttonInstall->setText( tr( "Upgrade plugin" ) );
    buttonInstall->setDefault( true );
  }
  else if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "newer" ) )
  {
    buttonInstall->setText( tr( "Downgrade plugin" ) );
  }
  else if ( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "not installed" ) || metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "new" ) )
  {
    buttonInstall->setText( tr( "Install plugin" ) );
  }
  else
  {
    // Default (will be grayed out if not available for reinstallation)
    buttonInstall->setText( tr( "Reinstall plugin" ) );
  }

  // Enable/disable buttons
  buttonInstall->setEnabled( metadata->value( QStringLiteral( "pythonic" ) ).toUpper() == QLatin1String( "TRUE" ) && metadata->value( QStringLiteral( "status" ) ) != QLatin1String( "orphan" ) );
  buttonUninstall->setEnabled( metadata->value( QStringLiteral( "pythonic" ) ).toUpper() == QLatin1String( "TRUE" ) && metadata->value( QStringLiteral( "readonly" ) ) != QLatin1String( "true" ) && metadata->value( QStringLiteral( "status" ) ) != QLatin1String( "not installed" ) && metadata->value( QStringLiteral( "status" ) ) != QLatin1String( "new" ) );
  buttonUninstall->setHidden( metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "not installed" ) || metadata->value( QStringLiteral( "status" ) ) == QLatin1String( "new" ) );

  // Store the id of the currently displayed plugin
  mCurrentlyDisplayedPlugin = metadata->value( QStringLiteral( "id" ) );
}



void QgsPluginManager::selectTabItem( int idx )
{
  mOptionsListWidget->setCurrentRow( idx );
}



void QgsPluginManager::clearPythonPluginMetadata()
{
  for ( QMap<QString, QMap<QString, QString> >::iterator it = mPlugins.begin();
        it != mPlugins.end();
      )
  {
    if ( it->value( QStringLiteral( "pythonic" ) ) == QLatin1String( "true" ) )
    {
      it = mPlugins.erase( it );
    }
    else
    {
      ++it;
    }
  }
}



void QgsPluginManager::addPluginMetadata( const QString &key, const QMap<QString, QString> &metadata )
{
  mPlugins.insert( key, metadata );
}



const QMap<QString, QString> *QgsPluginManager::pluginMetadata( const QString &key ) const
{
  QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.find( key );
  if ( it != mPlugins.end() )
  {
    return &it.value();
  }
  return nullptr;
}

void QgsPluginManager::clearRepositoryList()
{
  treeRepositories->clear();
  buttonRefreshRepos->setEnabled( false );
  buttonEditRep->setEnabled( false );
  buttonDeleteRep->setEnabled( false );
  Q_FOREACH ( QAction *action, treeRepositories->actions() )
  {
    treeRepositories->removeAction( action );
  }
}

void QgsPluginManager::addToRepositoryList( const QMap<QString, QString> &repository )
{
  // If it's the second item on the tree, change the button text to plural form and add the filter context menu
  if ( buttonRefreshRepos->isEnabled() && treeRepositories->actions().count() < 1 )
  {
    buttonRefreshRepos->setText( tr( "Reload all repositories" ) );
    QAction *actionEnableThisRepositoryOnly = new QAction( tr( "Only show plugins from selected repository" ), treeRepositories );
    treeRepositories->addAction( actionEnableThisRepositoryOnly );
    connect( actionEnableThisRepositoryOnly, &QAction::triggered, this, &QgsPluginManager::setRepositoryFilter );
    treeRepositories->setContextMenuPolicy( Qt::ActionsContextMenu );
    QAction *actionClearFilter = new QAction( tr( "Clear filter" ), treeRepositories );
    actionClearFilter->setEnabled( repository.value( QStringLiteral( "inspection_filter" ) ) == QLatin1String( "true" ) );
    treeRepositories->addAction( actionClearFilter );
    connect( actionClearFilter, &QAction::triggered, this, &QgsPluginManager::clearRepositoryFilter );
  }

  QString key = repository.value( QStringLiteral( "name" ) );
  if ( ! key.isEmpty() )
  {
    QTreeWidgetItem *a = new QTreeWidgetItem( treeRepositories );
    a->setText( 1, key );
    a->setText( 2, repository.value( QStringLiteral( "url" ) ) );
    if ( repository.value( QStringLiteral( "enabled" ) ) == QLatin1String( "true" ) && repository.value( QStringLiteral( "valid" ) ) == QLatin1String( "true" ) )
    {
      if ( repository.value( QStringLiteral( "state" ) ) == QLatin1String( "2" ) )
      {
        a->setText( 0, tr( "connected" ) );
        a->setIcon( 0, QIcon( ":/images/themes/default/repositoryConnected.png" ) );
        a->setToolTip( 0, tr( "The repository is connected" ) );
      }
      else
      {
        a->setText( 0, tr( "unavailable" ) );
        a->setIcon( 0, QIcon( ":/images/themes/default/repositoryUnavailable.png" ) );
        a->setToolTip( 0, tr( "The repository is enabled, but unavailable" ) );
      }
    }
    else
    {
      a->setText( 0, tr( "disabled" ) );
      a->setIcon( 0, QIcon( ":/images/themes/default/repositoryDisabled.png" ) );
      if ( repository.value( QStringLiteral( "valid" ) ) == QLatin1String( "true" ) )
      {
        a->setToolTip( 0, tr( "The repository is disabled" ) );
      }
      else
      {
        a->setToolTip( 0, tr( "The repository is blocked due to incompatibility with your QGIS version" ) );
      }

      QBrush grayBrush = QBrush( QColor( Qt::gray ) );
      a->setForeground( 0, grayBrush );
      a->setForeground( 1, grayBrush );
      a->setForeground( 2, grayBrush );
    }
  }
  treeRepositories->resizeColumnToContents( 0 );
  treeRepositories->resizeColumnToContents( 1 );
  treeRepositories->resizeColumnToContents( 2 );
  treeRepositories->sortItems( 1, Qt::AscendingOrder );
  buttonRefreshRepos->setEnabled( true );
}



// SLOTS ///////////////////////////////////////////////////////////////////


// "Close" button clicked
void QgsPluginManager::reject()
{
#ifdef WITH_BINDINGS
  if ( mPythonUtils && mPythonUtils->isEnabled() )
  {
    // get the QgsSettings group from the installer
    QString settingsGroup;
    QgsPythonRunner::eval( QStringLiteral( "pyplugin_installer.instance().exportSettingsGroup()" ), settingsGroup );
    QgsSettings settings;
    settings.setValue( settingsGroup + "/checkOnStart", QVariant( ckbCheckUpdates->isChecked() ) );
    settings.setValue( settingsGroup + "/checkOnStartInterval", QVariant( mCheckingOnStartIntervals.value( comboInterval->currentIndex() ) ) );
    QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().onManagerClose()" ) );
  }
#endif
  done( 1 );
}



void QgsPluginManager::setCurrentTab( int idx )
{
  if ( idx == ( mOptionsListWidget->count() - 1 ) )
  {
    QgsDebugMsg( "Switching current tab to Settings" );
    mOptionsStackedWidget->setCurrentIndex( 1 );
  }
  else
  {
    QgsDebugMsg( "Switching current tab to Plugins" );
    mOptionsStackedWidget->setCurrentIndex( 0 );

    QStringList acceptedStatuses;
    QString tabTitle;
    switch ( idx )
    {
      case PLUGMAN_TAB_ALL:
        // all (statuses ends with Z are for spacers to always sort properly)
        acceptedStatuses << QStringLiteral( "installed" ) << QStringLiteral( "not installed" ) << QStringLiteral( "new" ) << QStringLiteral( "orphan" ) << QStringLiteral( "newer" ) << QStringLiteral( "upgradeable" ) << QStringLiteral( "not installedZ" ) << QStringLiteral( "installedZ" ) << QStringLiteral( "upgradeableZ" ) << QStringLiteral( "orphanZ" ) << QStringLiteral( "newerZZ" ) << QLatin1String( "" );
        tabTitle = QStringLiteral( "all_plugins" );
        break;
      case PLUGMAN_TAB_INSTALLED:
        // installed (statuses ends with Z are for spacers to always sort properly)
        acceptedStatuses << QStringLiteral( "installed" ) << QStringLiteral( "orphan" ) << QStringLiteral( "newer" ) << QStringLiteral( "upgradeable" ) << QStringLiteral( "installedZ" ) << QStringLiteral( "upgradeableZ" ) << QStringLiteral( "orphanZ" ) << QStringLiteral( "newerZZ" ) << QLatin1String( "" );
        tabTitle = QStringLiteral( "installed_plugins" );
        break;
      case PLUGMAN_TAB_NOT_INSTALLED:
        // not installed (get more)
        acceptedStatuses << QStringLiteral( "not installed" ) << QStringLiteral( "new" );
        tabTitle = QStringLiteral( "not_installed_plugins" );
        break;
      case PLUGMAN_TAB_UPGRADEABLE:
        // upgradeable
        acceptedStatuses << QStringLiteral( "upgradeable" );
        tabTitle = QStringLiteral( "upgradeable_plugins" );
        break;
      case PLUGMAN_TAB_NEW:
        // new
        acceptedStatuses << QStringLiteral( "new" );
        tabTitle = QStringLiteral( "new_plugins" );
        break;
      case PLUGMAN_TAB_INVALID:
        // invalid
        acceptedStatuses << QStringLiteral( "invalid" );
        tabTitle = QStringLiteral( "invalid_plugins" );
        break;
    }
    mModelProxy->setAcceptedStatuses( acceptedStatuses );

    // load tab description HTML to the detail browser
    QString tabInfoHTML = QLatin1String( "" );
    QMap<QString, QString>::const_iterator it = mTabDescriptions.constFind( tabTitle );
    if ( it != mTabDescriptions.constEnd() )
    {
      tabInfoHTML += "<style>"
                     "  body, p {"
                     "      margin: 2px;"
                     "      font-family: verdana;"
                     "      font-size: 10pt;"
                     "  }"
                     "</style>";
      // tabInfoHTML += "<style>" + QgsApplication::reportStyleSheet() + "</style>";
      tabInfoHTML += it.value();
    }
    wvDetails->setHtml( tabInfoHTML );

    // disable buttons
    buttonInstall->setEnabled( false );
    buttonUninstall->setEnabled( false );
  }

  updateWindowTitle();
}



void QgsPluginManager::currentPluginChanged( const QModelIndex &index )
{
  if ( index.column() == 0 )
  {
    // Do exactly the same as if a plugin was clicked
    on_vwPlugins_clicked( index );
  }
}



void QgsPluginManager::on_vwPlugins_clicked( const QModelIndex &index )
{
  if ( index.column() == 0 )
  {
    // If the model has been filtered, the index row in the proxy won't match the index row in the underlying model
    // so we need to jump through this little hoop to get the correct item
    QModelIndex realIndex = mModelProxy->mapToSource( index );
    QStandardItem *mypItem = mModelPlugins->itemFromIndex( realIndex );
    if ( !mypItem->isEnabled() )
    {
      //The item is inactive (uncompatible or broken plugin), so it can't be selected. Display it's data anyway.
      vwPlugins->clearSelection();
    }
    // Display details in any case: selection changed, inactive button clicked,
    // or previously selected plugin clicked (while details view contains the welcome message for a category)
    showPluginDetails( mypItem );
  }
}



void QgsPluginManager::on_vwPlugins_doubleClicked( const QModelIndex &index )
{
  if ( index.column() == 0 )
  {
    // If the model has been filtered, the index row in the proxy won't match the index row in the underlying model
    // so we need to jump through this little hoop to get the correct item
    QModelIndex realIndex = mModelProxy->mapToSource( index );
    QStandardItem *mypItem = mModelPlugins->itemFromIndex( realIndex );
    if ( mypItem->isCheckable() )
    {
      if ( mypItem->checkState() == Qt::Checked )
      {
        mypItem->setCheckState( Qt::Unchecked );
      }
      else
      {
        mypItem->setCheckState( Qt::Checked );
      }
    }
  }
}

#ifndef WITH_QTWEBKIT
void QgsPluginManager::submitVote()
{
  if ( mCurrentPluginId < 0 )
    return;

  sendVote( mCurrentPluginId, voteSlider->value() );
}
#endif

void QgsPluginManager::sendVote( int pluginId, int vote )
{
  QString response;
  QgsPythonRunner::eval( QStringLiteral( "pyplugin_installer.instance().sendVote('%1', '%2')" ).arg( pluginId ).arg( vote ), response );
  if ( response == QLatin1String( "True" ) )
  {
    pushMessage( tr( "Vote sent successfully" ), QgsMessageBar::INFO );
  }
  else
  {
    pushMessage( tr( "Sending vote to the plugin repository failed." ), QgsMessageBar::WARNING );
  }
}

void QgsPluginManager::on_wvDetails_linkClicked( const QUrl &url )
{
  if ( url.scheme() == QLatin1String( "rpc2" ) )
  {
    if ( url.host() == QLatin1String( "plugin.vote" ) )
    {
      QString params = url.path();
      sendVote( params.split( '/' )[1].toInt(), params.split( '/' )[2].toInt() );
    }
  }
  else
  {
    QDesktopServices::openUrl( url );
  }
}


void QgsPluginManager::on_leFilter_textChanged( QString text )
{
  if ( text.startsWith( QLatin1String( "tag:" ), Qt::CaseInsensitive ) )
  {
    text = text.remove( QStringLiteral( "tag:" ) );
    mModelProxy->setFilterRole( PLUGIN_TAGS_ROLE );
    QgsDebugMsg( "PluginManager TAG filter changed to :" + text );
  }
  else
  {
    mModelProxy->setFilterRole( 0 );
    QgsDebugMsg( "PluginManager filter changed to :" + text );
  }

  QRegExp::PatternSyntax mySyntax = QRegExp::PatternSyntax( QRegExp::RegExp );
  Qt::CaseSensitivity myCaseSensitivity = Qt::CaseInsensitive;
  QRegExp myRegExp( text, myCaseSensitivity, mySyntax );
  mModelProxy->setFilterRegExp( myRegExp );
}



void QgsPluginManager::on_buttonUpgradeAll_clicked()
{
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().upgradeAllUpgradeable()" ) );
}



void QgsPluginManager::on_buttonInstall_clicked()
{
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().installPlugin('%1')" ).arg( mCurrentlyDisplayedPlugin ) );
}



void QgsPluginManager::on_buttonUninstall_clicked()
{
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().uninstallPlugin('%1')" ).arg( mCurrentlyDisplayedPlugin ) );
}



void QgsPluginManager::on_treeRepositories_itemSelectionChanged()
{
  buttonEditRep->setEnabled( ! treeRepositories->selectedItems().isEmpty() );
  buttonDeleteRep->setEnabled( ! treeRepositories->selectedItems().isEmpty() );
}



void QgsPluginManager::on_treeRepositories_doubleClicked( const QModelIndex & )
{
  on_buttonEditRep_clicked();
}



void QgsPluginManager::setRepositoryFilter()
{
  QTreeWidgetItem *current = treeRepositories->currentItem();
  if ( current )
  {
    QString key = current->text( 1 );
    key = key.replace( '\'', QLatin1String( "\\\'" ) ).replace( '\"', QLatin1String( "\\\"" ) );
    QgsDebugMsg( "Disabling all repositories but selected: " + key );
    QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().setRepositoryInspectionFilter('%1')" ).arg( key ) );
  }
}



void QgsPluginManager::clearRepositoryFilter()
{
  QgsDebugMsg( "Enabling all repositories back" );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().setRepositoryInspectionFilter()" ) );
}



void QgsPluginManager::on_buttonRefreshRepos_clicked()
{
  QgsDebugMsg( "Refreshing repositories..." );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().reloadAndExportData()" ) );
}



void QgsPluginManager::on_buttonAddRep_clicked()
{
  QgsDebugMsg( "Adding repository connection..." );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().addRepository()" ) );
}



void QgsPluginManager::on_buttonEditRep_clicked()
{
  QTreeWidgetItem *current = treeRepositories->currentItem();
  if ( current )
  {
    QString key = current->text( 1 );
    key = key.replace( '\'', QLatin1String( "\\\'" ) ).replace( '\"', QLatin1String( "\\\"" ) );
    QgsDebugMsg( "Editing repository connection: " + key );
    QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().editRepository('%1')" ).arg( key ) );
  }
}



void QgsPluginManager::on_buttonDeleteRep_clicked()
{
  QTreeWidgetItem *current = treeRepositories->currentItem();
  if ( current )
  {
    QString key = current->text( 1 );
    key = key.replace( '\'', QLatin1String( "\\\'" ) ).replace( '\"', QLatin1String( "\\\"" ) );
    QgsDebugMsg( "Deleting repository connection: " + key );
    QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().deleteRepository('%1')" ).arg( key ) );
  }
}



void QgsPluginManager::on_ckbExperimental_toggled( bool state )
{
  QString settingsGroup;
  QgsPythonRunner::eval( QStringLiteral( "pyplugin_installer.instance().exportSettingsGroup()" ), settingsGroup );
  QgsSettings settings;
  settings.setValue( settingsGroup + "/allowExperimental", QVariant( state ) );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.installer_data.plugins.rebuild()" ) );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().exportPluginsToManager()" ) );
}

void QgsPluginManager::on_ckbDeprecated_toggled( bool state )
{
  QString settingsGroup;
  QgsPythonRunner::eval( QStringLiteral( "pyplugin_installer.instance().exportSettingsGroup()" ), settingsGroup );
  QgsSettings settings;
  settings.setValue( settingsGroup + "/allowDeprecated", QVariant( state ) );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.installer_data.plugins.rebuild()" ) );
  QgsPythonRunner::run( QStringLiteral( "pyplugin_installer.instance().exportPluginsToManager()" ) );
}


// PRIVATE METHODS ///////////////////////////////////////////////////////////////////


bool QgsPluginManager::isPluginEnabled( QString key )
{
  const QMap<QString, QString> *plugin = pluginMetadata( key );
  if ( plugin->isEmpty() )
  {
    // No such plugin in the metadata registry
    return false;
  }

  QgsSettings mySettings;
  if ( plugin->value( QStringLiteral( "pythonic" ) ) != QLatin1String( "true" ) )
  {
    // Trim "cpp:" prefix from cpp plugin id
    key = key.mid( 4 );
    return ( mySettings.value( "/Plugins/" + key, QVariant( false ) ).toBool() );
  }
  else
  {
    return ( plugin->value( QStringLiteral( "installed" ) ) == QLatin1String( "true" ) && mySettings.value( "/PythonPlugins/" + key, QVariant( false ) ).toBool() );
  }
}



bool QgsPluginManager::hasAvailablePlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( it->value( QStringLiteral( "status" ) ) == QLatin1String( "not installed" ) || it->value( QStringLiteral( "status" ) ) == QLatin1String( "new" ) )
    {
      return true;
    }
  }

  return false;
}



bool QgsPluginManager::hasReinstallablePlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    // plugins marked as "installed" are available for download (otherwise they are marked "orphans")
    if ( it->value( QStringLiteral( "status" ) ) == QLatin1String( "installed" ) )
    {
      return true;
    }
  }

  return false;
}



bool QgsPluginManager::hasUpgradeablePlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( it->value( QStringLiteral( "status" ) ) == QLatin1String( "upgradeable" ) )
    {
      return true;
    }
  }

  return false;
}



bool QgsPluginManager::hasNewPlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( it->value( QStringLiteral( "status" ) ) == QLatin1String( "new" ) )
    {
      return true;
    }
  }

  return false;
}



bool QgsPluginManager::hasNewerPlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( it->value( QStringLiteral( "status" ) ) == QLatin1String( "newer" ) )
    {
      return true;
    }
  }

  return false;
}



bool QgsPluginManager::hasInvalidPlugins()
{
  for ( QMap<QString, QMap<QString, QString> >::const_iterator it = mPlugins.constBegin();
        it != mPlugins.constEnd();
        ++it )
  {
    if ( ! it->value( QStringLiteral( "error" ) ).isEmpty() )
    {
      return true;
    }
  }

  return false;
}



void QgsPluginManager::updateWindowTitle()
{
  QListWidgetItem *curitem = mOptListWidget->currentItem();
  if ( curitem )
  {
    QString title = QStringLiteral( "%1 | %2" ).arg( tr( "Plugins" ), curitem->text() );
    if ( mOptionsListWidget->currentRow() < mOptionsListWidget->count() - 1 && mModelPlugins )
    {
      // if it's not the Settings tab, add the plugin count
      title += QStringLiteral( " (%3)" ).arg( mModelProxy->countWithCurrentStatus() );
    }
    setWindowTitle( title );
  }
  else
  {
    setWindowTitle( mDialogTitle );
  }
}



void QgsPluginManager::showEvent( QShowEvent *e )
{
  if ( mInit )
  {
    updateOptionsListVerticalTabs();
  }
  else
  {
    QTimer::singleShot( 0, this, SLOT( warnAboutMissingObjects() ) );
  }

  QDialog::showEvent( e );
}



void QgsPluginManager::pushMessage( const QString &text, QgsMessageBar::MessageLevel level, int duration )
{
  if ( duration == -1 )
  {
    duration = QgisApp::instance()->messageTimeout();
  }
  msgBar->pushMessage( text, level, duration );
}

void QgsPluginManager::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "plugins/plugins.html" ) );
}
