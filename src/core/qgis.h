/***************************************************************************
                          qgis.h - QGIS namespace
                             -------------------
    begin                : Sat Jun 30 2002
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

#ifndef QGIS_H
#define QGIS_H

#include <QEvent>
#include "qgis_sip.h"
#include <QString>
#include <QRegExp>
#include <QMetaType>
#include <QVariant>
#include <QDateTime>
#include <QDate>
#include <QTime>
#include <QHash>
#include <cstdlib>
#include <cfloat>
#include <cmath>
#include <qnumeric.h>

#include "qgswkbtypes.h"
#include "qgis_core.h"
#include "qgis_sip.h"

#ifdef SIP_RUN
% ModuleHeaderCode
#include <qgis.h>
% End

% ModuleCode
int QgisEvent = QEvent::User + 1;
% End
#endif


/** \ingroup core
 * The Qgis class provides global constants for use throughout the application.
 */
class CORE_EXPORT Qgis
{
  public:
    // Version constants
    //
    //! Version string
    static const QString QGIS_VERSION;
    //! Version number used for comparing versions using the "Check QGIS Version" function
    static const int QGIS_VERSION_INT;
    //! Release name
    static const QString QGIS_RELEASE_NAME;
    //! The development version
    static const char *QGIS_DEV_VERSION;

    // Enumerations
    //

    /** Raster data types.
     *  This is modified and extended copy of GDALDataType.
     */
    enum DataType
    {
      UnknownDataType = 0, //!< Unknown or unspecified type
      Byte = 1, //!< Eight bit unsigned integer (quint8)
      UInt16 = 2, //!< Sixteen bit unsigned integer (quint16)
      Int16 = 3, //!< Sixteen bit signed integer (qint16)
      UInt32 = 4, //!< Thirty two bit unsigned integer (quint32)
      Int32 = 5, //!< Thirty two bit signed integer (qint32)
      Float32 = 6, //!< Thirty two bit floating point (float)
      Float64 = 7, //!< Sixty four bit floating point (double)
      CInt16 = 8, //!< Complex Int16
      CInt32 = 9, //!< Complex Int32
      CFloat32 = 10, //!< Complex Float32
      CFloat64 = 11, //!< Complex Float64
      ARGB32 = 12, //!< Color, alpha, red, green, blue, 4 bytes the same as QImage::Format_ARGB32
      ARGB32_Premultiplied = 13 //!< Color, alpha, red, green, blue, 4 bytes  the same as QImage::Format_ARGB32_Premultiplied
    };

    /** Identify search radius in mm
     *  \since QGIS 2.3 */
    static const double DEFAULT_SEARCH_RADIUS_MM;

    //! Default threshold between map coordinates and device coordinates for map2pixel simplification
    static const float DEFAULT_MAPTOPIXEL_THRESHOLD;

    /** Default highlight color.  The transparency is expected to only be applied to polygon
     *  fill. Lines and outlines are rendered opaque.
     *  \since QGIS 2.3 */
    static const QColor DEFAULT_HIGHLIGHT_COLOR;

    /** Default highlight buffer in mm.
     *  \since QGIS 2.3 */
    static const double DEFAULT_HIGHLIGHT_BUFFER_MM;

    /** Default highlight line/stroke minimum width in mm.
     *  \since QGIS 2.3 */
    static const double DEFAULT_HIGHLIGHT_MIN_WIDTH_MM;

    /** Fudge factor used to compare two scales. The code is often going from scale to scale
     *  denominator. So it looses precision and, when a limit is inclusive, can lead to errors.
     *  To avoid that, use this factor instead of using <= or >=.
     * \since QGIS 2.15*/
    static const double SCALE_PRECISION;

    /** Default Z coordinate value for 2.5d geometry
     *  This value have to be assigned to the Z coordinate for the new 2.5d geometry vertex.
     *  \since QGIS 3.0 */
    static const double DEFAULT_Z_COORDINATE;

    /**
     * UI scaling factor. This should be applied to all widget sizes obtained from font metrics,
     * to account for differences in the default font sizes across different platforms.
     *  \since QGIS 3.0
    */
    static const double UI_SCALE_FACTOR;

};

// hack to workaround warnings when casting void pointers
// retrieved from QLibrary::resolve to function pointers.
// It's assumed that this works on all systems supporting
// QLibrary
#define cast_to_fptr(f) f


/** \ingroup core
 * RAII signal blocking class. Used for temporarily blocking signals from a QObject
 * for the lifetime of QgsSignalBlocker object.
 * \see whileBlocking()
 * \since QGIS 2.16
 * \note not available in Python bindings
 */
