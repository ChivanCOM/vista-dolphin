#!/usr/bin/env python3
"""Print the Explorer window layout for Dolphin as a dolphinrc [MainWindow] State value.

Dolphin (KXmlGui) ignores newline="true" for a second toolbar when it has no saved
layout, so both rows would share one line. A saved QMainWindow state puts the command
bar on its own row under the address bar and fixes the side panels: Vista's navigation
pane = Places with the Folders tree under it; the Details Pane along the bottom (patched
Dolphin only; the normal one ignores it); Information and Terminal hidden until switched on.
Object names are Dolphin's own (src/dolphinmainwindow.cpp)."""
import os
import sys

os.environ.setdefault("QT_QPA_PLATFORM", "offscreen")
from PyQt6.QtCore import QCoreApplication, Qt  # noqa: E402
from PyQt6.QtWidgets import QApplication, QDockWidget, QMainWindow, QToolBar, QWidget  # noqa: E402

app = QApplication(sys.argv)
w = QMainWindow()
# the patched Dolphin's Details Pane runs the full width, under the navigation pane too
w.setCorner(Qt.Corner.BottomLeftCorner, Qt.DockWidgetArea.BottomDockWidgetArea)
w.setCorner(Qt.Corner.BottomRightCorner, Qt.DockWidgetArea.BottomDockWidgetArea)
w.setCentralWidget(QWidget())
w.resize(1024, 680)
for name in ("mainToolBar", "commandBar"):
    tb = QToolBar(name)
    tb.setObjectName(name)
    if name == "commandBar":
        w.addToolBarBreak(Qt.ToolBarArea.TopToolBarArea)
    w.addToolBar(Qt.ToolBarArea.TopToolBarArea, tb)
docks = {}
for name, area, shown in (("placesDock", Qt.DockWidgetArea.LeftDockWidgetArea, True),
                          ("foldersDock", Qt.DockWidgetArea.LeftDockWidgetArea, True),
                          ("infoDock", Qt.DockWidgetArea.RightDockWidgetArea, False),
                          ("terminalDock", Qt.DockWidgetArea.BottomDockWidgetArea, False),
                          ("vistaDetailsDock", Qt.DockWidgetArea.BottomDockWidgetArea, True)):
    d = QDockWidget(name)
    d.setObjectName(name)
    d.setWidget(QWidget())
    w.addDockWidget(area, d)
    d.setVisible(shown)
    docks[name] = d
w.splitDockWidget(docks["placesDock"], docks["foldersDock"], Qt.Orientation.Vertical)   # Folders under Places
w.show()
w.resizeDocks([docks["placesDock"]], [170], Qt.Orientation.Horizontal)   # Vista: ~166 px
w.resizeDocks([docks["placesDock"], docks["foldersDock"]], [360, 230], Qt.Orientation.Vertical)
for _ in range(5):
    QCoreApplication.processEvents()
print(bytes(w.saveState().toBase64()).decode())
