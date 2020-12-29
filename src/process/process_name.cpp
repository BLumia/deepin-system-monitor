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
#include "process_name.h"
#include "process.h"
#include "process_db.h"
#include "process_name_cache.h"
#include "tray_apps_cache.h"
#include "gui_apps_cache.h"
#include "desktop_entry_cache.h"
#include "common/common.h"
#include "common/thread_manager.h"
#include "system/system_monitor_thread.h"
#include "system/system_monitor.h"
#include "wm/wm_window_list.h"

#include <QFileInfo>
#include <QApplication>
#include <QUrl>

using namespace common::init;
using namespace common::core;
using namespace core::wm;
using namespace core::system;
using namespace core::process;

namespace core {
namespace process {

ProcessName::ProcessName(Process *proc)
{
    if (proc) {
        auto *cache = ProcessNameCache::instance();
        Q_ASSERT(cache != nullptr);

        if (cache->contains(proc->pid()) && cache->processName(proc->pid())->name() == proc->name()) {
            auto *procName = cache->processName(proc->pid());
            //            d->name = procName->name();
            //            d->normalizedName = procName->normalizedName();
            //            d->displayName = procName->displayName();
        } else {
            //            d->name = proc->name();
            //            d->normalizedName = ProcessName::normalizeProcessName(proc->name(), proc->cmdline());
            //            // display name
            //            d->displayName = getDisplayName(proc);

            //            auto *procName = new ProcessName();
            //            procName->d = d;
            //            cache->insert(proc->pid(), procName);
        }
    }
}

QString ProcessName::normalizeProcessName(const QString &source, const QByteArrayList &cmdline)
{
    if (cmdline.size() == 0 || source.length() < 15) {
        return source;
    }

    // windows apps case
    if (cmdline[0].length() > 3 && isalpha(cmdline[0].at(0)) && cmdline[0].at(1) == ':' && (cmdline[0].at(2) == '/' || cmdline[0].at(2) == '\\')) {
        return QFileInfo(cmdline[0]).fileName();
    }

    for (auto cmd : cmdline) {
        QString name = {basename(cmd)};
        if (name.startsWith(source.toLocal8Bit())) {
            return name;
        }
    }

    return source;
}

QString ProcessName::getDisplayName(Process *proc)
{
    std::shared_ptr<TrayAppsCache> trayAppsCache;
    std::shared_ptr<GuiAppsCache> guiAppsCache;
    std::shared_ptr<DesktopEntryCache> desktopEntryCache;
    std::shared_ptr<WMWindowList> windowList;

    auto *thread = ThreadManager::instance()->thread<SystemMonitorThread>(BaseThread::kSystemMonitorThread);
    if (thread) {
        auto *monitor = thread->threadJobInstance<SystemMonitor>();
        if (monitor) {
            auto processDB = monitor->processDB().lock();
            guiAppsCache = processDB->guiAppsCache().lock();
            trayAppsCache = processDB->trayAppsCache().lock();
            desktopEntryCache = processDB->desktopEntryCache().lock();
            windowList = processDB->windowList().lock();
        } // ::monitor
    } // ::monitorThreadPtr

#ifdef BUILD_WAYLAND

#else
    if (!proc->cmdline().isEmpty()) {
        if (trayAppsCache && trayAppsCache->isTrayApp(proc->pid())) {
            // process with tray window
            if (windowList) {
                auto title = windowList->getWindowTitle(proc->pid());

                if (!title.isEmpty()) {
                    return QString("%1: %2").arg(QApplication::translate("Process.Table", "Tray")).arg(title);

                } else if (proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE")) {
                    // can't grab window title, try use desktop file instead
                    auto desktopFile = proc->environ()["GIO_LAUNCHED_DESKTOP_FILE"];
                    if (desktopEntryCache) {
                        auto entry = desktopEntryCache->entryWithDesktopFile(desktopFile);
                        if (entry && !entry->displayName.isEmpty())
                            return QString("%1: %2").arg(QApplication::translate("Process.Table", "Tray")).arg(entry->displayName);
                    } // ::desktopEntryCache

                } else {
                    return QString("%1: %2").arg(QApplication::translate("Process.Table", "Tray")).arg(proc->name());
                }
            } // ::widnowList
        } // ::if(traysAppsCache)

        if (guiAppsCache && guiAppsCache->isGuiApp(proc->pid())) {
            // gui apps case, grab title with xcb call
            if (windowList) {
                auto title = windowList->getWindowTitle(proc->pid());

                if (!title.isEmpty()) {
                    if (proc->cmdline().size() > 1) {
                        auto url = QUrl(proc->cmdline()[proc->cmdline().size() - 1]);
                        auto finfo = QFileInfo(url.fileName());
                        auto rname = url.fileName();
                        rname.chop(finfo.completeSuffix().length() + 1);

                        // check if commandline ends with file name, if so prepend it before process name
                        if (url.isValid()
                            && (url.isLocalFile() || !url.host().isEmpty())
                            && !title.contains(rname)) {
                            return QString("%1 - %2").arg(rname).arg(title);

                        } else {
                            return title;
                        }
                    } // ::if(cmdline)
                } // ::if(title)
            } // ::if(windowList)
        } // ::if(guiAppsCache)

        if (desktopEntryCache && desktopEntryCache->entry(proc->name())) {
            // found desktop entry
            auto &entry = desktopEntryCache->entry(proc->name());
            if (!entry->startup_wm_class.isEmpty()) {
                //                d->normalizedName = entry->startup_wm_class;
            }
            if (!entry->displayName.isEmpty()) {
                if (proc->cmdline().size() > 1) {
                    // check if last arg of cmdline is url, if so take it's filename
                    auto url = QUrl(proc->cmdline()[proc->cmdline().size() - 1]);
                    if (url.isValid() && (url.isLocalFile() || !url.host().isEmpty()))
                        return QString("%1 - %2").arg(url.fileName()).arg(entry->displayName);
                }

                return entry->displayName;
            } // ::if(displayName)
        } // ::if(desktopEntryCache)

        // is shell?
        if (shellList.contains(proc->name())) {
            auto joined = proc->cmdline().join(' ');
            return QString(joined);
        }

        if (proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE") && proc->environ().contains("GIO_LAUNCHED_DESKTOP_FILE_PID") && proc->environ()["GIO_LAUNCHED_DESKTOP_FILE_PID"].toInt() == proc->pid()) {
            // has gio info set in environment
            auto desktopFile = proc->environ()["GIO_LAUNCHED_DESKTOP_FILE"];
            auto entry = desktopEntryCache->entryWithDesktopFile(desktopFile);
            if (entry && !entry->displayName.isEmpty())
                return entry->displayName;
        }

        if (proc->cmdline()[0].startsWith("/opt")) {
            QString fname = QFileInfo(QString(proc->cmdline()[0]).split(' ')[0]).fileName();
            auto entry = desktopEntryCache->entryWithSubName(fname);
            if (entry && !entry->displayName.isEmpty())
                return entry->displayName;
        }
    } // ::if(cmdline)
#endif

    return proc->name();
}

} // namespace process
} // namespace core
