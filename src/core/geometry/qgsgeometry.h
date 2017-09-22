/***************************************************************************
  qgsgeometry.h - Geometry (stored as Open Geospatial Consortium WKB)
  -------------------------------------------------------------------
Date                 : 02 May 2005
Copyright            : (C) 2005 by Brendan Morley
email                : morb at ozemail dot com dot au
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef QGSGEOMETRY_H
#define QGSGEOMETRY_H

#include <QDomDocument>
#include <QSet>
#include <QString>
#include <QVector>

#include <geos_c.h>
#include <climits>
#include <limits>

#include "qgis_core.h"
#include "qgis.h"


#if defined(GEOS_VERSION_MAJOR) && (GEOS_VERSION_MAJOR<3)
#define GEOSGeometry struct GEOSGeom_t
#define GEOSCoordSequence struct GEOSCoordSeq_t
#endif

#include "qgsabstractgeometry.h"
#include "qgsfeature.h"
#include "qgspointxy.h"
#include "qgspoint.h"


class QgsGeometryEngine;
class QgsVectorLayer;
class QgsMapToPixel;
class QPainter;
class QgsPolygonV2;
class QgsLineString;

//! Polyline is represented as a vector of points
typedef QVector<QgsPointXY> QgsPolyline;

//! Polygon: first item of the list is outer ring, inner rings (if any) start from second item
#ifndef SIP_RUN
typedef QVector<QgsPolyline> QgsPolygon;
#else
typedef QVector<QVector<QgsPointXY>> QgsPolygon;
#endif

//! A collection of QgsPoints that share a common collection of attributes
#ifndef SIP_RUN
typedef QVector<QgsPointXY> QgsMultiPoint;
#else
typedef QVector<QgsPointXY> QgsMultiPoint;
#endif

//! A collection of QgsPolylines that share a common collection of attributes
#ifndef SIP_RUN
typedef QVector<QgsPolyline> QgsMultiPolyline;
#else
typedef QVector<QVector<QgsPointXY>> QgsMultiPolyline;
#endif

//! A collection of QgsPolygons that share a common collection of attributes
#ifndef SIP_RUN
typedef QVector<QgsPolygon> QgsMultiPolygon;
#else
typedef QVector<QVector<QVector<QgsPointXY>>> QgsMultiPolygon;
#endif

class QgsRectangle;

class QgsConstWkbPtr;

struct QgsGeometryPrivate;

/** \ingroup core
 * A geometry is the spatial representation of a feature. Since QGIS 2.10, QgsGeometry acts as a generic container
 * for geometry objects. QgsGeometry is implicitly shared, so making copies of geometries is inexpensive. The geometry
 * container class can also be stored inside a QVariant object.
 *
 * The actual geometry representation is stored as a QgsAbstractGeometry within the container, and
 * can be accessed via the geometry() method or set using the setGeometry() method.
 */

class CORE_EXPORT QgsGeometry
{
  public:

    /**
     * Success or failure of a geometry operation.
     * This gives details about cause of failure.
     */
    enum OperationResult
    {
      Success = 0, //!< Operation succeeded
      NothingHappened = 1000, //!< Nothing happened, without any error
      InvalidBaseGeometry, //!< The base geometry on which the operation is done is invalid or empty
      InvalidInput, //!< The input geometry (ring, part, split line, etc.) has not the correct geometry type
      GeometryEngineError, //!< Geometry engine misses a method implemented or an error occurred in the geometry engine
      /* Add part issues */
      AddPartSelectedGeometryNotFound, //!< The selected geometry cannot be found
      AddPartNotMultiGeometry, //!< The source geometry is not multi
      /* Add ring issues*/
      AddRingNotClosed, //!< The input ring is not closed
      AddRingNotValid, //!< The input ring is not valid
      AddRingCrossesExistingRings, //!< The input ring crosses existing rings (it is not disjoint)
      AddRingNotInExistingFeature, //!< The input ring doesn't have any existing ring to fit into
      /* Split features */
      SplitCannotSplitPoint, //!< Cannot split points
    };

    //! Constructor
    QgsGeometry();

    //! Copy constructor will prompt a deep copy of the object
    QgsGeometry( const QgsGeometry & );

    /**
     * Creates a deep copy of the object
     * \note not available in Python bindings
     */
    QgsGeometry &operator=( QgsGeometry const &rhs ) SIP_SKIP;

    /**
     * Creates a geometry from an abstract geometry object. Ownership of
     * geom is transferred.
     * \since QGIS 2.10
     */
    explicit QgsGeometry( QgsAbstractGeometry *geom SIP_TRANSFER );


    ~QgsGeometry();

    /**
     * Returns the underlying geometry store.
    * \since QGIS 2.10
    * \see setGeometry
    */
    QgsAbstractGeometry *geometry() const;

    /**
     * Sets the underlying geometry store. Ownership of geometry is transferred.
     * \since QGIS 2.10
     * \see geometry
     */
    void setGeometry( QgsAbstractGeometry *geometry SIP_TRANSFER );

    /**
     * Returns true if the geometry is null (ie, contains no underlying geometry
     * accessible via geometry() ).
     * \see geometry
     * \since QGIS 2.10
     * \see isEmpty()
     */
    bool isNull() const;

    //! Creates a new geometry from a WKT string
    static QgsGeometry fromWkt( const QString &wkt );
    //! Creates a new geometry from a QgsPointXY object
    static QgsGeometry fromPoint( const QgsPointXY &point );
    //! Creates a new geometry from a QgsMultiPoint object
    static QgsGeometry fromMultiPoint( const QgsMultiPoint &multipoint );
    //! Creates a new geometry from a QgsPolyline object
    static QgsGeometry fromPolyline( const QgsPolyline &polyline );
    //! Creates a new geometry from a QgsMultiPolyline object
    static QgsGeometry fromMultiPolyline( const QgsMultiPolyline &multiline );
    //! Creates a new geometry from a QgsPolygon
    static QgsGeometry fromPolygon( const QgsPolygon &polygon );
    //! Creates a new geometry from a QgsMultiPolygon
    static QgsGeometry fromMultiPolygon( const QgsMultiPolygon &multipoly );
    //! Creates a new geometry from a QgsRectangle
    static QgsGeometry fromRect( const QgsRectangle &rect );
    //! Creates a new multipart geometry from a list of QgsGeometry objects
    static QgsGeometry collectGeometry( const QList< QgsGeometry > &geometries );

    /**
     * Set the geometry, feeding in a geometry in GEOS format.
     * This class will take ownership of the buffer.
     * \note not available in Python bindings
     */
    void fromGeos( GEOSGeometry *geos ) SIP_SKIP;

