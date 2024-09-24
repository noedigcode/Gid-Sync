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

#ifndef THREADWORKER_H
#define THREADWORKER_H

#include <QObject>
#include <QThread>

#include <functional>

class ThreadWorker : public QObject
{
    Q_OBJECT
public:
    explicit ThreadWorker(QObject *parent = nullptr);
    ~ThreadWorker();

    void doInGuiThread(std::function<void()> function);
    void doInWorkerThread(std::function<void()> function);

private:
    QObject mWorkerThreadObject;
    QThread mWorkerThread;
};

#endif // THREADWORKER_H
