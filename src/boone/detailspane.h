/*
 * BOONE: Windows Vista's Details Pane for Dolphin.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef BOONE_DETAILSPANE_H
#define BOONE_DETAILSPANE_H

#include <KFileItem>

#include <QPointer>
#include <QWidget>

class DolphinView;
class QGridLayout;
class QLabel;
class KJob;

namespace Boone
{
/**
 * The strip along the bottom of a Windows Vista Explorer window: a large icon (or a
 * thumbnail) of the selected item, its name, and its details as grey-key / black-value pairs.
 * With nothing selected it describes the folder being shown.
 */
class DetailsPane : public QWidget
{
    Q_OBJECT

public:
    explicit DetailsPane(QWidget *parent = nullptr);

    /** Follows @p view: its selection, its folder and its item count. */
    void setView(DolphinView *view);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void refresh();
    void setPairs(const QList<QPair<QString, QString>> &pairs);
    void showIcon(const QString &iconName);
    void requestPreview(const KFileItem &item);

    QPointer<DolphinView> m_view;
    QString m_statusText;
    QLabel *m_icon;
    QLabel *m_name;
    QLabel *m_summary;
    QGridLayout *m_pairs;
    QPointer<KJob> m_previewJob;
    KFileItem m_shownItem;
};
}

#endif
