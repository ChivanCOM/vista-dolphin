/*
 * BOONE: Windows Vista's Details Pane for Dolphin.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "detailspane.h"

#include "views/dolphinview.h"

#include <KIO/Global>
#include <KIO/PreviewJob>
#include <KLocalizedString>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLocale>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QVBoxLayout>

using namespace Boone;

namespace
{
constexpr int IconSize = 48;
const QColor KeyColor(0x5a, 0x67, 0x79); // Vista's grey for the labels
// measured from Vista Explorer: 70 px tall, a grey-blue top line, light shading under it,
// and a left-to-right wash from near white to light blue
const QColor TopLine(0xa7, 0xb1, 0xba);
constexpr int PaneHeight = 70;
constexpr int IconLeft = 18;   // icon's left edge
constexpr int TextGap = 23;    // icon to text (text starts at 89 px)
constexpr int TextTop = 9;     // first text line from the top line
}

DetailsPane::DetailsPane(QWidget *parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("booneDetailsPane"));
    setAutoFillBackground(false);

    const int margin = style()->pixelMetric(QStyle::PM_LayoutLeftMargin);
    const int spacing = style()->pixelMetric(QStyle::PM_LayoutHorizontalSpacing);

    auto row = new QHBoxLayout(this);
    // a little more air than a dialog: the pane is a calm strip, not a form
    row->setContentsMargins(IconLeft, TextTop, 2 * margin, margin);
    row->setSpacing(TextGap);

    m_icon = new QLabel(this);
    m_icon->setFixedSize(IconSize, IconSize);
    m_icon->setAlignment(Qt::AlignCenter);
    row->addWidget(m_icon, 0, Qt::AlignTop);

    // name + summary on the left, then the key/value grid; all on one text baseline grid
    auto text = new QGridLayout();
    text->setContentsMargins(0, 0, 0, 0);
    text->setHorizontalSpacing(2 * spacing);
    text->setVerticalSpacing(1);

    auto nameColumn = new QVBoxLayout();
    nameColumn->setContentsMargins(0, 0, 0, 0);
    nameColumn->setSpacing(1);
    m_name = new QLabel(this);
    m_name->setTextFormat(Qt::PlainText);
    m_name->setMinimumWidth(10 * fontMetrics().averageCharWidth());
    m_summary = new QLabel(this);
    m_summary->setTextFormat(Qt::PlainText);
    QPalette grey = m_summary->palette();
    grey.setColor(QPalette::WindowText, KeyColor);
    m_summary->setPalette(grey);
    nameColumn->addWidget(m_name);
    nameColumn->addWidget(m_summary);

    m_pairs = new QGridLayout();
    m_pairs->setContentsMargins(0, 0, 0, 0);
    m_pairs->setHorizontalSpacing(spacing);
    m_pairs->setVerticalSpacing(1);

    text->addLayout(nameColumn, 0, 0, Qt::AlignTop);
    text->addLayout(m_pairs, 0, 1, Qt::AlignTop);
    text->setColumnStretch(2, 1);
    // the text block sits level with the icon: centred on it as a whole, lines kept together
    row->addLayout(text, 1);
    row->setAlignment(text, Qt::AlignTop); // Vista starts the text at the top, beside the icon

    // three lines of text, like Vista's pane; never shorter than the icon
    const int lines = 3 * fontMetrics().height() + 2;
    setFixedHeight(qMax(PaneHeight, lines + TextTop + margin + 1));
}

void DetailsPane::setView(DolphinView *view)
{
    if (m_view == view) {
        return;
    }
    if (m_view) {
        disconnect(m_view, nullptr, this, nullptr);
    }
    m_view = view;
    m_statusText.clear();
    if (m_view) {
        connect(m_view, &DolphinView::selectionChanged, this, &DetailsPane::refresh);
        connect(m_view, &DolphinView::urlChanged, this, &DetailsPane::refresh);
        connect(m_view, &DolphinView::directoryLoadingCompleted, this, [this]() {
            m_view->requestStatusBarText();
            refresh();
        });
        connect(m_view, &DolphinView::statusBarTextChanged, this, [this](const QString &text) {
            m_statusText = text;
            refresh();
        });
        m_view->requestStatusBarText();
    }
    refresh();
}

void DetailsPane::refresh()
{
    if (!m_view) {
        return;
    }
    const KFileItemList selection = m_view->selectedItems();

    if (selection.isEmpty()) {
        // the folder itself: its icon, its name, how much is in it
        const KFileItem root = m_view->rootItem();
        const QUrl url = m_view->url();
        QString name = url.adjusted(QUrl::StripTrailingSlash).fileName();
        if (url.scheme().endsWith(QLatin1String("search"))) { // filenamesearch:, baloosearch:
            name = i18nc("@info Vista details pane", "Search Results");
        }
        if (name.isEmpty()) {
            name = url.toDisplayString(QUrl::PreferLocalFile);
        }
        m_shownItem = KFileItem();
        showIcon(root.isNull() ? KIO::iconNameForUrl(url) : root.iconName());
        m_name->setText(name);
        m_summary->setText(m_statusText);
        setPairs({});
        return;
    }

    if (selection.count() > 1) {
        qint64 size = 0;
        int files = 0;
        for (const KFileItem &item : selection) {
            if (!item.isDir()) {
                size += item.size();
                ++files;
            }
        }
        m_shownItem = KFileItem();
        showIcon(QStringLiteral("edit-select-all"));
        m_name->setText(i18ncp("@info Vista details pane", "%1 item selected", "%1 items selected", selection.count()));
        m_summary->clear();
        QList<QPair<QString, QString>> pairs;
        if (files > 0) {
            pairs.append({i18nc("@label Vista details pane", "Size:"), KIO::convertSize(size)});
        }
        setPairs(pairs);
        return;
    }

    const KFileItem item = selection.first();
    m_shownItem = item;
    showIcon(item.iconName());
    m_name->setText(item.text());
    m_summary->setText(item.mimeComment());
    QList<QPair<QString, QString>> pairs;
    const QDateTime modified = item.time(KFileItem::ModificationTime);
    if (modified.isValid()) {
        pairs.append({i18nc("@label Vista details pane", "Date modified:"), QLocale().toString(modified, QLocale::ShortFormat)});
    }
    if (!item.isDir()) {
        pairs.append({i18nc("@label Vista details pane", "Size:"), KIO::convertSize(item.size())});
    }
    const QDateTime created = item.time(KFileItem::CreationTime);
    if (created.isValid()) {
        pairs.append({i18nc("@label Vista details pane", "Date created:"), QLocale().toString(created, QLocale::ShortFormat)});
    }
    setPairs(pairs);
    if (!item.isDir()) {
        requestPreview(item);
    }
}

void DetailsPane::setPairs(const QList<QPair<QString, QString>> &pairs)
{
    while (QLayoutItem *child = m_pairs->takeAt(0)) {
        delete child->widget();
        delete child;
    }
    // Vista fills the pairs top to bottom, three to a column: grey key right-aligned, value left
    int row = 0;
    int column = 0;
    for (const auto &[key, value] : pairs) {
        auto keyLabel = new QLabel(key, this);
        QPalette grey = keyLabel->palette();
        grey.setColor(QPalette::WindowText, KeyColor);
        keyLabel->setPalette(grey);
        keyLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
        auto valueLabel = new QLabel(value, this);
        valueLabel->setTextFormat(Qt::PlainText);
        valueLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        m_pairs->addWidget(keyLabel, row, column);
        m_pairs->addWidget(valueLabel, row, column + 1);
        if (++row == 3) {
            row = 0;
            column += 2;
        }
    }
}

void DetailsPane::showIcon(const QString &iconName)
{
    const QIcon icon = QIcon::fromTheme(iconName, QIcon::fromTheme(QStringLiteral("unknown")));
    m_icon->setPixmap(icon.pixmap(QSize(IconSize, IconSize), devicePixelRatioF()));
}

void DetailsPane::requestPreview(const KFileItem &item)
{
    if (m_previewJob) {
        m_previewJob->kill();
    }
    const QStringList plugins = KIO::PreviewJob::defaultPlugins();
    auto job = KIO::filePreview(KFileItemList{item}, QSize(IconSize, IconSize), &plugins);
    job->setDevicePixelRatio(devicePixelRatioF());
    job->setIgnoreMaximumSize(false);
    connect(job, &KIO::PreviewJob::gotPreview, this, [this](const KFileItem &shown, const QPixmap &preview) {
        if (shown.url() == m_shownItem.url()) {
            m_icon->setPixmap(preview);
        }
    });
    m_previewJob = job;
}

void DetailsPane::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    const QRectF r = rect();
    QLinearGradient fill(r.topLeft(), r.topRight());
    fill.setColorAt(0.00, QColor(0xf6, 0xfa, 0xf9));
    fill.setColorAt(0.37, QColor(0xed, 0xfa, 0xff));
    fill.setColorAt(0.55, QColor(0xe8, 0xf7, 0xfc));
    fill.setColorAt(0.72, QColor(0xd5, 0xeb, 0xf8));
    fill.setColorAt(0.90, QColor(0xc7, 0xdd, 0xf2));
    fill.setColorAt(1.00, QColor(0xc0, 0xdb, 0xf0));
    painter.fillRect(r, fill);
    QLinearGradient shade(r.topLeft(), r.topLeft() + QPointF(0, 8));
    shade.setColorAt(0, QColor(0x9f, 0xbc, 0xd2, 120));
    shade.setColorAt(1, QColor(0x9f, 0xbc, 0xd2, 0));
    painter.fillRect(QRectF(r.left(), r.top(), r.width(), 8), shade);
    painter.setPen(TopLine);
    painter.drawLine(r.topLeft(), r.topRight());

    // Vista's light swoosh in the right-hand corner
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal w = r.width();
    const qreal h = r.height();
    for (int i = 0; i < 4; ++i) {
        QPainterPath swoosh;
        const qreal x0 = w - 0.22 * w + i * 9;
        swoosh.moveTo(x0, h + 2);
        swoosh.cubicTo(x0 + 0.06 * w, h * 0.55, w - 0.07 * w, h * 0.25, w + 4, 2 + i * 4);
        QColor c(255, 255, 255, 190 - i * 35);
        painter.setPen(QPen(c, 1.2));
        painter.drawPath(swoosh);
    }
}

#include "moc_detailspane.cpp"
