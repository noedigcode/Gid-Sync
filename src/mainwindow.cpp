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

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "version.h"

#include <QDateTime>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFileInfo>
#include <QHostInfo>
#include <QInputDialog>
#include <QMessageBox>
#include <QTimer>

void MainWindow::Repo::logError(QString summary, QString errorString)
{
    ok = false;
    statusSummary = summary;
    statusLines.append(summary);
    if (!errorString.isEmpty()) {
        statusLines.append(errorString);
    }
}

void MainWindow::Repo::log(QString line)
{
    statusLines.append(line);
}

MainWindow::MainWindow(Args args, QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , mArgs(args)
{
    ui->setupUi(this);

    setupAboutPage();

    setWindowTitle(QString("%1 %2").arg(APP_NAME).arg(APP_VERSION));
    mTrayIcon.setToolTip(this->windowTitle());

    // Default to main page
    ui->stackedWidget->setCurrentWidget(ui->page_main);

    // Initialise repo info area
    on_listWidget_repos_currentItemChanged(ui->listWidget_repos->currentItem(),
                                           nullptr);

    // Load settings
    GidFile::Result r = mSettings.load();
    if (r.success) {
        print("Settings loaded.");
    } else {
        print("Failed to load settings: " + r.errorString);;
    }
    ui->label_settingsPath->setText("Settings path: " + mSettings.settingsFilePath());

    // Load repos from settings
    foreach (Settings::RepoPtr repoSettings, mSettings.repos) {
        initRepo(repoSettings);
    }

    // Set default client name if missing
    if (mSettings.ourName.isEmpty()) {
        mSettings.ourName = QString("%1/%2").arg(getHostname(), getUsername());
    }
    ui->label_settings_ourName->setText(mSettings.ourName);

    setupTrayIcon();

    guiTimer.start(1000, this);
}

MainWindow::~MainWindow()
{
    mSettings.save();

    delete ui;
}

void MainWindow::setupAboutPage()
{
    QString text = ui->label_about_appname->text();
    text.replace("%APP_NAME%", APP_NAME);
    ui->label_about_appname->setText(text);

    text = ui->label_about_appInfo->text();
    text.replace("%APP_VERSION%", APP_VERSION);
    text.replace("%APP_YEAR_FROM%", APP_YEAR_FROM);
    text.replace("%APP_YEAR%", APP_YEAR);
    text.replace("%QT_VERSION%", QT_VERSION_STR);
    ui->label_about_appInfo->setText(text);

    QString changelog = "Could not load changelog";
    QFile f("://changelog");
    if (f.open(QIODevice::ReadOnly)) {
        changelog = f.readAll();
    }
    ui->plainTextEdit_about_changelog->setPlainText(changelog);
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!imQuitting) {
        hide();
        event->ignore();
    }
}

void MainWindow::initRepo(Settings::RepoPtr repoSettings)
{
    RepoPtr repo(new Repo());
    repo->settings = repoSettings;
    connect(&(repo->timer), &QTimer::timeout, this, [=]()
    {
        refreshRepo(repo);
    });
    repo->timer.setSingleShot(true);
    repo->timer.setInterval(repoSettings->refreshRateMinutes * 60 * 1000);

    repos.append(repo);

    // Setup tray menus
    mTrayMenu.insertMenu(mTrayMenuFirstNonRepoAction, &(repo->submenu));

    repo->statusAction = repo->submenu.addAction(QIcon("://status"),
                                                  "Status: Initialising",
                            this, [=](){ onRepoStatusActionTriggered(repo); });

    repo->refreshAction = repo->submenu.addAction(QIcon("://refresh"),
                                                   "Refresh",
                            this, [=](){ refreshRepo(repo); });

    repo->pauseAction = repo->submenu.addAction(QIcon("://pause"),
                                                 "Pause",
                            this, [=](){ onRepoPauseActionTriggered(repo); });

    repo->openPathAction = repo->submenu.addAction(QIcon("://folder"),
                                                    "Open Folder",
                            this, [=](){ onRepoOpenPathActionTriggered(repo); });

    // Add to GUI list
    QListWidgetItem* item = new QListWidgetItem();
    item->setText(repo->settings->name);
    listItemRepoMap.insert(item, repo);
    ui->listWidget_repos->addItem(item);

    // Select item in list
    ui->listWidget_repos->setCurrentItem(item);

    refreshRepo(repo);
}