    /**
     * Set the geometry, feeding in the buffer containing OGC Well-Known Binary and the buffer's length.
     * This class will take ownership of the buffer.
     * \note not available in Python bindings
     */
    void fromWkb( unsigned char *wkb, int length ) SIP_SKIP;

    /**
     * Set the geometry, feeding in the buffer containing OGC Well-Known Binary
     * \since QGIS 3.0
     */
    void fromWkb( const QByteArray &wkb );

    /** Returns a geos geometry - caller takes ownership of the object (should be deleted with GEOSGeom_destroy_r)
     *  \param precision The precision of the grid to which to snap the geometry vertices. If 0, no snapping is performed.
     *  \since QGIS 3.0
     *  \note not available in Python bindings
     */
    GEOSGeometry *exportToGeos( double precision = 0 ) const SIP_SKIP;

    /** Returns type of the geometry as a WKB type (point / linestring / polygon etc.)
     * \see type
     */
    QgsWkbTypes::Type wkbType() const;

    /** Returns type of the geometry as a QgsWkbTypes::GeometryType
     * \see wkbType
     */
    QgsWkbTypes::GeometryType type() const;

    /**
     * Returns true if the geometry is empty (eg a linestring with no vertices,
     * or a collection with no geometries). A null geometry will always
     * return true for isEmpty().
     * \see isNull()
     */
    bool isEmpty() const;

    //! Returns true if WKB of the geometry is of WKBMulti* type
    bool isMultipart() const;

    /** Compares the geometry with another geometry using GEOS
     * \since QGIS 1.5
     */
    bool isGeosEqual( const QgsGeometry & ) const;

    /** Checks validity of the geometry using GEOS
     * \since QGIS 1.5
     */
    bool isGeosValid() const;

    /** Determines whether the geometry is simple (according to OGC definition),
     * i.e. it has no anomalous geometric points, such as self-intersection or self-tangency.
     * Uses GEOS library for the test.
     * \note This is useful mainly for linestrings and linear rings. Polygons are simple by definition,
     * for checking anomalies in polygon geometries one can use isGeosValid().
     * \since QGIS 3.0
     */
    bool isSimple() const;

    /**
     * Returns the area of the geometry using GEOS
     * \since QGIS 1.5
     */
    double area() const;

    /**
     * Returns the length of geometry using GEOS
     * \since QGIS 1.5
     */
    double length() const;

    /**
     * Returns the minimum distance between this geometry and another geometry, using GEOS.
     * Will return a negative value if a geometry is missing.
     *
     * \param geom geometry to find minimum distance to
     */
    double distance( const QgsGeometry &geom ) const;

    /**
     * Returns the vertex closest to the given point, the corresponding vertex index, squared distance snap point / target point
     * and the indices of the vertices before and after the closest vertex.
     * \param point point to search for
     * \param atVertex will be set to the vertex index of the closest found vertex
     * \param beforeVertex will be set to the vertex index of the previous vertex from the closest one. Will be set to -1 if
     * not present.
     * \param afterVertex will be set to the vertex index of the next vertex after the closest one. Will be set to -1 if
     * not present.
     * \param sqrDist will be set to the square distance between the closest vertex and the specified point
     * \returns closest point in geometry. If not found (empty geometry), returns null point nad sqrDist is negative.
     */
    //TODO QGIS 3.0 - rename beforeVertex to previousVertex, afterVertex to nextVertex
    QgsPointXY closestVertex( const QgsPointXY &point, int &atVertex SIP_OUT, int &beforeVertex SIP_OUT, int &afterVertex SIP_OUT, double &sqrDist SIP_OUT ) const;

    /**
     * Returns the distance along this geometry from its first vertex to the specified vertex.
     * \param vertex vertex index to calculate distance to
     * \returns distance to vertex (following geometry), or -1 for invalid vertex numbers
     * \since QGIS 2.16
     */
    double distanceToVertex( int vertex ) const;

    /**
     * Returns the bisector angle for this geometry at the specified vertex.
     * \param vertex vertex index to calculate bisector angle at
     * \returns bisector angle, in radians clockwise from north
     * \since QGIS 3.0
     * \see interpolateAngle()
     */
    double angleAtVertex( int vertex ) const;

    /**
     * Returns the indexes of the vertices before and after the given vertex index.
     *
     * This function takes into account the following factors:
     *
     * 1. If the given vertex index is at the end of a linestring,
     *    the adjacent index will be -1 (for "no adjacent vertex")
     * 2. If the given vertex index is at the end of a linear ring
     *    (such as in a polygon), the adjacent index will take into
     *    account the first vertex is equal to the last vertex (and will
     *    skip equal vertex positions).
     */
    void adjacentVertices( int atVertex, int &beforeVertex SIP_OUT, int &afterVertex SIP_OUT ) const;

    /** Insert a new vertex before the given vertex index,
     *  ring and item (first number is index 0)
     *  If the requested vertex number (beforeVertex.back()) is greater
     *  than the last actual vertex on the requested ring and item,
     *  it is assumed that the vertex is to be appended instead of inserted.
     *  Returns false if atVertex does not correspond to a valid vertex
     *  on this geometry (including if this geometry is a Point).
     *  It is up to the caller to distinguish between
     *  these error conditions.  (Or maybe we add another method to this
     *  object to help make the distinction?)
     */
    bool insertVertex( double x, double y, int beforeVertex );

    /** Insert a new vertex before the given vertex index,
     *  ring and item (first number is index 0)
     *  If the requested vertex number (beforeVertex.back()) is greater
     *  than the last actual vertex on the requested ring and item,
     *  it is assumed that the vertex is to be appended instead of inserted.
     *  Returns false if atVertex does not correspond to a valid vertex
     *  on this geometry (including if this geometry is a Point).
     *  It is up to the caller to distinguish between
     *  these error conditions.  (Or maybe we add another method to this
     *  object to help make the distinction?)
     */
    bool insertVertex( const QgsPoint &point, int beforeVertex );

    /**
     * Moves the vertex at the given position number
     * and item (first number is index 0)
     * to the given coordinates.
     * Returns false if atVertex does not correspond to a valid vertex
     * on this geometry
     */
    bool moveVertex( double x, double y, int atVertex );

    /**
     * Moves the vertex at the given position number
     * and item (first number is index 0)
     * to the given coordinates.
     * Returns false if atVertex does not correspond to a valid vertex
     * on this geometry
     */
    bool moveVertex( const QgsPoint &p, int atVertex );

    /**
     * Deletes the vertex at the given position number and item
     * (first number is index 0)
     * \returns false if atVertex does not correspond to a valid vertex
     * on this geometry (including if this geometry is a Point),
     * or if the number of remaining vertices in the linestring
     * would be less than two.
     * It is up to the caller to distinguish between
     * these error conditions.  (Or maybe we add another method to this
     * object to help make the distinction?)
     */
    bool deleteVertex( int atVertex );

