/***************************************************************************
    qgscolorbutton.h - Color button
     --------------------------------------
    Date                 : 12-Dec-2006
    Copyright            : (C) 2006 by Tom Elwertowski
    Email                : telwertowski at users dot sourceforge dot net
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSCOLORBUTTON_H
#define QGSCOLORBUTTON_H

#include <QColorDialog>
#include <QToolButton>
#include <QTemporaryFile>
#include "qgis_gui.h"
#include "qgis.h"

class QMimeData;
class QgsColorSchemeRegistry;
class QgsPanelWidget;

/** \ingroup gui
 * \class QgsColorButton
 * A cross platform button subclass for selecting colors. Will open a color chooser dialog when clicked.
 * Offers live updates to button from color chooser dialog. An attached drop-down menu allows for copying
 * and pasting colors, picking colors from the screen, and selecting colors from color swatch grids.
 * \since QGIS 2.5
 */
class GUI_EXPORT QgsColorButton : public QToolButton
{

#ifdef SIP_RUN
    SIP_CONVERT_TO_SUBCLASS_CODE
    if ( qobject_cast<QgsColorButton *>( sipCpp ) )
      sipType = sipType_QgsColorButton;
    else
      sipType = NULL;
    SIP_END
#endif


    Q_OBJECT
    Q_ENUMS( Behavior )
    Q_PROPERTY( QString colorDialogTitle READ colorDialogTitle WRITE setColorDialogTitle )
    Q_PROPERTY( bool acceptLiveUpdates READ acceptLiveUpdates WRITE setAcceptLiveUpdates )
    Q_PROPERTY( QColor color READ color WRITE setColor )
    Q_PROPERTY( bool allowOpacity READ allowOpacity WRITE setAllowOpacity )
    Q_PROPERTY( bool showMenu READ showMenu WRITE setShowMenu )
    Q_PROPERTY( Behavior behavior READ behavior WRITE setBehavior )
    Q_PROPERTY( QColor defaultColor READ defaultColor WRITE setDefaultColor )
    Q_PROPERTY( bool showNoColor READ showNoColor WRITE setShowNoColor )
    Q_PROPERTY( QString noColorString READ noColorString WRITE setNoColorString )
    Q_PROPERTY( QString context READ context WRITE setContext )

  public:

    /** Specifies the behavior when the button is clicked
     */
    enum Behavior
    {
      ShowDialog = 0, //!< Show a color picker dialog when clicked
      SignalOnly //!< Emit colorClicked signal only, no dialog
    };

    /** Construct a new color ramp button.
     * Use \a parent to attach a parent QWidget to the dialog.
     * Use \a cdt string to define the title to show in the color ramp dialog
     * Use a color scheme \a registry for color swatch grids to show in the drop-down menu. If not specified,
     * the button will use the global color scheme registry instead
     */
    QgsColorButton( QWidget *parent SIP_TRANSFERTHIS = nullptr, const QString &cdt = "", QgsColorSchemeRegistry *registry = nullptr );

    virtual QSize minimumSizeHint() const override;
    virtual QSize sizeHint() const override;

    /** Return the currently selected color.
     * \returns currently selected color
     * \see setColor
     */
    QColor color() const;

    /**
     * Sets whether opacity modification (transparency) is permitted
     * for the color. Defaults to false.
     * \param allowOpacity set to true to allow opacity modification
     * \see allowOpacity()
     * \since QGIS 3.0
     */
    void setAllowOpacity( const bool allowOpacity );

    /**
     * Returns whether opacity modification (transparency) is permitted
     * for the color.
     * \returns true if opacity modification is allowed
     * \see setAllowOpacity()
     * \since QGIS 3.0
     */
    bool allowOpacity() const { return mAllowOpacity; }

    /** Set the title for the color chooser dialog window.
     * \param title Title for the color chooser dialog
     * \see colorDialogTitle
     */
    void setColorDialogTitle( const QString &title );

    /** Returns the title for the color chooser dialog window.
     * \returns title for the color chooser dialog
     * \see setColorDialogTitle
     */
    QString colorDialogTitle() const;

    /** Returns whether the button accepts live updates from QColorDialog.
     * \returns true if the button will be accepted immediately when the dialog's color changes
     * \see setAcceptLiveUpdates
     */
    bool acceptLiveUpdates() const { return mAcceptLiveUpdates; }

    /** Sets whether the button accepts live updates from QColorDialog. Live updates may cause changes
     * that are not undoable on QColorDialog cancel.
     * \param accept set to true to enable live updates
     * \see acceptLiveUpdates
     */
    void setAcceptLiveUpdates( const bool accept ) { mAcceptLiveUpdates = accept; }

    /** Sets whether the drop-down menu should be shown for the button. The default behavior is to
     * show the menu.
     * \param showMenu set to false to hide the drop-down menu
     * \see showMenu
     */
    void setShowMenu( const bool showMenu );

