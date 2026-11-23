/*
 * SPDX-FileCopyrightText: 2014 Emmanuel Pescosta <emmanuelpescosta099@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphintabbar.h"

#include <KLocalizedString>

#include <QApplication>
#include <QDragEnterEvent>
#include <QInputDialog>
#include <QMenu>
#include <QMimeData>
#include <QPainter>
#include <QTimer>

class PreventFocusWhileHidden : public QObject
{
public:
    PreventFocusWhileHidden(QObject *parent)
        : QObject(parent){};

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        switch (ev->type()) {
        case QEvent::Hide:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::NoFocus);
            return false;
        case QEvent::Show:
            static_cast<QWidget *>(obj)->setFocusPolicy(Qt::TabFocus);
            return false;
        default:
            return false;
        }
    };
};

// BOONE: the theme's close glyph sits at the bottom of its 16 px icon, so a button centred in
// the tab shows its X low. Lift each close button 2 px whenever the tab bar places it.
class BooneLiftCloseButton : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *obj, QEvent *ev) override
    {
        if (ev->type() == QEvent::Move) {
            auto *button = static_cast<QWidget *>(obj);
            if (button->pos() != button->property("booneLifted").toPoint()) {
                const QPoint lifted = button->pos() - QPoint(0, 2);
                button->setProperty("booneLifted", lifted);
                button->move(lifted);
            }
        }
        return false;
    }
};

DolphinTabBar::DolphinTabBar(QWidget *parent)
    : QTabBar(parent)
    , m_autoActivationIndex(-1)
    , m_tabToBeClosedOnMiddleMouseButtonRelease(-1)
{
    setAcceptDrops(true);
    setSelectionBehaviorOnRemove(QTabBar::SelectPreviousTab);
    setMovable(true);
    setTabsClosable(true);

    setFocusPolicy(Qt::NoFocus);
    installEventFilter(new PreventFocusWhileHidden(this));

    m_autoActivationTimer = new QTimer(this);
    m_autoActivationTimer->setSingleShot(true);
    m_autoActivationTimer->setInterval(800);
    connect(m_autoActivationTimer, &QTimer::timeout, this, &DolphinTabBar::slotAutoActivationTimeout);
}

void DolphinTabBar::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
        updateAutoActivationTimer(index);
    }

    QTabBar::dragEnterEvent(event);
}

void DolphinTabBar::dragLeaveEvent(QDragLeaveEvent *event)
{
    updateAutoActivationTimer(-1);

    QTabBar::dragLeaveEvent(event);
}

void DolphinTabBar::dragMoveEvent(QDragMoveEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDragMoveEvent(index, event);
        updateAutoActivationTimer(index);
    }

    QTabBar::dragMoveEvent(event);
}

void DolphinTabBar::dropEvent(QDropEvent *event)
{
    // Disable the auto activation timer
    updateAutoActivationTimer(-1);

    const QMimeData *mimeData = event->mimeData();
    const int index = tabAt(event->position().toPoint());

    if (mimeData->hasUrls()) {
        Q_EMIT tabDropEvent(index, event);
    }

    QTabBar::dropEvent(event);
}

void DolphinTabBar::mousePressEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0 && event->button() == Qt::MiddleButton) {
        m_tabToBeClosedOnMiddleMouseButtonRelease = index;
        return;
    }

    QTabBar::mousePressEvent(event);
}

void DolphinTabBar::mouseReleaseEvent(QMouseEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0 && index == m_tabToBeClosedOnMiddleMouseButtonRelease && event->button() == Qt::MiddleButton) {
        // Mouse middle click on a tab closes this tab.
        Q_EMIT tabCloseRequested(index);
        return;
    }

    QTabBar::mouseReleaseEvent(event);
}

void DolphinTabBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        int index = tabAt(event->pos());

        if (index < 0) {
            // empty tabbar area case
            index = currentIndex();
        }
        // Double left click on the tabbar opens a new activated tab
        // with the url from the doubleclicked tab or currentTab otherwise.
        Q_EMIT openNewActivatedTab(index);
    }

    QTabBar::mouseDoubleClickEvent(event);
}

void DolphinTabBar::contextMenuEvent(QContextMenuEvent *event)
{
    const int index = tabAt(event->pos());

    if (index >= 0) {
        // Tab context menu
        QMenu menu(this);

        QAction *newTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-new")), i18nc("@action:inmenu", "New Tab"));
        QAction *detachTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-detach")), i18nc("@action:inmenu", "Detach Tab"));
        QAction *closeOtherTabsAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close-other")), i18nc("@action:inmenu", "Close Other Tabs"));
        QAction *closeTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("tab-close")), i18nc("@action:inmenu", "Close Tab"));

        QAction *renameTabAction = menu.addAction(QIcon::fromTheme(QStringLiteral("edit-rename")), i18nc("@action:inmenu", "Rename Tab"));

        QAction *selectedAction = menu.exec(event->globalPos());
        if (selectedAction == newTabAction) {
            Q_EMIT openNewActivatedTab(index);
        } else if (selectedAction == detachTabAction) {
            Q_EMIT tabDetachRequested(index);
        } else if (selectedAction == closeOtherTabsAction) {
            const int tabCount = count();
            for (int i = 0; i < index; i++) {
                Q_EMIT tabCloseRequested(0);
            }
            for (int i = index + 1; i < tabCount; i++) {
                Q_EMIT tabCloseRequested(1);
            }
        } else if (selectedAction == closeTabAction) {
            Q_EMIT tabCloseRequested(index);
        } else if (selectedAction == renameTabAction) {
            bool renamed = false;
            const QString tabNewName = QInputDialog::getText(this, i18nc("@title:window for text input", "Rename Tab"), i18n("New tab name:"), QLineEdit::Normal, tabText(index), &renamed);

            if (renamed) {
                Q_EMIT tabRenamed(index, tabNewName);
            }
        }

        return;
    }

    QTabBar::contextMenuEvent(event);
}

void DolphinTabBar::slotAutoActivationTimeout()
{
    if (m_autoActivationIndex >= 0) {
        setCurrentIndex(m_autoActivationIndex);
        updateAutoActivationTimer(-1);
    }
}

void DolphinTabBar::updateAutoActivationTimer(const int index)
{
    if (m_autoActivationIndex != index) {
        m_autoActivationIndex = index;

        if (m_autoActivationIndex < 0) {
            m_autoActivationTimer->stop();
        } else {
            m_autoActivationTimer->start();
        }
    }
}

#include "moc_dolphintabbar.cpp"

void DolphinTabBar::tabInserted(int index)
{
    QTabBar::tabInserted(index);
    if (!qEnvironmentVariableIsSet("DENAIRU_SESSION")) {
        return;
    }
    if (QWidget *button = tabButton(index, QTabBar::RightSide)) {
        static auto *lift = new BooneLiftCloseButton(qApp);
        button->installEventFilter(lift);
        button->move(button->pos()); // no-op unless already placed; the next placement lifts it
    }
}

// BOONE: the tab strip's base line (the tabs' outline colour) runs under every tab but the
// current one, which stays open into the folder below. DolphinTabWidget continues it
// beside the tabs.
void DolphinTabBar::paintEvent(QPaintEvent *event)
{
    QTabBar::paintEvent(event);
    if (!qEnvironmentVariableIsSet("DENAIRU_SESSION") || count() < 1) {
        return;
    }
    QPainter painter(this);
    painter.setPen(QColor(0x89, 0x8c, 0x95));
    const int y = rect().bottom();
    const QRect current = tabRect(currentIndex());
    if (current.isValid()) {
        painter.drawLine(0, y, current.left(), y);
        painter.drawLine(current.right(), y, width() - 1, y);
    } else {
        painter.drawLine(0, y, width() - 1, y);
    }
}
