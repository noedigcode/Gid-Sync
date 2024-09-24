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

#include "ThreadWorker.h"

ThreadWorker::ThreadWorker(QObject *parent)
    : QObject{parent}
{
    mWorkerThreadObject.moveToThread(&mWorkerThread);
    mWorkerThread.start();
}

ThreadWorker::~ThreadWorker()
{
    mWorkerThread.quit();
    mWorkerThread.wait(10000);
}

void ThreadWorker::doInGuiThread(std::function<void ()> function)
{
    QMetaObject::invokeMethod(this, function, Qt::QueuedConnection);
}

void ThreadWorker::doInWorkerThread(std::function<void ()> function)
{
    QMetaObject::invokeMethod(&mWorkerThreadObject, function,
                              Qt::QueuedConnection);
}