    /**
     * Returns coordinates of a vertex.
     * \param atVertex index of the vertex
     * \returns Coordinates of the vertex or QgsPoint(0,0) on error
     */
    QgsPoint vertexAt( int atVertex ) const;

    /**
     * Returns the squared Cartesian distance between the given point
     * to the given vertex index (vertex at the given position number,
     * ring and item (first number is index 0))
     */
    double sqrDistToVertexAt( QgsPointXY &point SIP_IN, int atVertex ) const;

    /**
     * Returns the nearest point on this geometry to another geometry.
     * \since QGIS 2.14
     * \see shortestLine()
     */
    QgsGeometry nearestPoint( const QgsGeometry &other ) const;

    /**
     * Returns the shortest line joining this geometry to another geometry.
     * \since QGIS 2.14
     * \see nearestPoint()
     */
    QgsGeometry shortestLine( const QgsGeometry &other ) const;

    /**
     * Searches for the closest vertex in this geometry to the given point.
     * \param point Specifiest the point for search
     * \param atVertex Receives index of the closest vertex
     * \returns The squared Cartesian distance is also returned in sqrDist, negative number on error
     */
    double closestVertexWithContext( const QgsPointXY &point, int &atVertex SIP_OUT ) const;

    /**
     * Searches for the closest segment of geometry to the given point
     * \param point Specifies the point for search
     * \param minDistPoint Receives the nearest point on the segment
     * \param afterVertex Receives index of the vertex after the closest segment. The vertex
     * before the closest segment is always afterVertex - 1
     * \param leftOf Out: Returns if the point lies on the left of right side of the segment ( < 0 means left, > 0 means right )
     * \param epsilon epsilon for segment snapping
     * \returns The squared Cartesian distance is also returned in sqrDist, negative number on error
     */
#ifndef SIP_RUN
    double closestSegmentWithContext( const QgsPointXY &point, QgsPointXY &minDistPoint, int &afterVertex, double *leftOf = nullptr, double epsilon = DEFAULT_SEGMENT_EPSILON ) const;
#else
    double closestSegmentWithContext( const QgsPointXY &point, QgsPointXY &minDistPoint SIP_OUT, int &afterVertex SIP_OUT ) const;
#endif

    /**
     * Adds a new ring to this geometry. This makes only sense for polygon and multipolygons.
     * \param ring The ring to be added
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult addRing( const QList<QgsPointXY> &ring );

    /**
     * Adds a new ring to this geometry. This makes only sense for polygon and multipolygons.
     * \param ring The ring to be added
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult addRing( QgsCurve *ring SIP_TRANSFER );

    /**
     * Adds a new part to a the geometry.
     * \param points points describing part to add
     * \param geomType default geometry type to create if no existing geometry
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult addPart( const QList<QgsPointXY> &points, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::UnknownGeometry ) SIP_PYNAME( addPoints );

    /**
     * Adds a new part to a the geometry.
     * \param points points describing part to add
     * \param geomType default geometry type to create if no existing geometry
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult addPart( const QgsPointSequence &points, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::UnknownGeometry ) SIP_PYNAME( addPointsV2 );

    /**
     * Adds a new part to this geometry.
     * \param part part to add (ownership is transferred)
     * \param geomType default geometry type to create if no existing geometry
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult addPart( QgsAbstractGeometry *part SIP_TRANSFER, QgsWkbTypes::GeometryType geomType = QgsWkbTypes::UnknownGeometry );

    /**
     * Adds a new island polygon to a multipolygon feature
     * \param newPart part to add. Ownership is NOT transferred.
     * \returns OperationResult a result code: success or reason of failure
     * \note not available in python bindings
     */
    OperationResult addPart( GEOSGeometry *newPart ) SIP_SKIP;

    /**
     * Adds a new island polygon to a multipolygon feature
     * \returns OperationResult a result code: success or reason of failure
     * \note available in python bindings as addPartGeometry
     */
    OperationResult addPart( const QgsGeometry &newPart ) SIP_PYNAME( addPartGeometry );

    /**
     * Removes the interior rings from a (multi)polygon geometry. If the minimumAllowedArea
     * parameter is specified then only rings smaller than this minimum
     * area will be removed.
     * \since QGIS 3.0
     */
    QgsGeometry removeInteriorRings( double minimumAllowedArea = -1 ) const;

    /**
     * Translates this geometry by dx, dy
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult translate( double dx, double dy );

    /**
     * Transforms this geometry as described by CoordinateTransform ct
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult transform( const QgsCoordinateTransform &ct );

    /**
     * Transforms this geometry as described by QTransform ct
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult transform( const QTransform &ct );

    /**
     * Rotate this geometry around the Z axis
     * \param rotation clockwise rotation in degrees
     * \param center rotation center
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult rotate( double rotation, const QgsPointXY &center );

    /**
     * Splits this geometry according to a given line.
     * \param splitLine the line that splits the geometry
     * \param[out] newGeometries list of new geometries that have been created with the split
     * \param topological true if topological editing is enabled
     * \param[out] topologyTestPoints points that need to be tested for topological completeness in the dataset
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult splitGeometry( const QList<QgsPointXY> &splitLine, QList<QgsGeometry> &newGeometries SIP_OUT, bool topological, QList<QgsPointXY> &topologyTestPoints SIP_OUT );

    /**
     * Replaces a part of this geometry with another line
     * \returns OperationResult a result code: success or reason of failure
     */
    OperationResult reshapeGeometry( const QgsLineString &reshapeLineString );

    /**
     * Changes this geometry such that it does not intersect the other geometry
     * \param other geometry that should not be intersect
     * \note Not available in Python
     */
    int makeDifferenceInPlace( const QgsGeometry &other ) SIP_SKIP;

    /**
     * Returns the geometry formed by modifying this geometry such that it does not
     * intersect the other geometry.
     * \param other geometry that should not be intersect
     * \returns difference geometry, or empty geometry if difference could not be calculated
     * \since QGIS 3.0
     */
    QgsGeometry makeDifference( const QgsGeometry &other ) const;

    /**
     * Returns the bounding box of the geometry.
     * \see orientedMinimumBoundingBox()
     */
    QgsRectangle boundingBox() const;

    /**
     * Returns the oriented minimum bounding box for the geometry, which is the smallest (by area)
     * rotated rectangle which fully encompasses the geometry. The area, angle (clockwise in degrees from North),
     * width and height of the rotated bounding box will also be returned.
     * \since QGIS 3.0
     * \see boundingBox()
     */
    QgsGeometry orientedMinimumBoundingBox( double &area SIP_OUT, double &angle SIP_OUT, double &width SIP_OUT, double &height SIP_OUT ) const;

