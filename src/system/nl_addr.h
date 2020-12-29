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
#ifndef NL_ADDR_H
#define NL_ADDR_H

#include <QByteArray>

#include <netlink/route/addr.h>
#include <netlink/addr.h>

namespace core {
namespace system {

/**
 * @brief Wrapper class around the rtnl_addr api
 */
class NLAddr
{
public:
    explicit NLAddr(struct rtnl_addr *addr, bool hold = false);
    ~NLAddr();

    int ifindex() const;
    int family() const;
    int prefixlen() const;
    int scope() const;
    QByteArray local() const;
    QByteArray peer() const;
    QByteArray broadcast() const;
    QByteArray multicast() const;
    QByteArray anycast() const;

private:
    struct rtnl_addr *m_addr;
    bool m_hold;
};

inline int NLAddr::ifindex() const
{
    return rtnl_addr_get_ifindex(m_addr);
}

inline int NLAddr::family() const
{
    return rtnl_addr_get_family(m_addr);
}

inline int NLAddr::prefixlen() const
{
    return rtnl_addr_get_prefixlen(m_addr);
}

inline int NLAddr::scope() const
{
    return rtnl_addr_get_scope(m_addr);
}

inline QByteArray NLAddr::local() const
{
    QByteArray buffer {};
    const size_t bufsiz = 128;
    char tmpbuf[bufsiz];

    auto *addr = rtnl_addr_get_local(m_addr);
    nl_addr2str(addr, tmpbuf, bufsiz);
    tmpbuf[bufsiz - 1] = '\0';
    buffer.append(tmpbuf);
    return buffer;
}

inline QByteArray NLAddr::peer() const
{
    QByteArray buffer {};
    const size_t bufsiz = 128;
    char tmpbuf[bufsiz];

    auto *addr = rtnl_addr_get_peer(m_addr);
    nl_addr2str(addr, tmpbuf, bufsiz);
    tmpbuf[bufsiz - 1] = '\0';
    buffer.append(tmpbuf);
    return buffer;
}

inline QByteArray NLAddr::broadcast() const
{
    QByteArray buffer {};
    const size_t bufsiz = 128;
    char tmpbuf[bufsiz];

    auto *addr = rtnl_addr_get_broadcast(m_addr);
    nl_addr2str(addr, tmpbuf, bufsiz);
    tmpbuf[bufsiz - 1] = '\0';
    buffer.append(tmpbuf);
    return buffer;
}

inline QByteArray NLAddr::multicast() const
{
    QByteArray buffer {};
    const size_t bufsiz = 128;
    char tmpbuf[bufsiz];

    auto *addr = rtnl_addr_get_multicast(m_addr);
    nl_addr2str(addr, tmpbuf, bufsiz);
    tmpbuf[bufsiz - 1] = '\0';
    buffer.append(tmpbuf);
    return buffer;
}

inline QByteArray NLAddr::anycast() const
{
    QByteArray buffer {};
    const size_t bufsiz = 128;
    char tmpbuf[bufsiz];

    auto *addr = rtnl_addr_get_anycast(m_addr);
    nl_addr2str(addr, tmpbuf, bufsiz);
    tmpbuf[bufsiz - 1] = '\0';
    buffer.append(tmpbuf);
    return buffer;
}

} // namespace system
} // namespace core

#endif // NL_ADDR_H
