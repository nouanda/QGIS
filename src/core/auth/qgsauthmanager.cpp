/***************************************************************************
    qgsauthmanager.cpp
    ---------------------
    begin                : October 5, 2014
    copyright            : (C) 2014 by Boundless Spatial, Inc. USA
    author               : Larry Shaffer
    email                : lshaffer at boundlessgeo dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsauthmanager.h"

#include <QDir>
#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>
#include <QObject>
#include <QSet>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTextStream>
#include <QTime>
#include <QTimer>
#include <QVariant>

#include <QtCrypto>

#ifndef QT_NO_SSL
#include <QSslConfiguration>
#endif

// QtKeyChain library
#include "keychain.h"

// QGIS includes
#include "qgsapplication.h"
#include "qgsauthcertutils.h"
#include "qgsauthcrypto.h"
#include "qgsauthmethod.h"
#include "qgsauthmethodmetadata.h"
#include "qgsauthmethodregistry.h"
#include "qgscredentials.h"
#include "qgslogger.h"
#include "qgsmessagelog.h"
#include "qgssettings.h"

QgsAuthManager *QgsAuthManager::sInstance = nullptr;

const QString QgsAuthManager::AUTH_CONFIG_TABLE = QStringLiteral( "auth_configs" );
const QString QgsAuthManager::AUTH_PASS_TABLE = QStringLiteral( "auth_pass" );
const QString QgsAuthManager::AUTH_SETTINGS_TABLE = QStringLiteral( "auth_settings" );
const QString QgsAuthManager::AUTH_IDENTITIES_TABLE = QStringLiteral( "auth_identities" );
const QString QgsAuthManager::AUTH_SERVERS_TABLE = QStringLiteral( "auth_servers" );
const QString QgsAuthManager::AUTH_AUTHORITIES_TABLE = QStringLiteral( "auth_authorities" );
const QString QgsAuthManager::AUTH_TRUST_TABLE = QStringLiteral( "auth_trust" );
const QString QgsAuthManager::AUTH_MAN_TAG = QObject::tr( "Authentication Manager" );
const QString QgsAuthManager::AUTH_CFG_REGEX = QStringLiteral( "authcfg=([a-z]|[A-Z]|[0-9]){7}" );


const QLatin1String QgsAuthManager::AUTH_PASSWORD_HELPER_KEY_NAME( "QGIS-Master-Password" );
const QLatin1String QgsAuthManager::AUTH_PASSWORD_HELPER_FOLDER_NAME( "QGIS" );

#if defined(Q_OS_MAC)
const QString QgsAuthManager::AUTH_PASSWORD_HELPER_DISPLAY_NAME( "Keychain" );
static const QString sDescription = QObject::tr( "Master Password <-> KeyChain storage plugin. Store and retrieve your master password in your KeyChain" );
#elif defined(Q_OS_WIN)
const QString QgsAuthManager::AUTH_PASSWORD_HELPER_DISPLAY_NAME( "Password Manager" );
static const QString sDescription = QObject::tr( "Master Password <-> Password Manager storage plugin. Store and retrieve your master password in your Password Manager" );
#elif defined(Q_OS_LINUX)
const QString QgsAuthManager::AUTH_PASSWORD_HELPER_DISPLAY_NAME( "Wallet/KeyRing" );
static const QString sDescription = QObject::tr( "Master Password <-> Wallet/KeyRing storage plugin. Store and retrieve your master password in your Wallet/KeyRing" );
#else
const QString QgsAuthManager::AUTH_PASSWORD_HELPER_DISPLAY_NAME( "Password Manager" );
static const QString sDescription = QObject::tr( "Master Password <-> KeyChain storage plugin. Store and retrieve your master password in your Wallet/KeyChain/Password Manager" );
#endif


QgsAuthManager *QgsAuthManager::instance()
{
  if ( !sInstance )
  {
    sInstance = new QgsAuthManager();
  }
  return sInstance;
}

QSqlDatabase QgsAuthManager::authDatabaseConnection() const
{
  QSqlDatabase authdb;
  if ( isDisabled() )
    return authdb;

  QString connectionname = QStringLiteral( "authentication.configs" );
  if ( !QSqlDatabase::contains( connectionname ) )
  {
    authdb = QSqlDatabase::addDatabase( QStringLiteral( "QSQLITE" ), connectionname );
    authdb.setDatabaseName( authenticationDatabasePath() );
  }
  else
  {
    authdb = QSqlDatabase::database( connectionname );
  }
  if ( !authdb.isOpen() )
  {
    if ( !authdb.open() )
    {
      const char *err = QT_TR_NOOP( "Opening of authentication db FAILED" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), CRITICAL );
    }
  }

  return authdb;
}

bool QgsAuthManager::init( const QString &pluginPath )
{
  if ( mAuthInit )
    return true;
  mAuthInit = true;

  QgsDebugMsg( "Initializing QCA..." );
  mQcaInitializer = new QCA::Initializer( QCA::Practical, 256 );

  QgsDebugMsg( "QCA initialized." );
  QCA::scanForPlugins();

  QgsDebugMsg( QString( "QCA Plugin Diagnostics Context: %1" ).arg( QCA::pluginDiagnosticText() ) );
  QStringList capabilities;

  capabilities = QCA::supportedFeatures();
  QgsDebugMsg( QString( "QCA supports: %1" ).arg( capabilities.join( "," ) ) );

  // do run-time check for qca-ossl plugin
  if ( !QCA::isSupported( "cert", QStringLiteral( "qca-ossl" ) ) )
  {
    mAuthDisabled = true;
    mAuthDisabledMessage = tr( "QCA's OpenSSL plugin (qca-ossl) is missing" );
    return isDisabled();
  }

  QgsDebugMsg( "Prioritizing qca-ossl over all other QCA providers..." );
  QCA::ProviderList provds = QCA::providers();
  QStringList prlist;
  Q_FOREACH ( QCA::Provider *p, provds )
  {
    QString pn = p->name();
    int pr = 0;
    if ( pn != QLatin1String( "qca-ossl" ) )
    {
      pr = QCA::providerPriority( pn ) + 1;
    }
    QCA::setProviderPriority( pn, pr );
    prlist << QStringLiteral( "%1:%2" ).arg( pn ).arg( QCA::providerPriority( pn ) );
  }
  QgsDebugMsg( QString( "QCA provider priorities: %1" ).arg( prlist.join( ", " ) ) );

  QgsDebugMsg( "Populating auth method registry" );
  QgsAuthMethodRegistry *authreg = QgsAuthMethodRegistry::instance( pluginPath );

  QStringList methods = authreg->authMethodList();

  QgsDebugMsg( QString( "Authentication methods found: %1" ).arg( methods.join( ", " ) ) );

  if ( methods.isEmpty() )
  {
    mAuthDisabled = true;
    mAuthDisabledMessage = tr( "No authentication method plugins found" );
    return isDisabled();
  }

  if ( !registerCoreAuthMethods() )
  {
    mAuthDisabled = true;
    mAuthDisabledMessage = tr( "No authentication method plugins could be loaded" );
    return isDisabled();
  }

  mAuthDbPath = QDir::cleanPath( QgsApplication::qgisAuthDatabaseFilePath() );
  QgsDebugMsg( QString( "Auth database path: %1" ).arg( authenticationDatabasePath() ) );

  QFileInfo dbinfo( authenticationDatabasePath() );
  QFileInfo dbdirinfo( dbinfo.path() );
  QgsDebugMsg( QString( "Auth db directory path: %1" ).arg( dbdirinfo.filePath() ) );

  if ( !dbdirinfo.exists() )
  {
    QgsDebugMsg( QString( "Auth db directory path does not exist, making path: %1" ).arg( dbdirinfo.filePath() ) );
    if ( !QDir().mkpath( dbdirinfo.filePath() ) )
    {
      const char *err = QT_TR_NOOP( "Auth db directory path could not be created" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), CRITICAL );
      return false;
    }
  }

  if ( dbinfo.exists() )
  {
    if ( !dbinfo.permission( QFile::ReadOwner | QFile::WriteOwner ) )
    {
      const char *err = QT_TR_NOOP( "Auth db is not readable or writable by user" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), CRITICAL );
      return false;
    }
    if ( dbinfo.size() > 0 )
    {
      QgsDebugMsg( "Auth db exists and has data" );

      if ( !createCertTables() )
        return false;

      updateConfigAuthMethods();

#ifndef QT_NO_SSL
      initSslCaches();
#endif

      // set the master password from first line of file defined by QGIS_AUTH_PASSWORD_FILE env variable
      const char *passenv = "QGIS_AUTH_PASSWORD_FILE";
      if ( getenv( passenv ) && masterPasswordHashInDatabase() )
      {
        QString passpath( getenv( passenv ) );
        // clear the env variable, so it can not be accessed from plugins, etc.
        // (note: stored QgsApplication::systemEnvVars() skips this env variable as well)
#ifdef Q_OS_WIN
        putenv( passenv );
#else
        unsetenv( passenv );
#endif
        QString masterpass;
        QFile passfile( passpath );
        if ( passfile.exists() && passfile.open( QIODevice::ReadOnly | QIODevice::Text ) )
        {
          QTextStream passin( &passfile );
          while ( !passin.atEnd() )
          {
            masterpass = passin.readLine();
            break;
          }
          passfile.close();
        }
        if ( !masterpass.isEmpty() )
        {
          if ( setMasterPassword( masterpass, true ) )
          {
            QgsDebugMsg( "Authentication master password set from QGIS_AUTH_PASSWORD_FILE" );
          }
          else
          {
            QgsDebugMsg( "QGIS_AUTH_PASSWORD_FILE set, but FAILED to set password using: " + passpath );
            return false;
          }
        }
        else
        {
          QgsDebugMsg( "QGIS_AUTH_PASSWORD_FILE set, but FAILED to read password from: " + passpath );
          return false;
        }
      }

      return true;
    }
  }
  else
  {
    QgsDebugMsg( "Auth db does not exist: creating through QSqlDatabase initial connection" );

    if ( !createConfigTables() )
      return false;

    if ( !createCertTables() )
      return false;
  }

#ifndef QT_NO_SSL
  initSslCaches();
#endif

  return true;
}

bool QgsAuthManager::createConfigTables()
{
  // create and open the db
  if ( !authDbOpen() )
  {
    const char *err = QT_TR_NOOP( "Auth db could not be created and opened" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), CRITICAL );
    return false;
  }

  QSqlQuery query( authDatabaseConnection() );

  // create the tables
  QString qstr;

  qstr = QString( "CREATE TABLE %1 (\n"
                  "    'salt' TEXT NOT NULL,\n"
                  "    'civ' TEXT NOT NULL\n"
                  ", 'hash' TEXT  NOT NULL);" ).arg( authDbPassTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QString( "CREATE TABLE %1 (\n"
                  "    'id' TEXT NOT NULL,\n"
                  "    'name' TEXT NOT NULL,\n"
                  "    'uri' TEXT,\n"
                  "    'type' TEXT NOT NULL,\n"
                  "    'version' INTEGER NOT NULL\n"
                  ", 'config' TEXT  NOT NULL);" ).arg( authDatabaseConfigTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE UNIQUE INDEX 'id_index' on %1 (id ASC);" ).arg( authDatabaseConfigTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE INDEX 'uri_index' on %1 (uri ASC);" ).arg( authDatabaseConfigTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  return true;
}

bool QgsAuthManager::createCertTables()
{
  // NOTE: these tables were added later, so IF NOT EXISTS is used
  QgsDebugMsg( "Creating cert tables in auth db" );

  QSqlQuery query( authDatabaseConnection() );

  // create the tables
  QString qstr;

  qstr = QString( "CREATE TABLE IF NOT EXISTS %1 (\n"
                  "    'setting' TEXT NOT NULL\n"
                  ", 'value' TEXT);" ).arg( authDbSettingsTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();


  qstr = QString( "CREATE TABLE IF NOT EXISTS %1 (\n"
                  "    'id' TEXT NOT NULL,\n"
                  "    'key' TEXT NOT NULL\n"
                  ", 'cert' TEXT  NOT NULL);" ).arg( authDbIdentitiesTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE UNIQUE INDEX IF NOT EXISTS 'id_index' on %1 (id ASC);" ).arg( authDbIdentitiesTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();


  qstr = QString( "CREATE TABLE IF NOT EXISTS %1 (\n"
                  "    'id' TEXT NOT NULL,\n"
                  "    'host' TEXT NOT NULL,\n"
                  "    'cert' TEXT\n"
                  ", 'config' TEXT  NOT NULL);" ).arg( authDatabaseServersTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE UNIQUE INDEX IF NOT EXISTS 'host_index' on %1 (host ASC);" ).arg( authDatabaseServersTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();


  qstr = QString( "CREATE TABLE IF NOT EXISTS %1 (\n"
                  "    'id' TEXT NOT NULL\n"
                  ", 'cert' TEXT  NOT NULL);" ).arg( authDbAuthoritiesTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE UNIQUE INDEX IF NOT EXISTS 'id_index' on %1 (id ASC);" ).arg( authDbAuthoritiesTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QString( "CREATE TABLE IF NOT EXISTS %1 (\n"
                  "    'id' TEXT NOT NULL\n"
                  ", 'policy' TEXT  NOT NULL);" ).arg( authDbTrustTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  qstr = QStringLiteral( "CREATE UNIQUE INDEX IF NOT EXISTS 'id_index' on %1 (id ASC);" ).arg( authDbTrustTable() );
  query.prepare( qstr );
  if ( !authDbQuery( &query ) )
    return false;
  query.clear();

  return true;
}

bool QgsAuthManager::isDisabled() const
{
  if ( mAuthDisabled )
  {
    QgsDebugMsg( "Authentication system DISABLED: QCA's qca-ossl (OpenSSL) plugin is missing" );
  }
  return mAuthDisabled;
}

const QString QgsAuthManager::disabledMessage() const
{
  return tr( "Authentication system is DISABLED:\n%1" ).arg( mAuthDisabledMessage );
}

bool QgsAuthManager::setMasterPassword( bool verify )
{
  QMutexLocker locker( mMutex );
  if ( isDisabled() )
    return false;

  if ( mScheduledDbErase )
    return false;

  if ( mMasterPass.isEmpty() )
  {
    QgsDebugMsg( "Master password is not yet set by user" );
    if ( !masterPasswordInput() )
    {
      QgsDebugMsg( "Master password input canceled by user" );
      return false;
    }
  }
  else
  {
    QgsDebugMsg( "Master password is set" );
    if ( !verify )
      return true;
  }

  if ( !verifyMasterPassword() )
    return false;

  QgsDebugMsg( "Master password is set and verified" );
  return true;
}

bool QgsAuthManager::setMasterPassword( const QString &pass, bool verify )
{
  QMutexLocker locker( mMutex );
  if ( isDisabled() )
    return false;

  if ( mScheduledDbErase )
    return false;

  // since this is generally for automation, we don't care if passed-in is same as existing
  QString prevpass = QString( mMasterPass );
  mMasterPass = pass;
  if ( verify && !verifyMasterPassword() )
  {
    mMasterPass = prevpass;
    const char *err = QT_TR_NOOP( "Master password set: FAILED to verify, reset to previous" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  QgsDebugMsg( QString( "Master password set: SUCCESS%1" ).arg( verify ? " and verified" : "" ) );
  return true;
}

bool QgsAuthManager::verifyMasterPassword( const QString &compare )
{
  if ( isDisabled() )
    return false;

  int rows = 0;
  if ( !masterPasswordRowsInDb( &rows ) )
  {
    const char *err = QT_TR_NOOP( "Master password: FAILED to access database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), CRITICAL );

    clearMasterPassword();
    return false;
  }

  QgsDebugMsg( QString( "Master password: %1 rows in database" ).arg( rows ) );

  if ( rows > 1 )
  {
    const char *err = QT_TR_NOOP( "Master password: FAILED to find just one master password record in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );

    clearMasterPassword();
    return false;
  }
  else if ( rows == 1 )
  {
    if ( !masterPasswordCheckAgainstDb( compare ) )
    {
      if ( compare.isNull() ) // don't complain when comparing, since it could be an incomplete comparison string
      {
        const char *err = QT_TR_NOOP( "Master password: FAILED to verify against hash in database" );
        QgsDebugMsg( err );
        emit messageOut( tr( err ), authManTag(), WARNING );

        clearMasterPassword();

        emit masterPasswordVerified( false );
      }
      ++mPassTries;
      if ( mPassTries >= 5 )
      {
        mAuthDisabled = true;
        const char *err = QT_TR_NOOP( "Master password: failed 5 times authentication system DISABLED" );
        QgsDebugMsg( err );
        emit messageOut( tr( err ), authManTag(), WARNING );
      }
      return false;
    }
    else
    {
      QgsDebugMsg( "Master password: verified against hash in database" );
      if ( compare.isNull() )
        emit masterPasswordVerified( true );
    }
  }
  else if ( compare.isNull() ) // compares should never be stored
  {
    if ( !masterPasswordStoreInDb() )
    {
      const char *err = QT_TR_NOOP( "Master password: hash FAILED to be stored in database" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), CRITICAL );

      clearMasterPassword();
      return false;
    }
    else
    {
      QgsDebugMsg( "Master password: hash stored in database" );
    }
    // double-check storing
    if ( !masterPasswordCheckAgainstDb() )
    {
      const char *err = QT_TR_NOOP( "Master password: FAILED to verify against hash in database" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), WARNING );

      clearMasterPassword();
      emit masterPasswordVerified( false );
      return false;
    }
    else
    {
      QgsDebugMsg( "Master password: verified against hash in database" );
      emit masterPasswordVerified( true );
    }
  }

  return true;
}

bool QgsAuthManager::masterPasswordIsSet() const
{
  return !mMasterPass.isEmpty();
}

bool QgsAuthManager::masterPasswordSame( const QString &pass ) const
{
  return mMasterPass == pass;
}

bool QgsAuthManager::resetMasterPassword( const QString &newpass, const QString &oldpass,
    bool keepbackup, QString *backuppath )
{
  if ( isDisabled() )
    return false;

  // verify caller knows the current master password
  // this means that the user will have had to already set the master password as well
  if ( !masterPasswordSame( oldpass ) )
    return false;

  QString dbbackup;
  if ( !backupAuthenticationDatabase( &dbbackup ) )
    return false;

  QgsDebugMsg( "Master password reset: backed up current database" );

  // create new database and connection
  authDatabaseConnection();

  // store current password and civ
  QString prevpass = QString( mMasterPass );
  QString prevciv = QString( masterPasswordCiv() );

  // on ANY FAILURE from this point, reinstate previous password and database
  bool ok = true;

  // clear password hash table (also clears mMasterPass)
  if ( ok && !masterPasswordClearDb() )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not clear current password from database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }
  if ( ok )
  {
    QgsDebugMsg( "Master password reset: cleared current password from database" );
  }

  // mMasterPass empty, set new password (don't verify, since not stored yet)
  setMasterPassword( newpass, false );

  // store new password hash
  if ( ok && !masterPasswordStoreInDb() )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not store new password in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }
  if ( ok )
  {
    QgsDebugMsg( "Master password reset: stored new password in database" );
  }

  // verify it stored password properly
  if ( ok && !verifyMasterPassword() )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not verify new password in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }

  // re-encrypt everything with new password
  if ( ok && !reencryptAllAuthenticationConfigs( prevpass, prevciv ) )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not re-encrypt configs in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }
  if ( ok )
  {
    QgsDebugMsg( "Master password reset: re-encrypted configs in database" );
  }

  // verify it all worked
  if ( ok && !verifyPasswordCanDecryptConfigs() )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not verify password can decrypt re-encrypted configs" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }

  if ( ok && !reencryptAllAuthenticationSettings( prevpass, prevciv ) )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not re-encrypt settings in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }

  if ( ok && !reencryptAllAuthenticationIdentities( prevpass, prevciv ) )
  {
    ok = false;
    const char *err = QT_TR_NOOP( "Master password reset FAILED: could not re-encrypt identities in database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
  }

  // something went wrong, reinstate previous password and database
  if ( !ok )
  {
    // backup database of failed attempt, for inspection
    authDatabaseConnection().close();
    QString errdbbackup( dbbackup );
    errdbbackup.replace( QLatin1String( ".db" ), QLatin1String( "_ERROR.db" ) );
    QFile::rename( authenticationDatabasePath(), errdbbackup );
    QgsDebugMsg( QString( "Master password reset FAILED: backed up failed db at %1" ).arg( errdbbackup ) );

    // reinstate previous database and password
    QFile::rename( dbbackup, authenticationDatabasePath() );
    mMasterPass = prevpass;
    authDatabaseConnection();
    QgsDebugMsg( "Master password reset FAILED: reinstated previous password and database" );

    // assign error db backup
    if ( backuppath )
      *backuppath = errdbbackup;

    return false;
  }


  if ( !keepbackup && !QFile::remove( dbbackup ) )
  {
    const char *err = QT_TR_NOOP( "Master password reset: could not remove old database backup" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    // a non-blocking error, continue
  }

  if ( keepbackup )
  {
    QgsDebugMsg( QString( "Master password reset: backed up previous db at %1" ).arg( dbbackup ) );
    if ( backuppath )
      *backuppath = dbbackup;
  }

  QgsDebugMsg( "Master password reset: SUCCESS" );
  emit authDatabaseChanged();
  return true;
}

void QgsAuthManager::setScheduledAuthDatabaseErase( bool scheduleErase )
{
  mScheduledDbErase = scheduleErase;
  // any call (start or stop) should reset these
  mScheduledDbEraseRequestEmitted = false;
  mScheduledDbEraseRequestCount = 0;

  if ( scheduleErase )
  {
    if ( !mScheduledDbEraseTimer )
    {
      mScheduledDbEraseTimer = new QTimer( this );
      connect( mScheduledDbEraseTimer, &QTimer::timeout, this, &QgsAuthManager::tryToStartDbErase );
      mScheduledDbEraseTimer->start( mScheduledDbEraseRequestWait * 1000 );
    }
    else if ( !mScheduledDbEraseTimer->isActive() )
    {
      mScheduledDbEraseTimer->start();
    }
  }
  else
  {
    if ( mScheduledDbEraseTimer && mScheduledDbEraseTimer->isActive() )
      mScheduledDbEraseTimer->stop();
  }
}

bool QgsAuthManager::registerCoreAuthMethods()
{
  if ( isDisabled() )
    return false;

  qDeleteAll( mAuthMethods );
  mAuthMethods.clear();
  Q_FOREACH ( const QString &authMethodKey, QgsAuthMethodRegistry::instance()->authMethodList() )
  {
    mAuthMethods.insert( authMethodKey, QgsAuthMethodRegistry::instance()->authMethod( authMethodKey ) );
  }

  return !mAuthMethods.isEmpty();
}

const QString QgsAuthManager::uniqueConfigId() const
{
  QStringList configids = configIds();
  QString id;
  int len = 7;
  // sleep just a bit to make sure the current time has changed
  QEventLoop loop;
  QTimer::singleShot( 3, &loop, SLOT( quit() ) );
  loop.exec();

  uint seed = static_cast< uint >( QTime::currentTime().msec() );
  qsrand( seed );

  while ( true )
  {
    id = QLatin1String( "" );
    for ( int i = 0; i < len; i++ )
    {
      switch ( qrand() % 2 )
      {
        case 0:
          id += ( '0' + qrand() % 10 );
          break;
        case 1:
          id += ( 'a' + qrand() % 26 );
          break;
      }
    }
    if ( !configids.contains( id ) )
    {
      break;
    }
  }
  QgsDebugMsg( QString( "Generated unique ID: %1" ).arg( id ) );
  return id;
}

bool QgsAuthManager::configIdUnique( const QString &id ) const
{
  if ( isDisabled() )
    return false;

  if ( id.isEmpty() )
  {
    const char *err = QT_TR_NOOP( "Config ID is empty" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }
  QStringList configids = configIds();
  return !configids.contains( id );
}

bool QgsAuthManager::hasConfigId( const QString &txt ) const
{
  QRegExp rx( AUTH_CFG_REGEX );
  return rx.indexIn( txt ) != -1;
}

QgsAuthMethodConfigsMap QgsAuthManager::availableAuthMethodConfigs( const QString &dataprovider )
{
  QStringList providerAuthMethodsKeys;
  if ( !dataprovider.isEmpty() )
  {
    providerAuthMethodsKeys = authMethodsKeys( dataprovider.toLower() );
  }

  QgsAuthMethodConfigsMap baseConfigs;

  if ( isDisabled() )
    return baseConfigs;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, name, uri, type, version FROM %1" ).arg( authDatabaseConfigTable() ) );

  if ( !authDbQuery( &query ) )
  {
    return baseConfigs;
  }

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      QString authcfg = query.value( 0 ).toString();
      QgsAuthMethodConfig config;
      config.setId( authcfg );
      config.setName( query.value( 1 ).toString() );
      config.setUri( query.value( 2 ).toString() );
      config.setMethod( query.value( 3 ).toString() );
      config.setVersion( query.value( 4 ).toInt() );

      if ( !dataprovider.isEmpty() && !providerAuthMethodsKeys.contains( config.method() ) )
      {
        continue;
      }

      baseConfigs.insert( authcfg, config );
    }
  }
  return baseConfigs;
}

void QgsAuthManager::updateConfigAuthMethods()
{
  if ( isDisabled() )
    return;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, type FROM %1" ).arg( authDatabaseConfigTable() ) );

  if ( !authDbQuery( &query ) )
  {
    return;
  }

  if ( query.isActive() )
  {
    QgsDebugMsg( "Synching existing auth config and their auth methods" );
    mConfigAuthMethods.clear();
    QStringList cfgmethods;
    while ( query.next() )
    {
      mConfigAuthMethods.insert( query.value( 0 ).toString(),
                                 query.value( 1 ).toString() );
      cfgmethods << QStringLiteral( "%1=%2" ).arg( query.value( 0 ).toString(), query.value( 1 ).toString() );
    }
    QgsDebugMsg( QString( "Stored auth config/methods:\n%1" ).arg( cfgmethods.join( ", " ) ) );
  }
}

QgsAuthMethod *QgsAuthManager::configAuthMethod( const QString &authcfg )
{
  if ( isDisabled() )
    return nullptr;

  if ( !mConfigAuthMethods.contains( authcfg ) )
  {
    QgsDebugMsg( QString( "No config auth method found in database for authcfg: %1" ).arg( authcfg ) );
    return nullptr;
  }

  QString authMethodKey = mConfigAuthMethods.value( authcfg );

  return authMethod( authMethodKey );
}

QString QgsAuthManager::configAuthMethodKey( const QString &authcfg ) const
{
  if ( isDisabled() )
    return QString();

  return mConfigAuthMethods.value( authcfg, QString() );
}


QStringList QgsAuthManager::authMethodsKeys( const QString &dataprovider )
{
  return authMethodsMap( dataprovider.toLower() ).uniqueKeys();
}

QgsAuthMethod *QgsAuthManager::authMethod( const QString &authMethodKey )
{
  if ( !mAuthMethods.contains( authMethodKey ) )
  {
    QgsDebugMsg( QString( "No auth method registered for auth method key: %1" ).arg( authMethodKey ) );
    return nullptr;
  }

  return mAuthMethods.value( authMethodKey );
}

QgsAuthMethodsMap QgsAuthManager::authMethodsMap( const QString &dataprovider )
{
  if ( dataprovider.isEmpty() )
  {
    return mAuthMethods;
  }

  QgsAuthMethodsMap filteredmap;
  QgsAuthMethodsMap::const_iterator i = mAuthMethods.constBegin();
  while ( i != mAuthMethods.constEnd() )
  {
    if ( i.value()
         && ( i.value()->supportedDataProviders().contains( QStringLiteral( "all" ) )
              || i.value()->supportedDataProviders().contains( dataprovider ) ) )
    {
      filteredmap.insert( i.key(), i.value() );
    }
    ++i;
  }
  return filteredmap;
}

QWidget *QgsAuthManager::authMethodEditWidget( const QString &authMethodKey, QWidget *parent )
{
  return QgsAuthMethodRegistry::instance()->editWidget( authMethodKey, parent );
}

QgsAuthMethod::Expansions QgsAuthManager::supportedAuthMethodExpansions( const QString &authcfg )
{
  if ( isDisabled() )
    return QgsAuthMethod::Expansions( nullptr );

  QgsAuthMethod *authmethod = configAuthMethod( authcfg );
  if ( authmethod )
  {
    return authmethod->supportedExpansions();
  }
  return QgsAuthMethod::Expansions( nullptr );
}

bool QgsAuthManager::storeAuthenticationConfig( QgsAuthMethodConfig &mconfig )
{
  if ( !setMasterPassword( true ) )
    return false;

  // don't need to validate id, since it has not be defined yet
  if ( !mconfig.isValid() )
  {
    const char *err = QT_TR_NOOP( "Store config: FAILED because config is invalid" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  QString uid = mconfig.id();
  bool passedinID = !uid.isEmpty();
  if ( uid.isEmpty() )
  {
    uid = uniqueConfigId();
  }
  else if ( configIds().contains( uid ) )
  {
    const char *err = QT_TR_NOOP( "Store config: FAILED because pre-defined config ID is not unique" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  QString configstring = mconfig.configString();
  if ( configstring.isEmpty() )
  {
    const char *err = QT_TR_NOOP( "Store config: FAILED because config string is empty" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }
#if( 0 )
  QgsDebugMsg( QString( "authDbConfigTable(): %1" ).arg( authDbConfigTable() ) );
  QgsDebugMsg( QString( "name: %1" ).arg( config.name() ) );
  QgsDebugMsg( QString( "uri: %1" ).arg( config.uri() ) );
  QgsDebugMsg( QString( "type: %1" ).arg( config.method() ) );
  QgsDebugMsg( QString( "version: %1" ).arg( config.version() ) );
  QgsDebugMsg( QString( "config: %1" ).arg( configstring ) ); // DO NOT LEAVE THIS LINE UNCOMMENTED !
#endif

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (id, name, uri, type, version, config) "
                          "VALUES (:id, :name, :uri, :type, :version, :config)" ).arg( authDatabaseConfigTable() ) );

  query.bindValue( QStringLiteral( ":id" ), uid );
  query.bindValue( QStringLiteral( ":name" ), mconfig.name() );
  query.bindValue( QStringLiteral( ":uri" ), mconfig.uri() );
  query.bindValue( QStringLiteral( ":type" ), mconfig.method() );
  query.bindValue( QStringLiteral( ":version" ), mconfig.version() );
  query.bindValue( QStringLiteral( ":config" ), QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), configstring ) );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  // passed-in config should now be like as if it was just loaded from db
  if ( !passedinID )
    mconfig.setId( uid );

  updateConfigAuthMethods();

  QgsDebugMsg( QString( "Store config SUCCESS for authcfg: %1" ).arg( uid ) );
  return true;

}

bool QgsAuthManager::updateAuthenticationConfig( const QgsAuthMethodConfig &config )
{
  if ( !setMasterPassword( true ) )
    return false;

  // validate id
  if ( !config.isValid( true ) )
  {
    const char *err = QT_TR_NOOP( "Update config: FAILED because config is invalid" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  QString configstring = config.configString();
  if ( configstring.isEmpty() )
  {
    const char *err = QT_TR_NOOP( "Update config: FAILED because config is empty" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

#if( 0 )
  QgsDebugMsg( QString( "authDbConfigTable(): %1" ).arg( authDbConfigTable() ) );
  QgsDebugMsg( QString( "id: %1" ).arg( config.id() ) );
  QgsDebugMsg( QString( "name: %1" ).arg( config.name() ) );
  QgsDebugMsg( QString( "uri: %1" ).arg( config.uri() ) );
  QgsDebugMsg( QString( "type: %1" ).arg( config.method() ) );
  QgsDebugMsg( QString( "version: %1" ).arg( config.version() ) );
  QgsDebugMsg( QString( "config: %1" ).arg( configstring ) ); // DO NOT LEAVE THIS LINE UNCOMMENTED !
#endif

  QSqlQuery query( authDatabaseConnection() );
  if ( !query.prepare( QString( "UPDATE %1 "
                                "SET name = :name, uri = :uri, type = :type, version = :version, config = :config "
                                "WHERE id = :id" ).arg( authDatabaseConfigTable() ) ) )
  {
    const char *err = QT_TR_NOOP( "Update config: FAILED to prepare query" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  query.bindValue( QStringLiteral( ":id" ), config.id() );
  query.bindValue( QStringLiteral( ":name" ), config.name() );
  query.bindValue( QStringLiteral( ":uri" ), config.uri() );
  query.bindValue( QStringLiteral( ":type" ), config.method() );
  query.bindValue( QStringLiteral( ":version" ), config.version() );
  query.bindValue( QStringLiteral( ":config" ), QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), configstring ) );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  // should come before updating auth methods, in case user switched auth methods in config
  clearCachedConfig( config.id() );

  updateConfigAuthMethods();

  QgsDebugMsg( QString( "Update config SUCCESS for authcfg: %1" ).arg( config.id() ) );

  return true;
}

bool QgsAuthManager::loadAuthenticationConfig( const QString &authcfg, QgsAuthMethodConfig &mconfig, bool full )
{
  if ( isDisabled() )
    return false;

  if ( full && !setMasterPassword( true ) )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  if ( full )
  {
    query.prepare( QString( "SELECT id, name, uri, type, version, config FROM %1 "
                            "WHERE id = :id" ).arg( authDatabaseConfigTable() ) );
  }
  else
  {
    query.prepare( QString( "SELECT id, name, uri, type, version FROM %1 "
                            "WHERE id = :id" ).arg( authDatabaseConfigTable() ) );
  }

  query.bindValue( QStringLiteral( ":id" ), authcfg );

  if ( !authDbQuery( &query ) )
  {
    return false;
  }

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      mconfig.setId( query.value( 0 ).toString() );
      mconfig.setName( query.value( 1 ).toString() );
      mconfig.setUri( query.value( 2 ).toString() );
      mconfig.setMethod( query.value( 3 ).toString() );
      mconfig.setVersion( query.value( 4 ).toInt() );

      if ( full )
      {
        mconfig.loadConfigString( QgsAuthCrypto::decrypt( mMasterPass, masterPasswordCiv(), query.value( 5 ).toString() ) );
      }

      QString authMethodKey = configAuthMethodKey( authcfg );
      QgsAuthMethod *authmethod = authMethod( authMethodKey );
      if ( authmethod )
      {
        authmethod->updateMethodConfig( mconfig );
      }
      else
      {
        QgsDebugMsg( QString( "Update of authcfg %1 FAILED for auth method %2" ).arg( authcfg, authMethodKey ) );
      }

      QgsDebugMsg( QString( "Load %1 config SUCCESS for authcfg: %2" ).arg( full ? "full" : "base", authcfg ) );
      return true;
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for authcfg: %1" ).arg( authcfg ) );
      emit messageOut( tr( "Authentication database contains duplicate configuration IDs" ), authManTag(), WARNING );
    }
  }

  return false;
}

bool QgsAuthManager::removeAuthenticationConfig( const QString &authcfg )
{
  if ( isDisabled() )
    return false;

  if ( authcfg.isEmpty() )
    return false;

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE id = :id" ).arg( authDatabaseConfigTable() ) );

  query.bindValue( QStringLiteral( ":id" ), authcfg );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  clearCachedConfig( authcfg );

  updateConfigAuthMethods();

  QgsDebugMsg( QString( "REMOVED config for authcfg: %1" ).arg( authcfg ) );

  return true;
}

bool QgsAuthManager::removeAllAuthenticationConfigs()
{
  if ( isDisabled() )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "DELETE FROM %1" ).arg( authDatabaseConfigTable() ) );
  bool res = authDbTransactionQuery( &query );

  if ( res )
  {
    clearAllCachedConfigs();
    updateConfigAuthMethods();
  }

  QgsDebugMsg( QString( "Remove configs from database: %1" ).arg( res ? "SUCCEEDED" : "FAILED" ) );

  return res;
}

bool QgsAuthManager::backupAuthenticationDatabase( QString *backuppath )
{
  if ( !QFile::exists( authenticationDatabasePath() ) )
  {
    const char *err = QT_TR_NOOP( "No authentication database found" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  // close any connection to current db
  QSqlDatabase authConn = authDatabaseConnection();
  if ( authConn.isValid() && authConn.isOpen() )
    authConn.close();

  // duplicate current db file to 'qgis-auth_YYYY-MM-DD-HHMMSS.db' backup
  QString datestamp( QDateTime::currentDateTime().toString( QStringLiteral( "yyyy-MM-dd-hhmmss" ) ) );
  QString dbbackup( authenticationDatabasePath() );
  dbbackup.replace( QLatin1String( ".db" ), QStringLiteral( "_%1.db" ).arg( datestamp ) );

  if ( !QFile::copy( authenticationDatabasePath(), dbbackup ) )
  {
    const char *err = QT_TR_NOOP( "Could not back up authentication database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  if ( backuppath )
    *backuppath = dbbackup;

  QgsDebugMsg( QString( "Backed up auth database at %1" ).arg( dbbackup ) );
  return true;
}

bool QgsAuthManager::eraseAuthenticationDatabase( bool backup, QString *backuppath )
{
  if ( isDisabled() )
    return false;

  QString dbbackup;
  if ( backup && !backupAuthenticationDatabase( &dbbackup ) )
  {
    return false;
  }

  if ( backuppath && !dbbackup.isEmpty() )
    *backuppath = dbbackup;

  QFileInfo dbinfo( authenticationDatabasePath() );
  if ( dbinfo.exists() )
  {
    if ( !dbinfo.permission( QFile::ReadOwner | QFile::WriteOwner ) )
    {
      const char *err = QT_TR_NOOP( "Auth db is not readable or writable by user" );
      QgsDebugMsg( err );
      emit messageOut( tr( err ), authManTag(), CRITICAL );
      return false;
    }
  }
  else
  {
    const char *err = QT_TR_NOOP( "No authentication database found" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  if ( !QFile::remove( authenticationDatabasePath() ) )
  {
    const char *err = QT_TR_NOOP( "Authentication database could not be deleted" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  mMasterPass = QString();

  QgsDebugMsg( "Creating Auth db through QSqlDatabase initial connection" );

  QSqlDatabase authConn = authDatabaseConnection();
  if ( !authConn.isValid() || !authConn.isOpen() )
  {
    const char *err = QT_TR_NOOP( "Authentication database could not be initialized" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  if ( !createConfigTables() )
  {
    const char *err = QT_TR_NOOP( "FAILED to create auth database config tables" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  if ( !createCertTables() )
  {
    const char *err = QT_TR_NOOP( "FAILED to create auth database cert tables" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  clearAllCachedConfigs();
  updateConfigAuthMethods();
  initSslCaches();

  emit authDatabaseChanged();

  return true;
}

bool QgsAuthManager::updateNetworkRequest( QNetworkRequest &request, const QString &authcfg,
    const QString &dataprovider )
{
  if ( isDisabled() )
    return false;

  QgsAuthMethod *authmethod = configAuthMethod( authcfg );
  if ( authmethod )
  {
    if ( !( authmethod->supportedExpansions() & QgsAuthMethod::NetworkRequest ) )
    {
      QgsDebugMsg( QString( "Network request updating not supported by authcfg: %1" ).arg( authcfg ) );
      return true;
    }

    if ( !authmethod->updateNetworkRequest( request, authcfg, dataprovider.toLower() ) )
    {
      authmethod->clearCachedConfig( authcfg );
      return false;
    }
    return true;
  }

  return false;
}

bool QgsAuthManager::updateNetworkReply( QNetworkReply *reply, const QString &authcfg,
    const QString &dataprovider )
{
  if ( isDisabled() )
    return false;

  QgsAuthMethod *authmethod = configAuthMethod( authcfg );
  if ( authmethod )
  {
    if ( !( authmethod->supportedExpansions() & QgsAuthMethod::NetworkReply ) )
    {
      QgsDebugMsg( QString( "Network reply updating not supported by authcfg: %1" ).arg( authcfg ) );
      return true;
    }

    if ( !authmethod->updateNetworkReply( reply, authcfg, dataprovider.toLower() ) )
    {
      authmethod->clearCachedConfig( authcfg );
      return false;
    }
    return true;
  }

  return false;
}

bool QgsAuthManager::updateDataSourceUriItems( QStringList &connectionItems, const QString &authcfg,
    const QString &dataprovider )
{
  if ( isDisabled() )
    return false;

  QgsAuthMethod *authmethod = configAuthMethod( authcfg );
  if ( authmethod )
  {
    if ( !( authmethod->supportedExpansions() & QgsAuthMethod::DataSourceUri ) )
    {
      QgsDebugMsg( QString( "Data source URI updating not supported by authcfg: %1" ).arg( authcfg ) );
      return true;
    }

    if ( !authmethod->updateDataSourceUriItems( connectionItems, authcfg, dataprovider.toLower() ) )
    {
      authmethod->clearCachedConfig( authcfg );
      return false;
    }
    return true;
  }

  return false;
}

bool QgsAuthManager::storeAuthSetting( const QString &key, const QVariant &value, bool encrypt )
{
  if ( key.isEmpty() )
    return false;

  QString storeval( value.toString() );
  if ( encrypt )
  {
    if ( !setMasterPassword( true ) )
    {
      return false;
    }
    else
    {
      storeval = QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), value.toString() );
    }
  }

  removeAuthSetting( key );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (setting, value) "
                          "VALUES (:setting, :value)" ).arg( authDbSettingsTable() ) );

  query.bindValue( QStringLiteral( ":setting" ), key );
  query.bindValue( QStringLiteral( ":value" ), storeval );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "Store setting SUCCESS for key: %1" ).arg( key ) );
  return true;
}

QVariant QgsAuthManager::getAuthSetting( const QString &key, const QVariant &defaultValue, bool decrypt )
{
  if ( key.isEmpty() )
    return QVariant();

  if ( decrypt && !setMasterPassword( true ) )
    return QVariant();

  QVariant value = defaultValue;
  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT value FROM %1 "
                          "WHERE setting = :setting" ).arg( authDbSettingsTable() ) );

  query.bindValue( QStringLiteral( ":setting" ), key );

  if ( !authDbQuery( &query ) )
    return QVariant();

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      if ( decrypt )
      {
        value = QVariant( QgsAuthCrypto::decrypt( mMasterPass, masterPasswordCiv(), query.value( 0 ).toString() ) );
      }
      else
      {
        value = query.value( 0 );
      }
      QgsDebugMsg( QString( "Authentication setting retrieved for key: %1" ).arg( key ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for setting key: %1" ).arg( key ) );
      emit messageOut( tr( "Authentication database contains duplicate settings" ), authManTag(), WARNING );
      return QVariant();
    }
  }
  return value;
}

bool QgsAuthManager::existsAuthSetting( const QString &key )
{
  if ( key.isEmpty() )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT value FROM %1 "
                          "WHERE setting = :setting" ).arg( authDbSettingsTable() ) );

  query.bindValue( QStringLiteral( ":setting" ), key );

  if ( !authDbQuery( &query ) )
    return false;

  bool res = false;
  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      QgsDebugMsg( QString( "Authentication setting exists for key: %1" ).arg( key ) );
      res = true;
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for setting key: %1" ).arg( key ) );
      emit messageOut( tr( "Authentication database contains duplicate settings" ), authManTag(), WARNING );
      return false;
    }
  }
  return res;
}

bool QgsAuthManager::removeAuthSetting( const QString &key )
{
  if ( key.isEmpty() )
    return false;

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE setting = :setting" ).arg( authDbSettingsTable() ) );

  query.bindValue( QStringLiteral( ":setting" ), key );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "REMOVED setting for key: %1" ).arg( key ) );

  return true;
}


#ifndef QT_NO_SSL

////////////////// Certificate calls ///////////////////////

bool QgsAuthManager::initSslCaches()
{
  bool res = true;
  res = res && rebuildCaCertsCache();
  res = res && rebuildCertTrustCache();
  res = res && rebuildTrustedCaCertsCache();
  res = res && rebuildIgnoredSslErrorCache();

  QgsDebugMsg( QString( "Init of SSL caches %1" ).arg( res ? "SUCCEEDED" : "FAILED" ) );
  return res;
}

bool QgsAuthManager::storeCertIdentity( const QSslCertificate &cert, const QSslKey &key )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }
  if ( key.isNull() )
  {
    QgsDebugMsg( "Passed private key is null" );
    return false;
  }

  if ( !setMasterPassword( true ) )
    return false;

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );
  removeCertIdentity( id );

  QString certpem( cert.toPem() );
  QString keypem( QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), key.toPem() ) );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (id, key, cert) "
                          "VALUES (:id, :key, :cert)" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":key" ), keypem );
  query.bindValue( QStringLiteral( ":cert" ), certpem );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "Store certificate identity SUCCESS for id: %1" ).arg( id ) );
  return true;
}

const QSslCertificate QgsAuthManager::getCertIdentity( const QString &id )
{
  QSslCertificate emptycert;
  QSslCertificate cert;
  if ( id.isEmpty() )
    return emptycert;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT cert FROM %1 "
                          "WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return emptycert;

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      cert = QSslCertificate( query.value( 0 ).toByteArray(), QSsl::Pem );
      QgsDebugMsg( QString( "Certificate identity retrieved for id: %1" ).arg( id ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one certificate identity for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate certificate identity" ), authManTag(), WARNING );
      return emptycert;
    }
  }
  return cert;
}

const QPair<QSslCertificate, QSslKey> QgsAuthManager::getCertIdentityBundle( const QString &id )
{
  QPair<QSslCertificate, QSslKey> bundle;
  if ( id.isEmpty() )
    return bundle;

  if ( !setMasterPassword( true ) )
    return bundle;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT key, cert FROM %1 "
                          "WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return bundle;

  if ( query.isActive() && query.isSelect() )
  {
    QSslCertificate cert;
    QSslKey key;
    if ( query.first() )
    {
      key =  QSslKey( QgsAuthCrypto::decrypt( mMasterPass, masterPasswordCiv(), query.value( 0 ).toString() ).toLatin1(),
                      QSsl::Rsa, QSsl::Pem, QSsl::PrivateKey );
      if ( key.isNull() )
      {
        const char *err = QT_TR_NOOP( "Retrieve certificate identity bundle: FAILED to create private key" );
        QgsDebugMsg( err );
        emit messageOut( tr( err ), authManTag(), WARNING );
        return bundle;
      }
      cert = QSslCertificate( query.value( 1 ).toByteArray(), QSsl::Pem );
      if ( cert.isNull() )
      {
        const char *err = QT_TR_NOOP( "Retrieve certificate identity bundle: FAILED to create certificate" );
        QgsDebugMsg( err );
        emit messageOut( tr( err ), authManTag(), WARNING );
        return bundle;
      }
      QgsDebugMsg( QString( "Certificate identity bundle retrieved for id: %1" ).arg( id ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one certificate identity for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate certificate identity" ), authManTag(), WARNING );
      return bundle;
    }
    bundle = qMakePair( cert, key );
  }
  return bundle;
}

const QStringList QgsAuthManager::getCertIdentityBundleToPem( const QString &id )
{
  QPair<QSslCertificate, QSslKey> bundle( getCertIdentityBundle( id ) );
  if ( bundle.first.isValid() && !bundle.second.isNull() )
  {
    return QStringList() << QString( bundle.first.toPem() ) << QString( bundle.second.toPem() );
  }
  return QStringList();
}

const QList<QSslCertificate> QgsAuthManager::getCertIdentities()
{
  QList<QSslCertificate> certs;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, cert FROM %1" ).arg( authDbIdentitiesTable() ) );

  if ( !authDbQuery( &query ) )
    return certs;

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      certs << QSslCertificate( query.value( 1 ).toByteArray(), QSsl::Pem );
    }
  }

  return certs;
}

QStringList QgsAuthManager::getCertIdentityIds() const
{
  QStringList identityids = QStringList();

  if ( isDisabled() )
    return identityids;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id FROM %1" ).arg( authDbIdentitiesTable() ) );

  if ( !authDbQuery( &query ) )
  {
    return identityids;
  }

  if ( query.isActive() )
  {
    while ( query.next() )
    {
      identityids << query.value( 0 ).toString();
    }
  }
  return identityids;
}

bool QgsAuthManager::existsCertIdentity( const QString &id )
{
  if ( id.isEmpty() )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT cert FROM %1 "
                          "WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return false;

  bool res = false;
  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      QgsDebugMsg( QString( "Certificate bundle exists for id: %1" ).arg( id ) );
      res = true;
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one certificate bundle for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate certificate bundles" ), authManTag(), WARNING );
      return false;
    }
  }
  return res;
}

bool QgsAuthManager::removeCertIdentity( const QString &id )
{
  if ( id.isEmpty() )
  {
    QgsDebugMsg( "Passed bundle ID is empty" );
    return false;
  }

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "REMOVED certificate identity for id: %1" ).arg( id ) );
  return true;
}

bool QgsAuthManager::storeSslCertCustomConfig( const QgsAuthConfigSslServer &config )
{
  if ( config.isNull() )
  {
    QgsDebugMsg( "Passed config is null" );
    return false;
  }

  QSslCertificate cert( config.sslCertificate() );

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );
  removeSslCertCustomConfig( id, config.sslHostPort().trimmed() );

  QString certpem( cert.toPem() );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (id, host, cert, config) "
                          "VALUES (:id, :host, :cert, :config)" ).arg( authDatabaseServersTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":host" ), config.sslHostPort().trimmed() );
  query.bindValue( QStringLiteral( ":cert" ), certpem );
  query.bindValue( QStringLiteral( ":config" ), config.configString() );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "Store SSL cert custom config SUCCESS for host:port, id: %1, %2" )
               .arg( config.sslHostPort().trimmed(), id ) );

  updateIgnoredSslErrorsCacheFromConfig( config );

  return true;
}

const QgsAuthConfigSslServer QgsAuthManager::getSslCertCustomConfig( const QString &id, const QString &hostport )
{
  QgsAuthConfigSslServer config;

  if ( id.isEmpty() || hostport.isEmpty() )
  {
    QgsDebugMsg( "Passed config ID or host:port is empty" );
    return config;
  }

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT id, host, cert, config FROM %1 "
                          "WHERE id = :id AND host = :host" ).arg( authDatabaseServersTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":host" ), hostport.trimmed() );

  if ( !authDbQuery( &query ) )
    return config;

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      config.setSslCertificate( QSslCertificate( query.value( 2 ).toByteArray(), QSsl::Pem ) );
      config.setSslHostPort( query.value( 1 ).toString().trimmed() );
      config.loadConfigString( query.value( 3 ).toString() );
      QgsDebugMsg( QString( "SSL cert custom config retrieved for host:port, id: %1, %2" ).arg( hostport, id ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one SSL cert custom config for host:port, id: %1, %2" ).arg( hostport, id ) );
      emit messageOut( tr( "Authentication database contains duplicate SSL cert custom configs for host:port, id: %1, %2" )
                       .arg( hostport, id ), authManTag(), WARNING );
      QgsAuthConfigSslServer emptyconfig;
      return emptyconfig;
    }
  }
  return config;
}

const QgsAuthConfigSslServer QgsAuthManager::getSslCertCustomConfigByHost( const QString &hostport )
{
  QgsAuthConfigSslServer config;

  if ( hostport.isEmpty() )
  {
    QgsDebugMsg( "Passed host:port is empty" );
    return config;
  }

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT id, host, cert, config FROM %1 "
                          "WHERE host = :host" ).arg( authDatabaseServersTable() ) );

  query.bindValue( QStringLiteral( ":host" ), hostport.trimmed() );

  if ( !authDbQuery( &query ) )
    return config;

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      config.setSslCertificate( QSslCertificate( query.value( 2 ).toByteArray(), QSsl::Pem ) );
      config.setSslHostPort( query.value( 1 ).toString().trimmed() );
      config.loadConfigString( query.value( 3 ).toString() );
      QgsDebugMsg( QString( "SSL cert custom config retrieved for host:port: %1" ).arg( hostport ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one SSL cert custom config for host:port: %1" ).arg( hostport ) );
      emit messageOut( tr( "Authentication database contains duplicate SSL cert custom configs for host:port: %1" )
                       .arg( hostport ), authManTag(), WARNING );
      QgsAuthConfigSslServer emptyconfig;
      return emptyconfig;
    }
  }
  return config;
}

const QList<QgsAuthConfigSslServer> QgsAuthManager::getSslCertCustomConfigs()
{
  QList<QgsAuthConfigSslServer> configs;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, host, cert, config FROM %1" ).arg( authDatabaseServersTable() ) );

  if ( !authDbQuery( &query ) )
    return configs;

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      QgsAuthConfigSslServer config;
      config.setSslCertificate( QSslCertificate( query.value( 2 ).toByteArray(), QSsl::Pem ) );
      config.setSslHostPort( query.value( 1 ).toString().trimmed() );
      config.loadConfigString( query.value( 3 ).toString() );

      configs.append( config );
    }
  }

  return configs;
}

bool QgsAuthManager::existsSslCertCustomConfig( const QString &id, const QString &hostport )
{
  if ( id.isEmpty() || hostport.isEmpty() )
  {
    QgsDebugMsg( "Passed config ID or host:port is empty" );
    return false;
  }

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT cert FROM %1 "
                          "WHERE id = :id AND host = :host" ).arg( authDatabaseServersTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":host" ), hostport.trimmed() );

  if ( !authDbQuery( &query ) )
    return false;

  bool res = false;
  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      QgsDebugMsg( QString( "SSL cert custom config exists for host:port, id: %1, %2" ).arg( hostport, id ) );
      res = true;
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one SSL cert custom config for host:port, id: %1, %2" ).arg( hostport, id ) );
      emit messageOut( tr( "Authentication database contains duplicate SSL cert custom configs for host:port, id: %1, %2" )
                       .arg( hostport, id ), authManTag(), WARNING );
      return false;
    }
  }
  return res;
}

bool QgsAuthManager::removeSslCertCustomConfig( const QString &id, const QString &hostport )
{
  if ( id.isEmpty() || hostport.isEmpty() )
  {
    QgsDebugMsg( "Passed config ID or host:port is empty" );
    return false;
  }

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE id = :id AND host = :host" ).arg( authDatabaseServersTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":host" ), hostport.trimmed() );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QString shahostport( QStringLiteral( "%1:%2" ).arg( id, hostport ) );
  if ( mIgnoredSslErrorsCache.contains( shahostport ) )
  {
    mIgnoredSslErrorsCache.remove( shahostport );
  }

  QgsDebugMsg( QString( "REMOVED SSL cert custom config for host:port, id: %1, %2" ).arg( hostport, id ) );
  dumpIgnoredSslErrorsCache_();
  return true;
}

void QgsAuthManager::dumpIgnoredSslErrorsCache_()
{
  if ( !mIgnoredSslErrorsCache.isEmpty() )
  {
    QgsDebugMsg( "Ignored SSL errors cache items:" );
    QHash<QString, QSet<QSslError::SslError> >::const_iterator i = mIgnoredSslErrorsCache.constBegin();
    while ( i != mIgnoredSslErrorsCache.constEnd() )
    {
      QStringList errs;
      Q_FOREACH ( QSslError::SslError err, i.value() )
      {
        errs << QgsAuthCertUtils::sslErrorEnumString( err );
      }
      QgsDebugMsg( QString( "%1 = %2" ).arg( i.key(), errs.join( ", " ) ) );
      ++i;
    }
  }
  else
  {
    QgsDebugMsg( "Ignored SSL errors cache EMPTY" );
  }
}

bool QgsAuthManager::updateIgnoredSslErrorsCacheFromConfig( const QgsAuthConfigSslServer &config )
{
  if ( config.isNull() )
  {
    QgsDebugMsg( "Passed config is null" );
    return false;
  }

  QString shahostport( QStringLiteral( "%1:%2" )
                       .arg( QgsAuthCertUtils::shaHexForCert( config.sslCertificate() ).trimmed(),
                             config.sslHostPort().trimmed() ) );
  if ( mIgnoredSslErrorsCache.contains( shahostport ) )
  {
    mIgnoredSslErrorsCache.remove( shahostport );
  }
  QList<QSslError::SslError> errenums( config.sslIgnoredErrorEnums() );
  if ( !errenums.isEmpty() )
  {
    mIgnoredSslErrorsCache.insert( shahostport, QSet<QSslError::SslError>::fromList( errenums ) );
    QgsDebugMsg( QString( "Update of ignored SSL errors cache SUCCEEDED for sha:host:port = %1" ).arg( shahostport ) );
    dumpIgnoredSslErrorsCache_();
    return true;
  }

  QgsDebugMsg( QString( "No ignored SSL errors to cache for sha:host:port = %1" ).arg( shahostport ) );
  return true;
}

bool QgsAuthManager::updateIgnoredSslErrorsCache( const QString &shahostport, const QList<QSslError> &errors )
{
  QRegExp rx( "\\S+:\\S+:\\d+" );
  if ( !rx.exactMatch( shahostport ) )
  {
    QgsDebugMsg( "Passed shahostport does not match \\S+:\\S+:\\d+, "
                 "e.g. 74a4ef5ea94512a43769b744cda0ca5049a72491:www.example.com:443" );
    return false;
  }

  if ( mIgnoredSslErrorsCache.contains( shahostport ) )
  {
    mIgnoredSslErrorsCache.remove( shahostport );
  }

  if ( errors.isEmpty() )
  {
    QgsDebugMsg( "Passed errors list empty" );
    return false;
  }

  QSet<QSslError::SslError> errs;
  Q_FOREACH ( const QSslError &error, errors )
  {
    if ( error.error() == QSslError::NoError )
      continue;

    errs.insert( error.error() );
  }

  if ( errs.isEmpty() )
  {
    QgsDebugMsg( "Passed errors list does not contain errors" );
    return false;
  }

  mIgnoredSslErrorsCache.insert( shahostport, errs );

  QgsDebugMsg( QString( "Update of ignored SSL errors cache SUCCEEDED for sha:host:port = %1" ).arg( shahostport ) );
  dumpIgnoredSslErrorsCache_();
  return true;
}

bool QgsAuthManager::rebuildIgnoredSslErrorCache()
{
  QHash<QString, QSet<QSslError::SslError> > prevcache( mIgnoredSslErrorsCache );
  QHash<QString, QSet<QSslError::SslError> > nextcache;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, host, config FROM %1" ).arg( authDatabaseServersTable() ) );

  if ( !authDbQuery( &query ) )
  {
    QgsDebugMsg( "Rebuild of ignored SSL errors cache FAILED" );
    return false;
  }

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      QString shahostport( QStringLiteral( "%1:%2" )
                           .arg( query.value( 0 ).toString().trimmed(),
                                 query.value( 1 ).toString().trimmed() ) );
      QgsAuthConfigSslServer config;
      config.loadConfigString( query.value( 2 ).toString() );
      QList<QSslError::SslError> errenums( config.sslIgnoredErrorEnums() );
      if ( !errenums.isEmpty() )
      {
        nextcache.insert( shahostport, QSet<QSslError::SslError>::fromList( errenums ) );
      }
      if ( prevcache.contains( shahostport ) )
      {
        prevcache.remove( shahostport );
      }
    }
  }

  if ( !prevcache.isEmpty() )
  {
    // preserve any existing per-session ignored errors for hosts
    QHash<QString, QSet<QSslError::SslError> >::const_iterator i = prevcache.constBegin();
    while ( i != prevcache.constEnd() )
    {
      nextcache.insert( i.key(), i.value() );
      ++i;
    }
  }

  if ( nextcache != mIgnoredSslErrorsCache )
  {
    mIgnoredSslErrorsCache.clear();
    mIgnoredSslErrorsCache = nextcache;
    QgsDebugMsg( "Rebuild of ignored SSL errors cache SUCCEEDED" );
    dumpIgnoredSslErrorsCache_();
    return true;
  }

  QgsDebugMsg( "Rebuild of ignored SSL errors cache SAME AS BEFORE" );
  dumpIgnoredSslErrorsCache_();
  return true;
}


bool QgsAuthManager::storeCertAuthorities( const QList<QSslCertificate> &certs )
{
  if ( certs.size() < 1 )
  {
    QgsDebugMsg( "Passed certificate list has no certs" );
    return false;
  }

  Q_FOREACH ( const QSslCertificate &cert, certs )
  {
    if ( !storeCertAuthority( cert ) )
      return false;
  }
  return true;
}

bool QgsAuthManager::storeCertAuthority( const QSslCertificate &cert )
{
  // don't refuse !cert.isValid() (actually just expired) CAs,
  // as user may want to ignore that SSL connection error
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }

  removeCertAuthority( cert );

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );
  QString pem( cert.toPem() );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (id, cert) "
                          "VALUES (:id, :cert)" ).arg( authDbAuthoritiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":cert" ), pem );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "Store certificate authority SUCCESS for id: %1" ).arg( id ) );
  return true;
}

const QSslCertificate QgsAuthManager::getCertAuthority( const QString &id )
{
  QSslCertificate emptycert;
  QSslCertificate cert;
  if ( id.isEmpty() )
    return emptycert;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT cert FROM %1 "
                          "WHERE id = :id" ).arg( authDbAuthoritiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return emptycert;

  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      cert = QSslCertificate( query.value( 0 ).toByteArray(), QSsl::Pem );
      QgsDebugMsg( QString( "Certificate authority retrieved for id: %1" ).arg( id ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one certificate authority for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate certificate authorities" ), authManTag(), WARNING );
      return emptycert;
    }
  }
  return cert;
}

bool QgsAuthManager::existsCertAuthority( const QSslCertificate &cert )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT value FROM %1 "
                          "WHERE id = :id" ).arg( authDbAuthoritiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return false;

  bool res = false;
  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      QgsDebugMsg( QString( "Certificate authority exists for id: %1" ).arg( id ) );
      res = true;
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one certificate authority for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate certificate authorities" ), authManTag(), WARNING );
      return false;
    }
  }
  return res;
}

bool QgsAuthManager::removeCertAuthority( const QSslCertificate &cert )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE id = :id" ).arg( authDbAuthoritiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "REMOVED authority for id: %1" ).arg( id ) );
  return true;
}

const QList<QSslCertificate> QgsAuthManager::getSystemRootCAs()
{
#ifndef Q_OS_MAC
  return QSslSocket::systemCaCertificates();
#else
  QNetworkRequest req;
  return req.sslConfiguration().caCertificates();
#endif
}

const QList<QSslCertificate> QgsAuthManager::getExtraFileCAs()
{
  QList<QSslCertificate> certs;
  QList<QSslCertificate> filecerts;
  QVariant cafileval = QgsAuthManager::instance()->getAuthSetting( QStringLiteral( "cafile" ) );
  if ( cafileval.isNull() )
    return certs;

  QVariant allowinvalid = QgsAuthManager::instance()->getAuthSetting( QStringLiteral( "cafileallowinvalid" ), QVariant( false ) );
  if ( allowinvalid.isNull() )
    return certs;

  QString cafile( cafileval.toString() );
  if ( !cafile.isEmpty() && QFile::exists( cafile ) )
  {
    filecerts = QgsAuthCertUtils::certsFromFile( cafile );
  }
  // only CAs or certs capable of signing other certs are allowed
  Q_FOREACH ( const QSslCertificate &cert, filecerts )
  {
    if ( !allowinvalid.toBool() && !cert.isValid() )
    {
      continue;
    }

    if ( QgsAuthCertUtils::certificateIsAuthorityOrIssuer( cert ) )
    {
      certs << cert;
    }
  }
  return certs;
}

const QList<QSslCertificate> QgsAuthManager::getDatabaseCAs()
{
  QList<QSslCertificate> certs;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, cert FROM %1" ).arg( authDbAuthoritiesTable() ) );

  if ( !authDbQuery( &query ) )
    return certs;

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      certs << QSslCertificate( query.value( 1 ).toByteArray(), QSsl::Pem );
    }
  }

  return certs;
}

const QMap<QString, QSslCertificate> QgsAuthManager::getMappedDatabaseCAs()
{
  return QgsAuthCertUtils::mapDigestToCerts( getDatabaseCAs() );
}

bool QgsAuthManager::rebuildCaCertsCache()
{
  mCaCertsCache.clear();
  // in reverse order of precedence, with regards to duplicates, so QMap inserts overwrite
  insertCaCertInCache( QgsAuthCertUtils::SystemRoot, getSystemRootCAs() );
  insertCaCertInCache( QgsAuthCertUtils::FromFile, getExtraFileCAs() );
  insertCaCertInCache( QgsAuthCertUtils::InDatabase, getDatabaseCAs() );

  bool res = !mCaCertsCache.isEmpty(); // should at least contain system root CAs
  QgsDebugMsg( QString( "Rebuild of CA certs cache %1" ).arg( res ? "SUCCEEDED" : "FAILED" ) );
  return res;
}

bool QgsAuthManager::storeCertTrustPolicy( const QSslCertificate &cert, QgsAuthCertUtils::CertTrustPolicy policy )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }

  removeCertTrustPolicy( cert );

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );

  if ( policy == QgsAuthCertUtils::DefaultTrust )
  {
    QgsDebugMsg( QString( "Passed policy was default, all cert records in database were removed for id: %1" ).arg( id ) );
    return true;
  }

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "INSERT INTO %1 (id, policy) "
                          "VALUES (:id, :policy)" ).arg( authDbTrustTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );
  query.bindValue( QStringLiteral( ":policy" ), static_cast< int >( policy ) );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "Store certificate trust policy SUCCESS for id: %1" ).arg( id ) );
  return true;
}

QgsAuthCertUtils::CertTrustPolicy QgsAuthManager::getCertTrustPolicy( const QSslCertificate &cert )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return QgsAuthCertUtils::DefaultTrust;
  }

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QString( "SELECT policy FROM %1 "
                          "WHERE id = :id" ).arg( authDbTrustTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbQuery( &query ) )
    return QgsAuthCertUtils::DefaultTrust;

  QgsAuthCertUtils::CertTrustPolicy policy( QgsAuthCertUtils::DefaultTrust );
  if ( query.isActive() && query.isSelect() )
  {
    if ( query.first() )
    {
      policy = static_cast< QgsAuthCertUtils::CertTrustPolicy >( query.value( 0 ).toInt() );
      QgsDebugMsg( QString( "Authentication cert trust policy retrieved for id: %1" ).arg( id ) );
    }
    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one cert trust policy for id: %1" ).arg( id ) );
      emit messageOut( tr( "Authentication database contains duplicate cert trust policies" ), authManTag(), WARNING );
      return QgsAuthCertUtils::DefaultTrust;
    }
  }
  return policy;
}

bool QgsAuthManager::removeCertTrustPolicies( const QList<QSslCertificate> &certs )
{
  if ( certs.size() < 1 )
  {
    QgsDebugMsg( "Passed certificate list has no certs" );
    return false;
  }

  Q_FOREACH ( const QSslCertificate &cert, certs )
  {
    if ( !removeCertTrustPolicy( cert ) )
      return false;
  }
  return true;
}

bool QgsAuthManager::removeCertTrustPolicy( const QSslCertificate &cert )
{
  if ( cert.isNull() )
  {
    QgsDebugMsg( "Passed certificate is null" );
    return false;
  }

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "DELETE FROM %1 WHERE id = :id" ).arg( authDbTrustTable() ) );

  query.bindValue( QStringLiteral( ":id" ), id );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  QgsDebugMsg( QString( "REMOVED cert trust policy for id: %1" ).arg( id ) );

  return true;
}

QgsAuthCertUtils::CertTrustPolicy QgsAuthManager::getCertificateTrustPolicy( const QSslCertificate &cert )
{
  if ( cert.isNull() )
  {
    return QgsAuthCertUtils::NoPolicy;
  }

  QString id( QgsAuthCertUtils::shaHexForCert( cert ) );
  const QStringList &trustedids = mCertTrustCache.value( QgsAuthCertUtils::Trusted );
  const QStringList &untrustedids = mCertTrustCache.value( QgsAuthCertUtils::Untrusted );

  QgsAuthCertUtils::CertTrustPolicy policy( QgsAuthCertUtils::DefaultTrust );
  if ( trustedids.contains( id ) )
  {
    policy = QgsAuthCertUtils::Trusted;
  }
  else if ( untrustedids.contains( id ) )
  {
    policy = QgsAuthCertUtils::Untrusted;
  }
  return policy;
}

bool QgsAuthManager::setDefaultCertTrustPolicy( QgsAuthCertUtils::CertTrustPolicy policy )
{
  if ( policy == QgsAuthCertUtils::DefaultTrust )
  {
    // set default trust policy to Trusted by removing setting
    return removeAuthSetting( QStringLiteral( "certdefaulttrust" ) );
  }
  return storeAuthSetting( QStringLiteral( "certdefaulttrust" ), static_cast< int >( policy ) );
}

QgsAuthCertUtils::CertTrustPolicy QgsAuthManager::defaultCertTrustPolicy()
{
  QVariant policy( getAuthSetting( QStringLiteral( "certdefaulttrust" ) ) );
  if ( policy.isNull() )
  {
    return QgsAuthCertUtils::Trusted;
  }
  return static_cast< QgsAuthCertUtils::CertTrustPolicy >( policy.toInt() );
}

bool QgsAuthManager::rebuildCertTrustCache()
{
  mCertTrustCache.clear();

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id, policy FROM %1" ).arg( authDbTrustTable() ) );

  if ( !authDbQuery( &query ) )
  {
    QgsDebugMsg( "Rebuild of cert trust policy cache FAILED" );
    return false;
  }

  if ( query.isActive() && query.isSelect() )
  {
    while ( query.next() )
    {
      QString id = query.value( 0 ).toString();
      QgsAuthCertUtils::CertTrustPolicy policy = static_cast< QgsAuthCertUtils::CertTrustPolicy >( query.value( 1 ).toInt() );

      QStringList ids;
      if ( mCertTrustCache.contains( policy ) )
      {
        ids = mCertTrustCache.value( policy );
      }
      mCertTrustCache.insert( policy, ids << id );
    }
  }

  QgsDebugMsg( "Rebuild of cert trust policy cache SUCCEEDED" );
  return true;
}

const QList<QSslCertificate> QgsAuthManager::getTrustedCaCerts( bool includeinvalid )
{
  QgsAuthCertUtils::CertTrustPolicy defaultpolicy( defaultCertTrustPolicy() );
  QStringList trustedids = mCertTrustCache.value( QgsAuthCertUtils::Trusted );
  QStringList untrustedids = mCertTrustCache.value( QgsAuthCertUtils::Untrusted );
  const QList<QPair<QgsAuthCertUtils::CaCertSource, QSslCertificate> > &certpairs( mCaCertsCache.values() );

  QList<QSslCertificate> trustedcerts;
  for ( int i = 0; i < certpairs.size(); ++i )
  {
    QSslCertificate cert( certpairs.at( i ).second );
    QString certid( QgsAuthCertUtils::shaHexForCert( cert ) );
    if ( trustedids.contains( certid ) )
    {
      // trusted certs are always added regardless of their validity
      trustedcerts.append( cert );
    }
    else if ( defaultpolicy == QgsAuthCertUtils::Trusted && !untrustedids.contains( certid ) )
    {
      if ( !includeinvalid && !cert.isValid() )
        continue;
      trustedcerts.append( cert );
    }
  }

  // update application default SSL config for new requests
  QSslConfiguration sslconfig( QSslConfiguration::defaultConfiguration() );
  sslconfig.setCaCertificates( trustedcerts );
  QSslConfiguration::setDefaultConfiguration( sslconfig );

  return trustedcerts;
}

const QList<QSslCertificate> QgsAuthManager::getUntrustedCaCerts( QList<QSslCertificate> trustedCAs )
{
  if ( trustedCAs.isEmpty() )
  {
    if ( mTrustedCaCertsCache.isEmpty() )
    {
      rebuildTrustedCaCertsCache();
    }
    trustedCAs = getTrustedCaCertsCache();
  }

  const QList<QPair<QgsAuthCertUtils::CaCertSource, QSslCertificate> > &certpairs( mCaCertsCache.values() );

  QList<QSslCertificate> untrustedCAs;
  for ( int i = 0; i < certpairs.size(); ++i )
  {
    QSslCertificate cert( certpairs.at( i ).second );
    if ( !trustedCAs.contains( cert ) )
    {
      untrustedCAs.append( cert );
    }
  }
  return untrustedCAs;
}

bool QgsAuthManager::rebuildTrustedCaCertsCache()
{
  mTrustedCaCertsCache = getTrustedCaCerts();
  QgsDebugMsg( "Rebuilt trusted cert authorities cache" );
  // TODO: add some error trapping for the operation
  return true;
}

const QByteArray QgsAuthManager::getTrustedCaCertsPemText()
{
  QByteArray capem;
  QList<QSslCertificate> certs( getTrustedCaCertsCache() );
  if ( !certs.isEmpty() )
  {
    QStringList certslist;
    Q_FOREACH ( const QSslCertificate &cert, certs )
    {
      certslist << cert.toPem();
    }
    capem = certslist.join( QStringLiteral( "\n" ) ).toLatin1(); //+ "\n";
  }
  return capem;
}

bool QgsAuthManager::passwordHelperSync()
{
  if ( masterPasswordIsSet() )
  {
    return passwordHelperWrite( mMasterPass );
  }
  return false;
}


////////////////// Certificate calls - end ///////////////////////

#endif

void QgsAuthManager::clearAllCachedConfigs()
{
  if ( isDisabled() )
    return;

  Q_FOREACH ( QString authcfg, configIds() )
  {
    clearCachedConfig( authcfg );
  }
}

void QgsAuthManager::clearCachedConfig( const QString &authcfg )
{
  if ( isDisabled() )
    return;

  QgsAuthMethod *authmethod = configAuthMethod( authcfg );
  if ( authmethod )
  {
    authmethod->clearCachedConfig( authcfg );
  }
}

void QgsAuthManager::writeToConsole( const QString &message,
                                     const QString &tag,
                                     QgsAuthManager::MessageLevel level )
{
  Q_UNUSED( tag );

  // only output WARNING and CRITICAL messages
  if ( level == QgsAuthManager::INFO )
    return;

  QString msg;
  switch ( level )
  {
    case QgsAuthManager::WARNING:
      msg += QLatin1String( "WARNING: " );
      break;
    case QgsAuthManager::CRITICAL:
      msg += QLatin1String( "ERROR: " );
      break;
    default:
      break;
  }
  msg += message;

  QTextStream out( stdout, QIODevice::WriteOnly );
  out << msg << endl;
}

void QgsAuthManager::tryToStartDbErase()
{
  ++mScheduledDbEraseRequestCount;
  // wait a total of 90 seconds for GUI availiability or user interaction, then cancel schedule
  int trycutoff = 90 / ( mScheduledDbEraseRequestWait ? mScheduledDbEraseRequestWait : 3 );
  if ( mScheduledDbEraseRequestCount >= trycutoff )
  {
    setScheduledAuthDatabaseErase( false );
    QgsDebugMsg( "authDatabaseEraseRequest emitting/scheduling canceled" );
    return;
  }
  else
  {
    QgsDebugMsg( QString( "authDatabaseEraseRequest attempt (%1 of %2)" )
                 .arg( mScheduledDbEraseRequestCount ).arg( trycutoff ) );
  }

  if ( scheduledAuthDatabaseErase() && !mScheduledDbEraseRequestEmitted && mMutex->tryLock() )
  {
    // see note in header about this signal's use
    mScheduledDbEraseRequestEmitted = true;
    emit authDatabaseEraseRequested();

    mMutex->unlock();

    QgsDebugMsg( "authDatabaseEraseRequest emitted" );
    return;
  }
  QgsDebugMsg( "authDatabaseEraseRequest emit skipped" );
}

QgsAuthManager::QgsAuthManager()
  : QObject()
  , mAuthInit( false )
  , mAuthDbPath( QString() )
  , mQcaInitializer( nullptr )
  , mMasterPass( QString() )
  , mPassTries( 0 )
  , mAuthDisabled( false )
  , mScheduledDbEraseTimer( nullptr )
  , mScheduledDbErase( false )
  , mScheduledDbEraseRequestWait( 3 )
  , mScheduledDbEraseRequestEmitted( false )
  , mScheduledDbEraseRequestCount( 0 )
  , mMutex( nullptr )
  , mIgnoredSslErrorsCache( QHash<QString, QSet<QSslError::SslError> >() )
  , mPasswordHelperVerificationError( false )
  , mPasswordHelperErrorMessage( "" )
  , mPasswordHelperErrorCode( QKeychain::NoError )
  , mPasswordHelperLoggingEnabled( false )
  , mPasswordHelperFailedInit( false )

{
  mMutex = new QMutex( QMutex::Recursive );
  connect( this, &QgsAuthManager::messageOut,
           this, &QgsAuthManager::writeToConsole );
}

QgsAuthManager::~QgsAuthManager()
{
  if ( !isDisabled() )
  {
    delete QgsAuthMethodRegistry::instance();
    qDeleteAll( mAuthMethods );

    QSqlDatabase authConn = authDatabaseConnection();
    if ( authConn.isValid() && authConn.isOpen() )
      authConn.close();
  }
  delete mMutex;
  mMutex = nullptr;
  delete mScheduledDbEraseTimer;
  mScheduledDbEraseTimer = nullptr;
  delete mQcaInitializer;
  mQcaInitializer = nullptr;
  QSqlDatabase::removeDatabase( QStringLiteral( "authentication.configs" ) );
}


QString QgsAuthManager::passwordHelperName() const
{
  return tr( "Password Helper" );
}


void QgsAuthManager::passwordHelperLog( const QString &msg ) const
{
  if ( passwordHelperLoggingEnabled() )
  {
    QgsMessageLog::logMessage( msg, passwordHelperName() );
  }
}

bool QgsAuthManager::passwordHelperDelete()
{
  passwordHelperLog( tr( "Opening %1 for DELETE  ..." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ) );
  bool result;
  QKeychain::DeletePasswordJob job( AUTH_PASSWORD_HELPER_FOLDER_NAME );
  QgsSettings settings;
  job.setInsecureFallback( settings.value( QStringLiteral( "password_helper_insecure_fallback" ), false, QgsSettings::Section::Auth ).toBool() );
  job.setAutoDelete( false );
  job.setKey( AUTH_PASSWORD_HELPER_KEY_NAME );
  QEventLoop loop;
  connect( &job, &QKeychain::Job::finished, &loop, &QEventLoop::quit );
  job.start();
  loop.exec();
  if ( job.error() )
  {
    mPasswordHelperErrorCode = job.error();
    mPasswordHelperErrorMessage = tr( "Delete password failed: %1." ).arg( job.errorString() );
    // Signals used in the tests to exit main application loop
    emit passwordHelperFailure();
    result = false;
  }
  else
  {
    // Signals used in the tests to exit main application loop
    emit passwordHelperSuccess();
    result = true;
  }
  passwordHelperProcessError();
  return result;
}

QString QgsAuthManager::passwordHelperRead()
{
  // Retrieve it!
  QString password( "" );
  passwordHelperLog( tr( "Opening %1 for READ ..." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ) );
  QKeychain::ReadPasswordJob job( AUTH_PASSWORD_HELPER_FOLDER_NAME );
  QgsSettings settings;
  job.setInsecureFallback( settings.value( QStringLiteral( "password_helper_insecure_fallback" ), false, QgsSettings::Section::Auth ).toBool() );
  job.setAutoDelete( false );
  job.setKey( AUTH_PASSWORD_HELPER_KEY_NAME );
  QEventLoop loop;
  connect( &job, &QKeychain::Job::finished, &loop, &QEventLoop::quit );
  job.start();
  loop.exec();
  if ( job.error() )
  {
    mPasswordHelperErrorCode = job.error();
    mPasswordHelperErrorMessage = tr( "Retrieving password from your %1 failed: %2." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME, job.errorString() );
    // Signals used in the tests to exit main application loop
    emit passwordHelperFailure();
  }
  else
  {
    password = job.textData();
    // Password is there but it is empty, treat it like if it was not found
    if ( password.isEmpty() )
    {
      mPasswordHelperErrorCode = QKeychain::EntryNotFound;
      mPasswordHelperErrorMessage = tr( "Empty password retrieved from your %1." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME );
      // Signals used in the tests to exit main application loop
      emit passwordHelperFailure();
    }
    else
    {
      // Signals used in the tests to exit main application loop
      emit passwordHelperSuccess();
    }
  }
  passwordHelperProcessError();
  return password;
}

bool QgsAuthManager::passwordHelperWrite( const QString &password )
{
  Q_ASSERT( !password.isEmpty() );
  bool result;
  passwordHelperLog( tr( "Opening %1 for WRITE ..." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ) );
  QKeychain::WritePasswordJob job( AUTH_PASSWORD_HELPER_FOLDER_NAME );
  QgsSettings settings;
  job.setInsecureFallback( settings.value( QStringLiteral( "password_helper_insecure_fallback" ), false, QgsSettings::Section::Auth ).toBool() );
  job.setAutoDelete( false );
  job.setKey( AUTH_PASSWORD_HELPER_KEY_NAME );
  job.setTextData( password );
  QEventLoop loop;
  connect( &job, &QKeychain::Job::finished, &loop, &QEventLoop::quit );
  job.start();
  loop.exec();
  if ( job.error() )
  {
    mPasswordHelperErrorCode = job.error();
    mPasswordHelperErrorMessage = tr( "Storing password in your %1 failed: %2." ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME, job.errorString() );
    // Signals used in the tests to exit main application loop
    emit passwordHelperFailure();
    result = false;
  }
  else
  {
    passwordHelperClearErrors();
    // Signals used in the tests to exit main application loop
    emit passwordHelperSuccess();
    result = true;
  }
  passwordHelperProcessError();
  return result;
}

bool QgsAuthManager::passwordHelperEnabled() const
{
  // Does the user want to store the password in the wallet?
  QgsSettings settings;
  return settings.value( QStringLiteral( "use_password_helper" ), true, QgsSettings::Section::Auth ).toBool();
}

void QgsAuthManager::setPasswordHelperEnabled( const bool enabled )
{
  QgsSettings settings;
  settings.setValue( QStringLiteral( "use_password_helper" ),  enabled, QgsSettings::Section::Auth );
  emit messageOut( enabled ? tr( "Your %1 will be <b>used from now</b> on to store and retrieve the master password." )
                   .arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ) :
                   tr( "Your %1 will <b>not be used anymore</b> to store and retrieve the master password." )
                   .arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ) );
}

bool QgsAuthManager::passwordHelperLoggingEnabled() const
{
  // Does the user want to store the password in the wallet?
  QgsSettings settings;
  return settings.value( QStringLiteral( "password_helper_logging" ), false, QgsSettings::Section::Auth ).toBool();
}

void QgsAuthManager::setPasswordHelperLoggingEnabled( const bool enabled )
{
  QgsSettings settings;
  settings.setValue( QStringLiteral( "password_helper_logging" ),  enabled, QgsSettings::Section::Auth );
}

void QgsAuthManager::passwordHelperClearErrors()
{
  mPasswordHelperErrorCode = QKeychain::NoError;
  mPasswordHelperErrorMessage = "";
}

void QgsAuthManager::passwordHelperProcessError()
{
  if ( mPasswordHelperErrorCode == QKeychain::AccessDenied ||
       mPasswordHelperErrorCode == QKeychain::AccessDeniedByUser ||
       mPasswordHelperErrorCode == QKeychain::NoBackendAvailable ||
       mPasswordHelperErrorCode == QKeychain::NotImplemented )
  {
    // If the error is permanent or the user denied access to the wallet
    // we also want to disable the wallet system to prevent annoying
    // notification on each subsequent access.
    setPasswordHelperEnabled( false );
    mPasswordHelperErrorMessage = tr( "There was an error and integration with your %1 system has been disabled. "
                                      "You can re-enable it at any time through the \"Utilities\" menu "
                                      "in the Authentication pane of the options dialog. %2" )
                                  .arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME )
                                  .arg( mPasswordHelperErrorMessage );
  }
  if ( mPasswordHelperErrorCode != QKeychain::NoError )
  {
    // We've got an error from the wallet
    passwordHelperLog( tr( "Error in %1: %2" ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME, mPasswordHelperErrorMessage ) );
    emit passwordHelperMessageOut( mPasswordHelperErrorMessage, authManTag(), CRITICAL );
  }
  passwordHelperClearErrors();
}


bool QgsAuthManager::masterPasswordInput()
{
  if ( isDisabled() )
    return false;

  QString pass;
  bool storedPasswordIsValid = false;
  bool ok = false;

  // Read the password from the wallet
  if ( passwordHelperEnabled() )
  {
    pass = passwordHelperRead();
    if ( ! pass.isEmpty() && ( mPasswordHelperErrorCode == QKeychain::NoError ) )
    {
      // Let's check the password!
      if ( verifyMasterPassword( pass ) )
      {
        ok = true;
        storedPasswordIsValid = true;
        emit passwordHelperMessageOut( tr( "Master password has been successfully read from your %1" ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ), authManTag(), INFO );
      }
      else
      {
        emit passwordHelperMessageOut( tr( "Master password stored in your %1 is not valid" ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ), authManTag(), WARNING );
      }
    }
  }

  if ( ! ok )
  {
    QgsCredentials *creds = QgsCredentials::instance();
    creds->lock();
    pass.clear();
    ok = creds->getMasterPassword( pass, masterPasswordHashInDatabase() );
    creds->unlock();
  }

  if ( ok && !pass.isEmpty() && mMasterPass != pass )
  {
    mMasterPass = pass;
    if ( passwordHelperEnabled() && ! storedPasswordIsValid )
    {
      if ( passwordHelperWrite( pass ) )
      {
        emit passwordHelperMessageOut( tr( "Master password has been successfully written to your %1" ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ), authManTag(), INFO );
      }
      else
      {
        emit passwordHelperMessageOut( tr( "Master password could not be written to your %1" ).arg( AUTH_PASSWORD_HELPER_DISPLAY_NAME ), authManTag(), WARNING );
      }
    }
    return true;
  }
  return false;
}

bool QgsAuthManager::masterPasswordRowsInDb( int *rows ) const
{
  if ( isDisabled() )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT Count(*) FROM %1" ).arg( authDbPassTable() ) );

  bool ok = authDbQuery( &query );
  if ( query.first() )
  {
    *rows = query.value( 0 ).toInt();
  }

  return ok;
}

bool QgsAuthManager::masterPasswordHashInDatabase() const
{
  if ( isDisabled() )
    return false;

  int rows = 0;
  if ( !masterPasswordRowsInDb( &rows ) )
  {
    const char *err = QT_TR_NOOP( "Master password: FAILED to access database" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), CRITICAL );

    return false;
  }
  return ( rows == 1 );
}

bool QgsAuthManager::masterPasswordCheckAgainstDb( const QString &compare ) const
{
  if ( isDisabled() )
    return false;

  // first verify there is only one row in auth db (uses first found)

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT salt, hash FROM %1" ).arg( authDbPassTable() ) );
  if ( !authDbQuery( &query ) )
    return false;

  if ( !query.first() )
    return false;

  QString salt = query.value( 0 ).toString();
  QString hash = query.value( 1 ).toString();

  return QgsAuthCrypto::verifyPasswordKeyHash( compare.isNull() ? mMasterPass : compare, salt, hash );
}

bool QgsAuthManager::masterPasswordStoreInDb() const
{
  if ( isDisabled() )
    return false;

  QString salt, hash, civ;
  QgsAuthCrypto::passwordKeyHash( mMasterPass, &salt, &hash, &civ );

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "INSERT INTO %1 (salt, hash, civ) VALUES (:salt, :hash, :civ)" ).arg( authDbPassTable() ) );

  query.bindValue( QStringLiteral( ":salt" ), salt );
  query.bindValue( QStringLiteral( ":hash" ), hash );
  query.bindValue( QStringLiteral( ":civ" ), civ );

  if ( !authDbStartTransaction() )
    return false;

  if ( !authDbQuery( &query ) )
    return false;

  if ( !authDbCommit() )
    return false;

  return true;
}

bool QgsAuthManager::masterPasswordClearDb()
{
  if ( isDisabled() )
    return false;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "DELETE FROM %1" ).arg( authDbPassTable() ) );
  bool res = authDbTransactionQuery( &query );
  if ( res )
    clearMasterPassword();
  return res;
}

const QString QgsAuthManager::masterPasswordCiv() const
{
  if ( isDisabled() )
    return QString();

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT civ FROM %1" ).arg( authDbPassTable() ) );
  if ( !authDbQuery( &query ) )
    return QString();

  if ( !query.first() )
    return QString();

  return query.value( 0 ).toString();
}

QStringList QgsAuthManager::configIds() const
{
  QStringList configids = QStringList();

  if ( isDisabled() )
    return configids;

  QSqlQuery query( authDatabaseConnection() );
  query.prepare( QStringLiteral( "SELECT id FROM %1" ).arg( authDatabaseConfigTable() ) );

  if ( !authDbQuery( &query ) )
  {
    return configids;
  }

  if ( query.isActive() )
  {
    while ( query.next() )
    {
      configids << query.value( 0 ).toString();
    }
  }
  return configids;
}

bool QgsAuthManager::verifyPasswordCanDecryptConfigs() const
{
  if ( isDisabled() )
    return false;

  // no need to check for setMasterPassword, since this is private and it will be set

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QStringLiteral( "SELECT id, config FROM %1" ).arg( authDatabaseConfigTable() ) );

  if ( !authDbQuery( &query ) )
    return false;

  if ( !query.isActive() || !query.isSelect() )
  {
    QgsDebugMsg( QString( "Verify password can decrypt configs FAILED, query not active or a select operation" ) );
    return false;
  }

  int checked = 0;
  while ( query.next() )
  {
    ++checked;
    QString configstring( QgsAuthCrypto::decrypt( mMasterPass, masterPasswordCiv(), query.value( 1 ).toString() ) );
    if ( configstring.isEmpty() )
    {
      QgsDebugMsg( QString( "Verify password can decrypt configs FAILED, could not decrypt a config (id: %1)" )
                   .arg( query.value( 0 ).toString() ) );
      return false;
    }
  }

  QgsDebugMsg( QString( "Verify password can decrypt configs SUCCESS (checked %1 configs)" ).arg( checked ) );
  return true;
}

bool QgsAuthManager::reencryptAllAuthenticationConfigs( const QString &prevpass, const QString &prevciv )
{
  if ( isDisabled() )
    return false;

  bool res = true;
  Q_FOREACH ( QString configid, configIds() )
  {
    res = res && reencryptAuthenticationConfig( configid, prevpass, prevciv );
  }
  return res;
}

bool QgsAuthManager::reencryptAuthenticationConfig( const QString &authcfg, const QString &prevpass, const QString &prevciv )
{
  if ( isDisabled() )
    return false;

  // no need to check for setMasterPassword, since this is private and it will be set

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QString( "SELECT config FROM %1 "
                          "WHERE id = :id" ).arg( authDatabaseConfigTable() ) );

  query.bindValue( QStringLiteral( ":id" ), authcfg );

  if ( !authDbQuery( &query ) )
    return false;

  if ( !query.isActive() || !query.isSelect() )
  {
    QgsDebugMsg( QString( "Reencrypt FAILED, query not active or a select operation for authcfg: %2" ).arg( authcfg ) );
    return false;
  }

  if ( query.first() )
  {
    QString configstring( QgsAuthCrypto::decrypt( prevpass, prevciv, query.value( 0 ).toString() ) );

    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for authcfg: %1" ).arg( authcfg ) );
      emit messageOut( tr( "Authentication database contains duplicate configuration IDs" ), authManTag(), WARNING );
      return false;
    }

    query.clear();

    query.prepare( QString( "UPDATE %1 "
                            "SET config = :config "
                            "WHERE id = :id" ).arg( authDatabaseConfigTable() ) );

    query.bindValue( QStringLiteral( ":id" ), authcfg );
    query.bindValue( QStringLiteral( ":config" ), QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), configstring ) );

    if ( !authDbStartTransaction() )
      return false;

    if ( !authDbQuery( &query ) )
      return false;

    if ( !authDbCommit() )
      return false;

    QgsDebugMsg( QString( "Reencrypt SUCCESS for authcfg: %2" ).arg( authcfg ) );
    return true;
  }
  else
  {
    QgsDebugMsg( QString( "Reencrypt FAILED, could not find in db authcfg: %2" ).arg( authcfg ) );
    return false;
  }
}

bool QgsAuthManager::reencryptAllAuthenticationSettings( const QString &prevpass, const QString &prevciv )
{
  // TODO: start remove (when function is actually used)
  Q_UNUSED( prevpass );
  Q_UNUSED( prevciv );
  return true;
  // end remove

#if 0
  if ( isDisabled() )
    return false;

  ///////////////////////////////////////////////////////////////
  // When adding settings that require encryption, add to list //
  ///////////////////////////////////////////////////////////////

  QStringList encryptedsettings;
  encryptedsettings << "";

  Q_FOREACH ( const QString &sett, encryptedsettings )
  {
    if ( sett.isEmpty() || !existsAuthSetting( sett ) )
      continue;

    // no need to check for setMasterPassword, since this is private and it will be set

    QSqlQuery query( authDbConnection() );

    query.prepare( QString( "SELECT value FROM %1 "
                            "WHERE setting = :setting" ).arg( authDbSettingsTable() ) );

    query.bindValue( ":setting", sett );

    if ( !authDbQuery( &query ) )
      return false;

    if ( !query.isActive() || !query.isSelect() )
    {
      QgsDebugMsg( QString( "Reencrypt FAILED, query not active or a select operation for setting: %2" ).arg( sett ) );
      return false;
    }

    if ( query.first() )
    {
      QString settvalue( QgsAuthCrypto::decrypt( prevpass, prevciv, query.value( 0 ).toString() ) );

      query.clear();

      query.prepare( QString( "UPDATE %1 "
                              "SET value = :value "
                              "WHERE setting = :setting" ).arg( authDbSettingsTable() ) );

      query.bindValue( ":setting", sett );
      query.bindValue( ":value", QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), settvalue ) );

      if ( !authDbStartTransaction() )
        return false;

      if ( !authDbQuery( &query ) )
        return false;

      if ( !authDbCommit() )
        return false;

      QgsDebugMsg( QString( "Reencrypt SUCCESS for setting: %2" ).arg( sett ) );
      return true;
    }
    else
    {
      QgsDebugMsg( QString( "Reencrypt FAILED, could not find in db setting: %2" ).arg( sett ) );
      return false;
    }

    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for setting: %1" ).arg( sett ) );
      emit messageOut( tr( "Authentication database contains duplicate setting keys" ), authManTag(), WARNING );
    }

    return false;
  }

  return true;
#endif
}

bool QgsAuthManager::reencryptAllAuthenticationIdentities( const QString &prevpass, const QString &prevciv )
{
  if ( isDisabled() )
    return false;

  bool res = true;
  Q_FOREACH ( const QString &identid, getCertIdentityIds() )
  {
    res = res && reencryptAuthenticationIdentity( identid, prevpass, prevciv );
  }
  return res;
}

bool QgsAuthManager::reencryptAuthenticationIdentity(
  const QString &identid,
  const QString &prevpass,
  const QString &prevciv )
{
  if ( isDisabled() )
    return false;

  // no need to check for setMasterPassword, since this is private and it will be set

  QSqlQuery query( authDatabaseConnection() );

  query.prepare( QString( "SELECT key FROM %1 "
                          "WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

  query.bindValue( QStringLiteral( ":id" ), identid );

  if ( !authDbQuery( &query ) )
    return false;

  if ( !query.isActive() || !query.isSelect() )
  {
    QgsDebugMsg( QString( "Reencrypt FAILED, query not active or a select operation for identity id: %2" ).arg( identid ) );
    return false;
  }

  if ( query.first() )
  {
    QString keystring( QgsAuthCrypto::decrypt( prevpass, prevciv, query.value( 0 ).toString() ) );

    if ( query.next() )
    {
      QgsDebugMsg( QString( "Select contains more than one for identity id: %1" ).arg( identid ) );
      emit messageOut( tr( "Authentication database contains duplicate identity IDs" ), authManTag(), WARNING );
      return false;
    }

    query.clear();

    query.prepare( QString( "UPDATE %1 "
                            "SET key = :key "
                            "WHERE id = :id" ).arg( authDbIdentitiesTable() ) );

    query.bindValue( QStringLiteral( ":id" ), identid );
    query.bindValue( QStringLiteral( ":key" ), QgsAuthCrypto::encrypt( mMasterPass, masterPasswordCiv(), keystring ) );

    if ( !authDbStartTransaction() )
      return false;

    if ( !authDbQuery( &query ) )
      return false;

    if ( !authDbCommit() )
      return false;

    QgsDebugMsg( QString( "Reencrypt SUCCESS for identity id: %2" ).arg( identid ) );
    return true;
  }
  else
  {
    QgsDebugMsg( QString( "Reencrypt FAILED, could not find in db identity id: %2" ).arg( identid ) );
    return false;
  }
}

bool QgsAuthManager::authDbOpen() const
{
  if ( isDisabled() )
    return false;

  QSqlDatabase authdb = authDatabaseConnection();
  if ( !authdb.isOpen() )
  {
    if ( !authdb.open() )
    {
      QgsDebugMsg( QString( "Unable to establish database connection\nDatabase: %1\nDriver error: %2\nDatabase error: %3" )
                   .arg( authenticationDatabasePath(),
                         authdb.lastError().driverText(),
                         authdb.lastError().databaseText() ) );
      emit messageOut( tr( "Unable to establish authentication database connection" ), authManTag(), CRITICAL );
      return false;
    }
  }
  return true;
}

bool QgsAuthManager::authDbQuery( QSqlQuery *query ) const
{
  if ( isDisabled() )
    return false;

  query->setForwardOnly( true );
  if ( !query->exec() )
  {
    const char *err = QT_TR_NOOP( "Auth db query exec() FAILED" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  if ( query->lastError().isValid() )
  {
    QgsDebugMsg( QString( "Auth db query FAILED: %1\nError: %2" )
                 .arg( query->executedQuery(),
                       query->lastError().text() ) );
    emit messageOut( tr( "Auth db query FAILED" ), authManTag(), WARNING );
    return false;
  }

  return true;
}

bool QgsAuthManager::authDbStartTransaction() const
{
  if ( isDisabled() )
    return false;

  if ( !authDatabaseConnection().transaction() )
  {
    const char *err = QT_TR_NOOP( "Auth db FAILED to start transaction" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  return true;
}

bool QgsAuthManager::authDbCommit() const
{
  if ( isDisabled() )
    return false;

  if ( !authDatabaseConnection().commit() )
  {
    const char *err = QT_TR_NOOP( "Auth db FAILED to rollback changes" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    ( void )authDatabaseConnection().rollback();
    return false;
  }

  return true;
}

bool QgsAuthManager::authDbTransactionQuery( QSqlQuery *query ) const
{
  if ( isDisabled() )
    return false;

  if ( !authDatabaseConnection().transaction() )
  {
    const char *err = QT_TR_NOOP( "Auth db FAILED to start transaction" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    return false;
  }

  bool ok = authDbQuery( query );

  if ( ok && !authDatabaseConnection().commit() )
  {
    const char *err = QT_TR_NOOP( "Auth db FAILED to rollback changes" );
    QgsDebugMsg( err );
    emit messageOut( tr( err ), authManTag(), WARNING );
    ( void )authDatabaseConnection().rollback();
    return false;
  }

  return ok;
}

void QgsAuthManager::insertCaCertInCache( QgsAuthCertUtils::CaCertSource source, const QList<QSslCertificate> &certs )
{
  Q_FOREACH ( const QSslCertificate &cert, certs )
  {
    mCaCertsCache.insert( QgsAuthCertUtils::shaHexForCert( cert ),
                          QPair<QgsAuthCertUtils::CaCertSource, QSslCertificate>( source, cert ) );
  }
}

