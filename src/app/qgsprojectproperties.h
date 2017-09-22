/***************************************************************************
                             qgsprojectproperties.h
       Set various project properties (inherits qgsprojectpropertiesbase)
                              -------------------
  begin                : May 18, 2004
  copyright            : (C) 2004 by Gary E.Sherman
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


#include "qgsoptionsdialogbase.h"
#include "ui_qgsprojectpropertiesbase.h"
#include "qgis.h"
#include "qgsunittypes.h"
#include "qgsguiutils.h"
#include "qgshelp.h"
#include "qgis_app.h"

class QgsMapCanvas;
class QgsRelationManagerDialog;
class QgsStyle;
class QgsExpressionContext;
class QgsLayerTreeGroup;

/** Dialog to set project level properties

  \note actual state is stored in QgsProject singleton instance

 */
class APP_EXPORT QgsProjectProperties : public QgsOptionsDialogBase, private Ui::QgsProjectPropertiesBase
{
    Q_OBJECT

  public:
    //! Constructor
    QgsProjectProperties( QgsMapCanvas *mapCanvas, QWidget *parent = nullptr, Qt::WindowFlags fl = QgsGuiUtils::ModalDialogFlags );


    ~QgsProjectProperties();

    /**
       Every project has a title
     */
    QString title() const;
    void title( QString const &title );

    //! Accessor for projection
    QString projectionWkt();

  public slots:

    /**
     * Slot called when apply button is pressed or dialog is accepted
     */
    void apply();

    /**
     * Slot to show the projections tab when the dialog is opened
     */
    void showProjectionsTab();

    /** Let the user add a scale to the list of project scales
     * used in scale combobox instead of global ones */
    void on_pbnAddScale_clicked();

    /** Let the user remove a scale from the list of project scales
     * used in scale combobox instead of global ones */
    void on_pbnRemoveScale_clicked();

    //! Let the user load scales from file
    void on_pbnImportScales_clicked();

    //! Let the user load scales from file
    void on_pbnExportScales_clicked();

    //! A scale in the list of project scales changed
    void scaleItemChanged( QListWidgetItem *changedScaleItem );

    /**
     * Set WMS default extent to current canvas extent
     */
    void on_pbnWMSExtCanvas_clicked();
    void on_pbnWMSAddSRS_clicked();
    void on_pbnWMSRemoveSRS_clicked();
    void on_pbnWMSSetUsedSRS_clicked();
    void on_mAddWMSComposerButton_clicked();
    void on_mRemoveWMSComposerButton_clicked();
    void on_mAddLayerRestrictionButton_clicked();
    void on_mRemoveLayerRestrictionButton_clicked();
    void on_mWMSInspireScenario1_toggled( bool on );
    void on_mWMSInspireScenario2_toggled( bool on );

    /**
     * Slots to select/deselect all the WFS layers
     */
    void on_pbnWFSLayersSelectAll_clicked();
    void on_pbnWFSLayersDeselectAll_clicked();

    /**
     * Slots to select/deselect all the WCS layers
     */
    void on_pbnWCSLayersSelectAll_clicked();
    void on_pbnWCSLayersDeselectAll_clicked();

    /**
     * Slots to launch OWS test
     */
    void on_pbnLaunchOWSChecker_clicked();

    /**
     * Slots for Styles
     */
    void on_pbtnStyleManager_clicked();
    void on_pbtnStyleMarker_clicked();
    void on_pbtnStyleLine_clicked();
    void on_pbtnStyleFill_clicked();
    void on_pbtnStyleColorRamp_clicked();

    /**
     * Slot to link WFS checkboxes
     */
    void cbxWFSPubliedStateChanged( int aIdx );

    /**
     * Slot to link WCS checkboxes
     */
    void cbxWCSPubliedStateChanged( int aIdx );

    /**
      * If user changes the CRS, set the corresponding map units
      */
    void srIdUpdated();

    /* Update ComboBox accorindg to the selected new index
     * Also sets the new selected Ellipsoid. */
    void updateEllipsoidUI( int newIndex );

    //! sets the right ellipsoid for measuring (from settings)
    void projectionSelectorInitialized();

    void on_mButtonAddColor_clicked();

  signals:
    //! Signal used to inform listeners that the mouse display precision may have changed
    void displayPrecisionChanged();

    //! Signal used to inform listeners that project scale list may have changed
    void scalesChanged( const QStringList &scales = QStringList() );

  private:

    //! Formats for displaying coordinates
    enum CoordinateFormat
    {
      DecimalDegrees, //!< Decimal degrees
      DegreesMinutes, //!< Degrees, decimal minutes
      DegreesMinutesSeconds, //!< Degrees, minutes, seconds
      MapUnits, //! Show coordinates in map units
    };

    QgsRelationManagerDialog *mRelationManagerDlg = nullptr;
    QgsMapCanvas *mMapCanvas = nullptr;
    QgsStyle *mStyle = nullptr;

    void populateStyles();
    void editSymbol( QComboBox *cbo );

    /**
     * Function to save non-base dialog states
     */
    void saveState();

    /**
     * Function to restore non-base dialog states
     */
    void restoreState();

    /**
     * Reset the Python macros
     */
    void resetPythonMacros();

    // List for all ellispods, also None and Custom
    struct EllipsoidDefs
    {
      QString acronym;
      QString description;
      double semiMajor;
      double semiMinor;
    };
    QList<EllipsoidDefs> mEllipsoidList;
    int mEllipsoidIndex;

    //! Check OWS configuration
    void checkOWS( QgsLayerTreeGroup *treeGroup, QStringList &owsNames, QStringList &encodingMessages );

    //! Populates list with ellipsoids from Sqlite3 db
    void populateEllipsoidList();

    //! Create a new scale item and add it to the list of scales
    QListWidgetItem *addScaleToScaleList( const QString &newScale );

    //! Add a scale item to the list of scales
    void addScaleToScaleList( QListWidgetItem *newItem );

    static const char *GEO_NONE_DESC;

    void updateGuiForMapUnits( QgsUnitTypes::DistanceUnit units );

    void showHelp();
};
