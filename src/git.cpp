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

#include "git.h"

Git::Git(QObject* parent)
    : QObject(parent)
{
    init();
}

Git::Git(QString path, QObject *parent)
    : QObject(parent)
    , mPath(path)
{
    init();
}

void Git::setPath(QString path)
{
    mPath = path;
}

QString Git::path()
{
    return mPath;
}

Git::Result<bool> Git::isRepoModified(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Result<bool> ret(false);

    if (!isBareRepository(path).result) {
        ret.gitOutput = runGit("status --porcelain", path);
        if (!ret.gitOutput.hasError) {
            // If output is empty, repo has no changes
            if (!ret.gitOutput.stdoutput.isEmpty()) {
                ret.result = true;
            }
        }
    }

    return ret;
}

Git::Result<bool> Git::pathIsRepo(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Result<bool> ret(false);

    // Quick & dirty check for normal repository (working dir)
    QStringList entries = QDir(path).entryList(QDir::Files | QDir::Dirs | QDir::Hidden);
    if (entries.contains(".git")) {
        ret.result = true;
        return ret;
    }

    // Quick & dirty check for bare repo
    if (    entries.contains("objects")
         && entries.contains("refs")
         && entries.contains("config")
         && entries.contains("HEAD") ) {
        ret.result = true;
        return ret;
    }

    return false; // Skip long proper check

    ret.gitOutput = runGit("rev-parse --git-dir", path);
    ret.result = !ret.gitOutput.hasError;

    return ret;
}

Git::Result<bool> Git::isBareRepository(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Result<bool> ret(false);
    ret.gitOutput = runGit("rev-parse --is-bare-repository", path);
    if (!ret.gitOutput.hasError) {
        ret.result = ret.gitOutput.stdoutput.startsWith("true");
    }

    return ret;
}

Git::Output Git::initBareRepo(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = runGit("init --bare", path);

    return out;
}

Git::Output Git::gitGui(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = runGit("gui", path);

    return out;
}

Git::Output Git::gitkAll(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = run(path, "gitk --all");

    return out;
}

Git::Output Git::cleanDryRun(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = runGit("clean -xdfn", path);

    return out;
}

Git::Output Git::clean(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = runGit("clean -xdf", path);

    return out;
}

Git::Output Git::resetHard(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Output out = runGit("reset --hard", path);

    return out;
}

int Git::getOngoingOperationState(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    // TODO: This only works for root dir.
    //       Can use git rev-parse --git-dir to get git dir from anywhere in
    //       work tree.

    int ret = OpNone;

    QDir dir(QString("%1/.git").arg(path));
    QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::Hidden);
    if (entries.contains("rebase-merge")) {
        ret |= OpRebase;
    } else if (entries.contains("rebase-apply")) {
        ret |= OpRebase;
    } else if (entries.contains("MERGE_HEAD")) {
        ret |= OpMerge;
    } else if (entries.contains("CHERRY_PICK_HEAD")) {
        ret |= OpCherryPick;
    } else if (entries.contains("BISECT_LOG")) {
        ret |= OpBisect;
    }

    return ret;
}

Git::Result<Git::Compare> Git::compareWithHead(QString ref, QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Result<Compare> ret(Compare::NoUpstream);

    QString args = QString("rev-list --count --left-right %1...HEAD").arg(ref);
    ret.gitOutput = runGit(args, path);
    if (!ret.gitOutput.hasError) {
        QStringList lr = QString(ret.gitOutput.stdoutput).simplified().split(" ");
        QString l = lr.value(0);
        QString r = lr.value(1);
        if ((l == "0") && (r == "0")) {
            ret.result = Compare::Equal;
        } else if (l == "0") {
            ret.result = Compare::Ahead;
        } else if (r == "0") {
            ret.result = Compare::Behind;
        } else if (l.isEmpty() && r.isEmpty()) {
            ret.result = Compare::NoUpstream;
        } else {
            ret.result = Compare::Diverged;
        }
    }

    return ret;
}

Git::Result<QString> Git::currentBranch(QString path)
{
    if (path.isEmpty()) { path = mPath; }

    Result<QString> ret;

    // The --quiet option will suppress an error output message
    // --short ensures only the branch hame is given, without refs/heads/

    ret.gitOutput = runGit(QString("symbolic-ref --quiet --short HEAD"), path);
    if (!ret.gitOutput.hasError) {
        ret.result = ret.gitOutput.stdoutput.trimmed();
    }

    return ret;
}

QString Git::getGitCmd()
{
    return mGitCmd;
}

void Git::setGitCmd(QString c)
{
    mGitCmd = c;
}

Git::Output Git::run(QString path, QString cmd)
{
    Output out;
    out.command = cmd;
    mProcess.setWorkingDirectory(path);
    mProcess.start(cmd);
    mProcess.waitForFinished();
    out.stdoutput = mProcess.readAllStandardOutput();
    out.erroroutput = mProcess.readAllStandardError();

    if (mProcess.exitStatus() == QProcess::CrashExit) {
        out.hasError = true;
    } else {
        out.exitcode = mProcess.exitCode();
        if (mProcess.exitCode() == 0) {
            out.hasError = false;
        } else {
            out.hasError = true;
        }
    }

    return out;
}

Git::Output Git::runGit(QString arguments, QString path)
{
    if (path.isEmpty()) { path = mPath; }

    return run(path, QString("%1 %2").arg(mGitCmd).arg(arguments));
}

void Git::init()
{
#ifdef Q_OS_WIN
    mGitCmd = "\"C:/Program Files/Git/bin/git.exe\"";
#else
    mGitCmd = "git";
#endif
}

QString Git::Output::toString() const
{
    QString s;
    s.append("Command: " + command + "\n");
    s.append(QString("Exitcode: %1 - %2\n")
                 .arg(exitcode).arg(exitcode == 0 ? "Success" : "Error"));
    s.append("Stdout:\n" + stdoutput + "\n");
    s.append("Stderr:\n" + erroroutput + "\n");
    return s;
}
