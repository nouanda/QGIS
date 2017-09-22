/***************************************************************************
    qgisexpressionbuilderwidget.h - A generic expression string builder widget.
     --------------------------------------
    Date                 :  29-May-2011
    Copyright            : (C) 2011 by Nathan Woodrow
    Email                : woodrow.nathan at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSEXPRESSIONBUILDER_H
#define QGSEXPRESSIONBUILDER_H

#include <QWidget>
#include "qgis.h"
#include "ui_qgsexpressionbuilder.h"
#include "qgsdistancearea.h"
#include "qgsexpressioncontext.h"
#include "qgsfeature.h"

#include "QStandardItemModel"
#include "QStandardItem"
#include "QSortFilterProxyModel"
#include "QStringListModel"
#include "qgis_gui.h"

class QgsFields;
class QgsExpressionHighlighter;
class QgsRelation;

/** \ingroup gui
 * An expression item that can be used in the QgsExpressionBuilderWidget tree.
  */
class GUI_EXPORT QgsExpressionItem : public QStandardItem
{
  public:
    enum ItemType
    {
      Header,
      Field,
      ExpressionNode
    };

    QgsExpressionItem( const QString &label,
                       const QString &expressionText,
                       const QString &helpText,
                       QgsExpressionItem::ItemType itemType = ExpressionNode )
      : QStandardItem( label )
    {
      mExpressionText = expressionText;
      mHelpText = helpText;
      mType = itemType;
      setData( itemType, ITEM_TYPE_ROLE );
    }

    QgsExpressionItem( const QString &label,
                       const QString &expressionText,
                       QgsExpressionItem::ItemType itemType = ExpressionNode )
      : QStandardItem( label )
    {
      mExpressionText = expressionText;
      mType = itemType;
      setData( itemType, ITEM_TYPE_ROLE );
    }

    QString getExpressionText() const { return mExpressionText; }

    /** Get the help text that is associated with this expression item.
      *
      * \returns The help text.
      */
    QString getHelpText() const { return mHelpText; }

    /** Set the help text for the current item
      *
      * \note The help text can be set as a html string.
      */
    void setHelpText( const QString &helpText ) { mHelpText = helpText; }

    /** Get the type of expression item, e.g., header, field, ExpressionNode.
      *
      * \returns The QgsExpressionItem::ItemType
      */
    QgsExpressionItem::ItemType getItemType() const { return mType; }

    //! Custom sort order role
    static const int CUSTOM_SORT_ROLE = Qt::UserRole + 1;
    //! Item type role
    static const int ITEM_TYPE_ROLE = Qt::UserRole + 2;

  private:
    QString mExpressionText;
    QString mHelpText;
    QgsExpressionItem::ItemType mType;

};

/** \ingroup gui
 * Search proxy used to filter the QgsExpressionBuilderWidget tree.
  * The default search for a tree model only searches top level this will handle one
  * level down
  */
class GUI_EXPORT QgsExpressionItemSearchProxy : public QSortFilterProxyModel
{
    Q_OBJECT

  public:
    QgsExpressionItemSearchProxy();

    bool filterAcceptsRow( int source_row, const QModelIndex &source_parent ) const override;

  protected:

    bool lessThan( const QModelIndex &left, const QModelIndex &right ) const override;
};


/** \ingroup gui
 * A reusable widget that can be used to build a expression string.
  * See QgsExpressionBuilderDialog for example of usage.
  */
class GUI_EXPORT QgsExpressionBuilderWidget : public QWidget, private Ui::QgsExpressionBuilderWidgetBase
{
    Q_OBJECT
  public:

    /**
     * Create a new expression builder widget with an optional parent.
     */
    QgsExpressionBuilderWidget( QWidget *parent SIP_TRANSFERTHIS = 0 );
    ~QgsExpressionBuilderWidget();

    /** Sets layer in order to get the fields and values
      * \note this needs to be called before calling loadFieldNames().
      */
    void setLayer( QgsVectorLayer *layer );

    /** Loads all the field names from the layer.
      * @remarks Should this really be public couldn't we just do this for the user?
      */
    void loadFieldNames();

    void loadFieldNames( const QgsFields &fields );

    /** Loads field names and values from the specified map.
     *  \note The field values must be quoted appropriately if they are strings.
     *  \since QGIS 2.12
     */
    void loadFieldsAndValues( const QMap<QString, QStringList> &fieldValues );

    //! Sets geometry calculator used in distance/area calculations.
    void setGeomCalculator( const QgsDistanceArea &da );

    /** Gets the expression string that has been set in the expression area.
      * \returns The expression as a string. */
    QString expressionText();

    //! Sets the expression string for the widget
    void setExpressionText( const QString &expression );

    /** Returns the expression context for the widget. The context is used for the expression
     * preview result and for populating the list of available functions and variables.
     * \see setExpressionContext
     * \since QGIS 2.12
     */
    QgsExpressionContext expressionContext() const { return mExpressionContext; }

    /** Sets the expression context for the widget. The context is used for the expression
     * preview result and for populating the list of available functions and variables.
     * \param context expression context
     * \see expressionContext
     * \since QGIS 2.12
     */
    void setExpressionContext( const QgsExpressionContext &context );

    /** Registers a node item for the expression builder.
      * \param group The group the item will be show in the tree view.  If the group doesn't exsit it will be created.
      * \param label The label that is show to the user for the item in the tree.
      * \param expressionText The text that is inserted into the expression area when the user double clicks on the item.
      * \param helpText The help text that the user will see when item is selected.
      * \param type The type of the expression item.
      * \param highlightedItem set to true to make the item highlighted, which inserts a bold copy of the item at the top level
      * \param sortOrder sort ranking for item
      */
    void registerItem( const QString &group, const QString &label, const QString &expressionText,
                       const QString &helpText = "",
                       QgsExpressionItem::ItemType type = QgsExpressionItem::ExpressionNode,
                       bool highlightedItem = false, int sortOrder = 1 );