void MainWindow::startRepoTimer(RepoPtr repo)
{
    int msec = repo->settings->refreshRateMinutes * 60 * 1000;

    // Interval of zero means never refresh
    if (msec > 0) {
        repo->timer.start(msec);
    }
}

void MainWindow::refreshRepo(RepoPtr repo)
{
    bool contains = false;
    foreach (RefreshJobPtr job, refreshJobs) {
        if (job->repo == repo) {
            contains = true;
            break;
        }
    }

    if (!contains) {
        RefreshJobPtr job(new RefreshJob());
        job->repo = repo;
        repo->refreshing = true;
        refreshJobs.append(job);
        if (!refreshBusy) {
            processRefreshJobs();
        }
        updateRepoGui(repo);
        updateTrayIcon();
    }
}

void MainWindow::processRefreshJobs()
{
    if (refreshJobs.isEmpty()) {
        refreshBusy = false;
        return;
    }

    refreshBusy = true;
    RefreshJobPtr job = refreshJobs.value(0);

    switch (job->state) {
    case 0:
        refresh_init(job);
        break;
    case 1:
        refresh_ongoingOps(job);
        break;
    case 2:
        refresh_branchRemoteInfo(job);
        break;
    case 3:
        refresh_commit(job);
        break;
    case 4:
        refresh_fetch(job);
        break;
    case 5:
        refresh_compare(job);
        break;
    case 6:
        refresh_compareAfterRebase(job);
        break;
    case 7:
        refresh_pushAfterRebase(job);
        break;
    }
}

void MainWindow::popRefreshJob()
{
    if (!refreshJobs.isEmpty()) {
        refreshJobs.removeFirst();
    }
}

void MainWindow::refresh_successNext(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->refreshing = false;

    startRepoTimer(repo);

    updateRepoGui(repo);
    updateTrayIcon();

    popRefreshJob();
    refresh_continue();
}

void MainWindow::refresh_errorNext(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->refreshing = false;

    // Tray popup message
    if (!this->isVisible()) {
        mTrayIcon.showMessage(repo->settings->path,
                              repo->statusSummary);
    }

    updateRepoGui(repo);
    updateTrayIcon();

    popRefreshJob();
    refresh_continue();
}

void MainWindow::refresh_nextState(RefreshJobPtr job)
{
    job->state++;
    updateRepoGui(job->repo);
    refresh_continue();
}

void MainWindow::refresh_continue()
{
    threadWorker.doInGuiThread([=](){ processRefreshJobs(); });
}

