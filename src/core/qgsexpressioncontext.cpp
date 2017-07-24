/***************************************************************************
     qgsexpressioncontext.cpp
     ------------------------
    Date                 : April 2015
    Copyright            : (C) 2015 by Nyall Dawson
    Email                : nyall dot dawson at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsexpressioncontext.h"

#include "qgslogger.h"
#include "qgsexpression.h"
#include "qgsexpressionfunction.h"
#include "qgsfields.h"
#include "qgsvectorlayer.h"
#include "qgsproject.h"
#include "qgssymbollayerutils.h"
#include "qgsgeometry.h"
#include "qgscomposition.h"
#include "qgscomposeritem.h"
#include "qgsatlascomposition.h"
#include "qgsapplication.h"
#include "qgsmapsettings.h"
#include "qgsmaplayerlistutils.h"
#include "qgsprocessingcontext.h"
#include "qgsprocessingalgorithm.h"
#include "qgslayout.h"

#include <QSettings>
#include <QDir>


const QString QgsExpressionContext::EXPR_FIELDS( QStringLiteral( "_fields_" ) );
const QString QgsExpressionContext::EXPR_ORIGINAL_VALUE( QStringLiteral( "value" ) );
const QString QgsExpressionContext::EXPR_SYMBOL_COLOR( QStringLiteral( "symbol_color" ) );
const QString QgsExpressionContext::EXPR_SYMBOL_ANGLE( QStringLiteral( "symbol_angle" ) );
const QString QgsExpressionContext::EXPR_GEOMETRY_PART_COUNT( QStringLiteral( "geometry_part_count" ) );
const QString QgsExpressionContext::EXPR_GEOMETRY_PART_NUM( QStringLiteral( "geometry_part_num" ) );
const QString QgsExpressionContext::EXPR_GEOMETRY_POINT_COUNT( QStringLiteral( "geometry_point_count" ) );
const QString QgsExpressionContext::EXPR_GEOMETRY_POINT_NUM( QStringLiteral( "geometry_point_num" ) );
const QString QgsExpressionContext::EXPR_CLUSTER_SIZE( QStringLiteral( "cluster_size" ) );
const QString QgsExpressionContext::EXPR_CLUSTER_COLOR( QStringLiteral( "cluster_color" ) );

//
// QgsExpressionContextScope
//

QgsExpressionContextScope::QgsExpressionContextScope( const QString &name )
  : mName( name )
{

}

QgsExpressionContextScope::QgsExpressionContextScope( const QgsExpressionContextScope &other )
  : mName( other.mName )
  , mVariables( other.mVariables )
  , mHasFeature( other.mHasFeature )
  , mFeature( other.mFeature )
{
  QHash<QString, QgsScopedExpressionFunction * >::const_iterator it = other.mFunctions.constBegin();
  for ( ; it != other.mFunctions.constEnd(); ++it )
  {
    mFunctions.insert( it.key(), it.value()->clone() );
  }
}

QgsExpressionContextScope &QgsExpressionContextScope::operator=( const QgsExpressionContextScope &other )
{
  mName = other.mName;
  mVariables = other.mVariables;
  mHasFeature = other.mHasFeature;
  mFeature = other.mFeature;

  qDeleteAll( mFunctions );
  mFunctions.clear();
  QHash<QString, QgsScopedExpressionFunction * >::const_iterator it = other.mFunctions.constBegin();
  for ( ; it != other.mFunctions.constEnd(); ++it )
  {
    mFunctions.insert( it.key(), it.value()->clone() );
  }

  return *this;
}

QgsExpressionContextScope::~QgsExpressionContextScope()
{
  qDeleteAll( mFunctions );
}

void QgsExpressionContextScope::setVariable( const QString &name, const QVariant &value, bool isStatic )
{
  if ( mVariables.contains( name ) )
  {
    StaticVariable existing = mVariables.value( name );
    existing.value = value;
    existing.isStatic = isStatic;
    addVariable( existing );
  }
  else
  {
    addVariable( QgsExpressionContextScope::StaticVariable( name, value, false, isStatic ) );
  }
}

void QgsExpressionContextScope::addVariable( const QgsExpressionContextScope::StaticVariable &variable )
{
  mVariables.insert( variable.name, variable );
}

bool QgsExpressionContextScope::removeVariable( const QString &name )
{
  return mVariables.remove( name ) > 0;
}

bool QgsExpressionContextScope::hasVariable( const QString &name ) const
{
  return mVariables.contains( name );
}

QVariant QgsExpressionContextScope::variable( const QString &name ) const
{
  return hasVariable( name ) ? mVariables.value( name ).value : QVariant();
}

QStringList QgsExpressionContextScope::variableNames() const
{
  QStringList names = mVariables.keys();
  return names;
}

bool QgsExpressionContextScope::variableNameSort( const QString &a, const QString &b )
{
  return QString::localeAwareCompare( a, b ) < 0;
}

/// @cond PRIVATE
class QgsExpressionContextVariableCompare
{
  public:
    explicit QgsExpressionContextVariableCompare( const QgsExpressionContextScope &scope )
      : mScope( scope )
    {  }

    bool operator()( const QString &a, const QString &b ) const
    {
      bool aReadOnly = mScope.isReadOnly( a );
      bool bReadOnly = mScope.isReadOnly( b );
      if ( aReadOnly != bReadOnly )
        return aReadOnly;
      return QString::localeAwareCompare( a, b ) < 0;
    }

  private:
    const QgsExpressionContextScope &mScope;
};
/// @endcond

QStringList QgsExpressionContextScope::filteredVariableNames() const
{
  QStringList allVariables = mVariables.keys();
  QStringList filtered;
  Q_FOREACH ( const QString &variable, allVariables )
  {
    if ( variable.startsWith( '_' ) )
      continue;

    filtered << variable;
  }
  QgsExpressionContextVariableCompare cmp( *this );
  std::sort( filtered.begin(), filtered.end(), cmp );

  return filtered;
}

bool QgsExpressionContextScope::isReadOnly( const QString &name ) const
{
  return hasVariable( name ) ? mVariables.value( name ).readOnly : false;
}

bool QgsExpressionContextScope::isStatic( const QString &name ) const
{
  return hasVariable( name ) ? mVariables.value( name ).isStatic : false;
}

QString QgsExpressionContextScope::description( const QString &name ) const
{
  return hasVariable( name ) ? mVariables.value( name ).description : QString();
}

bool QgsExpressionContextScope::hasFunction( const QString &name ) const
{
  return mFunctions.contains( name );
}

QgsExpressionFunction *QgsExpressionContextScope::function( const QString &name ) const
{
  return mFunctions.contains( name ) ? mFunctions.value( name ) : nullptr;
}

QStringList QgsExpressionContextScope::functionNames() const
{
  return mFunctions.keys();
}

void QgsExpressionContextScope::addFunction( const QString &name, QgsScopedExpressionFunction *function )
{
  mFunctions.insert( name, function );
}


void QgsExpressionContextScope::setFields( const QgsFields &fields )
{
  addVariable( StaticVariable( QgsExpressionContext::EXPR_FIELDS, QVariant::fromValue( fields ), true ) );
}


//
// QgsExpressionContext
//

QgsExpressionContext::QgsExpressionContext( const QList<QgsExpressionContextScope *> &scopes )
  : mStack( scopes )
{
}

QgsExpressionContext::QgsExpressionContext( const QgsExpressionContext &other )
{
  Q_FOREACH ( const QgsExpressionContextScope *scope, other.mStack )
  {
    mStack << new QgsExpressionContextScope( *scope );
  }
  mHighlightedVariables = other.mHighlightedVariables;
  mCachedValues = other.mCachedValues;
}

QgsExpressionContext &QgsExpressionContext::operator=( QgsExpressionContext &&other ) noexcept
{
  if ( this != &other )
  {
    qDeleteAll( mStack );
    // move the stack over
    mStack = other.mStack;
    other.mStack.clear();

    mHighlightedVariables = other.mHighlightedVariables;
    mCachedValues = other.mCachedValues;
  }
  return *this;
}

QgsExpressionContext &QgsExpressionContext::operator=( const QgsExpressionContext &other )
{
  qDeleteAll( mStack );
  mStack.clear();
  Q_FOREACH ( const QgsExpressionContextScope *scope, other.mStack )
  {
    mStack << new QgsExpressionContextScope( *scope );
  }
  mHighlightedVariables = other.mHighlightedVariables;
  mCachedValues = other.mCachedValues;
  return *this;
}

QgsExpressionContext::~QgsExpressionContext()
{
  qDeleteAll( mStack );
  mStack.clear();
}

bool QgsExpressionContext::hasVariable( const QString &name ) const
{
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    if ( scope->hasVariable( name ) )
      return true;
  }
  return false;
}

QVariant QgsExpressionContext::variable( const QString &name ) const
{
  const QgsExpressionContextScope *scope = activeScopeForVariable( name );
  return scope ? scope->variable( name ) : QVariant();
}

QVariantMap QgsExpressionContext::variablesToMap() const
{
  QStringList names = variableNames();
  QVariantMap m;
  Q_FOREACH ( const QString &name, names )
  {
    m.insert( name, variable( name ) );
  }
  return m;
}

bool QgsExpressionContext::isHighlightedVariable( const QString &name ) const
{
  return mHighlightedVariables.contains( name );
}

void QgsExpressionContext::setHighlightedVariables( const QStringList &variableNames )
{
  mHighlightedVariables = variableNames;
}

const QgsExpressionContextScope *QgsExpressionContext::activeScopeForVariable( const QString &name ) const
{
  //iterate through stack backwards, so that higher priority variables take precedence
  QList< QgsExpressionContextScope * >::const_iterator it = mStack.constEnd();
  while ( it != mStack.constBegin() )
  {
    --it;
    if ( ( *it )->hasVariable( name ) )
      return ( *it );
  }
  return nullptr;
}

QgsExpressionContextScope *QgsExpressionContext::activeScopeForVariable( const QString &name )
{
  //iterate through stack backwards, so that higher priority variables take precedence
  QList< QgsExpressionContextScope * >::const_iterator it = mStack.constEnd();
  while ( it != mStack.constBegin() )
  {
    --it;
    if ( ( *it )->hasVariable( name ) )
      return ( *it );
  }
  return nullptr;
}

QgsExpressionContextScope *QgsExpressionContext::scope( int index )
{
  if ( index < 0 || index >= mStack.count() )
    return nullptr;

  return mStack.at( index );
}

QgsExpressionContextScope *QgsExpressionContext::lastScope()
{
  if ( mStack.count() < 1 )
    return nullptr;

  return mStack.last();
}

int QgsExpressionContext::indexOfScope( QgsExpressionContextScope *scope ) const
{
  if ( !scope )
    return -1;

  return mStack.indexOf( scope );
}

int QgsExpressionContext::indexOfScope( const QString &scopeName ) const
{
  int index = 0;
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    if ( scope->name() == scopeName )
      return index;

    index++;
  }
  return -1;
}

QStringList QgsExpressionContext::variableNames() const
{
  QStringList names;
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    names << scope->variableNames();
  }
  return names.toSet().toList();
}

QStringList QgsExpressionContext::filteredVariableNames() const
{
  QStringList allVariables = variableNames();
  QStringList filtered;
  Q_FOREACH ( const QString &variable, allVariables )
  {
    if ( variable.startsWith( '_' ) )
      continue;

    filtered << variable;
  }

  filtered.sort();
  return filtered;
}

bool QgsExpressionContext::isReadOnly( const QString &name ) const
{
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    if ( scope->isReadOnly( name ) )
      return true;
  }
  return false;
}

QString QgsExpressionContext::description( const QString &name ) const
{
  const QgsExpressionContextScope *scope = activeScopeForVariable( name );
  return ( scope && !scope->description( name ).isEmpty() ) ? scope->description( name ) : QgsExpression::variableHelpText( name );
}

bool QgsExpressionContext::hasFunction( const QString &name ) const
{
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    if ( scope->hasFunction( name ) )
      return true;
  }
  return false;
}

QStringList QgsExpressionContext::functionNames() const
{
  QStringList result;
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    result << scope->functionNames();
  }
  result = result.toSet().toList();
  result.sort();
  return result;
}

QgsExpressionFunction *QgsExpressionContext::function( const QString &name ) const
{
  //iterate through stack backwards, so that higher priority variables take precedence
  QList< QgsExpressionContextScope * >::const_iterator it = mStack.constEnd();
  while ( it != mStack.constBegin() )
  {
    --it;
    if ( ( *it )->hasFunction( name ) )
      return ( *it )->function( name );
  }
  return nullptr;
}

int QgsExpressionContext::scopeCount() const
{
  return mStack.count();
}

void QgsExpressionContext::appendScope( QgsExpressionContextScope *scope )
{
  mStack.append( scope );
}

void QgsExpressionContext::appendScopes( const QList<QgsExpressionContextScope *> &scopes )
{
  mStack.append( scopes );
}

QgsExpressionContextScope *QgsExpressionContext::popScope()
{
  if ( !mStack.isEmpty() )
    return mStack.takeLast();

  return nullptr;
}

QgsExpressionContext &QgsExpressionContext::operator<<( QgsExpressionContextScope *scope )
{
  mStack.append( scope );
  return *this;
}

void QgsExpressionContext::setFeature( const QgsFeature &feature )
{
  if ( mStack.isEmpty() )
    mStack.append( new QgsExpressionContextScope() );

  mStack.last()->setFeature( feature );
}

bool QgsExpressionContext::hasFeature() const
{
  Q_FOREACH ( const QgsExpressionContextScope *scope, mStack )
  {
    if ( scope->hasFeature() )
      return true;
  }
  return false;
}

QgsFeature QgsExpressionContext::feature() const
{
  //iterate through stack backwards, so that higher priority variables take precedence
  QList< QgsExpressionContextScope * >::const_iterator it = mStack.constEnd();
  while ( it != mStack.constBegin() )
  {
    --it;
    if ( ( *it )->hasFeature() )
      return ( *it )->feature();
  }
  return QgsFeature();
}

void QgsExpressionContext::setFields( const QgsFields &fields )
{
  if ( mStack.isEmpty() )
    mStack.append( new QgsExpressionContextScope() );

  mStack.last()->setFields( fields );
}

QgsFields QgsExpressionContext::fields() const
{
  return qvariant_cast<QgsFields>( variable( QgsExpressionContext::EXPR_FIELDS ) );
}

void QgsExpressionContext::setOriginalValueVariable( const QVariant &value )
{
  if ( mStack.isEmpty() )
    mStack.append( new QgsExpressionContextScope() );

  mStack.last()->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_ORIGINAL_VALUE,
                              value, true ) );
}

void QgsExpressionContext::setCachedValue( const QString &key, const QVariant &value ) const
{
  mCachedValues.insert( key, value );
}

bool QgsExpressionContext::hasCachedValue( const QString &key ) const
{
  return mCachedValues.contains( key );
}

QVariant QgsExpressionContext::cachedValue( const QString &key ) const
{
  return mCachedValues.value( key, QVariant() );
}

void QgsExpressionContext::clearCachedValues() const
{
  mCachedValues.clear();
}


//
// QgsExpressionContextUtils
//

QgsExpressionContextScope *QgsExpressionContextUtils::globalScope()
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Global" ) );

  QVariantMap customVariables = QgsApplication::customVariables();

  for ( QVariantMap::const_iterator it = customVariables.constBegin(); it != customVariables.constEnd(); ++it )
  {
    scope->setVariable( it.key(), it.value(), true );
  }

  //add some extra global variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_version" ), Qgis::QGIS_VERSION, true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_version_no" ), Qgis::QGIS_VERSION_INT, true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_short_version" ), QStringLiteral( "%1.%2" ).arg( Qgis::QGIS_VERSION_INT / 10000 ).arg( Qgis::QGIS_VERSION_INT / 100 % 100 ), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_release_name" ), Qgis::QGIS_RELEASE_NAME, true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_platform" ), QgsApplication::platform(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_os_name" ), QgsApplication::osName(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "qgis_locale" ), QgsApplication::locale(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "user_account_name" ), QgsApplication::userLoginName(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "user_full_name" ), QgsApplication::userFullName(), true, true ) );

  return scope;
}

void QgsExpressionContextUtils::setGlobalVariable( const QString &name, const QVariant &value )
{
  QgsApplication::setCustomVariable( name, value );
}

void QgsExpressionContextUtils::setGlobalVariables( const QVariantMap &variables )
{
  QgsApplication::setCustomVariables( variables );
}

/// @cond PRIVATE

class GetNamedProjectColor : public QgsScopedExpressionFunction
{
  public:
    GetNamedProjectColor( const QgsProject *project )
      : QgsScopedExpressionFunction( QStringLiteral( "project_color" ), 1, QStringLiteral( "Color" ) )
      , mProject( project )
    {
      if ( !project )
        return;

      //build up color list from project. Do this in advance for speed
      QStringList colorStrings = project->readListEntry( QStringLiteral( "Palette" ), QStringLiteral( "/Colors" ) );
      QStringList colorLabels = project->readListEntry( QStringLiteral( "Palette" ), QStringLiteral( "/Labels" ) );

      //generate list from custom colors
      int colorIndex = 0;
      for ( QStringList::iterator it = colorStrings.begin();
            it != colorStrings.end(); ++it )
      {
        QColor color = QgsSymbolLayerUtils::decodeColor( *it );
        QString label;
        if ( colorLabels.length() > colorIndex )
        {
          label = colorLabels.at( colorIndex );
        }

        mColors.insert( label.toLower(), color );
        colorIndex++;
      }
    }

    QVariant func( const QVariantList &values, const QgsExpressionContext *, QgsExpression * ) override
    {
      QString colorName = values.at( 0 ).toString().toLower();
      if ( mColors.contains( colorName ) )
      {
        return QStringLiteral( "%1,%2,%3" ).arg( mColors.value( colorName ).red() ).arg( mColors.value( colorName ).green() ).arg( mColors.value( colorName ).blue() );
      }
      else
        return QVariant();
    }

    QgsScopedExpressionFunction *clone() const override
    {
      return new GetNamedProjectColor( mProject );
    }

  private:

    const QgsProject *mProject = nullptr;
    QHash< QString, QColor > mColors;

};

class GetComposerItemVariables : public QgsScopedExpressionFunction
{
  public:
    GetComposerItemVariables( const QgsComposition *c )
      : QgsScopedExpressionFunction( QStringLiteral( "item_variables" ), QgsExpressionFunction::ParameterList() << QgsExpressionFunction::Parameter( QStringLiteral( "id" ) ), QStringLiteral( "Composition" ) )
      , mComposition( c )
    {}

    QVariant func( const QVariantList &values, const QgsExpressionContext *, QgsExpression * ) override
    {
      if ( !mComposition )
        return QVariant();

      QString id = values.at( 0 ).toString().toLower();

      const QgsComposerItem *item = mComposition->getComposerItemById( id );
      if ( !item )
        return QVariant();

      QgsExpressionContext c = item->createExpressionContext();

      return c.variablesToMap();
    }

    QgsScopedExpressionFunction *clone() const override
    {
      return new GetComposerItemVariables( mComposition );
    }

  private:

    const QgsComposition *mComposition = nullptr;

};

class GetLayerVisibility : public QgsScopedExpressionFunction
{
  public:
    GetLayerVisibility( const QList<QgsMapLayer *> &layers )
      : QgsScopedExpressionFunction( QStringLiteral( "is_layer_visible" ), QgsExpressionFunction::ParameterList() << QgsExpressionFunction::Parameter( QStringLiteral( "id" ) ), QStringLiteral( "General" ) )
      , mLayers( layers )
    {}

    QVariant func( const QVariantList &values, const QgsExpressionContext *, QgsExpression * ) override
    {
      if ( mLayers.isEmpty() )
      {
        return QVariant( false );
      }

      QgsMapLayer *layer = _qgis_findLayer( mLayers, values.at( 0 ).toString() );
      if ( layer )
      {
        return QVariant( true );
      }
      else
      {
        return QVariant( false );
      }
    }

    QgsScopedExpressionFunction *clone() const override
    {
      return new GetLayerVisibility( mLayers );
    }

  private:

    const QList<QgsMapLayer *> mLayers;

};

class GetProcessingParameterValue : public QgsScopedExpressionFunction
{
  public:
    GetProcessingParameterValue( const QVariantMap &params )
      : QgsScopedExpressionFunction( QStringLiteral( "parameter" ), QgsExpressionFunction::ParameterList() << QgsExpressionFunction::Parameter( QStringLiteral( "name" ) ), QStringLiteral( "Processing" ) )
      , mParams( params )
    {}

    QVariant func( const QVariantList &values, const QgsExpressionContext *, QgsExpression * ) override
    {
      return mParams.value( values.at( 0 ).toString() );
    }

    QgsScopedExpressionFunction *clone() const override
    {
      return new GetProcessingParameterValue( mParams );
    }

  private:

    const QVariantMap mParams;

};

///@endcond

QgsExpressionContextScope *QgsExpressionContextUtils::projectScope( const QgsProject *project )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Project" ) );

  if ( !project )
    return scope;

  const QVariantMap vars = project->customVariables();

  QVariantMap::const_iterator it = vars.constBegin();

  for ( ; it != vars.constEnd(); ++it )
  {
    scope->setVariable( it.key(), it.value(), true );
  }

  //add other known project variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_title" ), project->title(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_path" ), QDir::toNativeSeparators( project->fileInfo().filePath() ), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_folder" ), QDir::toNativeSeparators( project->fileInfo().dir().path() ), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_filename" ), project->fileInfo().fileName(), true, true ) );
  QgsCoordinateReferenceSystem projectCrs = project->crs();
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_crs" ), projectCrs.authid(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "project_crs_definition" ), projectCrs.toProj4(), true, true ) );

  scope->addFunction( QStringLiteral( "project_color" ), new GetNamedProjectColor( project ) );
  return scope;
}

void QgsExpressionContextUtils::setProjectVariable( QgsProject *project, const QString &name, const QVariant &value )
{
  if ( !project )
    return;

  QVariantMap vars = project->customVariables();

  vars.insert( name, value );

  project->setCustomVariables( vars );
}

void QgsExpressionContextUtils::setProjectVariables( QgsProject *project, const QVariantMap &variables )
{
  if ( !project )
    return;

  project->setCustomVariables( variables );
}

QgsExpressionContextScope *QgsExpressionContextUtils::layerScope( const QgsMapLayer *layer )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Layer" ) );

  if ( !layer )
    return scope;

  //add variables defined in layer properties
  QStringList variableNames = layer->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = layer->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  int varIndex = 0;
  Q_FOREACH ( const QString &variableName, variableNames )
  {
    if ( varIndex >= variableValues.length() )
    {
      break;
    }

    QVariant varValue = variableValues.at( varIndex );
    varIndex++;
    scope->setVariable( variableName, varValue, true );
  }

  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layer_name" ), layer->name(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layer_id" ), layer->id(), true, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layer" ), QVariant::fromValue<QgsWeakMapLayerPointer >( QgsWeakMapLayerPointer( const_cast<QgsMapLayer *>( layer ) ) ), true, true ) );

  const QgsVectorLayer *vLayer = qobject_cast< const QgsVectorLayer * >( layer );
  if ( vLayer )
  {
    scope->setFields( vLayer->fields() );
  }

  //TODO - add functions. Possibilities include:
  //is_selected
  //field summary stats

  return scope;
}

QList<QgsExpressionContextScope *> QgsExpressionContextUtils::globalProjectLayerScopes( const QgsMapLayer *layer )
{
  QList<QgsExpressionContextScope *> scopes;
  scopes << globalScope();

  QgsProject *project = QgsProject::instance();  // TODO: use project associated with layer
  if ( project )
    scopes << projectScope( project );

  if ( layer )
    scopes << layerScope( layer );
  return scopes;
}

void QgsExpressionContextUtils::setLayerVariable( QgsMapLayer *layer, const QString &name, const QVariant &value )
{
  if ( !layer )
    return;

  //write variable to layer
  QStringList variableNames = layer->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = layer->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  variableNames << name;
  variableValues << value.toString();

  layer->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  layer->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

void QgsExpressionContextUtils::setLayerVariables( QgsMapLayer *layer, const QVariantMap &variables )
{
  if ( !layer )
    return;

  QStringList variableNames;
  QStringList variableValues;

  QVariantMap::const_iterator it = variables.constBegin();
  for ( ; it != variables.constEnd(); ++it )
  {
    variableNames << it.key();
    variableValues << it.value().toString();
  }

  layer->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  layer->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

QgsExpressionContextScope *QgsExpressionContextUtils::mapSettingsScope( const QgsMapSettings &mapSettings )
{
  // IMPORTANT: ANY CHANGES HERE ALSO NEED TO BE MADE TO QgsComposerMap::createExpressionContext()
  // (rationale is described in QgsComposerMap::createExpressionContext() )

  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Map Settings" ) );

  //add known map settings context variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_id" ), "canvas", true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_rotation" ), mapSettings.rotation(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_scale" ), mapSettings.scale(), true ) );
  QgsGeometry extent = QgsGeometry::fromRect( mapSettings.visibleExtent() );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_extent" ), QVariant::fromValue( extent ), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_extent_width" ), mapSettings.visibleExtent().width(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_extent_height" ), mapSettings.visibleExtent().height(), true ) );
  QgsGeometry centerPoint = QgsGeometry::fromPoint( mapSettings.visibleExtent().center() );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_extent_center" ), QVariant::fromValue( centerPoint ), true ) );

  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_crs" ), mapSettings.destinationCrs().authid(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_crs_definition" ), mapSettings.destinationCrs().toProj4(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "map_units" ), QgsUnitTypes::toString( mapSettings.mapUnits() ), true ) );

  scope->addFunction( QStringLiteral( "is_layer_visible" ), new GetLayerVisibility( mapSettings.layers() ) );

  return scope;
}

QgsExpressionContextScope *QgsExpressionContextUtils::mapToolCaptureScope( const QList<QgsPointLocator::Match> &matches )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Map Tool Capture" ) );

  QVariantList matchList;

  for ( const QgsPointLocator::Match &match : matches )
  {
    QVariantMap matchMap;

    matchMap.insert( QStringLiteral( "valid" ), match.isValid() );
    matchMap.insert( QStringLiteral( "layer" ), QVariant::fromValue<QgsWeakMapLayerPointer>( QgsWeakMapLayerPointer( match.layer() ) ) );
    matchMap.insert( QStringLiteral( "feature_id" ), match.featureId() );
    matchMap.insert( QStringLiteral( "vertex_index" ), match.vertexIndex() );
    matchMap.insert( QStringLiteral( "distance" ), match.distance() );

    matchList.append( matchMap );
  }

  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "snapping_results" ), matchList ) );

  return scope;
}

QgsExpressionContextScope *QgsExpressionContextUtils::updateSymbolScope( const QgsSymbol *symbol, QgsExpressionContextScope *symbolScope )
{
  if ( !symbolScope )
    return nullptr;

  symbolScope->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_SYMBOL_COLOR, symbol ? symbol->color() : QColor(), true ) );

  double angle = 0.0;
  const QgsMarkerSymbol *markerSymbol = dynamic_cast< const QgsMarkerSymbol * >( symbol );
  if ( markerSymbol )
  {
    angle = markerSymbol->angle();
  }
  symbolScope->addVariable( QgsExpressionContextScope::StaticVariable( QgsExpressionContext::EXPR_SYMBOL_ANGLE, angle, true ) );

  return symbolScope;
}

QgsExpressionContextScope *QgsExpressionContextUtils::compositionScope( const QgsComposition *composition )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Composition" ) );
  if ( !composition )
    return scope;

  //add variables defined in composition properties
  QStringList variableNames = composition->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = composition->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  int varIndex = 0;
  Q_FOREACH ( const QString &variableName, variableNames )
  {
    if ( varIndex >= variableValues.length() )
    {
      break;
    }

    QVariant varValue = variableValues.at( varIndex );
    varIndex++;
    scope->setVariable( variableName, varValue );
  }

  //add known composition context variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_name" ), composition->name(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_numpages" ), composition->numPages(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_pageheight" ), composition->paperHeight(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_pagewidth" ), composition->paperWidth(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_dpi" ), composition->printResolution(), true ) );


  scope->addFunction( QStringLiteral( "item_variables" ), new GetComposerItemVariables( composition ) );

  return scope;
}

void QgsExpressionContextUtils::setCompositionVariable( QgsComposition *composition, const QString &name, const QVariant &value )
{
  if ( !composition )
    return;

  //write variable to composition
  QStringList variableNames = composition->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = composition->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  variableNames << name;
  variableValues << value.toString();

  composition->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  composition->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

void QgsExpressionContextUtils::setCompositionVariables( QgsComposition *composition, const QVariantMap &variables )
{
  if ( !composition )
    return;

  QStringList variableNames;
  QStringList variableValues;

  QVariantMap::const_iterator it = variables.constBegin();
  for ( ; it != variables.constEnd(); ++it )
  {
    variableNames << it.key();
    variableValues << it.value().toString();
  }

  composition->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  composition->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

QgsExpressionContextScope *QgsExpressionContextUtils::layoutScope( const QgsLayout *layout )
{
  std::unique_ptr< QgsExpressionContextScope > scope( new QgsExpressionContextScope( QObject::tr( "Layout" ) ) );
  if ( !layout )
    return scope.release();

  //add variables defined in layout properties
  QStringList variableNames = layout->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = layout->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  int varIndex = 0;
  Q_FOREACH ( const QString &variableName, variableNames )
  {
    if ( varIndex >= variableValues.length() )
    {
      break;
    }

    QVariant varValue = variableValues.at( varIndex );
    varIndex++;
    scope->setVariable( variableName, varValue );
  }

  //add known layout context variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_name" ), layout->name(), true ) );
#if 0 //TODO
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_numpages" ), composition->numPages(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_pageheight" ), composition->paperHeight(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_pagewidth" ), composition->paperWidth(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_dpi" ), layout->context().dpi(), true ) );
#endif

#if 0 //TODO
  scope->addFunction( QStringLiteral( "item_variables" ), new GetComposerItemVariables( composition ) );
#endif

  return scope.release();
}

void QgsExpressionContextUtils::setLayoutVariable( QgsLayout *layout, const QString &name, const QVariant &value )
{
  if ( !layout )
    return;

  //write variable to composition
  QStringList variableNames = layout->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = layout->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  variableNames << name;
  variableValues << value.toString();

  layout->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  layout->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

void QgsExpressionContextUtils::setLayoutVariables( QgsLayout *layout, const QVariantMap &variables )
{
  if ( !layout )
    return;

  QStringList variableNames;
  QStringList variableValues;

  QVariantMap::const_iterator it = variables.constBegin();
  for ( ; it != variables.constEnd(); ++it )
  {
    variableNames << it.key();
    variableValues << it.value().toString();
  }

  layout->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  layout->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

QgsExpressionContextScope *QgsExpressionContextUtils::atlasScope( const QgsAtlasComposition *atlas )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Atlas" ) );
  if ( !atlas )
  {
    //add some dummy atlas variables. This is done so that as in certain contexts we want to show
    //users that these variables are available even if they have no current value
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_pagename" ), QString(), true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_feature" ), QVariant::fromValue( QgsFeature() ), true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_featureid" ), 0, true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_geometry" ), QVariant::fromValue( QgsGeometry() ), true ) );
    return scope;
  }

  //add known atlas variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_totalfeatures" ), atlas->numFeatures(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_featurenumber" ), atlas->currentFeatureNumber() + 1, true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_filename" ), atlas->currentFilename(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_pagename" ), atlas->currentPageName(), true ) );

  if ( atlas->enabled() && atlas->coverageLayer() )
  {
    scope->setFields( atlas->coverageLayer()->fields() );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_layerid" ), atlas->coverageLayer()->id(), true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_layername" ), atlas->coverageLayer()->name(), true ) );
  }

  if ( atlas->enabled() )
  {
    QgsFeature atlasFeature = atlas->feature();
    scope->setFeature( atlasFeature );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_feature" ), QVariant::fromValue( atlasFeature ), true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_featureid" ), atlasFeature.id(), true ) );
    scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "atlas_geometry" ), QVariant::fromValue( atlasFeature.geometry() ), true ) );
  }

  return scope;
}

QgsExpressionContextScope *QgsExpressionContextUtils::composerItemScope( const QgsComposerItem *composerItem )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope( QObject::tr( "Composer Item" ) );
  if ( !composerItem )
    return scope;

  //add variables defined in composer item properties
  QStringList variableNames = composerItem->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = composerItem->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  int varIndex = 0;
  Q_FOREACH ( const QString &variableName, variableNames )
  {
    if ( varIndex >= variableValues.length() )
    {
      break;
    }

    QVariant varValue = variableValues.at( varIndex );
    varIndex++;
    scope->setVariable( variableName, varValue );
  }

  //add known composer item context variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "item_id" ), composerItem->id(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "item_uuid" ), composerItem->uuid(), true ) );
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "layout_page" ), composerItem->page(), true ) );

  return scope;
}

void QgsExpressionContextUtils::setComposerItemVariable( QgsComposerItem *composerItem, const QString &name, const QVariant &value )
{
  if ( !composerItem )
    return;

  //write variable to composer item
  QStringList variableNames = composerItem->customProperty( QStringLiteral( "variableNames" ) ).toStringList();
  QStringList variableValues = composerItem->customProperty( QStringLiteral( "variableValues" ) ).toStringList();

  variableNames << name;
  variableValues << value.toString();

  composerItem->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  composerItem->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

void QgsExpressionContextUtils::setComposerItemVariables( QgsComposerItem *composerItem, const QVariantMap &variables )
{
  if ( !composerItem )
    return;

  QStringList variableNames;
  QStringList variableValues;

  QVariantMap::const_iterator it = variables.constBegin();
  for ( ; it != variables.constEnd(); ++it )
  {
    variableNames << it.key();
    variableValues << it.value().toString();
  }

  composerItem->setCustomProperty( QStringLiteral( "variableNames" ), variableNames );
  composerItem->setCustomProperty( QStringLiteral( "variableValues" ), variableValues );
}

QgsExpressionContext QgsExpressionContextUtils::createFeatureBasedContext( const QgsFeature &feature, const QgsFields &fields )
{
  QgsExpressionContextScope *scope = new QgsExpressionContextScope();
  scope->setFeature( feature );
  scope->setFields( fields );
  return QgsExpressionContext() << scope;
}

QgsExpressionContextScope *QgsExpressionContextUtils::processingAlgorithmScope( const QgsProcessingAlgorithm *algorithm, const QVariantMap &parameters, QgsProcessingContext &context )
{
  // set aside for future use
  Q_UNUSED( context );

  std::unique_ptr< QgsExpressionContextScope > scope( new QgsExpressionContextScope( QObject::tr( "Algorithm" ) ) );
  if ( !algorithm )
    return scope.release();

  //add standard algorithm variables
  scope->addVariable( QgsExpressionContextScope::StaticVariable( QStringLiteral( "algorithm_id" ), algorithm->id(), true ) );

  scope->addFunction( QStringLiteral( "parameter" ), new GetProcessingParameterValue( parameters ) );

  return scope.release();
}

void QgsExpressionContextUtils::registerContextFunctions()
{
  QgsExpression::registerFunction( new GetNamedProjectColor( nullptr ) );
  QgsExpression::registerFunction( new GetComposerItemVariables( nullptr ) );
  QgsExpression::registerFunction( new GetLayerVisibility( QList<QgsMapLayer *>() ) );
  QgsExpression::registerFunction( new GetProcessingParameterValue( QVariantMap() ) );
}

bool QgsScopedExpressionFunction::usesGeometry( const QgsExpressionNodeFunction *node ) const
{
  Q_UNUSED( node )
  return mUsesGeometry;
}

QSet<QString> QgsScopedExpressionFunction::referencedColumns( const QgsExpressionNodeFunction *node ) const
{
  Q_UNUSED( node )
  return mReferencedColumns;
}

bool QgsScopedExpressionFunction::isStatic( const QgsExpressionNodeFunction *node, QgsExpression *parent, const QgsExpressionContext *context ) const
{
  return allParamsStatic( node, parent, context );
}
