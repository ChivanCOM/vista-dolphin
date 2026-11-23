/*
 * BOONE: Windows Vista's search box for Dolphin's address-bar row.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "searchbox.h"

#include <KLocalizedString>

#include <QAction>
#include <QEvent>
#include <QIcon>
#include <QKeyEvent>
#include <QLineEdit>

using namespace Boone;

namespace
{
/** Escape in the field: clear it, which ends the search. */
class EscapeClears : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            auto field = static_cast<QLineEdit *>(watched);
            if (!field->text().isEmpty()) {
                field->clear(); // the action sees the field empty and ends the search
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};
}

SearchBoxAction::SearchBoxAction(QObject *parent)
    : QWidgetAction(parent)
{
    setIcon(QIcon::fromTheme(QStringLiteral("edit-find")));
    QAction::setText(i18nc("@action Vista search box", "Search Box"));
}

QWidget *SearchBoxAction::createWidget(QWidget *parent)
{
    auto field = new QLineEdit(parent);
    field->setObjectName(QStringLiteral("booneSearchBox"));
    field->setPlaceholderText(i18nc("@info:placeholder Vista search box", "Search"));
    field->setClearButtonEnabled(true);
    field->setToolTip(i18nc("@info:tooltip", "Search the folder you are in"));
    // Vista's box is about a quarter of the address bar: room for a few words
    field->setFixedWidth(22 * field->fontMetrics().averageCharWidth());
    // the address bar's height: both fill the toolbar row
    field->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
    QAction *go = field->addAction(QIcon::fromTheme(QStringLiteral("edit-find")), QLineEdit::TrailingPosition);
    go->setToolTip(i18nc("@info:tooltip", "Search"));
    connect(go, &QAction::triggered, this, &SearchBoxAction::searchCommitted);
    // Typing searches. Emptying the field in any way (backspace, its clear button, Escape)
    // ends the search; setText() below changes the field without signals, so only the
    // person at the keyboard gets here.
    connect(field, &QLineEdit::textEdited, this, [this](const QString &text) {
        if (!text.isEmpty()) {
            Q_EMIT searchTermEdited(text);
        }
    });
    connect(field, &QLineEdit::textChanged, this, [this](const QString &text) {
        if (text.isEmpty()) {
            Q_EMIT searchTermEdited(QString());
        }
    });
    connect(field, &QLineEdit::returnPressed, this, &SearchBoxAction::searchCommitted);
    // Vista's grey italic "Search"; what you type is upright
    auto italicWhenEmpty = [field](const QString &text) {
        QFont font = field->font();
        font.setItalic(text.isEmpty());
        field->setFont(font);
    };
    connect(field, &QLineEdit::textChanged, field, italicWhenEmpty);
    italicWhenEmpty(QString());
    field->installEventFilter(new EscapeClears(field));
    m_field = field;
    return field;
}

QLineEdit *SearchBoxAction::field() const
{
    return m_field;
}

void SearchBoxAction::setText(const QString &text)
{
    if (m_field && m_field->text() != text) {
        const QSignalBlocker blocker(m_field);
        m_field->setText(text);
    }
}

#include "moc_searchbox.cpp"
