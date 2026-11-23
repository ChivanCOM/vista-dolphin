/*
    This file is part of the KDE project
    SPDX-FileCopyrightText: 2020 Felix Ernst <felixernst@kde.org>

    SPDX-License-Identifier: LGPL-2.1-only OR LGPL-3.0-only OR LicenseRef-KDE-Accepted-LGPL
*/

#include "dolphinurlnavigator.h"

#include "dolphin_generalsettings.h"
#include "dolphinplacesmodelsingleton.h"
#include "dolphinurlnavigatorscontroller.h"
#include "global.h"

#include <KLocalizedString>
#include <KUrlComboBox>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QFontInfo>
#include <QKeyEvent>
#include <QLabel>
#include <QLayout>
#include <QLineEdit>
#include <QToolButton>

namespace
{
class BooneRegularCrumbs : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::Polish && watched->inherits("KDEPrivate::KUrlNavigatorButton")) {
            auto *button = static_cast<QWidget *>(watched);
            QFont regular = button->font();
            regular.setBold(false);
            regular.setStyleName(QFontInfo(regular).styleName());
            button->setFont(regular);
        } else if (event->type() == QEvent::Polish && watched->inherits("QToolButton") && watched->parent()
                   && watched->parent()->objectName() == QLatin1String("mainToolBar")) {
            // Vista's refresh button is a segment at the end of the address bar: name it for the
            // style sheet, and let it fill the row like the address bar does
            auto *button = static_cast<QToolButton *>(watched);
            if (button->defaultAction() && button->defaultAction()->objectName() == QLatin1String("view_redisplay")) {
                button->setObjectName(QStringLiteral("booneRefresh"));
                button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
                // joined to the address bar: no gaps between the row's items (the style
                // sheet spaces the search box)
                if (QLayout *row = button->parentWidget()->layout()) {
                    row->setSpacing(0);
                }
            }
        }
        return false;
    }
};
}

DolphinUrlNavigator::DolphinUrlNavigator(QWidget *parent)
    : DolphinUrlNavigator(QUrl(), parent)
{
}

DolphinUrlNavigator::DolphinUrlNavigator(const QUrl &url, QWidget *parent)
    : KUrlNavigator(DolphinPlacesModelSingleton::instance().placesModel(), url, parent)
{
    const GeneralSettings *settings = GeneralSettings::self();
    setUrlEditable(settings->editableUrl());
    setShowFullPath(settings->showFullPath());
    setHomeUrl(Dolphin::homeUrl());
    setPlacesSelectorVisible(DolphinUrlNavigatorsController::placesSelectorVisible());
    editor()->setCompletionMode(KCompletion::CompletionMode(settings->urlCompletionMode()));
    setWhatsThis(xi18nc("@info:whatsthis location bar",
                        "<para>This describes the location of the files and folders "
                        "displayed below.</para><para>The name of the currently viewed "
                        "folder can be read at the very right. To the left of it is the "
                        "name of the folder that contains it. The whole line is called "
                        "the <emphasis>path</emphasis> to the current location because "
                        "following these folders from left to right leads here.</para>"
                        "<para>This interactive path "
                        "is more powerful than one would expect. To learn more "
                        "about the basic and advanced features of the location bar "
                        "<link url='help:/dolphin/location-bar.html'>click here</link>. "
                        "This will open the dedicated page in the Handbook.</para>"));

    // BOONE: Vista's breadcrumb is all one weight; KIO draws the current folder in bold.
    // Each crumb button's font is pinned to its regular face, so "bold" picks that same face
    // (synthetic emboldening is switched off in main.cpp). KIO makes crumbs as folders are
    // entered and they don't inherit a style name, so it is done as each one is polished.
    if (qEnvironmentVariableIsSet("DENAIRU_SESSION")) {
        static QObject *regularCrumbs = nullptr;
        if (!regularCrumbs) {
            regularCrumbs = new BooneRegularCrumbs(qApp);
            qApp->installEventFilter(regularCrumbs);
        }
    }

    DolphinUrlNavigatorsController::registerDolphinUrlNavigator(this);

    connect(this, &KUrlNavigator::returnPressed, this, &DolphinUrlNavigator::slotReturnPressed);

    auto readOnlyBadge = new QLabel();
    readOnlyBadge->setPixmap(QIcon::fromTheme(QStringLiteral("emblem-readonly")).pixmap(12, 12));
    readOnlyBadge->setToolTip(i18nc("@info:tooltip of a 'locked' symbol in url navigator", "This folder is not writable for you."));
    readOnlyBadge->hide();
    setBadgeWidget(readOnlyBadge);
}