    /** Returns whether the drop-down menu is shown for the button.
     * \returns true if drop-down menu is shown
     * \see setShowMenu
     */
    bool showMenu() const { return menu() ? true : false; }

    /** Sets the behavior for when the button is clicked. The default behavior is to show
     * a color picker dialog.
     * \param behavior behavior when button is clicked
     * \see behavior
     */
    void setBehavior( const Behavior behavior );

    /** Returns the behavior for when the button is clicked.
     * \returns behavior when button is clicked
     * \see setBehavior
     */
    Behavior behavior() const { return mBehavior; }

    /** Sets the default color for the button, which is shown in the button's drop-down menu for the
     * "default color" option.
     * \param color default color for the button. Set to an invalid QColor to disable the default color
     * option.
     * \see defaultColor
     */
    void setDefaultColor( const QColor &color );

    /** Returns the default color for the button, which is shown in the button's drop-down menu for the
     * "default color" option.
     * \returns default color for the button. Returns an invalid QColor if the default color
     * option is disabled.
     * \see setDefaultColor
     */
    QColor defaultColor() const { return mDefaultColor; }

    /** Sets whether the "no color" option should be shown in the button's drop-down menu. If selected,
     * the "no color" option sets the color button's color to a totally transparent color.
     * \param showNoColorOption set to true to show the no color option. This is disabled by default.
     * \see showNoColor
     * \see setNoColorString
     * \note The "no color" option is only shown if the color button is set to show an alpha channel in the color
     * dialog (see setColorDialogOptions)
     */
    void setShowNoColor( const bool showNoColorOption ) { mShowNoColorOption = showNoColorOption; }

    /** Returns whether the "no color" option is shown in the button's drop-down menu. If selected,
     * the "no color" option sets the color button's color to a totally transparent color.
     * \returns true if the no color option is shown.
     * \see setShowNoColor
     * \see noColorString
     * \note The "no color" option is only shown if the color button is set to show an alpha channel in the color
     * dialog (see setColorDialogOptions)
     */
    bool showNoColor() const { return mShowNoColorOption; }

    /** Sets the string to use for the "no color" option in the button's drop-down menu.
     * \param noColorString string to use for the "no color" menu option
     * \see noColorString
     * \see setShowNoColor
     * \note The "no color" option is only shown if the color button is set to show an alpha channel in the color
     * dialog (see setColorDialogOptions)
     */
    void setNoColorString( const QString &noColorString ) { mNoColorString = noColorString; }

    /** Sets whether a set to null (clear) option is shown in the button's drop-down menu.
     * \param showNull set to true to show a null option
     * \since QGIS 2.16
     * \see showNull()
     * \see isNull()
     */
    void setShowNull( bool showNull );

    /** Returns whether the set to null (clear) option is shown in the button's drop-down menu.
     * \since QGIS 2.16
     * \see setShowNull()
     * \see isNull()
     */
    bool showNull() const;

    /** Returns true if the current color is null.
     * \since QGIS 2.16
     * \see setShowNull()
     * \see showNull()
     */
    bool isNull() const;

    /** Returns the string used for the "no color" option in the button's drop-down menu.
     * \returns string used for the "no color" menu option
     * \see setNoColorString
     * \see showNoColor
     * \note The "no color" option is only shown if the color button is set to show an alpha channel in the color
     * dialog (see setColorDialogOptions)
     */
    QString noColorString() const { return mNoColorString; }

    /** Sets the context string for the color button. The context string is passed to all color swatch
     * grids shown in the button's drop-down menu, to allow them to customise their display colors
     * based on the context.
     * \param context context string for the color button's color swatch grids
     * \see context
     */
    void setContext( const QString &context ) { mContext = context; }

    /** Returns the context string for the color button. The context string is passed to all color swatch
     * grids shown in the button's drop-down menu, to allow them to customise their display colors
     * based on the context.
     * \returns context string for the color button's color swatch grids
     * \see setContext
     */
    QString context() const { return mContext; }

    /** Sets the color scheme registry for the button, which controls the color swatch grids
     * that are shown in the button's drop-down menu.
     * \param registry color scheme registry for the button. Set to 0 to hide all color
     * swatch grids from the button's drop-down menu.
     * \see colorSchemeRegistry
     */
    void setColorSchemeRegistry( QgsColorSchemeRegistry *registry ) { mColorSchemeRegistry = registry; }

    /** Returns the color scheme registry for the button, which controls the color swatch grids
     * that are shown in the button's drop-down menu.
     * \returns color scheme registry for the button. If returned value is 0 then all color
     * swatch grids are hidden from the button's drop-down menu.
     * \see setColorSchemeRegistry
     */
    QgsColorSchemeRegistry *colorSchemeRegistry() { return mColorSchemeRegistry; }

  public slots:

    /** Sets the current color for the button. Will emit a colorChanged signal if the color is different
     * to the previous color.
     * \param color new color for the button
     * \see color
     */
    void setColor( const QColor &color );

