/***************************************************************************
                             qgslayoutpropertieswidget.h
                             ----------------------------
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

#ifndef QGSLAYOUTPROPERTIESWIDGET_H
#define QGSLAYOUTPROPERTIESWIDGET_H

#include "ui_qgslayoutwidgetbase.h"
#include "qgspanelwidget.h"

class QgsLayout;

class QgsLayoutPropertiesWidget: public QgsPanelWidget, private Ui::QgsLayoutWidgetBase
{
    Q_OBJECT
  public:
    QgsLayoutPropertiesWidget( QWidget *parent, QgsLayout *layout );

  private slots:

    void gridResolutionChanged( double d );
    void gridResolutionUnitsChanged( QgsUnitTypes::LayoutUnit unit );
    void gridOffsetXChanged( double d );
    void gridOffsetYChanged( double d );
    void gridOffsetUnitsChanged( QgsUnitTypes::LayoutUnit unit );
    void snapToleranceChanged( int tolerance );

  private:

    QgsLayout *mLayout = nullptr;

    void updateSnappingElements();
    void blockSignals( bool block );

};

#endif // QGSLAYOUTPROPERTIESWIDGET_H
