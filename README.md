![Gid-Sync Logo](images/gid-sync_64.png)
Gid-Sync
========

Automatically sync a folder with Git
------------------------------------

2024 Gideon van der Kolf, noedigcode@gmail.com

Gid-Sync is a Qt GUI app that attempts to automatically sync a folder with a
remote server using Git. This app is somewhere in-between
[git-sync](https://github.com/simonthum/git-sync) and
[SparkleShare](https://www.sparkleshare.org/). It provides a GUI, but is not as
complete and seamless as SparkleShare. It is similar to the functionality of
git-sync (in fact, its procedure was roughly copied from git-sync), but it adds
automatic syncing at fixed time intervals. It does not shield you from Git like
SparkleShare and allows you to manually refresh syncing.

Method of operation:
--------------------

A repo to be synced must be added to the app first. The app does not handle
initial cloning. The user has to clone a repo manually. The repo is then added
to the app by specifying the local path to the repo.

The repo refresh (sync) operation consists of the following steps:

* Some sanity checks are done first. If the repo is in the middle of a merge,
  rebase, cherry-pick or bisect, the sync operation is stopped and the user is
  alerted.
* If local changes have been made, everything is added and a commit is made.
* Fetch from the "origin" remote, using the current checked out branch.
* The current HEAD and fetched remote branch are compared.
* If local is ahead of remote and branches have not diverged (fast-forward is
  possible), a push is done.
* If local is behind remote and branches have not diverged, a fast-forward merge
  is done.
* If branches have diverged, a rebase is attempted. On success, a push is done.
  On failure due to conflicts, the sync operation is stopped and the user is
  alerted to resolve the conflict and finalise the rebase manually.
* Any other unexpected errors also stop the sync operation and alerts the user.

Features:
---------

* System tray icon which indicates when everything is fine, an ongoing refresh
  (sync) operation or when some error occurred.
* System tray icon context menu allowing refreshing repos.
* Multiple repos can be handled by the app.
* Auto-refresh (sync) with rate definable per repo in minutes.
* Main application window shows all repos being handled with settings per repo
  and Git output and error messages.

Not features:
-------------

* Initial cloning of a repo is not handled.
* Authentication is not handled by the app. On Windows, it is useful to use the
  Windows credential manager, allowing https repos to be used without typing in
  passwords. Alternatively, it is up to the user to set up an ssh agent or
  similar to handle authentication.
* Merge conflicts are not automatically handled. It is up to the user to resolve
  these when reported by the app.
* The filesystem is not watched for changes. Refreshing (syncing) is only
  triggered periodically by the user specified refresh rate, or manually by the
  user.

Requirements:
-------------

* Qt 5
* Git must be installed
* Windows or Linux (and probably Mac too)

Building:
---------

Open the gid-sync.pro file with QtCreator and build.

Or run the following from the command line:
```
mkdir build
cd build
qmake ../gid-sync.pro
make
```

