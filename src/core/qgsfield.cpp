/***************************************************************************
       qgsfield.cpp - Describes a field in a layer or table
        --------------------------------------
       Date                 : 01-Jan-2004
       Copyright            : (C) 2004 by Gary E.Sherman
       email                : sherman at mrcc.com

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsfields.h"
#include "qgsfield_p.h"
#include "qgis.h"
#include "qgsapplication.h"
#include "qgssettings.h"

#include <QDataStream>
#include <QIcon>

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

#if 0
QgsField::QgsField( QString nam, QString typ, int len, int prec, bool num,
                    QString comment )
  : mName( nam ), mType( typ ), mLength( len ), mPrecision( prec ), mNumeric( num )
  , mComment( comment )
{
  // This function used to lower case the field name since some stores
  // use upper case (e.g., shapefiles), but that caused problems with
  // attribute actions getting confused between uppercase and
  // lowercase versions of the attribute names, so just leave the
  // names how they are now.
}
#endif
QgsField::QgsField( const QString &name, QVariant::Type type,
                    const QString &typeName, int len, int prec, const QString &comment,
                    QVariant::Type subType )
{
  d = new QgsFieldPrivate( name, type, subType, typeName, len, prec, comment );
}

QgsField::QgsField( const QgsField &other ) //NOLINT
  : d( other.d )
{

}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

QgsField &QgsField::operator =( const QgsField &other )  //NOLINT
{
  d = other.d;
  return *this;
}

bool QgsField::operator==( const QgsField &other ) const
{
  return *( other.d ) == *d;
}

bool QgsField::operator!=( const QgsField &other ) const
{
  return !( *this == other );
}

QString QgsField::name() const
{
  return d->name;
}

QString QgsField::displayName() const
{
  if ( !d->alias.isEmpty() )
    return d->alias;
  else
    return d->name;
}

QVariant::Type QgsField::type() const
{
  return d->type;
}

QVariant::Type QgsField::subType() const
{
  return d->subType;
}

QString QgsField::typeName() const
{
  return d->typeName;
}

int QgsField::length() const
{
  return d->length;
}

int QgsField::precision() const
{
  return d->precision;
}

QString QgsField::comment() const
{
  return d->comment;
}

bool QgsField::isNumeric() const
{
  return d->type == QVariant::Double || d->type == QVariant::Int || d->type == QVariant::UInt || d->type == QVariant::LongLong || d->type == QVariant::ULongLong;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

void QgsField::setName( const QString &name )
{
  d->name = name;
}

void QgsField::setType( QVariant::Type type )
{
  d->type = type;
}

void QgsField::setSubType( QVariant::Type subType )
{
  d->subType = subType;
}

void QgsField::setTypeName( const QString &typeName )
{
  d->typeName = typeName;
}

void QgsField::setLength( int len )
{
  d->length = len;
}
void QgsField::setPrecision( int precision )
{
  d->precision = precision;
}

void QgsField::setComment( const QString &comment )
{
  d->comment = comment;
}

QString QgsField::defaultValueExpression() const
{
  return d->defaultValueExpression;
}

void QgsField::setDefaultValueExpression( const QString &expression )
{
  d->defaultValueExpression = expression;
}

void QgsField::setConstraints( const QgsFieldConstraints &constraints )
{
  d->constraints = constraints;
}

const QgsFieldConstraints &QgsField::constraints() const
{
  return d->constraints;
}

QString QgsField::alias() const
{
  return d->alias;
}

void QgsField::setAlias( const QString &alias )
{
  d->alias = alias;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

QString QgsField::displayString( const QVariant &v ) const
{
  if ( v.isNull() )
  {
    QgsSettings settings;
    return QgsApplication::nullRepresentation();
  }

  if ( d->type == QVariant::Double && d->precision > 0 )
    return QString::number( v.toDouble(), 'f', d->precision );

  return v.toString();
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

bool QgsField::convertCompatible( QVariant &v ) const
{
  if ( v.isNull() )
  {
    v.convert( d->type );
    return true;
  }

  if ( d->type == QVariant::Int && v.toInt() != v.toLongLong() )
  {
    v = QVariant( d->type );
    return false;
  }

  //String representations of doubles in QVariant will return false to convert( QVariant::Int )
  //work around this by first converting to double, and then checking whether the double is convertible to int
  if ( d->type == QVariant::Int && v.canConvert( QVariant::Double ) )
  {
    bool ok = false;
    double dbl = v.toDouble( &ok );
    if ( !ok )
    {
      //couldn't convert to number
      v = QVariant( d->type );
      return false;
    }

    double round = std::round( dbl );
    if ( round  > INT_MAX || round < -INT_MAX )
    {
      //double too large to fit in int
      v = QVariant( d->type );
      return false;
    }
    v = QVariant( static_cast< int >( std::round( dbl ) ) );
    return true;
  }

  if ( !v.convert( d->type ) )
  {
    v = QVariant( d->type );
    return false;
  }

  if ( d->type == QVariant::Double && d->precision > 0 )
  {
    double s = std::pow( 10, d->precision );
    double d = v.toDouble() * s;
    v = QVariant( ( d < 0 ? std::ceil( d - 0.5 ) : std::floor( d + 0.5 ) ) / s );
    return true;
  }

  if ( d->type == QVariant::String && d->length > 0 && v.toString().length() > d->length )
  {
    v = v.toString().left( d->length );
    return false;
  }

  return true;
}

void QgsField::setEditorWidgetSetup( const QgsEditorWidgetSetup &v )
{
  d->editorWidgetSetup = v;
}

QgsEditorWidgetSetup QgsField::editorWidgetSetup() const
{
  return d->editorWidgetSetup;
}

/***************************************************************************
 * This class is considered CRITICAL and any change MUST be accompanied with
 * full unit tests in testqgsfield.cpp.
 * See details in QEP #17
 ****************************************************************************/

