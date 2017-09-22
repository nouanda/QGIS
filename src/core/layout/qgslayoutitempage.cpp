/***************************************************************************
                              qgslayoutitempage.cpp
                             ---------------------
    begin                : July 2017
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

#include "qgslayoutitempage.h"
#include "qgslayout.h"
#include "qgslayoututils.h"
#include "qgspagesizeregistry.h"
#include "qgssymbollayerutils.h"
#include <QPainter>
#include <QStyleOptionGraphicsItem>

QgsLayoutItemPage::QgsLayoutItemPage( QgsLayout *layout )
  : QgsLayoutItem( layout )
{
  setFlag( QGraphicsItem::ItemIsSelectable, false );
  setFlag( QGraphicsItem::ItemIsMovable, false );
  setZValue( QgsLayout::ZPage );

  // use a hidden pen to specify the amount the page "bleeds" outside it's scene bounds,
  // (it's a lot easier than reimplementing boundingRect() just to handle this)
  QPen shadowPen( QBrush( Qt::transparent ), layout->pageCollection()->pageShadowWidth() * 2 );
  setPen( shadowPen );

  QFont font;
  QFontMetrics fm( font );
  mMaximumShadowWidth = fm.width( "X" );

  mGrid.reset( new QgsLayoutItemPageGrid( pos().x(), pos().y(), rect().width(), rect().height(), mLayout ) );
  mGrid->setParentItem( this );
}

void QgsLayoutItemPage::setPageSize( const QgsLayoutSize &size )
{
  attemptResize( size );
}

bool QgsLayoutItemPage::setPageSize( const QString &size, Orientation orientation )
{
  QgsPageSize newSize;
  if ( QgsApplication::pageSizeRegistry()->decodePageSize( size, newSize ) )
  {
    switch ( orientation )
    {
      case Portrait:
        break; // nothing to do

      case Landscape:
      {
        // flip height and width
        double x = newSize.size.width();
        newSize.size.setWidth( newSize.size.height() );
        newSize.size.setHeight( x );
        break;
      }
    }

    setPageSize( newSize.size );
    return true;
  }
  else
  {
    return false;
  }
}

QgsLayoutSize QgsLayoutItemPage::pageSize() const
{
  return sizeWithUnits();
}

QgsLayoutItemPage::Orientation QgsLayoutItemPage::orientation() const
{
  if ( sizeWithUnits().width() >= sizeWithUnits().height() )
    return Landscape;
  else
    return Portrait;
}

QgsLayoutItemPage::Orientation QgsLayoutItemPage::decodePageOrientation( const QString &string, bool *ok )
{
  if ( ok )
    *ok = false;

  QString trimmedString = string.trimmed();
  if ( trimmedString.compare( QStringLiteral( "portrait" ), Qt::CaseInsensitive ) == 0 )
  {
    if ( ok )
      *ok = true;
    return Portrait;
  }
  else if ( trimmedString.compare( QStringLiteral( "landscape" ), Qt::CaseInsensitive ) == 0 )
  {
    if ( ok )
      *ok = true;
    return Landscape;
  }
  return Landscape;
}

void QgsLayoutItemPage::attemptResize( const QgsLayoutSize &size )
{
  QgsLayoutItem::attemptResize( size );
  //update size of attached grid to reflect new page size and position
  mGrid->setRect( 0, 0, rect().width(), rect().height() );

  mLayout->guides().update();
}

void QgsLayoutItemPage::redraw()
{
  QgsLayoutItem::redraw();
  mGrid->update();
}

void QgsLayoutItemPage::draw( QgsRenderContext &context, const QStyleOptionGraphicsItem * )
{
  if ( !context.painter() || !mLayout /*|| !mLayout->pagesVisible() */ )
  {
    return;
  }

  double scale = context.convertToPainterUnits( 1, QgsUnitTypes::RenderMillimeters );

  QgsExpressionContext expressionContext = createExpressionContext();
  context.setExpressionContext( expressionContext );

  QPainter *painter = context.painter();
  painter->save();

#if 0 //TODO
  if ( mComposition->plotStyle() ==  QgsComposition::Preview )