    bool isExpressionValid();

    /**
     * Adds the current expression to the given collection.
     * By default it is saved to the collection "generic".
     */
    void saveToRecent( const QString &collection = "generic" );

    /**
     * Loads the recent expressions from the given collection.
     * By default it is loaded from the collection "generic".
     */
    void loadRecent( const QString &collection = "generic" );

    /** Create a new file in the function editor
     */
    void newFunctionFile( const QString &fileName = "scratch" );

    /** Save the current function editor text to the given file.
     */
    void saveFunctionFile( QString fileName );

    /** Load code from the given file into the function editor
     */
    void loadCodeFromFile( QString path );

    /** Load code into the function editor
     */
    void loadFunctionCode( const QString &code );

    /** Update the list of function files found at the given path
     */
    void updateFunctionFileList( const QString &path );

    /**
     * Returns a pointer to the dialog's function item model.
     * This method is exposed for testing purposes only - it should not be used to modify the model.
     * \since QGIS 3.0
     */
    QStandardItemModel *model();

    /**
     * Returns the project currently associated with the widget.
     * \see setProject()
     * \since QGIS 3.0
     */
    QgsProject *project();

    /**
     * Sets the \a project currently associated with the widget. This
     * controls which layers and relations and other project-specific items are shown in the widget.
     * \see project()
     * \since QGIS 3.0
     */
    void setProject( QgsProject *project );

  public slots:

    /**
     * Load sample values into the sample value area
     */
    void loadSampleValues();

    /**
     * Load all unique values from the set layer into the sample area
     */
    void loadAllValues();

    /**
     * Auto save the current Python function code.
     */
    void autosave();

    /**
     * Enabled or disable auto saving. When enabled Python scripts will be auto saved
     * when text changes.
     * \param enabled True to enable auto saving.
     */
    void setAutoSave( bool enabled ) { mAutoSave = enabled; }

  private slots:
    void showContextMenu( QPoint );
    void setExpressionState( bool state );
    void currentChanged( const QModelIndex &index, const QModelIndex & );
    void operatorButtonClicked();
    void on_btnRun_pressed();
    void on_btnNewFile_pressed();
    void on_cmbFileNames_currentItemChanged( QListWidgetItem *item, QListWidgetItem *lastitem );
    void on_expressionTree_doubleClicked( const QModelIndex &index );
    void on_txtExpressionString_textChanged();
    void on_txtSearchEdit_textChanged();
    void on_txtSearchEditValues_textChanged();
    void on_lblPreview_linkActivated( const QString &link );
    void on_mValuesListView_doubleClicked( const QModelIndex &index );
    void on_txtPython_textChanged();

  signals:

    /** Emitted when the user changes the expression in the widget.
      * Users of this widget should connect to this signal to decide if to let the user
      * continue.
      * \param isValid Is true if the expression the user has typed is valid.
      */
    void expressionParsed( bool isValid );

  protected:
    void showEvent( QShowEvent *e );

  private:
    void runPythonCode( const QString &code );
    void updateFunctionTree();
    void fillFieldValues( const QString &fieldName, int countLimit );
    QString loadFunctionHelp( QgsExpressionItem *functionName );
    QString helpStylesheet() const;

    void loadExpressionContext();

    //! Loads current project relations names/id into the expression help tree
    void loadRelations();

    //! Loads current project layer names/ids into the expression help tree
    void loadLayers();

    /** Registers a node item for the expression builder, adding multiple items when the function exists in multiple groups
      * \param groups The groups the item will be show in the tree view.  If a group doesn't exist it will be created.
      * \param label The label that is show to the user for the item in the tree.
      * \param expressionText The text that is inserted into the expression area when the user double clicks on the item.
      * \param helpText The help text that the user will see when item is selected.
      * \param type The type of the expression item.
      * \param highlightedItem set to true to make the item highlighted, which inserts a bold copy of the item at the top level
      * \param sortOrder sort ranking for item
      */
    void registerItemForAllGroups( const QStringList &groups, const QString &label, const QString &expressionText,
                                   const QString &helpText = "",
                                   QgsExpressionItem::ItemType type = QgsExpressionItem::ExpressionNode,
                                   bool highlightedItem = false, int sortOrder = 1 );

    /**
     * Returns a HTML formatted string for use as a \a relation item help.
     */
    QString formatRelationHelp( const QgsRelation &relation ) const;

    /**
     * Returns a HTML formatted string for use as a \a layer item help.
     */
    QString formatLayerHelp( const QgsMapLayer *layer ) const;

    bool mAutoSave;
    QString mFunctionsPath;
    QgsVectorLayer *mLayer = nullptr;
    QStandardItemModel *mModel = nullptr;
    QStringListModel *mValuesModel = nullptr;
    QSortFilterProxyModel *mProxyValues = nullptr;
    QgsExpressionItemSearchProxy *mProxyModel = nullptr;
    QMap<QString, QgsExpressionItem *> mExpressionGroups;
    QgsExpressionHighlighter *highlighter = nullptr;
    bool mExpressionValid;
    QgsDistanceArea mDa;
    QString mRecentKey;
    QMap<QString, QStringList> mFieldValues;
    QgsExpressionContext mExpressionContext;
    QPointer< QgsProject > mProject;
};

#endif // QGSEXPRESSIONBUILDER_H
