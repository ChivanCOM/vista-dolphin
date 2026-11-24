/*
 * SPDX-FileCopyrightText: 2010 Peter Penz <peter.penz19@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "dolphindockwidget.h"

#include <KConfigGroup>
#include <KLocalizedString>
#include <KSharedConfig>

#include <QEvent>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QTimer>

namespace
{
// Disable the 'Floatable' feature, i.e., the possibility to drag the
// dock widget out of the main window. This works around problems like
// https://bugs.kde.org/show_bug.cgi?id=288629
// https://bugs.kde.org/show_bug.cgi?id=322299
const QDockWidget::DockWidgetFeatures DefaultDockWidgetFeatures = QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetClosable;
}

// Titlebar for the dock widgets when "Lock Layout" has been activated: empty, except for the
// Folders panel, which gets Windows Vista's "Folders" header (Vista). Clicking it folds the
// tree away to just the header, leaving the room to the Favorite Links above, and back.
class DolphinDockTitleBar : public QWidget
{
    Q_OBJECT

public:
    explicit DolphinDockTitleBar(QDockWidget *dock)
        : QWidget(dock)
        , m_dock(dock)
    {
        setAttribute(Qt::WA_Hover);
        setMouseTracking(true);
    }
    ~DolphinDockTitleBar() override
    {
    }

    bool isHeader() const
    {
        return m_dock->objectName() == QLatin1String("foldersDock");
    }

    QSize minimumSizeHint() const override
    {
        if (!isHeader()) {
            return QSize(0, 0);
        }
        // the same height as a row in the panels above and below, plus room to breathe
        const int height = fontMetrics().height() + 10;
        return QSize(0, height);
    }

    QSize sizeHint() const override
    {
        return minimumSizeHint();
    }

    bool isCollapsed() const
    {
        // isHidden(), not !isVisible(): the tree is only folded when we hid it ourselves, not
        // because the dock or the window is not (yet) shown
        return m_dock->widget() && m_dock->widget()->isHidden();
    }

    void setCollapsed(bool collapsed)
    {
        QWidget *content = m_dock->widget();
        auto mainWindow = qobject_cast<QMainWindow *>(m_dock->parentWidget());
        if (!content || collapsed == isCollapsed()) {
            return;
        }
        if (collapsed) {
            m_expandedHeight = m_dock->height();
            content->setVisible(false);
            m_dock->setMaximumHeight(sizeHint().height());
        } else {
            content->setVisible(true);
            m_dock->setMaximumHeight(QWIDGETSIZE_MAX);
            if (mainWindow) {
                mainWindow->resizeDocks({m_dock}, {qMax(m_expandedHeight, 8 * fontMetrics().height())}, Qt::Vertical);
            }
        }
        KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("General"));
        group.writeEntry("VistaFoldersCollapsed", collapsed);
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        if (!isHeader()) {
            return;
        }
        QPainter painter(this);
        const QRect r = rect();
        const bool hovered = underMouse();
        painter.fillRect(r, palette().base());
        if (hovered) {
            QLinearGradient hover(r.topLeft(), r.bottomLeft());
            hover.setColorAt(0, QColor(0xf2, 0xf8, 0xfd));
            hover.setColorAt(1, QColor(0xe1, 0xee, 0xf9));
            painter.fillRect(r, hover);
        }
        // Vista's thin divider above the header (measured #f1f1f1, two pixels)
        painter.setPen(QColor(0xe6, 0xe6, 0xe6));
        painter.drawLine(r.left(), r.top() + 1, r.right(), r.top() + 1);
        painter.setPen(QColor(0xf1, 0xf1, 0xf1));
        painter.drawLine(r.left(), r.top(), r.right(), r.top());

        const int margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
        const int arrow = 7;
        QRect textRect = r.adjusted(margin, 1, -(margin + arrow + margin), 0);
        painter.setPen(QColor(0x1e, 0x39, 0x5b)); // Vista's navy for pane headers
        painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, KLocalizedString::removeAcceleratorMarker(m_dock->windowTitle()));

        // chevron: down when the tree is shown (click folds it away), up when folded
        const QPointF c(r.right() - margin - arrow / 2.0, r.center().y() + 1);
        const qreal h = arrow / 2.0;
        const qreal dir = isCollapsed() ? -1 : 1;
        QPen pen(QColor(0x3b, 0x5a, 0x82), 1.5);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(pen);
        painter.drawPolyline(QPolygonF{QPointF(c.x() - h, c.y() - dir * h / 2), QPointF(c.x(), c.y() + dir * h / 2), QPointF(c.x() + h, c.y() - dir * h / 2)});
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        if (isHeader() && event->button() == Qt::LeftButton && rect().contains(event->position().toPoint())) {
            setCollapsed(!isCollapsed());
        }
    }

    bool event(QEvent *event) override
    {
        if (event->type() == QEvent::HoverEnter || event->type() == QEvent::HoverLeave) {
            update();
        }
        return QWidget::event(event);
    }

private:
    QDockWidget *m_dock;
    int m_expandedHeight = 0;
};

DolphinDockWidget::DolphinDockWidget(const QString &title, QWidget *parent, Qt::WindowFlags flags)
    : QDockWidget(title, parent, flags)
    , m_locked(false)
    , m_dockTitleBar(nullptr)
{
    setFeatures(DefaultDockWidgetFeatures);
}

DolphinDockWidget::~DolphinDockWidget()
{
}

void DolphinDockWidget::setLocked(bool lock)
{
    if (lock != m_locked) {
        m_locked = lock;

        if (lock) {
            if (!m_dockTitleBar) {
                auto titleBar = new DolphinDockTitleBar(this);
                m_dockTitleBar = titleBar;
                // Vista: come back folded if the Folders tree was folded last time
                QTimer::singleShot(0, titleBar, [titleBar]() {
                    const KConfigGroup group(KSharedConfig::openConfig(), QStringLiteral("General"));
                    if (titleBar->isHeader() && group.readEntry("VistaFoldersCollapsed", false)) {
                        titleBar->setCollapsed(true);
                    }
                });
            }
            setTitleBarWidget(m_dockTitleBar);
            setFeatures(QDockWidget::DockWidgetClosable);
        } else {
            if (auto titleBar = qobject_cast<DolphinDockTitleBar *>(m_dockTitleBar)) {
                titleBar->setCollapsed(false); // a folded panel can't be unfolded without its header
            }
            setTitleBarWidget(nullptr);
            setFeatures(DefaultDockWidgetFeatures);
        }
    }
}

bool DolphinDockWidget::isLocked() const
{
    return m_locked;
}

bool DolphinDockWidget::event(QEvent *event)
{
    switch (event->type()) {
    case QEvent::Show:
    case QEvent::Hide:
        if (event->spontaneous()) {
            // The Dolphin window has been minimized or restored. We do not want this to be interpreted like a user was toggling the visibility of this widget.
            // We return here so no QDockWidget::visibilityChanged() signal is emitted. This does not seem to happen either way on Wayland.
            return true;
        }
        [[fallthrough]];
    default:
        return QDockWidget::event(event);
    }
}

#include "dolphindockwidget.moc"
#include "moc_dolphindockwidget.cpp"
