# SPDX-License-Identifier: GPL-2.0-or-later
# Vista Dolphin: the files that complete Windows Vista Explorer's look (see README.vista.md).

# Explorer's command bar, address field and tabs: Dolphin loads this style sheet at start.
set(VISTA_DATA_DIR "${KDE_INSTALL_FULL_DATADIR}/vista-dolphin")
set(EXPLORER_DIR "${VISTA_DATA_DIR}")
configure_file(explorer.qss ${CMAKE_CURRENT_BINARY_DIR}/explorer.qss @ONLY)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/explorer.qss arrow-down-white.svg DESTINATION ${KDE_INSTALL_DATADIR}/vista-dolphin)

# Explorer's toolbar layout (address bar row, command bar row with Organize and Views).
install(FILES dolphinui.rc DESTINATION ${KDE_INSTALL_KXMLGUIDIR}/dolphin)

# Explorer's default window layout and settings as the system dolphinrc.
option(VISTA_INSTALL_DEFAULTS "Install Vista Explorer defaults as the system-wide dolphinrc" ON)
if(VISTA_INSTALL_DEFAULTS)
    install(FILES dolphinrc DESTINATION ${KDE_INSTALL_CONFDIR})
endif()