    /** Sets the background pixmap for the button based upon color and transparency.
     * Call directly to update background after adding/removing QColorDialog::ShowAlphaChannel option
     * but the color has not changed, i.e. setColor() wouldn't update button and
     * you want the button to retain the set color's alpha component regardless
     * \param color Color for button background. If no color is specified, the button's current
     * color will be used
     */
    void setButtonBackground( const QColor &color = QColor() );

    /** Copies the current color to the clipboard
     * \see pasteColor
     */
    void copyColor();

    /** Pastes a color from the clipboard to the color button. If clipboard does not contain a valid
     * color or string representation of a color, then no change is applied.
     * \see copyColor
     */
    void pasteColor();

    /** Activates the color picker tool, which allows for sampling a color from anywhere on the screen
     */
    void activatePicker();

    /** Sets color to a totally transparent color.
     * \note If the color button is not set to show an opacity channel in the color
     * dialog (see setColorDialogOptions) then the color will not be changed.
     * \see setToNull()
     */
    void setToNoColor();

    /** Sets color to the button's default color, if set.
     * \see setDefaultColor
     * \see defaultColor
     * \see setToNull()
     */
    void setToDefaultColor();

    /** Sets color to null.
     * \see setToDefaultColor()
     * \see setToNoColor()
     * \since QGIS 2.16
     */
    void setToNull();

  signals:

    /** Is emitted whenever a new color is set for the button. The color is always valid.
     * In case the new color is the same no signal is emitted, to avoid infinite loops.
     * \param color New color
     */
    void colorChanged( const QColor &color );

    /** Emitted when the button is clicked, if the button's behavior is set to SignalOnly
     * \param color button color
     * \see setBehavior
     * \see behavior
     */
    void colorClicked( const QColor &color );

  protected:

    bool event( QEvent *e ) override;
    void changeEvent( QEvent *e ) override;
    void showEvent( QShowEvent *e ) override;
    void resizeEvent( QResizeEvent *event ) override;

    /** Returns a checkboard pattern pixmap for use as a background to transparent colors
     */
    static const QPixmap &transparentBackground();

    /**
     * Reimplemented to detect right mouse button clicks on the color button and allow dragging colors
     */
    void mousePressEvent( QMouseEvent *e ) override;

    /**
     * Reimplemented to allow dragging colors from button
     */
    void mouseMoveEvent( QMouseEvent *e ) override;

    /**
     * Reimplemented to allow color picking
     */
    void mouseReleaseEvent( QMouseEvent *e ) override;

    /**
     * Reimplemented to allow canceling color pick via keypress, and sample via space bar press
     */
    void keyPressEvent( QKeyEvent *e ) override;

    /**
     * Reimplemented to accept dragged colors
     */
    void dragEnterEvent( QDragEnterEvent *e ) override;

    /**
     * Reimplemented to reset button appearance after drag leave
     */
    void dragLeaveEvent( QDragLeaveEvent *e ) override;

    /**
     * Reimplemented to accept dropped colors
     */
    void dropEvent( QDropEvent *e ) override;

  private:

    Behavior mBehavior;
    QString mColorDialogTitle;
    QColor mColor;
    QSize mMinimumSize;

    QgsColorSchemeRegistry *mColorSchemeRegistry = nullptr;

    QColor mDefaultColor;
    QString mContext;
    bool mAllowOpacity;
    bool mAcceptLiveUpdates;
    bool mColorSet;

    bool mShowNoColorOption;
    QString mNoColorString;
    bool mShowNull;

    QPoint mDragStartPosition;
    bool mPickingColor;

    QMenu *mMenu = nullptr;

    QSize mIconSize;

    /** Attempts to parse mimeData as a color, either via the mime data's color data or by
     * parsing a textual representation of a color.
     * \returns true if mime data could be intrepreted as a color
     * \param mimeData mime data
     * \param resultColor QColor to store evaluated color
     * \see createColorMimeData
     */
    bool colorFromMimeData( const QMimeData *mimeData, QColor &resultColor );

    /** Ends a color picking operation
     * \param eventPos global position of pixel to sample color from
     * \param sampleColor set to true to actually sample the color, false to just cancel
     * the color picking operation
     */
    void stopPicking( QPointF eventPos, bool sampleColor = true );

    /** Create a color icon for display in the drop-down menu
     * \param color for icon
     * \param showChecks set to true to display a checkboard pattern behind
     * transparent colors
     */
    QPixmap createMenuIcon( const QColor &color, const bool showChecks = true );

  private slots:

    void buttonClicked();

    void showColorDialog();

    /** Sets color for button, if valid.
     */
    void setValidColor( const QColor &newColor );

    /** Sets color for button, if valid. The color is treated as a temporary color, and is not
     * added to the recent colors list.
     */
    void setValidTemporaryColor( const QColor &newColor );

    /** Adds a color to the recent colors list
     * \param color to add to recent colors list
     */
    void addRecentColor( const QColor &color );

    /** Creates the drop-down menu entries
     */
    void prepareMenu();
};

#endif
