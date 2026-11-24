/*
 * Vista: drives in the navigation pane with Vista's capacity bar.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "placesdelegate.h"

#include "capacitybar.h"

#include <KFilePlacesModel>
#include <KFilePlacesView>

#include <QApplication>
#include <QDateTime>
#include <QIcon>
#include <QPainter>
#include <QStyle>

using namespace Vista;

namespace
{
constexpr int LateralMargin = 4; // KIO's, so icons and text line up with the other places
constexpr int TextToBar = 3;
constexpr int MaxBarWidth = 150;
constexpr qint64 SpaceRefreshMs = 30 * 1000;
}

PlacesDelegate::PlacesDelegate(KFilePlacesView *view, QAbstractItemDelegate *inner)
    : QAbstractItemDelegate(view)
    , m_view(view)
    , m_inner(inner)
{
    connect(inner, &QAbstractItemDelegate::sizeHintChanged, this, &QAbstractItemDelegate::sizeHintChanged);
}

bool PlacesDelegate::showsBar(const QModelIndex &index) const
{
    if (!index.data(KFilePlacesModel::CapacityBarRecommendedRole).toBool()) {
        return false;
    }
    const QUrl url = index.data(KFilePlacesModel::UrlRole).toUrl();
    return url.isLocalFile() && usedSpace(url.toLocalFile()) >= 0;
}

qreal PlacesDelegate::usedSpace(const QString &path) const
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_space.constFind(path);
    if (it != m_space.constEnd() && now - it->when < SpaceRefreshMs) {
        return it->used;
    }
    const DriveSpace space = driveSpace(path);
    const qreal used = space.isValid() ? space.used() : -1.0;
    m_space.insert(path, {used, now});
    return used;
}

int PlacesDelegate::barHeight(const QStyleOptionViewItem &option) const
{
    // about half a text line: clearly a bar, like Vista's, without making drives huge
    return qMax(7, (option.fontMetrics.height() + 1) / 2);
}

QSize PlacesDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!m_inner) {
        return {};
    }
    QSize size = m_inner->sizeHint(option, index);
    if (showsBar(index)) {
        const int needed = option.fontMetrics.height() + TextToBar + barHeight(option) + 2 * LateralMargin;
        const int iconRow = m_view->iconSize().height() + 2 * LateralMargin;
        size.setHeight(size.height() + qMax(0, needed - iconRow));
    }
    return size;
}

void PlacesDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    if (!m_inner) {
        return;
    }
    const auto *model = qobject_cast<const KFilePlacesModel *>(index.model());
    const auto access = model ? model->deviceAccessibility(index) : KFilePlacesModel::Accessible;
    const bool busy = access == KFilePlacesModel::SetupInProgress || access == KFilePlacesModel::TeardownInProgress;
    if (!model || busy || !showsBar(index)) {
        m_inner->paint(painter, option, index);
        return;
    }

    // a heading above the drive ("Devices") stays KIO's: let it paint, clipped to the heading
    const int iconSize = m_view->iconSize().width();
    QStyleOptionViewItem itemOption(option);
    itemOption.features |= QStyleOptionViewItem::HasDecoration;
    itemOption.decorationSize = QSize(iconSize, iconSize);
    const int plainHeight = QApplication::style()->sizeFromContents(QStyle::CT_ItemViewItem, &itemOption, {}, option.widget).height();
    const int headingHeight = qMax(0, m_inner->sizeHint(option, index).height() - plainHeight);
    if (headingHeight > 0) {
        painter->save();
        painter->setClipRect(QRect(option.rect.left(), option.rect.top(), option.rect.width(), headingHeight));
        m_inner->paint(painter, option, index);
        painter->restore();
    }

    QStyleOptionViewItem opt(option);
    opt.rect.setTop(option.rect.top() + headingHeight);
    const QRect r = opt.rect;
    painter->save();
    if (model->isHidden(index)) {
        painter->setOpacity(painter->opacity() * 0.6);
    }
    QApplication::style()->drawPrimitive(QStyle::PE_PanelItemViewItem, &opt, painter, option.widget);

    const bool ltr = opt.direction == Qt::LeftToRight;
    const bool selectedAndActive = (opt.state & QStyle::State_Selected) && (opt.state & QStyle::State_Active);
    const QIcon icon = index.data(Qt::DecorationRole).value<QIcon>();
    const QPixmap pixmap = icon.pixmap(QSize(iconSize, iconSize), m_view->devicePixelRatioF(), selectedAndActive ? QIcon::Selected : QIcon::Normal);
    const int iconX = ltr ? r.left() + LateralMargin : r.right() - LateralMargin - iconSize;
    painter->drawPixmap(iconX, r.top() + (r.height() - iconSize) / 2, pixmap);

    int actionWidth = LateralMargin;
    if (model->isTeardownOverlayRecommended(index)) {
        const int size = QApplication::style()->pixelMetric(QStyle::PM_SmallIconSize, nullptr, m_view);
        actionWidth += size;
        const QPixmap eject = QIcon::fromTheme(QStringLiteral("media-eject")).pixmap(QSize(size, size), m_view->devicePixelRatioF());
        const int x = ltr ? r.right() - actionWidth : r.left() + LateralMargin;
        painter->drawPixmap(x, r.top() + (r.height() - size) / 2, eject);
    }

    const int iconArea = LateralMargin + iconSize;
    QRect text((ltr ? iconArea : actionWidth) + LateralMargin + r.left(), r.top(), r.width() - iconArea - actionWidth - 2 * LateralMargin, r.height());
    const int lineHeight = opt.fontMetrics.height();
    const int bar = barHeight(opt);
    const int blockTop = r.top() + (r.height() - lineHeight - TextToBar - bar) / 2;
    painter->setFont(opt.font);
    painter->setPen(selectedAndActive ? opt.palette.color(QPalette::HighlightedText) : opt.palette.color(QPalette::Text));
    const QString name = index.data(Qt::DisplayRole).toString();
    painter->drawText(QRect(text.left(), blockTop, text.width(), lineHeight),
                      Qt::AlignVCenter | (ltr ? Qt::AlignLeft : Qt::AlignRight),
                      opt.fontMetrics.elidedText(name, Qt::ElideRight, text.width()));

    const int barWidth = qMin(text.width() - 2, MaxBarWidth);
    const int barX = ltr ? text.left() : text.right() - barWidth;
    paintCapacityBar(painter, QRect(barX, blockTop + lineHeight + TextToBar, barWidth, bar), usedSpace(index.data(KFilePlacesModel::UrlRole).toUrl().toLocalFile()));
    painter->restore();
}

bool PlacesDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return m_inner ? m_inner->editorEvent(event, model, option, index) : false;
}

bool PlacesDelegate::helpEvent(QHelpEvent *event, QAbstractItemView *view, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    return m_inner ? m_inner->helpEvent(event, view, option, index) : false;
}

#include "moc_placesdelegate.cpp"
