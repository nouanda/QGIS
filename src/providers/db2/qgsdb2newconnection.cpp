/***************************************************************************
  qgsdb2newconnection.cpp - new DB2 connection dialog
  --------------------------------------
  Date      : 2016-01-27
  Copyright : (C) 2016 by David Adler
                          Shirley Xiao, David Nguyen
  Email     : dadler at adtechgeospatial.com
              xshirley2012 at yahoo.com, davidng0123 at gmail.com
  Adapted from MSSQL provider by Tamas Szekeres
****************************************************************************
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 ***************************************************************************/

#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlError>
#include <QRegExpValidator>

#include "qgssettings.h"
#include "qgslogger.h"
#include "qgsdb2newconnection.h"
#include "qgsdb2dataitems.h"
#include "qgsdb2provider.h"

QgsDb2NewConnection::QgsDb2NewConnection( QWidget *parent, const QString &connName, Qt::WindowFlags fl )
  : QDialog( parent, fl )
  , mOriginalConnName( connName )
  , mAuthConfigSelect( nullptr )
{
  setupUi( this );
  connect( buttonBox, &QDialogButtonBox::helpRequested, this, &QgsDb2NewConnection::showHelp );

  mAuthConfigSelect = new QgsAuthConfigSelect( this, QStringLiteral( "db2" ) );
  tabAuthentication->insertTab( 1, mAuthConfigSelect, tr( "Configurations" ) );

  if ( !connName.isEmpty() )
  {
    // populate the dialog with the information stored for the connection
    // populate the fields with the stored setting parameters
    QgsSettings settings;

    QString key = "/DB2/connections/" + connName;
    txtService->setText( settings.value( key + "/service" ).toString() );
    txtHost->setText( settings.value( key + "/host" ).toString() );
    txtPort->setText( settings.value( key + "/port" ).toString() );
    txtDriver->setText( settings.value( key + "/driver" ).toString() );
    txtDatabase->setText( settings.value( key + "/database" ).toString() );

    if ( settings.value( key + "/saveUsername" ).toString() == QLatin1String( "true" ) )
    {
      txtUsername->setText( settings.value( key + "/username" ).toString() );
      chkStoreUsername->setChecked( true );
    }

    if ( settings.value( key + "/savePassword" ).toString() == QLatin1String( "true" ) )
    {
      txtPassword->setText( settings.value( key + "/password" ).toString() );
      chkStorePassword->setChecked( true );
    }

    QString authcfg = settings.value( key + "/authcfg" ).toString();
    QgsDebugMsg( QString( "authcfg: %1" ).arg( authcfg ) );
    mAuthConfigSelect->setConfigId( authcfg );
    if ( !authcfg.isEmpty() )
    {
      tabAuthentication->setCurrentIndex( tabAuthentication->indexOf( mAuthConfigSelect ) );
    }

    txtName->setText( connName );
  }
  txtName->setValidator( new QRegExpValidator( QRegExp( "[^\\/]+" ), txtName ) );
}

