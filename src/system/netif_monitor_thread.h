/*
* Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
*
* Author:      maojj <maojunjie@uniontech.com>
* Maintainer:  maojj <maojunjie@uniontech.com>
*
* This program is free software: you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation, either version 3 of the License, or
* any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef NETIF_MONITOR_THREAD_H
#define NETIF_MONITOR_THREAD_H

#include "common/base_thread.h"
#include "netif_monitor.h"

using namespace common::core;

namespace core {
namespace system {

class NetifMonitorThread : public BaseThread
{
public:
    explicit NetifMonitorThread(QObject *parent = nullptr);
    ~NetifMonitorThread();

    int threadKey() const final;
    QObject *threadJobInstance() const;

private:
    NetifMonitor *m_netifMonitor;
};

inline int NetifMonitorThread::threadKey() const
{
    return kNetifMonitorThread;
}

inline QObject *NetifMonitorThread::threadJobInstance() const
{
    return m_netifMonitor;
}

} // namespace system
} // namespace core

#endif // NETIF_MONITOR_THREAD_H
