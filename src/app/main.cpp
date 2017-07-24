/***************************************************************************
                            main.cpp  -  description
                              -------------------
              begin                : Fri Jun 21 10:48:28 AKDT 2002
              copyright            : (C) 2002 by Gary E.Sherman
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


//qt includes
#include <QBitmap>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QFontDatabase>
#include <QPixmap>
#include <QLocale>
#include <QSplashScreen>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QStyleFactory>
#include <QDesktopWidget>
#include <QTranslator>
#include <QImageReader>
#include <QMessageBox>

#include <cstdio>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef WIN32
// Open files in binary mode
#include <fcntl.h> /*  _O_BINARY */
#include <windows.h>
#include <dbghelp.h>
#include <time.h>
#ifdef MSVC
#undef _fmode
int _fmode = _O_BINARY;
#else
// Only do this if we are not building on windows with msvc.
// Recommended method for doing this with msvc is with a call to _set_fmode
// which is the first thing we do in main().
// Similarly, with MinGW set _fmode in main().
#endif  //_MSC_VER
#else
#include <getopt.h>
#endif

#ifdef Q_OS_MACX
#include <ApplicationServices/ApplicationServices.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED < 1050
typedef SInt32 SRefCon;
#endif
// For setting the maximum open files limit higher
#include <sys/resource.h>
#include <limits.h>
#endif

#if ((defined(linux) || defined(__linux__)) && !defined(ANDROID)) || defined(__FreeBSD__)
#include <unistd.h>
#include <execinfo.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h>
#endif

#include "qgscustomization.h"
#include "qgssettings.h"
#include "qgsfontutils.h"
#include "qgspluginregistry.h"
#include "qgsmessagelog.h"
#include "qgspythonrunner.h"
#include "qgslocalec.h"
#include "qgisapp.h"
#include "qgsmapcanvas.h"
#include "qgsapplication.h"
#include "qgsconfig.h"
#include "qgsversion.h"
#include "qgsexception.h"
#include "qgsproject.h"
#include "qgsrectangle.h"
#include "qgslogger.h"
#include "qgsdxfexport.h"
#include "qgsmapthemes.h"
#include "qgsvectorlayer.h"
#include "qgis_app.h"
#include "qgscrashhandler.h"

#include "qgsuserprofilemanager.h"
#include "qgsuserprofile.h"

/** Print usage text
 */
void usage( const QString &appName )
{
  QStringList msg;

  msg
      << QStringLiteral( "QGIS - " ) << VERSION << QStringLiteral( " '" ) << RELEASE_NAME << QStringLiteral( "' (" )
      << QGSVERSION << QStringLiteral( ")\n" )
      << QStringLiteral( "QGIS is a user friendly Open Source Geographic Information System.\n" )
      << QStringLiteral( "Usage: " ) << appName <<  QStringLiteral( " [OPTION] [FILE]\n" )
      << QStringLiteral( "  OPTION:\n" )
      << QStringLiteral( "\t[--snapshot filename]\temit snapshot of loaded datasets to given file\n" )
      << QStringLiteral( "\t[--width width]\twidth of snapshot to emit\n" )
      << QStringLiteral( "\t[--height height]\theight of snapshot to emit\n" )
      << QStringLiteral( "\t[--lang language]\tuse language for interface text\n" )
      << QStringLiteral( "\t[--project projectfile]\tload the given QGIS project\n" )
      << QStringLiteral( "\t[--extent xmin,ymin,xmax,ymax]\tset initial map extent\n" )
      << QStringLiteral( "\t[--nologo]\thide splash screen\n" )
      << QStringLiteral( "\t[--noversioncheck]\tdon't check for new version of QGIS at startup\n" )
      << QStringLiteral( "\t[--noplugins]\tdon't restore plugins on startup\n" )
      << QStringLiteral( "\t[--nocustomization]\tdon't apply GUI customization\n" )
      << QStringLiteral( "\t[--customizationfile path]\tuse the given ini file as GUI customization\n" )
      << QStringLiteral( "\t[--globalsettingsfile path]\tuse the given ini file as Global Settings (defaults)\n" )
      << QStringLiteral( "\t[--authdbdirectory path] use the given directory for authentication database\n" )
      << QStringLiteral( "\t[--code path]\trun the given python file on load\n" )
      << QStringLiteral( "\t[--defaultui]\tstart by resetting user ui settings to default\n" )
      << QStringLiteral( "\t[--dxf-export filename.dxf]\temit dxf output of loaded datasets to given file\n" )
      << QStringLiteral( "\t[--dxf-extent xmin,ymin,xmax,ymax]\tset extent to export to dxf\n" )
      << QStringLiteral( "\t[--dxf-symbology-mode none|symbollayer|feature]\tsymbology mode for dxf output\n" )
      << QStringLiteral( "\t[--dxf-scale-denom scale]\tscale for dxf output\n" )
      << QStringLiteral( "\t[--dxf-encoding encoding]\tencoding to use for dxf output\n" )
      << QStringLiteral( "\t[--dxf-preset maptheme]\tmap theme to use for dxf output\n" )
      << QStringLiteral( "\t[--profile name]\tload a named profile from the users profiles folder.\n" )
      << QStringLiteral( "\t[--profiles-path path]\tpath to store user profile folders. Will create profiles inside a {path}\\profiles folder \n" )
      << QStringLiteral( "\t[--help]\t\tthis text\n" )
      << QStringLiteral( "\t[--]\t\ttreat all following arguments as FILEs\n\n" )
      << QStringLiteral( "  FILE:\n" )
      << QStringLiteral( "    Files specified on the command line can include rasters,\n" )
      << QStringLiteral( "    vectors, and QGIS project files (.qgs): \n" )
      << QStringLiteral( "     1. Rasters - supported formats include GeoTiff, DEM \n" )
      << QStringLiteral( "        and others supported by GDAL\n" )
      << QStringLiteral( "     2. Vectors - supported formats include ESRI Shapefiles\n" )
      << QStringLiteral( "        and others supported by OGR and PostgreSQL layers using\n" )
      << QStringLiteral( "        the PostGIS extension\n" )  ; // OK

#ifdef Q_OS_WIN
  MessageBox( nullptr,
              msg.join( QString() ).toLocal8Bit().constData(),
              "QGIS command line options",
              MB_OK );
#else
  std::cerr << msg.join( QString() ).toLocal8Bit().constData();
#endif

} // usage()


