/***************************************************************************
                              qgslayout.h
                             -------------------
    begin                : June 2017
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
#ifndef QGSLAYOUT_H
#define QGSLAYOUT_H

#include "qgis_core.h"
#include <QGraphicsScene>
#include "qgslayoutcontext.h"
#include "qgslayoutsnapper.h"
#include "qgsexpressioncontextgenerator.h"
#include "qgslayoutpagecollection.h"
#include "qgslayoutgridsettings.h"
#include "qgslayoutguidecollection.h"

class QgsLayoutItemMap;

/**
 * \ingroup core
 * \class QgsLayout
 * \brief Base class for layouts, which can contain items such as maps, labels, scalebars, etc.
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsLayout : public QGraphicsScene, public QgsExpressionContextGenerator
{
    Q_OBJECT

  public:

    //! Preset item z-values, to ensure correct stacking
    enum ZValues
    {
      ZPage = 0, //!< Z-value for page (paper) items
      ZItem = 1, //!< Minimum z value for items
      ZGrid = 9998, //!< Z-value for page grids
      ZGuide = 9999, //!< Z-value for page guides
      ZMapTool = 10000, //!< Z-value for temporary map tool items
      ZSnapIndicator = 10001, //!< Z-value for snapping indicator
    };

    /**
     * Construct a new layout linked to the specified \a project.
     *
     * If the layout is a "new" layout (as opposed to a layout which will
     * restore a previous state from XML) then initializeDefaults() should be
     * called on the new layout.
     */
    QgsLayout( QgsProject *project );

    ~QgsLayout();

    /**
     * Initializes an empty layout, e.g. by adding a default page to the layout. This should be called after creating
     * a new layout.
     */
    void initializeDefaults();

    /**
     * The project associated with the layout. Used to get access to layers, map themes,
     * relations and various other bits. It is never null.
     *
     */
    QgsProject *project() const;

    /**
     * Returns the layout's name.
     * \see setName()
     */
    QString name() const { return mName; }

    /**
     * Sets the layout's name.
     * \see name()
     */
    void setName( const QString &name ) { mName = name; }

    /**
     * Sets the native measurement \a units for the layout. These also form the default unit
     * for measurements for the layout.
     * \see units()
     * \see convertToLayoutUnits()
    */
    void setUnits( QgsUnitTypes::LayoutUnit units ) { mUnits = units; }

    /**
     * Returns the native units for the layout.
     * \see setUnits()
     * \see convertToLayoutUnits()
    */
    QgsUnitTypes::LayoutUnit units() const { return mUnits; }

    /**
     * Converts a measurement into the layout's native units.
     * \returns length of measurement in layout units
     * \see convertFromLayoutUnits()
     * \see units()
    */
    double convertToLayoutUnits( const QgsLayoutMeasurement &measurement ) const;

    /**
     * Converts a size into the layout's native units.
     * \returns size of measurement in layout units
     * \see convertFromLayoutUnits()
     * \see units()
    */
    QSizeF convertToLayoutUnits( const QgsLayoutSize &size ) const;

    /**
     * Converts a \a point into the layout's native units.
     * \returns point in layout units
     * \see convertFromLayoutUnits()
     * \see units()
     */
    QPointF convertToLayoutUnits( const QgsLayoutPoint &point ) const;

    /**
     * Converts a \a length measurement from the layout's native units to a specified target \a unit.
     * \returns length of measurement in specified units
     * \see convertToLayoutUnits()
     * \see units()
    */
    QgsLayoutMeasurement convertFromLayoutUnits( const double length, const QgsUnitTypes::LayoutUnit unit ) const;

    /**
     * Converts a \a size from the layout's native units to a specified target \a unit.
     * \returns size of measurement in specified units
     * \see convertToLayoutUnits()
     * \see units()
    */
    QgsLayoutSize convertFromLayoutUnits( const QSizeF &size, const QgsUnitTypes::LayoutUnit unit ) const;

    /**
     * Converts a \a point from the layout's native units to a specified target \a unit.
     * \returns point in specified units
     * \see convertToLayoutUnits()
     * \see units()
    */
    QgsLayoutPoint convertFromLayoutUnits( const QPointF &point, const QgsUnitTypes::LayoutUnit unit ) const;

    /**
     * Returns a reference to the layout's context, which stores information relating to the
     * current context and rendering settings for the layout.
     */
    QgsLayoutContext &context() { return mContext; }

    /**
     * Returns a reference to the layout's context, which stores information relating to the
     * current context and rendering settings for the layout.
     */
    SIP_SKIP const QgsLayoutContext &context() const { return mContext; }

    /**
     * Returns a reference to the layout's snapper, which stores handles layout snap grids and lines
     * and snapping points to the nearest matching point.
     */
    QgsLayoutSnapper &snapper() { return mSnapper; }

    /**
     * Returns a reference to the layout's snapper, which stores handles layout snap grids and lines
     * and snapping points to the nearest matching point.
     */
    SIP_SKIP const QgsLayoutSnapper &snapper() const { return mSnapper; }

    /**
     * Returns a reference to the layout's grid settings, which stores settings relating
     * to grid appearance, spacing and offsets.
     */
    QgsLayoutGridSettings &gridSettings() { return mGridSettings; }

    /**
     * Returns a reference to the layout's grid settings, which stores settings relating
     * to grid appearance, spacing and offsets.
     */
    SIP_SKIP const QgsLayoutGridSettings &gridSettings() const { return mGridSettings; }

    /**
     * Returns a reference to the layout's guide collection, which manages page snap guides.
     */
    QgsLayoutGuideCollection &guides();

    /**
     * Returns a reference to the layout's guide collection, which manages page snap guides.
     */
    SIP_SKIP const QgsLayoutGuideCollection &guides() const;

    /**
     * Creates an expression context relating to the layout's current state. The context includes
     * scopes for global, project, layout and layout context properties.
     */
    QgsExpressionContext createExpressionContext() const override;

    /**
     * Set a custom property for the layout.
     * \param key property key. If a property with the same key already exists it will be overwritten.
     * \param value property value
     * \see customProperty()
     * \see removeCustomProperty()
     * \see customProperties()
     */
    void setCustomProperty( const QString &key, const QVariant &value );

    /**
     * Read a custom property from the layout.
     * \param key property key
     * \param defaultValue default value to return if property with matching key does not exist
     * \returns value of matching property
     * \see setCustomProperty()
     * \see removeCustomProperty()
     * \see customProperties()
     */
    QVariant customProperty( const QString &key, const QVariant &defaultValue = QVariant() ) const;

    /**
     * Remove a custom property from the layout.
     * \param key property key
     * \see setCustomProperty()
     * \see customProperty()
     * \see customProperties()
     */
    void removeCustomProperty( const QString &key );

    /**
     * Return list of keys stored in custom properties for the layout.
     * \see setCustomProperty()
     * \see customProperty()
     * \see removeCustomProperty()
     */
    QStringList customProperties() const;

    /**
     * Returns the map item which will be used to generate corresponding world files when the
     * layout is exported. If no map was explicitly set via setReferenceMap(), the largest
     * map in the layout will be returned (or nullptr if there are no maps in the layout).
     * \see setReferenceMap()
     * \see generateWorldFile()
     */
    //TODO
    QgsLayoutItemMap *referenceMap() const;

    /**
     * Sets the \a map item which will be used to generate corresponding world files when the
     * layout is exported.
     * \see referenceMap()
     * \see setGenerateWorldFile()
     */
    //TODO
    void setReferenceMap( QgsLayoutItemMap *map );

    /**
     * Returns a pointer to the layout's page collection, which stores and manages
     * page items in the layout.
     */
    QgsLayoutPageCollection *pageCollection();

    /**
     * Returns a pointer to the layout's page collection, which stores and manages
     * page items in the layout.
     */
    SIP_SKIP const QgsLayoutPageCollection *pageCollection() const;

    /**
     * Calculates the bounds of all non-gui items in the layout. Ignores snap lines, mouse handles
     * and other cosmetic items.
     * \param ignorePages set to true to ignore page items
     * \param margin optional marginal (in percent, e.g., 0.05 = 5% ) to add around items
     * \returns layout bounds, in layout units.
     */
    QRectF layoutBounds( bool ignorePages = false, double margin = 0.0 ) const;

    /**
     * Adds an \a item to the layout. This should be called instead of the base class addItem()
     * method. Ownership of the item is transferred to the layout.
     */
    void addLayoutItem( QgsLayoutItem *item SIP_TRANSFER );

  public slots:

    /**
     * Updates the scene bounds of the layout.
     */
    void updateBounds();

  signals:

    /**
     * Emitted whenever the expression variables stored in the layout have been changed.
     */
    void variablesChanged();

  private:

    QgsProject *mProject = nullptr;

    QString mName;

    QgsObjectCustomProperties mCustomProperties;

    QgsUnitTypes::LayoutUnit mUnits = QgsUnitTypes::LayoutMillimeters;
    QgsLayoutContext mContext;
    QgsLayoutSnapper mSnapper;
    QgsLayoutGridSettings mGridSettings;

    std::unique_ptr< QgsLayoutPageCollection > mPageCollection;

    std::unique_ptr< QgsLayoutGuideCollection > mGuideCollection;

};

#endif //QGSLAYOUT_H