    /**
     * Attempts to orthogonalize a line or polygon geometry by shifting vertices to make the geometries
     * angles either right angles or flat lines. This is an iterative algorithm which will loop until
     * either the vertices are within a specified tolerance of right angles or a set number of maximum
     * iterations is reached. The angle threshold parameter specifies how close to a right angle or
     * straight line an angle must be before it is attempted to be straightened.
     * \since QGIS 3.0
     */
    QgsGeometry orthogonalize( double tolerance = 1.0E-8, int maxIterations = 1000, double angleThreshold = 15.0 ) const;

    //! Tests for intersection with a rectangle (uses GEOS)
    bool intersects( const QgsRectangle &r ) const;

    //! Tests for intersection with a geometry (uses GEOS)
    bool intersects( const QgsGeometry &geometry ) const;

    //! Tests for containment of a point (uses GEOS)
    bool contains( const QgsPointXY *p ) const;

    /** Tests for if geometry is contained in another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool contains( const QgsGeometry &geometry ) const;

    /** Tests for if geometry is disjoint of another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool disjoint( const QgsGeometry &geometry ) const;

    /** Test for if geometry equals another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool equals( const QgsGeometry &geometry ) const;

    /** Test for if geometry touch another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool touches( const QgsGeometry &geometry ) const;

    /** Test for if geometry overlaps another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool overlaps( const QgsGeometry &geometry ) const;

    /** Test for if geometry is within another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool within( const QgsGeometry &geometry ) const;

    /** Test for if geometry crosses another (uses GEOS)
     *  \since QGIS 1.5
     */
    bool crosses( const QgsGeometry &geometry ) const;

    //! Side of line to buffer
    enum BufferSide
    {
      SideLeft = 0, //!< Buffer to left of line
      SideRight, //!< Buffer to right of line
    };

    //! End cap styles for buffers
    enum EndCapStyle
    {
      CapRound = 1, //!< Round cap
      CapFlat, //!< Flat cap (in line with start/end of line)
      CapSquare, //!< Square cap (extends past start/end of line by buffer distance)
    };

    //! Join styles for buffers
    enum JoinStyle
    {
      JoinStyleRound = 1, //!< Use rounded joins
      JoinStyleMiter, //!< Use mitered joins
      JoinStyleBevel, //!< Use beveled joins
    };

    /**
     * Returns a buffer region around this geometry having the given width and with a specified number
     * of segments used to approximate curves
     */
    QgsGeometry buffer( double distance, int segments ) const;

    /**
     * Returns a buffer region around the geometry, with additional style options.
     * \param distance    buffer distance
     * \param segments    for round joins, number of segments to approximate quarter-circle
     * \param endCapStyle end cap style
     * \param joinStyle   join style for corners in geometry
     * \param miterLimit  limit on the miter ratio used for very sharp corners (JoinStyleMiter only)
     * \since QGIS 2.4
     */
    QgsGeometry buffer( double distance, int segments, EndCapStyle endCapStyle, JoinStyle joinStyle, double miterLimit ) const;

    /**
     * Returns an offset line at a given distance and side from an input line.
     * \param distance    buffer distance
     * \param segments    for round joins, number of segments to approximate quarter-circle
     * \param joinStyle   join style for corners in geometry
     * \param miterLimit  limit on the miter ratio used for very sharp corners (JoinStyleMiter only)
     * \since QGIS 2.4
     */
    QgsGeometry offsetCurve( double distance, int segments, JoinStyle joinStyle, double miterLimit ) const;

    /**
     * Returns a single sided buffer for a (multi)line geometry. The buffer is only
     * applied to one side of the line.
     * \param distance buffer distance
     * \param segments for round joins, number of segments to approximate quarter-circle
     * \param side side of geometry to buffer
     * \param joinStyle join style for corners
     * \param miterLimit limit on the miter ratio used for very sharp corners
     * \returns buffered geometry, or an empty geometry if buffer could not be
     * calculated
     * \since QGIS 3.0
     */
    QgsGeometry singleSidedBuffer( double distance, int segments, BufferSide side,
                                   JoinStyle joinStyle = JoinStyleRound,
                                   double miterLimit = 2.0 ) const;

    /**
     * Extends a (multi)line geometry by extrapolating out the start or end of the line
     * by a specified distance. Lines are extended using the bearing of the first or last
     * segment in the line.
     * \since QGIS 3.0
     */
    QgsGeometry extendLine( double startDistance, double endDistance ) const;

    //! Returns a simplified version of this geometry using a specified tolerance value
    QgsGeometry simplify( double tolerance ) const;

    /**
     * Returns a copy of the geometry which has been densified by adding the specified
     * number of extra nodes within each segment of the geometry.
     * If the geometry has z or m values present then these will be linearly interpolated
     * at the added nodes.
     * Curved geometry types are automatically segmentized by this routine.
     * \since QGIS 3.0
     * \see densifyByDistance()
     */
    QgsGeometry densifyByCount( int extraNodesPerSegment ) const;

    /**
     * Densifies the geometry by adding regularly placed extra nodes inside each segment
     * so that the maximum distance between any two nodes does not exceed the
     * specified \a distance.
     * E.g. specifying a distance 3 would cause the segment [0 0] -> [10 0]
     * to be converted to [0 0] -> [2.5 0] -> [5 0] -> [7.5 0] -> [10 0], since
     * 3 extra nodes are required on the segment and spacing these at 2.5 increments
     * allows them to be evenly spaced over the segment.
     * If the geometry has z or m values present then these will be linearly interpolated
     * at the added nodes.
     * Curved geometry types are automatically segmentized by this routine.
     * \since QGIS 3.0
     * \see densifyByCount()
     */
    QgsGeometry densifyByDistance( double distance ) const;

    /**
     * Returns the center of mass of a geometry.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \note for line based geometries, the center point of the line is returned,
     * and for point based geometries, the point itself is returned
     * \see pointOnSurface()
     * \see poleOfInaccessibility()
     */
    QgsGeometry centroid() const;

    /**
     * Returns a point guaranteed to lie on the surface of a geometry. While the centroid()
     * of a geometry may be located outside of the geometry itself (e.g., for concave shapes),
     * the point on surface will always be inside the geometry.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \see centroid()
     * \see poleOfInaccessibility()
     */
    QgsGeometry pointOnSurface() const;

    /**
     * Calculates the approximate pole of inaccessibility for a surface, which is the
     * most distant internal point from the boundary of the surface. This function
     * uses the 'polylabel' algorithm (Vladimir Agafonkin, 2016), which is an iterative
     * approach guaranteed to find the true pole of inaccessibility within a specified
     * tolerance. More precise tolerances require more iterations and will take longer
     * to calculate.
     * Optionally, the distance to the polygon boundary from the pole can be stored.
     * \see centroid()
     * \see pointOnSurface()
     * \since QGIS 3.0
     */
    QgsGeometry poleOfInaccessibility( double precision, double *distanceToBoundary SIP_OUT = nullptr ) const;