/////////////////////////////////////////////////////////////////
// Command line options 'behavior' flag setup
////////////////////////////////////////////////////////////////

// These two are global so that they can be set by the OpenDocuments
// AppleEvent handler as well as by the main routine argv processing

// This behavior will cause QGIS to autoload a project
static QString sProjectFileName = QLatin1String( "" );

// This is the 'leftover' arguments collection
static QStringList sFileList;


/* Test to determine if this program was started on Mac OS X by double-clicking
 * the application bundle rather then from a command line. If clicked, argv[1]
 * contains a process serial number in the form -psn_0_1234567. Don't process
 * the command line arguments in this case because argv[1] confuses the processing.
 */
bool bundleclicked( int argc, char *argv[] )
{
  return ( argc > 1 && memcmp( argv[1], "-psn_", 5 ) == 0 );
}

void myPrint( const char *fmt, ... )
{
  va_list ap;
  va_start( ap, fmt );
#if defined(Q_OS_WIN)
  char buffer[1024];
  vsnprintf( buffer, sizeof buffer, fmt, ap );
  OutputDebugString( buffer );
#else
  vfprintf( stderr, fmt, ap );
#endif
  va_end( ap );
}

static void dumpBacktrace( unsigned int depth )
{
  if ( depth == 0 )
    depth = 20;

#if ((defined(linux) || defined(__linux__)) && !defined(ANDROID)) || defined(__FreeBSD__)
  // Below there is a bunch of operations that are not safe in multi-threaded
  // environment (dup()+close() combo, wait(), juggling with file descriptors).
  // Maybe some problems could be resolved with dup2() and waitpid(), but it seems
  // that if the operations on descriptors are not serialized, things will get nasty.
  // That's why there's this lovely mutex here...
  static QMutex sMutex;
  QMutexLocker locker( &sMutex );

  int stderr_fd = -1;
  if ( access( "/usr/bin/c++filt", X_OK ) < 0 )
  {
    myPrint( "Stacktrace (c++filt NOT FOUND):\n" );
  }
  else
  {
    int fd[2];

    if ( pipe( fd ) == 0 && fork() == 0 )
    {
      close( STDIN_FILENO ); // close stdin

      // stdin from pipe
      if ( dup( fd[0] ) != STDIN_FILENO )
      {
        QgsDebugMsg( "dup to stdin failed" );
      }

      close( fd[1] );        // close writing end
      execl( "/usr/bin/c++filt", "c++filt", static_cast< char * >( nullptr ) );
      perror( "could not start c++filt" );
      exit( 1 );
    }

    myPrint( "Stacktrace (piped through c++filt):\n" );
    stderr_fd = dup( STDERR_FILENO );
    close( fd[0] );          // close reading end
    close( STDERR_FILENO );  // close stderr

    // stderr to pipe
    int stderr_new = dup( fd[1] );
    if ( stderr_new != STDERR_FILENO )
    {
      if ( stderr_new >= 0 )
        close( stderr_new );
      QgsDebugMsg( "dup to stderr failed" );
    }

    close( fd[1] );  // close duped pipe
  }

  void **buffer = new void *[ depth ];
  int nptrs = backtrace( buffer, depth );
  backtrace_symbols_fd( buffer, nptrs, STDERR_FILENO );
  delete [] buffer;
  if ( stderr_fd >= 0 )
  {
    int status;
    close( STDERR_FILENO );
    int dup_stderr = dup( stderr_fd );
    if ( dup_stderr != STDERR_FILENO )
    {
      close( dup_stderr );
      QgsDebugMsg( "dup to stderr failed" );
    }
    close( stderr_fd );
    wait( &status );
  }
#elif defined(Q_OS_WIN)
  // TODO Replace with incoming QgsStackTrace
#else
  Q_UNUSED( depth );
#endif
}

#if (defined(linux) && !defined(ANDROID)) || defined(__FreeBSD__)
void qgisCrash( int signal )
{
  fprintf( stderr, "QGIS died on signal %d", signal );

  if ( access( "/usr/bin/gdb", X_OK ) == 0 )
  {
    // take full stacktrace using gdb
    // http://stackoverflow.com/questions/3151779/how-its-better-to-invoke-gdb-from-program-to-print-its-stacktrace
    // unfortunately, this is not so simple. the proper method is way more OS-specific
    // than this code would suggest, see http://stackoverflow.com/a/1024937

    char exename[512];
#if defined(__FreeBSD__)
    int len = readlink( "/proc/curproc/file", exename, sizeof( exename ) - 1 );
#else
    int len = readlink( "/proc/self/exe", exename, sizeof( exename ) - 1 );
#endif
    if ( len < 0 )
    {
      myPrint( "Could not read link (%d:%s)\n", errno, strerror( errno ) );
    }
    else
    {
      exename[ len ] = 0;

      char pidstr[32];
      snprintf( pidstr, sizeof pidstr, "--pid=%d", getpid() );

      int gdbpid = fork();
      if ( gdbpid == 0 )
      {
        // attach, backtrace and continue
        execl( "/usr/bin/gdb", "gdb", "-q", "-batch", "-n", pidstr, "-ex", "thread", "-ex", "bt full", exename, NULL );
        perror( "cannot exec gdb" );
        exit( 1 );
      }
      else if ( gdbpid >= 0 )
      {
        int status;
        waitpid( gdbpid, &status, 0 );
        myPrint( "gdb returned %d\n", status );
      }
      else
      {
        myPrint( "Cannot fork (%d:%s)\n", errno, strerror( errno ) );
        dumpBacktrace( 256 );
      }
    }
  }

  abort();
}
#endif

/*
 * Hook into the qWarning/qFatal mechanism so that we can channel messages
 * from libpng to the user.
 *
 * Some JPL WMS images tend to overload the libpng 1.2.2 implementation
 * somehow (especially when zoomed in)
 * and it would be useful for the user to know why their picture turned up blank
 *
 * Based on qInstallMsgHandler example code in the Qt documentation.
 *
 */
