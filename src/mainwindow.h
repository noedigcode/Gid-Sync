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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "git.h"
#include "settings.h"
#include "ThreadWorker.h"

#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QSystemTrayIcon>
#include <QTimer>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    struct Args {
        QString dummyArg;
    };

    // -------------------------------------------------------------------------

    struct Repo
    {
        Settings::RepoPtr settings;
        QTimer timer;
        bool ok = true;
        bool refreshing = false;
        QString statusSummary;
        QStringList statusLines;
        QString branch;
        QString remote;
        QString remoteUrl;

        void logError(QString summary, QString errorString = "");
        void log(QString line);

        QMenu submenu;
        QAction* statusAction = nullptr;
        QAction* refreshAction = nullptr;
        QAction* pauseAction = nullptr;
        QAction* openPathAction = nullptr;
    };
    typedef QSharedPointer<Repo> RepoPtr;

    // -------------------------------------------------------------------------

    MainWindow(Args args, QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;
    Args mArgs;
    Settings mSettings;

    void setupAboutPage();

    bool imQuitting = false;
    void closeEvent(QCloseEvent* event) override;

    QList<RepoPtr> repos;
    QMap<QListWidgetItem*, RepoPtr> listItemRepoMap;

    void initRepo(Settings::RepoPtr repoSettings);
    void startRepoTimer(RepoPtr repo);

    // -------------------------------------------------------------------------

    struct RefreshJob {
        RefreshJob() {}
        RepoPtr repo;
        int state = 0;
        QString branch;
        QString remote;
    };
    typedef QSharedPointer<RefreshJob> RefreshJobPtr;

    QList<RefreshJobPtr> refreshJobs;
    ThreadWorker threadWorker;

    void refreshRepo(RepoPtr repo);
    bool refreshBusy = false;
    void processRefreshJobs();
    void popRefreshJob();

    void refresh_successNext(RefreshJobPtr job);
    void refresh_errorNext(RefreshJobPtr job);
    void refresh_nextState(RefreshJobPtr job);
    void refresh_continue();

    void refresh_init(RefreshJobPtr job);
    void refresh_ongoingOps(RefreshJobPtr job);
    void refresh_branchRemoteInfo(RefreshJobPtr job);
    void refresh_commit(RefreshJobPtr job);
    void refresh_fetch(RefreshJobPtr job);
    void refresh_compare(RefreshJobPtr job);
    void refresh_ahead(RefreshJobPtr job);
    void refresh_behind(RefreshJobPtr job);
    void refresh_diverged(RefreshJobPtr job);
    void refresh_compareAfterRebase(RefreshJobPtr job);
    void refresh_pushAfterRebase(RefreshJobPtr job);

    // -------------------------------------------------------------------------

    void updateRepoGui(RepoPtr repo);
    QBasicTimer guiTimer;
    void timerEvent(QTimerEvent *event);
    void updateRepoRefreshTimeInGui(RepoPtr repo);

    void print(QString msg);

    QString getUsername();
    QString getHostname();

    // -------------------------------------------------------------------------

    QSystemTrayIcon mTrayIcon;
    QMenu mTrayMenu;
    QAction* mTrayMenuFirstNonRepoAction = nullptr;
    void initTrayMenu();
    void setupTrayIcon();

    QTimer mTrayIconTimer;
    enum class TrayIconState { Ok, Refresh, Success, Error };
    TrayIconState mTrayIconState = TrayIconState::Ok;
    void updateTrayIcon();

    // -------------------------------------------------------------------------

private slots:
    void onRepoStatusActionTriggered(RepoPtr repo);
    void onRepoPauseActionTriggered(RepoPtr repo);
    void onRepoOpenPathActionTriggered(RepoPtr repo);

    void on_pushButton_addRepo_clicked();
    void on_listWidget_repos_currentItemChanged(QListWidgetItem *current,
                                                QListWidgetItem *previous);
    void on_pushButton_repoOpenPath_clicked();
    void on_pushButton_refresh_clicked();
    void on_pushButton_pause_clicked();
    void on_action_Refresh_All_triggered();
    void on_action_Quit_triggered();
    void on_action_Show_Hide_triggered();
    void on_toolButton_ourName_edit_clicked();
    void on_toolButton_editRepoName_clicked();
    void on_toolButton_editRepoRefreshTime_clicked();
    void on_toolButton_removeRepo_clicked();
    void on_action_Settings_triggered();
    void on_pushButton_settings_back_clicked();
    void on_action_About_triggered();
    void on_pushButton_about_back_clicked();
};

#endif // MAINWINDOW_H