DolphinUrlNavigator::~DolphinUrlNavigator()
{
    DolphinUrlNavigatorsController::unregisterDolphinUrlNavigator(this);
}

QSize DolphinUrlNavigator::sizeHint() const
{
    if (isUrlEditable()) {
        return editor()->lineEdit()->sizeHint();
    }
    int widthHint = 0;
    for (int i = 0; i < layout()->count(); ++i) {
        QWidget *widget = layout()->itemAt(i)->widget();
        const QAbstractButton *button = qobject_cast<QAbstractButton *>(widget);
        if (button && button->icon().isNull()) {
            widthHint += widget->minimumSizeHint().width();
        }
    }
    if (readOnlyBadgeVisible()) {
        widthHint += badgeWidget()->sizeHint().width();
    }
    return QSize(widthHint, KUrlNavigator::sizeHint().height());
}

std::unique_ptr<DolphinUrlNavigator::VisualState> DolphinUrlNavigator::visualState() const
{
    std::unique_ptr<VisualState> visualState{new VisualState};
    visualState->isUrlEditable = (isUrlEditable());
    const QLineEdit *lineEdit = editor()->lineEdit();
    visualState->hasFocus = lineEdit->hasFocus();
    visualState->text = lineEdit->text();
    visualState->cursorPosition = lineEdit->cursorPosition();
    visualState->selectionStart = lineEdit->selectionStart();
    visualState->selectionLength = lineEdit->selectionLength();
    return visualState;
}

void DolphinUrlNavigator::setVisualState(const VisualState &visualState)
{
    setUrlEditable(visualState.isUrlEditable);
    if (!visualState.isUrlEditable) {
        return;
    }
    editor()->lineEdit()->setText(visualState.text);
    if (visualState.hasFocus) {
        editor()->lineEdit()->setFocus();
        editor()->lineEdit()->setCursorPosition(visualState.cursorPosition);
        if (visualState.selectionStart != -1) {
            editor()->lineEdit()->setSelection(visualState.selectionStart, visualState.selectionLength);
        }
    }
}

void DolphinUrlNavigator::clearText() const
{
    editor()->lineEdit()->clear();
}

void DolphinUrlNavigator::setPlaceholderText(const QString &text)
{
    editor()->lineEdit()->setPlaceholderText(text);
}

void DolphinUrlNavigator::setReadOnlyBadgeVisible(bool visible)
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        readOnlyBadge->setVisible(visible);
    }
}

bool DolphinUrlNavigator::readOnlyBadgeVisible() const
{
    QWidget *readOnlyBadge = badgeWidget();
    if (readOnlyBadge) {
        return readOnlyBadge->isVisible();
    }
    return false;
}

void DolphinUrlNavigator::slotReturnPressed()
{
    if (!GeneralSettings::editableUrl()) {
        setUrlEditable(false);
    }
}

void DolphinUrlNavigator::keyPressEvent(QKeyEvent *keyEvent)
{
    if (keyEvent->key() == Qt::Key_Escape && !isUrlEditable()) {
        Q_EMIT requestToLoseFocus();
        return;
    }
    KUrlNavigator::keyPressEvent(keyEvent);
}

#include "moc_dolphinurlnavigator.cpp"