    /**
     * Returns the smallest convex polygon that contains all the points in the geometry.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     */
    QgsGeometry convexHull() const;

    /**
     * Creates a Voronoi diagram for the nodes contained within the geometry.
     *
     * Returns the Voronoi polygons for the nodes contained within the geometry.
     * If \a extent is specified then it will be used as a clipping envelope for the diagram.
     * If no extent is set then the clipping envelope will be automatically calculated.
     * In either case the diagram will be clipped to the larger of the provided envelope
     * OR the envelope surrounding all input nodes.
     * The \a tolerance parameter specifies an optional snapping tolerance which can
     * be used to improve the robustness of the diagram calculation.
     * If \a edgesOnly is true than line string boundary geometries will be returned
     * instead of polygons.
     * An empty geometry will be returned if the diagram could not be calculated.
     * \since QGIS 3.0
     */
    QgsGeometry voronoiDiagram( const QgsGeometry &extent = QgsGeometry(), double tolerance = 0.0, bool edgesOnly = false ) const;

    /**
     * Returns the Delaunay triangulation for the vertices of the geometry.
     * The \a tolerance parameter specifies an optional snapping tolerance which can
     * be used to improve the robustness of the triangulation.
     * If \a edgesOnly is true than line string boundary geometries will be returned
     * instead of polygons.
     * An empty geometry will be returned if the diagram could not be calculated.
     * \since QGIS 3.0
     */
    QgsGeometry delaunayTriangulation( double tolerance = 0.0, bool edgesOnly = false ) const;

    /**
     * Subdivides the geometry. The returned geometry will be a collection containing subdivided parts
     * from the original geometry, where no part has more then the specified maximum number of nodes (\a maxNodes).
     *
     * This is useful for dividing a complex geometry into less complex parts, which are better able to be spatially
     * indexed and faster to perform further operations such as intersects on. The returned geometry parts may
     * not be valid and may contain self-intersections.
     *
     * The minimum allowed value for \a maxNodes is 8.
     *
     * Curved geometries will be segmentized before subdivision.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \since QGIS 3.0
     */
    QgsGeometry subdivide( int maxNodes = 256 ) const;

    /**
     * Returns interpolated point on line at distance.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \since QGIS 2.0
     * \see lineLocatePoint()
     */
    QgsGeometry interpolate( double distance ) const;

    /**
     * Returns a distance representing the location along this linestring of the closest point
     * on this linestring geometry to the specified point. Ie, the returned value indicates
     * how far along this linestring you need to traverse to get to the closest location
     * where this linestring comes to the specified point.
     * \param point point to seek proximity to
     * \returns distance along line, or -1 on error
     * \note only valid for linestring geometries
     * \see interpolate()
     * \since QGIS 3.0
     */
    double lineLocatePoint( const QgsGeometry &point ) const;

    /**
     * Returns the angle parallel to the linestring or polygon boundary at the specified distance
     * along the geometry. Angles are in radians, clockwise from north.
     * If the distance coincides precisely at a node then the average angle from the segment either side
     * of the node is returned.
     * \param distance distance along geometry
     * \since QGIS 3.0
     * \see angleAtVertex()
     */
    double interpolateAngle( double distance ) const;

    /**
     * Returns a geometry representing the points shared by this geometry and other.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     */
    QgsGeometry intersection( const QgsGeometry &geometry ) const;

    /**
     * Clips the geometry using the specified \a rectangle.
     *
     * Performs a fast, non-robust intersection between the geometry and
     * a \a rectangle. The returned geometry may be invalid.
     * \since QGIS 3.0
     */
    QgsGeometry clipped( const QgsRectangle &rectangle );

    /**
     * Returns a geometry representing all the points in this geometry and other (a
     * union geometry operation).
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \note this operation is not called union since its a reserved word in C++.
     */
    QgsGeometry combine( const QgsGeometry &geometry ) const;

    /**
     * Merges any connected lines in a LineString/MultiLineString geometry and
     * converts them to single line strings.
     * \returns a LineString or MultiLineString geometry, with any connected lines
     * joined. An empty geometry will be returned if the input geometry was not a
     * MultiLineString geometry.
     * \since QGIS 3.0
     */
    QgsGeometry mergeLines() const;

    /**
     * Returns a geometry representing the points making up this geometry that do not make up other.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     */
    QgsGeometry difference( const QgsGeometry &geometry ) const;

    /**
     * Returns a geometry representing the points making up this geometry that do not make up other.
     *
     * If the input is a NULL geometry, the output will also be a NULL geometry.
     *
     * If an error was encountered while creating the result, more information can be retrieved
     * by calling `error()` on the returned geometry.
     */
    QgsGeometry symDifference( const QgsGeometry &geometry ) const;

    //! Returns an extruded version of this geometry.
    QgsGeometry extrude( double x, double y );

    /** Export the geometry to WKB
     * \since QGIS 3.0
     */
    QByteArray exportToWkb() const;

    /**
     * Exports the geometry to WKT
     * \note precision parameter added in QGIS 2.4
     * \returns true in case of success and false else
     */
    QString exportToWkt( int precision = 17 ) const;

    /**
     * Exports the geometry to GeoJSON
     * \returns a QString representing the geometry as GeoJSON
     * \since QGIS 1.8
     * \note Available in Python bindings since QGIS 1.9
     * \note precision parameter added in QGIS 2.4
     */
    QString exportToGeoJSON( int precision = 17 ) const;

    /**
     * Try to convert the geometry to the requested type
     * \param destType the geometry type to be converted to
     * \param destMultipart determines if the output geometry will be multipart or not
     * \returns the converted geometry or nullptr if the conversion fails.
     * \since QGIS 2.2
     */
    QgsGeometry convertToType( QgsWkbTypes::GeometryType destType, bool destMultipart = false ) const SIP_FACTORY;

    /* Accessor functions for getting geometry data */

    /**
     * Returns contents of the geometry as a point
     * if wkbType is WKBPoint, otherwise returns [0,0]
     */
    QgsPointXY asPoint() const;

    /**
     * Returns contents of the geometry as a polyline
     * if wkbType is WKBLineString, otherwise an empty list
     */
    QgsPolyline asPolyline() const;

    /**
     * Returns contents of the geometry as a polygon
     * if wkbType is WKBPolygon, otherwise an empty list
     */
    QgsPolygon asPolygon() const;

    /**
     * Returns contents of the geometry as a multi point
     * if wkbType is WKBMultiPoint, otherwise an empty list
     */
    QgsMultiPoint asMultiPoint() const;

