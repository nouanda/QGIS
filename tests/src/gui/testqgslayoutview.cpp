/***************************************************************************
    testqgslayoutview.cpp
     --------------------
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

#include "qgstest.h"
#include "qgslayout.h"
#include "qgslayoutview.h"
#include "qgslayoutviewtool.h"
#include "qgslayoutviewmouseevent.h"
#include "qgslayoutitem.h"
#include "qgslayoutviewrubberband.h"
#include "qgslayoutitemregistry.h"
#include "qgslayoutitemguiregistry.h"
#include "qgslayoutitemwidget.h"
#include "qgstestutils.h"
#include "qgsproject.h"
#include "qgsgui.h"
#include <QtTest/QSignalSpy>

class TestQgsLayoutView: public QObject
{
    Q_OBJECT
  private slots:
    void initTestCase(); // will be called before the first testfunction is executed.
    void cleanupTestCase(); // will be called after the last testfunction was executed.
    void init(); // will be called before each testfunction is executed.
    void cleanup(); // will be called after every testfunction.
    void basic();
    void tool();
    void events();
    void guiRegistry();
    void rubberBand();

  private:

};

void TestQgsLayoutView::initTestCase()
{

}

void TestQgsLayoutView::cleanupTestCase()
{
}

void TestQgsLayoutView::init()
{
}

void TestQgsLayoutView::cleanup()
{
}

void TestQgsLayoutView::basic()
{
  QgsProject p;
  QgsLayout *layout = new QgsLayout( &p );
  QgsLayoutView *view = new QgsLayoutView();

  QSignalSpy spyLayoutChanged( view, &QgsLayoutView::layoutSet );
  view->setCurrentLayout( layout );
  QCOMPARE( view->currentLayout(), layout );
  QCOMPARE( spyLayoutChanged.count(), 1 );

  delete view;
  delete layout;
}

void TestQgsLayoutView::tool()
{
  QgsLayoutView *view = new QgsLayoutView();
  QgsLayoutViewTool *tool = new QgsLayoutViewTool( view, QStringLiteral( "name" ) );
  QgsLayoutViewTool *tool2 = new QgsLayoutViewTool( view, QStringLiteral( "name2" ) );

  QVERIFY( tool->isClickAndDrag( QPoint( 0, 10 ), QPoint( 5, 10 ) ) );
  QVERIFY( tool->isClickAndDrag( QPoint( 0, 10 ), QPoint( 5, 15 ) ) );
  QVERIFY( tool->isClickAndDrag( QPoint( 5, 10 ), QPoint( 5, 15 ) ) );
  QVERIFY( !tool->isClickAndDrag( QPoint( 0, 10 ), QPoint( 1, 11 ) ) );
  QVERIFY( !tool->isClickAndDrag( QPoint( 1, 10 ), QPoint( 1, 11 ) ) );
  QVERIFY( !tool->isClickAndDrag( QPoint( 0, 10 ), QPoint( 1, 10 ) ) );
  QVERIFY( !tool->isClickAndDrag( QPoint( 0, 10 ), QPoint( 0, 10 ) ) );

  QSignalSpy spySetTool( view, &QgsLayoutView::toolSet );
  QSignalSpy spyToolActivated( tool, &QgsLayoutViewTool::activated );
  QSignalSpy spyToolActivated2( tool2, &QgsLayoutViewTool::activated );
  QSignalSpy spyToolDeactivated( tool, &QgsLayoutViewTool::deactivated );
  QSignalSpy spyToolDeactivated2( tool2, &QgsLayoutViewTool::deactivated );
  view->setTool( tool );
  QCOMPARE( view->tool(), tool );
  QCOMPARE( spySetTool.count(), 1 );
  QCOMPARE( spyToolActivated.count(), 1 );
  QCOMPARE( spyToolDeactivated.count(), 0 );

  view->setTool( tool2 );
  QCOMPARE( view->tool(), tool2 );
  QCOMPARE( spySetTool.count(), 2 );
  QCOMPARE( spyToolActivated.count(), 1 );
  QCOMPARE( spyToolDeactivated.count(), 1 );
  QCOMPARE( spyToolActivated2.count(), 1 );
  QCOMPARE( spyToolDeactivated2.count(), 0 );

  delete tool2;
  QVERIFY( !view->tool() );
  QCOMPARE( spySetTool.count(), 3 );
  QCOMPARE( spyToolActivated.count(), 1 );
  QCOMPARE( spyToolDeactivated.count(), 1 );
  QCOMPARE( spyToolActivated2.count(), 1 );
  QCOMPARE( spyToolDeactivated2.count(), 1 );

  delete view;
}

class LoggingTool : public QgsLayoutViewTool
{
  public:

    LoggingTool( QgsLayoutView *view )
      : QgsLayoutViewTool( view, "logging" )
    {}

    bool receivedMoveEvent = false;
    void layoutMoveEvent( QgsLayoutViewMouseEvent *event ) override
    {
      receivedMoveEvent = true;
      QCOMPARE( event->layoutPoint().x(), 8.0 );
      QCOMPARE( event->layoutPoint().y(), 6.0 );
    }

    bool receivedDoubleClickEvent = false;
    void layoutDoubleClickEvent( QgsLayoutViewMouseEvent *event ) override
    {
      receivedDoubleClickEvent = true;
      QCOMPARE( event->layoutPoint().x(), 8.0 );
      QCOMPARE( event->layoutPoint().y(), 6.0 );
    }

    bool receivedPressEvent = false;
    void layoutPressEvent( QgsLayoutViewMouseEvent *event ) override
    {
      receivedPressEvent  = true;
      QCOMPARE( event->layoutPoint().x(), 8.0 );
      QCOMPARE( event->layoutPoint().y(), 6.0 );
    }

    bool receivedReleaseEvent = false;
    void layoutReleaseEvent( QgsLayoutViewMouseEvent *event ) override
    {
      receivedReleaseEvent  = true;
      QCOMPARE( event->layoutPoint().x(), 8.0 );
      QCOMPARE( event->layoutPoint().y(), 6.0 );
    }

    bool receivedWheelEvent = false;
    void wheelEvent( QWheelEvent * ) override
    {
      receivedWheelEvent = true;
    }

    bool receivedKeyPressEvent = false;
    void keyPressEvent( QKeyEvent * ) override
    {
      receivedKeyPressEvent  = true;
    }

    bool receivedKeyReleaseEvent = false;
    void keyReleaseEvent( QKeyEvent * ) override
    {
      receivedKeyReleaseEvent = true;
    }
};

void TestQgsLayoutView::events()
{
  QgsProject p;
  QgsLayoutView *view = new QgsLayoutView();
  QgsLayout *layout = new QgsLayout( &p );
  view->setCurrentLayout( layout );
  layout->setSceneRect( 0, 0, 1000, 1000 );
  view->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  view->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
  view->setFrameStyle( 0 );
  view->resize( 100, 100 );
  view->setFixedSize( 100, 100 );
  QCOMPARE( view->width(), 100 );
  QCOMPARE( view->height(), 100 );

  QTransform transform;
  transform.scale( 10, 10 );
  view->setTransform( transform );

  LoggingTool *tool = new LoggingTool( view );
  view->setTool( tool );

  QPointF point( 80, 60 );
  QMouseEvent press( QEvent::MouseButtonPress, point,
                     Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QMouseEvent move( QEvent::MouseMove, point,
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QMouseEvent releases( QEvent::MouseButtonRelease, point,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QMouseEvent dblClick( QEvent::MouseButtonDblClick, point,
                        Qt::LeftButton, Qt::LeftButton, Qt::NoModifier );
  QWheelEvent wheelEvent( point, 10,
                          Qt::LeftButton, Qt::NoModifier );
  QKeyEvent keyPress( QEvent::KeyPress, 10, Qt::NoModifier );
  QKeyEvent keyRelease( QEvent::KeyRelease, 10, Qt::NoModifier );

  view->mouseMoveEvent( &move );
  QVERIFY( tool->receivedMoveEvent );
  view->mousePressEvent( &press );
  QVERIFY( tool->receivedPressEvent );
  view->mouseReleaseEvent( &releases );
  QVERIFY( tool->receivedReleaseEvent );
  view->mouseDoubleClickEvent( &dblClick );
  QVERIFY( tool->receivedDoubleClickEvent );
  view->wheelEvent( &wheelEvent );
  QVERIFY( tool->receivedWheelEvent );
  view->keyPressEvent( &keyPress );
  QVERIFY( tool->receivedKeyPressEvent );
  view->keyReleaseEvent( &keyRelease );
  QVERIFY( tool->receivedKeyReleaseEvent );
}

//simple item for testing, since some methods in QgsLayoutItem are pure virtual
class TestItem : public QgsLayoutItem
{
  public:

    TestItem( QgsLayout *layout ) : QgsLayoutItem( layout ) {}
    ~TestItem() {}

    //implement pure virtual methods
    int type() const override { return QgsLayoutItemRegistry::LayoutItem + 101; }
    QString stringType() const override { return QStringLiteral( "testitem" ); }
    void draw( QgsRenderContext &, const QStyleOptionGraphicsItem * = nullptr ) override
    {    }
};

QgsLayout *mLayout = nullptr;
QString mReport;

bool renderCheck( QString testName, QImage &image, int mismatchCount );

void TestQgsLayoutView::guiRegistry()
{
  // test QgsLayoutItemGuiRegistry
  QgsLayoutItemGuiRegistry registry;

  // empty registry
  QVERIFY( !registry.itemMetadata( -1 ) );
  QVERIFY( registry.itemTypes().isEmpty() );
  QVERIFY( !registry.createItemWidget( nullptr ) );
  QVERIFY( !registry.createItemWidget( nullptr ) );
  TestItem *testItem = new TestItem( nullptr );
  QVERIFY( !registry.createItemWidget( testItem ) ); // not in registry

  QSignalSpy spyTypeAdded( &registry, &QgsLayoutItemGuiRegistry::typeAdded );

  // add a dummy item to registry
  auto createWidget = []( QgsLayoutItem * item )->QgsLayoutItemBaseWidget*
  {
    return new QgsLayoutItemBaseWidget( nullptr, item );
  };

  auto createRubberBand = []( QgsLayoutView * view )->QgsLayoutViewRubberBand *
  {
    return new QgsLayoutViewRectangularRubberBand( view );
  };

  QgsLayoutItemGuiMetadata *metadata = new QgsLayoutItemGuiMetadata( QgsLayoutItemRegistry::LayoutItem + 101, QIcon(), createWidget, createRubberBand );
  QVERIFY( registry.addLayoutItemGuiMetadata( metadata ) );
  QCOMPARE( spyTypeAdded.count(), 1 );
  QCOMPARE( spyTypeAdded.value( 0 ).at( 0 ).toInt(), QgsLayoutItemRegistry::LayoutItem + 101 );
  // duplicate type id
  QVERIFY( !registry.addLayoutItemGuiMetadata( metadata ) );
  QCOMPARE( spyTypeAdded.count(), 1 );

  //retrieve metadata
  QVERIFY( !registry.itemMetadata( -1 ) );
  QVERIFY( registry.itemMetadata( QgsLayoutItemRegistry::LayoutItem + 101 ) );
  QCOMPARE( registry.itemTypes().count(), 1 );
  QCOMPARE( registry.itemTypes().value( 0 ), QgsLayoutItemRegistry::LayoutItem + 101 );

  QWidget *widget = registry.createItemWidget( testItem );
  QVERIFY( widget );
  delete widget;

  QgsLayoutView *view = new QgsLayoutView();
  //should use metadata's method
  QgsLayoutViewRubberBand *band = registry.createItemRubberBand( QgsLayoutItemRegistry::LayoutItem + 101, view );
  QVERIFY( band );
  QVERIFY( dynamic_cast< QgsLayoutViewRectangularRubberBand * >( band ) );
  QCOMPARE( band->view(), view );
  delete band;

  // groups
  QVERIFY( registry.addItemGroup( QgsLayoutItemGuiGroup( QStringLiteral( "g1" ) ) ) );
  QCOMPARE( registry.itemGroup( QStringLiteral( "g1" ) ).id, QStringLiteral( "g1" ) );
  // can't add duplicate group
  QVERIFY( !registry.addItemGroup( QgsLayoutItemGuiGroup( QStringLiteral( "g1" ) ) ) );

  //test populate
  QgsLayoutItemGuiRegistry reg2;
  QVERIFY( reg2.itemTypes().isEmpty() );
  QVERIFY( reg2.populate() );
  QVERIFY( !reg2.itemTypes().isEmpty() );
  QVERIFY( !reg2.populate() );
}

void TestQgsLayoutView::rubberBand()
{
  QgsLayoutViewRectangularRubberBand band;
  band.setBrush( QBrush( QColor( 255, 0, 0 ) ) );
  QCOMPARE( band.brush().color(), QColor( 255, 0, 0 ) );
  band.setPen( QPen( QColor( 0, 255, 0 ) ) );
  QCOMPARE( band.pen().color(), QColor( 0, 255, 0 ) );
}

QGSTEST_MAIN( TestQgsLayoutView )
#include "testqgslayoutview.moc"