void MainWindow::refresh_init(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->timer.stop();

    repo->statusLines.clear();
    repo->statusSummary.clear();

    repo->log(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    if (repo->settings->path.isEmpty()) {
        repo->logError("Path empty");
        refresh_errorNext(job);
        return;
    }

    Git git(repo->settings->path);
    if (!git.pathIsRepo().result) {
        repo->logError("Path is not a Git repo.");
        refresh_errorNext(job);
        return;
    }

    refresh_nextState(job);
}

void MainWindow::refresh_ongoingOps(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    Git git(repo->settings->path);
    int ongoingOp = git.getOngoingOperationState();
    if (ongoingOp != Git::OpNone) {
        QStringList ops;
        if (ongoingOp & Git::OpRebase) { ops.append("rebase"); }
        if (ongoingOp & Git::OpMerge) { ops.append("merge"); }
        if (ongoingOp & Git::OpCherryPick) { ops.append("cherry-pick"); }
        if (ongoingOp & Git::OpBisect) { ops.append("bisect"); }
        if (ops.isEmpty()) { ops.append("unknown"); }

        repo->logError(QString("Unsafe to sync, operation ongoing: %1.")
                           .arg(ops.join(", ")));
        refresh_errorNext(job);
        return;
    }

    refresh_nextState(job);
}

void MainWindow::refresh_branchRemoteInfo(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    // Get current branch name
    Git git(repo->settings->path);
    Git::Result<QString> s = git.currentBranch();
    if (!s.gitOutput.hasError) {
        repo->log("Detected current branch: " + s.result);
        job->branch = s.result;
    } else {
        repo->logError("Could not detect current branch. Possibly detached head.",
                       s.gitOutput.toString());
        refresh_errorNext(job);
        return;
    }
    repo->branch = job->branch;

    // TODO: Could detect remote to use here
    job->remote = "origin";
    repo->remote = job->remote;

    // Get remote URL
    Git::Output out = git.runGit("remote get-url " + job->remote);
    if (!out.hasError) {
        repo->remoteUrl = QString(out.stdoutput).trimmed();
    } else {
        repo->logError(QString("Could not get URL for remote: %1. Check if remote exists.")
                           .arg(job->remote));
        refresh_errorNext(job);
        return;
    }

    refresh_nextState(job);
}

void MainWindow::refresh_commit(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    Git git(repo->settings->path);
    Git::Result<bool> b = git.isRepoModified();
    if (b.gitOutput.hasError) {
        repo->logError("Git error occurred while checking if repo is modified",
                       b.gitOutput.toString());
        refresh_errorNext(job);
        return;
    }
    if (!b.result) {
        repo->log("Repo has not been modified locally.");
    } else {
        repo->log("Repo has been modified locally.");

        Git::Output out;

        // Add all changes
        out = git.runGit("add -A");
        if (out.hasError) {
            repo->logError("Git error occurred while adding all",
                           out.toString());
            refresh_errorNext(job);
            return;
        }

        // Commit
        repo->log("Committing local changes...");
        QString ourName = mSettings.ourName;
        ourName = ourName.replace("\"", "\\\"");
        QString args = QString("commit -m \"Changes from %1\"").arg(ourName);
        out = git.runGit(args);
        if (out.hasError) {
            repo->logError("Git error occurred while committing",
                           out.toString());
            refresh_errorNext(job);
            return;
        }

        // Confirm that repo is now unmodified
        b = git.isRepoModified();
        if (b.gitOutput.hasError) {
            repo->logError("Git error occurred while checking if repo is modified:",
                           b.gitOutput.toString());
            refresh_errorNext(job);
            return;
        }
        if (b.result) {
            repo->logError("Repo is still unclean after commit.");
            refresh_errorNext(job);
            return;
        } else {
            repo->log("Repo clean after commit.");
        }
    }

    refresh_nextState(job);
}

void MainWindow::refresh_fetch(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->log("Fetching...");
    updateRepoGui(repo);

    QString path = repo->settings->path; // For use in worker thread
    threadWorker.doInWorkerThread([=]()
    {
        // Fetch
        QString args = QString("fetch %1 %2").arg(job->remote, job->branch);
        Git git(path);
        Git::Output out = git.runGit(args);
        threadWorker.doInGuiThread([=]()
        {
            if (out.hasError) {
                repo->logError("Git error occurred while fetching.",
                               out.toString());
                refresh_errorNext(job);
                return;
            }
            refresh_nextState(job);
        });
    });
}

void MainWindow::refresh_compare(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    Git git(repo->settings->path);
    Git::Result<Git::Compare> c = git.compareWithHead(QString("%1/%2")
                                            .arg(job->remote, job->branch));
    if (c.gitOutput.hasError) {
        repo->logError("Git error while comparing:",
                       c.gitOutput.toString());
        refresh_errorNext(job);
        return;
    }

    if (c.result == Git::Compare::NoUpstream) {

        repo->logError("No relation between remote and HEAD. Aborting.");
        refresh_errorNext(job);
        return;

    } else if (c.result == Git::Compare::Equal) {

        repo->log("In sync! Done.");
        repo->ok = true;
        refresh_successNext(job);
        return;

    } else if (c.result == Git::Compare::Ahead) {

        refresh_ahead(job);

    } else if (c.result == Git::Compare::Behind) {

        refresh_behind(job);

    } else if (c.result == Git::Compare::Diverged) {

        refresh_diverged(job);

    }
}

void MainWindow::refresh_ahead(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->log("Ahead of remote. Pushing changes...");
    updateRepoGui(repo);

    QString path = repo->settings->path; // For use in worker thread
    threadWorker.doInWorkerThread([=]()
    {
        Git git(path);
        Git::Output out = git.runGit(QString("push %1 %2:%2")
                                              .arg(job->remote, job->branch));
        threadWorker.doInGuiThread([=]()
        {
            if (out.hasError) {
                repo->logError("Git error while pushing:",
                               out.toString());
                refresh_errorNext(job);
                return;
            }
            repo->log("Pushed successfully. In sync! Done.");
            repo->ok = true;
            refresh_successNext(job);
        });
    });
}

void MainWindow::refresh_behind(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->log("Behind remote. Fast-forwarding...");
    updateRepoGui(repo);

    QString path = repo->settings->path; // For use in worker thread
    threadWorker.doInWorkerThread([=]()
    {
        Git git(path);
        Git::Output out = git.runGit(QString("merge --ff --ff-only %1/%2")
                                              .arg(job->remote, job->branch));
        threadWorker.doInGuiThread([=]()
        {
            if (out.hasError) {
                repo->logError("Git error while merging:",
                               out.toString());
                refresh_errorNext(job);
                return;
            }
            repo->log("Merged successfully. In sync! Done.");
            repo->ok = true;
            refresh_successNext(job);
        });
    });
}

void MainWindow::refresh_diverged(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->log("Diverged from remote. Rebasing...");
    updateRepoGui(repo);

    QString path = repo->settings->path; // For use in worker thread
    threadWorker.doInWorkerThread([=]()
    {
        Git git(path);
        Git::Output out = git.runGit(QString("rebase %1/%2")
                                              .arg(job->remote, job->branch));
        threadWorker.doInGuiThread([=]()
        {
            if (out.hasError) {
                repo->logError("Git error while rebasing.",
                               out.toString());
                repo->log("Rebasing failed. There are likely conflicting changes."
                          " Resolve them and finish the rebase before trying again.");
                refresh_errorNext(job);
                return;
            }
            refresh_nextState(job);
        });
    });
}

void MainWindow::refresh_compareAfterRebase(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    // Compare again and confirm we are ahead
    Git git(repo->settings->path);
    Git::Result<Git::Compare> c = git.compareWithHead(QString("%1/%2")
                                            .arg(job->remote, job->branch));
    if (c.gitOutput.hasError) {
        repo->logError("Git error while comparing:",
                       c.gitOutput.toString());
        refresh_errorNext(job);
        return;
    }
    if (c.result != Git::Compare::Ahead) {
        repo->logError("We are not ahead."
                       " Something may have gone wrong with the rebase.");
        refresh_errorNext(job);
        return;
    }
    refresh_nextState(job);
}

void MainWindow::refresh_pushAfterRebase(RefreshJobPtr job)
{
    RepoPtr repo = job->repo;

    repo->log("We are ahead. Rebase went fine. Pushing...");
    updateRepoGui(repo);

    QString path = repo->settings->path; // For use in worker thread
    threadWorker.doInWorkerThread([=]()
    {
        Git git(path);
        Git::Output out = git.runGit(QString("push %1 %2:%2")
                                              .arg(job->remote, job->branch));
        threadWorker.doInGuiThread([=]()
        {
            if (out.hasError) {
                repo->logError("Git error while pushing:",
                               out.toString());
                refresh_errorNext(job);
                return;
            }
            repo->log("Pushed successfully. In sync! Done.");
            repo->ok = true;
            refresh_successNext(job);
        });
    });
}

void MainWindow::updateRepoGui(RepoPtr repo)
{
    static QIcon okIcon("://repo");
    static QIcon errorIcon("://repo_error");
    static QIcon pausedIcon("://pause");
    static QIcon refreshingIcon("://refresh_repo");

    QIcon icon;
    QString statusText;
    if (repo->refreshing) {
        statusText = "Refreshing";
        icon = refreshingIcon;
    } else if (!repo->ok) {
        statusText = "Error";
        icon = errorIcon;
    } else if (repo->timer.isActive()) {
        statusText = "OK";
        icon = okIcon;
    } else {
        statusText = "Paused";
        icon = pausedIcon;
    }

    repo->submenu.setIcon(icon);
    repo->submenu.setTitle(QString("%1 - %2")
                            .arg(repo->settings->name)
                            .arg(statusText));
    repo->statusAction->setText(QString("Status: %1")
                                .arg(statusText));
    repo->pauseAction->setEnabled(repo->timer.isActive());

    // Update repo info area if selected in list widget
    QListWidgetItem* item = listItemRepoMap.key(repo);
    if (item == ui->listWidget_repos->currentItem()) {
        ui->label_repoName->setText(repo->settings->name);
        ui->lineEdit_repoPath->setText(repo->settings->path);
        ui->label_repoBranchRemote->setText(QString("%1 @ %2")
                            .arg(repo->branch.isEmpty() ? "?" : repo->branch)
                            .arg(repo->remote.isEmpty() ? "?" : repo->remote));
        ui->lineEdit_repoRemoteUrl->setText(repo->remoteUrl);
        ui->label_repoStatus->setText(QString("Status: %1%2%3")
                      .arg(statusText)
                      .arg(repo->statusSummary.trimmed().isEmpty() ? "" : ": ")
                      .arg(repo->statusSummary.trimmed()));
        ui->plainTextEdit_repoLog->setPlainText(repo->statusLines.join("\n"));
        ui->pushButton_pause->setEnabled(repo->timer.isActive());
        updateRepoRefreshTimeInGui(repo);
    }
    // Update list item name
    if (item) {
        item->setIcon(icon);
        item->setText(QString("%1 - %2").arg(repo->settings->name,
                                             statusText));
    }
}

void MainWindow::timerEvent(QTimerEvent* event)
{
    if (event->timerId() == guiTimer.timerId()) {

        // Update remaining time to refresh displayed in GUI for selected repo
        RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
        if (repo) {
            updateRepoRefreshTimeInGui(repo);
        }

    }
}

void MainWindow::updateRepoRefreshTimeInGui(RepoPtr repo)
{
    QString text;
    if (repo->settings->refreshRateMinutes == 0) {
        text = "Auto-refresh disabled";
    } else {
        if (repo->refreshing) {
            text = "Refreshing";
        } else if (repo->timer.isActive()) {
            text = "Next refresh: ";
            int secsRemaining = repo->timer.remainingTime() / 1000;
            if (secsRemaining < 60) {
                text += QString("%1 secs").arg(secsRemaining);
            } else {
                text += QString("%1 mins").arg(secsRemaining / 60);
            }
        } else {
            text = "Auto-refresh paused";
        }
        text = QString("%1 (Rate: every %2 mins)")
                   .arg(text).arg(repo->settings->refreshRateMinutes);
    }
    ui->label_repoRefreshTime->setText(text);
}

void MainWindow::print(QString msg)
{
    qDebug() << msg;
}

QString MainWindow::getUsername()
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
#ifdef Q_OS_WIN
    return env.value("USERNAME");
#else
    return env.value("USER");
#endif
}

