/***************************************************************************
                          qgsvectorlayersaveasdialog.h
 Dialog to select destination, type and crs for ogr layers
                             -------------------
    begin                : Mon Mar 22 2010
    copyright            : (C) 2010 by Juergen E. Fischer
    email                : jef at norbit dot de
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include "qgslogger.h"
#include "qgsvectorlayersaveasdialog.h"
#include "qgsprojectionselectiondialog.h"
#include "qgsvectordataprovider.h"
#include "qgscoordinatereferencesystem.h"
#include "qgseditorwidgetfactory.h"
#include "qgseditorwidgetregistry.h"
#include "qgssettings.h"
#include "qgsmapcanvas.h"
#include "qgsgui.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QTextCodec>
#include <QSpinBox>

static const int COLUMN_IDX_NAME = 0;
static const int COLUMN_IDX_TYPE = 1;
static const int COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE = 2;

QgsVectorLayerSaveAsDialog::QgsVectorLayerSaveAsDialog( long srsid, QWidget *parent, Qt::WindowFlags fl )
  : QDialog( parent, fl )
  , mCRS( srsid )
  , mLayer( 0 )
  , mAttributeTableItemChangedSlotEnabled( true )
  , mReplaceRawFieldValuesStateChangedSlotEnabled( true )
  , mActionOnExistingFile( QgsVectorFileWriter::CreateOrOverwriteFile )
{
  setup();
}

QgsVectorLayerSaveAsDialog::QgsVectorLayerSaveAsDialog( QgsVectorLayer *layer, int options, QWidget *parent, Qt::WindowFlags fl )
  : QDialog( parent, fl )
  , mLayer( layer )
  , mAttributeTableItemChangedSlotEnabled( true )
  , mReplaceRawFieldValuesStateChangedSlotEnabled( true )
  , mActionOnExistingFile( QgsVectorFileWriter::CreateOrOverwriteFile )
{
  if ( layer )
  {
    mCRS = layer->crs().srsid();
    mLayerExtent = layer->extent();
  }
  setup();
  if ( !( options & Symbology ) )
  {
    mSymbologyExportLabel->hide();
    mSymbologyExportComboBox->hide();
    mScaleLabel->hide();
    mScaleWidget->hide();
  }

  mSelectedOnly->setEnabled( layer && layer->selectedFeatureCount() != 0 );
  buttonBox->button( QDialogButtonBox::Ok )->setDisabled( true );
}

void QgsVectorLayerSaveAsDialog::setup()
{
  setupUi( this );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsVectorLayerSaveAsDialog::showHelp );

  QgsSettings settings;
  restoreGeometry( settings.value( QStringLiteral( "Windows/VectorLayerSaveAs/geometry" ) ).toByteArray() );

  QMap<QString, QString> map = QgsVectorFileWriter::ogrDriverList();
  mFormatComboBox->blockSignals( true );
  for ( QMap< QString, QString>::const_iterator it = map.constBegin(); it != map.constEnd(); ++it )
  {
    mFormatComboBox->addItem( it.key(), it.value() );
  }

  QString format = settings.value( QStringLiteral( "UI/lastVectorFormat" ), "ESRI Shapefile" ).toString();
  mFormatComboBox->setCurrentIndex( mFormatComboBox->findData( format ) );
  mFormatComboBox->blockSignals( false );

  //add geometry types to combobox
  mGeometryTypeComboBox->addItem( tr( "Automatic" ), -1 );
  mGeometryTypeComboBox->addItem( QgsWkbTypes::displayString( QgsWkbTypes::Point ), QgsWkbTypes::Point );
  mGeometryTypeComboBox->addItem( QgsWkbTypes::displayString( QgsWkbTypes::LineString ), QgsWkbTypes::LineString );
  mGeometryTypeComboBox->addItem( QgsWkbTypes::displayString( QgsWkbTypes::Polygon ), QgsWkbTypes::Polygon );
  mGeometryTypeComboBox->addItem( QgsWkbTypes::displayString( QgsWkbTypes::GeometryCollection ), QgsWkbTypes::GeometryCollection );
  mGeometryTypeComboBox->addItem( tr( "No geometry" ), QgsWkbTypes::NoGeometry );
  mGeometryTypeComboBox->setCurrentIndex( mGeometryTypeComboBox->findData( -1 ) );

  mEncodingComboBox->addItems( QgsVectorDataProvider::availableEncodings() );

  QString enc = settings.value( QStringLiteral( "UI/encoding" ), "System" ).toString();
  int idx = mEncodingComboBox->findText( enc );
  if ( idx < 0 )
  {
    mEncodingComboBox->insertItem( 0, enc );
    idx = 0;
  }

  QgsCoordinateReferenceSystem srs = QgsCoordinateReferenceSystem::fromSrsId( mCRS );
  mCrsSelector->setCrs( srs );
  mCrsSelector->setLayerCrs( srs );
  mCrsSelector->dialog()->setMessage( tr( "Select the coordinate reference system for the vector file. "
                                          "The data points will be transformed from the layer coordinate reference system." ) );

  mEncodingComboBox->setCurrentIndex( idx );
  on_mFormatComboBox_currentIndexChanged( mFormatComboBox->currentIndex() );

  //symbology export combo box
  mSymbologyExportComboBox->addItem( tr( "No symbology" ), QgsVectorFileWriter::NoSymbology );
  mSymbologyExportComboBox->addItem( tr( "Feature symbology" ), QgsVectorFileWriter::FeatureSymbology );
  mSymbologyExportComboBox->addItem( tr( "Symbol layer symbology" ), QgsVectorFileWriter::SymbolLayerSymbology );
  on_mSymbologyExportComboBox_currentIndexChanged( mSymbologyExportComboBox->currentText() );

  // extent group box
  mExtentGroupBox->setOutputCrs( srs );
  mExtentGroupBox->setOriginalExtent( mLayerExtent, srs );
  mExtentGroupBox->setOutputExtentFromOriginal();
  mExtentGroupBox->setCheckable( true );
  mExtentGroupBox->setChecked( false );
  mExtentGroupBox->setCollapsed( true );
}

QList<QPair<QLabel *, QWidget *> > QgsVectorLayerSaveAsDialog::createControls( const QMap<QString, QgsVectorFileWriter::Option *> &options )
{
  QList<QPair<QLabel *, QWidget *> > controls;
  QMap<QString, QgsVectorFileWriter::Option *>::ConstIterator it;

  for ( it = options.constBegin(); it != options.constEnd(); ++it )
  {
    QgsVectorFileWriter::Option *option = it.value();
    QLabel *label = new QLabel( it.key() );
    QWidget *control = nullptr;
    switch ( option->type )
    {
      case QgsVectorFileWriter::Int:
      {
        QgsVectorFileWriter::IntOption *opt = dynamic_cast<QgsVectorFileWriter::IntOption *>( option );
        if ( opt )
        {
          QSpinBox *sb = new QSpinBox();
          sb->setObjectName( it.key() );
          sb->setValue( opt->defaultValue );
          control = sb;
        }
        break;
      }

      case QgsVectorFileWriter::Set:
      {
        QgsVectorFileWriter::SetOption *opt = dynamic_cast<QgsVectorFileWriter::SetOption *>( option );
        if ( opt )
        {
          QComboBox *cb = new QComboBox();
          cb->setObjectName( it.key() );
          Q_FOREACH ( const QString &val, opt->values )
          {
            cb->addItem( val, val );
          }
          if ( opt->allowNone )
            cb->addItem( tr( "<Default>" ), QVariant( QVariant::String ) );
          int idx = cb->findText( opt->defaultValue );
          if ( idx == -1 )
            idx = cb->findData( QVariant( QVariant::String ) );
          cb->setCurrentIndex( idx );
          control = cb;
        }
        break;
      }

      case QgsVectorFileWriter::String:
      {
        QgsVectorFileWriter::StringOption *opt = dynamic_cast<QgsVectorFileWriter::StringOption *>( option );
        if ( opt )
        {
          QLineEdit *le = new QLineEdit( opt->defaultValue );
          le->setObjectName( it.key() );
          control = le;
        }
        break;
      }

      case QgsVectorFileWriter::Hidden:
        control = nullptr;
        break;
    }

    if ( control )
    {
      // Pack the tooltip in some html element, so it gets linebreaks.
      label->setToolTip( QStringLiteral( "<p>%1</p>" ).arg( option->docString ) );
      control->setToolTip( QStringLiteral( "<p>%1</p>" ).arg( option->docString ) );

      controls << QPair<QLabel *, QWidget *>( label, control );
    }
  }

  return controls;
}

QgsVectorLayerSaveAsDialog::~QgsVectorLayerSaveAsDialog()
{
  QgsSettings settings;
  settings.setValue( QStringLiteral( "Windows/VectorLayerSaveAs/geometry" ), saveGeometry() );
}

void QgsVectorLayerSaveAsDialog::accept()
{
  if ( QFile::exists( filename() ) )
  {
    QgsVectorFileWriter::EditionCapabilities caps =
      QgsVectorFileWriter::editionCapabilities( filename() );
    bool layerExists = QgsVectorFileWriter::targetLayerExists( filename(),
                       layername() );
    if ( layerExists )
    {
      if ( !( caps & QgsVectorFileWriter::CanAppendToExistingLayer ) &&
           ( caps & QgsVectorFileWriter::CanDeleteLayer ) &&
           ( caps & QgsVectorFileWriter::CanAddNewLayer ) )
      {
        QMessageBox msgBox;
        msgBox.setIcon( QMessageBox::Question );
        msgBox.setWindowTitle( tr( "The Layer Already Exists" ) );
        msgBox.setText( tr( "Do you want to overwrite the whole file or overwrite the layer?" ) );
        QPushButton *overwriteFileButton = msgBox.addButton( tr( "Overwrite file" ), QMessageBox::ActionRole );
        QPushButton *overwriteLayerButton = msgBox.addButton( tr( "Overwrite layer" ), QMessageBox::ActionRole );
        msgBox.setStandardButtons( QMessageBox::Cancel );
        msgBox.setDefaultButton( QMessageBox::Cancel );
        int ret = msgBox.exec();
        if ( ret == QMessageBox::Cancel )
          return;
        if ( msgBox.clickedButton() == overwriteFileButton )
          mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
        else if ( msgBox.clickedButton() == overwriteLayerButton )
          mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      }
      else if ( !( caps & QgsVectorFileWriter::CanAppendToExistingLayer ) )
      {
        if ( QMessageBox::question( this,
                                    tr( "The file already exists" ),
                                    tr( "Do you want to overwrite the existing file?" ) ) == QMessageBox::NoButton )
        {
          return;
        }
        mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
      }
      else if ( ( caps & QgsVectorFileWriter::CanDeleteLayer ) &&
                ( caps & QgsVectorFileWriter::CanAddNewLayer ) )
      {
        QMessageBox msgBox;
        msgBox.setIcon( QMessageBox::Question );
        msgBox.setWindowTitle( tr( "The Layer Already Exists" ) );
        msgBox.setText( tr( "Do you want to overwrite the whole file, overwrite the layer or append features to the layer?" ) );
        QPushButton *overwriteFileButton = msgBox.addButton( tr( "Overwrite file" ), QMessageBox::ActionRole );
        QPushButton *overwriteLayerButton = msgBox.addButton( tr( "Overwrite layer" ), QMessageBox::ActionRole );
        QPushButton *appendToLayerButton = msgBox.addButton( tr( "Append to layer" ), QMessageBox::ActionRole );
        msgBox.setStandardButtons( QMessageBox::Cancel );
        msgBox.setDefaultButton( QMessageBox::Cancel );
        int ret = msgBox.exec();
        if ( ret == QMessageBox::Cancel )
          return;
        if ( msgBox.clickedButton() == overwriteFileButton )
          mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
        else if ( msgBox.clickedButton() == overwriteLayerButton )
          mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
        else if ( msgBox.clickedButton() == appendToLayerButton )
          mActionOnExistingFile = QgsVectorFileWriter::AppendToLayerNoNewFields;
      }
      else
      {
        QMessageBox msgBox;
        msgBox.setIcon( QMessageBox::Question );
        msgBox.setWindowTitle( tr( "The Layer Already Exists" ) );
        msgBox.setText( tr( "Do you want to overwrite the whole file or append features to the layer?" ) );
        QPushButton *overwriteFileButton = msgBox.addButton( tr( "Overwrite file" ), QMessageBox::ActionRole );
        QPushButton *appendToLayerButton = msgBox.addButton( tr( "Append to layer" ), QMessageBox::ActionRole );
        msgBox.setStandardButtons( QMessageBox::Cancel );
        msgBox.setDefaultButton( QMessageBox::Cancel );
        int ret = msgBox.exec();
        if ( ret == QMessageBox::Cancel )
          return;
        if ( msgBox.clickedButton() == overwriteFileButton )
          mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
        else if ( msgBox.clickedButton() == appendToLayerButton )
          mActionOnExistingFile = QgsVectorFileWriter::AppendToLayerNoNewFields;
      }

      if ( mActionOnExistingFile == QgsVectorFileWriter::AppendToLayerNoNewFields )
      {
        if ( QgsVectorFileWriter::areThereNewFieldsToCreate( filename(),
             layername(),
             mLayer,
             selectedAttributes() ) )
        {
          if ( QMessageBox::question( this,
                                      tr( "The existing layer has different fields" ),
                                      tr( "Do you want to add the missing fields to the layer?" ) ) == QMessageBox::Yes )
          {
            mActionOnExistingFile = QgsVectorFileWriter::AppendToLayerAddFields;
          }
        }
      }

    }
    else
    {
      if ( ( caps & QgsVectorFileWriter::CanAddNewLayer ) )
      {
        mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteLayer;
      }
      else
      {
        if ( QMessageBox::question( this,
                                    tr( "The file already exists" ),
                                    tr( "Do you want to overwrite the existing file?" ) ) == QMessageBox::NoButton )
        {
          return;
        }
        mActionOnExistingFile = QgsVectorFileWriter::CreateOrOverwriteFile;
      }
    }
  }

  QgsSettings settings;
  settings.setValue( QStringLiteral( "UI/lastVectorFileFilterDir" ), QFileInfo( filename() ).absolutePath() );
  settings.setValue( QStringLiteral( "UI/lastVectorFormat" ), format() );
  settings.setValue( QStringLiteral( "UI/encoding" ), encoding() );
  QDialog::accept();
}

void QgsVectorLayerSaveAsDialog::on_mFormatComboBox_currentIndexChanged( int idx )
{
  Q_UNUSED( idx );

  browseFilename->setEnabled( true );
  leFilename->setEnabled( true );
  bool selectAllFields = true;
  bool fieldsAsDisplayedValues = false;

  const QString sFormat( format() );
  if ( sFormat == QLatin1String( "DXF" ) || sFormat == QLatin1String( "DGN" ) )
  {
    mAttributesSelection->setEnabled( false );
    selectAllFields = false;
  }
  else
  {
    mAttributesSelection->setEnabled( true );
    fieldsAsDisplayedValues = ( sFormat == QLatin1String( "CSV" ) || sFormat == QLatin1String( "XLS" ) || sFormat == QLatin1String( "XLSX" ) || sFormat == QLatin1String( "ODS" ) );
  }

  leLayername->setEnabled( sFormat == QLatin1String( "KML" ) ||
                           sFormat == QLatin1String( "GPKG" ) ||
                           sFormat == QLatin1String( "XLSX" ) ||
                           sFormat == QLatin1String( "ODS" ) ||
                           sFormat == QLatin1String( "FileGDB" ) ||
                           sFormat == QLatin1String( "SQLite" ) ||
                           sFormat == QLatin1String( "SpatiaLite" ) );
  if ( !leLayername->isEnabled() )
    leLayername->setText( QString() );
  else if ( leLayername->text().isEmpty() &&
            !leFilename->text().isEmpty() )
  {
    QString layerName = QFileInfo( leFilename->text() ).baseName();
    leLayername->setText( layerName ) ;
  }

  if ( mLayer )
  {
    mAttributeTable->setRowCount( mLayer->fields().count() );

    bool foundFieldThatCanBeExportedAsDisplayedValue = false;
    for ( int i = 0; i < mLayer->fields().size(); ++i )
    {
      const QgsEditorWidgetSetup setup = QgsGui::editorWidgetRegistry()->findBest( mLayer, mLayer->fields()[i].name() );
      if ( setup.type() != QLatin1String( "TextEdit" ) &&
           QgsGui::editorWidgetRegistry()->factory( setup.type() ) )
      {
        foundFieldThatCanBeExportedAsDisplayedValue = true;
        break;
      }
    }
    if ( foundFieldThatCanBeExportedAsDisplayedValue )
    {
      mAttributeTable->setColumnCount( 3 );
      mAttributeTable->setHorizontalHeaderLabels( QStringList() << tr( "Name" ) << tr( "Type" ) << tr( "Replace with displayed values" ) );
    }
    else
    {
      mAttributeTable->setColumnCount( 2 );
      mAttributeTable->setHorizontalHeaderLabels( QStringList() << tr( "Name" ) << tr( "Type" ) );
    }

    mAttributeTableItemChangedSlotEnabled = false;

    for ( int i = 0; i < mLayer->fields().size(); ++i )
    {
      QgsField fld = mLayer->fields().at( i );
      Qt::ItemFlags flags = mLayer->providerType() != QLatin1String( "oracle" ) || !fld.typeName().contains( QLatin1String( "SDO_GEOMETRY" ) ) ? Qt::ItemIsEnabled : Qt::NoItemFlags;
      QTableWidgetItem *item = nullptr;
      item = new QTableWidgetItem( fld.name() );
      item->setFlags( flags | Qt::ItemIsUserCheckable );
      item->setCheckState( ( selectAllFields ) ? Qt::Checked : Qt::Unchecked );
      mAttributeTable->setItem( i, COLUMN_IDX_NAME, item );

      item = new QTableWidgetItem( fld.typeName() );
      item->setFlags( flags );
      mAttributeTable->setItem( i, COLUMN_IDX_TYPE, item );

      if ( foundFieldThatCanBeExportedAsDisplayedValue )
      {
        const QgsEditorWidgetSetup setup = QgsGui::editorWidgetRegistry()->findBest( mLayer, mLayer->fields()[i].name() );
        QgsEditorWidgetFactory *factory = nullptr;
        if ( flags == Qt::ItemIsEnabled &&
             setup.type() != QLatin1String( "TextEdit" ) &&
             ( factory = QgsGui::editorWidgetRegistry()->factory( setup.type() ) ) )
        {
          item = new QTableWidgetItem( tr( "Use %1" ).arg( factory->name() ) );
          item->setFlags( ( selectAllFields ) ? ( Qt::ItemIsEnabled | Qt::ItemIsUserCheckable ) : Qt::ItemIsUserCheckable );
          item->setCheckState( ( selectAllFields && fieldsAsDisplayedValues ) ? Qt::Checked : Qt::Unchecked );
          mAttributeTable->setItem( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE, item );
        }
        else
        {
          item = new QTableWidgetItem();
          item->setFlags( Qt::NoItemFlags );
          mAttributeTable->setItem( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE, item );
        }
      }
    }

    mAttributeTableItemChangedSlotEnabled = true;

    mReplaceRawFieldValuesStateChangedSlotEnabled = false;
    mReplaceRawFieldValues->setChecked( selectAllFields && fieldsAsDisplayedValues );
    mReplaceRawFieldValuesStateChangedSlotEnabled = true;
    mReplaceRawFieldValues->setEnabled( selectAllFields );
    mReplaceRawFieldValues->setVisible( foundFieldThatCanBeExportedAsDisplayedValue );

    mAttributeTable->resizeColumnsToContents();
  }

  QgsVectorFileWriter::MetaData driverMetaData;

  while ( mDatasourceOptionsGroupBox->layout()->count() )
  {
    QLayoutItem *item = mDatasourceOptionsGroupBox->layout()->takeAt( 0 );
    delete item->widget();
    delete item;
  }

  while ( mLayerOptionsGroupBox->layout()->count() )
  {
    QLayoutItem *item = mLayerOptionsGroupBox->layout()->takeAt( 0 );
    delete item->widget();
    delete item;
  }

  // workaround so the Q_FOREACH macro does not get confused by the ','
  typedef QPair<QLabel *, QWidget *> LabelControlPair;

  if ( QgsVectorFileWriter::driverMetadata( format(), driverMetaData ) )
  {
    if ( driverMetaData.driverOptions.size() != 0 )
    {
      mDatasourceOptionsGroupBox->setVisible( true );
      QList<QPair<QLabel *, QWidget *> > controls = createControls( driverMetaData.driverOptions );

      QFormLayout *datasourceLayout = dynamic_cast<QFormLayout *>( mDatasourceOptionsGroupBox->layout() );

      Q_FOREACH ( LabelControlPair control, controls )
      {
        datasourceLayout->addRow( control.first, control.second );
      }
    }
    else
    {
      mDatasourceOptionsGroupBox->setVisible( false );
    }

    if ( driverMetaData.layerOptions.size() != 0 )
    {
      mLayerOptionsGroupBox->setVisible( true );
      QList<QPair<QLabel *, QWidget *> > controls = createControls( driverMetaData.layerOptions );

      QFormLayout *layerOptionsLayout = dynamic_cast<QFormLayout *>( mLayerOptionsGroupBox->layout() );

      Q_FOREACH ( LabelControlPair control, controls )
      {
        layerOptionsLayout->addRow( control.first, control.second );
      }
    }
    else
    {
      mLayerOptionsGroupBox->setVisible( false );
    }

    if ( driverMetaData.compulsoryEncoding.isEmpty() )
    {
      mEncodingComboBox->setEnabled( true );
    }
    else
    {
      int idx = mEncodingComboBox->findText( driverMetaData.compulsoryEncoding );
      if ( idx >= 0 )
      {
        mEncodingComboBox->setCurrentIndex( idx );
        mEncodingComboBox->setDisabled( true );
      }
      else
      {
        mEncodingComboBox->setEnabled( true );
      }
    }

  }
  else
  {
    mEncodingComboBox->setEnabled( true );
  }
}

void QgsVectorLayerSaveAsDialog::on_mReplaceRawFieldValues_stateChanged( int )
{
  if ( !mReplaceRawFieldValuesStateChangedSlotEnabled )
    return;
  if ( mAttributeTable->columnCount() != 3 )
    return;
  mReplaceRawFieldValuesStateChangedSlotEnabled = false;
  mAttributeTableItemChangedSlotEnabled = false;
  if ( mReplaceRawFieldValues->checkState() != Qt::PartiallyChecked )
  {
    for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
    {
      if ( mAttributeTable->item( i, COLUMN_IDX_NAME )->checkState() == Qt::Checked &&
           mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE ) &&
           mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsEnabled )
      {
        mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setCheckState( mReplaceRawFieldValues->checkState() );
      }
    }
  }
  mReplaceRawFieldValues->setTristate( false );
  mAttributeTableItemChangedSlotEnabled = true;
  mReplaceRawFieldValuesStateChangedSlotEnabled = true;
}

void QgsVectorLayerSaveAsDialog::on_mAttributeTable_itemChanged( QTableWidgetItem *item )
{
  if ( !mAttributeTableItemChangedSlotEnabled )
    return;
  mReplaceRawFieldValuesStateChangedSlotEnabled = false;
  mAttributeTableItemChangedSlotEnabled = false;
  int row = item->row();
  int column = item->column();
  if ( column == COLUMN_IDX_NAME &&
       mAttributeTable->item( row, column )->checkState() == Qt::Unchecked &&
       mAttributeTable->columnCount() == 3 &&
       mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE ) &&
       ( mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsUserCheckable ) )
  {
    mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setCheckState( Qt::Unchecked );
    mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setFlags( Qt::ItemIsUserCheckable );
    bool checkBoxEnabled = false;
    for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
    {
      if ( mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE ) &&
           mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsEnabled )
      {
        checkBoxEnabled = true;
        break;
      }
    }
    mReplaceRawFieldValues->setEnabled( checkBoxEnabled );
    if ( !checkBoxEnabled )
      mReplaceRawFieldValues->setCheckState( Qt::Unchecked );
  }
  else if ( column == COLUMN_IDX_NAME &&
            mAttributeTable->item( row, column )->checkState() == Qt::Checked &&
            mAttributeTable->columnCount() == 3 &&
            mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE ) &&
            ( mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsUserCheckable ) )
  {
    mAttributeTable->item( row, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
    mReplaceRawFieldValues->setEnabled( true );
  }
  else if ( column == COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE &&
            ( mAttributeTable->item( row, column )->flags() & Qt::ItemIsUserCheckable ) )
  {
    bool allChecked = true;
    bool allUnchecked = true;
    for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
    {
      if ( mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE ) &&
           mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsEnabled )
      {
        if ( mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->checkState() == Qt::Unchecked )
          allChecked = false;
        else
          allUnchecked = false;
      }
    }
    mReplaceRawFieldValues->setCheckState( ( !allChecked && !allUnchecked ) ? Qt::PartiallyChecked : ( allChecked ) ? Qt::Checked : Qt::Unchecked );
  }
  mAttributeTableItemChangedSlotEnabled = true;
  mReplaceRawFieldValuesStateChangedSlotEnabled = true;
}

void QgsVectorLayerSaveAsDialog::on_leFilename_textChanged( const QString &text )
{
  buttonBox->button( QDialogButtonBox::Ok )->setEnabled(
    !text.isEmpty() && QFileInfo( text ).absoluteDir().exists() );

  if ( leLayername->isEnabled() )
  {
    QString layerName = QFileInfo( text ).baseName();
    leLayername->setText( layerName );
  }
}

void QgsVectorLayerSaveAsDialog::on_browseFilename_clicked()
{
  QgsSettings settings;
  QString dirName = leFilename->text().isEmpty() ? settings.value( QStringLiteral( "UI/lastVectorFileFilterDir" ), QDir::homePath() ).toString() : leFilename->text();
  QString filterString = QgsVectorFileWriter::filterForDriver( format() );
  QString outputFile = QFileDialog::getSaveFileName( nullptr, tr( "Save layer as..." ), dirName, filterString, nullptr, QFileDialog::DontConfirmOverwrite );
  if ( !outputFile.isNull() )
  {
    leFilename->setText( outputFile );
  }
}

void QgsVectorLayerSaveAsDialog::on_mCrsSelector_crsChanged( const QgsCoordinateReferenceSystem &crs )
{
  mCRS = crs.srsid();
  mExtentGroupBox->setOutputCrs( crs );
}

QString QgsVectorLayerSaveAsDialog::filename() const
{
  return leFilename->text();
}

QString QgsVectorLayerSaveAsDialog::layername() const
{
  return leLayername->text();
}

QString QgsVectorLayerSaveAsDialog::encoding() const
{
  return mEncodingComboBox->currentText();
}

QString QgsVectorLayerSaveAsDialog::format() const
{
  return mFormatComboBox->currentData().toString();
}

long QgsVectorLayerSaveAsDialog::crs() const
{
  return mCRS;
}

QStringList QgsVectorLayerSaveAsDialog::datasourceOptions() const
{
  QStringList options;

  QgsVectorFileWriter::MetaData driverMetaData;

  if ( QgsVectorFileWriter::driverMetadata( format(), driverMetaData ) )
  {
    QMap<QString, QgsVectorFileWriter::Option *>::ConstIterator it;

    for ( it = driverMetaData.driverOptions.constBegin(); it != driverMetaData.driverOptions.constEnd(); ++it )
    {
      switch ( it.value()->type )
      {
        case QgsVectorFileWriter::Int:
        {
          QgsVectorFileWriter::IntOption *opt = dynamic_cast<QgsVectorFileWriter::IntOption *>( *it );
          QSpinBox *sb = mDatasourceOptionsGroupBox->findChild<QSpinBox *>( it.key() );
          if ( opt && sb && sb->value() != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key() ).arg( sb->value() );
          break;
        }

        case QgsVectorFileWriter::Set:
        {
          QgsVectorFileWriter::SetOption *opt = dynamic_cast<QgsVectorFileWriter::SetOption *>( *it );
          QComboBox *cb = mDatasourceOptionsGroupBox->findChild<QComboBox *>( it.key() );
          if ( opt && cb && cb->itemData( cb->currentIndex() ) != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key(), cb->currentText() );
          break;
        }

        case QgsVectorFileWriter::String:
        {
          QgsVectorFileWriter::StringOption *opt = dynamic_cast<QgsVectorFileWriter::StringOption *>( *it );
          QLineEdit *le = mDatasourceOptionsGroupBox->findChild<QLineEdit *>( it.key() );
          if ( opt && le && le->text() != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key(), le->text() );
          break;
        }

        case QgsVectorFileWriter::Hidden:
        {
          QgsVectorFileWriter::HiddenOption *opt =
            dynamic_cast<QgsVectorFileWriter::HiddenOption *>( it.value() );
          options << QStringLiteral( "%1=%2" ).arg( it.key(), opt->mValue );
          break;
        }
      }
    }
  }

  return options + mOgrDatasourceOptions->toPlainText().split( '\n' );
}

QStringList QgsVectorLayerSaveAsDialog::layerOptions() const
{
  QStringList options;

  QgsVectorFileWriter::MetaData driverMetaData;

  if ( QgsVectorFileWriter::driverMetadata( format(), driverMetaData ) )
  {
    QMap<QString, QgsVectorFileWriter::Option *>::ConstIterator it;

    for ( it = driverMetaData.layerOptions.constBegin(); it != driverMetaData.layerOptions.constEnd(); ++it )
    {
      switch ( it.value()->type )
      {
        case QgsVectorFileWriter::Int:
        {
          QgsVectorFileWriter::IntOption *opt = dynamic_cast<QgsVectorFileWriter::IntOption *>( *it );
          QSpinBox *sb = mLayerOptionsGroupBox->findChild<QSpinBox *>( it.key() );
          if ( opt && sb && sb->value() != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key() ).arg( sb->value() );
          break;
        }

        case QgsVectorFileWriter::Set:
        {
          QgsVectorFileWriter::SetOption *opt = dynamic_cast<QgsVectorFileWriter::SetOption *>( *it );
          QComboBox *cb = mLayerOptionsGroupBox->findChild<QComboBox *>( it.key() );
          if ( opt && cb && cb->itemData( cb->currentIndex() ) != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key(), cb->currentText() );
          break;
        }

        case QgsVectorFileWriter::String:
        {
          QgsVectorFileWriter::StringOption *opt = dynamic_cast<QgsVectorFileWriter::StringOption *>( *it );
          QLineEdit *le = mLayerOptionsGroupBox->findChild<QLineEdit *>( it.key() );
          if ( opt && le && le->text() != opt->defaultValue )
            options << QStringLiteral( "%1=%2" ).arg( it.key(), le->text() );
          break;
        }

        case QgsVectorFileWriter::Hidden:
        {
          QgsVectorFileWriter::HiddenOption *opt =
            dynamic_cast<QgsVectorFileWriter::HiddenOption *>( it.value() );
          options << QStringLiteral( "%1=%2" ).arg( it.key(), opt->mValue );
          break;
        }
      }
    }
  }

  return options + mOgrLayerOptions->toPlainText().split( '\n' );
}

QgsAttributeList QgsVectorLayerSaveAsDialog::selectedAttributes() const
{
  QgsAttributeList attributes;

  for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
  {
    if ( mAttributeTable->item( i, COLUMN_IDX_NAME )->checkState() == Qt::Checked )
    {
      attributes.append( i );
    }
  }

  return attributes;
}

QgsAttributeList QgsVectorLayerSaveAsDialog::attributesAsDisplayedValues() const
{
  QgsAttributeList attributes;

  for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
  {
    if ( mAttributeTable->item( i, COLUMN_IDX_NAME )->checkState() == Qt::Checked &&
         mAttributeTable->columnCount() == 3 &&
         mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->checkState() == Qt::Checked )
    {
      attributes.append( i );
    }
  }

  return attributes;
}

bool QgsVectorLayerSaveAsDialog::addToCanvas() const
{
  return mAddToCanvas->isChecked();
}

int QgsVectorLayerSaveAsDialog::symbologyExport() const
{
  return mSymbologyExportComboBox->currentData().toInt();
}

double QgsVectorLayerSaveAsDialog::scale() const
{
  return mScaleWidget->scale();
}

void QgsVectorLayerSaveAsDialog::setMapCanvas( QgsMapCanvas *canvas )
{
  mMapCanvas = canvas;
  mScaleWidget->setMapCanvas( canvas );
  mScaleWidget->setShowCurrentScaleButton( true );
  mExtentGroupBox->setCurrentExtent( canvas->mapSettings().visibleExtent(), canvas->mapSettings().destinationCrs() );
}

bool QgsVectorLayerSaveAsDialog::hasFilterExtent() const
{
  return mExtentGroupBox->isChecked();
}

QgsRectangle QgsVectorLayerSaveAsDialog::filterExtent() const
{
  return mExtentGroupBox->outputExtent();
}

bool QgsVectorLayerSaveAsDialog::onlySelected() const
{
  return mSelectedOnly->isChecked();
}

QgsWkbTypes::Type QgsVectorLayerSaveAsDialog::geometryType() const
{
  int currentIndexData = mGeometryTypeComboBox->currentData().toInt();
  if ( currentIndexData == -1 )
  {
    //automatic
    return QgsWkbTypes::Unknown;
  }

  return ( QgsWkbTypes::Type )currentIndexData;
}

bool QgsVectorLayerSaveAsDialog::automaticGeometryType() const
{
  int currentIndexData = mGeometryTypeComboBox->currentData().toInt();
  return currentIndexData == -1;
}

bool QgsVectorLayerSaveAsDialog::forceMulti() const
{
  return mForceMultiCheckBox->isChecked();
}

void QgsVectorLayerSaveAsDialog::setForceMulti( bool checked )
{
  mForceMultiCheckBox->setChecked( checked );
}

bool QgsVectorLayerSaveAsDialog::includeZ() const
{
  return mIncludeZCheckBox->isChecked();
}

QgsVectorFileWriter::ActionOnExistingFile QgsVectorLayerSaveAsDialog::creationActionOnExistingFile() const
{
  return mActionOnExistingFile;
}

void QgsVectorLayerSaveAsDialog::setIncludeZ( bool checked )
{
  mIncludeZCheckBox->setChecked( checked );
}

void QgsVectorLayerSaveAsDialog::on_mSymbologyExportComboBox_currentIndexChanged( const QString &text )
{
  bool scaleEnabled = true;
  if ( text == tr( "No symbology" ) )
  {
    scaleEnabled = false;
  }
  mScaleWidget->setEnabled( scaleEnabled );
  mScaleLabel->setEnabled( scaleEnabled );
}

void QgsVectorLayerSaveAsDialog::on_mGeometryTypeComboBox_currentIndexChanged( int index )
{
  int currentIndexData = mGeometryTypeComboBox->itemData( index ).toInt();

  if ( currentIndexData != -1 && currentIndexData != QgsWkbTypes::NoGeometry )
  {
    mForceMultiCheckBox->setEnabled( true );
    mIncludeZCheckBox->setEnabled( true );
  }
  else
  {
    mForceMultiCheckBox->setEnabled( false );
    mForceMultiCheckBox->setChecked( false );
    mIncludeZCheckBox->setEnabled( false );
    mIncludeZCheckBox->setChecked( false );
  }
}

void QgsVectorLayerSaveAsDialog::on_mSelectAllAttributes_clicked()
{
  mAttributeTableItemChangedSlotEnabled = false;
  mReplaceRawFieldValuesStateChangedSlotEnabled = false;
  for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
  {
    if ( mAttributeTable->item( i, COLUMN_IDX_NAME )->flags() & Qt::ItemIsEnabled )
    {
      if ( mAttributeTable->columnCount() == 3 &&
           ( mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsUserCheckable ) )
      {
        mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setFlags( Qt::ItemIsUserCheckable | Qt::ItemIsEnabled );
      }
      mAttributeTable->item( i, COLUMN_IDX_NAME )->setCheckState( Qt::Checked );
    }
  }
  if ( mAttributeTable->columnCount() == 3 )
  {
    mReplaceRawFieldValues->setEnabled( true );
  }
  mAttributeTableItemChangedSlotEnabled = true;
  mReplaceRawFieldValuesStateChangedSlotEnabled = true;
}

void QgsVectorLayerSaveAsDialog::on_mDeselectAllAttributes_clicked()
{
  mAttributeTableItemChangedSlotEnabled = false;
  mReplaceRawFieldValuesStateChangedSlotEnabled = false;
  for ( int i = 0; i < mAttributeTable->rowCount(); i++ )
  {
    mAttributeTable->item( i, COLUMN_IDX_NAME )->setCheckState( Qt::Unchecked );
    if ( mAttributeTable->columnCount() == 3 &&
         ( mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->flags() & Qt::ItemIsUserCheckable ) )
    {
      mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setFlags( Qt::ItemIsUserCheckable );
      mAttributeTable->item( i, COLUMN_IDX_EXPORT_AS_DISPLAYED_VALUE )->setCheckState( Qt::Unchecked );
    }
  }
  if ( mAttributeTable->columnCount() == 3 )
  {
    mReplaceRawFieldValues->setCheckState( Qt::Unchecked );
    mReplaceRawFieldValues->setEnabled( false );
  }
  mAttributeTableItemChangedSlotEnabled = true;
  mReplaceRawFieldValuesStateChangedSlotEnabled = true;
}

void QgsVectorLayerSaveAsDialog::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/create_layers.html#save-layer-from-an-existing-file" ) );
}