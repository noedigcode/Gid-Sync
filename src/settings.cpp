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

#include "settings.h"

#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>

Settings::Settings() {}

QString Settings::settingsFilePath()
{
    return QString("%1/%2").arg(settingsDir()).arg("gid-sync-settings");
}

QString Settings::settingsDir()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
}

GidFile::Result Settings::save()
{
    QDir dir(settingsDir());
    if (!dir.exists()) {
        if (dir.mkpath(dir.path())) {
            qDebug() << "Created settings directory: " + dir.path();
        } else {
            qDebug() << "Failed to create settings directory: " + dir.path();
        }
    }

    // Repos array
    QJsonArray aRepos;
    foreach (RepoPtr repo, repos) {
        aRepos.append(repo->toJson());
    }

    // Main settings object
    QJsonObject jMain;
    jMain.insert("repos", aRepos);
    jMain.insert("ourName", ourName);

    QJsonDocument jDoc;
    jDoc.setObject(jMain);

    GidFile::Result r = GidFile::write(settingsFilePath(), jDoc.toJson());

    return r;
}

GidFile::Result Settings::load()
{
    GidFile::ReadResult r = GidFile::read(settingsFilePath());

    if (r.result.success) {

        QJsonDocument jDoc = QJsonDocument::fromJson(r.data);

        QJsonObject jMain = jDoc.object();

        QJsonArray aRepos = jMain.value("repos").toArray();
        foreach (QJsonValue v, aRepos) {
            QJsonObject obj = v.toObject();
            RepoPtr repo(new Repo());
            repo->fromJson(obj);
            repos.append(repo);
        }

        ourName = jMain.value("ourName").toString();

    }

    return r.result;
}

QJsonObject Settings::Repo::toJson()
{
    QJsonObject j;
    j.insert("name", name);
    j.insert("path", path);
    j.insert("refreshRateMinutes", refreshRateMinutes);
    return j;
}

void Settings::Repo::fromJson(QJsonObject json)
{
    name = json.value("name").toString();
    path = json.value("path").toString();
    refreshRateMinutes = json.value("refreshRateMinutes").toInt();
}