QString MainWindow::getHostname()
{
    return QHostInfo::localHostName();
}

void MainWindow::initTrayMenu()
{
    QMenu* menu = &mTrayMenu;

    QList<QAction*> actions = {
        ui->action_Refresh_All,
        ui->action_Quit,
        ui->action_Show_Hide
    };
    mTrayMenuFirstNonRepoAction = actions.value(0);

    menu->addActions(actions);
}

void MainWindow::setupTrayIcon()
{
    initTrayMenu();
    mTrayIcon.setContextMenu(&mTrayMenu);

    connect(&mTrayIcon, &QSystemTrayIcon::activated,
            this, [=](QSystemTrayIcon::ActivationReason reason)
    {
        if (reason == QSystemTrayIcon::Context) { return; }

        if (this->isVisible()) {
            this->hide();
        } else {
            this->show();
            this->activateWindow();
        }
    });

    connect(&mTrayIconTimer, &QTimer::timeout, this, [=]() { updateTrayIcon(); });
    updateTrayIcon();

    mTrayIcon.show();
}

void MainWindow::updateTrayIcon()
{
    static QIcon okIcon("://trayicon_ok");
    static QIcon errorIcon("://trayicon_error");
    static QIcon refreshIcon("://trayicon_refresh");
    static QIcon successIcon("://trayicon_success");

    bool errors = false;
    bool refreshing = false;

    mTrayIconTimer.stop();

    foreach (RepoPtr repo, repos) {
        if (repo->refreshing) { refreshing = true; }
        if (!repo->ok) { errors = true; }
    }

    if (refreshing) {
        mTrayIconState = TrayIconState::Refresh;
        mTrayIcon.setIcon(refreshIcon);
    } else if (!errors) {
        if (mTrayIconState == TrayIconState::Refresh) {
            mTrayIconState = TrayIconState::Ok;
            mTrayIcon.setIcon(successIcon);
            mTrayIconTimer.start(30000);
        } else {
            mTrayIcon.setIcon(okIcon);
        }
    } else {
        mTrayIcon.setIcon(errorIcon);
    }
}