void myMessageOutput( QtMsgType type, const char *msg )
{
  switch ( type )
  {
    case QtDebugMsg:
      myPrint( "%s\n", msg );
      if ( strncmp( msg, "Backtrace", 9 ) == 0 )
        dumpBacktrace( atoi( msg + 9 ) );
      break;
    case QtCriticalMsg:
      myPrint( "Critical: %s\n", msg );
      break;
    case QtWarningMsg:
      myPrint( "Warning: %s\n", msg );

#ifdef QGISDEBUG
      // Print all warnings except setNamedColor.
      // Only seems to happen on windows
      if ( 0 != strncmp( msg, "QColor::setNamedColor: Unknown color name 'param", 48 ) )
      {
        // TODO: Verify this code in action.
        dumpBacktrace( 20 );
        QgsMessageLog::logMessage( msg, QStringLiteral( "Qt" ) );
      }
#endif

      // TODO: Verify this code in action.
      if ( 0 == strncmp( msg, "libpng error:", 13 ) )
      {
        // Let the user know
        QgsMessageLog::logMessage( msg, QStringLiteral( "libpng" ) );
      }

      break;
    case QtFatalMsg:
    {
      myPrint( "Fatal: %s\n", msg );
#if (defined(linux) && !defined(ANDROID)) || defined(__FreeBSD__)
      qgisCrash( -1 );
#else
      dumpBacktrace( 256 );
      abort();                    // deliberately dump core
#endif
      break; // silence warnings
    }

#if QT_VERSION >= 0x050500
    case QtInfoMsg:
      myPrint( "Info: %s\n", msg );
      break;
#endif
  }
}

#ifdef _MSC_VER
#undef APP_EXPORT
#define APP_EXPORT __declspec(dllexport)
#endif

