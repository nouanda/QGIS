/************************************************************************
 * This file has been generated automatically from                      *
 *                                                                      *
 * src/gui/qgscheckablecombobox.h                                       *
 *                                                                      *
 * Do not edit manually ! Edit header and run scripts/sipify.pl again   *
 ************************************************************************/








class QgsCheckableComboBox : QComboBox
{
%Docstring
 QComboBox subclass which allows selecting multiple items.
.. versionadded:: 3.0
*
%End

%TypeHeaderCode
#include "qgscheckablecombobox.h"
%End
  public:

    QgsCheckableComboBox( QWidget *parent /TransferThis/ = 0 );
%Docstring
 Constructor for QgsCheckableComboBox.
%End

    QString separator() const;
%Docstring
 Returns separator used to separate items in the display text.
.. seealso:: setSeparator()
 :rtype: str
%End

    void setSeparator( const QString &separator );
%Docstring
 Set separator used to separate items in the display text.
 \param separator separator to use
.. seealso:: separator()
%End

    QString defaultText() const;
%Docstring
 Returns default text which will be displayed in the widget
 when no items selected.
.. seealso:: setDefaultText()
 :rtype: str
%End

    void setDefaultText( const QString &text );
%Docstring
 Set default text which will be displayed in the widget when
 no items selected.
 \param text default text
.. seealso:: defaultText()
%End

    QStringList checkedItems() const;
%Docstring
 Returns currently checked items.
.. seealso:: setCheckedItems()
 :rtype: list of str
%End

    Qt::CheckState itemCheckState( int index ) const;
%Docstring
 Returns the checked state of the item identified by index
 \param index item index
.. seealso:: setItemCheckState()
.. seealso:: toggleItemCheckState()
 :rtype: Qt.CheckState
%End

    void setItemCheckState( int index, Qt::CheckState state );
%Docstring
 Sets the item check state to state
 \param index item index
 \param state check state
.. seealso:: itemCheckState()
.. seealso:: toggleItemCheckState()
%End

    void toggleItemCheckState( int index );
%Docstring
 Toggles the item check state
 \param index item index
.. seealso:: itemCheckState()
.. seealso:: setItemCheckState()
%End

    virtual void hidePopup();
%Docstring
 Hides the list of items in the combobox if it is currently
 visible and resets the internal state.
%End

    virtual bool eventFilter( QObject *object, QEvent *event );
%Docstring
 Filters events to enable context menu
 :rtype: bool
%End

  signals:

    void checkedItemsChanged( const QStringList &items );
%Docstring
 This signal is emitted whenever the checked items list changed.
%End

  public slots:

    void setCheckedItems( const QStringList &items );
%Docstring
 Set items which should be checked/selected.
 \param items items to select
.. seealso:: checkedItems()
%End

  protected:

    virtual void resizeEvent( QResizeEvent *event );
%Docstring
 Handler for widget resizing
%End

  protected slots:

    void showContextMenu( const QPoint &pos );
%Docstring
 Display context menu which allows selecting/deselecting
 all items at once.
%End

    void selectAllOptions();
%Docstring
 Selects all items.
%End

    void deselectAllOptions();
%Docstring
 Removes selection from all items.
%End

};

/************************************************************************
 * This file has been generated automatically from                      *
 *                                                                      *
 * src/gui/qgscheckablecombobox.h                                       *
 *                                                                      *
 * Do not edit manually ! Edit header and run scripts/sipify.pl again   *
 ************************************************************************/