void MainWindow::onRepoStatusActionTriggered(RepoPtr repo)
{
    // Show main window and select the repo
    this->show();
    this->activateWindow();
    QListWidgetItem* item = listItemRepoMap.key(repo);
    if (item) {
        ui->listWidget_repos->setCurrentItem(item);
    }
}

void MainWindow::onRepoPauseActionTriggered(RepoPtr repo)
{
    repo->timer.stop();
    updateRepoGui(repo);
}

void MainWindow::onRepoOpenPathActionTriggered(RepoPtr repo)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(repo->settings->path));
}

void MainWindow::on_pushButton_addRepo_clicked()
{
    QString path = QFileDialog::getExistingDirectory(this);
    if (!path.isEmpty()) {
        Settings::RepoPtr r(new Settings::Repo());
        r->name = QFileInfo(path).baseName();
        r->path = path;
        mSettings.repos.append(r);
        initRepo(r);
    }
}

void MainWindow::on_listWidget_repos_currentItemChanged(QListWidgetItem* current,
                                                        QListWidgetItem* /*previous*/)
{
    ui->groupBox_repo->setEnabled(current != nullptr);

    if (!current) {
        return;
    }
    RepoPtr repo = listItemRepoMap.value(current);
    if (!repo) {
        return;
    }

    updateRepoGui(repo);
}

