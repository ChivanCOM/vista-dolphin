/*
 * Vista: drives in the navigation pane with Vista's capacity bar.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VISTA_PLACESDELEGATE_H
#define VISTA_PLACESDELEGATE_H

#include <QAbstractItemDelegate>
#include <QHash>
#include <QPointer>

class KFilePlacesView;

namespace Vista
{
/**
 * Wraps KIO's places delegate. Drives get a taller row: the name, and under it Vista's
 * bevelled capacity bar (KIO draws a 2 px Breeze line). Everything else - headings, other
 * places, drives being mounted - is KIO's own drawing.
 */
class PlacesDelegate : public QAbstractItemDelegate
{
    Q_OBJECT

public:
    PlacesDelegate(KFilePlacesView *view, QAbstractItemDelegate *inner);

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index) override;
    bool helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index) override;

private:
    bool showsBar(const QModelIndex &index) const;
    qreal usedSpace(const QString &path) const;
    int barHeight(const QStyleOptionViewItem &option) const;

    KFilePlacesView *m_view;
    QPointer<QAbstractItemDelegate> m_inner;
    struct Cached {
        qreal used;
        qint64 when;
    };
    mutable QHash<QString, Cached> m_space;
};
}

#endif