//! Autoconnected SLOTS *
void QgsDb2NewConnection::accept()
{
  QgsSettings settings;
  QString baseKey = QStringLiteral( "/DB2/connections/" );
  settings.setValue( baseKey + "selected", txtName->text() );
  bool hasAuthConfigID = !mAuthConfigSelect->configId().isEmpty();
  QgsDebugMsg( QString( "hasAuthConfigID: %1" ).arg( hasAuthConfigID ) );
  if ( !hasAuthConfigID && chkStorePassword->isChecked() &&
       QMessageBox::question( this,
                              tr( "Saving passwords" ),
                              tr( "WARNING: You have opted to save your password. It will be stored in plain text in your project files and in your home directory on Unix-like systems, or in your user profile on Windows. If you do not want this to happen, please press the Cancel button.\n" ),
                              QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Cancel )
  {
    return;
  }

  // warn if entry was renamed to an existing connection
  if ( ( mOriginalConnName.isNull() || mOriginalConnName.compare( txtName->text(), Qt::CaseInsensitive ) != 0 ) &&
       ( settings.contains( baseKey + txtName->text() + "/service" ) ||
         settings.contains( baseKey + txtName->text() + "/host" ) ) &&
       QMessageBox::question( this,
                              tr( "Save connection" ),
                              tr( "Should the existing connection %1 be overwritten?" ).arg( txtName->text() ),
                              QMessageBox::Ok | QMessageBox::Cancel ) == QMessageBox::Cancel )
  {
    return;
  }

  // on rename delete the original entry first
  if ( !mOriginalConnName.isNull() && mOriginalConnName != txtName->text() )
  {
    settings.remove( baseKey + mOriginalConnName );
    settings.sync();
  }

  baseKey += txtName->text();

  settings.setValue( baseKey + "/service", txtService->text().trimmed() );
  settings.setValue( baseKey + "/host", txtHost->text() );
  settings.setValue( baseKey + "/port", txtPort->text() );
  settings.setValue( baseKey + "/driver", txtDriver->text() );
  settings.setValue( baseKey + "/database", txtDatabase->text() );
  settings.setValue( baseKey + "/username", chkStoreUsername->isChecked() && !hasAuthConfigID ? txtUsername->text() : QLatin1String( "" ) );
  settings.setValue( baseKey + "/password", chkStorePassword->isChecked() && !hasAuthConfigID ? txtPassword->text() : QLatin1String( "" ) );
  settings.setValue( baseKey + "/saveUsername", chkStoreUsername->isChecked() && !hasAuthConfigID ? "true" : "false" );
  settings.setValue( baseKey + "/savePassword", chkStorePassword->isChecked() && !hasAuthConfigID ? "true" : "false" );
  settings.setValue( baseKey + "/authcfg", mAuthConfigSelect->configId() );

  QDialog::accept();
}

void QgsDb2NewConnection::on_btnConnect_clicked()
{
  testConnection();
}

void QgsDb2NewConnection::on_btnListDatabase_clicked()
{
  listDatabases();
}

void QgsDb2NewConnection::on_cb_trustedConnection_clicked()
{

}

//! End  Autoconnected SLOTS *

QgsDb2NewConnection::~QgsDb2NewConnection()
{
}

bool QgsDb2NewConnection::testConnection()
{
  QSqlDatabase db;

  QString authcfg;
  QString connInfo;
  QString errMsg;
  bool rc = QgsDb2ConnectionItem::ConnInfoFromParameters(
              txtService->text().trimmed(),
              txtDriver->text().trimmed(),
              txtHost->text().trimmed(),
              txtPort->text().trimmed(),
              txtDatabase->text().trimmed(),
              txtUsername->text().trimmed(),
              txtPassword->text().trimmed(),
              authcfg,
              connInfo, errMsg );

  if ( !rc )
  {
    bar->pushMessage( tr( "Error: %1." ).arg( errMsg ),
                      QgsMessageBar::WARNING );
    QgsDebugMsg( "errMsg: " + errMsg );
    return false;
  }

  db = QgsDb2Provider::getDatabase( connInfo, errMsg );
  if ( errMsg.isEmpty() )
  {
    QgsDebugMsg( "connection open succeeded " + connInfo );
    bar->pushMessage( tr( "Connection to %1 was successful" ).arg( txtDatabase->text() ),
                      QgsMessageBar::INFO );
    return true;
  }
  else
  {
    QgsDebugMsg( "connection open failed: " + errMsg );
    bar->pushMessage( tr( "Connection failed: %1." ).arg( errMsg ),
                      QgsMessageBar::WARNING );
    return false;
  }
}

void QgsDb2NewConnection::listDatabases()
{
}

void QgsDb2NewConnection::showHelp()
{
  QgsHelp::openHelp( QStringLiteral( "managing_data_source/opening_data.html#connecting-to-db2-spatial" ) );
}
