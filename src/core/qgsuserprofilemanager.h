/***************************************************************************
                        qgsuserprofilemanager.h
     --------------------------------------
    Date                 :  Jul-2017
    Copyright            : (C) 2017 by Nathan Woodrow
    Email                : woodrow.nathan at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#ifndef QGSUSERPROFILEMANAGER_H
#define QGSUSERPROFILEMANAGER_H

#include <QSettings>
#include <QFileSystemWatcher>


#include "qgis.h"
#include "qgis_core.h"
#include "qgserror.h"
#include "qgsuserprofile.h"

#include <memory>

/** \ingroup core
 * User profile manager is used to manager list, and manage user profiles on the users machine.
 *
 * In QGIS 3 all settings, plugins, etc were moved into a %APPDATA%/profiles folder for each platform.
 * This allows for manage different user profiles per machine vs the single default one that was allowed in the
 * past.
 *
 * A user profile is all settings and anything that used to be found in .qgis3 in the users home folder.
 *
 * \since QGIS 3.0
 */
class CORE_EXPORT QgsUserProfileManager : public QObject
{
    Q_OBJECT

  public:

    /**
     * User profile manager used to manage user profiles for the instance of QGIS.
     */
    QgsUserProfileManager( const QString &rootLocation = QString(), QObject *parent = nullptr );

    /**
     * Resolves the profiles folder for the given path. Path will have \\profiles appended to the path
     * \param basePath The base path to resolve the path from to append the \\profiles folder to.
     * \return The root path to store user profiles.
     */
    static QString resolveProfilesFolder( const QString  &basePath = QString() );

    /**
     * Return the profile from the given root profile location.
     * If no name is given it returns a profile called "default".
     * By default will create the profile folder if not found.
     * By default will init the user settings.
     * \note Returns a new QgsUserProfile. Ownership transferred to caller.
     * \param defaultProfile The profile name to find. Empty profile name will return "default" for the name.
     * \param createNew Create the profile folder if it doesn't exist.
     * \param initSettings if the settings should be initialized
     * \return The user profile
     */
    QgsUserProfile *getProfile( const QString &defaultProfile = "default", bool createNew = true, bool initSettings = true ) SIP_FACTORY;

    /**
     * Set the root profile location for the profile manager. All profiles are loaded from this
     * location. Will also contain a profiles.ini for holding profile settings.
     * \param rootProfileLocation Path to the top level profile folder which contains folders for each profile.
     */
    void setRootLocation( QString rootProfileLocation );

    /**
     * Returns the path to the root profiles location.
     * \return The root path to the profiles folder.
     */
    QString rootLocation() { return mRootProfilePath; }

    /**
     * Check if the root location has been set for the manager.
     * \return True if the root location has been set.
     */
    bool rootLocationIsSet() const;

    /**
     * A list of all found profile names.
     * \return
     */
    QStringList allProfiles() const;

    /**
     * Check if a profile exists.
     * \return False if the profile can't be found.
     */
    bool profileExists( const QString &name ) const;

    /**
     * Returns the name of the default profile that has been set in .default.
     * First checks profile.ini in \\profiles folder
     * Then checks defaultProfile in global settings
     * Finally returns "default" if all else fails
     * \note Setting overrideLocalProfile in global settings will always ignore profiles.ini
     * \return The name of the default profile.
     */
    QString defaultProfileName() const;

    /**
     * Sets the default profile name. The default profile name is used when loading QGIS
     * with no arguments.
     * \param name The name of the profile to save.
     */
    void setDefaultProfileName( const QString &name );

    /**
     * Set the default profile name from the current active profile.
     */
    void setDefaultFromActive();

    /**
     * Return the profile found for a given name.
     * \param name The name of the profile to return.
     * \return A QgsUserprofile pointing to the location of the user profile.
     */
    QgsUserProfile *profileForName( const QString name ) const;

    /**
     * Create a user profile given by the name
     * \param name
     * \return A QgsError which report if there was any error creating the user profile.
     */
    QgsError createUserProfile( const QString &name );

    /**
     * Deletes a profile from the root profiles folder.
     * \note There is no undo on this as it deletes the folder from the machine.
     * \param name The name of the profile to delete.
     * \return A QgsError with a message if the profile failed to be deleted.
     */
    QgsError deleteProfile( const QString name );

    /**
     * The currently active user profile.
     * \return The currently active user profile.
     */
    QgsUserProfile *userProfile();

    /**
     * Sets the active profile in the manager.
     * This can only be set once.
     * Setting this again does nothing.
     *
     * \param profile The name of the active profile
     */
    void setActiveUserProfile( const QString &profile );

    /**
     * Starts a new instance of QGIS for the given profile.
     * \param name The profile to start QGIS with.
     */
    void loadUserProfile( const QString &name );

  signals:

    /**
     * Emitted when the list of profiles is changed.
     */
    void profilesChanged( );

  private:

    std::unique_ptr<QFileSystemWatcher> mWatcher;

    QString mRootProfilePath;

    std::unique_ptr<QgsUserProfile> mUserProfile;

    QString settingsFile();

    QSettings *mSettings = nullptr;
};

#endif // QGSUSERPROFILEMANAGER_H
