/***************************************************************************
 qgssymbollayerwidget.cpp - symbol layer widgets

 ---------------------
 begin                : November 2009
 copyright            : (C) 2009 by Martin Dobias
 email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssymbollayerwidget.h"

#include "qgslinesymbollayer.h"
#include "qgsmarkersymbollayer.h"
#include "qgsfillsymbollayer.h"
#include "qgsgeometrygeneratorsymbollayer.h"
#include "qgssymbolslistwidget.h"

#include "characterwidget.h"
#include "qgsdashspacedialog.h"
#include "qgssymbolselectordialog.h"
#include "qgssvgcache.h"
#include "qgssymbollayerutils.h"
#include "qgscolorramp.h"
#include "qgscolorrampbutton.h"
#include "qgsgradientcolorrampdialog.h"
#include "qgsproperty.h"
#include "qgsstyle.h" //for symbol selector dialog
#include "qgsmapcanvas.h"
#include "qgsapplication.h"
#include "qgsvectorlayer.h"
#include "qgssvgselectorwidget.h"
#include "qgslogger.h"
#include "qgssettings.h"

#include <QAbstractButton>
#include <QColorDialog>
#include <QCursor>
#include <QDir>
#include <QFileDialog>
#include <QPainter>
#include <QStandardItemModel>
#include <QSvgRenderer>
#include <QMessageBox>

QgsExpressionContext QgsSymbolLayerWidget::createExpressionContext() const
{
  if ( mContext.expressionContext() )
    return *mContext.expressionContext();

  QgsExpressionContext expContext( mContext.globalProjectAtlasMapLayerScopes( vectorLayer() ) );

  QgsExpressionContextScope *symbolScope = QgsExpressionContextUtils::updateSymbolScope( nullptr, new QgsExpressionContextScope() );
  if ( const QgsSymbolLayer *symbolLayer = const_cast< QgsSymbolLayerWidget * >( this )->symbolLayer() )
  {
    //cheat a bit - set the symbol color variable to match the symbol layer's color (when we should really be using the *symbols*
    //color, but that's not accessible here). 99% of the time these will be the same anyway
    symbolScope->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_SYMBOL_COLOR, symbolLayer->color(), true ) );
  }
  expContext << symbolScope;
  expContext.lastScope()->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_GEOMETRY_PART_COUNT, 1, true ) );
  expContext.lastScope()->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_GEOMETRY_PART_NUM, 1, true ) );
  expContext.lastScope()->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_GEOMETRY_POINT_COUNT, 1, true ) );
  expContext.lastScope()->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_GEOMETRY_POINT_NUM, 1, true ) );

  // additional scopes
  Q_FOREACH ( const QgsExpressionContextScope &scope, mContext.additionalExpressionContextScopes() )
  {
    expContext.appendScope( new QgsExpressionContextScope( scope ) );
  }

  //TODO - show actual value
  expContext.setOriginalValueVariable( QVariant() );

  expContext.setHighlightedVariables( QStringList() << QgsExpressionContext::EXPR_ORIGINAL_VALUE << QgsExpressionContext::EXPR_SYMBOL_COLOR
                                      << QgsExpressionContext::EXPR_GEOMETRY_PART_COUNT << QgsExpressionContext::EXPR_GEOMETRY_PART_NUM
                                      << QgsExpressionContext::EXPR_GEOMETRY_POINT_COUNT << QgsExpressionContext::EXPR_GEOMETRY_POINT_NUM
                                      << QgsExpressionContext::EXPR_CLUSTER_COLOR << QgsExpressionContext::EXPR_CLUSTER_SIZE );

  return expContext;
}

void QgsSymbolLayerWidget::setContext( const QgsSymbolWidgetContext &context )
{
  mContext = context;
  Q_FOREACH ( QgsUnitSelectionWidget *unitWidget, findChildren<QgsUnitSelectionWidget *>() )
  {
    unitWidget->setMapCanvas( mContext.mapCanvas() );
  }
#if 0
  Q_FOREACH ( QgsPropertyOverrideButton *ddButton, findChildren<QgsPropertyOverrideButton *>() )
  {
    if ( ddButton->assistant() )
      ddButton->assistant()->setMapCanvas( mContext.mapCanvas() );
  }
#endif
}

QgsSymbolWidgetContext QgsSymbolLayerWidget::context() const
{
  return mContext;
}

void QgsSymbolLayerWidget::registerDataDefinedButton( QgsPropertyOverrideButton *button, QgsSymbolLayer::Property key )
{
  button->init( key, symbolLayer()->dataDefinedProperties(), QgsSymbolLayer::propertyDefinitions(), mVectorLayer );
  connect( button, &QgsPropertyOverrideButton::changed, this, &QgsSymbolLayerWidget::updateDataDefinedProperty );

  button->registerExpressionContextGenerator( this );
}

void QgsSymbolLayerWidget::updateDataDefinedProperty()
{
  QgsPropertyOverrideButton *button = qobject_cast<QgsPropertyOverrideButton *>( sender() );
  QgsSymbolLayer::Property key = static_cast<  QgsSymbolLayer::Property >( button->propertyKey() );
  symbolLayer()->setDataDefinedProperty( key, button->toProperty() );
  emit changed();
}

QgsSimpleLineSymbolLayerWidget::QgsSimpleLineSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mPenWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                 << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mDashPatternUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                    << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  btnChangeColor->setAllowOpacity( true );
  btnChangeColor->setColorDialogTitle( tr( "Select Line color" ) );
  btnChangeColor->setContext( QStringLiteral( "symbology" ) );

  spinOffset->setClearValue( 0.0 );

  if ( vl && vl->geometryType() != QgsWkbTypes::PolygonGeometry )
  {
    //draw inside polygon checkbox only makes sense for polygon layers
    mDrawInsideCheckBox->hide();
  }

  //make a temporary symbol for the size assistant preview
  mAssistantPreviewSymbol.reset( new QgsLineSymbol() );

  if ( vectorLayer() )
    mPenWidthDDBtn->setSymbol( mAssistantPreviewSymbol );

  connect( spinWidth, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleLineSymbolLayerWidget::penWidthChanged );
  connect( btnChangeColor, &QgsColorButton::colorChanged, this, &QgsSimpleLineSymbolLayerWidget::colorChanged );
  connect( cboPenStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleLineSymbolLayerWidget::penStyleChanged );
  connect( spinOffset, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleLineSymbolLayerWidget::offsetChanged );
  connect( cboCapStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleLineSymbolLayerWidget::penStyleChanged );
  connect( cboJoinStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleLineSymbolLayerWidget::penStyleChanged );

  updatePatternIcon();

  connect( this, &QgsSymbolLayerWidget::changed, this, &QgsSimpleLineSymbolLayerWidget::updateAssistantSymbol );
}

void QgsSimpleLineSymbolLayerWidget::updateAssistantSymbol()
{
  for ( int i = mAssistantPreviewSymbol->symbolLayerCount() - 1 ; i >= 0; --i )
  {
    mAssistantPreviewSymbol->deleteSymbolLayer( i );
  }
  mAssistantPreviewSymbol->appendSymbolLayer( mLayer->clone() );
  QgsProperty ddWidth = mLayer->dataDefinedProperties().property( QgsSymbolLayer::PropertyStrokeWidth );
  if ( ddWidth )
    mAssistantPreviewSymbol->setDataDefinedWidth( ddWidth );
}


void QgsSimpleLineSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( !layer || layer->layerType() != QLatin1String( "SimpleLine" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsSimpleLineSymbolLayer *>( layer );

  // set units
  mPenWidthUnitWidget->blockSignals( true );
  mPenWidthUnitWidget->setUnit( mLayer->widthUnit() );
  mPenWidthUnitWidget->setMapUnitScale( mLayer->widthMapUnitScale() );
  mPenWidthUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );
  mDashPatternUnitWidget->blockSignals( true );
  mDashPatternUnitWidget->setUnit( mLayer->customDashPatternUnit() );
  mDashPatternUnitWidget->setMapUnitScale( mLayer->customDashPatternMapUnitScale() );
  mDashPatternUnitWidget->setMapUnitScale( mLayer->customDashPatternMapUnitScale() );
  mDashPatternUnitWidget->blockSignals( false );

  // set values
  spinWidth->blockSignals( true );
  spinWidth->setValue( mLayer->width() );
  spinWidth->blockSignals( false );
  btnChangeColor->blockSignals( true );
  btnChangeColor->setColor( mLayer->color() );
  btnChangeColor->blockSignals( false );
  spinOffset->blockSignals( true );
  spinOffset->setValue( mLayer->offset() );
  spinOffset->blockSignals( false );
  cboPenStyle->blockSignals( true );
  cboJoinStyle->blockSignals( true );
  cboCapStyle->blockSignals( true );
  cboPenStyle->setPenStyle( mLayer->penStyle() );
  cboJoinStyle->setPenJoinStyle( mLayer->penJoinStyle() );
  cboCapStyle->setPenCapStyle( mLayer->penCapStyle() );
  cboPenStyle->blockSignals( false );
  cboJoinStyle->blockSignals( false );
  cboCapStyle->blockSignals( false );

  //use a custom dash pattern?
  bool useCustomDashPattern = mLayer->useCustomDashPattern();
  mChangePatternButton->setEnabled( useCustomDashPattern );
  label_3->setEnabled( !useCustomDashPattern );
  cboPenStyle->setEnabled( !useCustomDashPattern );
  mCustomCheckBox->blockSignals( true );
  mCustomCheckBox->setCheckState( useCustomDashPattern ? Qt::Checked : Qt::Unchecked );
  mCustomCheckBox->blockSignals( false );

  //draw inside polygon?
  bool drawInsidePolygon = mLayer->drawInsidePolygon();
  mDrawInsideCheckBox->blockSignals( true );
  mDrawInsideCheckBox->setCheckState( drawInsidePolygon ? Qt::Checked : Qt::Unchecked );
  mDrawInsideCheckBox->blockSignals( false );

  updatePatternIcon();

  registerDataDefinedButton( mColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mPenWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
  registerDataDefinedButton( mOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mDashPatternDDBtn, QgsSymbolLayer::PropertyCustomDash );
  registerDataDefinedButton( mPenStyleDDBtn, QgsSymbolLayer::PropertyStrokeStyle );
  registerDataDefinedButton( mJoinStyleDDBtn, QgsSymbolLayer::PropertyJoinStyle );
  registerDataDefinedButton( mCapStyleDDBtn, QgsSymbolLayer::PropertyCapStyle );

  updateAssistantSymbol();
}

QgsSymbolLayer *QgsSimpleLineSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsSimpleLineSymbolLayerWidget::penWidthChanged()
{
  mLayer->setWidth( spinWidth->value() );
  updatePatternIcon();
  emit changed();
}

void QgsSimpleLineSymbolLayerWidget::colorChanged( const QColor &color )
{
  mLayer->setColor( color );
  updatePatternIcon();
  emit changed();
}

void QgsSimpleLineSymbolLayerWidget::penStyleChanged()
{
  mLayer->setPenStyle( cboPenStyle->penStyle() );
  mLayer->setPenJoinStyle( cboJoinStyle->penJoinStyle() );
  mLayer->setPenCapStyle( cboCapStyle->penCapStyle() );
  emit changed();
}

void QgsSimpleLineSymbolLayerWidget::offsetChanged()
{
  mLayer->setOffset( spinOffset->value() );
  updatePatternIcon();
  emit changed();
}

void QgsSimpleLineSymbolLayerWidget::on_mCustomCheckBox_stateChanged( int state )
{
  bool checked = ( state == Qt::Checked );
  mChangePatternButton->setEnabled( checked );
  label_3->setEnabled( !checked );
  cboPenStyle->setEnabled( !checked );

  mLayer->setUseCustomDashPattern( checked );
  emit changed();
}

void QgsSimpleLineSymbolLayerWidget::on_mChangePatternButton_clicked()
{
  QgsDashSpaceDialog d( mLayer->customDashVector() );
  if ( d.exec() == QDialog::Accepted )
  {
    mLayer->setCustomDashVector( d.dashDotVector() );
    updatePatternIcon();
    emit changed();
  }
}

void QgsSimpleLineSymbolLayerWidget::on_mPenWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setWidthUnit( mPenWidthUnitWidget->unit() );
    mLayer->setWidthMapUnitScale( mPenWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleLineSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleLineSymbolLayerWidget::on_mDashPatternUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setCustomDashPatternUnit( mDashPatternUnitWidget->unit() );
    mLayer->setCustomDashPatternMapUnitScale( mDashPatternUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleLineSymbolLayerWidget::on_mDrawInsideCheckBox_stateChanged( int state )
{
  bool checked = ( state == Qt::Checked );
  mLayer->setDrawInsidePolygon( checked );
  emit changed();
}


void QgsSimpleLineSymbolLayerWidget::updatePatternIcon()
{
  if ( !mLayer )
  {
    return;
  }
  QgsSimpleLineSymbolLayer *layerCopy = mLayer->clone();
  if ( !layerCopy )
  {
    return;
  }
  layerCopy->setUseCustomDashPattern( true );
  QIcon buttonIcon = QgsSymbolLayerUtils::symbolLayerPreviewIcon( layerCopy, QgsUnitTypes::RenderMillimeters, mChangePatternButton->iconSize() );
  mChangePatternButton->setIcon( buttonIcon );
  delete layerCopy;
}


///////////


QgsSimpleMarkerSymbolLayerWidget::QgsSimpleMarkerSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mSizeUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                             << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mStrokeWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                    << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  btnChangeColorFill->setAllowOpacity( true );
  btnChangeColorFill->setColorDialogTitle( tr( "Select Fill Color" ) );
  btnChangeColorFill->setContext( QStringLiteral( "symbology" ) );
  btnChangeColorFill->setShowNoColor( true );
  btnChangeColorFill->setNoColorString( tr( "Transparent fill" ) );
  btnChangeColorStroke->setAllowOpacity( true );
  btnChangeColorStroke->setColorDialogTitle( tr( "Select Stroke Color" ) );
  btnChangeColorStroke->setContext( QStringLiteral( "symbology" ) );
  btnChangeColorStroke->setShowNoColor( true );
  btnChangeColorStroke->setNoColorString( tr( "Transparent Stroke" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );
  spinAngle->setClearValue( 0.0 );

  //make a temporary symbol for the size assistant preview
  mAssistantPreviewSymbol.reset( new QgsMarkerSymbol() );

  if ( vectorLayer() )
    mSizeDDBtn->setSymbol( mAssistantPreviewSymbol );

  int size = lstNames->iconSize().width();
  size = std::max( 30, static_cast< int >( std::round( Qgis::UI_SCALE_FACTOR * fontMetrics().width( QStringLiteral( "XXX" ) ) ) ) );
  lstNames->setGridSize( QSize( size * 1.2, size * 1.2 ) );
  lstNames->setIconSize( QSize( size, size ) );

  double markerSize = size * 0.8;
  Q_FOREACH ( QgsSimpleMarkerSymbolLayerBase::Shape shape, QgsSimpleMarkerSymbolLayerBase::availableShapes() )
  {
    QgsSimpleMarkerSymbolLayer *lyr = new QgsSimpleMarkerSymbolLayer( shape, markerSize );
    lyr->setSizeUnit( QgsUnitTypes::RenderPixels );
    lyr->setColor( QColor( 200, 200, 200 ) );
    lyr->setStrokeColor( QColor( 0, 0, 0 ) );
    QIcon icon = QgsSymbolLayerUtils::symbolLayerPreviewIcon( lyr, QgsUnitTypes::RenderPixels, QSize( size, size ) );
    QListWidgetItem *item = new QListWidgetItem( icon, QString(), lstNames );
    item->setData( Qt::UserRole, static_cast< int >( shape ) );
    item->setToolTip( QgsSimpleMarkerSymbolLayerBase::encodeShape( shape ) );
    delete lyr;
  }
  // show at least 3 rows
  lstNames->setMinimumHeight( lstNames->gridSize().height() * 3.1 );

  connect( lstNames, &QListWidget::currentRowChanged, this, &QgsSimpleMarkerSymbolLayerWidget::setShape );
  connect( btnChangeColorStroke, &QgsColorButton::colorChanged, this, &QgsSimpleMarkerSymbolLayerWidget::setColorStroke );
  connect( btnChangeColorFill, &QgsColorButton::colorChanged, this, &QgsSimpleMarkerSymbolLayerWidget::setColorFill );
  connect( cboJoinStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleMarkerSymbolLayerWidget::penJoinStyleChanged );
  connect( spinSize, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleMarkerSymbolLayerWidget::setSize );
  connect( spinAngle, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleMarkerSymbolLayerWidget::setAngle );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleMarkerSymbolLayerWidget::setOffset );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleMarkerSymbolLayerWidget::setOffset );
  connect( this, &QgsSymbolLayerWidget::changed, this, &QgsSimpleMarkerSymbolLayerWidget::updateAssistantSymbol );
}

void QgsSimpleMarkerSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "SimpleMarker" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsSimpleMarkerSymbolLayer *>( layer );

  // set values
  QgsSimpleMarkerSymbolLayerBase::Shape shape = mLayer->shape();
  for ( int i = 0; i < lstNames->count(); ++i )
  {
    if ( static_cast< QgsSimpleMarkerSymbolLayerBase::Shape >( lstNames->item( i )->data( Qt::UserRole ).toInt() ) == shape )
    {
      lstNames->setCurrentRow( i );
      break;
    }
  }
  btnChangeColorStroke->blockSignals( true );
  btnChangeColorStroke->setColor( mLayer->strokeColor() );
  btnChangeColorStroke->blockSignals( false );
  btnChangeColorFill->blockSignals( true );
  btnChangeColorFill->setColor( mLayer->fillColor() );
  btnChangeColorFill->setEnabled( QgsSimpleMarkerSymbolLayerBase::shapeIsFilled( mLayer->shape() ) );
  btnChangeColorFill->blockSignals( false );
  spinSize->blockSignals( true );
  spinSize->setValue( mLayer->size() );
  spinSize->blockSignals( false );
  spinAngle->blockSignals( true );
  spinAngle->setValue( mLayer->angle() );
  spinAngle->blockSignals( false );
  mStrokeStyleComboBox->blockSignals( true );
  mStrokeStyleComboBox->setPenStyle( mLayer->strokeStyle() );
  mStrokeStyleComboBox->blockSignals( false );
  mStrokeWidthSpinBox->blockSignals( true );
  mStrokeWidthSpinBox->setValue( mLayer->strokeWidth() );
  mStrokeWidthSpinBox->blockSignals( false );
  cboJoinStyle->blockSignals( true );
  cboJoinStyle->setPenJoinStyle( mLayer->penJoinStyle() );
  cboJoinStyle->blockSignals( false );

  // without blocking signals the value gets changed because of slot setOffset()
  spinOffsetX->blockSignals( true );
  spinOffsetX->setValue( mLayer->offset().x() );
  spinOffsetX->blockSignals( false );
  spinOffsetY->blockSignals( true );
  spinOffsetY->setValue( mLayer->offset().y() );
  spinOffsetY->blockSignals( false );

  mSizeUnitWidget->blockSignals( true );
  mSizeUnitWidget->setUnit( mLayer->sizeUnit() );
  mSizeUnitWidget->setMapUnitScale( mLayer->sizeMapUnitScale() );
  mSizeUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );
  mStrokeWidthUnitWidget->blockSignals( true );
  mStrokeWidthUnitWidget->setUnit( mLayer->strokeWidthUnit() );
  mStrokeWidthUnitWidget->setMapUnitScale( mLayer->strokeWidthMapUnitScale() );
  mStrokeWidthUnitWidget->blockSignals( false );

  //anchor points
  mHorizontalAnchorComboBox->blockSignals( true );
  mVerticalAnchorComboBox->blockSignals( true );
  mHorizontalAnchorComboBox->setCurrentIndex( mLayer->horizontalAnchorPoint() );
  mVerticalAnchorComboBox->setCurrentIndex( mLayer->verticalAnchorPoint() );
  mHorizontalAnchorComboBox->blockSignals( false );
  mVerticalAnchorComboBox->blockSignals( false );

  registerDataDefinedButton( mNameDDBtn, QgsSymbolLayer::PropertyName );
  registerDataDefinedButton( mFillColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mStrokeColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mStrokeWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
  registerDataDefinedButton( mStrokeStyleDDBtn, QgsSymbolLayer::PropertyStrokeStyle );
  registerDataDefinedButton( mJoinStyleDDBtn, QgsSymbolLayer::PropertyJoinStyle );
  registerDataDefinedButton( mSizeDDBtn, QgsSymbolLayer::PropertySize );
  registerDataDefinedButton( mAngleDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mHorizontalAnchorDDBtn, QgsSymbolLayer::PropertyHorizontalAnchor );
  registerDataDefinedButton( mVerticalAnchorDDBtn, QgsSymbolLayer::PropertyVerticalAnchor );

  updateAssistantSymbol();
}

QgsSymbolLayer *QgsSimpleMarkerSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsSimpleMarkerSymbolLayerWidget::setShape()
{
  mLayer->setShape( static_cast< QgsSimpleMarkerSymbolLayerBase::Shape>( lstNames->currentItem()->data( Qt::UserRole ).toInt() ) );
  btnChangeColorFill->setEnabled( QgsSimpleMarkerSymbolLayerBase::shapeIsFilled( mLayer->shape() ) );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::setColorStroke( const QColor &color )
{
  mLayer->setStrokeColor( color );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::setColorFill( const QColor &color )
{
  mLayer->setColor( color );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::penJoinStyleChanged()
{
  mLayer->setPenJoinStyle( cboJoinStyle->penJoinStyle() );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::setSize()
{
  mLayer->setSize( spinSize->value() );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::setAngle()
{
  mLayer->setAngle( spinAngle->value() );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::setOffset()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsSimpleMarkerSymbolLayerWidget::on_mStrokeStyleComboBox_currentIndexChanged( int index )
{
  Q_UNUSED( index );

  if ( mLayer )
  {
    mLayer->setStrokeStyle( mStrokeStyleComboBox->penStyle() );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mStrokeWidthSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setStrokeWidth( d );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mSizeUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setSizeUnit( mSizeUnitWidget->unit() );
    mLayer->setSizeMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mStrokeWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setStrokeWidthUnit( mStrokeWidthUnitWidget->unit() );
    mLayer->setStrokeWidthMapUnitScale( mStrokeWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mHorizontalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setHorizontalAnchorPoint( ( QgsMarkerSymbolLayer::HorizontalAnchorPoint ) index );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::on_mVerticalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setVerticalAnchorPoint( ( QgsMarkerSymbolLayer::VerticalAnchorPoint ) index );
    emit changed();
  }
}

void QgsSimpleMarkerSymbolLayerWidget::updateAssistantSymbol()
{
  for ( int i = mAssistantPreviewSymbol->symbolLayerCount() - 1 ; i >= 0; --i )
  {
    mAssistantPreviewSymbol->deleteSymbolLayer( i );
  }
  mAssistantPreviewSymbol->appendSymbolLayer( mLayer->clone() );
  QgsProperty ddSize = mLayer->dataDefinedProperties().property( QgsSymbolLayer::PropertySize );
  if ( ddSize )
    mAssistantPreviewSymbol->setDataDefinedSize( ddSize );
}


///////////

QgsSimpleFillSymbolLayerWidget::QgsSimpleFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mStrokeWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                    << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  btnChangeColor->setAllowOpacity( true );
  btnChangeColor->setColorDialogTitle( tr( "Select Fill Color" ) );
  btnChangeColor->setContext( QStringLiteral( "symbology" ) );
  btnChangeColor->setShowNoColor( true );
  btnChangeColor->setNoColorString( tr( "Transparent fill" ) );
  btnChangeStrokeColor->setAllowOpacity( true );
  btnChangeStrokeColor->setColorDialogTitle( tr( "Select Stroke Color" ) );
  btnChangeStrokeColor->setContext( QStringLiteral( "symbology" ) );
  btnChangeStrokeColor->setShowNoColor( true );
  btnChangeStrokeColor->setNoColorString( tr( "Transparent stroke" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );

  connect( btnChangeColor, &QgsColorButton::colorChanged, this, &QgsSimpleFillSymbolLayerWidget::setColor );
  connect( cboFillStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleFillSymbolLayerWidget::setBrushStyle );
  connect( btnChangeStrokeColor, &QgsColorButton::colorChanged, this, &QgsSimpleFillSymbolLayerWidget::setStrokeColor );
  connect( spinStrokeWidth, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleFillSymbolLayerWidget::strokeWidthChanged );
  connect( cboStrokeStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleFillSymbolLayerWidget::strokeStyleChanged );
  connect( cboJoinStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsSimpleFillSymbolLayerWidget::strokeStyleChanged );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleFillSymbolLayerWidget::offsetChanged );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSimpleFillSymbolLayerWidget::offsetChanged );
}

void QgsSimpleFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "SimpleFill" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsSimpleFillSymbolLayer *>( layer );

  // set values
  btnChangeColor->blockSignals( true );
  btnChangeColor->setColor( mLayer->color() );
  btnChangeColor->blockSignals( false );
  cboFillStyle->blockSignals( true );
  cboFillStyle->setBrushStyle( mLayer->brushStyle() );
  cboFillStyle->blockSignals( false );
  btnChangeStrokeColor->blockSignals( true );
  btnChangeStrokeColor->setColor( mLayer->strokeColor() );
  btnChangeStrokeColor->blockSignals( false );
  cboStrokeStyle->blockSignals( true );
  cboStrokeStyle->setPenStyle( mLayer->strokeStyle() );
  cboStrokeStyle->blockSignals( false );
  spinStrokeWidth->blockSignals( true );
  spinStrokeWidth->setValue( mLayer->strokeWidth() );
  spinStrokeWidth->blockSignals( false );
  cboJoinStyle->blockSignals( true );
  cboJoinStyle->setPenJoinStyle( mLayer->penJoinStyle() );
  cboJoinStyle->blockSignals( false );
  spinOffsetX->blockSignals( true );
  spinOffsetX->setValue( mLayer->offset().x() );
  spinOffsetX->blockSignals( false );
  spinOffsetY->blockSignals( true );
  spinOffsetY->setValue( mLayer->offset().y() );
  spinOffsetY->blockSignals( false );

  mStrokeWidthUnitWidget->blockSignals( true );
  mStrokeWidthUnitWidget->setUnit( mLayer->strokeWidthUnit() );
  mStrokeWidthUnitWidget->setMapUnitScale( mLayer->strokeWidthMapUnitScale() );
  mStrokeWidthUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  registerDataDefinedButton( mFillColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mStrokeColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mStrokeWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
  registerDataDefinedButton( mFillStyleDDBtn, QgsSymbolLayer::PropertyFillStyle );
  registerDataDefinedButton( mStrokeStyleDDBtn, QgsSymbolLayer::PropertyStrokeStyle );
  registerDataDefinedButton( mJoinStyleDDBtn, QgsSymbolLayer::PropertyJoinStyle );

}

QgsSymbolLayer *QgsSimpleFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsSimpleFillSymbolLayerWidget::setColor( const QColor &color )
{
  mLayer->setColor( color );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::setStrokeColor( const QColor &color )
{
  mLayer->setStrokeColor( color );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::setBrushStyle()
{
  mLayer->setBrushStyle( cboFillStyle->brushStyle() );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::strokeWidthChanged()
{
  mLayer->setStrokeWidth( spinStrokeWidth->value() );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::strokeStyleChanged()
{
  mLayer->setStrokeStyle( cboStrokeStyle->penStyle() );
  mLayer->setPenJoinStyle( cboJoinStyle->penJoinStyle() );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::offsetChanged()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsSimpleFillSymbolLayerWidget::on_mStrokeWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setStrokeWidthUnit( mStrokeWidthUnitWidget->unit() );
    mLayer->setStrokeWidthMapUnitScale( mStrokeWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSimpleFillSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

///////////

QgsFilledMarkerSymbolLayerWidget::QgsFilledMarkerSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mSizeUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                             << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );
  spinAngle->setClearValue( 0.0 );

  //make a temporary symbol for the size assistant preview
  mAssistantPreviewSymbol.reset( new QgsMarkerSymbol() );

  if ( vectorLayer() )
    mSizeDDBtn->setSymbol( mAssistantPreviewSymbol );

  QSize size = lstNames->iconSize();
  double markerSize = DEFAULT_POINT_SIZE * 2;
  Q_FOREACH ( QgsSimpleMarkerSymbolLayerBase::Shape shape, QgsSimpleMarkerSymbolLayerBase::availableShapes() )
  {
    if ( !QgsSimpleMarkerSymbolLayerBase::shapeIsFilled( shape ) )
      continue;

    QgsSimpleMarkerSymbolLayer *lyr = new QgsSimpleMarkerSymbolLayer( shape, markerSize );
    lyr->setColor( QColor( 200, 200, 200 ) );
    lyr->setStrokeColor( QColor( 0, 0, 0 ) );
    QIcon icon = QgsSymbolLayerUtils::symbolLayerPreviewIcon( lyr, QgsUnitTypes::RenderMillimeters, size );
    QListWidgetItem *item = new QListWidgetItem( icon, QString(), lstNames );
    item->setData( Qt::UserRole, static_cast< int >( shape ) );
    item->setToolTip( QgsSimpleMarkerSymbolLayerBase::encodeShape( shape ) );
    delete lyr;
  }

  connect( lstNames, &QListWidget::currentRowChanged, this, &QgsFilledMarkerSymbolLayerWidget::setShape );
  connect( spinSize, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFilledMarkerSymbolLayerWidget::setSize );
  connect( spinAngle, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFilledMarkerSymbolLayerWidget::setAngle );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFilledMarkerSymbolLayerWidget::setOffset );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFilledMarkerSymbolLayerWidget::setOffset );
  connect( this, &QgsSymbolLayerWidget::changed, this, &QgsFilledMarkerSymbolLayerWidget::updateAssistantSymbol );
}

void QgsFilledMarkerSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "FilledMarker" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsFilledMarkerSymbolLayer *>( layer );

  // set values
  QgsSimpleMarkerSymbolLayerBase::Shape shape = mLayer->shape();
  for ( int i = 0; i < lstNames->count(); ++i )
  {
    if ( static_cast< QgsSimpleMarkerSymbolLayerBase::Shape >( lstNames->item( i )->data( Qt::UserRole ).toInt() ) == shape )
    {
      lstNames->setCurrentRow( i );
      break;
    }
  }
  whileBlocking( spinSize )->setValue( mLayer->size() );
  whileBlocking( spinAngle )->setValue( mLayer->angle() );
  whileBlocking( spinOffsetX )->setValue( mLayer->offset().x() );
  whileBlocking( spinOffsetY )->setValue( mLayer->offset().y() );

  mSizeUnitWidget->blockSignals( true );
  mSizeUnitWidget->setUnit( mLayer->sizeUnit() );
  mSizeUnitWidget->setMapUnitScale( mLayer->sizeMapUnitScale() );
  mSizeUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  //anchor points
  whileBlocking( mHorizontalAnchorComboBox )->setCurrentIndex( mLayer->horizontalAnchorPoint() );
  whileBlocking( mVerticalAnchorComboBox )->setCurrentIndex( mLayer->verticalAnchorPoint() );

  registerDataDefinedButton( mNameDDBtn, QgsSymbolLayer::PropertyName );
  registerDataDefinedButton( mSizeDDBtn, QgsSymbolLayer::PropertySize );
  registerDataDefinedButton( mAngleDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mHorizontalAnchorDDBtn, QgsSymbolLayer::PropertyHorizontalAnchor );
  registerDataDefinedButton( mVerticalAnchorDDBtn, QgsSymbolLayer::PropertyVerticalAnchor );

  updateAssistantSymbol();
}

QgsSymbolLayer *QgsFilledMarkerSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsFilledMarkerSymbolLayerWidget::setShape()
{
  mLayer->setShape( static_cast< QgsSimpleMarkerSymbolLayerBase::Shape>( lstNames->currentItem()->data( Qt::UserRole ).toInt() ) );
  emit changed();
}

void QgsFilledMarkerSymbolLayerWidget::setSize()
{
  mLayer->setSize( spinSize->value() );
  emit changed();
}

void QgsFilledMarkerSymbolLayerWidget::setAngle()
{
  mLayer->setAngle( spinAngle->value() );
  emit changed();
}

void QgsFilledMarkerSymbolLayerWidget::setOffset()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsFilledMarkerSymbolLayerWidget::on_mSizeUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setSizeUnit( mSizeUnitWidget->unit() );
    mLayer->setSizeMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsFilledMarkerSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsFilledMarkerSymbolLayerWidget::on_mHorizontalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setHorizontalAnchorPoint( ( QgsMarkerSymbolLayer::HorizontalAnchorPoint ) index );
    emit changed();
  }
}

void QgsFilledMarkerSymbolLayerWidget::on_mVerticalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setVerticalAnchorPoint( ( QgsMarkerSymbolLayer::VerticalAnchorPoint ) index );
    emit changed();
  }
}

void QgsFilledMarkerSymbolLayerWidget::updateAssistantSymbol()
{
  for ( int i = mAssistantPreviewSymbol->symbolLayerCount() - 1 ; i >= 0; --i )
  {
    mAssistantPreviewSymbol->deleteSymbolLayer( i );
  }
  mAssistantPreviewSymbol->appendSymbolLayer( mLayer->clone() );
  QgsProperty ddSize = mLayer->dataDefinedProperties().property( QgsSymbolLayer::PropertySize );
  if ( ddSize )
    mAssistantPreviewSymbol->setDataDefinedSize( ddSize );
}


///////////

QgsGradientFillSymbolLayerWidget::QgsGradientFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  btnColorRamp->setShowGradientOnly( true );

  btnChangeColor->setAllowOpacity( true );
  btnChangeColor->setColorDialogTitle( tr( "Select Gradient Color" ) );
  btnChangeColor->setContext( QStringLiteral( "symbology" ) );
  btnChangeColor->setShowNoColor( true );
  btnChangeColor->setNoColorString( tr( "Transparent" ) );
  btnChangeColor2->setAllowOpacity( true );
  btnChangeColor2->setColorDialogTitle( tr( "Select Gradient Color" ) );
  btnChangeColor2->setContext( QStringLiteral( "symbology" ) );
  btnChangeColor2->setShowNoColor( true );
  btnChangeColor2->setNoColorString( tr( "Transparent" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );
  mSpinAngle->setClearValue( 0.0 );

  connect( btnChangeColor, &QgsColorButton::colorChanged, this, &QgsGradientFillSymbolLayerWidget::setColor );
  connect( btnChangeColor2, &QgsColorButton::colorChanged, this, &QgsGradientFillSymbolLayerWidget::setColor2 );
  connect( btnColorRamp, &QgsColorRampButton::colorRampChanged, this, &QgsGradientFillSymbolLayerWidget::applyColorRamp );
  connect( cboGradientType, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsGradientFillSymbolLayerWidget::setGradientType );
  connect( cboCoordinateMode, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsGradientFillSymbolLayerWidget::setCoordinateMode );
  connect( cboGradientSpread, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsGradientFillSymbolLayerWidget::setGradientSpread );
  connect( radioTwoColor, &QAbstractButton::toggled, this, &QgsGradientFillSymbolLayerWidget::colorModeChanged );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::offsetChanged );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::offsetChanged );
  connect( spinRefPoint1X, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
  connect( spinRefPoint1Y, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
  connect( checkRefPoint1Centroid, &QAbstractButton::toggled, this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
  connect( spinRefPoint2X, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
  connect( spinRefPoint2Y, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
  connect( checkRefPoint2Centroid, &QAbstractButton::toggled, this, &QgsGradientFillSymbolLayerWidget::referencePointChanged );
}

void QgsGradientFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "GradientFill" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsGradientFillSymbolLayer *>( layer );

  // set values
  btnChangeColor->blockSignals( true );
  btnChangeColor->setColor( mLayer->color() );
  btnChangeColor->blockSignals( false );
  btnChangeColor2->blockSignals( true );
  btnChangeColor2->setColor( mLayer->color2() );
  btnChangeColor2->blockSignals( false );

  if ( mLayer->gradientColorType() == QgsGradientFillSymbolLayer::SimpleTwoColor )
  {
    radioTwoColor->setChecked( true );
    btnColorRamp->setEnabled( false );
  }
  else
  {
    radioColorRamp->setChecked( true );
    btnChangeColor->setEnabled( false );
    btnChangeColor2->setEnabled( false );
  }

  // set source color ramp
  if ( mLayer->colorRamp() )
  {
    btnColorRamp->blockSignals( true );
    btnColorRamp->setColorRamp( mLayer->colorRamp() );
    btnColorRamp->blockSignals( false );
  }

  cboGradientType->blockSignals( true );
  switch ( mLayer->gradientType() )
  {
    case QgsGradientFillSymbolLayer::Linear:
      cboGradientType->setCurrentIndex( 0 );
      break;
    case QgsGradientFillSymbolLayer::Radial:
      cboGradientType->setCurrentIndex( 1 );
      break;
    case QgsGradientFillSymbolLayer::Conical:
      cboGradientType->setCurrentIndex( 2 );
      break;
  }
  cboGradientType->blockSignals( false );

  cboCoordinateMode->blockSignals( true );
  switch ( mLayer->coordinateMode() )
  {
    case QgsGradientFillSymbolLayer::Viewport:
      cboCoordinateMode->setCurrentIndex( 1 );
      checkRefPoint1Centroid->setEnabled( false );
      checkRefPoint2Centroid->setEnabled( false );
      break;
    case QgsGradientFillSymbolLayer::Feature:
    default:
      cboCoordinateMode->setCurrentIndex( 0 );
      break;
  }
  cboCoordinateMode->blockSignals( false );

  cboGradientSpread->blockSignals( true );
  switch ( mLayer->gradientSpread() )
  {
    case QgsGradientFillSymbolLayer::Pad:
      cboGradientSpread->setCurrentIndex( 0 );
      break;
    case QgsGradientFillSymbolLayer::Repeat:
      cboGradientSpread->setCurrentIndex( 1 );
      break;
    case QgsGradientFillSymbolLayer::Reflect:
      cboGradientSpread->setCurrentIndex( 2 );
      break;
  }
  cboGradientSpread->blockSignals( false );

  spinRefPoint1X->blockSignals( true );
  spinRefPoint1X->setValue( mLayer->referencePoint1().x() );
  spinRefPoint1X->blockSignals( false );
  spinRefPoint1Y->blockSignals( true );
  spinRefPoint1Y->setValue( mLayer->referencePoint1().y() );
  spinRefPoint1Y->blockSignals( false );
  checkRefPoint1Centroid->blockSignals( true );
  checkRefPoint1Centroid->setChecked( mLayer->referencePoint1IsCentroid() );
  if ( mLayer->referencePoint1IsCentroid() )
  {
    spinRefPoint1X->setEnabled( false );
    spinRefPoint1Y->setEnabled( false );
  }
  checkRefPoint1Centroid->blockSignals( false );
  spinRefPoint2X->blockSignals( true );
  spinRefPoint2X->setValue( mLayer->referencePoint2().x() );
  spinRefPoint2X->blockSignals( false );
  spinRefPoint2Y->blockSignals( true );
  spinRefPoint2Y->setValue( mLayer->referencePoint2().y() );
  spinRefPoint2Y->blockSignals( false );
  checkRefPoint2Centroid->blockSignals( true );
  checkRefPoint2Centroid->setChecked( mLayer->referencePoint2IsCentroid() );
  if ( mLayer->referencePoint2IsCentroid() )
  {
    spinRefPoint2X->setEnabled( false );
    spinRefPoint2Y->setEnabled( false );
  }
  checkRefPoint2Centroid->blockSignals( false );

  spinOffsetX->blockSignals( true );
  spinOffsetX->setValue( mLayer->offset().x() );
  spinOffsetX->blockSignals( false );
  spinOffsetY->blockSignals( true );
  spinOffsetY->setValue( mLayer->offset().y() );
  spinOffsetY->blockSignals( false );
  mSpinAngle->blockSignals( true );
  mSpinAngle->setValue( mLayer->angle() );
  mSpinAngle->blockSignals( false );

  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  registerDataDefinedButton( mStartColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mEndColorDDBtn, QgsSymbolLayer::PropertySecondaryColor );
  registerDataDefinedButton( mAngleDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mGradientTypeDDBtn, QgsSymbolLayer::PropertyGradientType );
  registerDataDefinedButton( mCoordinateModeDDBtn, QgsSymbolLayer::PropertyCoordinateMode );
  registerDataDefinedButton( mSpreadDDBtn, QgsSymbolLayer::PropertyGradientSpread );
  registerDataDefinedButton( mRefPoint1XDDBtn, QgsSymbolLayer::PropertyGradientReference1X );
  registerDataDefinedButton( mRefPoint1YDDBtn, QgsSymbolLayer::PropertyGradientReference1Y );
  registerDataDefinedButton( mRefPoint1CentroidDDBtn, QgsSymbolLayer::PropertyGradientReference1IsCentroid );
  registerDataDefinedButton( mRefPoint2XDDBtn, QgsSymbolLayer::PropertyGradientReference2X );
  registerDataDefinedButton( mRefPoint2YDDBtn, QgsSymbolLayer::PropertyGradientReference2Y );
  registerDataDefinedButton( mRefPoint2CentroidDDBtn, QgsSymbolLayer::PropertyGradientReference2IsCentroid );
}

QgsSymbolLayer *QgsGradientFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsGradientFillSymbolLayerWidget::setColor( const QColor &color )
{
  mLayer->setColor( color );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::setColor2( const QColor &color )
{
  mLayer->setColor2( color );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::colorModeChanged()
{
  if ( radioTwoColor->isChecked() )
  {
    mLayer->setGradientColorType( QgsGradientFillSymbolLayer::SimpleTwoColor );
  }
  else
  {
    mLayer->setGradientColorType( QgsGradientFillSymbolLayer::ColorRamp );
  }
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::applyColorRamp()
{
  if ( btnColorRamp->isNull() )
    return;

  mLayer->setColorRamp( btnColorRamp->colorRamp()->clone() );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::setGradientType( int index )
{
  switch ( index )
  {
    case 0:
      mLayer->setGradientType( QgsGradientFillSymbolLayer::Linear );
      //set sensible default reference points
      spinRefPoint1X->setValue( 0.5 );
      spinRefPoint1Y->setValue( 0 );
      spinRefPoint2X->setValue( 0.5 );
      spinRefPoint2Y->setValue( 1 );
      break;
    case 1:
      mLayer->setGradientType( QgsGradientFillSymbolLayer::Radial );
      //set sensible default reference points
      spinRefPoint1X->setValue( 0 );
      spinRefPoint1Y->setValue( 0 );
      spinRefPoint2X->setValue( 1 );
      spinRefPoint2Y->setValue( 1 );
      break;
    case 2:
      mLayer->setGradientType( QgsGradientFillSymbolLayer::Conical );
      spinRefPoint1X->setValue( 0.5 );
      spinRefPoint1Y->setValue( 0.5 );
      spinRefPoint2X->setValue( 1 );
      spinRefPoint2Y->setValue( 1 );
      break;
  }
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::setCoordinateMode( int index )
{

  switch ( index )
  {
    case 0:
      //feature coordinate mode
      mLayer->setCoordinateMode( QgsGradientFillSymbolLayer::Feature );
      //allow choice of centroid reference positions
      checkRefPoint1Centroid->setEnabled( true );
      checkRefPoint2Centroid->setEnabled( true );
      break;
    case 1:
      //viewport coordinate mode
      mLayer->setCoordinateMode( QgsGradientFillSymbolLayer::Viewport );
      //disable choice of centroid reference positions
      checkRefPoint1Centroid->setChecked( Qt::Unchecked );
      checkRefPoint1Centroid->setEnabled( false );
      checkRefPoint2Centroid->setChecked( Qt::Unchecked );
      checkRefPoint2Centroid->setEnabled( false );
      break;
  }

  emit changed();
}

void QgsGradientFillSymbolLayerWidget::setGradientSpread( int index )
{
  switch ( index )
  {
    case 0:
      mLayer->setGradientSpread( QgsGradientFillSymbolLayer::Pad );
      break;
    case 1:
      mLayer->setGradientSpread( QgsGradientFillSymbolLayer::Repeat );
      break;
    case 2:
      mLayer->setGradientSpread( QgsGradientFillSymbolLayer::Reflect );
      break;
  }

  emit changed();
}

void QgsGradientFillSymbolLayerWidget::offsetChanged()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::referencePointChanged()
{
  mLayer->setReferencePoint1( QPointF( spinRefPoint1X->value(), spinRefPoint1Y->value() ) );
  mLayer->setReferencePoint1IsCentroid( checkRefPoint1Centroid->isChecked() );
  mLayer->setReferencePoint2( QPointF( spinRefPoint2X->value(), spinRefPoint2Y->value() ) );
  mLayer->setReferencePoint2IsCentroid( checkRefPoint2Centroid->isChecked() );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::on_mSpinAngle_valueChanged( double value )
{
  mLayer->setAngle( value );
  emit changed();
}

void QgsGradientFillSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

///////////

QgsShapeburstFillSymbolLayerWidget::QgsShapeburstFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mDistanceUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                 << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  QButtonGroup *group1 = new QButtonGroup( this );
  group1->addButton( radioColorRamp );
  group1->addButton( radioTwoColor );
  QButtonGroup *group2 = new QButtonGroup( this );
  group2->addButton( mRadioUseMaxDistance );
  group2->addButton( mRadioUseWholeShape );
  btnChangeColor->setAllowOpacity( true );
  btnChangeColor->setColorDialogTitle( tr( "Select Gradient color" ) );
  btnChangeColor->setContext( QStringLiteral( "symbology" ) );
  btnChangeColor->setShowNoColor( true );
  btnChangeColor->setNoColorString( tr( "Transparent" ) );
  btnChangeColor2->setAllowOpacity( true );
  btnChangeColor2->setColorDialogTitle( tr( "Select Gradient color" ) );
  btnChangeColor2->setContext( QStringLiteral( "symbology" ) );
  btnChangeColor2->setShowNoColor( true );
  btnChangeColor2->setNoColorString( tr( "Transparent" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );

  btnColorRamp->setShowGradientOnly( true );

  connect( btnColorRamp, &QgsColorRampButton::colorRampChanged, this, &QgsShapeburstFillSymbolLayerWidget::applyColorRamp );

  connect( btnChangeColor, &QgsColorButton::colorChanged, this, &QgsShapeburstFillSymbolLayerWidget::setColor );
  connect( btnChangeColor2, &QgsColorButton::colorChanged, this, &QgsShapeburstFillSymbolLayerWidget::setColor2 );
  connect( radioTwoColor, &QAbstractButton::toggled, this, &QgsShapeburstFillSymbolLayerWidget::colorModeChanged );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsShapeburstFillSymbolLayerWidget::offsetChanged );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsShapeburstFillSymbolLayerWidget::offsetChanged );

  connect( mBlurSlider, &QAbstractSlider::valueChanged, mSpinBlurRadius, &QSpinBox::setValue );
  connect( mSpinBlurRadius, static_cast < void ( QSpinBox::* )( int ) > ( &QSpinBox::valueChanged ), mBlurSlider, &QAbstractSlider::setValue );
}

void QgsShapeburstFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "ShapeburstFill" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsShapeburstFillSymbolLayer *>( layer );

  // set values
  btnChangeColor->blockSignals( true );
  btnChangeColor->setColor( mLayer->color() );
  btnChangeColor->blockSignals( false );
  btnChangeColor2->blockSignals( true );
  btnChangeColor2->setColor( mLayer->color2() );
  btnChangeColor2->blockSignals( false );

  if ( mLayer->colorType() == QgsShapeburstFillSymbolLayer::SimpleTwoColor )
  {
    radioTwoColor->setChecked( true );
    btnColorRamp->setEnabled( false );
  }
  else
  {
    radioColorRamp->setChecked( true );
    btnChangeColor->setEnabled( false );
    btnChangeColor2->setEnabled( false );
  }

  mSpinBlurRadius->blockSignals( true );
  mBlurSlider->blockSignals( true );
  mSpinBlurRadius->setValue( mLayer->blurRadius() );
  mBlurSlider->setValue( mLayer->blurRadius() );
  mSpinBlurRadius->blockSignals( false );
  mBlurSlider->blockSignals( false );

  mSpinMaxDistance->blockSignals( true );
  mSpinMaxDistance->setValue( mLayer->maxDistance() );
  mSpinMaxDistance->blockSignals( false );

  mRadioUseWholeShape->blockSignals( true );
  mRadioUseMaxDistance->blockSignals( true );
  if ( mLayer->useWholeShape() )
  {
    mRadioUseWholeShape->setChecked( true );
    mSpinMaxDistance->setEnabled( false );
    mDistanceUnitWidget->setEnabled( false );
  }
  else
  {
    mRadioUseMaxDistance->setChecked( true );
    mSpinMaxDistance->setEnabled( true );
    mDistanceUnitWidget->setEnabled( true );
  }
  mRadioUseWholeShape->blockSignals( false );
  mRadioUseMaxDistance->blockSignals( false );

  mDistanceUnitWidget->blockSignals( true );
  mDistanceUnitWidget->setUnit( mLayer->distanceUnit() );
  mDistanceUnitWidget->setMapUnitScale( mLayer->distanceMapUnitScale() );
  mDistanceUnitWidget->blockSignals( false );

  mIgnoreRingsCheckBox->blockSignals( true );
  mIgnoreRingsCheckBox->setCheckState( mLayer->ignoreRings() ? Qt::Checked : Qt::Unchecked );
  mIgnoreRingsCheckBox->blockSignals( false );

  // set source color ramp
  if ( mLayer->colorRamp() )
  {
    btnColorRamp->blockSignals( true );
    btnColorRamp->setColorRamp( mLayer->colorRamp() );
    btnColorRamp->blockSignals( false );
  }

  spinOffsetX->blockSignals( true );
  spinOffsetX->setValue( mLayer->offset().x() );
  spinOffsetX->blockSignals( false );
  spinOffsetY->blockSignals( true );
  spinOffsetY->setValue( mLayer->offset().y() );
  spinOffsetY->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  registerDataDefinedButton( mStartColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mEndColorDDBtn, QgsSymbolLayer::PropertySecondaryColor );
  registerDataDefinedButton( mBlurRadiusDDBtn, QgsSymbolLayer::PropertyBlurRadius );
  registerDataDefinedButton( mShadeWholeShapeDDBtn, QgsSymbolLayer::PropertyShapeburstUseWholeShape );
  registerDataDefinedButton( mShadeDistanceDDBtn, QgsSymbolLayer::PropertyShapeburstMaxDistance );
  registerDataDefinedButton( mIgnoreRingsDDBtn, QgsSymbolLayer::PropertyShapeburstIgnoreRings );
}

QgsSymbolLayer *QgsShapeburstFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsShapeburstFillSymbolLayerWidget::setColor( const QColor &color )
{
  if ( mLayer )
  {
    mLayer->setColor( color );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::setColor2( const QColor &color )
{
  if ( mLayer )
  {
    mLayer->setColor2( color );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::colorModeChanged()
{
  if ( !mLayer )
  {
    return;
  }

  if ( radioTwoColor->isChecked() )
  {
    mLayer->setColorType( QgsShapeburstFillSymbolLayer::SimpleTwoColor );
  }
  else
  {
    mLayer->setColorType( QgsShapeburstFillSymbolLayer::ColorRamp );
  }
  emit changed();
}

void QgsShapeburstFillSymbolLayerWidget::on_mSpinBlurRadius_valueChanged( int value )
{
  if ( mLayer )
  {
    mLayer->setBlurRadius( value );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::on_mSpinMaxDistance_valueChanged( double value )
{
  if ( mLayer )
  {
    mLayer->setMaxDistance( value );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::on_mDistanceUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDistanceUnit( mDistanceUnitWidget->unit() );
    mLayer->setDistanceMapUnitScale( mDistanceUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::on_mRadioUseWholeShape_toggled( bool value )
{
  if ( mLayer )
  {
    mLayer->setUseWholeShape( value );
    mDistanceUnitWidget->setEnabled( !value );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::applyColorRamp()
{
  QgsColorRamp *ramp = btnColorRamp->colorRamp();
  if ( !ramp )
    return;

  mLayer->setColorRamp( ramp );
  emit changed();
}

void QgsShapeburstFillSymbolLayerWidget::offsetChanged()
{
  if ( mLayer )
  {
    mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
    emit changed();
  }
}

void QgsShapeburstFillSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}


void QgsShapeburstFillSymbolLayerWidget::on_mIgnoreRingsCheckBox_stateChanged( int state )
{
  bool checked = ( state == Qt::Checked );
  mLayer->setIgnoreRings( checked );
  emit changed();
}

///////////

QgsMarkerLineSymbolLayerWidget::QgsMarkerLineSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mIntervalUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                 << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetAlongLineUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                        << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  spinOffset->setClearValue( 0.0 );

  connect( spinInterval, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsMarkerLineSymbolLayerWidget::setInterval );
  connect( mSpinOffsetAlongLine, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsMarkerLineSymbolLayerWidget::setOffsetAlongLine );
  connect( chkRotateMarker, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setRotate );
  connect( spinOffset, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsMarkerLineSymbolLayerWidget::setOffset );
  connect( radInterval, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
  connect( radVertex, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
  connect( radVertexLast, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
  connect( radVertexFirst, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
  connect( radCentralPoint, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
  connect( radCurvePoint, &QAbstractButton::clicked, this, &QgsMarkerLineSymbolLayerWidget::setPlacement );
}

void QgsMarkerLineSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "MarkerLine" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsMarkerLineSymbolLayer *>( layer );

  // set values
  spinInterval->blockSignals( true );
  spinInterval->setValue( mLayer->interval() );
  spinInterval->blockSignals( false );
  mSpinOffsetAlongLine->blockSignals( true );
  mSpinOffsetAlongLine->setValue( mLayer->offsetAlongLine() );
  mSpinOffsetAlongLine->blockSignals( false );
  chkRotateMarker->blockSignals( true );
  chkRotateMarker->setChecked( mLayer->rotateMarker() );
  chkRotateMarker->blockSignals( false );
  spinOffset->blockSignals( true );
  spinOffset->setValue( mLayer->offset() );
  spinOffset->blockSignals( false );
  if ( mLayer->placement() == QgsMarkerLineSymbolLayer::Interval )
    radInterval->setChecked( true );
  else if ( mLayer->placement() == QgsMarkerLineSymbolLayer::Vertex )
    radVertex->setChecked( true );
  else if ( mLayer->placement() == QgsMarkerLineSymbolLayer::LastVertex )
    radVertexLast->setChecked( true );
  else if ( mLayer->placement() == QgsMarkerLineSymbolLayer::CentralPoint )
    radCentralPoint->setChecked( true );
  else if ( mLayer->placement() == QgsMarkerLineSymbolLayer::CurvePoint )
    radCurvePoint->setChecked( true );
  else
    radVertexFirst->setChecked( true );

  // set units
  mIntervalUnitWidget->blockSignals( true );
  mIntervalUnitWidget->setUnit( mLayer->intervalUnit() );
  mIntervalUnitWidget->setMapUnitScale( mLayer->intervalMapUnitScale() );
  mIntervalUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );
  mOffsetAlongLineUnitWidget->blockSignals( true );
  mOffsetAlongLineUnitWidget->setUnit( mLayer->offsetAlongLineUnit() );
  mOffsetAlongLineUnitWidget->setMapUnitScale( mLayer->offsetAlongLineMapUnitScale() );
  mOffsetAlongLineUnitWidget->blockSignals( false );

  setPlacement(); // update gui

  registerDataDefinedButton( mIntervalDDBtn, QgsSymbolLayer::PropertyInterval );
  registerDataDefinedButton( mLineOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mPlacementDDBtn, QgsSymbolLayer::PropertyPlacement );
  registerDataDefinedButton( mOffsetAlongLineDDBtn, QgsSymbolLayer::PropertyOffsetAlongLine );
}

QgsSymbolLayer *QgsMarkerLineSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsMarkerLineSymbolLayerWidget::setInterval( double val )
{
  mLayer->setInterval( val );
  emit changed();
}

void QgsMarkerLineSymbolLayerWidget::setOffsetAlongLine( double val )
{
  mLayer->setOffsetAlongLine( val );
  emit changed();
}

void QgsMarkerLineSymbolLayerWidget::setRotate()
{
  mLayer->setRotateMarker( chkRotateMarker->isChecked() );
  emit changed();
}

void QgsMarkerLineSymbolLayerWidget::setOffset()
{
  mLayer->setOffset( spinOffset->value() );
  emit changed();
}

void QgsMarkerLineSymbolLayerWidget::setPlacement()
{
  bool interval = radInterval->isChecked();
  spinInterval->setEnabled( interval );
  mSpinOffsetAlongLine->setEnabled( radInterval->isChecked() || radVertexLast->isChecked() || radVertexFirst->isChecked() );
  //mLayer->setPlacement( interval ? QgsMarkerLineSymbolLayer::Interval : QgsMarkerLineSymbolLayer::Vertex );
  if ( radInterval->isChecked() )
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::Interval );
  else if ( radVertex->isChecked() )
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::Vertex );
  else if ( radVertexLast->isChecked() )
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::LastVertex );
  else if ( radVertexFirst->isChecked() )
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::FirstVertex );
  else if ( radCurvePoint->isChecked() )
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::CurvePoint );
  else
    mLayer->setPlacement( QgsMarkerLineSymbolLayer::CentralPoint );

  emit changed();
}

void QgsMarkerLineSymbolLayerWidget::on_mIntervalUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setIntervalUnit( mIntervalUnitWidget->unit() );
    mLayer->setIntervalMapUnitScale( mIntervalUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsMarkerLineSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsMarkerLineSymbolLayerWidget::on_mOffsetAlongLineUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetAlongLineUnit( mOffsetAlongLineUnitWidget->unit() );
    mLayer->setOffsetAlongLineMapUnitScale( mOffsetAlongLineUnitWidget->getMapUnitScale() );
  }
  emit changed();
}

///////////


QgsSvgMarkerSymbolLayerWidget::QgsSvgMarkerSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mSizeUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                             << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mStrokeWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                    << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  viewGroups->setHeaderHidden( true );
  mChangeColorButton->setAllowOpacity( true );
  mChangeColorButton->setColorDialogTitle( tr( "Select Fill color" ) );
  mChangeColorButton->setContext( QStringLiteral( "symbology" ) );
  mChangeStrokeColorButton->setAllowOpacity( true );
  mChangeStrokeColorButton->setColorDialogTitle( tr( "Select Stroke Color" ) );
  mChangeStrokeColorButton->setContext( QStringLiteral( "symbology" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );
  spinAngle->setClearValue( 0.0 );

  mIconSize = std::max( 30, static_cast< int >( std::round( Qgis::UI_SCALE_FACTOR * fontMetrics().width( QStringLiteral( "XXXX" ) ) ) ) );
  viewImages->setGridSize( QSize( mIconSize * 1.2, mIconSize * 1.2 ) );

  populateList();

  connect( viewImages->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSvgMarkerSymbolLayerWidget::setName );
  connect( viewGroups->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSvgMarkerSymbolLayerWidget::populateIcons );
  connect( spinSize, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSvgMarkerSymbolLayerWidget::setSize );
  connect( spinAngle, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSvgMarkerSymbolLayerWidget::setAngle );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSvgMarkerSymbolLayerWidget::setOffset );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsSvgMarkerSymbolLayerWidget::setOffset );
  connect( this, &QgsSymbolLayerWidget::changed, this, &QgsSvgMarkerSymbolLayerWidget::updateAssistantSymbol );

  //make a temporary symbol for the size assistant preview
  mAssistantPreviewSymbol.reset( new QgsMarkerSymbol() );

  if ( vectorLayer() )
    mSizeDDBtn->setSymbol( mAssistantPreviewSymbol );
}

#include <QTime>
#include <QAbstractListModel>
#include <QPixmapCache>
#include <QStyle>


void QgsSvgMarkerSymbolLayerWidget::populateList()
{
  QAbstractItemModel *oldModel = viewGroups->model();
  QgsSvgSelectorGroupsModel *g = new QgsSvgSelectorGroupsModel( viewGroups );
  viewGroups->setModel( g );
  delete oldModel;

  // Set the tree expanded at the first level
  int rows = g->rowCount( g->indexFromItem( g->invisibleRootItem() ) );
  for ( int i = 0; i < rows; i++ )
  {
    viewGroups->setExpanded( g->indexFromItem( g->item( i ) ), true );
  }

  // Initially load the icons in the List view without any grouping
  oldModel = viewImages->model();
  QgsSvgSelectorListModel *m = new QgsSvgSelectorListModel( viewImages, mIconSize );
  viewImages->setModel( m );
  delete oldModel;
}

void QgsSvgMarkerSymbolLayerWidget::populateIcons( const QModelIndex &idx )
{
  QString path = idx.data( Qt::UserRole + 1 ).toString();

  QAbstractItemModel *oldModel = viewImages->model();
  QgsSvgSelectorListModel *m = new QgsSvgSelectorListModel( viewImages, path );
  viewImages->setModel( m );
  delete oldModel;

  connect( viewImages->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSvgMarkerSymbolLayerWidget::setName );
}

void QgsSvgMarkerSymbolLayerWidget::setGuiForSvg( const QgsSvgMarkerSymbolLayer *layer )
{
  if ( !layer )
  {
    return;
  }

  //activate gui for svg parameters only if supported by the svg file
  bool hasFillParam, hasFillOpacityParam, hasStrokeParam, hasStrokeWidthParam, hasStrokeOpacityParam;
  QColor defaultFill, defaultStroke;
  double defaultStrokeWidth, defaultFillOpacity, defaultStrokeOpacity;
  bool hasDefaultFillColor, hasDefaultFillOpacity, hasDefaultStrokeColor, hasDefaultStrokeWidth, hasDefaultStrokeOpacity;
  QgsApplication::svgCache()->containsParams( layer->path(), hasFillParam, hasDefaultFillColor, defaultFill,
      hasFillOpacityParam, hasDefaultFillOpacity, defaultFillOpacity,
      hasStrokeParam, hasDefaultStrokeColor, defaultStroke,
      hasStrokeWidthParam, hasDefaultStrokeWidth, defaultStrokeWidth,
      hasStrokeOpacityParam, hasDefaultStrokeOpacity, defaultStrokeOpacity );
  mChangeColorButton->setEnabled( hasFillParam );
  mChangeColorButton->setAllowOpacity( hasFillOpacityParam );
  mChangeStrokeColorButton->setEnabled( hasStrokeParam );
  mChangeStrokeColorButton->setAllowOpacity( hasStrokeOpacityParam );
  mStrokeWidthSpinBox->setEnabled( hasStrokeWidthParam );

  if ( hasFillParam )
  {
    QColor fill = layer->fillColor();
    double existingOpacity = hasFillOpacityParam ? fill.alphaF() : 1.0;
    if ( hasDefaultFillColor )
    {
      fill = defaultFill;
    }
    fill.setAlphaF( hasDefaultFillOpacity ? defaultFillOpacity : existingOpacity );
    mChangeColorButton->setColor( fill );
  }
  if ( hasStrokeParam )
  {
    QColor stroke = layer->strokeColor();
    double existingOpacity = hasStrokeOpacityParam ? stroke.alphaF() : 1.0;
    if ( hasDefaultStrokeColor )
    {
      stroke = defaultStroke;
    }
    stroke.setAlphaF( hasDefaultStrokeOpacity ? defaultStrokeOpacity : existingOpacity );
    mChangeStrokeColorButton->setColor( stroke );
  }

  mFileLineEdit->blockSignals( true );
  mFileLineEdit->setText( layer->path() );
  mFileLineEdit->blockSignals( false );

  mStrokeWidthSpinBox->blockSignals( true );
  mStrokeWidthSpinBox->setValue( hasDefaultStrokeWidth ? defaultStrokeWidth : layer->strokeWidth() );
  mStrokeWidthSpinBox->blockSignals( false );
}

void QgsSvgMarkerSymbolLayerWidget::updateAssistantSymbol()
{
  for ( int i = mAssistantPreviewSymbol->symbolLayerCount() - 1 ; i >= 0; --i )
  {
    mAssistantPreviewSymbol->deleteSymbolLayer( i );
  }
  mAssistantPreviewSymbol->appendSymbolLayer( mLayer->clone() );
  QgsProperty ddSize = mLayer->dataDefinedProperties().property( QgsSymbolLayer::PropertySize );
  if ( ddSize )
    mAssistantPreviewSymbol->setDataDefinedSize( ddSize );
}


void QgsSvgMarkerSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( !layer )
  {
    return;
  }

  if ( layer->layerType() != QLatin1String( "SvgMarker" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsSvgMarkerSymbolLayer *>( layer );

  // set values

  QAbstractItemModel *m = viewImages->model();
  QItemSelectionModel *selModel = viewImages->selectionModel();
  for ( int i = 0; i < m->rowCount(); i++ )
  {
    QModelIndex idx( m->index( i, 0 ) );
    if ( m->data( idx ).toString() == mLayer->path() )
    {
      selModel->select( idx, QItemSelectionModel::SelectCurrent );
      selModel->setCurrentIndex( idx, QItemSelectionModel::SelectCurrent );
      setName( idx );
      break;
    }
  }

  spinSize->blockSignals( true );
  spinSize->setValue( mLayer->size() );
  spinSize->blockSignals( false );
  spinAngle->blockSignals( true );
  spinAngle->setValue( mLayer->angle() );
  spinAngle->blockSignals( false );

  // without blocking signals the value gets changed because of slot setOffset()
  spinOffsetX->blockSignals( true );
  spinOffsetX->setValue( mLayer->offset().x() );
  spinOffsetX->blockSignals( false );
  spinOffsetY->blockSignals( true );
  spinOffsetY->setValue( mLayer->offset().y() );
  spinOffsetY->blockSignals( false );

  mSizeUnitWidget->blockSignals( true );
  mSizeUnitWidget->setUnit( mLayer->sizeUnit() );
  mSizeUnitWidget->setMapUnitScale( mLayer->sizeMapUnitScale() );
  mSizeUnitWidget->blockSignals( false );
  mStrokeWidthUnitWidget->blockSignals( true );
  mStrokeWidthUnitWidget->setUnit( mLayer->strokeWidthUnit() );
  mStrokeWidthUnitWidget->setMapUnitScale( mLayer->strokeWidthMapUnitScale() );
  mStrokeWidthUnitWidget->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  //anchor points
  mHorizontalAnchorComboBox->blockSignals( true );
  mVerticalAnchorComboBox->blockSignals( true );
  mHorizontalAnchorComboBox->setCurrentIndex( mLayer->horizontalAnchorPoint() );
  mVerticalAnchorComboBox->setCurrentIndex( mLayer->verticalAnchorPoint() );
  mHorizontalAnchorComboBox->blockSignals( false );
  mVerticalAnchorComboBox->blockSignals( false );

  setGuiForSvg( mLayer );

  registerDataDefinedButton( mSizeDDBtn, QgsSymbolLayer::PropertySize );
  registerDataDefinedButton( mStrokeWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
  registerDataDefinedButton( mAngleDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mFilenameDDBtn, QgsSymbolLayer::PropertyName );
  registerDataDefinedButton( mFillColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mStrokeColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mHorizontalAnchorDDBtn, QgsSymbolLayer::PropertyHorizontalAnchor );
  registerDataDefinedButton( mVerticalAnchorDDBtn, QgsSymbolLayer::PropertyVerticalAnchor );

  updateAssistantSymbol();
}

QgsSymbolLayer *QgsSvgMarkerSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsSvgMarkerSymbolLayerWidget::setName( const QModelIndex &idx )
{
  QString name = idx.data( Qt::UserRole ).toString();
  mLayer->setPath( name );
  mFileLineEdit->setText( name );

  setGuiForSvg( mLayer );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::setSize()
{
  mLayer->setSize( spinSize->value() );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::setAngle()
{
  mLayer->setAngle( spinAngle->value() );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::setOffset()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mFileToolButton_clicked()
{
  QgsSettings s;
  QString file = QFileDialog::getOpenFileName( nullptr,
                 tr( "Select SVG file" ),
                 s.value( QStringLiteral( "/UI/lastSVGMarkerDir" ), QDir::homePath() ).toString(),
                 tr( "SVG files" ) + " (*.svg)" );
  QFileInfo fi( file );
  if ( file.isEmpty() || !fi.exists() )
  {
    return;
  }
  mFileLineEdit->setText( file );
  mLayer->setPath( file );
  s.setValue( QStringLiteral( "/UI/lastSVGMarkerDir" ), fi.absolutePath() );
  setGuiForSvg( mLayer );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mFileLineEdit_textEdited( const QString &text )
{
  if ( !QFileInfo::exists( text ) )
  {
    return;
  }
  mLayer->setPath( text );
  setGuiForSvg( mLayer );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mFileLineEdit_editingFinished()
{
  if ( !QFileInfo::exists( mFileLineEdit->text() ) )
  {
    QUrl url( mFileLineEdit->text() );
    if ( !url.isValid() )
    {
      return;
    }
  }

  QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
  mLayer->setPath( mFileLineEdit->text() );
  QApplication::restoreOverrideCursor();

  setGuiForSvg( mLayer );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mChangeColorButton_colorChanged( const QColor &color )
{
  if ( !mLayer )
  {
    return;
  }

  mLayer->setFillColor( color );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mChangeStrokeColorButton_colorChanged( const QColor &color )
{
  if ( !mLayer )
  {
    return;
  }

  mLayer->setStrokeColor( color );
  emit changed();
}

void QgsSvgMarkerSymbolLayerWidget::on_mStrokeWidthSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setStrokeWidth( d );
    emit changed();
  }
}

void QgsSvgMarkerSymbolLayerWidget::on_mSizeUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setSizeUnit( mSizeUnitWidget->unit() );
    mLayer->setSizeMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSvgMarkerSymbolLayerWidget::on_mStrokeWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setStrokeWidthUnit( mStrokeWidthUnitWidget->unit() );
    mLayer->setStrokeWidthMapUnitScale( mStrokeWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSvgMarkerSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSvgMarkerSymbolLayerWidget::on_mHorizontalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setHorizontalAnchorPoint( QgsMarkerSymbolLayer::HorizontalAnchorPoint( index ) );
    emit changed();
  }
}

void QgsSvgMarkerSymbolLayerWidget::on_mVerticalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setVerticalAnchorPoint( QgsMarkerSymbolLayer::VerticalAnchorPoint( index ) );
    emit changed();
  }
}

/////////////

QgsSVGFillSymbolLayerWidget::QgsSVGFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent ): QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;
  setupUi( this );
  mTextureWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                     << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mSvgStrokeWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                       << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mSvgTreeView->setHeaderHidden( true );
  insertIcons();

  mRotationSpinBox->setClearValue( 0.0 );

  mChangeColorButton->setColorDialogTitle( tr( "Select Fill Color" ) );
  mChangeColorButton->setContext( QStringLiteral( "symbology" ) );
  mChangeStrokeColorButton->setColorDialogTitle( tr( "Select Stroke Color" ) );
  mChangeStrokeColorButton->setContext( QStringLiteral( "symbology" ) );

  connect( mSvgListView->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSVGFillSymbolLayerWidget::setFile );
  connect( mSvgTreeView->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSVGFillSymbolLayerWidget::populateIcons );
}

void QgsSVGFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( !layer )
  {
    return;
  }

  if ( layer->layerType() != QLatin1String( "SVGFill" ) )
  {
    return;
  }

  mLayer = dynamic_cast<QgsSVGFillSymbolLayer *>( layer );
  if ( mLayer )
  {
    double width = mLayer->patternWidth();
    mTextureWidthSpinBox->blockSignals( true );
    mTextureWidthSpinBox->setValue( width );
    mTextureWidthSpinBox->blockSignals( false );
    mSVGLineEdit->setText( mLayer->svgFilePath() );
    mRotationSpinBox->blockSignals( true );
    mRotationSpinBox->setValue( mLayer->angle() );
    mRotationSpinBox->blockSignals( false );
    mTextureWidthUnitWidget->blockSignals( true );
    mTextureWidthUnitWidget->setUnit( mLayer->patternWidthUnit() );
    mTextureWidthUnitWidget->setMapUnitScale( mLayer->patternWidthMapUnitScale() );
    mTextureWidthUnitWidget->blockSignals( false );
    mSvgStrokeWidthUnitWidget->blockSignals( true );
    mSvgStrokeWidthUnitWidget->setUnit( mLayer->svgStrokeWidthUnit() );
    mSvgStrokeWidthUnitWidget->setMapUnitScale( mLayer->svgStrokeWidthMapUnitScale() );
    mSvgStrokeWidthUnitWidget->blockSignals( false );
    mChangeColorButton->blockSignals( true );
    mChangeColorButton->setColor( mLayer->svgFillColor() );
    mChangeColorButton->blockSignals( false );
    mChangeStrokeColorButton->blockSignals( true );
    mChangeStrokeColorButton->setColor( mLayer->svgStrokeColor() );
    mChangeStrokeColorButton->blockSignals( false );
    mStrokeWidthSpinBox->blockSignals( true );
    mStrokeWidthSpinBox->setValue( mLayer->svgStrokeWidth() );
    mStrokeWidthSpinBox->blockSignals( false );
  }
  updateParamGui( false );

  registerDataDefinedButton( mTextureWidthDDBtn, QgsSymbolLayer::PropertyWidth );
  registerDataDefinedButton( mSVGDDBtn, QgsSymbolLayer::PropertyFile );
  registerDataDefinedButton( mRotationDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mFilColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mStrokeColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mStrokeWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
}

QgsSymbolLayer *QgsSVGFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsSVGFillSymbolLayerWidget::on_mBrowseToolButton_clicked()
{
  QString filePath = QFileDialog::getOpenFileName( nullptr, tr( "Select SVG texture file" ), QDir::homePath(), tr( "SVG file" ) + " (*.svg);;" + tr( "All files" ) + " (*.*)" );
  if ( !filePath.isNull() )
  {
    mSVGLineEdit->setText( filePath );
    emit changed();
  }
}

void QgsSVGFillSymbolLayerWidget::on_mTextureWidthSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setPatternWidth( d );
    emit changed();
  }
}

void QgsSVGFillSymbolLayerWidget::on_mSVGLineEdit_textEdited( const QString &text )
{
  if ( !mLayer )
  {
    return;
  }

  QFileInfo fi( text );
  if ( !fi.exists() )
  {
    return;
  }
  mLayer->setSvgFilePath( text );
  updateParamGui();
  emit changed();
}

void QgsSVGFillSymbolLayerWidget::on_mSVGLineEdit_editingFinished()
{
  if ( !mLayer )
  {
    return;
  }

  QFileInfo fi( mSVGLineEdit->text() );
  if ( !fi.exists() )
  {
    QUrl url( mSVGLineEdit->text() );
    if ( !url.isValid() )
    {
      return;
    }
  }

  QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
  mLayer->setSvgFilePath( mSVGLineEdit->text() );
  QApplication::restoreOverrideCursor();

  updateParamGui();
  emit changed();
}

void QgsSVGFillSymbolLayerWidget::setFile( const QModelIndex &item )
{
  QString file = item.data( Qt::UserRole ).toString();
  mLayer->setSvgFilePath( file );
  mSVGLineEdit->setText( file );

  updateParamGui();
  emit changed();
}

void QgsSVGFillSymbolLayerWidget::insertIcons()
{
  QAbstractItemModel *oldModel = mSvgTreeView->model();
  QgsSvgSelectorGroupsModel *g = new QgsSvgSelectorGroupsModel( mSvgTreeView );
  mSvgTreeView->setModel( g );
  delete oldModel;

  // Set the tree expanded at the first level
  int rows = g->rowCount( g->indexFromItem( g->invisibleRootItem() ) );
  for ( int i = 0; i < rows; i++ )
  {
    mSvgTreeView->setExpanded( g->indexFromItem( g->item( i ) ), true );
  }

  oldModel = mSvgListView->model();
  QgsSvgSelectorListModel *m = new QgsSvgSelectorListModel( mSvgListView );
  mSvgListView->setModel( m );
  delete oldModel;
}

void QgsSVGFillSymbolLayerWidget::populateIcons( const QModelIndex &idx )
{
  QString path = idx.data( Qt::UserRole + 1 ).toString();

  QAbstractItemModel *oldModel = mSvgListView->model();
  QgsSvgSelectorListModel *m = new QgsSvgSelectorListModel( mSvgListView, path );
  mSvgListView->setModel( m );
  delete oldModel;

  connect( mSvgListView->selectionModel(), &QItemSelectionModel::currentChanged, this, &QgsSVGFillSymbolLayerWidget::setFile );
}


void QgsSVGFillSymbolLayerWidget::on_mRotationSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setAngle( d );
    emit changed();
  }
}

void QgsSVGFillSymbolLayerWidget::updateParamGui( bool resetValues )
{
  //activate gui for svg parameters only if supported by the svg file
  bool hasFillParam, hasFillOpacityParam, hasStrokeParam, hasStrokeWidthParam, hasStrokeOpacityParam;
  QColor defaultFill, defaultStroke;
  double defaultStrokeWidth, defaultFillOpacity, defaultStrokeOpacity;
  bool hasDefaultFillColor, hasDefaultFillOpacity, hasDefaultStrokeColor, hasDefaultStrokeWidth, hasDefaultStrokeOpacity;
  QgsApplication::svgCache()->containsParams( mSVGLineEdit->text(), hasFillParam, hasDefaultFillColor, defaultFill,
      hasFillOpacityParam, hasDefaultFillOpacity, defaultFillOpacity,
      hasStrokeParam, hasDefaultStrokeColor, defaultStroke,
      hasStrokeWidthParam, hasDefaultStrokeWidth, defaultStrokeWidth,
      hasStrokeOpacityParam, hasDefaultStrokeOpacity, defaultStrokeOpacity );
  if ( resetValues )
  {
    QColor fill = mChangeColorButton->color();
    double newOpacity = hasFillOpacityParam ? fill.alphaF() : 1.0;
    if ( hasDefaultFillColor )
    {
      fill = defaultFill;
    }
    fill.setAlphaF( hasDefaultFillOpacity ? defaultFillOpacity : newOpacity );
    mChangeColorButton->setColor( fill );
  }
  mChangeColorButton->setEnabled( hasFillParam );
  mChangeColorButton->setAllowOpacity( hasFillOpacityParam );
  if ( resetValues )
  {
    QColor stroke = mChangeStrokeColorButton->color();
    double newOpacity = hasStrokeOpacityParam ? stroke.alphaF() : 1.0;
    if ( hasDefaultStrokeColor )
    {
      stroke = defaultStroke;
    }
    stroke.setAlphaF( hasDefaultStrokeOpacity ? defaultStrokeOpacity : newOpacity );
    mChangeStrokeColorButton->setColor( stroke );
  }
  mChangeStrokeColorButton->setEnabled( hasStrokeParam );
  mChangeStrokeColorButton->setAllowOpacity( hasStrokeOpacityParam );
  if ( hasDefaultStrokeWidth && resetValues )
  {
    mStrokeWidthSpinBox->setValue( defaultStrokeWidth );
  }
  mStrokeWidthSpinBox->setEnabled( hasStrokeWidthParam );
}

void QgsSVGFillSymbolLayerWidget::on_mChangeColorButton_colorChanged( const QColor &color )
{
  if ( !mLayer )
  {
    return;
  }

  mLayer->setSvgFillColor( color );
  emit changed();
}

void QgsSVGFillSymbolLayerWidget::on_mChangeStrokeColorButton_colorChanged( const QColor &color )
{
  if ( !mLayer )
  {
    return;
  }

  mLayer->setSvgStrokeColor( color );
  emit changed();
}

void QgsSVGFillSymbolLayerWidget::on_mStrokeWidthSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setSvgStrokeWidth( d );
    emit changed();
  }
}

void QgsSVGFillSymbolLayerWidget::on_mTextureWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setPatternWidthUnit( mTextureWidthUnitWidget->unit() );
    mLayer->setPatternWidthMapUnitScale( mTextureWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsSVGFillSymbolLayerWidget::on_mSvgStrokeWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setSvgStrokeWidthUnit( mSvgStrokeWidthUnitWidget->unit() );
    mLayer->setSvgStrokeWidthMapUnitScale( mSvgStrokeWidthUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

/////////////

QgsLinePatternFillSymbolLayerWidget::QgsLinePatternFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent ):
  QgsSymbolLayerWidget( parent, vl ), mLayer( nullptr )
{
  setupUi( this );
  mDistanceUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                 << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetSpinBox->setClearValue( 0 );
  mAngleSpinBox->setClearValue( 0 );
}

void QgsLinePatternFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "LinePatternFill" ) )
  {
    return;
  }

  QgsLinePatternFillSymbolLayer *patternLayer = static_cast<QgsLinePatternFillSymbolLayer *>( layer );
  if ( patternLayer )
  {
    mLayer = patternLayer;
    mAngleSpinBox->blockSignals( true );
    mAngleSpinBox->setValue( mLayer->lineAngle() );
    mAngleSpinBox->blockSignals( false );
    mDistanceSpinBox->blockSignals( true );
    mDistanceSpinBox->setValue( mLayer->distance() );
    mDistanceSpinBox->blockSignals( false );
    mOffsetSpinBox->blockSignals( true );
    mOffsetSpinBox->setValue( mLayer->offset() );
    mOffsetSpinBox->blockSignals( false );

    //units
    mDistanceUnitWidget->blockSignals( true );
    mDistanceUnitWidget->setUnit( mLayer->distanceUnit() );
    mDistanceUnitWidget->setMapUnitScale( mLayer->distanceMapUnitScale() );
    mDistanceUnitWidget->blockSignals( false );
    mOffsetUnitWidget->blockSignals( true );
    mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
    mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
    mOffsetUnitWidget->blockSignals( false );
  }

  registerDataDefinedButton( mAngleDDBtn, QgsSymbolLayer::PropertyLineAngle );
  registerDataDefinedButton( mDistanceDDBtn, QgsSymbolLayer::PropertyLineDistance );
}

QgsSymbolLayer *QgsLinePatternFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsLinePatternFillSymbolLayerWidget::on_mAngleSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setLineAngle( d );
    emit changed();
  }
}

void QgsLinePatternFillSymbolLayerWidget::on_mDistanceSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setDistance( d );
    emit changed();
  }
}

void QgsLinePatternFillSymbolLayerWidget::on_mOffsetSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setOffset( d );
    emit changed();
  }
}

void QgsLinePatternFillSymbolLayerWidget::on_mDistanceUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDistanceUnit( mDistanceUnitWidget->unit() );
    mLayer->setDistanceMapUnitScale( mDistanceUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsLinePatternFillSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

/////////////

QgsPointPatternFillSymbolLayerWidget::QgsPointPatternFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent ):
  QgsSymbolLayerWidget( parent, vl ), mLayer( nullptr )
{
  setupUi( this );
  mHorizontalDistanceUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
      << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mVerticalDistanceUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                         << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mHorizontalDisplacementUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
      << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mVerticalDisplacementUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
      << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
}


void QgsPointPatternFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( !layer || layer->layerType() != QLatin1String( "PointPatternFill" ) )
  {
    return;
  }

  mLayer = static_cast<QgsPointPatternFillSymbolLayer *>( layer );
  mHorizontalDistanceSpinBox->blockSignals( true );
  mHorizontalDistanceSpinBox->setValue( mLayer->distanceX() );
  mHorizontalDistanceSpinBox->blockSignals( false );
  mVerticalDistanceSpinBox->blockSignals( true );
  mVerticalDistanceSpinBox->setValue( mLayer->distanceY() );
  mVerticalDistanceSpinBox->blockSignals( false );
  mHorizontalDisplacementSpinBox->blockSignals( true );
  mHorizontalDisplacementSpinBox->setValue( mLayer->displacementX() );
  mHorizontalDisplacementSpinBox->blockSignals( false );
  mVerticalDisplacementSpinBox->blockSignals( true );
  mVerticalDisplacementSpinBox->setValue( mLayer->displacementY() );
  mVerticalDisplacementSpinBox->blockSignals( false );

  mHorizontalDistanceUnitWidget->blockSignals( true );
  mHorizontalDistanceUnitWidget->setUnit( mLayer->distanceXUnit() );
  mHorizontalDistanceUnitWidget->setMapUnitScale( mLayer->distanceXMapUnitScale() );
  mHorizontalDistanceUnitWidget->blockSignals( false );
  mVerticalDistanceUnitWidget->blockSignals( true );
  mVerticalDistanceUnitWidget->setUnit( mLayer->distanceYUnit() );
  mVerticalDistanceUnitWidget->setMapUnitScale( mLayer->distanceYMapUnitScale() );
  mVerticalDistanceUnitWidget->blockSignals( false );
  mHorizontalDisplacementUnitWidget->blockSignals( true );
  mHorizontalDisplacementUnitWidget->setUnit( mLayer->displacementXUnit() );
  mHorizontalDisplacementUnitWidget->setMapUnitScale( mLayer->displacementXMapUnitScale() );
  mHorizontalDisplacementUnitWidget->blockSignals( false );
  mVerticalDisplacementUnitWidget->blockSignals( true );
  mVerticalDisplacementUnitWidget->setUnit( mLayer->displacementYUnit() );
  mVerticalDisplacementUnitWidget->setMapUnitScale( mLayer->displacementYMapUnitScale() );
  mVerticalDisplacementUnitWidget->blockSignals( false );

  registerDataDefinedButton( mHorizontalDistanceDDBtn, QgsSymbolLayer::PropertyDistanceX );
  registerDataDefinedButton( mVerticalDistanceDDBtn, QgsSymbolLayer::PropertyDistanceY );
  registerDataDefinedButton( mHorizontalDisplacementDDBtn, QgsSymbolLayer::PropertyDisplacementX );
  registerDataDefinedButton( mVerticalDisplacementDDBtn, QgsSymbolLayer::PropertyDisplacementY );
}

QgsSymbolLayer *QgsPointPatternFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsPointPatternFillSymbolLayerWidget::on_mHorizontalDistanceSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setDistanceX( d );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mVerticalDistanceSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setDistanceY( d );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mHorizontalDisplacementSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setDisplacementX( d );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mVerticalDisplacementSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setDisplacementY( d );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mHorizontalDistanceUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDistanceXUnit( mHorizontalDistanceUnitWidget->unit() );
    mLayer->setDistanceXMapUnitScale( mHorizontalDistanceUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mVerticalDistanceUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDistanceYUnit( mVerticalDistanceUnitWidget->unit() );
    mLayer->setDistanceYMapUnitScale( mVerticalDistanceUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mHorizontalDisplacementUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDisplacementXUnit( mHorizontalDisplacementUnitWidget->unit() );
    mLayer->setDisplacementXMapUnitScale( mHorizontalDisplacementUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsPointPatternFillSymbolLayerWidget::on_mVerticalDisplacementUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setDisplacementYUnit( mVerticalDisplacementUnitWidget->unit() );
    mLayer->setDisplacementYMapUnitScale( mVerticalDisplacementUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

/////////////

QgsFontMarkerSymbolLayerWidget::QgsFontMarkerSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
  mSizeUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                             << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mStrokeWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                                    << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  widgetChar = new CharacterWidget;
  scrollArea->setWidget( widgetChar );

  btnColor->setAllowOpacity( true );
  btnColor->setColorDialogTitle( tr( "Select Symbol Fill Color" ) );
  btnColor->setContext( QStringLiteral( "symbology" ) );
  btnStrokeColor->setAllowOpacity( true );
  btnStrokeColor->setColorDialogTitle( tr( "Select Symbol Stroke Color" ) );
  btnStrokeColor->setContext( QStringLiteral( "symbology" ) );

  spinOffsetX->setClearValue( 0.0 );
  spinOffsetY->setClearValue( 0.0 );
  spinAngle->setClearValue( 0.0 );

  //make a temporary symbol for the size assistant preview
  mAssistantPreviewSymbol.reset( new QgsMarkerSymbol() );

  if ( vectorLayer() )
    mSizeDDBtn->setSymbol( mAssistantPreviewSymbol );

  connect( cboFont, &QFontComboBox::currentFontChanged, this, &QgsFontMarkerSymbolLayerWidget::setFontFamily );
  connect( spinSize, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFontMarkerSymbolLayerWidget::setSize );
  connect( cboJoinStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsFontMarkerSymbolLayerWidget::penJoinStyleChanged );
  connect( btnColor, &QgsColorButton::colorChanged, this, &QgsFontMarkerSymbolLayerWidget::setColor );
  connect( btnStrokeColor, &QgsColorButton::colorChanged, this, &QgsFontMarkerSymbolLayerWidget::setColorStroke );
  connect( cboJoinStyle, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsFontMarkerSymbolLayerWidget::penJoinStyleChanged );
  connect( spinAngle, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFontMarkerSymbolLayerWidget::setAngle );
  connect( spinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFontMarkerSymbolLayerWidget::setOffset );
  connect( spinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsFontMarkerSymbolLayerWidget::setOffset );
  connect( widgetChar, &CharacterWidget::characterSelected, this, &QgsFontMarkerSymbolLayerWidget::setCharacter );
  connect( this, &QgsSymbolLayerWidget::changed, this, &QgsFontMarkerSymbolLayerWidget::updateAssistantSymbol );
}

void QgsFontMarkerSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "FontMarker" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsFontMarkerSymbolLayer *>( layer );

  QFont layerFont( mLayer->fontFamily() );
  // set values
  whileBlocking( cboFont )->setCurrentFont( layerFont );
  whileBlocking( spinSize )->setValue( mLayer->size() );
  whileBlocking( btnColor )->setColor( mLayer->color() );
  whileBlocking( btnStrokeColor )->setColor( mLayer->strokeColor() );
  whileBlocking( mStrokeWidthSpinBox )->setValue( mLayer->strokeWidth() );
  whileBlocking( spinAngle )->setValue( mLayer->angle() );

  widgetChar->blockSignals( true );
  widgetChar->setFont( layerFont );
  widgetChar->setCharacter( mLayer->character() );
  widgetChar->blockSignals( false );

  //block
  whileBlocking( spinOffsetX )->setValue( mLayer->offset().x() );
  whileBlocking( spinOffsetY )->setValue( mLayer->offset().y() );

  mSizeUnitWidget->blockSignals( true );
  mSizeUnitWidget->setUnit( mLayer->sizeUnit() );
  mSizeUnitWidget->setMapUnitScale( mLayer->sizeMapUnitScale() );
  mSizeUnitWidget->blockSignals( false );

  mStrokeWidthUnitWidget->blockSignals( true );
  mStrokeWidthUnitWidget->setUnit( mLayer->strokeWidthUnit() );
  mStrokeWidthUnitWidget->setMapUnitScale( mLayer->strokeWidthMapUnitScale() );
  mStrokeWidthUnitWidget->blockSignals( false );

  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  whileBlocking( cboJoinStyle )->setPenJoinStyle( mLayer->penJoinStyle() );

  //anchor points
  whileBlocking( mHorizontalAnchorComboBox )->setCurrentIndex( mLayer->horizontalAnchorPoint() );
  whileBlocking( mVerticalAnchorComboBox )->setCurrentIndex( mLayer->verticalAnchorPoint() );

  registerDataDefinedButton( mSizeDDBtn, QgsSymbolLayer::PropertySize );
  registerDataDefinedButton( mRotationDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mColorDDBtn, QgsSymbolLayer::PropertyFillColor );
  registerDataDefinedButton( mStrokeColorDDBtn, QgsSymbolLayer::PropertyStrokeColor );
  registerDataDefinedButton( mStrokeWidthDDBtn, QgsSymbolLayer::PropertyStrokeWidth );
  registerDataDefinedButton( mJoinStyleDDBtn, QgsSymbolLayer::PropertyJoinStyle );
  registerDataDefinedButton( mOffsetDDBtn, QgsSymbolLayer::PropertyOffset );
  registerDataDefinedButton( mHorizontalAnchorDDBtn, QgsSymbolLayer::PropertyHorizontalAnchor );
  registerDataDefinedButton( mVerticalAnchorDDBtn, QgsSymbolLayer::PropertyVerticalAnchor );
  registerDataDefinedButton( mCharDDBtn, QgsSymbolLayer::PropertyCharacter );

  updateAssistantSymbol();
}

QgsSymbolLayer *QgsFontMarkerSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsFontMarkerSymbolLayerWidget::setFontFamily( const QFont &font )
{
  mLayer->setFontFamily( font.family() );
  widgetChar->setFont( font );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setColor( const QColor &color )
{
  mLayer->setColor( color );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setColorStroke( const QColor &color )
{
  mLayer->setStrokeColor( color );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setSize( double size )
{
  mLayer->setSize( size );
  //widgetChar->updateSize(size);
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setAngle( double angle )
{
  mLayer->setAngle( angle );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setCharacter( QChar chr )
{
  mLayer->setCharacter( chr );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::setOffset()
{
  mLayer->setOffset( QPointF( spinOffsetX->value(), spinOffsetY->value() ) );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::penJoinStyleChanged()
{
  mLayer->setPenJoinStyle( cboJoinStyle->penJoinStyle() );
  emit changed();
}

void QgsFontMarkerSymbolLayerWidget::on_mSizeUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setSizeUnit( mSizeUnitWidget->unit() );
    mLayer->setSizeMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
    mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::on_mStrokeWidthUnitWidget_changed()
{
  if ( mLayer )
  {
    mLayer->setStrokeWidthUnit( mSizeUnitWidget->unit() );
    mLayer->setStrokeWidthMapUnitScale( mSizeUnitWidget->getMapUnitScale() );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::on_mHorizontalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setHorizontalAnchorPoint( QgsMarkerSymbolLayer::HorizontalAnchorPoint( index ) );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::on_mVerticalAnchorComboBox_currentIndexChanged( int index )
{
  if ( mLayer )
  {
    mLayer->setVerticalAnchorPoint( QgsMarkerSymbolLayer::VerticalAnchorPoint( index ) );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::on_mStrokeWidthSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setStrokeWidth( d );
    emit changed();
  }
}

void QgsFontMarkerSymbolLayerWidget::updateAssistantSymbol()
{
  for ( int i = mAssistantPreviewSymbol->symbolLayerCount() - 1 ; i >= 0; --i )
  {
    mAssistantPreviewSymbol->deleteSymbolLayer( i );
  }
  mAssistantPreviewSymbol->appendSymbolLayer( mLayer->clone() );
  QgsProperty ddSize = mLayer->dataDefinedProperties().property( QgsSymbolLayer::PropertySize );
  if ( ddSize )
    mAssistantPreviewSymbol->setDataDefinedSize( ddSize );
}

///////////////


QgsCentroidFillSymbolLayerWidget::QgsCentroidFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;

  setupUi( this );
}

void QgsCentroidFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( layer->layerType() != QLatin1String( "CentroidFill" ) )
    return;

  // layer type is correct, we can do the cast
  mLayer = static_cast<QgsCentroidFillSymbolLayer *>( layer );

  // set values
  whileBlocking( mDrawInsideCheckBox )->setChecked( mLayer->pointOnSurface() );
  whileBlocking( mDrawAllPartsCheckBox )->setChecked( mLayer->pointOnAllParts() );
}

QgsSymbolLayer *QgsCentroidFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsCentroidFillSymbolLayerWidget::on_mDrawInsideCheckBox_stateChanged( int state )
{
  mLayer->setPointOnSurface( state == Qt::Checked );
  emit changed();
}

void QgsCentroidFillSymbolLayerWidget::on_mDrawAllPartsCheckBox_stateChanged( int state )
{
  mLayer->setPointOnAllParts( state == Qt::Checked );
  emit changed();
}

///////////////

QgsRasterFillSymbolLayerWidget::QgsRasterFillSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
{
  mLayer = nullptr;
  setupUi( this );

  mWidthUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderPixels << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits
                              << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );
  mOffsetUnitWidget->setUnits( QgsUnitTypes::RenderUnitList() << QgsUnitTypes::RenderMillimeters << QgsUnitTypes::RenderMetersInMapUnits << QgsUnitTypes::RenderMapUnits << QgsUnitTypes::RenderPixels
                               << QgsUnitTypes::RenderPoints << QgsUnitTypes::RenderInches );

  mSpinOffsetX->setClearValue( 0.0 );
  mSpinOffsetY->setClearValue( 0.0 );
  mRotationSpinBox->setClearValue( 0.0 );

  connect( cboCoordinateMode, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsRasterFillSymbolLayerWidget::setCoordinateMode );
  connect( mSpinOffsetX, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsRasterFillSymbolLayerWidget::offsetChanged );
  connect( mSpinOffsetY, static_cast < void ( QDoubleSpinBox::* )( double ) > ( &QDoubleSpinBox::valueChanged ), this, &QgsRasterFillSymbolLayerWidget::offsetChanged );
  connect( mOpacityWidget, &QgsOpacityWidget::opacityChanged, this, &QgsRasterFillSymbolLayerWidget::opacityChanged );
}


void QgsRasterFillSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *layer )
{
  if ( !layer )
  {
    return;
  }

  if ( layer->layerType() != QLatin1String( "RasterFill" ) )
  {
    return;
  }

  mLayer = dynamic_cast<QgsRasterFillSymbolLayer *>( layer );
  if ( !mLayer )
  {
    return;
  }

  mImageLineEdit->blockSignals( true );
  mImageLineEdit->setText( mLayer->imageFilePath() );
  mImageLineEdit->blockSignals( false );

  cboCoordinateMode->blockSignals( true );
  switch ( mLayer->coordinateMode() )
  {
    case QgsRasterFillSymbolLayer::Viewport:
      cboCoordinateMode->setCurrentIndex( 1 );
      break;
    case QgsRasterFillSymbolLayer::Feature:
    default:
      cboCoordinateMode->setCurrentIndex( 0 );
      break;
  }
  cboCoordinateMode->blockSignals( false );
  mOpacityWidget->blockSignals( true );
  mOpacityWidget->setOpacity( mLayer->opacity() );
  mOpacityWidget->blockSignals( false );
  mRotationSpinBox->blockSignals( true );
  mRotationSpinBox->setValue( mLayer->angle() );
  mRotationSpinBox->blockSignals( false );

  mSpinOffsetX->blockSignals( true );
  mSpinOffsetX->setValue( mLayer->offset().x() );
  mSpinOffsetX->blockSignals( false );
  mSpinOffsetY->blockSignals( true );
  mSpinOffsetY->setValue( mLayer->offset().y() );
  mSpinOffsetY->blockSignals( false );
  mOffsetUnitWidget->blockSignals( true );
  mOffsetUnitWidget->setUnit( mLayer->offsetUnit() );
  mOffsetUnitWidget->setMapUnitScale( mLayer->offsetMapUnitScale() );
  mOffsetUnitWidget->blockSignals( false );

  mWidthSpinBox->blockSignals( true );
  mWidthSpinBox->setValue( mLayer->width() );
  mWidthSpinBox->blockSignals( false );
  mWidthUnitWidget->blockSignals( true );
  mWidthUnitWidget->setUnit( mLayer->widthUnit() );
  mWidthUnitWidget->setMapUnitScale( mLayer->widthMapUnitScale() );
  mWidthUnitWidget->blockSignals( false );
  updatePreviewImage();

  registerDataDefinedButton( mFilenameDDBtn, QgsSymbolLayer::PropertyFile );
  registerDataDefinedButton( mOpacityDDBtn, QgsSymbolLayer::PropertyOpacity );
  registerDataDefinedButton( mRotationDDBtn, QgsSymbolLayer::PropertyAngle );
  registerDataDefinedButton( mWidthDDBtn, QgsSymbolLayer::PropertyWidth );
}

QgsSymbolLayer *QgsRasterFillSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsRasterFillSymbolLayerWidget::on_mBrowseToolButton_clicked()
{
  QgsSettings s;
  QString openDir;
  QString lineEditText = mImageLineEdit->text();
  if ( !lineEditText.isEmpty() )
  {
    QFileInfo openDirFileInfo( lineEditText );
    openDir = openDirFileInfo.path();
  }

  if ( openDir.isEmpty() )
  {
    openDir = s.value( QStringLiteral( "/UI/lastRasterFillImageDir" ), QDir::homePath() ).toString();
  }

  //show file dialog
  QString filePath = QFileDialog::getOpenFileName( nullptr, tr( "Select image file" ), openDir );
  if ( !filePath.isNull() )
  {
    //check if file exists
    QFileInfo fileInfo( filePath );
    if ( !fileInfo.exists() || !fileInfo.isReadable() )
    {
      QMessageBox::critical( nullptr, QStringLiteral( "Invalid file" ), QStringLiteral( "Error, file does not exist or is not readable" ) );
      return;
    }

    s.setValue( QStringLiteral( "/UI/lastRasterFillImageDir" ), fileInfo.absolutePath() );
    mImageLineEdit->setText( filePath );
    on_mImageLineEdit_editingFinished();
  }
}

void QgsRasterFillSymbolLayerWidget::on_mImageLineEdit_editingFinished()
{
  if ( !mLayer )
  {
    return;
  }

  QFileInfo fi( mImageLineEdit->text() );
  if ( !fi.exists() )
  {
    QUrl url( mImageLineEdit->text() );
    if ( !url.isValid() )
    {
      return;
    }
  }

  QApplication::setOverrideCursor( QCursor( Qt::WaitCursor ) );
  mLayer->setImageFilePath( mImageLineEdit->text() );
  updatePreviewImage();
  QApplication::restoreOverrideCursor();

  emit changed();
}

void QgsRasterFillSymbolLayerWidget::setCoordinateMode( int index )
{
  switch ( index )
  {
    case 0:
      //feature coordinate mode
      mLayer->setCoordinateMode( QgsRasterFillSymbolLayer::Feature );
      break;
    case 1:
      //viewport coordinate mode
      mLayer->setCoordinateMode( QgsRasterFillSymbolLayer::Viewport );
      break;
  }

  emit changed();
}

void QgsRasterFillSymbolLayerWidget::opacityChanged( double value )
{
  if ( !mLayer )
  {
    return;
  }

  mLayer->setOpacity( value );
  emit changed();
  updatePreviewImage();
}

void QgsRasterFillSymbolLayerWidget::offsetChanged()
{
  mLayer->setOffset( QPointF( mSpinOffsetX->value(), mSpinOffsetY->value() ) );
  emit changed();
}

void QgsRasterFillSymbolLayerWidget::on_mOffsetUnitWidget_changed()
{
  if ( !mLayer )
  {
    return;
  }
  mLayer->setOffsetUnit( mOffsetUnitWidget->unit() );
  mLayer->setOffsetMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
  emit changed();
}

void QgsRasterFillSymbolLayerWidget::on_mRotationSpinBox_valueChanged( double d )
{
  if ( mLayer )
  {
    mLayer->setAngle( d );
    emit changed();
  }
}

void QgsRasterFillSymbolLayerWidget::on_mWidthUnitWidget_changed()
{
  if ( !mLayer )
  {
    return;
  }
  mLayer->setWidthUnit( mWidthUnitWidget->unit() );
  mLayer->setWidthMapUnitScale( mOffsetUnitWidget->getMapUnitScale() );
  emit changed();
}

void QgsRasterFillSymbolLayerWidget::on_mWidthSpinBox_valueChanged( double d )
{
  if ( !mLayer )
  {
    return;
  }
  mLayer->setWidth( d );
  emit changed();
}


void QgsRasterFillSymbolLayerWidget::updatePreviewImage()
{
  if ( !mLayer )
  {
    return;
  }

  QImage image( mLayer->imageFilePath() );
  if ( image.isNull() )
  {
    mLabelImagePreview->setPixmap( QPixmap() );
    return;
  }

  if ( image.height() > 150 || image.width() > 150 )
  {
    image = image.scaled( 150, 150, Qt::KeepAspectRatio, Qt::SmoothTransformation );
  }

  QImage previewImage( 150, 150, QImage::Format_ARGB32 );
  previewImage.fill( Qt::transparent );
  QRect imageRect( ( 150 - image.width() ) / 2.0, ( 150 - image.height() ) / 2.0, image.width(), image.height() );
  QPainter p;
  p.begin( &previewImage );
  //draw a checkerboard background
  uchar pixDataRGB[] = { 150, 150, 150, 150,
                         100, 100, 100, 150,
                         100, 100, 100, 150,
                         150, 150, 150, 150
                       };
  QImage img( pixDataRGB, 2, 2, 8, QImage::Format_ARGB32 );
  QPixmap pix = QPixmap::fromImage( img.scaled( 8, 8 ) );
  QBrush checkerBrush;
  checkerBrush.setTexture( pix );
  p.fillRect( imageRect, checkerBrush );

  if ( mLayer->opacity() < 1.0 )
  {
    p.setOpacity( mLayer->opacity() );
  }

  p.drawImage( imageRect.left(), imageRect.top(), image );
  p.end();
  mLabelImagePreview->setPixmap( QPixmap::fromImage( previewImage ) );
}


QgsGeometryGeneratorSymbolLayerWidget::QgsGeometryGeneratorSymbolLayerWidget( const QgsVectorLayer *vl, QWidget *parent )
  : QgsSymbolLayerWidget( parent, vl )
  , mLayer( nullptr )
{
  setupUi( this );
  modificationExpressionSelector->setMultiLine( true );
  modificationExpressionSelector->setLayer( const_cast<QgsVectorLayer *>( vl ) );
  modificationExpressionSelector->registerExpressionContextGenerator( this );
  cbxGeometryType->addItem( QgsApplication::getThemeIcon( QStringLiteral( "/mIconPolygonLayer.svg" ) ), tr( "Polygon / MultiPolygon" ), QgsSymbol::Fill );
  cbxGeometryType->addItem( QgsApplication::getThemeIcon( QStringLiteral( "/mIconLineLayer.svg" ) ), tr( "LineString / MultiLineString" ), QgsSymbol::Line );
  cbxGeometryType->addItem( QgsApplication::getThemeIcon( QStringLiteral( "/mIconPointLayer.svg" ) ), tr( "Point / MultiPoint" ), QgsSymbol::Marker );
  connect( modificationExpressionSelector, &QgsExpressionLineEdit::expressionChanged, this, &QgsGeometryGeneratorSymbolLayerWidget::updateExpression );
  connect( cbxGeometryType, static_cast<void ( QComboBox::* )( int )>( &QComboBox::currentIndexChanged ), this, &QgsGeometryGeneratorSymbolLayerWidget::updateSymbolType );
}

void QgsGeometryGeneratorSymbolLayerWidget::setSymbolLayer( QgsSymbolLayer *l )
{
  mLayer = static_cast<QgsGeometryGeneratorSymbolLayer *>( l );
  modificationExpressionSelector->setExpression( mLayer->geometryExpression() );
  cbxGeometryType->setCurrentIndex( cbxGeometryType->findData( mLayer->symbolType() ) );
}

QgsSymbolLayer *QgsGeometryGeneratorSymbolLayerWidget::symbolLayer()
{
  return mLayer;
}

void QgsGeometryGeneratorSymbolLayerWidget::updateExpression( const QString &string )
{
  mLayer->setGeometryExpression( string );

  emit changed();
}

void QgsGeometryGeneratorSymbolLayerWidget::updateSymbolType()
{
  mLayer->setSymbolType( static_cast<QgsSymbol::SymbolType>( cbxGeometryType->currentData().toInt() ) );

  emit symbolChanged();
}