void MainWindow::on_pushButton_repoOpenPath_clicked()
{
    RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
    if (!repo) { return; }

    onRepoOpenPathActionTriggered(repo);
}

void MainWindow::on_pushButton_refresh_clicked()
{
    RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
    if (!repo) { return; }

    refreshRepo(repo);
}

void MainWindow::on_pushButton_pause_clicked()
{
    RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
    if (!repo) { return; }

    onRepoPauseActionTriggered(repo);
}

void MainWindow::on_action_Refresh_All_triggered()
{
    foreach (RepoPtr repo, repos) {
        refreshRepo(repo);
    }
}

void MainWindow::on_action_Quit_triggered()
{
    imQuitting = true;
    close();
}

void MainWindow::on_action_Show_Hide_triggered()
{
    if (this->isVisible()) {
        this->hide();
    } else {
        this->show();
        this->activateWindow();
    }
}

void MainWindow::on_toolButton_ourName_edit_clicked()
{
    QString name = QInputDialog::getText(this, "Client Name", "Name",
                                         QLineEdit::Normal, mSettings.ourName);
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    mSettings.ourName = name;
    ui->label_settings_ourName->setText(name);
}

void MainWindow::on_toolButton_editRepoName_clicked()
{
    RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
    if (!repo) { return; }

    QString name = QInputDialog::getText(this, "Repo Name", "Name",
                                         QLineEdit::Normal, repo->settings->name);
    name = name.trimmed();
    if (name.isEmpty()) { return; }

    repo->settings->name = name;
    updateRepoGui(repo);
}

void MainWindow::on_toolButton_editRepoRefreshTime_clicked()
{
    RepoPtr repo = listItemRepoMap.value(ui->listWidget_repos->currentItem());
    if (!repo) { return; }

    bool ok = true;
    int mins = QInputDialog::getInt(this, "Repo Refresh Rate",
                                    "Minutes (zero to disable auto-refresh)",
                                    repo->settings->refreshRateMinutes,
                                    0, 1000, 1, &ok);
    if (!ok) { return; }

    if (mins != repo->settings->refreshRateMinutes) {

        int lastInterval = repo->settings->refreshRateMinutes;
        repo->settings->refreshRateMinutes = mins;

        if (mins == 0) {
            // Stop timer.
            repo->timer.stop();
        } else {
            if (repo->timer.isActive()) {
                // Restart the timer with the new interval if it is already active
                startRepoTimer(repo);
            } else {
                // If the timer is not active, only restart it if the previous
                // interval was zero (i.e. timer deactivated).
                // Otherwise, timer is inactive due to error, and then we don't
                // automatically restart the timer.
                if (lastInterval == 0) {
                    startRepoTimer(repo);
                }
            }
        }

    }

    updateRepoGui(repo);
}

void MainWindow::on_toolButton_removeRepo_clicked()
{
    QListWidgetItem* item = ui->listWidget_repos->currentItem();
    if (!item) { return; }
    RepoPtr repo = listItemRepoMap.value(item);
    if (!repo) { return; }

    int choice = QMessageBox::question(this, "Remove Repo",
                        "Are you sure you want to remove the selected repo?");
    if (choice == QMessageBox::No) { return; }

    repo->timer.stop();
    mSettings.repos.removeAll(repo->settings);
    repos.removeAll(repo);
    listItemRepoMap.remove(item);
    delete item;
    mTrayMenu.removeAction(repo->submenu.menuAction());

    updateTrayIcon();
}

void MainWindow::on_action_Settings_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->page_settings);
}

void MainWindow::on_pushButton_settings_back_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_main);
}

void MainWindow::on_action_About_triggered()
{
    ui->stackedWidget->setCurrentWidget(ui->page_about);
}

void MainWindow::on_pushButton_about_back_clicked()
{
    ui->stackedWidget->setCurrentWidget(ui->page_main);
}

