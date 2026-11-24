/*
 * Vista: Windows Vista's command bar tasks - buttons after Organize and Views that follow
 * what is selected (Preview, Slide Show, Play, E-mail, Extract All Files...).
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VISTA_COMMANDTASKS_H
#define VISTA_COMMANDTASKS_H

#include <KFileItem>

#include <QList>
#include <QObject>
#include <QPointer>
#include <QUrl>

#include <functional>

class DolphinView;
class KToolBar;
class QAction;
class QMainWindow;

namespace Vista
{
class CommandTasks : public QObject
{
    Q_OBJECT

public:
    struct Navigation {
        std::function<void(const QUrl &)> open;
        std::function<void(const QUrl &)> openInNewTab;
    };
    CommandTasks(QMainWindow *window, Navigation navigation);

    /** Follows @p view: its folder and its selection. */
    void setView(DolphinView *view);

private:
    void scheduleRefresh();
    void refresh();
    KToolBar *commandBar() const;
    QAction *addTask(const QString &text, const QString &icon, std::function<void()> run);
    void addOpenTask(const KFileItemList &items, const QString &text);
    void runCommand(const QString &program, const QStringList &args);

    QMainWindow *m_window;
    Navigation m_navigation;
    QPointer<DolphinView> m_view;
    QList<QPointer<QAction>> m_tasks;
    bool m_refreshPending = false;
};
}

#endif
