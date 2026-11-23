/*
 * BOONE: Windows Vista's drive capacity bar.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "capacitybar.h"

#include <QDir>
#include <QLinearGradient>
#include <QPainter>
#include <QStorageInfo>

namespace Boone
{
void paintCapacityBar(QPainter *painter, const QRect &rect, qreal used)
{
    if (rect.width() < 4 || rect.height() < 4) {
        return;
    }
    used = qBound<qreal>(0.0, used, 1.0);
    const bool full = used > 0.9;
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, false);
    painter->setPen(Qt::NoPen);

    const QRect r = rect;
    const QRect in = r.adjusted(1, 1, -1, -1);
    const int h = in.height();

    // trough: shaded at the top and the bottom, near white in the middle
    QLinearGradient trough(0, in.top(), 0, in.bottom() + 1);
    trough.setColorAt(0.00, QColor(0xe2, 0xe2, 0xe3));
    trough.setColorAt(0.18, QColor(0xf2, 0xf2, 0xf3));
    trough.setColorAt(0.35, QColor(0xfb, 0xfb, 0xfb));
    trough.setColorAt(0.75, QColor(0xf8, 0xf8, 0xf9));
    trough.setColorAt(0.92, QColor(0xec, 0xec, 0xed));
    trough.setColorAt(1.00, QColor(0xd5, 0xd5, 0xd5));
    painter->fillRect(in, trough);

    // the used part: a lit upper half over a deep lower half, like Vista's glass
    const int fillWidth = qRound(in.width() * used);
    if (fillWidth > 0) {
        const QRect fill(in.left(), in.top(), fillWidth, h);
        const int split = qRound(h * 0.55);
        QLinearGradient upper(0, fill.top(), 0, fill.top() + split);
        QLinearGradient lower(0, fill.top() + split, 0, fill.bottom() + 1);
        if (full) {
            upper.setColorAt(0, QColor(0xf6, 0x8f, 0x8f));
            upper.setColorAt(0.25, QColor(0xec, 0x5e, 0x5e));
            upper.setColorAt(1, QColor(0xe0, 0x46, 0x46));
            lower.setColorAt(0, QColor(0xbe, 0x1b, 0x1b));
            lower.setColorAt(0.8, QColor(0xb0, 0x16, 0x16));
            lower.setColorAt(1, QColor(0xa4, 0x3c, 0x3c));
        } else {
            upper.setColorAt(0, QColor(0x7c, 0xd8, 0xf3));
            upper.setColorAt(0.25, QColor(0x4c, 0xc2, 0xe8));
            upper.setColorAt(1, QColor(0x3e, 0xb8, 0xe1));
            lower.setColorAt(0, QColor(0x08, 0x8c, 0xa9));
            lower.setColorAt(0.8, QColor(0x08, 0x88, 0xa4));
            lower.setColorAt(1, QColor(0x45, 0x8a, 0x9b));
        }
        painter->fillRect(QRect(fill.left(), fill.top(), fill.width(), split), upper);
        painter->fillRect(QRect(fill.left(), fill.top() + split, fill.width(), h - split), lower);
        // Vista's faint vertical sheen in the lit half
        if (h >= 8) {
            const QColor sheen = full ? QColor(255, 220, 220, 28) : QColor(0xb4, 0xf0, 0xff, 40);
            for (int x = fill.left() + 12; x < fill.right() - 1; x += 15) {
                painter->fillRect(QRect(x, fill.top() + 1, 1, split - 1), sheen);
            }
        }
        // a darker edge where the used part ends
        if (fill.right() < in.right()) {
            painter->fillRect(QRect(fill.right(), fill.top(), 1, h), full ? QColor(0x9a, 0x22, 0x22, 150) : QColor(0x16, 0x6f, 0x88, 150));
        }
    }

    // frame: light top, grey sides, dark bottom, softened corners
    painter->fillRect(QRect(r.left() + 1, r.top(), r.width() - 2, 1), QColor(0xb3, 0xb2, 0xb3));
    painter->fillRect(QRect(r.left() + 1, r.bottom(), r.width() - 2, 1), QColor(0x8b, 0x8b, 0x8b));
    QLinearGradient side(0, r.top(), 0, r.bottom());
    side.setColorAt(0, QColor(0xbb, 0xbb, 0xbc));
    side.setColorAt(1, QColor(0x94, 0x94, 0x95));
    painter->fillRect(QRect(r.left(), r.top() + 1, 1, r.height() - 2), side);
    painter->fillRect(QRect(r.right(), r.top() + 1, 1, r.height() - 2), side);
    const QColor corner(0xd8, 0xd8, 0xd9);
    for (const QPoint &p : {r.topLeft(), r.topRight(), r.bottomLeft(), r.bottomRight()}) {
        painter->fillRect(QRect(p, QSize(1, 1)), corner);
    }
    painter->restore();
}

DriveSpace driveSpace(const QString &path)
{
    DriveSpace space;
    if (path.isEmpty()) {
        return space;
    }
    const QStorageInfo info(path);
    if (info.isValid() && info.isReady() && info.bytesTotal() > 0) {
        space.total = info.bytesTotal();
        space.free = info.bytesAvailable();
        space.fileSystem = QString::fromUtf8(info.fileSystemType());
    }
    return space;
}

bool isMountPoint(const QString &path)
{
    if (path.isEmpty()) {
        return false;
    }
    const QStorageInfo info(path);
    if (!info.isValid()) {
        return false;
    }
    return QDir::cleanPath(info.rootPath()) == QDir::cleanPath(QDir(path).canonicalPath());
}

CapacityBar::CapacityBar(qreal used, QWidget *parent)
    : QWidget(parent)
    , m_used(used)
{
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
}

QSize CapacityBar::sizeHint() const
{
    return QSize(157, 13);
}

void CapacityBar::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    // centred on the text line it stands in for
    const QSize s = sizeHint();
    const QRect r(0, (height() - s.height()) / 2, qMin(width(), s.width()), s.height());
    paintCapacityBar(&painter, r, m_used);
}
}