#if defined(ANDROID) || defined(Q_OS_WIN)
// On Android, there there is a libqgis.so instead of a qgis executable.
// The main method symbol of this library needs to be exported so it can be called by java
// On Windows this main is included in qgis_app and called from mainwin.cpp
APP_EXPORT
#endif
int main( int argc, char *argv[] )
{
#ifdef Q_OS_MACX
  // Increase file resource limits (i.e., number of allowed open files)
  // (from code provided by Larry Biehl, Purdue University, USA, from 'MultiSpec' project)
  // This is generally 256 for the soft limit on Mac
  // NOTE: setrlimit() must come *before* initialization of stdio strings,
  //       e.g. before any debug messages, or setrlimit() gets ignored
  // see: http://stackoverflow.com/a/17726104/2865523
  struct rlimit rescLimit;
  if ( getrlimit( RLIMIT_NOFILE, &rescLimit ) == 0 )
  {
    rlim_t oldSoft( rescLimit.rlim_cur );
    rlim_t oldHard( rescLimit.rlim_max );
#ifdef OPEN_MAX
    rlim_t newSoft( OPEN_MAX );
    rlim_t newHard( std::min( oldHard, newSoft ) );
#else
    rlim_t newSoft( 4096 );
    rlim_t newHard( std::min( ( rlim_t )8192, oldHard ) );
#endif
    if ( rescLimit.rlim_cur < newSoft )
    {
      rescLimit.rlim_cur = newSoft;
      rescLimit.rlim_max = newHard;

      if ( setrlimit( RLIMIT_NOFILE, &rescLimit ) == 0 )
      {
        QgsDebugMsg( QString( "Mac RLIMIT_NOFILE Soft/Hard NEW: %1 / %2" )
                     .arg( rescLimit.rlim_cur ).arg( rescLimit.rlim_max ) );
      }
    }
    Q_UNUSED( oldSoft ); //avoid warnings
    QgsDebugMsg( QString( "Mac RLIMIT_NOFILE Soft/Hard ORIG: %1 / %2" )
                 .arg( oldSoft ).arg( oldHard ) );
  }
#endif

  QgsDebugMsg( QString( "Starting qgis main" ) );
#ifdef WIN32  // Windows
#ifdef _MSC_VER
  _set_fmode( _O_BINARY );
#else //MinGW
  _fmode = _O_BINARY;
#endif  // _MSC_VER
#endif  // WIN32

  // Set up the custom qWarning/qDebug custom handler
#ifndef ANDROID
  qInstallMsgHandler( myMessageOutput );
#endif

#if (defined(linux) && !defined(ANDROID)) || defined(__FreeBSD__)
  signal( SIGQUIT, qgisCrash );
  signal( SIGILL, qgisCrash );
  signal( SIGFPE, qgisCrash );
  signal( SIGSEGV, qgisCrash );
  signal( SIGBUS, qgisCrash );
  signal( SIGSYS, qgisCrash );
  signal( SIGTRAP, qgisCrash );
  signal( SIGXCPU, qgisCrash );
  signal( SIGXFSZ, qgisCrash );
#endif

#ifdef Q_OS_WIN
  SetUnhandledExceptionFilter( QgsCrashHandler::handle );
#endif

  // initialize random number seed
  qsrand( time( nullptr ) );

  /////////////////////////////////////////////////////////////////
  // Command line options 'behavior' flag setup
  ////////////////////////////////////////////////////////////////

  //
  // Parse the command line arguments, looking to see if the user has asked for any
  // special behaviors. Any remaining non command arguments will be kept aside to
  // be passed as a list of layers and / or a project that should be loaded.
  //

  // This behavior is used to load the app, snapshot the map,
  // save the image to disk and then exit
  QString mySnapshotFileName = "";
  QString configLocalStorageLocation =  "";
  QString profileName;
  int mySnapshotWidth = 800;
  int mySnapshotHeight = 600;

  bool myHideSplash = false;
  bool mySkipVersionCheck = false;
#if defined(ANDROID)
  QgsDebugMsg( QString( "Android: Splash hidden" ) );
  myHideSplash = true;
#endif

  bool myRestoreDefaultWindowState = false;
  bool myRestorePlugins = true;
  bool myCustomization = true;

  QString dxfOutputFile;
  QgsDxfExport::SymbologyExport dxfSymbologyMode = QgsDxfExport::SymbolLayerSymbology;
  double dxfScale = 50000.0;
  QString dxfEncoding = QStringLiteral( "CP1252" );
  QString dxfPreset;
  QgsRectangle dxfExtent;

  // This behavior will set initial extent of map canvas, but only if
  // there are no command line arguments. This gives a usable map
  // extent when qgis starts with no layers loaded. When layers are
  // loaded, we let the layers define the initial extent.
  QString myInitialExtent = QLatin1String( "" );
  if ( argc == 1 )
    myInitialExtent = QStringLiteral( "-1,-1,1,1" );

  // This behavior will allow you to force the use of a translation file
  // which is useful for testing
  QString myTranslationCode;

  // The user can specify a path which will override the default path of custom
  // user settings (~/.qgis) and it will be used for QgsSettings INI file
  QString configpath;
  QString authdbdirectory;

  QString pythonfile;

  QString customizationfile;
  QString globalsettingsfile;

// TODO Fix android
#if defined(ANDROID)
  QgsDebugMsg( QString( "Android: All params stripped" ) );// Param %1" ).arg( argv[0] ) );
  //put all QGIS settings in the same place
  configpath = QgsApplication::qgisSettingsDirPath();
  QgsDebugMsg( QString( "Android: configpath set to %1" ).arg( configpath ) );
#endif

  QStringList args;

  if ( !bundleclicked( argc, argv ) )
  {
    // Build a local QCoreApplication from arguments. This way, arguments are correctly parsed from their native locale
    // It will use QString::fromLocal8Bit( argv ) under Unix and GetCommandLine() under Windows.
    QCoreApplication coreApp( argc, argv );
    args = QCoreApplication::arguments();

    for ( int i = 1; i < args.size(); ++i )
    {
      const QString &arg = args[i];

      if ( arg == QLatin1String( "--help" ) || arg == QLatin1String( "-?" ) )
      {
        usage( args[0] );
        return 2;
      }
      else if ( arg == QLatin1String( "--nologo" ) || arg == QLatin1String( "-n" ) )
      {
        myHideSplash = true;
      }
      else if ( arg == QLatin1String( "--noversioncheck" ) || arg == QLatin1String( "-V" ) )
      {
        mySkipVersionCheck = true;
      }
      else if ( arg == QLatin1String( "--noplugins" ) || arg == QLatin1String( "-P" ) )
      {
        myRestorePlugins = false;
      }
      else if ( arg == QLatin1String( "--nocustomization" ) || arg == QLatin1String( "-C" ) )
      {
        myCustomization = false;
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--profile" ) ) )
      {
        profileName = args[++i];
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--profiles-path" ) || arg == QLatin1String( "-s" ) ) )
      {
        configLocalStorageLocation = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--snapshot" ) || arg == QLatin1String( "-s" ) ) )
      {
        mySnapshotFileName = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--width" ) || arg == QLatin1String( "-w" ) ) )
      {
        mySnapshotWidth = QString( args[++i] ).toInt();
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--height" ) || arg == QLatin1String( "-h" ) ) )
      {
        mySnapshotHeight = QString( args[++i] ).toInt();
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--lang" ) || arg == QLatin1String( "-l" ) ) )
      {
        myTranslationCode = args[++i];
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--project" ) || arg == QLatin1String( "-p" ) ) )
      {
        sProjectFileName = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--extent" ) || arg == QLatin1String( "-e" ) ) )
      {
        myInitialExtent = args[++i];
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--authdbdirectory" ) || arg == QLatin1String( "-a" ) ) )
      {
        authdbdirectory = QDir::toNativeSeparators( QDir( args[++i] ).absolutePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--code" ) || arg == QLatin1String( "-f" ) ) )
      {
        pythonfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--customizationfile" ) || arg == QLatin1String( "-z" ) ) )
      {
        customizationfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( i + 1 < argc && ( arg == QLatin1String( "--globalsettingsfile" ) || arg == QLatin1String( "-g" ) ) )
      {
        globalsettingsfile = QDir::toNativeSeparators( QFileInfo( args[++i] ).absoluteFilePath() );
      }
      else if ( arg == QLatin1String( "--defaultui" ) || arg == QLatin1String( "-d" ) )
      {
        myRestoreDefaultWindowState = true;
      }
      else if ( arg == QLatin1String( "--dxf-export" ) )
      {
        dxfOutputFile = args[++i];
      }
      else if ( arg == QLatin1String( "--dxf-extent" ) )
      {
        QgsLocaleNumC l;
        QString ext( args[++i] );
        QStringList coords( ext.split( ',' ) );

        if ( coords.size() != 4 )
        {
          std::cerr << "invalid dxf extent " << ext.toStdString() << std::endl;
          return 2;
        }

        for ( int i = 0; i < 4; i++ )
        {
          bool ok;
          double d;

          d = coords[i].toDouble( &ok );
          if ( !ok )
          {
            std::cerr << "invalid dxf coordinate " << coords[i].toStdString() << " in extent " << ext.toStdString() << std::endl;
            return 2;
          }

          switch ( i )
          {
            case 0:
              dxfExtent.setXMinimum( d );
              break;
            case 1:
              dxfExtent.setYMinimum( d );
              break;
            case 2:
              dxfExtent.setXMaximum( d );
              break;
            case 3:
              dxfExtent.setYMaximum( d );
              break;
          }
        }
      }
      else if ( arg == QLatin1String( "--dxf-symbology-mode" ) )
      {
        QString mode( args[++i] );
        if ( mode == QLatin1String( "none" ) )
        {
          dxfSymbologyMode = QgsDxfExport::NoSymbology;
        }
        else if ( mode == QLatin1String( "symbollayer" ) )
        {
          dxfSymbologyMode = QgsDxfExport::SymbolLayerSymbology;
        }
        else if ( mode == QLatin1String( "feature" ) )
        {
          dxfSymbologyMode = QgsDxfExport::FeatureSymbology;
        }
        else
        {
          std::cerr << "invalid dxf symbology mode " << mode.toStdString() << std::endl;
          return 2;
        }
      }
      else if ( arg == QLatin1String( "--dxf-scale-denom" ) )
      {
        bool ok;
        QString scale( args[++i] );
        dxfScale = scale.toDouble( &ok );
        if ( !ok )
        {
          std::cerr << "invalid dxf scale " << scale.toStdString() << std::endl;
          return 2;
        }
      }
      else if ( arg == QLatin1String( "--dxf-encoding" ) )
      {
        dxfEncoding = args[++i];
      }
      else if ( arg == QLatin1String( "--dxf-preset" ) )
      {
        dxfPreset = args[++i];
      }
      else if ( arg == QLatin1String( "--" ) )
      {
        for ( i++; i < args.size(); ++i )
          sFileList.append( QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() ) );
      }
      else
      {
        sFileList.append( QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() ) );
      }
    }
  }

  /////////////////////////////////////////////////////////////////////
  // If no --project was specified, parse the args to look for a     //
  // .qgs file and set myProjectFileName to it. This allows loading  //
  // of a project file by clicking on it in various desktop managers //
  // where an appropriate mime-type has been set up.                 //
  /////////////////////////////////////////////////////////////////////
  if ( sProjectFileName.isEmpty() )
  {
    // check for a .qgs
    for ( int i = 0; i < args.size(); i++ )
    {
      QString arg = QDir::toNativeSeparators( QFileInfo( args[i] ).absoluteFilePath() );
      if ( arg.endsWith( QLatin1String( ".qgs" ), Qt::CaseInsensitive ) )
      {
        sProjectFileName = arg;
        break;
      }
    }
  }


  /////////////////////////////////////////////////////////////////////
  // Now we have the handlers for the different behaviors...
  ////////////////////////////////////////////////////////////////////

  /////////////////////////////////////////////////////////////////////
  // Initialize the application and the translation stuff
  /////////////////////////////////////////////////////////////////////

#if defined(Q_OS_UNIX) && !defined(Q_OS_MAC) && !defined(ANDROID)
  bool myUseGuiFlag = nullptr != getenv( "DISPLAY" );
#else
  bool myUseGuiFlag = true;
#endif
  if ( !myUseGuiFlag )
  {
    std::cerr << QObject::tr(
                "QGIS starting in non-interactive mode not supported.\n"
                "You are seeing this message most likely because you "
                "have no DISPLAY environment variable set.\n"
              ).toUtf8().constData();
    exit( 1 ); //exit for now until a version of qgis is capabable of running non interactive
  }

  // GUI customization is enabled according to settings (loaded when instance is created)
  // we force disabled here if --nocustomization argument is used
  if ( !myCustomization )
  {
    QgsCustomization::instance()->setEnabled( false );
  }

  QCoreApplication::setOrganizationName( QgsApplication::QGIS_ORGANIZATION_NAME );
  QCoreApplication::setOrganizationDomain( QgsApplication::QGIS_ORGANIZATION_DOMAIN );
  QCoreApplication::setApplicationName( QgsApplication::QGIS_APPLICATION_NAME );
  QCoreApplication::setAttribute( Qt::AA_DontShowIconsInMenus, false );


  // SetUp the QgsSettings Global Settings:
  // - use the path specified with --globalsettings path,
  // - use the environment if not found
  // - use a default location as a fallback
  if ( globalsettingsfile.isEmpty() )
  {
    globalsettingsfile = getenv( "QGIS_GLOBAL_SETTINGS_FILE" );
  }
  if ( globalsettingsfile.isEmpty() )
  {
    QString default_globalsettingsfile = QgsApplication::pkgDataPath() + "/qgis_global_settings.ini";
    if ( QFile::exists( default_globalsettingsfile ) )
    {
      globalsettingsfile = default_globalsettingsfile;
    }
  }
  if ( !globalsettingsfile.isEmpty() )
  {
    if ( ! QgsSettings::setGlobalSettingsPath( globalsettingsfile ) )
    {
      QgsMessageLog::logMessage( QString( "Invalid globalsettingsfile path: %1" ).arg( globalsettingsfile ), QStringLiteral( "QGIS" ) );
    }
    else
    {
      QgsMessageLog::logMessage( QString( "Successfully loaded globalsettingsfile path: %1" ).arg( globalsettingsfile ), QStringLiteral( "QGIS" ) );
    }
  }

  QgsSettings settings;
  if ( configLocalStorageLocation.isEmpty() )
  {
    if ( getenv( "QGIS_CUSTOM_CONFIG_PATH" ) )
    {
      configLocalStorageLocation = getenv( "QGIS_CUSTOM_CONFIG_PATH" );
    }
    else if ( settings.contains( "profilesPath", QgsSettings::Core ) )
    {
      configLocalStorageLocation = settings.value( "profilesPath", "", QgsSettings::Core ).toString();
      QgsDebugMsg( QString( "Loading profiles path from global config at %1" ).arg( configLocalStorageLocation ) );
    }

    // If it is still empty at this point we get it from the standard location.
    if ( configLocalStorageLocation.isEmpty() )
    {
      configLocalStorageLocation = QStandardPaths::standardLocations( QStandardPaths::AppDataLocation ).value( 0 );
    }
  }

  QString rootProfileFolder = QgsUserProfileManager::resolveProfilesFolder( configLocalStorageLocation );
  QgsUserProfileManager manager( rootProfileFolder );
  QgsUserProfile *profile = manager.getProfile( profileName, true );
  QString profileFolder = profile->folder();
  profileName = profile->name();
  delete profile;

  QgsDebugMsg( "User profile details:" );
  QgsDebugMsg( QString( "\t - %1" ).arg( profileName ) );
  QgsDebugMsg( QString( "\t - %1" ).arg( profileFolder ) );
  QgsDebugMsg( QString( "\t - %1" ).arg( rootProfileFolder ) );

  QgsApplication myApp( argc, argv, myUseGuiFlag, profileFolder );

#ifdef Q_OS_MAC
  // Set hidpi icons; use SVG icons, as PNGs will be relatively too small
  QCoreApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );

  // Set 1024x1024 icon for dock, app switcher, etc., rendering
  myApp.setWindowIcon( QIcon( QgsApplication::iconsPath() + QStringLiteral( "qgis-icon-macos.png" ) ) );
#else
  myApp.setWindowIcon( QIcon( QgsApplication::appIconPath() ) );
#endif

#ifdef Q_OS_WIN
  if ( !QgsApplication::isRunningFromBuildDir() )
  {
    QString symbolPath( getenv( "QGIS_PREFIX_PATH" ) );
    symbolPath = symbolPath + "\\pdb;http://msdl.microsoft.com/download/symbols;http://download.osgeo.org/osgeo4w/symstore";
    QgsStackTrace::setSymbolPath( symbolPath );
  }
  else
  {
    QString symbolPath( getenv( "QGIS_PDB_PATH" ) );
    symbolPath = symbolPath + ";http://msdl.microsoft.com/download/symbols;http://download.osgeo.org/osgeo4w/symstore";
    QgsStackTrace::setSymbolPath( symbolPath );
  }
#endif

  // TODO: use QgsSettings
  QSettings *customizationsettings = nullptr;

  // Using the customizationfile option always overrides the option and config path options.
  if ( !customizationfile.isEmpty() )
  {
    customizationsettings = new QSettings( customizationfile, QSettings::IniFormat );
    QgsCustomization::instance()->setEnabled( true );
  }
  else
  {
    customizationsettings = new QSettings( QStringLiteral( "QGIS" ), QStringLiteral( "QGISCUSTOMIZATION2" ) );
  }

  // Load and set possible default customization, must be done after QgsApplication init and QgsSettings ( QCoreApplication ) init
  QgsCustomization::instance()->setSettings( customizationsettings );
  QgsCustomization::instance()->loadDefault();

#ifdef Q_OS_MACX
  // If the GDAL plugins are bundled with the application and GDAL_DRIVER_PATH
  // is not already defined, use the GDAL plugins in the application bundle.
  QString gdalPlugins( QCoreApplication::applicationDirPath().append( "/lib/gdalplugins" ) );
  if ( QFile::exists( gdalPlugins ) && !getenv( "GDAL_DRIVER_PATH" ) )
  {
    setenv( "GDAL_DRIVER_PATH", gdalPlugins.toUtf8(), 1 );
  }

  // Point GDAL_DATA at any GDAL share directory embedded in the app bundle
  if ( !getenv( "GDAL_DATA" ) )
  {
    QStringList gdalShares;
    QString appResources( QDir::cleanPath( QgsApplication::pkgDataPath() ) );
    gdalShares << QCoreApplication::applicationDirPath().append( "/share/gdal" )
               << appResources.append( "/share/gdal" )
               << appResources.append( "/gdal" );
    Q_FOREACH ( const QString &gdalShare, gdalShares )
    {
      if ( QFile::exists( gdalShare ) )
      {
        setenv( "GDAL_DATA", gdalShare.toUtf8().constData(), 1 );
        break;
      }
    }
  }
#endif


  QgsSettings mySettings;

  // update any saved setting for older themes to new default 'gis' theme (2013-04-15)
  if ( mySettings.contains( QStringLiteral( "/Themes" ) ) )
  {
    QString theme = mySettings.value( QStringLiteral( "Themes" ), "default" ).toString();
    if ( theme == QLatin1String( "gis" )
         || theme == QLatin1String( "classic" )
         || theme == QLatin1String( "nkids" ) )
    {
      mySettings.setValue( QStringLiteral( "Themes" ), QStringLiteral( "default" ) );
    }
  }

  // custom environment variables
  QMap<QString, QString> systemEnvVars = QgsApplication::systemEnvVars();
  bool useCustomVars = mySettings.value( QStringLiteral( "qgis/customEnvVarsUse" ), QVariant( false ) ).toBool();
  if ( useCustomVars )
  {
    QStringList customVarsList = mySettings.value( QStringLiteral( "qgis/customEnvVars" ), "" ).toStringList();
    if ( !customVarsList.isEmpty() )
    {
      Q_FOREACH ( const QString &varStr, customVarsList )
      {
        int pos = varStr.indexOf( QLatin1Char( '|' ) );
        if ( pos == -1 )
          continue;
        QString envVarApply = varStr.left( pos );
        QString varStrNameValue = varStr.mid( pos + 1 );
        pos = varStrNameValue.indexOf( QLatin1Char( '=' ) );
        if ( pos == -1 )
          continue;
        QString envVarName = varStrNameValue.left( pos );
        QString envVarValue = varStrNameValue.mid( pos + 1 );

        if ( systemEnvVars.contains( envVarName ) )
        {
          if ( envVarApply == QLatin1String( "prepend" ) )
          {
            envVarValue += systemEnvVars.value( envVarName );
          }
          else if ( envVarApply == QLatin1String( "append" ) )
          {
            envVarValue = systemEnvVars.value( envVarName ) + envVarValue;
          }
        }

        if ( systemEnvVars.contains( envVarName ) && envVarApply == QLatin1String( "unset" ) )
        {
#ifdef Q_OS_WIN
          putenv( envVarName.toUtf8().constData() );
#else
          unsetenv( envVarName.toUtf8().constData() );
#endif
        }
        else
        {
#ifdef Q_OS_WIN
          if ( envVarApply != "undefined" || !getenv( envVarName.toUtf8().constData() ) )
            putenv( QString( "%1=%2" ).arg( envVarName ).arg( envVarValue ).toUtf8().constData() );
#else
          setenv( envVarName.toUtf8().constData(), envVarValue.toUtf8().constData(), envVarApply == QLatin1String( "undefined" ) ? 0 : 1 );
#endif
        }
      }
    }
  }

#ifdef QGISDEBUG
  QgsFontUtils::loadStandardTestFonts( QStringList() << QStringLiteral( "Roman" ) << QStringLiteral( "Bold" ) );
#endif

  // Set the application style.  If it's not set QT will use the platform style except on Windows
  // as it looks really ugly so we use QPlastiqueStyle.
  QString presetStyle = mySettings.value( QStringLiteral( "qgis/style" ) ).toString();
  QString activeStyleName = presetStyle;
  if ( activeStyleName.isEmpty() ) // not set, using default style
  {
    //not set, check default
    activeStyleName = QApplication::style()->metaObject()->className() ;
  }
  if ( activeStyleName.contains( QStringLiteral( "adwaita" ), Qt::CaseInsensitive ) )
  {
    //never allow Adwaita themes - the Qt variants of these are VERY broken
    //for apps like QGIS. E.g. oversized controls like spinbox widgets prevent actually showing
    //any content in these widgets, leaving a very bad impression of QGIS

    //note... we only do this if there's a known good style available (fusion), as SOME
    //style choices can cause Qt apps to crash...
    if ( QStyleFactory::keys().contains( QStringLiteral( "fusion" ), Qt::CaseInsensitive ) )
    {
      presetStyle = QStringLiteral( "fusion" );
    }
  }
  if ( !presetStyle.isEmpty() )
  {
    QApplication::setStyle( presetStyle );
    mySettings.setValue( QStringLiteral( "qgis/style" ), QApplication::style()->objectName() );
  }
  /* Translation file for QGIS.
   */
  QString i18nPath = QgsApplication::i18nPath();
  QString myUserLocale = mySettings.value( QStringLiteral( "locale/userLocale" ), "" ).toString();
  bool myLocaleOverrideFlag = mySettings.value( QStringLiteral( "locale/overrideFlag" ), false ).toBool();

  //
  // Priority of translation is:
  //  - command line
  //  - user specified in options dialog (with group checked on)
  //  - system locale
  //
  //  When specifying from the command line it will change the user
  //  specified user locale
  //
  if ( !myTranslationCode.isNull() && !myTranslationCode.isEmpty() )
  {
    mySettings.setValue( QStringLiteral( "locale/userLocale" ), myTranslationCode );
  }
  else
  {
    if ( !myLocaleOverrideFlag || myUserLocale.isEmpty() )
    {
      myTranslationCode = QLocale::system().name();
      //setting the locale/userLocale when the --lang= option is not set will allow third party
      //plugins to always use the same locale as the QGIS, otherwise they can be out of sync
      mySettings.setValue( QStringLiteral( "locale/userLocale" ), myTranslationCode );
    }
    else
    {
      myTranslationCode = myUserLocale;
    }
  }

  QTranslator qgistor( nullptr );
  QTranslator qttor( nullptr );
  if ( myTranslationCode != QLatin1String( "C" ) )
  {
    if ( qgistor.load( QStringLiteral( "qgis_" ) + myTranslationCode, i18nPath ) )
    {
      myApp.installTranslator( &qgistor );
    }
    else
    {
      QgsDebugMsg( QStringLiteral( "loading of qgis translation failed %1/qgis_%2" ).arg( i18nPath, myTranslationCode ) );
    }

    /* Translation file for Qt.
     * The strings from the QMenuBar context section are used by Qt/Mac to shift
     * the About, Preferences and Quit items to the Mac Application menu.
     * These items must be translated identically in both qt_ and qgis_ files.
     */
    if ( qttor.load( QStringLiteral( "qt_" ) + myTranslationCode, QLibraryInfo::location( QLibraryInfo::TranslationsPath ) ) )
    {
      myApp.installTranslator( &qttor );
    }
    else
    {
      QgsDebugMsg( QStringLiteral( "loading of qt translation failed %1/qt_%2" ).arg( QLibraryInfo::location( QLibraryInfo::TranslationsPath ), myTranslationCode ) );
    }
  }

  // For non static builds on mac and win (static builds are not supported)
  // we need to be sure we can find the qt image
  // plugins. In mac be sure to look in the
  // application bundle...
#ifdef Q_OS_WIN
  QCoreApplication::addLibraryPath( QApplication::applicationDirPath()
                                    + QDir::separator() + "qtplugins" );
#endif
#ifdef Q_OS_MACX
  // IMPORTANT: do before Qt uses any plugins, e.g. before loading splash screen
  QString  myPath( QCoreApplication::applicationDirPath().append( "/../PlugIns" ) );
  // Check if it contains a standard Qt-specific plugin subdirectory
  if ( !QFile::exists( myPath + "/imageformats" ) )
  {
    // We are either running from build dir bundle, or launching binary directly.
    // Use system Qt plugins, since they are not bundled.
    // An app bundled with QGIS_MACAPP_BUNDLE=0 will still have Plugins/qgis in it
    myPath = QT_PLUGINS_DIR;
  }

  // First clear the plugin search paths so we can be sure only plugins we define
  // are being used. Note: this strips QgsApplication::pluginPath()
  QStringList myPathList;
  QCoreApplication::setLibraryPaths( myPathList );

  QgsDebugMsg( QString( "Adding Mac QGIS and Qt plugins dirs to search path: %1" ).arg( myPath ) );
  QCoreApplication::addLibraryPath( QgsApplication::pluginPath() );
  QCoreApplication::addLibraryPath( myPath );
#endif

  // set authentication database directory
  if ( !authdbdirectory.isEmpty() )
  {
    QgsApplication::setAuthDatabaseDirPath( authdbdirectory );
  }

  //set up splash screen
  QString mySplashPath( QgsCustomization::instance()->splashPath() );
  QPixmap myPixmap( mySplashPath + QStringLiteral( "splash.png" ) );

  int w = 600 * qApp->desktop()->logicalDpiX() / 96;
  int h = 300 * qApp->desktop()->logicalDpiY() / 96;

  QSplashScreen *mypSplash = new QSplashScreen( myPixmap.scaled( w, h, Qt::KeepAspectRatio, Qt::SmoothTransformation ) );
  if ( !myHideSplash && !mySettings.value( QStringLiteral( "qgis/hideSplash" ) ).toBool() )
  {
    //for win and linux we can just automask and png transparency areas will be used
    mypSplash->setMask( myPixmap.mask() );
    mypSplash->show();
  }

  // optionally restore default window state
  // use restoreDefaultWindowState setting only if NOT using command line (then it is set already)
  if ( myRestoreDefaultWindowState || mySettings.value( QStringLiteral( "qgis/restoreDefaultWindowState" ), false ).toBool() )
  {
    QgsDebugMsg( "Resetting /UI/state settings!" );
    mySettings.remove( QStringLiteral( "/UI/state" ) );
    mySettings.remove( QStringLiteral( "/qgis/restoreDefaultWindowState" ) );
  }

  // set max. thread count
  // this should be done in QgsApplication::init() but it doesn't know the settings dir.
  QgsApplication::setMaxThreads( mySettings.value( QStringLiteral( "qgis/max_threads" ), -1 ).toInt() );

  QgisApp *qgis = new QgisApp( mypSplash, myRestorePlugins, mySkipVersionCheck, rootProfileFolder, profileName ); // "QgisApp" used to find canonical instance
  qgis->setObjectName( QStringLiteral( "QgisApp" ) );

  myApp.connect(
    &myApp, SIGNAL( preNotify( QObject *, QEvent *, bool * ) ),
    //qgis, SLOT( preNotify( QObject *, QEvent *))
    QgsCustomization::instance(), SLOT( preNotify( QObject *, QEvent *, bool * ) )
  );

  /////////////////////////////////////////////////////////////////////
  // Load a project file if one was specified
  /////////////////////////////////////////////////////////////////////
  if ( ! sProjectFileName.isEmpty() )
  {
    qgis->openProject( sProjectFileName );
  }

  /////////////////////////////////////////////////////////////////////
  // autoload any file names that were passed in on the command line
  /////////////////////////////////////////////////////////////////////
  QgsDebugMsg( QString( "Number of files in myFileList: %1" ).arg( sFileList.count() ) );
  for ( QStringList::Iterator myIterator = sFileList.begin(); myIterator != sFileList.end(); ++myIterator )
  {
    QgsDebugMsg( QString( "Trying to load file : %1" ).arg( ( *myIterator ) ) );
    QString myLayerName = *myIterator;
    // don't load anything with a .qgs extension - these are project files
    if ( !myLayerName.endsWith( QLatin1String( ".qgs" ), Qt::CaseInsensitive ) )
    {
      qgis->openLayer( myLayerName );
    }
  }


  /////////////////////////////////////////////////////////////////////
  // Set initial extent if requested
  /////////////////////////////////////////////////////////////////////
  if ( ! myInitialExtent.isEmpty() )
  {
    QgsLocaleNumC l;
    double coords[4];
    int pos, posOld = 0;
    bool ok = true;

    // parse values from string
    // extent is defined by string "xmin,ymin,xmax,ymax"
    for ( int i = 0; i < 3; i++ )
    {
      // find comma and get coordinate
      pos = myInitialExtent.indexOf( ',', posOld );
      if ( pos == -1 )
      {
        ok = false;
        break;
      }

      coords[i] = myInitialExtent.midRef( posOld, pos - posOld ).toDouble( &ok );
      if ( !ok )
        break;

      posOld = pos + 1;
    }

    // parse last coordinate
    if ( ok )
      coords[3] = myInitialExtent.midRef( posOld ).toDouble( &ok );

    if ( !ok )
    {
      QgsDebugMsg( "Error while parsing initial extent!" );
    }
    else
    {
      // set extent from parsed values
      QgsRectangle rect( coords[0], coords[1], coords[2], coords[3] );
      qgis->setExtent( rect );
      if ( qgis->mapCanvas() )
      {
        qgis->mapCanvas()->refresh();
      }
    }
  }

  if ( !pythonfile.isEmpty() )
  {
#ifdef Q_OS_WIN
    //replace backslashes with forward slashes
    pythonfile.replace( '\\', '/' );
#endif
    QgsPythonRunner::run( QStringLiteral( "exec(open('%1').read())" ).arg( pythonfile ) );
  }

  /////////////////////////////////`////////////////////////////////////
  // Take a snapshot of the map view then exit if snapshot mode requested
  /////////////////////////////////////////////////////////////////////
  if ( mySnapshotFileName != QLatin1String( "" ) )
  {
    /*You must have at least one paintEvent() delivered for the window to be
      rendered properly.

      It looks like you don't run the event loop in non-interactive mode, so the
      event is never occurring.

      To achieve this without running the event loop: show the window, then call
      qApp->processEvents(), grab the pixmap, save it, hide the window and exit.
      */
    //qgis->show();
    myApp.processEvents();
    QPixmap *myQPixmap = new QPixmap( mySnapshotWidth, mySnapshotHeight );
    myQPixmap->fill();
    qgis->saveMapAsImage( mySnapshotFileName, myQPixmap );
    myApp.processEvents();
    qgis->hide();

    return 1;
  }

  if ( !dxfOutputFile.isEmpty() )
  {
    qgis->hide();

    QgsDxfExport dxfExport;
    dxfExport.setSymbologyScale( dxfScale );
    dxfExport.setSymbologyExport( dxfSymbologyMode );
    dxfExport.setExtent( dxfExtent );

    QStringList layerIds;
    QList< QPair<QgsVectorLayer *, int > > layers;
    if ( !dxfPreset.isEmpty() )
    {
      Q_FOREACH ( QgsMapLayer *layer, QgsProject::instance()->mapThemeCollection()->mapThemeVisibleLayers( dxfPreset ) )
      {
        QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( layer );
        if ( !vl )
          continue;
        layers << qMakePair<QgsVectorLayer *, int>( vl, -1 );
        layerIds << vl->id();
      }
    }
    else
    {
      Q_FOREACH ( QgsMapLayer *ml, QgsProject::instance()->mapLayers() )
      {
        QgsVectorLayer *vl = qobject_cast<QgsVectorLayer *>( ml );
        if ( !vl )
          continue;
        layers << qMakePair<QgsVectorLayer *, int>( vl, -1 );
        layerIds << vl->id();
      }
    }

    if ( !layers.isEmpty() )
    {
      dxfExport.addLayers( layers );
    }

    QFile dxfFile;
    if ( dxfOutputFile == QLatin1String( "-" ) )
    {
      if ( !dxfFile.open( stdout, QIODevice::WriteOnly | QIODevice::Truncate ) )
      {
        std::cerr << "could not open stdout" << std::endl;
        return 2;
      }
    }
    else
    {
      if ( !dxfOutputFile.endsWith( QLatin1String( ".dxf" ), Qt::CaseInsensitive ) )
        dxfOutputFile += QLatin1String( ".dxf" );
      dxfFile.setFileName( dxfOutputFile );
    }

    int res = dxfExport.writeToFile( &dxfFile, dxfEncoding );
    if ( res )
      std::cerr << "dxf output failed with error code " << res << std::endl;

    delete qgis;

    return res;
  }

  /////////////////////////////////////////////////////////////////////
  // Continue on to interactive gui...
  /////////////////////////////////////////////////////////////////////
  qgis->show();
  myApp.connect( &myApp, SIGNAL( lastWindowClosed() ), &myApp, SLOT( quit() ) );

  mypSplash->finish( qgis );
  delete mypSplash;

  qgis->completeInitialization();

#if defined(ANDROID)
  // fix for Qt Ministro hiding app's menubar in favor of native Android menus
  qgis->menuBar()->setNativeMenuBar( false );
  qgis->menuBar()->setVisible( true );
#endif

  int retval = myApp.exec();
  delete qgis;
  return retval;
}