// based on Boojum's code from http://stackoverflow.com/questions/3556687/prevent-firing-signals-in-qt
template<class Object> class QgsSignalBlocker SIP_SKIP SIP_SKIP // clazy:exclude=rule-of-three
{
  public:

    /** Constructor for QgsSignalBlocker
     * \param object QObject to block signals from
     */
    explicit QgsSignalBlocker( Object *object )
      : mObject( object )
      , mPreviousState( object->blockSignals( true ) )
    {}

    ~QgsSignalBlocker()
    {
      mObject->blockSignals( mPreviousState );
    }

    //! Returns pointer to blocked QObject
    Object *operator->() { return mObject; }

  private:

    Object *mObject = nullptr;
    bool mPreviousState;

};

/** Temporarily blocks signals from a QObject while calling a single method from the object.
 *
 * Usage:
 *   whileBlocking( checkBox )->setChecked( true );
 *   whileBlocking( spinBox )->setValue( 50 );
 *
 * No signals will be emitted when calling these methods.
 *
 * \since QGIS 2.16
 * \see QgsSignalBlocker
 * \note not available in Python bindings
 */
// based on Boojum's code from http://stackoverflow.com/questions/3556687/prevent-firing-signals-in-qt
template<class Object> inline QgsSignalBlocker<Object> whileBlocking( Object *object ) SIP_SKIP SIP_SKIP
{
  return QgsSignalBlocker<Object>( object );
}

//! Hash for QVariant
CORE_EXPORT uint qHash( const QVariant &variant );

//! Returns a string representation of a double
//! \param a double value
//! \param precision number of decimal places to retain
inline QString qgsDoubleToString( double a, int precision = 17 )
{
  if ( precision )
    return QString::number( a, 'f', precision ).remove( QRegExp( "\\.?0+$" ) );
  else
    return QString::number( a, 'f', precision );
}

//! Compare two doubles (but allow some difference)
//! \param a first double
//! \param b second double
//! \param epsilon maximum difference allowable between doubles
inline bool qgsDoubleNear( double a, double b, double epsilon = 4 * DBL_EPSILON )
{
  const double diff = a - b;
  return diff > -epsilon && diff <= epsilon;
}

//! Compare two floats (but allow some difference)
//! \param a first float
//! \param b second float
//! \param epsilon maximum difference allowable between floats
inline bool qgsFloatNear( float a, float b, float epsilon = 4 * FLT_EPSILON )
{
  const float diff = a - b;
  return diff > -epsilon && diff <= epsilon;
}

//! Compare two doubles using specified number of significant digits
inline bool qgsDoubleNearSig( double a, double b, int significantDigits = 10 )
{
  // The most simple would be to print numbers as %.xe and compare as strings
  // but that is probably too costly
  // Then the fastest would be to set some bits directly, but little/big endian
  // has to be considered (maybe TODO)
  // Is there a better way?
  int aexp, bexp;
  double ar = std::frexp( a, &aexp );
  double br = std::frexp( b, &bexp );

  return aexp == bexp &&
         std::round( ar * std::pow( 10.0, significantDigits ) ) == std::round( br * std::pow( 10.0, significantDigits ) );
}

/**
 * Returns a double \a number, rounded (as close as possible) to the specified number of \a places.
 *
 * \since QGIS 3.0
 */
inline double qgsRound( double number, double places )
{
  int scaleFactor = std::pow( 10, places );
  return static_cast<double>( static_cast<qlonglong>( number * scaleFactor + 0.5 ) ) / scaleFactor;
}

/** Converts a string to a double in a permissive way, e.g., allowing for incorrect
 * numbers of digits between thousand separators
 * \param string string to convert
 * \param ok will be set to true if conversion was successful
 * \returns string converted to double if possible
 * \since QGIS 2.9
 * \see permissiveToInt
 */
CORE_EXPORT double qgsPermissiveToDouble( QString string, bool &ok );

/** Converts a string to an integer in a permissive way, e.g., allowing for incorrect
 * numbers of digits between thousand separators
 * \param string string to convert
 * \param ok will be set to true if conversion was successful
 * \returns string converted to int if possible
 * \since QGIS 2.9
 * \see permissiveToDouble
 */
CORE_EXPORT int qgsPermissiveToInt( QString string, bool &ok );

//! Compares two QVariant values and returns whether the first is less than the second.
//! Useful for sorting lists of variants, correctly handling sorting of the various
//! QVariant data types (such as strings, numeric values, dates and times)
//! \see qgsVariantGreaterThan()
CORE_EXPORT bool qgsVariantLessThan( const QVariant &lhs, const QVariant &rhs );

//! Compares two QVariant values and returns whether the first is greater than the second.
//! Useful for sorting lists of variants, correctly handling sorting of the various
//! QVariant data types (such as strings, numeric values, dates and times)
//! \see qgsVariantLessThan()
CORE_EXPORT bool qgsVariantGreaterThan( const QVariant &lhs, const QVariant &rhs );

CORE_EXPORT QString qgsVsiPrefix( const QString &path );