QDataStream &operator<<( QDataStream &out, const QgsField &field )
{
  out << field.name();
  out << static_cast< quint32 >( field.type() );
  out << field.typeName();
  out << field.length();
  out << field.precision();
  out << field.comment();
  out << field.alias();
  out << field.defaultValueExpression();
  out << field.constraints().constraints();
  out << static_cast< quint32 >( field.constraints().constraintOrigin( QgsFieldConstraints::ConstraintNotNull ) );
  out << static_cast< quint32 >( field.constraints().constraintOrigin( QgsFieldConstraints::ConstraintUnique ) );
  out << static_cast< quint32 >( field.constraints().constraintOrigin( QgsFieldConstraints::ConstraintExpression ) );
  out << static_cast< quint32 >( field.constraints().constraintStrength( QgsFieldConstraints::ConstraintNotNull ) );
  out << static_cast< quint32 >( field.constraints().constraintStrength( QgsFieldConstraints::ConstraintUnique ) );
  out << static_cast< quint32 >( field.constraints().constraintStrength( QgsFieldConstraints::ConstraintExpression ) );
  out << field.constraints().constraintExpression();
  out << field.constraints().constraintDescription();
  out << static_cast< quint32 >( field.subType() );
  return out;
}

QDataStream &operator>>( QDataStream &in, QgsField &field )
{
  quint32 type, subType, length, precision, constraints, originNotNull, originUnique, originExpression, strengthNotNull, strengthUnique, strengthExpression;
  QString name, typeName, comment, alias, defaultValueExpression, constraintExpression, constraintDescription;
  in >> name >> type >> typeName >> length >> precision >> comment >> alias
     >> defaultValueExpression >> constraints >> originNotNull >> originUnique >> originExpression >> strengthNotNull >> strengthUnique >> strengthExpression >>
     constraintExpression >> constraintDescription >> subType;
  field.setName( name );
  field.setType( static_cast< QVariant::Type >( type ) );
  field.setTypeName( typeName );
  field.setLength( static_cast< int >( length ) );
  field.setPrecision( static_cast< int >( precision ) );
  field.setComment( comment );
  field.setAlias( alias );
  field.setDefaultValueExpression( defaultValueExpression );
  QgsFieldConstraints fieldConstraints;
  if ( constraints & QgsFieldConstraints::ConstraintNotNull )
  {
    fieldConstraints.setConstraint( QgsFieldConstraints::ConstraintNotNull, static_cast< QgsFieldConstraints::ConstraintOrigin>( originNotNull ) );
    fieldConstraints.setConstraintStrength( QgsFieldConstraints::ConstraintNotNull, static_cast< QgsFieldConstraints::ConstraintStrength>( strengthNotNull ) );
  }
  else
    fieldConstraints.removeConstraint( QgsFieldConstraints::ConstraintNotNull );
  if ( constraints & QgsFieldConstraints::ConstraintUnique )
  {
    fieldConstraints.setConstraint( QgsFieldConstraints::ConstraintUnique, static_cast< QgsFieldConstraints::ConstraintOrigin>( originUnique ) );
    fieldConstraints.setConstraintStrength( QgsFieldConstraints::ConstraintUnique, static_cast< QgsFieldConstraints::ConstraintStrength>( strengthUnique ) );
  }
  else
    fieldConstraints.removeConstraint( QgsFieldConstraints::ConstraintUnique );
  if ( constraints & QgsFieldConstraints::ConstraintExpression )
  {
    fieldConstraints.setConstraint( QgsFieldConstraints::ConstraintExpression, static_cast< QgsFieldConstraints::ConstraintOrigin>( originExpression ) );
    fieldConstraints.setConstraintStrength( QgsFieldConstraints::ConstraintExpression, static_cast< QgsFieldConstraints::ConstraintStrength>( strengthExpression ) );
  }
  else
    fieldConstraints.removeConstraint( QgsFieldConstraints::ConstraintExpression );
  fieldConstraints.setConstraintExpression( constraintExpression, constraintDescription );
  field.setConstraints( fieldConstraints );
  field.setSubType( static_cast< QVariant::Type >( subType ) );
  return in;
}
