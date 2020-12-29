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
#include "wm_window_list.h"

#include "common/uevent_loop.h"
#include "common/uevent.h"
#include "wm_atom.h"
#include "process/gui_apps_cache.h"
#include "process/tray_apps_cache.h"
#include "common/thread_manager.h"
#include "system/system_monitor.h"
#include "system/system_monitor_thread.h"
#include "process/process_db.h"

#include <QtDBus>

#include <xcb/xcb.h>

using namespace core::process;
using namespace common::core;
using namespace core::system;

namespace core {
namespace wm {

struct XReplyDeleter {
    void operator()(void *p)
    {
        free(p);
    }
};
using XGetPropertyReply = std::unique_ptr<xcb_get_property_reply_t, XReplyDeleter>;

WMWindowList::WMWindowList(QObject *parent)
    : QObject(parent)
{
}

const QMap<uint64_t, QVector<uint>> WMWindowList::getWindowIcon(pid_t pid) const
{
    QMap<uint64_t, QVector<uint>> pixMap;

    auto search = m_cache.find(pid);
    WMWId winId = UINT32_MAX;
    if (search != m_cache.end()) {
        winId = search->second->winId;
    }

    auto *conn = m_conn.xcb_connection();
    auto cookie = xcb_get_property(conn, false, winId, m_conn.atom(WMAtom::_NET_WM_ICON), XCB_ATOM_ANY, 0, UINT32_MAX);
    XGetPropertyReply reply(xcb_get_property_reply(conn, cookie, nullptr));

    if (reply) {
        auto len = uint(xcb_get_property_value_length(reply.get()));
        if (len < 2)
            return {};

        uint *data, *ptr;
        uint width, height;
        uint pos = 0;
        data = reinterpret_cast<uint *>(xcb_get_property_value(reply.get()));
        ptr = data;
        while (pos + 2 < len) {
            width = *ptr++;
            height = *ptr++;
            // malformed data (non ARGB32)
            if (width == 0 || height == 0 || (len - pos) < (2 + width * height))
                return pixMap;

            QVector<uint> pix(int(width * height)); // assume image wouldn't be large enough to cause overflow
            std::copy(ptr, ptr + width * height, pix.data());
            union size_u sz;
            sz.s.w = width;
            sz.s.h = height;
            pixMap[sz.k] = pix;

            pos += (width * height + 2);
            ptr += width * height;
        }
    }
    return pixMap;
}

const QString WMWindowList::getWindowTitle(pid_t pid) const
{
    // process may have multiple window opened, each with a different title
    auto range = m_cache.equal_range(pid);
    QList<QString> titles;
    for (auto i = range.first; i != range.second; ++i)
        titles << i->second->title;

    if (titles.size() > 1) {
        QString title;
        // format: [abc]|[def]|[ghi]...
        for (int j = titles.size() - 1; j >= 0; j--) {
            title.append(QString("[%1]").arg(titles[j]));
            if (j > 0)
                title.append(" | ");
        }
        return title;
    } else if (titles.size() == 1) {
        return titles[0];
    } else {
        return {};
    }
}

const QList<pid_t> WMWindowList::getTrayProcessList() const
{
    auto trayWndList = getTrayWindows();
    QList<pid_t> pidList;
    for (auto &it : m_cache) {
        if (trayWndList.contains(it.second->winId))
            pidList << it.first;
    }
    return pidList;
}

const QList<pid_t> WMWindowList::getGuiProcessList() const
{
    QList<pid_t> list;
    for (auto &it : m_cache) {
        list << it.first;
    }
    return list;
}

void WMWindowList::scheduleUpdate(UEventLoop *loop, const struct timeval *interval)
{
    loop->installTimerEventFilter(kListUpdateTimer, true, UEvent::kEventPriorityHigh, interval, this);
}

bool WMWindowList::uevent(UEvent *event)
{
    if (!event)
        return false;

    if (event->type == UEvent::kEventTypeTimer) {
        auto *ev = dynamic_cast<UTimerEvent *>(event);
        if (ev->timerId == kListUpdateTimer) {
            updateWindowListCache();
            return true;
        }
    }

    return false;
}

QList<WMWId> WMWindowList::getTrayWindows() const
{
    QDBusInterface bus("com.deepin.dde.TrayManager", "/com/deepin/dde/TrayManager",
                       "org.freedesktop.DBus.Properties", QDBusConnection::sessionBus());
    auto reply = bus.call("Get", "com.deepin.dde.TrayManager", "TrayIcons");
    auto v = reply.arguments().first();
    auto argument = v.value<QDBusVariant>().variant().value<QDBusArgument>();

    argument.beginArray();
    QList<WMWId> winIds;
    while (!argument.atEnd()) {
        WMWId winId;

        argument >> winId;
        winIds << winId;
    }
    argument.endArray();

    return winIds;
}

void WMWindowList::updateWindowListCache()
{
    auto *conn = m_conn.xcb_connection();
    // get window info
    auto cookie = xcb_get_property(conn, 0, m_conn.rootWindow(), m_conn.atom(WMAtom::_NET_CLIENT_LIST_STACKING), XCB_ATOM_WINDOW, 0, UINT_MAX);
    xcb_flush(conn);
    XGetPropertyReply reply(xcb_get_property_reply(conn, cookie, nullptr));
    if (!reply)
        return;

    xcb_window_t *clientList = reinterpret_cast<xcb_window_t *>(xcb_get_property_value(reply.get()));
    auto len = xcb_get_property_value_length(reply.get());
    for (auto i = 0; i < len; i++) {
        auto wid = clientList[i];
        auto winfo = getWindowInfo(wid);
        if (winfo)
            m_cache.insert({winfo->pid, std::move(winfo)});
    }

    auto *thread = ThreadManager::instance()->thread<SystemMonitorThread>(BaseThread::kSystemMonitorThread);
    if (thread) {
        auto monitor = thread->threadJobInstance<SystemMonitor>();
        if (monitor) {
            //            // TODO: use qt signal or libevent custom fd event
        }
    }
}

WMWindow WMWindowList::getWindowInfo(WMWId winId)
{
    WMWindow window(new struct wm_window_t());
    window->winId = winId;

    auto *conn = m_conn.xcb_connection();
    // pid
    auto pidCookie = xcb_get_property(conn, 0, winId, m_conn.atom(WMAtom::_NET_WM_PID), XCB_ATOM_CARDINAL, 0, 4);
    // title
    auto netNameCookie = xcb_get_property(conn, 0, winId, m_conn.atom(WMAtom::_NET_WM_NAME), m_conn.atom(WMAtom::UTF8_STRING), 0, BUFSIZ);

    xcb_flush(conn);

    XGetPropertyReply pidReply(xcb_get_property_reply(conn, pidCookie, nullptr));
    if (pidReply && pidReply->type == XCB_ATOM_CARDINAL) {
        auto *pid = reinterpret_cast<pid_t *>(xcb_get_property_value(pidReply.get()));
        window->pid = *pid;
    } else
        window->pid = -1;

    const char *name {};
    int len {};
    xcb_atom_t encoding = XCB_NONE;
    XGetPropertyReply netNameReply(xcb_get_property_reply(conn, netNameCookie, nullptr));
    if (netNameReply && netNameReply->type != XCB_NONE) {
        name = reinterpret_cast<const char *>(xcb_get_property_value(netNameReply.get()));
        len = xcb_get_property_value_length(netNameReply.get());
        encoding = netNameReply->type;
    } else {
        auto nameCookie = xcb_icccm_get_wm_name(conn, winId);
        XGetPropertyReply nameReply(xcb_get_property_reply(conn, nameCookie, nullptr));
        if (nameReply && nameReply->type != XCB_NONE) {
            name = reinterpret_cast<const char *>(xcb_get_property_value(nameReply.get()));
            len = xcb_get_property_value_length(nameReply.get());
            encoding = nameReply->type;
        }
    }

    if (len != 0) {
        if (encoding == XCB_ATOM_STRING) {
            window->title = QString::fromLocal8Bit(name, len);
        } else if (encoding == m_conn.atom(WMAtom::UTF8_STRING)) {
            window->title = QString::fromUtf8(name, len);
        }
    }

    return window;
}

} // namespace wm
} // namespace core
