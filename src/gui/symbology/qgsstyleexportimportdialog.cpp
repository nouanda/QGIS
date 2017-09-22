/***************************************************************************
    qgsstyleexportimportdialog.cpp
    ---------------------
    begin                : Jan 2011
    copyright            : (C) 2011 by Alexander Bruy
    email                : alexander dot bruy at gmail dot com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsstyleexportimportdialog.h"
#include "ui_qgsstyleexportimportdialogbase.h"

#include "qgsstyle.h"
#include "qgssymbol.h"
#include "qgssymbollayerutils.h"
#include "qgscolorramp.h"
#include "qgslogger.h"
#include "qgsstylegroupselectiondialog.h"

#include <QInputDialog>
#include <QCloseEvent>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardItemModel>


QgsStyleExportImportDialog::QgsStyleExportImportDialog( QgsStyle *style, QWidget *parent, Mode mode )
  : QDialog( parent )
  , mDialogMode( mode )
  , mStyle( style )
{
  setupUi( this );

  // additional buttons
  QPushButton *pb = nullptr;
  pb = new QPushButton( tr( "Select all" ) );
  buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
  connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::selectAll );

  pb = new QPushButton( tr( "Clear selection" ) );
  buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
  connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::clearSelection );

  QStandardItemModel *model = new QStandardItemModel( listItems );
  listItems->setModel( model );
  connect( listItems->selectionModel(), &QItemSelectionModel::selectionChanged,
           this, &QgsStyleExportImportDialog::selectionChanged );

  mTempStyle = new QgsStyle();
  mTempStyle->createMemoryDatabase();

  // TODO validate
  mFileName = QLatin1String( "" );
  mProgressDlg = nullptr;
  mGroupSelectionDlg = nullptr;
  mTempFile = nullptr;
  mNetManager = new QNetworkAccessManager( this );
  mNetReply = nullptr;

  if ( mDialogMode == Import )
  {
    setWindowTitle( tr( "Import Symbol(s)" ) );
    // populate the import types
    importTypeCombo->addItem( tr( "file specified below" ), QVariant( "file" ) );
    // importTypeCombo->addItem( "official QGIS repo online", QVariant( "official" ) );
    importTypeCombo->addItem( tr( "URL specified below" ), QVariant( "url" ) );
    connect( importTypeCombo, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsStyleExportImportDialog::importTypeChanged );

    mSymbolTags->setText( "imported" );

    btnBrowse->setText( QStringLiteral( "Browse" ) );
    connect( btnBrowse, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::browse );

    label->setText( tr( "Select symbols to import" ) );
    buttonBox->button( QDialogButtonBox::Ok )->setText( tr( "Import" ) );
  }
  else
  {
    setWindowTitle( tr( "Export Symbol(s)" ) );
    // hide import specific controls when exporting
    btnBrowse->setHidden( true );
    fromLabel->setHidden( true );
    importTypeCombo->setHidden( true );
    locationLabel->setHidden( true );
    locationLineEdit->setHidden( true );

    mFavorite->setHidden( true );
    mIgnoreXMLTags->setHidden( true );

    pb = new QPushButton( tr( "Select by group" ) );
    buttonBox->addButton( pb, QDialogButtonBox::ActionRole );
    connect( pb, &QAbstractButton::clicked, this, &QgsStyleExportImportDialog::selectByGroup );
    tagLabel->setHidden( true );
    mSymbolTags->setHidden( true );
    tagHintLabel->setHidden( true );

    buttonBox->button( QDialogButtonBox::Ok )->setText( tr( "Export" ) );
    if ( !populateStyles( mStyle ) )
    {
      QApplication::postEvent( this, new QCloseEvent() );
    }

  }
  // use Ok button for starting import and export operations
  disconnect( buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept );
  connect( buttonBox, &QDialogButtonBox::accepted, this, &QgsStyleExportImportDialog::doExportImport );
  buttonBox->button( QDialogButtonBox::Ok )->setEnabled( false );
}

void QgsStyleExportImportDialog::doExportImport()
{
  QModelIndexList selection = listItems->selectionModel()->selectedIndexes();
  if ( selection.isEmpty() )
  {
    QMessageBox::warning( this, tr( "Export/import error" ),
                          tr( "You should select at least one symbol/color ramp." ) );
    return;
  }

  if ( mDialogMode == Export )
  {
    QString fileName = QFileDialog::getSaveFileName( this, tr( "Save styles" ), QDir::homePath(),
                       tr( "XML files (*.xml *.XML)" ) );
    if ( fileName.isEmpty() )
    {
      return;
    }

    // ensure the user never omitted the extension from the file name
    if ( !fileName.endsWith( QLatin1String( ".xml" ), Qt::CaseInsensitive ) )
    {
      fileName += QLatin1String( ".xml" );
    }

    mFileName = fileName;

    moveStyles( &selection, mStyle, mTempStyle );
    if ( !mTempStyle->exportXml( mFileName ) )
    {
      QMessageBox::warning( this, tr( "Export/import error" ),
                            tr( "Error when saving selected symbols to file:\n%1" )
                            .arg( mTempStyle->errorString() ) );
      return;
    }
    else
    {
      QMessageBox::information( this, tr( "Export successful" ),
                                tr( "The selected symbols were successfully exported to file:\n%1" )
                                .arg( mFileName ) );
    }
  }
  else // import
  {
    moveStyles( &selection, mTempStyle, mStyle );

    // clear model
    QStandardItemModel *model = qobject_cast<QStandardItemModel *>( listItems->model() );
    model->clear();
    accept();
  }

  mFileName = QLatin1String( "" );
  mTempStyle->clear();
}

bool QgsStyleExportImportDialog::populateStyles( QgsStyle *style )
{
  // load symbols and color ramps from file
  if ( mDialogMode == Import )
  {
    // NOTE mTempStyle is style here
    if ( !style->importXml( mFileName ) )
    {
      QMessageBox::warning( this, tr( "Import error" ),
                            tr( "An error occurred during import:\n%1" ).arg( style->errorString() ) );
      return false;
    }
  }

  QStandardItemModel *model = qobject_cast<QStandardItemModel *>( listItems->model() );
  model->clear();

  // populate symbols
  QStringList styleNames = style->symbolNames();
  QString name;

  for ( int i = 0; i < styleNames.count(); ++i )
  {
    name = styleNames[i];
    QStringList tags = style->tagsOfSymbol( QgsStyle::SymbolEntity, name );
    QgsSymbol *symbol = style->symbol( name );
    QStandardItem *item = new QStandardItem( name );
    QIcon icon = QgsSymbolLayerUtils::symbolPreviewIcon( symbol, listItems->iconSize(), 15 );
    item->setIcon( icon );
    item->setToolTip( QString( "<b>%1</b><br><i>%2</i>" ).arg( name ).arg( tags.count() > 0 ? tags.join( ", " ) : tr( "Not tagged" ) ) );
    // Set font to 10points to show reasonable text
    QFont itemFont = item->font();
    itemFont.setPointSize( 10 );
    item->setFont( itemFont );
    model->appendRow( item );
    delete symbol;
  }

  // and color ramps
  styleNames = style->colorRampNames();

  for ( int i = 0; i < styleNames.count(); ++i )
  {
    name = styleNames[i];
    std::unique_ptr< QgsColorRamp > ramp( style->colorRamp( name ) );

    QStandardItem *item = new QStandardItem( name );
    QIcon icon = QgsSymbolLayerUtils::colorRampPreviewIcon( ramp.get(), listItems->iconSize(), 15 );
    item->setIcon( icon );
    model->appendRow( item );
  }
  return true;
}

void QgsStyleExportImportDialog::moveStyles( QModelIndexList *selection, QgsStyle *src, QgsStyle *dst )
{
  QString symbolName;
  QgsSymbol *symbol = nullptr;
  QStringList symbolTags;
  bool symbolFavorite;
  QgsColorRamp *ramp = nullptr;
  QModelIndex index;
  bool isSymbol = true;
  bool prompt = true;
  bool overwrite = true;

  QStringList importTags = mSymbolTags->text().split( ',' );

  QStringList favoriteSymbols = src->symbolsOfFavorite( QgsStyle::SymbolEntity );
  QStringList favoriteColorramps = src->symbolsOfFavorite( QgsStyle::ColorrampEntity );

  for ( int i = 0; i < selection->size(); ++i )
  {
    index = selection->at( i );
    symbolName = index.model()->data( index, 0 ).toString();
    symbol = src->symbol( symbolName );

    if ( !mIgnoreXMLTags->isChecked() )
    {
      symbolTags = src->tagsOfSymbol( !symbol ? QgsStyle::ColorrampEntity : QgsStyle::SymbolEntity, symbolName );
    }
    else
    {
      symbolTags.clear();
    }

    if ( mDialogMode == Import )
    {
      symbolTags << importTags;
      symbolFavorite = mFavorite->isChecked();
    }
    else
    {
      symbolFavorite = !symbol ? favoriteColorramps.contains( symbolName ) : favoriteSymbols.contains( symbolName );
    }

    if ( !symbol )
    {
      isSymbol = false;
      ramp = src->colorRamp( symbolName );
    }

    if ( isSymbol )
    {
      if ( dst->symbolNames().contains( symbolName ) && prompt )
      {
        int res = QMessageBox::warning( this, tr( "Duplicate names" ),
                                        tr( "Symbol with name '%1' already exists.\nOverwrite?" )
                                        .arg( symbolName ),
                                        QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel );
        switch ( res )
        {
          case QMessageBox::Cancel:
            return;
          case QMessageBox::No:
            continue;
          case QMessageBox::Yes:
            dst->addSymbol( symbolName, symbol );
            dst->saveSymbol( symbolName, symbol, symbolFavorite, symbolTags );
            continue;
          case QMessageBox::YesToAll:
            prompt = false;
            overwrite = true;
            break;
          case QMessageBox::NoToAll:
            prompt = false;
            overwrite = false;
            break;
        }
      }

      if ( dst->symbolNames().contains( symbolName ) && overwrite )
      {
        dst->addSymbol( symbolName, symbol );
        dst->saveSymbol( symbolName, symbol, symbolFavorite, symbolTags );
      }
      else if ( dst->symbolNames().contains( symbolName ) && !overwrite )
      {
        continue;
      }
      else
      {
        dst->addSymbol( symbolName, symbol );
        dst->saveSymbol( symbolName, symbol, symbolFavorite, symbolTags );
      }
    }
    else
    {
      if ( dst->colorRampNames().contains( symbolName ) && prompt )
      {
        int res = QMessageBox::warning( this, tr( "Duplicate names" ),
                                        tr( "Color ramp with name '%1' already exists.\nOverwrite?" )
                                        .arg( symbolName ),
                                        QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel );
        switch ( res )
        {
          case QMessageBox::Cancel:
            return;
          case QMessageBox::No:
            continue;
          case QMessageBox::Yes:
            dst->addColorRamp( symbolName, ramp );
            dst->saveColorRamp( symbolName, ramp, symbolFavorite, symbolTags );
            continue;
          case QMessageBox::YesToAll:
            prompt = false;
            overwrite = true;
            break;
          case QMessageBox::NoToAll:
            prompt = false;
            overwrite = false;
            break;
        }
      }

      if ( dst->colorRampNames().contains( symbolName ) && overwrite )
      {
        dst->addColorRamp( symbolName, ramp );
        dst->saveColorRamp( symbolName, ramp, symbolFavorite, symbolTags );
      }
      else if ( dst->colorRampNames().contains( symbolName ) && !overwrite )
      {
        continue;
      }
      else
      {
        dst->addColorRamp( symbolName, ramp );
        dst->saveColorRamp( symbolName, ramp, symbolFavorite, symbolTags );
      }
    }
  }
}

QgsStyleExportImportDialog::~QgsStyleExportImportDialog()
{
  delete mTempFile;
  delete mTempStyle;
  delete mGroupSelectionDlg;
}

void QgsStyleExportImportDialog::selectAll()
{
  listItems->selectAll();
}

void QgsStyleExportImportDialog::clearSelection()
{
  listItems->clearSelection();
}

void QgsStyleExportImportDialog::selectSymbols( const QStringList &symbolNames )
{
  Q_FOREACH ( const QString &symbolName, symbolNames )
  {
    QModelIndexList indexes = listItems->model()->match( listItems->model()->index( 0, 0 ), Qt::DisplayRole, symbolName, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive );
    Q_FOREACH ( const QModelIndex &index, indexes )
    {
      listItems->selectionModel()->select( index, QItemSelectionModel::Select );
    }
  }
}

void QgsStyleExportImportDialog::deselectSymbols( const QStringList &symbolNames )
{
  Q_FOREACH ( const QString &symbolName, symbolNames )
  {
    QModelIndexList indexes = listItems->model()->match( listItems->model()->index( 0, 0 ), Qt::DisplayRole, symbolName, 1, Qt::MatchFixedString | Qt::MatchCaseSensitive );
    Q_FOREACH ( const QModelIndex &index, indexes )
    {
      QItemSelection deselection( index, index );
      listItems->selectionModel()->select( deselection, QItemSelectionModel::Deselect );
    }
  }
}

void QgsStyleExportImportDialog::selectTag( const QString &tagName )
{
  QStringList symbolNames = mStyle->symbolsWithTag( QgsStyle::SymbolEntity, mStyle->tagId( tagName ) );
  selectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::deselectTag( const QString &tagName )
{
  QStringList symbolNames = mStyle->symbolsWithTag( QgsStyle::SymbolEntity, mStyle->tagId( tagName ) );
  deselectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::selectSmartgroup( const QString &groupName )
{
  QStringList symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::SymbolEntity, mStyle->smartgroupId( groupName ) );
  selectSymbols( symbolNames );
  symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::ColorrampEntity, mStyle->smartgroupId( groupName ) );
  selectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::deselectSmartgroup( const QString &groupName )
{
  QStringList symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::SymbolEntity, mStyle->smartgroupId( groupName ) );
  deselectSymbols( symbolNames );
  symbolNames = mStyle->symbolsOfSmartgroup( QgsStyle::ColorrampEntity, mStyle->smartgroupId( groupName ) );
  deselectSymbols( symbolNames );
}

void QgsStyleExportImportDialog::selectByGroup()
{
  if ( ! mGroupSelectionDlg )
  {
    mGroupSelectionDlg = new QgsStyleGroupSelectionDialog( mStyle, this );
    mGroupSelectionDlg->setWindowTitle( tr( "Select Symbols by Group" ) );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::tagSelected, this, &QgsStyleExportImportDialog::selectTag );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::tagDeselected, this, &QgsStyleExportImportDialog::deselectTag );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::allSelected, this, &QgsStyleExportImportDialog::selectAll );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::allDeselected, this, &QgsStyleExportImportDialog::clearSelection );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::smartgroupSelected, this, &QgsStyleExportImportDialog::selectSmartgroup );
    connect( mGroupSelectionDlg, &QgsStyleGroupSelectionDialog::smartgroupDeselected, this, &QgsStyleExportImportDialog::deselectSmartgroup );
  }
  mGroupSelectionDlg->show();
  mGroupSelectionDlg->raise();
  mGroupSelectionDlg->activateWindow();
}

void QgsStyleExportImportDialog::importTypeChanged( int index )
{
  QString type = importTypeCombo->itemData( index ).toString();

  locationLineEdit->setText( QLatin1String( "" ) );

  if ( type == QLatin1String( "file" ) )
  {
    locationLineEdit->setEnabled( true );
    btnBrowse->setText( QStringLiteral( "Browse" ) );
  }
  else if ( type == QLatin1String( "official" ) )
  {
    btnBrowse->setText( QStringLiteral( "Fetch Symbols" ) );
    locationLineEdit->setEnabled( false );
  }
  else
  {
    btnBrowse->setText( QStringLiteral( "Fetch Symbols" ) );
    locationLineEdit->setEnabled( true );
  }
}

void QgsStyleExportImportDialog::browse()
{
  QString type = importTypeCombo->currentData().toString();

  if ( type == QLatin1String( "file" ) )
  {
    mFileName = QFileDialog::getOpenFileName( this, tr( "Load styles" ), QDir::homePath(),
                tr( "XML files (*.xml *XML)" ) );
    if ( mFileName.isEmpty() )
    {
      return;
    }
    QFileInfo pathInfo( mFileName );
    QString tag = pathInfo.fileName().remove( QStringLiteral( ".xml" ) );
    mSymbolTags->setText( tag );
    locationLineEdit->setText( mFileName );
    populateStyles( mTempStyle );
  }
  else if ( type == QLatin1String( "official" ) )
  {
    // TODO set URL
    // downloadStyleXML( QUrl( "http://...." ) );
  }
  else
  {
    downloadStyleXml( QUrl( locationLineEdit->text() ) );
  }
}

void QgsStyleExportImportDialog::downloadStyleXml( const QUrl &url )
{
  // XXX Try to move this code to some core Network interface,
  // HTTP downloading is a generic functionality that might be used elsewhere

  mTempFile = new QTemporaryFile();
  if ( mTempFile->open() )
  {
    mFileName = mTempFile->fileName();

    if ( mProgressDlg )
    {
      QProgressDialog *dummy = mProgressDlg;
      mProgressDlg = nullptr;
      delete dummy;
    }
    mProgressDlg = new QProgressDialog();
    mProgressDlg->setLabelText( tr( "Downloading style ... " ) );
    mProgressDlg->setAutoClose( true );

    connect( mProgressDlg, &QProgressDialog::canceled, this, &QgsStyleExportImportDialog::downloadCanceled );

    // open the network connection and connect the respective slots
    if ( mNetReply )
    {
      QNetworkReply *dummyReply = mNetReply;
      mNetReply = nullptr;
      delete dummyReply;
    }
    mNetReply = mNetManager->get( QNetworkRequest( url ) );

    connect( mNetReply, &QNetworkReply::finished, this, &QgsStyleExportImportDialog::httpFinished );
    connect( mNetReply, &QIODevice::readyRead, this, &QgsStyleExportImportDialog::fileReadyRead );
    connect( mNetReply, &QNetworkReply::downloadProgress, this, &QgsStyleExportImportDialog::updateProgress );
  }
}

void QgsStyleExportImportDialog::httpFinished()
{
  if ( mNetReply->error() )
  {
    mTempFile->remove();
    mFileName = QLatin1String( "" );
    mProgressDlg->hide();
    QMessageBox::information( this, tr( "HTTP Error!" ),
                              tr( "Download failed: %1." ).arg( mNetReply->errorString() ) );
    return;
  }
  else
  {
    mTempFile->flush();
    mTempFile->close();
    populateStyles( mTempStyle );
  }
}

void QgsStyleExportImportDialog::fileReadyRead()
{
  mTempFile->write( mNetReply->readAll() );
}

void QgsStyleExportImportDialog::updateProgress( qint64 bytesRead, qint64 bytesTotal )
{
  mProgressDlg->setMaximum( bytesTotal );
  mProgressDlg->setValue( bytesRead );
}

void QgsStyleExportImportDialog::downloadCanceled()
{
  mNetReply->abort();
  mTempFile->remove();
  mFileName = QLatin1String( "" );
}

void QgsStyleExportImportDialog::selectionChanged( const QItemSelection &selected, const QItemSelection &deselected )
{
  Q_UNUSED( selected );
  Q_UNUSED( deselected );
  bool nothingSelected = listItems->selectionModel()->selectedIndexes().empty();
  buttonBox->button( QDialogButtonBox::Ok )->setDisabled( nothingSelected );
}
