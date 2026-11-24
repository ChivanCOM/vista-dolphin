/*
 * Vista: Windows Vista's search box for Dolphin's address-bar row.
 *
 * SPDX-FileCopyrightText: 2026 Vista Dolphin contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef VISTA_SEARCHBOX_H
#define VISTA_SEARCHBOX_H

#include <QPointer>
#include <QWidgetAction>

class QLineEdit;

namespace Vista
{
/**
 * A "Search" field for the toolbar, like the one beside Vista Explorer's address bar.
 * Typing searches as you go; Enter or the magnifier searches right away; clearing it
 * (or Escape) goes back to the folder. Dolphin's search bar still offers its filters.
 */
class SearchBoxAction : public QWidgetAction
{
    Q_OBJECT

public:
    explicit SearchBoxAction(QObject *parent);

    /** The field on the toolbar, if it is shown there. */
    QLineEdit *field() const;
    /** Shows @p text without searching (e.g. when another tab becomes active). */
    void setText(const QString &text);

Q_SIGNALS:
    void searchTermEdited(const QString &text);
    void searchCommitted();

protected:
    QWidget *createWidget(QWidget *parent) override;

private:
    QPointer<QLineEdit> m_field;
};
}

#endif
