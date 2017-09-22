/***************************************************************************
  qgslegendsymbolitem.h
  --------------------------------------
  Date                 : August 2014
  Copyright            : (C) 2014 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSLEGENDSYMBOLITEMV2_H
#define QGSLEGENDSYMBOLITEMV2_H

#include <memory>
#include <QString>

#include "qgis.h"
#include "qgis_core.h"

class QgsDataDefinedSizeLegend;
class QgsSymbol;

/** \ingroup core
 * The class stores information about one class/rule of a vector layer renderer in a unified way
 * that can be used by legend model for rendering of legend.
 *
 * \see QgsSymbolLegendNode
 * \since QGIS 2.6
 */
class CORE_EXPORT QgsLegendSymbolItem
{
  public:
    QgsLegendSymbolItem();
    //! Construct item. Does not take ownership of symbol (makes internal clone)
    //! \since QGIS 2.8
    QgsLegendSymbolItem( QgsSymbol *symbol, const QString &label, const QString &ruleKey, bool checkable = false, int scaleMinDenom = -1, int scaleMaxDenom = -1, int level = 0, const QString &parentRuleKey = QString() );
    ~QgsLegendSymbolItem();

    QgsLegendSymbolItem( const QgsLegendSymbolItem &other );
    QgsLegendSymbolItem &operator=( const QgsLegendSymbolItem &other );

    //! Return associated symbol. May be null.
    QgsSymbol *symbol() const { return mSymbol; }
    //! Return text label
    QString label() const { return mLabel; }
    //! Return unique identifier of the rule for identification of the item within renderer
    QString ruleKey() const { return mKey; }
    //! Return whether the item is user-checkable - whether renderer supports enabling/disabling it
    bool isCheckable() const { return mCheckable; }

    //! Used for older code that identifies legend entries from symbol pointer within renderer
    QgsSymbol *legacyRuleKey() const { return mOriginalSymbolPointer; }

    //! Determine whether given scale is within the scale range. Returns true if scale or scale range is invalid (value <= 0)
    bool isScaleOK( double scale ) const;
    //! Min scale denominator of the scale range. For range 1:1000 to 1:2000 this will return 1000.
    //! Value <= 0 means the range is unbounded on this side
    int scaleMinDenom() const { return mScaleMinDenom; }
    //! Max scale denominator of the scale range. For range 1:1000 to 1:2000 this will return 2000.
    //! Value <= 0 means the range is unbounded on this side
    int scaleMaxDenom() const { return mScaleMaxDenom; }

    //! Identation level that tells how deep the item is in a hierarchy of items. For flat lists level is 0
    int level() const { return mLevel; }

    //! Key of the parent legend node. For legends with tree hierarchy
    //! \note Parameter parentRuleKey added in QGIS 2.8
    QString parentRuleKey() const { return mParentKey; }

    //! Set symbol of the item. Takes ownership of symbol.
    void setSymbol( QgsSymbol *s SIP_TRANSFER );

    /**
     * Sets extra information about data-defined size. If set, this item should be converted to QgsDataDefinedSizeLegendNode
     * rather than QgsSymbolLegendNode instance as usual. Passing null removes any data-defined size legend settings.
     *
     * Takes ownership of the settings object.
     * \since QGIS 3.0
     */
    void setDataDefinedSizeLegendSettings( QgsDataDefinedSizeLegend *settings SIP_TRANSFER );

    /**
     * Returns extra information for data-defined size legend rendering. Normally it returns null.
     * \since QGIS 3.0
     */
    QgsDataDefinedSizeLegend *dataDefinedSizeLegendSettings() const;

  private:
    //! symbol. owned by the struct. can be null.
    QgsSymbol *mSymbol = nullptr;
    //! label of the item (may be empty or non-unique)
    QString mLabel;
    //! unique identifier of the symbol item (within renderer)
    QString mKey;
    //! whether it can be enabled/disabled
    bool mCheckable;

    QgsSymbol *mOriginalSymbolPointer = nullptr;

    //! optional pointer to data-defined legend size settings - if set, the output legend
    //! node should be QgsDataDefinedSizeLegendNode rather than ordinary QgsSymbolLegendNode
    QgsDataDefinedSizeLegend *mDataDefinedSizeLegendSettings = nullptr;

    // additional data that may be used for filtering

    int mScaleMinDenom;
    int mScaleMaxDenom;

    //! Identation level that tells how deep the item is in a hierarchy of items. For flat lists level is 0
    int mLevel;
    //! Key of the parent legend node. For legends with tree hierarchy
    QString mParentKey;
};


typedef QList< QgsLegendSymbolItem > QgsLegendSymbolList;

#endif // QGSLEGENDSYMBOLITEMV2_H