/** Allocates size bytes and returns a pointer to the allocated  memory.
    Works like C malloc() but prints debug message by QgsLogger if allocation fails.
    \param size size in bytes
 */
void CORE_EXPORT *qgsMalloc( size_t size ) SIP_SKIP;

/** Allocates  memory for an array of nmemb elements of size bytes each and returns
    a pointer to the allocated memory. Works like C calloc() but prints debug message
    by QgsLogger if allocation fails.
    \param nmemb number of elements
    \param size size of element in bytes
 */
void CORE_EXPORT *qgsCalloc( size_t nmemb, size_t size ) SIP_SKIP;

/** Frees the memory space  pointed  to  by  ptr. Works like C free().
    \param ptr pointer to memory space
 */
void CORE_EXPORT qgsFree( void *ptr ) SIP_SKIP;

/** Wkt string that represents a geographic coord sys
 * \since QGIS GEOWkt
 */
extern CORE_EXPORT const QString GEOWKT;
extern CORE_EXPORT const QString PROJECT_SCALES;

//! PROJ4 string that represents a geographic coord sys
extern CORE_EXPORT const QString GEOPROJ4;
//! Magic number for a geographic coord sys in POSTGIS SRID
const long GEOSRID = 4326;
//! Magic number for a geographic coord sys in QGIS srs.db tbl_srs.srs_id
const long GEOCRS_ID = 3452;
//! Magic number for a geographic coord sys in EpsgCrsId ID format
const long GEO_EPSG_CRS_ID = 4326;
//! Geographic coord sys from EPSG authority
extern CORE_EXPORT const QString GEO_EPSG_CRS_AUTHID;

/** Magick number that determines whether a projection crsid is a system (srs.db)
 *  or user (~/.qgis.qgis.db) defined projection. */
const int USER_CRS_START_ID = 100000;

//! Constant that holds the string representation for "No ellips/No CRS"
extern CORE_EXPORT const QString GEO_NONE;

//
// Constants for point symbols
//

//! Magic number that determines the default point size for point symbols
const double DEFAULT_POINT_SIZE = 2.0;
const double DEFAULT_LINE_WIDTH = 0.26;

//! Default snapping tolerance for segments
const double DEFAULT_SEGMENT_EPSILON = 1e-8;

typedef QMap<QString, QString> QgsStringMap SIP_SKIP;

/** Qgssize is used instead of size_t, because size_t is stdlib type, unknown
 *  by SIP, and it would be hard to define size_t correctly in SIP.
 *  Currently used "unsigned long long" was introduced in C++11 (2011)
 *  but it was supported already before C++11 on common platforms.
 *  "unsigned long long int" gives syntax error in SIP.
 *  KEEP IN SYNC WITH qgssize defined in SIP! */
typedef unsigned long long qgssize;

#ifndef SIP_RUN
#if (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)) || defined(__clang__)

#define Q_NOWARN_DEPRECATED_PUSH \
  _Pragma("GCC diagnostic push") \
  _Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"");
#define Q_NOWARN_DEPRECATED_POP \
  _Pragma("GCC diagnostic pop");
#define Q_NOWARN_UNREACHABLE_PUSH
#define Q_NOWARN_UNREACHABLE_POP

#elif defined(_MSC_VER)

#define Q_NOWARN_DEPRECATED_PUSH \
  __pragma(warning(push)) \
  __pragma(warning(disable:4996))
#define Q_NOWARN_DEPRECATED_POP \
  __pragma(warning(pop))
#define Q_NOWARN_UNREACHABLE_PUSH \
  __pragma(warning(push)) \
  __pragma(warning(disable:4702))
#define Q_NOWARN_UNREACHABLE_POP \
  __pragma(warning(pop))

#else

#define Q_NOWARN_DEPRECATED_PUSH
#define Q_NOWARN_DEPRECATED_POP
#define Q_NOWARN_UNREACHABLE_PUSH
#define Q_NOWARN_UNREACHABLE_POP

#endif
#endif

#ifndef QGISEXTERN
#ifdef Q_OS_WIN
#  define QGISEXTERN extern "C" __declspec( dllexport )
#  ifdef _MSC_VER
// do not warn about C bindings returning QString
#    pragma warning(disable:4190)
#  endif
#else
#  if defined(__GNUC__) || defined(__clang__)
#    define QGISEXTERN extern "C" __attribute__ ((visibility ("default")))
#  else
#    define QGISEXTERN extern "C"
#  endif
#endif
#endif
#endif

#if __cplusplus >= 201500
#define FALLTHROUGH [[fallthrough]];
#elif defined(__clang__)
#define FALLTHROUGH //[[clang::fallthrough]]
#elif defined(__GNUC__) && __GNUC__ >= 7
#define FALLTHROUGH [[gnu::fallthrough]];
#else
#define FALLTHROUGH
#endif


