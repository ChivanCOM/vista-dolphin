/*
 * Vista Dolphin: the DOLPHIN_VISTA switch, and where to find the Vista Control Panel.
 *
 * SPDX-FileCopyrightText: 2026 Vista Dolphin contributors
 *
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
 * True unless DOLPHIN_VISTA=0 is set. Read once.
 *
 * DOLPHIN_VISTA=0 only switches off the code that checks this, which is mostly drawing:
 * the explorer.qss style sheet, unsynthesised bold and regular-weight breadcrumb crumbs,
 * the joined refresh button, the session-restore launcher, Vista's item highlight colours
 * and hover, the Vista column header painting, the wider icon-view padding, unstriped
 * Details rows, and the tab strip's base line and lifted close buttons.
 *
 * It is NOT a "plain Dolphin" switch. Still on: the Details Pane, command bar tasks, the
 * Organize menu (replacing the hamburger menu), the Views menu, the search box (and the
 * hidden search button), column headers in the Icons and Compact views, the Folders panel
 * header, "Favorite Links", the places colours and drive capacity bars, the always-shown
 * places selector, the breadcrumb spacing, and whatever the installed dolphinui.rc and
 * dolphinrc set (toolbar layout, window layout, settings).
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
