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

#ifndef GIT_H
#define GIT_H

#include <QDir>
#include <QFileInfo>
#include <QObject>
#include <QProcess>
#include <QStringList>

class Git : public QObject
{
    Q_OBJECT
public:
    explicit Git(QObject *parent = 0);
    explicit Git(QString path, QObject *parent = 0);

    struct Output {
        QString command;
        QByteArray stdoutput;
        QByteArray erroroutput;
        int exitcode = -1;
        bool hasError = false;
        QString toString() const;
    };

    template <typename T>
    struct Result {
        T result;
        Result() {}
        Result(T value) : result(value) {}
        Output gitOutput;
    };

    void setPath(QString path);
    QString path();

    Result<bool> isRepoModified(QString path = "");
    Result<bool> pathIsRepo(QString path = "");
    Result<bool> isBareRepository(QString path = "");
    Output initBareRepo(QString path = "");
    Output gitGui(QString path = "");
    Output gitkAll(QString path = "");
    Output cleanDryRun(QString path = "");
    Output clean(QString path = "");
    Output resetHard(QString path = "");

    enum OngoingOperation {
        OpNone = 0x00,
        OpRebase = 0x01,
        OpMerge = 0x02,
        OpCherryPick = 0x04,
        OpBisect = 0x08
    };
    int getOngoingOperationState(QString path = "");

    enum class Compare {
        NoUpstream,
        Equal,
        Ahead,
        Behind,
        Diverged
    };
    Result<Compare> compareWithHead(QString ref, QString path = "");

    Result<QString> currentBranch(QString path = "");

    QString getGitCmd();
    void setGitCmd(QString c);

    Output runGit(QString arguments, QString path = "");

private:
    void init();

    QString mPath;
    QString mGitCmd;

    QProcess mProcess;
    Output run(QString path, QString cmd);
};

#endif // GIT_H
