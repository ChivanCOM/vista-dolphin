/*
 * Vista: Windows Vista's drive capacity bar, for the navigation pane and the Details Pane.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VISTA_CAPACITYBAR_H
#define VISTA_CAPACITYBAR_H

#include <QString>
#include <QWidget>

class QPainter;

namespace Vista
{
/**
 * Paints Vista's capacity bar in @p rect (frame included): measured from Vista Explorer's
 * Computer folder - a 1 px grey frame (lighter on top, darker at the bottom), a pale
 * bevelled trough, and the used part in glossy cyan over deep teal; red once the drive is
 * more than 90 % full. Any height works; Vista's own is 13 px.
 */
void paintCapacityBar(QPainter *painter, const QRect &rect, qreal used);

/** Space on the file system holding a local path (all zero when unknown). */
struct DriveSpace {
    qint64 total = 0;
    qint64 free = 0;
    QString fileSystem;
    bool isValid() const
    {
        return total > 0;
    }
    qreal used() const
    {
        return total > 0 ? qreal(total - free) / qreal(total) : 0.0;
    }
};
DriveSpace driveSpace(const QString &path);
/** True when @p path is where a file system is mounted (a "drive" to Vista). */
bool isMountPoint(const QString &path);

/** The bar as a widget, Vista's 157 x 13 px. */
class CapacityBar : public QWidget
{
public:
    explicit CapacityBar(qreal used, QWidget *parent = nullptr);
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    qreal m_used;
};
}

#endif