    /**
     * Returns contents of the geometry as a multi linestring
     * if wkbType is WKBMultiLineString, otherwise an empty list
     */
    QgsMultiPolyline asMultiPolyline() const;

    /**
     * Returns contents of the geometry as a multi polygon
     * if wkbType is WKBMultiPolygon, otherwise an empty list
     */
    QgsMultiPolygon asMultiPolygon() const;

    /**
     * Return contents of the geometry as a list of geometries
     * \since QGIS 1.1
     */
    QList<QgsGeometry> asGeometryCollection() const;

    /**
     * Returns contents of the geometry as a QPointF if wkbType is WKBPoint,
     * otherwise returns a null QPointF.
     * \since QGIS 2.7
     */
    QPointF asQPointF() const;

    /**
     * Returns contents of the geometry as a QPolygonF. If geometry is a linestring,
     * then the result will be an open QPolygonF. If the geometry is a polygon,
     * then the result will be a closed QPolygonF of the geometry's exterior ring.
     * \since QGIS 2.7
     */
    QPolygonF asQPolygonF() const;

    /**
     * Deletes a ring in polygon or multipolygon.
     * Ring 0 is outer ring and can't be deleted.
     * \returns true on success
     * \since QGIS 1.2
     */
    bool deleteRing( int ringNum, int partNum = 0 );

    /**
     * Deletes part identified by the part number
     * \returns true on success
     * \since QGIS 1.2
     */
    bool deletePart( int partNum );

    /**
     * Converts single type geometry into multitype geometry
     * e.g. a polygon into a multipolygon geometry with one polygon
     * If it is already a multipart geometry, it will return true and
     * not change the geometry.
     *
     * \returns true in case of success and false else
     */
    bool convertToMultiType();

    /**
     * Converts multi type geometry into single type geometry
     * e.g. a multipolygon into a polygon geometry. Only the first part of the
     * multi geometry will be retained.
     * If it is already a single part geometry, it will return true and
     * not change the geometry.
     *
     * \returns true in case of success and false else
     */
    bool convertToSingleType();

    /**
     * Modifies geometry to avoid intersections with the layers specified in project properties
     * \returns 0 in case of success,
     *         1 if geometry is not of polygon type,
     *         2 if avoid intersection would change the geometry type,
     *         3 other error during intersection removal
     * \param avoidIntersectionsLayers list of layers to check for intersections
     * \param ignoreFeatures possibility to give a list of features where intersections should be ignored (not available in Python bindings)
     * \since QGIS 1.5
     */
    int avoidIntersections( const QList<QgsVectorLayer *> &avoidIntersectionsLayers,
                            const QHash<QgsVectorLayer *, QSet<QgsFeatureId> > &ignoreFeatures SIP_PYARGREMOVE = ( QHash<QgsVectorLayer *, QSet<QgsFeatureId> >() ) );

    /**
     * Attempts to make an invalid geometry valid without losing vertices.
     *
     * Already-valid geometries are returned without further intervention.
     * In case of full or partial dimensional collapses, the output geometry may be a collection
     * of lower-to-equal dimension geometries or a geometry of lower dimension.
     * Single polygons may become multi-geometries in case of self-intersections.
     * It preserves Z values, but M values will be dropped.
     *
     * If an error was encountered during the process, more information can be retrieved
     * by calling `error()` on the returned geometry.
     *
     * \returns new valid QgsGeometry or null geometry on error
     *
     * \note Ported from PostGIS ST_MakeValid() and it should return equivalent results.
     *
     * \since QGIS 3.0
     */
    QgsGeometry makeValid();

    /** \ingroup core
     */
    class CORE_EXPORT Error
    {
        QString message;
        QgsPointXY location;
        bool hasLocation;

      public:
        Error()
          : message( QStringLiteral( "none" ) )
          , hasLocation( false ) {}

        explicit Error( const QString &m )
          : message( m )
          , hasLocation( false ) {}

        Error( const QString &m, const QgsPointXY &p )
          : message( m )
          , location( p )
          , hasLocation( true ) {}

        QString what() { return message; }
        QgsPointXY where() { return location; }
        bool hasWhere() { return hasLocation; }
    };

    /**
     * Available methods for validating geometries.
     * \since QGIS 3.0
     */
    enum ValidationMethod
    {
      ValidatorQgisInternal, //!< Use internal QgsGeometryValidator method
      ValidatorGeos, //!< Use GEOS validation methods
    };

    /**
     * Validates geometry and produces a list of geometry errors.
     * The \a method argument dictates which validator to utilize.
     * \since QGIS 1.5
     * \note Available in Python bindings since QGIS 1.6
     **/
    void validateGeometry( QList<QgsGeometry::Error> &errors SIP_OUT, ValidationMethod method = ValidatorQgisInternal );

    /** Compute the unary union on a list of \a geometries. May be faster than an iterative union on a set of geometries.
     * The returned geometry will be fully noded, i.e. a node will be created at every common intersection of the
     * input geometries. An empty geometry will be returned in the case of errors.
     */
    static QgsGeometry unaryUnion( const QList<QgsGeometry> &geometries );

    /**
     * Creates a GeometryCollection geometry containing possible polygons formed from the constituent
     * linework of a set of \a geometries. The input geometries must be fully noded (i.e. nodes exist
     * at every common intersection of the geometries). The easiest way to ensure this is to first
     * call unaryUnion() on the set of input geometries and then pass the result to polygonize().
     * An empty geometry will be returned in the case of errors.
     * \since QGIS 3.0
     */
    static QgsGeometry polygonize( const QList< QgsGeometry> &geometries );

    /**
     * Converts the geometry to straight line segments, if it is a curved geometry type.
     * \since QGIS 2.10
     * \see requiresConversionToStraightSegments
     */
    void convertToStraightSegment();

    /**
     * Returns true if the geometry is a curved geometry type which requires conversion to
     * display as straight line segments.
     * \since QGIS 2.10
     * \see convertToStraightSegment
     */
    bool requiresConversionToStraightSegments() const;

    /**
     * Transforms the geometry from map units to pixels in place.
     * \param mtp map to pixel transform
     * \since QGIS 2.10
     */
    void mapToPixel( const QgsMapToPixel &mtp );

    /**
     * Draws the geometry onto a QPainter
     * \param p destination QPainter
     * \since QGIS 2.10
     */
    void draw( QPainter &p ) const;

    /**
     * Calculates the vertex ID from a vertex number
     * \param nr vertex number
     * \param id reference to QgsVertexId for storing result
     * \returns true if vertex was found
     * \since QGIS 2.10
     * \see vertexNrFromVertexId
     */
    bool vertexIdFromVertexNr( int nr, QgsVertexId &id SIP_OUT ) const;

