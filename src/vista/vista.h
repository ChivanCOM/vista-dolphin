/*
 * Vista Dolphin: the switch for everything that makes Dolphin look and work like
 * Windows Vista's Explorer.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VISTA_VISTA_H
#define VISTA_VISTA_H

#include <QStandardPaths>
#include <QString>
#include <QtGlobal>

namespace Vista
{
/**
 * True unless DOLPHIN_VISTA=0 is set, in which case this build behaves like plain Dolphin
 * (handy for comparing, or for a session that should keep the stock look). Read once.
 */
inline bool enabled()
{
    static const bool on = qgetenv("DOLPHIN_VISTA") != "0";
    return on;
}

/**
 * The Vista Control Panel's command (https://github.com/ChivanCOM/vista-control-panel), or
 * BOONE's name for it; empty when neither is installed (its tasks and menu items then stay
 * hidden).
 */
inline QString controlPanel()
{
    for (const auto *cmd : {"vista-control-panel", "denairu-control-panel"}) {
        const QString path = QStandardPaths::findExecutable(QString::fromLatin1(cmd));
        if (!path.isEmpty()) {
            return path;
        }
    }
    return {};
}
}

#endif
