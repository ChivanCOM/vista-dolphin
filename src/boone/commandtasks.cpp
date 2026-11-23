/*
 * BOONE: Windows Vista's command bar tasks.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "commandtasks.h"

#include "capacitybar.h"
#include "trash/dolphintrash.h"
#include "views/dolphinview.h"

#include <KApplicationTrader>
#include <KIO/ApplicationLauncherJob>
#include <KIO/CommandLauncherJob>
#include <KIO/JobUiDelegateFactory>
#include <KIO/RestoreJob>
#include <KLocalizedString>
#include <KToolBar>

#include <QAction>
#include <QMainWindow>
#include <QMenu>
#include <QMimeDatabase>
#include <QStandardPaths>
#include <QTimer>
#include <QToolButton>

using namespace Boone;

namespace
{
enum class Kind { Folder, Image, Audio, Video, Archive, Other };

Kind kindOf(const KFileItem &item)
{
    if (item.isDir()) {
        return Kind::Folder;
    }
    const QString mime = item.mimetype();
    if (mime.startsWith(QLatin1String("image/"))) {
        return Kind::Image;
    }
    if (mime.startsWith(QLatin1String("audio/"))) {
        return Kind::Audio;
    }
    if (mime.startsWith(QLatin1String("video/"))) {
        return Kind::Video;
    }
    static const QStringList archives{
        QStringLiteral("application/zip"),         QStringLiteral("application/x-tar"),        QStringLiteral("application/x-compressed-tar"),
        QStringLiteral("application/x-bzip-compressed-tar"), QStringLiteral("application/x-xz-compressed-tar"), QStringLiteral("application/x-zstd-compressed-tar"),
        QStringLiteral("application/x-7z-compressed"), QStringLiteral("application/vnd.rar"), QStringLiteral("application/x-rar"),
        QStringLiteral("application/gzip"),        QStringLiteral("application/x-bzip2"),      QStringLiteral("application/x-xz"),
    };
    const QMimeType type = QMimeDatabase().mimeTypeForName(mime);
    for (const QString &archive : archives) {
        if (type.inherits(archive)) {
            return Kind::Archive;
        }
    }
    return Kind::Other;
}

bool have(const QString &program)
{
    return !QStandardPaths::findExecutable(program).isEmpty();
}

QList<QUrl> urlsOf(const KFileItemList &items)
{
    QList<QUrl> urls;
    for (const KFileItem &item : items) {
        urls.append(item.url());
    }
    return urls;
}

constexpr int MaxTasks = 5; // Vista shows a handful; more would crowd the bar
}

CommandTasks::CommandTasks(QMainWindow *window, Navigation navigation)
    : QObject(window)
    , m_window(window)
    , m_navigation(std::move(navigation))
{
}

void CommandTasks::setView(DolphinView *view)
{
    if (m_view == view) {
        return;
    }
    if (m_view) {
        disconnect(m_view, nullptr, this, nullptr);
    }
    m_view = view;
    if (m_view) {
        connect(m_view, &DolphinView::selectionChanged, this, &CommandTasks::scheduleRefresh);
        connect(m_view, &DolphinView::urlChanged, this, &CommandTasks::scheduleRefresh);
        connect(m_view, &DolphinView::directoryLoadingCompleted, this, &CommandTasks::scheduleRefresh);
    }
    scheduleRefresh();
}

void CommandTasks::scheduleRefresh()
{
    // a rubber-band selection changes the selection many times; rebuild once it settles
    if (!m_refreshPending) {
        m_refreshPending = true;
        QTimer::singleShot(60, this, &CommandTasks::refresh);
    }
}

KToolBar *CommandTasks::commandBar() const
{
    return m_window->findChild<KToolBar *>(QStringLiteral("commandBar"));
}

QAction *CommandTasks::addTask(const QString &text, const QString &icon, std::function<void()> run)
{
    KToolBar *bar = commandBar();
    if (!bar || m_tasks.count() >= MaxTasks) {
        return nullptr;
    }
    auto action = new QAction(QIcon::fromTheme(icon), text, this);
    connect(action, &QAction::triggered, this, [run = std::move(run)]() {
        run();
    });
    bar->addAction(action);
    m_tasks.append(action);
    return action;
}

void CommandTasks::runCommand(const QString &program, const QStringList &args)
{
    auto job = new KIO::CommandLauncherJob(program, args);
    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_window));
    job->start();
}

void CommandTasks::addOpenTask(const KFileItemList &items, const QString &text)
{
    // Vista's first task opens the selection in its program; the arrow lists the others
    const QString mime = items.first().mimetype();
    const KService::Ptr preferred = KApplicationTrader::preferredService(mime);
    const QList<QUrl> urls = urlsOf(items);
    QWidget *window = m_window;
    auto launch = [urls, window](const KService::Ptr &service) {
        auto job = service ? new KIO::ApplicationLauncherJob(service) : new KIO::ApplicationLauncherJob();
        job->setUrls(urls);
        job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
        job->start();
    };
    QAction *open = addTask(text, preferred ? preferred->icon() : QStringLiteral("document-open"), [launch, preferred]() {
        launch(preferred);
    });
    if (!open) {
        return;
    }
    auto menu = new QMenu(m_window);
    connect(open, &QObject::destroyed, menu, &QObject::deleteLater);
    const KService::List services = KApplicationTrader::queryByMimeType(mime);
    QStringList listed; // one entry per program name (a program can be installed twice)
    for (const KService::Ptr &service : services) {
        if (listed.contains(service->name())) {
            continue;
        }
        listed.append(service->name());
        QAction *entry = menu->addAction(QIcon::fromTheme(service->icon()), service->name());
        if (preferred && service->storageId() == preferred->storageId()) {
            QFont bold = entry->font();
            bold.setBold(true); // Vista marks the default program in bold
            entry->setFont(bold);
        }
        connect(entry, &QAction::triggered, this, [launch, service]() {
            launch(service);
        });
    }
    if (!services.isEmpty()) {
        menu->addSeparator();
    }
    connect(menu->addAction(i18nc("@action:inmenu Vista command bar", "Choose Program…")), &QAction::triggered, this, [launch]() {
        launch(KService::Ptr());
    });
    open->setMenu(menu);
    if (auto button = qobject_cast<QToolButton *>(commandBar()->widgetForAction(open))) {
        button->setPopupMode(QToolButton::MenuButtonPopup);
    }
}

void CommandTasks::refresh()
{
    m_refreshPending = false;
    for (const QPointer<QAction> &task : std::as_const(m_tasks)) {
        delete task.data();
    }
    m_tasks.clear();
    if (!m_view || !commandBar()) {
        return;
    }

    const QUrl url = m_view->url();
    const KFileItemList selection = m_view->selectedItems();

    // --- the Recycle Bin --------------------------------------------------------------
    if (url.scheme() == QLatin1String("trash")) {
        if (!selection.isEmpty()) {
            const QList<QUrl> urls = urlsOf(selection);
            addTask(i18nc("@action Vista command bar", "Restore"), QStringLiteral("edit-undo"), [this, urls]() {
                auto job = KIO::restoreFromTrash(urls);
                job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, m_window));
            });
        }
        if (!Trash::isEmpty()) {
            addTask(i18nc("@action Vista command bar", "Empty Recycle Bin"), QStringLiteral("trash-empty"), [this]() {
                Trash::empty(m_window);
            });
        }
        return;
    }

    // --- nothing selected: tasks for the folder itself ----------------------------------
    if (selection.isEmpty()) {
        if (url.isLocalFile() && url.toLocalFile() == QLatin1String("/")) {
            // the system drive stands in for Vista's Computer folder
            if (have(QStringLiteral("denairu-control-panel"))) {
                addTask(i18nc("@action Vista command bar", "System properties"), QStringLiteral("preferences-system"), [this]() {
                    runCommand(QStringLiteral("denairu-control-panel"), {QStringLiteral("sysinfo")});
                });
                addTask(i18nc("@action Vista command bar", "Uninstall or change a program"), QStringLiteral("preferences-desktop-default-applications"), [this]() {
                    runCommand(QStringLiteral("denairu-control-panel"), {QStringLiteral("--applet"), QStringLiteral("programs")});
                });
                addTask(i18nc("@action Vista command bar", "Open Control Panel"), QStringLiteral("preferences-system"), [this]() {
                    runCommand(QStringLiteral("denairu-control-panel"), {});
                });
            }
            return;
        }
        int images = 0;
        KFileItemList music;
        const KFileItemList items = m_view->items();
        for (const KFileItem &item : items) {
            const Kind kind = kindOf(item);
            images += kind == Kind::Image;
            if (kind == Kind::Audio) {
                music.append(item);
            }
        }
        if (images > 0 && url.isLocalFile() && have(QStringLiteral("gwenview"))) {
            const QString folder = url.toLocalFile();
            addTask(i18nc("@action Vista command bar", "Slide Show"), QStringLiteral("view-presentation"), [this, folder]() {
                runCommand(QStringLiteral("gwenview"), {QStringLiteral("--slideshow"), folder});
            });
        }
        if (!music.isEmpty()) {
            const KService::Ptr player = KApplicationTrader::preferredService(music.first().mimetype());
            if (player) {
                const QList<QUrl> urls = urlsOf(music);
                QWidget *window = m_window;
                addTask(i18nc("@action Vista command bar", "Play All"), QStringLiteral("media-playback-start"), [player, urls, window]() {
                    auto job = new KIO::ApplicationLauncherJob(player);
                    job->setUrls(urls);
                    job->setUiDelegate(KIO::createDefaultJobUiDelegate(KJobUiDelegate::AutoHandlingEnabled, window));
                    job->start();
                });
            }
        }
        return;
    }

    // --- a selection: what Vista offers for that kind of item ---------------------------
    Kind common = kindOf(selection.first());
    bool allLocalFiles = true;
    for (const KFileItem &item : selection) {
        if (kindOf(item) != common) {
            common = Kind::Other;
        }
        if (item.isDir() || item.localPath().isEmpty()) {
            allLocalFiles = false;
        }
    }
    const bool mixedFolders = common == Kind::Other && std::any_of(selection.begin(), selection.end(), [](const KFileItem &i) {
                                  return i.isDir();
                              });

    if (common == Kind::Folder) {
        if (selection.count() == 1) {
            const QUrl folder = selection.first().url();
            addTask(i18nc("@action Vista command bar: open a folder", "Explore"), QStringLiteral("document-open-folder"), [this, folder]() {
                m_navigation.open(folder);
            });
        }
        const QList<QUrl> folders = urlsOf(selection);
        addTask(i18nc("@action Vista command bar", "Open in New Tab"), QStringLiteral("tab-new"), [this, folders]() {
            for (const QUrl &folder : folders) {
                m_navigation.openInNewTab(folder);
            }
        });
    } else if (!mixedFolders) {
        const bool sameMime = std::all_of(selection.begin(), selection.end(), [&](const KFileItem &i) {
            return i.mimetype() == selection.first().mimetype();
        });
        if (sameMime || common != Kind::Other) {
            QString text = i18nc("@action Vista command bar", "Open");
            if (common == Kind::Image) {
                text = i18nc("@action Vista command bar", "Preview");
            } else if (common == Kind::Audio || common == Kind::Video) {
                text = i18nc("@action Vista command bar", "Play");
            }
            addOpenTask(selection, text);
        }
        if (common == Kind::Image && selection.first().isLocalFile() && have(QStringLiteral("gwenview"))) {
            const QString first = selection.first().localPath();
            addTask(i18nc("@action Vista command bar", "Slide Show"), QStringLiteral("view-presentation"), [this, first]() {
                runCommand(QStringLiteral("gwenview"), {QStringLiteral("--slideshow"), first});
            });
        }
        if (common == Kind::Archive && allLocalFiles && have(QStringLiteral("ark"))) {
            QStringList files;
            for (const KFileItem &item : selection) {
                files.append(item.localPath());
            }
            addTask(i18nc("@action Vista command bar", "Extract All Files"), QStringLiteral("archive-extract"), [this, files]() {
                runCommand(QStringLiteral("ark"), QStringList{QStringLiteral("--batch"), QStringLiteral("--autosubfolder"), QStringLiteral("--autodestination")} + files);
            });
        }
    }

    if (allLocalFiles && have(QStringLiteral("xdg-email")) && KApplicationTrader::preferredService(QStringLiteral("x-scheme-handler/mailto"))) {
        QStringList args;
        for (const KFileItem &item : selection) {
            args << QStringLiteral("--attach") << item.localPath();
        }
        addTask(i18nc("@action Vista command bar", "E-mail"), QStringLiteral("mail-send"), [this, args]() {
            runCommand(QStringLiteral("xdg-email"), args);
        });
    }

    if (common == Kind::Image && selection.count() == 1 && allLocalFiles && have(QStringLiteral("plasma-apply-wallpaperimage"))) {
        const QString image = selection.first().localPath();
        addTask(i18nc("@action Vista command bar", "Set as Desktop Background"), QStringLiteral("preferences-desktop-wallpaper"), [this, image]() {
            runCommand(QStringLiteral("plasma-apply-wallpaperimage"), {image});
        });
    }
}

#include "moc_commandtasks.cpp"