    /**
     * Returns the vertex number corresponding to a vertex idd
     * \param i vertex id
     * \returns vertex number
     * \since QGIS 2.10
     * \see vertexIdFromVertexNr
     */
    int vertexNrFromVertexId( QgsVertexId i ) const;

    /**
     * Returns an error string referring to an error that was produced
     * when this geometry was created.
     *
     * \since QGIS 3.0
     */
    QString error() const;

    /**
     * Return GEOS context handle
     * \since QGIS 2.6
     * \note not available in Python
     */
    static GEOSContextHandle_t getGEOSHandler() SIP_SKIP;

    /**
     * Construct geometry from a QPointF
     * \param point source QPointF
     * \since QGIS 2.7
     */
    static QgsGeometry fromQPointF( QPointF point );

    /**
     * Construct geometry from a QPolygonF. If the polygon is closed than
     * the resultant geometry will be a polygon, if it is open than the
     * geometry will be a polyline.
     * \param polygon source QPolygonF
     * \since QGIS 2.7
     */
    static QgsGeometry fromQPolygonF( const QPolygonF &polygon );

    /**
     * Creates a QgsPolyline from a QPolygonF.
     * \param polygon source polygon
     * \returns QgsPolyline
     * \see createPolygonFromQPolygonF
     */
    static QgsPolyline createPolylineFromQPolygonF( const QPolygonF &polygon ) SIP_FACTORY;

    /**
     * Creates a QgsPolygon from a QPolygonF.
     * \param polygon source polygon
     * \returns QgsPolygon
     * \see createPolylineFromQPolygonF
     */
    static QgsPolygon createPolygonFromQPolygonF( const QPolygonF &polygon ) SIP_FACTORY;

#ifndef SIP_RUN

    /** Compares two polylines for equality within a specified tolerance.
     * \param p1 first polyline
     * \param p2 second polyline
     * \param epsilon maximum difference for coordinates between the polylines
     * \returns true if polylines have the same number of points and all
     * points are equal within the specified tolerance
     * \since QGIS 2.9
     */
    static bool compare( const QgsPolyline &p1, const QgsPolyline &p2,
                         double epsilon = 4 * std::numeric_limits<double>::epsilon() );

    /**
     * Compares two polygons for equality within a specified tolerance.
     * \param p1 first polygon
     * \param p2 second polygon
     * \param epsilon maximum difference for coordinates between the polygons
     * \returns true if polygons have the same number of rings, and each ring has the same
     * number of points and all points are equal within the specified tolerance
     * \since QGIS 2.9
     */
    static bool compare( const QgsPolygon &p1, const QgsPolygon &p2,
                         double epsilon = 4 * std::numeric_limits<double>::epsilon() );

    /**
     * Compares two multipolygons for equality within a specified tolerance.
     * \param p1 first multipolygon
     * \param p2 second multipolygon
     * \param epsilon maximum difference for coordinates between the multipolygons
     * \returns true if multipolygons have the same number of polygons, the polygons have the same number
     * of rings, and each ring has the same number of points and all points are equal within the specified
     * tolerance
     * \since QGIS 2.9
     */
    static bool compare( const QgsMultiPolygon &p1, const QgsMultiPolygon &p2,
                         double epsilon = 4 * std::numeric_limits<double>::epsilon() );
#else

