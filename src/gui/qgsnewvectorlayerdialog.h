/***************************************************************************
                         qgsnewvectorlayerdialog.h  -  description
                             -------------------
    begin                : October 2004
    copyright            : (C) 2004 by Marco Hugentobler
    email                : marco.hugentobler@autoform.ch
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef qgsnewvectorlayerdialog_H
#define qgsnewvectorlayerdialog_H

#include "ui_qgsnewvectorlayerdialogbase.h"
#include "qgsguiutils.h"
#include "qgshelp.h"

#include "qgis.h"
#include "qgis_gui.h"

/** \ingroup gui
 * \class QgsNewVectorLayerDialog
 */
class GUI_EXPORT QgsNewVectorLayerDialog: public QDialog, private Ui::QgsNewVectorLayerDialogBase
{
    Q_OBJECT

  public:

    /**
     * Runs the dialog and creates a layer matching the dialog parameters.
     * \returns fileName on success, empty string use aborted, QString() if creation failed
     */
    static QString runAndCreateLayer( QWidget *parent = nullptr, QString *enc = nullptr, const QgsCoordinateReferenceSystem &crs = QgsCoordinateReferenceSystem() );

    QgsNewVectorLayerDialog( QWidget *parent SIP_TRANSFERTHIS = nullptr, Qt::WindowFlags fl = QgsGuiUtils::ModalDialogFlags );
    ~QgsNewVectorLayerDialog();
    //! Returns the selected geometry type
    QgsWkbTypes::Type selectedType() const;
    //! Appends the chosen attribute names and types to at
    void attributes( QList< QPair<QString, QString> > &at ) const;
    //! Returns the file format for storage
    QString selectedFileFormat() const;
    //! Returns the file format for storage
    QString selectedFileEncoding() const;

    /**
     * Returns the selected CRS for the new layer.
     * \see setCrs()
     */
    QgsCoordinateReferenceSystem crs() const;

    /**
     * Sets the \a crs value for the new layer in the dialog.
     * \since QGIS 3.0
     * \see crs()
     */
    void setCrs( const QgsCoordinateReferenceSystem &crs );

  protected slots:
    void on_mAddAttributeButton_clicked();
    void on_mRemoveAttributeButton_clicked();
    void on_mFileFormatComboBox_currentIndexChanged( int index );
    void on_mTypeBox_currentIndexChanged( int index );

    //! Open the associated help
    void showHelp();
    void nameChanged( const QString & );
    void selectionChanged();

  private:
    QPushButton *mOkButton = nullptr;
};

#endif //qgsnewvectorlayerdialog_H
