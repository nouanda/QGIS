/***************************************************************************
                             qgslayoutviewtool.cpp
                             ---------------------
    Date                 : July 2017
    Copyright            : (C) 2017 Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslayoutviewtool.h"
#include "qgslayoutview.h"
#include "qgslayoutviewmouseevent.h"

QgsLayoutViewTool::QgsLayoutViewTool( QgsLayoutView *view, const QString &name )
  : QObject( view )
  , mView( view )
  , mFlags( 0 )
  , mCursor( Qt::ArrowCursor )
  , mToolName( name )
{

}

bool QgsLayoutViewTool::isClickAndDrag( QPoint startViewPoint, QPoint endViewPoint ) const
{
  int diffX = endViewPoint.x() - startViewPoint.x();
  int diffY = endViewPoint.y() - startViewPoint.y();
  if ( std::abs( diffX ) >= 2 || std::abs( diffY ) >= 2 )
  {
    return true;
  }
  return false;
}

QgsLayoutView *QgsLayoutViewTool::view() const
{
  return mView;
}

QgsLayout *QgsLayoutViewTool::layout() const
{
  return mView->currentLayout();
}

QgsLayoutViewTool::~QgsLayoutViewTool()
{
  mView->unsetTool( this );
}

QgsLayoutViewTool::Flags QgsLayoutViewTool::flags() const
{
  return mFlags;
}

void QgsLayoutViewTool::setFlags( QgsLayoutViewTool::Flags flags )
{
  mFlags = flags;
}

void QgsLayoutViewTool::layoutMoveEvent( QgsLayoutViewMouseEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::layoutDoubleClickEvent( QgsLayoutViewMouseEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::layoutPressEvent( QgsLayoutViewMouseEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::layoutReleaseEvent( QgsLayoutViewMouseEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::wheelEvent( QWheelEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::keyPressEvent( QKeyEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::keyReleaseEvent( QKeyEvent *event )
{
  event->ignore();
}

void QgsLayoutViewTool::setAction( QAction *action )
{
  mAction = action;
}

QAction *QgsLayoutViewTool::action()
{
  return mAction;

}

void QgsLayoutViewTool::setCursor( const QCursor &cursor )
{
  mCursor = cursor;
}

void QgsLayoutViewTool::activate()
{
  // make action and/or button active
  if ( mAction )
    mAction->setChecked( true );

  mView->viewport()->setCursor( mCursor );
  emit activated();
}

void QgsLayoutViewTool::deactivate()
{
  if ( mAction )
    mAction->setChecked( false );

  emit deactivated();
}