    /**
     * Compares two geometry objects for equality within a specified tolerance.
     * The objects can be of type QgsPolyline, QgsPolygon or QgsMultiPolygon.
     * The 2 types should match.
     * \param p1 first geometry object
     * \param p2 second geometry object
     * \param epsilon maximum difference for coordinates between the objects
     * \returns true if objects are
     *   - polylines and have the same number of points and all
     *     points are equal within the specified tolerance
     *   - polygons and have the same number of points and all
     *     points are equal within the specified tolerance
     *   - multipolygons and  have the same number of polygons, the polygons have the same number
     *     of rings, and each ring has the same number of points and all points are equal
     *     within the specified
     * tolerance
     * \since QGIS 2.9
     */
    static bool compare( PyObject *obj1, PyObject *obj2, double epsilon = 4 * DBL_EPSILON );
    % MethodCode
    {
      sipRes = false;
      int state0;
      int state1;
      int sipIsErr = 0;

      if ( PyList_Check( a0 ) && PyList_Check( a1 ) &&
           PyList_GET_SIZE( a0 ) && PyList_GET_SIZE( a1 ) )
      {
        PyObject *o0 = PyList_GetItem( a0, 0 );
        PyObject *o1 = PyList_GetItem( a1, 0 );
        if ( o0 && o1 )
        {
          // compare polyline - polyline
          if ( sipCanConvertToType( o0, sipType_QgsPointXY, SIP_NOT_NONE ) &&
               sipCanConvertToType( o1, sipType_QgsPointXY, SIP_NOT_NONE ) &&
               sipCanConvertToType( a0, sipType_QVector_0100QgsPointXY, SIP_NOT_NONE ) &&
               sipCanConvertToType( a1, sipType_QVector_0100QgsPointXY, SIP_NOT_NONE ) )
          {
            QgsPolyline *p0;
            QgsPolyline *p1;
            p0 = reinterpret_cast<QgsPolyline *>( sipConvertToType( a0, sipType_QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state0, &sipIsErr ) );
            p1 = reinterpret_cast<QgsPolyline *>( sipConvertToType( a1, sipType_QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state1, &sipIsErr ) );
            if ( sipIsErr )
            {
              sipReleaseType( p0, sipType_QVector_0100QgsPointXY, state0 );
              sipReleaseType( p1, sipType_QVector_0100QgsPointXY, state1 );
            }
            else
            {
              sipRes = QgsGeometry::compare( *p0, *p1, a2 );
            }
          }
          else if ( PyList_Check( o0 ) && PyList_Check( o1 ) &&
                    PyList_GET_SIZE( o0 ) && PyList_GET_SIZE( o1 ) )
          {
            PyObject *oo0 = PyList_GetItem( o0, 0 );
            PyObject *oo1 = PyList_GetItem( o1, 0 );
            if ( oo0 && oo1 )
            {
              // compare polygon - polygon
              if ( sipCanConvertToType( oo0, sipType_QgsPointXY, SIP_NOT_NONE ) &&
                   sipCanConvertToType( oo1, sipType_QgsPointXY, SIP_NOT_NONE ) &&
                   sipCanConvertToType( a0, sipType_QVector_0600QVector_0100QgsPointXY, SIP_NOT_NONE ) &&
                   sipCanConvertToType( a1, sipType_QVector_0600QVector_0100QgsPointXY, SIP_NOT_NONE ) )
              {
                QgsPolygon *p0;
                QgsPolygon *p1;
                p0 = reinterpret_cast<QgsPolygon *>( sipConvertToType( a0, sipType_QVector_0600QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state0, &sipIsErr ) );
                p1 = reinterpret_cast<QgsPolygon *>( sipConvertToType( a1, sipType_QVector_0600QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state1, &sipIsErr ) );
                if ( sipIsErr )
                {
                  sipReleaseType( p0, sipType_QVector_0600QVector_0100QgsPointXY, state0 );
                  sipReleaseType( p1, sipType_QVector_0600QVector_0100QgsPointXY, state1 );
                }
                else
                {
                  sipRes = QgsGeometry::compare( *p0, *p1, a2 );
                }
              }
              else if ( PyList_Check( oo0 ) && PyList_Check( oo1 ) &&
                        PyList_GET_SIZE( oo0 ) && PyList_GET_SIZE( oo1 ) )
              {
                PyObject *ooo0 = PyList_GetItem( oo0, 0 );
                PyObject *ooo1 = PyList_GetItem( oo1, 0 );
                if ( ooo0 && ooo1 )
                {
                  // compare multipolygon - multipolygon
                  if ( sipCanConvertToType( ooo0, sipType_QgsPointXY, SIP_NOT_NONE ) &&
                       sipCanConvertToType( ooo1, sipType_QgsPointXY, SIP_NOT_NONE ) &&
                       sipCanConvertToType( a0, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, SIP_NOT_NONE ) &&
                       sipCanConvertToType( a1, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, SIP_NOT_NONE ) )
                  {
                    QgsMultiPolygon *p0;
                    QgsMultiPolygon *p1;
                    p0 = reinterpret_cast<QgsMultiPolygon *>( sipConvertToType( a0, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state0, &sipIsErr ) );
                    p1 = reinterpret_cast<QgsMultiPolygon *>( sipConvertToType( a1, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, 0, SIP_NOT_NONE, &state1, &sipIsErr ) );
                    if ( sipIsErr )
                    {
                      sipReleaseType( p0, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, state0 );
                      sipReleaseType( p1, sipType_QVector_0600QVector_0600QVector_0100QgsPointXY, state1 );
                    }
                    else
                    {
                      sipRes = QgsGeometry::compare( *p0, *p1, a2 );
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
    % End
#endif

    /**
     * Smooths a geometry by rounding off corners using the Chaikin algorithm. This operation
     * roughly doubles the number of vertices in a geometry.
     * \param iterations number of smoothing iterations to run. More iterations results
     * in a smoother geometry
     * \param offset fraction of line to create new vertices along, between 0 and 1.0,
     * e.g., the default value of 0.25 will create new vertices 25% and 75% along each line segment
     * of the geometry for each iteration. Smaller values result in "tighter" smoothing.
     * \param minimumDistance minimum segment length to apply smoothing to
     * \param maxAngle maximum angle at node (0-180) at which smoothing will be applied
     * \since QGIS 2.9
     */
    QgsGeometry smooth( const unsigned int iterations = 1, const double offset = 0.25,
                        double minimumDistance = -1.0, double maxAngle = 180.0 ) const;

    /**
     * Creates and returns a new geometry engine
     */
    static QgsGeometryEngine *createGeometryEngine( const QgsAbstractGeometry *geometry ) SIP_FACTORY;

    /**
     * Upgrades a point list from QgsPointXY to QgsPointV2
     * \param input list of QgsPointXY objects to be upgraded
     * \param output destination for list of points converted to QgsPointV2
     */
    static void convertPointList( const QList<QgsPointXY> &input, QgsPointSequence &output );

    /**
     * Downgrades a point list from QgsPoint to QgsPoint
     * \param input list of QgsPoint objects to be downgraded
     * \param output destination for list of points converted to QgsPoint
     */
    static void convertPointList( const QgsPointSequence &input, QList<QgsPointXY> &output );

    //! Allows direct construction of QVariants from geometry.
    operator QVariant() const
    {
      return QVariant::fromValue( *this );
    }

    /**
     * Returns true if the geometry is non empty (ie, isNull() returns false),
     * or false if it is an empty, uninitialized geometry (ie, isNull() returns true).
     * \since QGIS 3.0
     */
    operator bool() const;

  private:

    QgsGeometryPrivate *d; //implicitly shared data pointer

    void detach( bool cloneGeom = true ); //make sure mGeometry only referenced from this instance

    static void convertToPolyline( const QgsPointSequence &input, QgsPolyline &output );
    static void convertPolygon( const QgsPolygonV2 &input, QgsPolygon &output );

    //! Try to convert the geometry to a point
    QgsGeometry convertToPoint( bool destMultipart ) const;
    //! Try to convert the geometry to a line
    QgsGeometry convertToLine( bool destMultipart ) const;
    //! Try to convert the geometry to a polygon
    QgsGeometry convertToPolygon( bool destMultipart ) const;

    /** Smooths a polyline using the Chaikin algorithm
     * \param line line to smooth
     * \param iterations number of smoothing iterations to run. More iterations results
     * in a smoother geometry
     * \param offset fraction of line to create new vertices along, between 0 and 1.0,
     * e.g., the default value of 0.25 will create new vertices 25% and 75% along each line segment
     * of the geometry for each iteration. Smaller values result in "tighter" smoothing.
     * \param minimumDistance minimum segment length to apply smoothing to
     * \param maxAngle maximum angle at node (0-180) at which smoothing will be applied
    */
    QgsLineString *smoothLine( const QgsLineString &line, const unsigned int iterations = 1, const double offset = 0.25,
                               double minimumDistance = -1, double maxAngle = 180.0 ) const;

    /** Smooths a polygon using the Chaikin algorithm
     * \param polygon polygon to smooth
     * \param iterations number of smoothing iterations to run. More iterations results
     * in a smoother geometry
     * \param offset fraction of segment to create new vertices along, between 0 and 1.0,
     * e.g., the default value of 0.25 will create new vertices 25% and 75% along each line segment
     * of the geometry for each iteration. Smaller values result in "tighter" smoothing.
     * \param minimumDistance minimum segment length to apply smoothing to
     * \param maxAngle maximum angle at node (0-180) at which smoothing will be applied
    */
    QgsPolygonV2 *smoothPolygon( const QgsPolygonV2 &polygon, const unsigned int iterations = 1, const double offset = 0.25,
                                 double minimumDistance = -1, double maxAngle = 180.0 ) const;


}; // class QgsGeometry

Q_DECLARE_METATYPE( QgsGeometry )

//! Writes the geometry to stream out. QGIS version compatibility is not guaranteed.
CORE_EXPORT QDataStream &operator<<( QDataStream &out, const QgsGeometry &geometry );
//! Reads a geometry from stream in into geometry. QGIS version compatibility is not guaranteed.
CORE_EXPORT QDataStream &operator>>( QDataStream &in, QgsGeometry &geometry );

#endif
