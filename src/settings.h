/******************************************************************************
 *
 * This file is part of Gid-Sync.
 * Copyright (C) 2024 Gideon van der Kolf
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#ifndef SETTINGS_H
#define SETTINGS_H

#include "gidfile.h"

#include <QJsonObject>
#include <QSharedPointer>
#include <QString>

class Settings
{
public:
    Settings();

    struct Repo
    {
        QString name;
        QString path;
        int refreshRateMinutes = 60;
        QJsonObject toJson();
        void fromJson(QJsonObject json);
    };
    typedef QSharedPointer<Repo> RepoPtr;

    QList<RepoPtr> repos;
    QString ourName;

    QString settingsFilePath();
    QString settingsDir();

    GidFile::Result save();
    GidFile::Result load();
};

#endif // SETTINGS_H
