/***************************************************************************
                         qgsprocessingmodelalgorithm.cpp
                         ------------------------------
    begin                : June 2017
    copyright            : (C) 2017 by Nyall Dawson
    email                : nyall dot dawson at gmail dot com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgsprocessingmodelalgorithm.h"
#include "qgsprocessingregistry.h"
#include "qgsprocessingfeedback.h"
#include "qgsprocessingutils.h"
#include "qgsxmlutils.h"
#include "qgsexception.h"
#include <QFile>
#include <QTextStream>

///@cond NOT_STABLE

QgsProcessingModelAlgorithm::QgsProcessingModelAlgorithm( const QString &name, const QString &group )
  : QgsProcessingAlgorithm()
  , mModelName( name.isEmpty() ? QObject::tr( "model" ) : name )
  , mModelGroup( group )
{}

void QgsProcessingModelAlgorithm::initAlgorithm( const QVariantMap & )
{
}

QString QgsProcessingModelAlgorithm::name() const
{
  return mModelName;
}

QString QgsProcessingModelAlgorithm::displayName() const
{
  return mModelName;
}

QString QgsProcessingModelAlgorithm::group() const
{
  return mModelGroup;
}

QIcon QgsProcessingModelAlgorithm::icon() const
{
  return QgsApplication::getThemeIcon( QStringLiteral( "/processingModel.svg" ) );
}

QString QgsProcessingModelAlgorithm::svgIconPath() const
{
  return QgsApplication::iconPath( QStringLiteral( "processingModel.svg" ) );
}

QString QgsProcessingModelAlgorithm::shortHelpString() const
{
  if ( mHelpContent.contains( QStringLiteral( "ALG_DESC" ) ) )
    return mHelpContent.value( QStringLiteral( "ALG_DESC" ) ).toString();
  return QString();
}

QString QgsProcessingModelAlgorithm::helpUrl() const
{
  return QgsProcessingUtils::formatHelpMapAsHtml( mHelpContent, this );
}

QVariantMap QgsProcessingModelAlgorithm::parametersForChildAlgorithm( const QgsProcessingModelChildAlgorithm &child, const QVariantMap &modelParameters, const QVariantMap &results, const QgsExpressionContext &expressionContext ) const
{
  QVariantMap childParams;
  Q_FOREACH ( const QgsProcessingParameterDefinition *def, child.algorithm()->parameterDefinitions() )
  {
    if ( def->flags() & QgsProcessingParameterDefinition::FlagHidden )
      continue;

    if ( !def->isDestination() )
    {
      if ( !child.parameterSources().contains( def->name() ) )
        continue; // use default value

      QgsProcessingModelChildParameterSources paramSources = child.parameterSources().value( def->name() );

      QVariantList paramParts;
      Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, paramSources )
      {
        switch ( source.source() )
        {
          case QgsProcessingModelChildParameterSource::StaticValue:
            paramParts << source.staticValue();
            break;

          case QgsProcessingModelChildParameterSource::ModelParameter:
            paramParts << modelParameters.value( source.parameterName() );
            break;

          case QgsProcessingModelChildParameterSource::ChildOutput:
          {
            QVariantMap linkedChildResults = results.value( source.outputChildId() ).toMap();
            paramParts << linkedChildResults.value( source.outputName() );
            break;
          }

          case QgsProcessingModelChildParameterSource::Expression:
          {
            QgsExpression exp( source.expression() );
            paramParts << exp.evaluate( &expressionContext );
            break;
          }
        }
      }
      if ( paramParts.count() == 1 )
        childParams.insert( def->name(), paramParts.at( 0 ) );
      else
        childParams.insert( def->name(), paramParts );

    }
    else
    {
      const QgsProcessingDestinationParameter *destParam = static_cast< const QgsProcessingDestinationParameter * >( def );

      // is destination linked to one of the final outputs from this model?
      bool isFinalOutput = false;
      QMap<QString, QgsProcessingModelOutput> outputs = child.modelOutputs();
      QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
      for ( ; outputIt != outputs.constEnd(); ++outputIt )
      {
        if ( outputIt->childOutputName() == destParam->name() )
        {
          QString paramName = child.childId() + ':' + outputIt.key();
          if ( modelParameters.contains( paramName ) )
          {
            QVariant value = modelParameters.value( paramName );
            if ( value.canConvert<QgsProcessingOutputLayerDefinition>() )
            {
              // make sure layout output name is correctly set
              QgsProcessingOutputLayerDefinition fromVar = qvariant_cast<QgsProcessingOutputLayerDefinition>( value );
              fromVar.destinationName = outputIt.key();
              value = QVariant::fromValue( fromVar );
            }

            childParams.insert( destParam->name(), value );
          }
          isFinalOutput = true;
          break;
        }
      }

      if ( !isFinalOutput )
      {
        // output is temporary

        // check whether it's optional, and if so - is it required?
        bool required = true;
        if ( destParam->flags() & QgsProcessingParameterDefinition::FlagOptional )
        {
          required = childOutputIsRequired( child.childId(), destParam->name() );
        }

        // not optional, or required elsewhere in model
        if ( required )
          childParams.insert( destParam->name(), destParam->generateTemporaryDestination() );
      }
    }
  }
  return childParams;
}

bool QgsProcessingModelAlgorithm::childOutputIsRequired( const QString &childId, const QString &outputName ) const
{
  // look through all child algs
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->childId() == childId || !childIt->isActive() )
      continue;

    // look through all sources for child
    QMap<QString, QgsProcessingModelChildParameterSources> candidateChildParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator childParamIt = candidateChildParams.constBegin();
    for ( ; childParamIt != candidateChildParams.constEnd(); ++childParamIt )
    {
      Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, childParamIt.value() )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput
             && source.outputChildId() == childId
             && source.outputName() == outputName )
        {
          return true;
        }
      }
    }
  }
  return false;
}

QVariantMap QgsProcessingModelAlgorithm::processAlgorithm( const QVariantMap &parameters, QgsProcessingContext &context, QgsProcessingFeedback *feedback )
{
  QSet< QString > toExecute;
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->isActive() && childIt->algorithm() )
      toExecute.insert( childIt->childId() );
  }

  QTime totalTime;
  totalTime.start();

  QgsExpressionContext baseContext = createExpressionContext( parameters, context );

  QVariantMap childResults;
  QVariantMap finalResults;
  QSet< QString > executed;
  bool executedAlg = true;
  while ( executedAlg && executed.count() < toExecute.count() )
  {
    executedAlg = false;
    Q_FOREACH ( const QString &childId, toExecute )
    {
      if ( executed.contains( childId ) )
        continue;

      bool canExecute = true;
      Q_FOREACH ( const QString &dependency, dependsOnChildAlgorithms( childId ) )
      {
        if ( !executed.contains( dependency ) )
        {
          canExecute = false;
          break;
        }
      }

      if ( !canExecute )
        continue;

      executedAlg = true;
      feedback->pushDebugInfo( QObject::tr( "Prepare algorithm: %1" ).arg( childId ) );

      const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms[ childId ];

      QgsExpressionContext expContext = baseContext;
      expContext << QgsExpressionContextUtils::processingAlgorithmScope( child.algorithm(), parameters, context )
                 << createExpressionContextScopeForChildAlgorithm( childId, context, parameters, childResults );

      QVariantMap childParams = parametersForChildAlgorithm( child, parameters, childResults, expContext );
      feedback->setProgressText( QObject::tr( "Running %1 [%2/%3]" ).arg( child.description() ).arg( executed.count() + 1 ).arg( toExecute.count() ) );
      //feedback->pushDebugInfo( "Parameters: " + ', '.join( [str( p ).strip() +
      //           '=' + str( p.value ) for p in alg.algorithm.parameters] ) )

      QTime childTime;
      childTime.start();

      bool ok = false;
      std::unique_ptr< QgsProcessingAlgorithm > childAlg( child.algorithm()->create( child.configuration() ) );
      QVariantMap results = childAlg->run( childParams, context, feedback, &ok );
      childAlg.reset( nullptr );
      if ( !ok )
      {
        QString error = QObject::tr( "Error encountered while running %1" ).arg( child.description() );
        feedback->reportError( error );
        throw QgsProcessingException( error );
      }
      childResults.insert( childId, results );

      // look through child alg's outputs to determine whether any of these should be copied
      // to the final model outputs
      QMap<QString, QgsProcessingModelOutput> outputs = child.modelOutputs();
      QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
      for ( ; outputIt != outputs.constEnd(); ++outputIt )
      {
        finalResults.insert( childId + ':' + outputIt->name(), results.value( outputIt->childOutputName() ) );
      }

      executed.insert( childId );
      feedback->pushDebugInfo( QObject::tr( "OK. Execution took %1 s (%2 outputs)." ).arg( childTime.elapsed() / 1000.0 ).arg( results.count() ) );
    }
  }
  feedback->pushDebugInfo( QObject::tr( "Model processed OK. Executed %1 algorithms total in %2 s." ).arg( executed.count() ).arg( totalTime.elapsed() / 1000.0 ) );

  mResults = finalResults;
  return mResults;
}

QString QgsProcessingModelAlgorithm::sourceFilePath() const
{
  return mSourceFile;
}

void QgsProcessingModelAlgorithm::setSourceFilePath( const QString &sourceFile )
{
  mSourceFile = sourceFile;
}

QString QgsProcessingModelAlgorithm::asPythonCode() const
{
  QStringList lines;
  lines << QStringLiteral( "##%1=name" ).arg( name() );

  QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
  for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
  {
    QString name = paramIt.value().parameterName();
    if ( parameterDefinition( name ) )
    {
      lines << parameterDefinition( name )->asScriptCode();
    }
  }

  auto safeName = []( const QString & name )->QString
  {
    QString n = name.toLower().trimmed();
    QRegularExpression rx( "[^a-z_]" );
    n.replace( rx, QString() );
    return n;
  };

  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( !childIt->isActive() || !childIt->algorithm() )
      continue;

    // look through all outputs for child
    QMap<QString, QgsProcessingModelOutput> outputs = childIt->modelOutputs();
    QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
    for ( ; outputIt != outputs.constEnd(); ++outputIt )
    {
      const QgsProcessingOutputDefinition *output = childIt->algorithm()->outputDefinition( outputIt->childOutputName() );
      lines << QStringLiteral( "##%1=output %2" ).arg( safeName( outputIt->name() ), output->type() );
    }
  }

  lines << QStringLiteral( "results={}" );

  QSet< QString > toExecute;
  childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( childIt->isActive() && childIt->algorithm() )
      toExecute.insert( childIt->childId() );
  }

  QSet< QString > executed;
  bool executedAlg = true;
  while ( executedAlg && executed.count() < toExecute.count() )
  {
    executedAlg = false;
    Q_FOREACH ( const QString &childId, toExecute )
    {
      if ( executed.contains( childId ) )
        continue;

      bool canExecute = true;
      Q_FOREACH ( const QString &dependency, dependsOnChildAlgorithms( childId ) )
      {
        if ( !executed.contains( dependency ) )
        {
          canExecute = false;
          break;
        }
      }

      if ( !canExecute )
        continue;

      executedAlg = true;

      const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms[ childId ];
      lines << child.asPythonCode();

      executed.insert( childId );
    }
  }

  lines << QStringLiteral( "return results" );

  return lines.join( '\n' );
}

QMap<QString, QgsProcessingModelAlgorithm::VariableDefinition> QgsProcessingModelAlgorithm::variablesForChildAlgorithm( const QString &childId, QgsProcessingContext &context, const QVariantMap &modelParameters, const QVariantMap &results ) const
{
  QMap<QString, QgsProcessingModelAlgorithm::VariableDefinition> variables;

  auto safeName = []( const QString & name )->QString
  {
    QString s = name;
    return s.replace( QRegularExpression( "[\\s'\"\\(\\):]" ), QStringLiteral( "_" ) );
  };

  // "static"/single value sources
  QgsProcessingModelChildParameterSources sources = availableSourcesForChild( childId, QStringList() << QgsProcessingParameterNumber::typeName()
      << QgsProcessingParameterBoolean::typeName()
      << QgsProcessingParameterExpression::typeName()
      << QgsProcessingParameterField::typeName()
      << QgsProcessingParameterString::typeName(),
      QStringList() << QgsProcessingOutputNumber::typeName()
      << QgsProcessingOutputString::typeName() );
  Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, sources )
  {
    QString name;
    QVariant value;
    QString description;
    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;
    };
    variables.insert( safeName( name ), VariableDefinition( value, source, description ) );
  }

  // layer sources
  sources = availableSourcesForChild( childId, QStringList()
                                      << QgsProcessingParameterVectorLayer::typeName()
                                      << QgsProcessingParameterRasterLayer::typeName(),
                                      QStringList() << QgsProcessingOutputVectorLayer::typeName()
                                      << QgsProcessingOutputRasterLayer::typeName() );

  Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, sources )
  {
    QString name;
    QVariant value;
    QString description;

    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;

    };

    QgsMapLayer *layer = qobject_cast< QgsMapLayer * >( qvariant_cast<QObject *>( value ) );
    if ( !layer )
      layer = QgsProcessingUtils::mapLayerFromString( value.toString(), context );

    variables.insert( safeName( QStringLiteral( "%1_minx" ).arg( name ) ), VariableDefinition( layer ? layer->extent().xMinimum() : QVariant(), source, QObject::tr( "Minimum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_miny" ).arg( name ) ), VariableDefinition( layer ? layer->extent().yMinimum() : QVariant(), source, QObject::tr( "Minimum Y of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxx" ).arg( name ) ), VariableDefinition( layer ? layer->extent().xMaximum() : QVariant(), source, QObject::tr( "Maximum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxy" ).arg( name ) ), VariableDefinition( layer ? layer->extent().yMaximum() : QVariant(), source, QObject::tr( "Maximum Y of %1" ).arg( description ) ) );

    continue;
  }

  sources = availableSourcesForChild( childId, QStringList()
                                      << QgsProcessingParameterFeatureSource::typeName() );
  Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, sources )
  {
    QString name;
    QVariant value;
    QString description;

    switch ( source.source() )
    {
      case QgsProcessingModelChildParameterSource::ModelParameter:
      {
        name = source.parameterName();
        value = modelParameters.value( source.parameterName() );
        description = parameterDefinition( source.parameterName() )->description();
        break;
      }
      case QgsProcessingModelChildParameterSource::ChildOutput:
      {
        const QgsProcessingModelChildAlgorithm &child = mChildAlgorithms.value( source.outputChildId() );
        name = QStringLiteral( "%1_%2" ).arg( child.description().isEmpty() ?
                                              source.outputChildId() : child.description(), source.outputName() );
        value = results.value( source.outputChildId() ).toMap().value( source.outputName() );
        if ( const QgsProcessingAlgorithm *alg = child.algorithm() )
        {
          description = QObject::tr( "Output '%1' from algorithm '%2'" ).arg( alg->outputDefinition( source.outputName() )->description(),
                        child.description() );
        }
        break;
      }

      case QgsProcessingModelChildParameterSource::Expression:
      case QgsProcessingModelChildParameterSource::StaticValue:
        continue;

    };

    QgsFeatureSource *featureSource = nullptr;
    if ( value.canConvert<QgsProcessingFeatureSourceDefinition>() )
    {
      QgsProcessingFeatureSourceDefinition fromVar = qvariant_cast<QgsProcessingFeatureSourceDefinition>( value );
      value = fromVar.source;
    }
    if ( QgsVectorLayer *layer = qobject_cast< QgsVectorLayer * >( qvariant_cast<QObject *>( value ) ) )
    {
      featureSource = layer;
    }
    if ( !featureSource )
    {
      if ( QgsVectorLayer *vl = qobject_cast< QgsVectorLayer *>( QgsProcessingUtils::mapLayerFromString( value.toString(), context ) ) )
        featureSource = vl;
    }

    variables.insert( safeName( QStringLiteral( "%1_minx" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().xMinimum() : QVariant(), source, QObject::tr( "Minimum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_miny" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().yMinimum() : QVariant(), source, QObject::tr( "Minimum Y of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxx" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().xMaximum() : QVariant(), source, QObject::tr( "Maximum X of %1" ).arg( description ) ) );
    variables.insert( safeName( QStringLiteral( "%1_maxy" ).arg( name ) ), VariableDefinition( featureSource ? featureSource->sourceExtent().yMaximum() : QVariant(), source, QObject::tr( "Maximum Y of %1" ).arg( description ) ) );
  }

  return variables;
}

QgsExpressionContextScope *QgsProcessingModelAlgorithm::createExpressionContextScopeForChildAlgorithm( const QString &childId, QgsProcessingContext &context, const QVariantMap &modelParameters, const QVariantMap &results ) const
{
  std::unique_ptr< QgsExpressionContextScope > scope( new QgsExpressionContextScope() );
  QMap< QString, QgsProcessingModelAlgorithm::VariableDefinition> variables = variablesForChildAlgorithm( childId, context, modelParameters, results );
  QMap< QString, QgsProcessingModelAlgorithm::VariableDefinition>::const_iterator varIt = variables.constBegin();
  for ( ; varIt != variables.constEnd(); ++varIt )
  {
    scope->addVariable( QgsExpressionContextScope::StaticVariable( varIt.key(), varIt->value, true, false, varIt->description ) );
  }
  return scope.release();
}

QgsProcessingModelChildParameterSources QgsProcessingModelAlgorithm::availableSourcesForChild( const QString &childId, const QStringList &parameterTypes, const QStringList &outputTypes, const QList<int> &dataTypes ) const
{
  QgsProcessingModelChildParameterSources sources;

  // first look through model parameters
  QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
  for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
  {
    const QgsProcessingParameterDefinition *def = parameterDefinition( paramIt->parameterName() );
    if ( !def )
      continue;

    if ( parameterTypes.contains( def->type() ) )
    {
      if ( !dataTypes.isEmpty() )
      {
        if ( def->type() == QgsProcessingParameterField::typeName() )
        {
          const QgsProcessingParameterField *fieldDef = static_cast< const QgsProcessingParameterField * >( def );
          if ( !( dataTypes.contains( fieldDef->dataType() ) || fieldDef->dataType() == QgsProcessingParameterField::Any ) )
          {
            continue;
          }
        }
        else if ( def->type() == QgsProcessingParameterFeatureSource::typeName() || def->type() == QgsProcessingParameterVectorLayer::typeName() )
        {
          const QgsProcessingParameterLimitedDataTypes *sourceDef = dynamic_cast< const QgsProcessingParameterLimitedDataTypes *>( def );
          if ( !sourceDef )
            continue;

          bool ok = sourceDef->dataTypes().isEmpty();
          Q_FOREACH ( int type, sourceDef->dataTypes() )
          {
            if ( dataTypes.contains( type ) || type == QgsProcessing::TypeMapLayer || type == QgsProcessing::TypeVector || type == QgsProcessing::TypeVectorAnyGeometry )
            {
              ok = true;
              break;
            }
          }
          if ( dataTypes.contains( QgsProcessing::TypeMapLayer ) || dataTypes.contains( QgsProcessing::TypeVector ) || dataTypes.contains( QgsProcessing::TypeVectorAnyGeometry ) )
            ok = true;

          if ( !ok )
            continue;
        }
      }
      sources << QgsProcessingModelChildParameterSource::fromModelParameter( paramIt->parameterName() );
    }
  }

  QSet< QString > dependents;
  if ( !childId.isEmpty() )
  {
    dependents = dependentChildAlgorithms( childId );
    dependents << childId;
  }

  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( dependents.contains( childIt->childId() ) )
      continue;

    const QgsProcessingAlgorithm *alg = childIt->algorithm();
    if ( !alg )
      continue;

    Q_FOREACH ( const QgsProcessingOutputDefinition *out, alg->outputDefinitions() )
    {
      if ( outputTypes.contains( out->type() ) )
      {
        if ( !dataTypes.isEmpty() )
        {
          if ( out->type() == QgsProcessingOutputVectorLayer::typeName() )
          {
            const QgsProcessingOutputVectorLayer *vectorOut = static_cast< const QgsProcessingOutputVectorLayer *>( out );

            if ( !( dataTypes.contains( vectorOut->dataType() ) || vectorOut->dataType() == QgsProcessing::TypeMapLayer || vectorOut->dataType() == QgsProcessing::TypeVector
                    || vectorOut->dataType() == QgsProcessing::TypeVectorAnyGeometry ) )
            {
              continue;
            }
          }
        }
        sources << QgsProcessingModelChildParameterSource::fromChildOutput( childIt->childId(), out->name() );
      }
    }
  }

  return sources;
}

QVariantMap QgsProcessingModelAlgorithm::helpContent() const
{
  return mHelpContent;
}

void QgsProcessingModelAlgorithm::setHelpContent( const QVariantMap &helpContent )
{
  mHelpContent = helpContent;
}

void QgsProcessingModelAlgorithm::setName( const QString &name )
{
  mModelName = name;
}

void QgsProcessingModelAlgorithm::setGroup( const QString &group )
{
  mModelGroup = group;
}

QMap<QString, QgsProcessingModelChildAlgorithm> QgsProcessingModelAlgorithm::childAlgorithms() const
{
  return mChildAlgorithms;
}

void QgsProcessingModelAlgorithm::setParameterComponents( const QMap<QString, QgsProcessingModelParameter> &parameterComponents )
{
  mParameterComponents = parameterComponents;
}

void QgsProcessingModelAlgorithm::setParameterComponent( const QgsProcessingModelParameter &component )
{
  mParameterComponents.insert( component.parameterName(), component );
}

QgsProcessingModelParameter &QgsProcessingModelAlgorithm::parameterComponent( const QString &name )
{
  if ( !mParameterComponents.contains( name ) )
  {
    QgsProcessingModelParameter &component = mParameterComponents[ name ];
    component.setParameterName( name );
    return component;
  }
  return mParameterComponents[ name ];
}

void QgsProcessingModelAlgorithm::updateDestinationParameters()
{
  //delete existing destination parameters
  QMutableListIterator<const QgsProcessingParameterDefinition *> it( mParameters );
  while ( it.hasNext() )
  {
    const QgsProcessingParameterDefinition *def = it.next();
    if ( def->isDestination() )
    {
      delete def;
      it.remove();
    }
  }
  // also delete outputs
  qDeleteAll( mOutputs );
  mOutputs.clear();

  // rebuild
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    QMap<QString, QgsProcessingModelOutput> outputs = childIt->modelOutputs();
    QMap<QString, QgsProcessingModelOutput>::const_iterator outputIt = outputs.constBegin();
    for ( ; outputIt != outputs.constEnd(); ++outputIt )
    {
      if ( !childIt->isActive() || !childIt->algorithm() )
        continue;

      // child algorithm has a destination parameter set, copy it to the model
      const QgsProcessingParameterDefinition *source = childIt->algorithm()->parameterDefinition( outputIt->childOutputName() );
      if ( !source )
        continue;

      std::unique_ptr< QgsProcessingParameterDefinition > param( source->clone() );
      param->setName( outputIt->childId() + ':' + outputIt->name() );
      param->setDescription( outputIt->description() );

      if ( const QgsProcessingDestinationParameter *destParam = dynamic_cast< const QgsProcessingDestinationParameter *>( param.get() ) )
      {
        std::unique_ptr< QgsProcessingOutputDefinition > output( destParam->toOutputDefinition() );
        if ( output )
        {
          addOutput( output.release() );
        }
      }
      addParameter( param.release() );
    }
  }
}

QVariant QgsProcessingModelAlgorithm::toVariant() const
{
  QVariantMap map;
  map.insert( QStringLiteral( "model_name" ), mModelName );
  map.insert( QStringLiteral( "model_group" ), mModelGroup );
  map.insert( QStringLiteral( "help" ), mHelpContent );

  QVariantMap childMap;
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    childMap.insert( childIt.key(), childIt.value().toVariant() );
  }
  map.insert( "children", childMap );

  QVariantMap paramMap;
  QMap< QString, QgsProcessingModelParameter >::const_iterator paramIt = mParameterComponents.constBegin();
  for ( ; paramIt != mParameterComponents.constEnd(); ++paramIt )
  {
    paramMap.insert( paramIt.key(), paramIt.value().toVariant() );
  }
  map.insert( "parameters", paramMap );

  QVariantMap paramDefMap;
  Q_FOREACH ( const QgsProcessingParameterDefinition *def, mParameters )
  {
    paramDefMap.insert( def->name(), def->toVariantMap() );
  }
  map.insert( "parameterDefinitions", paramDefMap );

  return map;
}

bool QgsProcessingModelAlgorithm::loadVariant( const QVariant &model )
{
  QVariantMap map = model.toMap();

  mModelName = map.value( QStringLiteral( "model_name" ) ).toString();
  mModelGroup = map.value( QStringLiteral( "model_group" ) ).toString();
  mHelpContent = map.value( QStringLiteral( "help" ) ).toMap();

  mChildAlgorithms.clear();
  QVariantMap childMap = map.value( QStringLiteral( "children" ) ).toMap();
  QVariantMap::const_iterator childIt = childMap.constBegin();
  for ( ; childIt != childMap.constEnd(); ++childIt )
  {
    QgsProcessingModelChildAlgorithm child;
    // we be leniant here - even if we couldn't load a parameter, don't interrupt the model loading
    // otherwise models may become unusable (e.g. due to removed plugins providing algs/parameters)
    // with no way for users to repair them
    if ( !child.loadVariant( childIt.value() ) )
      continue;

    mChildAlgorithms.insert( child.childId(), child );
  }

  mParameterComponents.clear();
  QVariantMap paramMap = map.value( QStringLiteral( "parameters" ) ).toMap();
  QVariantMap::const_iterator paramIt = paramMap.constBegin();
  for ( ; paramIt != paramMap.constEnd(); ++paramIt )
  {
    QgsProcessingModelParameter param;
    if ( !param.loadVariant( paramIt.value().toMap() ) )
      return false;

    mParameterComponents.insert( param.parameterName(), param );
  }

  qDeleteAll( mParameters );
  mParameters.clear();
  QVariantMap paramDefMap = map.value( QStringLiteral( "parameterDefinitions" ) ).toMap();
  QVariantMap::const_iterator paramDefIt = paramDefMap.constBegin();
  for ( ; paramDefIt != paramDefMap.constEnd(); ++paramDefIt )
  {
    std::unique_ptr< QgsProcessingParameterDefinition > param( QgsProcessingParameters::parameterFromVariantMap( paramDefIt.value().toMap() ) );
    // we be leniant here - even if we couldn't load a parameter, don't interrupt the model loading
    // otherwise models may become unusable (e.g. due to removed plugins providing algs/parameters)
    // with no way for users to repair them
    if ( param )
      addParameter( param.release() );
  }

  updateDestinationParameters();

  return true;
}

bool QgsProcessingModelAlgorithm::toFile( const QString &path ) const
{
  QDomDocument doc = QDomDocument( "model" );
  QDomElement elem = QgsXmlUtils::writeVariant( toVariant(), doc );
  doc.appendChild( elem );

  QFile file( path );
  if ( file.open( QFile::WriteOnly | QFile::Truncate ) )
  {
    QTextStream stream( &file );
    doc.save( stream, 2 );
    file.close();
    return true;
  }
  return false;
}

bool QgsProcessingModelAlgorithm::fromFile( const QString &path )
{
  QDomDocument doc;

  QFile file( path );
  if ( file.open( QFile::ReadOnly ) )
  {
    if ( !doc.setContent( &file ) )
      return false;

    file.close();
  }

  QVariant props = QgsXmlUtils::readVariant( doc.firstChildElement() );
  return loadVariant( props );
}

void QgsProcessingModelAlgorithm::setChildAlgorithms( const QMap<QString, QgsProcessingModelChildAlgorithm> &childAlgorithms )
{
  mChildAlgorithms = childAlgorithms;
  updateDestinationParameters();
}

void QgsProcessingModelAlgorithm::setChildAlgorithm( const QgsProcessingModelChildAlgorithm &algorithm )
{
  mChildAlgorithms.insert( algorithm.childId(), algorithm );
  updateDestinationParameters();
}

QString QgsProcessingModelAlgorithm::addChildAlgorithm( QgsProcessingModelChildAlgorithm &algorithm )
{
  if ( algorithm.childId().isEmpty() || mChildAlgorithms.contains( algorithm.childId() ) )
    algorithm.generateChildId( *this );

  mChildAlgorithms.insert( algorithm.childId(), algorithm );
  updateDestinationParameters();
  return algorithm.childId();
}

QgsProcessingModelChildAlgorithm &QgsProcessingModelAlgorithm::childAlgorithm( const QString &childId )
{
  return mChildAlgorithms[ childId ];
}

bool QgsProcessingModelAlgorithm::removeChildAlgorithm( const QString &id )
{
  if ( !dependentChildAlgorithms( id ).isEmpty() )
    return false;

  mChildAlgorithms.remove( id );
  updateDestinationParameters();
  return true;
}

void QgsProcessingModelAlgorithm::deactivateChildAlgorithm( const QString &id )
{
  Q_FOREACH ( const QString &child, dependentChildAlgorithms( id ) )
  {
    childAlgorithm( child ).setActive( false );
  }
  childAlgorithm( id ).setActive( false );
  updateDestinationParameters();
}

bool QgsProcessingModelAlgorithm::activateChildAlgorithm( const QString &id )
{
  Q_FOREACH ( const QString &child, dependsOnChildAlgorithms( id ) )
  {
    if ( !childAlgorithm( child ).isActive() )
      return false;
  }
  childAlgorithm( id ).setActive( true );
  updateDestinationParameters();
  return true;
}

void QgsProcessingModelAlgorithm::addModelParameter( QgsProcessingParameterDefinition *definition, const QgsProcessingModelParameter &component )
{
  addParameter( definition );
  mParameterComponents.insert( definition->name(), component );
}

void QgsProcessingModelAlgorithm::updateModelParameter( QgsProcessingParameterDefinition *definition )
{
  removeParameter( definition->name() );
  addParameter( definition );
}

void QgsProcessingModelAlgorithm::removeModelParameter( const QString &name )
{
  removeParameter( name );
  mParameterComponents.remove( name );
}

bool QgsProcessingModelAlgorithm::childAlgorithmsDependOnParameter( const QString &name ) const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    // check whether child requires this parameter
    QMap<QString, QgsProcessingModelChildParameterSources> childParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
    for ( ; paramIt != childParams.constEnd(); ++paramIt )
    {
      Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, paramIt.value() )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ModelParameter
             && source.parameterName() == name )
        {
          return true;
        }
      }
    }
  }
  return false;
}

bool QgsProcessingModelAlgorithm::otherParametersDependOnParameter( const QString &name ) const
{
  Q_FOREACH ( const QgsProcessingParameterDefinition *def, mParameters )
  {
    if ( def->name() == name )
      continue;

    if ( def->dependsOnOtherParameters().contains( name ) )
      return true;
  }
  return false;
}

QMap<QString, QgsProcessingModelParameter> QgsProcessingModelAlgorithm::parameterComponents() const
{
  return mParameterComponents;
}

void QgsProcessingModelAlgorithm::dependentChildAlgorithmsRecursive( const QString &childId, QSet<QString> &depends ) const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( depends.contains( childIt->childId() ) )
      continue;

    // does alg have a direct dependency on this child?
    if ( childIt->dependencies().contains( childId ) )
    {
      depends.insert( childIt->childId() );
      dependentChildAlgorithmsRecursive( childIt->childId(), depends );
      continue;
    }

    // check whether child requires any outputs from the target alg
    QMap<QString, QgsProcessingModelChildParameterSources> childParams = childIt->parameterSources();
    QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
    for ( ; paramIt != childParams.constEnd(); ++paramIt )
    {
      Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, paramIt.value() )
      {
        if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput
             && source.outputChildId() == childId )
        {
          depends.insert( childIt->childId() );
          dependsOnChildAlgorithmsRecursive( childIt->childId(), depends );
          break;
        }
      }
    }
  }
}

QSet<QString> QgsProcessingModelAlgorithm::dependentChildAlgorithms( const QString &childId ) const
{
  QSet< QString > algs;

  // temporarily insert the target child algorithm to avoid
  // unnecessarily recursion though it
  algs.insert( childId );

  dependentChildAlgorithmsRecursive( childId, algs );

  // remove temporary target alg
  algs.remove( childId );

  return algs;
}


void QgsProcessingModelAlgorithm::dependsOnChildAlgorithmsRecursive( const QString &childId, QSet< QString > &depends ) const
{
  const QgsProcessingModelChildAlgorithm &alg = mChildAlgorithms.value( childId );

  // add direct dependencies
  Q_FOREACH ( const QString &c, alg.dependencies() )
  {
    if ( !depends.contains( c ) )
    {
      depends.insert( c );
      dependsOnChildAlgorithmsRecursive( c, depends );
    }
  }

  // check through parameter dependencies
  QMap<QString, QgsProcessingModelChildParameterSources> childParams = alg.parameterSources();
  QMap<QString, QgsProcessingModelChildParameterSources>::const_iterator paramIt = childParams.constBegin();
  for ( ; paramIt != childParams.constEnd(); ++paramIt )
  {
    Q_FOREACH ( const QgsProcessingModelChildParameterSource &source, paramIt.value() )
    {
      if ( source.source() == QgsProcessingModelChildParameterSource::ChildOutput && !depends.contains( source.outputChildId() ) )
      {
        depends.insert( source.outputChildId() );
        dependsOnChildAlgorithmsRecursive( source.outputChildId(), depends );
      }
    }
  }
}

QSet< QString > QgsProcessingModelAlgorithm::dependsOnChildAlgorithms( const QString &childId ) const
{
  QSet< QString > algs;

  // temporarily insert the target child algorithm to avoid
  // unnecessarily recursion though it
  algs.insert( childId );

  dependsOnChildAlgorithmsRecursive( childId, algs );

  // remove temporary target alg
  algs.remove( childId );

  return algs;
}

bool QgsProcessingModelAlgorithm::canExecute( QString *errorMessage ) const
{
  QMap< QString, QgsProcessingModelChildAlgorithm >::const_iterator childIt = mChildAlgorithms.constBegin();
  for ( ; childIt != mChildAlgorithms.constEnd(); ++childIt )
  {
    if ( !childIt->algorithm() )
    {
      if ( errorMessage )
      {
        *errorMessage = QObject::tr( "The model you are trying to run contains an algorithm that is not available: <i>%1</i>" ).arg( childIt->algorithmId() );
      }
      return false;
    }
  }
  return true;
}

QString QgsProcessingModelAlgorithm::asPythonCommand( const QVariantMap &parameters, QgsProcessingContext &context ) const
{
  if ( mSourceFile.isEmpty() )
    return QString(); // temporary model - can't run as python command

  return QgsProcessingAlgorithm::asPythonCommand( parameters, context );
}

QgsProcessingAlgorithm *QgsProcessingModelAlgorithm::createInstance() const
{
  QgsProcessingModelAlgorithm *alg = new QgsProcessingModelAlgorithm();
  alg->loadVariant( toVariant() );
  alg->setProvider( provider() );
  alg->setSourceFilePath( sourceFilePath() );
  return alg;
}

///@endcond