#endif
  {
    //if in preview mode, draw page border and shadow so that it's
    //still possible to tell where pages with a transparent style begin and end
    painter->setRenderHint( QPainter::Antialiasing, false );

    QRectF pageRect = QRectF( 0, 0, scale * rect().width(), scale * rect().height() );

    //shadow
    painter->setBrush( QBrush( QColor( 150, 150, 150 ) ) );
    painter->setPen( Qt::NoPen );
    painter->drawRect( pageRect.translated( std::min( scale * mLayout->pageCollection()->pageShadowWidth(), mMaximumShadowWidth ),
                                            std::min( scale * mLayout->pageCollection()->pageShadowWidth(), mMaximumShadowWidth ) ) );

    //page area
    painter->setBrush( QColor( 215, 215, 215 ) );
    QPen pagePen = QPen( QColor( 100, 100, 100 ), 0 );
    pagePen.setJoinStyle( Qt::MiterJoin );
    pagePen.setCosmetic( true );
    painter->setPen( pagePen );
    painter->drawRect( pageRect );
  }

  std::unique_ptr< QgsFillSymbol > symbol( mLayout->pageCollection()->pageStyleSymbol()->clone() );
  symbol->startRender( context );

  //get max bleed from symbol
  double maxBleedPixels = QgsSymbolLayerUtils::estimateMaxSymbolBleed( symbol.get(), context );

  //Now subtract 1 pixel to prevent semi-transparent borders at edge of solid page caused by
  //anti-aliased painting. This may cause a pixel to be cropped from certain edge lines/symbols,
  //but that can be counteracted by adding a dummy transparent line symbol layer with a wider line width
  maxBleedPixels--;

  QPolygonF pagePolygon = QPolygonF( QRectF( maxBleedPixels, maxBleedPixels,
                                     ( rect().width() * scale - 2 * maxBleedPixels ), ( rect().height() * scale - 2 * maxBleedPixels ) ) );
  QList<QPolygonF> rings; //empty list

  symbol->renderPolygon( pagePolygon, &rings, nullptr, context );
  symbol->stopRender( context );

  painter->restore();
}


//
// QgsLayoutItemPageGrid
//
///@cond PRIVATE

QgsLayoutItemPageGrid::QgsLayoutItemPageGrid( double x, double y, double width, double height, QgsLayout *layout )
  : QGraphicsRectItem( 0, 0, width, height )
  , mLayout( layout )
{
  // needed to access current view transform during paint operations
  setFlags( flags() | QGraphicsItem::ItemUsesExtendedStyleOption );
  setCacheMode( QGraphicsItem::DeviceCoordinateCache );
  setFlag( QGraphicsItem::ItemIsSelectable, false );
  setFlag( QGraphicsItem::ItemIsMovable, false );
  setZValue( QgsLayout::ZGrid );
  setPos( x, y );
}

void QgsLayoutItemPageGrid::paint( QPainter *painter, const QStyleOptionGraphicsItem *itemStyle, QWidget *pWidget )
{
  Q_UNUSED( pWidget );

  //draw grid
  if ( !mLayout )
    return;

  const QgsLayoutContext &context = mLayout->context();
  const QgsLayoutGridSettings &grid = mLayout->gridSettings();

  if ( !context.gridVisible() || grid.resolution().length() <= 0 )
    return;

  QPointF gridOffset = mLayout->convertToLayoutUnits( grid.offset() );
  double gridResolution = mLayout->convertToLayoutUnits( grid.resolution() );
  int gridMultiplyX = static_cast< int >( gridOffset.x() / gridResolution );
  int gridMultiplyY = static_cast< int >( gridOffset.y() / gridResolution );
  double currentXCoord = gridOffset.x() - gridMultiplyX * gridResolution;
  double currentYCoord;
  double minYCoord = gridOffset.y() - gridMultiplyY * gridResolution;

  painter->save();
  //turn of antialiasing so grid is nice and sharp
  painter->setRenderHint( QPainter::Antialiasing, false );

  switch ( grid.style() )
  {
    case QgsLayoutGridSettings::StyleLines:
    {
      painter->setPen( grid.pen() );

      //draw vertical lines
      for ( ; currentXCoord <= rect().width(); currentXCoord += gridResolution )
      {
        painter->drawLine( QPointF( currentXCoord, 0 ), QPointF( currentXCoord, rect().height() ) );
      }

      //draw horizontal lines
      currentYCoord = minYCoord;
      for ( ; currentYCoord <= rect().height(); currentYCoord += gridResolution )
      {
        painter->drawLine( QPointF( 0, currentYCoord ), QPointF( rect().width(), currentYCoord ) );
      }
      break;
    }

    case QgsLayoutGridSettings::StyleDots:
    case QgsLayoutGridSettings::StyleCrosses:
    {
      QPen gridPen = grid.pen();
      painter->setPen( gridPen );
      painter->setBrush( QBrush( gridPen.color() ) );
      double halfCrossLength = 1;
      if ( grid.style() == QgsLayoutGridSettings::StyleDots )
      {
        //dots are actually drawn as tiny crosses a few pixels across
        //set halfCrossLength to equivalent of 1 pixel
        halfCrossLength = 1 / itemStyle->matrix.m11();
      }
      else
      {
        halfCrossLength = gridResolution / 6;
      }

      for ( ; currentXCoord <= rect().width(); currentXCoord += gridResolution )
      {
        currentYCoord = minYCoord;
        for ( ; currentYCoord <= rect().height(); currentYCoord += gridResolution )
        {
          painter->drawLine( QPointF( currentXCoord - halfCrossLength, currentYCoord ), QPointF( currentXCoord + halfCrossLength, currentYCoord ) );
          painter->drawLine( QPointF( currentXCoord, currentYCoord - halfCrossLength ), QPointF( currentXCoord, currentYCoord + halfCrossLength ) );
        }
      }
      break;
    }
  }
  painter->restore();
}

///@endcond
