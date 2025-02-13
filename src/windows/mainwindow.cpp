#include "windows/mainwindow.h"
#include "windows/aboutdialog.h"
#include "windows/devicemanager.h"
#include "windows/dialogs.h"
#include "windows/keymanager.h"
#include "windows/optionsdialog.h"
#include "windows/waitdialog.h"
#include "widgets/resourcetree.h"
#include "widgets/filesystemtree.h"
#include "widgets/iconlist.h"
#include "base/application.h"
#include <QMenuBar>
#include <QDockWidget>
#include <QMimeData>
#include <QMimeDatabase>
#include <QTimer>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    setAcceptDrops(true);

    initWidgets();
    initMenus();
    loadSettings();
    initLanguages();

    connect(&app->projects, &ProjectItemsModel::changed, [=](Project *project) {
        if (project == projectsWidget->getCurrentProject()) {
            updateWindowForProject(project);
        }
    });

    connect(app->recent, &Recent::changed, this, &MainWindow::updateRecentMenu);
    updateRecentMenu();

    QEvent languageChangeEvent(QEvent::LanguageChange);
    app->sendEvent(this, &languageChangeEvent);

    if (app->settings->getAutoUpdates()) {
        // Delay to prevent uninitialized window render
        auto timer = new QTimer(this);
        connect(timer, &QTimer::timeout, [=]() {
            Updater::check(false, this);
            timer->deleteLater();
        });
        timer->setSingleShot(true);
        timer->start(1000);
    }

    qDebug();
}

void MainWindow::setInitialSize()
{
    resize(app->scale(1100, 600));
}

void MainWindow::initWidgets()
{
    qDebug() << "Initializing widgets...";
    setInitialSize();

    projectsWidget = new ProjectsWidget(this);
    projectsWidget->setModel(&app->projects);
    setCentralWidget(projectsWidget);
    connect(projectsWidget, &ProjectsWidget::currentProjectChanged, [=](Project *project) {
        setCurrentProject(project);
        projectList->setCurrentProject(project);
    });
    connect(projectsWidget, &ProjectsWidget::currentTabChanged, [=]() {
        menuEditor->clear();
        menuEditor->addActions(projectsWidget->getCurrentTabActions());
    });

    QWidget *dockProjectsWidget = new QWidget(this);
    QVBoxLayout *projectsLayout = new QVBoxLayout(dockProjectsWidget);
    projectList = new ProjectList(this);
    projectList->setModel(&app->projects);
    connect(projectList, &ProjectList::currentProjectChanged, [=](Project *project) {
        setCurrentProject(project);
        projectsWidget->setCurrentProject(project);
    });

    logView = new LogView(this);
    projectsLayout->addWidget(projectList);
    projectsLayout->addWidget(logView);
    projectsLayout->setMargin(0);
    projectsLayout->setSpacing(1);

    QWidget *dockResourceWidget = new QWidget(this);
    QVBoxLayout *resourceLayout = new QVBoxLayout(dockResourceWidget);
    resourceTree = new ResourceAbstractView(new ResourceTree, this);
    resourceLayout->addWidget(resourceTree);
    resourceLayout->setMargin(0);
    connect(resourceTree, &ResourceAbstractView::editRequested, this, &MainWindow::openResource);

    QWidget *dockFilesystemWidget = new QWidget(this);
    QVBoxLayout *filesystemLayout = new QVBoxLayout(dockFilesystemWidget);
    filesystemTree = new ResourceAbstractView(new FilesystemTree, this);
    filesystemLayout->addWidget(filesystemTree);
    filesystemLayout->setMargin(0);
    connect(filesystemTree, &ResourceAbstractView::editRequested, this, &MainWindow::openResource);

    QWidget *dockIconsWidget = new QWidget(this);
    QVBoxLayout *iconsLayout = new QVBoxLayout(dockIconsWidget);
    iconList = new ResourceAbstractView(new IconList, this);
    iconsLayout->addWidget(iconList);
    iconsLayout->setMargin(0);
    iconsLayout->setSpacing(1);
    connect(iconList, &ResourceAbstractView::editRequested, this, &MainWindow::openResource);

    QWidget *dockManifestWidget = new QWidget(this);
    QVBoxLayout *manifestLayout = new QVBoxLayout(dockManifestWidget);
    manifestTable = new ManifestView(this);
    manifestLayout->addWidget(manifestTable);
    manifestLayout->setMargin(0);
    connect(manifestTable, &ManifestView::titleEditorRequested, projectsWidget, &ProjectsWidget::openTitlesTab);

    dockProjects = new QDockWidget(this);
    dockResources = new QDockWidget(this);
    dockFilesystem = new QDockWidget(this);
    dockManifest = new QDockWidget(this);
    dockIcons = new QDockWidget(this);
    dockProjects->setObjectName("DockProjects");
    dockResources->setObjectName("DockResources");
    dockFilesystem->setObjectName("DockFilesystem");
    dockManifest->setObjectName("DockManifest");
    dockIcons->setObjectName("DockIcons");
    dockProjects->setWidget(dockProjectsWidget);
    dockResources->setWidget(dockResourceWidget);
    dockFilesystem->setWidget(dockFilesystemWidget);
    dockManifest->setWidget(dockManifestWidget);
    dockIcons->setWidget(dockIconsWidget);
    addDockWidget(Qt::LeftDockWidgetArea, dockProjects);
    addDockWidget(Qt::LeftDockWidgetArea, dockResources);
    addDockWidget(Qt::LeftDockWidgetArea, dockFilesystem);
    addDockWidget(Qt::RightDockWidgetArea, dockManifest);
    addDockWidget(Qt::RightDockWidgetArea, dockIcons);
    tabifyDockWidget(dockResources, dockFilesystem);
    dockResources->raise();

    rubberBand = new QRubberBand(QRubberBand::Rectangle, this);

    defaultState = saveState();
}

void MainWindow::initMenus()
{
    qDebug() << "Initializing menus...";

    // File Menu:

    actionApkOpen = new QAction(app->icons.get("open.png"), QString(), this);
    actionApkOpen->setShortcut(QKeySequence::Open);
    actionApkSave = new QAction(app->icons.get("pack.png"), QString(), this);
    actionApkSave->setShortcut(QKeySequence("Ctrl+Alt+S"));
    actionApkInstall = new QAction(app->icons.get("install.png"), QString(), this);
    actionApkInstall->setShortcut(QKeySequence("Ctrl+I"));
    actionApkInstallExternal = new QAction(app->icons.get("install.png"), QString(), this);
    actionApkInstallExternal->setShortcut(QKeySequence("Ctrl+Shift+I"));
    actionApkExplore = new QAction(app->icons.get("explore.png"), QString(), this);
    actionApkExplore->setShortcut(QKeySequence("Ctrl+E"));
    actionApkClose = new QAction(app->icons.get("close-project.png"), QString(), this);
    actionApkClose->setShortcut(QKeySequence("Ctrl+W"));
    actionExit = new QAction(QIcon(app->icons.get("close.png")), QString(), this);
    actionExit->setShortcut(QKeySequence::Quit);
    actionExit->setMenuRole(QAction::QuitRole);

    // Recent Menu:

    menuRecent = new QMenu(this);
    menuRecent->setIcon(app->icons.get("recent.png"));
    actionRecentClear = new QAction(app->icons.get("close.png"), QString(), this);
    actionRecentNone = new QAction(this);
    actionRecentNone->setEnabled(false);

    // Tools Menu:

    actionKeyManager = new QAction(this);
    actionKeyManager->setIcon(app->icons.get("key.png"));
    actionKeyManager->setShortcut(QKeySequence("Ctrl+K"));
    actionDeviceManager = new QAction(this);
    actionDeviceManager->setIcon(app->icons.get("devices.png"));
    actionDeviceManager->setShortcut(QKeySequence("Ctrl+D"));
    actionProjectManager = new QAction(this);
    actionProjectManager->setIcon(app->icons.get("project.png"));
    actionProjectManager->setShortcut(QKeySequence("Ctrl+M"));
    actionTitleEditor = new QAction(this);
    actionTitleEditor->setIcon(app->icons.get("title.png"));
    actionTitleEditor->setShortcut(QKeySequence("Ctrl+T"));

    // Settings Menu:

    actionOptions = new QAction(this);
    actionOptions->setIcon(app->icons.get("settings.png"));
    actionOptions->setShortcut(QKeySequence("Ctrl+P"));
    actionOptions->setMenuRole(QAction::PreferencesRole);
    menuLanguage = new QMenu(this);
    actionSettingsReset = new QAction(this);
    actionSettingsReset->setIcon(app->icons.get("close.png"));

    // Help Menu:

    actionWebsite = new QAction(app->icons.get("website.png"), QString(), this);
    actionGithub = new QAction(app->icons.get("github.png"), QString(), this);
    actionDonate = new QAction(app->icons.get("donate.png"), QString(), this);
    actionUpdate = new QAction(app->icons.get("update.png"), QString(), this);
    actionAbout = new QAction(app->icons.get("application.png"), QString(), this);
    actionAbout->setMenuRole(QAction::AboutRole);
    actionAboutQt = new QAction(app->icons.get("qt.png"), QString(), this);
    actionAboutQt->setMenuRole(QAction::AboutQtRole);

    // Menu Bar:

    menuFile = menuBar()->addMenu(QString());
    menuFile->addAction(actionApkOpen);
    menuFile->addMenu(menuRecent);
    menuFile->addSeparator();
    menuFile->addAction(actionApkSave);
    menuFile->addSeparator();
    menuFile->addAction(actionApkInstall);
    menuFile->addAction(actionApkInstallExternal);
    menuFile->addSeparator();
    menuFile->addAction(actionApkExplore);
    menuFile->addSeparator();
    menuFile->addAction(actionApkClose);
    menuFile->addSeparator();
    menuFile->addAction(actionExit);
    menuEditor = menuBar()->addMenu(QString());
    menuEditor->addActions(projectsWidget->getCurrentTabActions());
    menuTools = menuBar()->addMenu(QString());
    menuTools->addAction(actionKeyManager);
    menuTools->addAction(actionDeviceManager);
    menuTools->addSeparator();
    menuTools->addAction(actionProjectManager);
    menuTools->addAction(actionTitleEditor);
    menuSettings = menuBar()->addMenu(QString());
    menuSettings->addAction(actionOptions);
    menuSettings->addSeparator();
    menuSettings->addMenu(menuLanguage);
    menuSettings->addSeparator();
    menuSettings->addAction(actionSettingsReset);
    menuWindow = menuBar()->addMenu(QString());
    menuHelp = menuBar()->addMenu(QString());
    menuHelp->addAction(actionWebsite);
    menuHelp->addAction(actionGithub);
    menuHelp->addAction(actionDonate);
    menuHelp->addSeparator();
    menuHelp->addAction(actionUpdate);
    menuHelp->addSeparator();
    menuHelp->addAction(actionAboutQt);
    menuHelp->addAction(actionAbout);

    // Tool Bar:

    toolbar = new Toolbar(this);
    toolbar->setObjectName("Toolbar");
    Toolbar::addToPool("open-project", actionApkOpen);
    Toolbar::addToPool("save-project", actionApkSave);
    Toolbar::addToPool("install-project", actionApkInstall);
    Toolbar::addToPool("open-contents", actionApkExplore);
    Toolbar::addToPool("close-project", actionApkClose);
    Toolbar::addToPool("project-manager", actionProjectManager);
    Toolbar::addToPool("title-editor", actionTitleEditor);
    Toolbar::addToPool("device-manager", actionDeviceManager);
    Toolbar::addToPool("key-manager", actionKeyManager);
    Toolbar::addToPool("settings", actionOptions);
    Toolbar::addToPool("donate", actionDonate);
    addToolBar(toolbar);

    setActionsEnabled(nullptr);

    // Signals / Slots

    connect(actionApkOpen, &QAction::triggered, [=]() { Dialogs::openApk(this); });
    connect(actionApkSave, &QAction::triggered, projectsWidget, &ProjectsWidget::saveCurrentProject);
    connect(actionApkInstall, &QAction::triggered, projectsWidget, &ProjectsWidget::installCurrentProject);
    connect(actionApkInstallExternal, &QAction::triggered, app, &Application::installExternalApk);
    connect(actionApkExplore, &QAction::triggered, projectsWidget, &ProjectsWidget::exploreCurrentProject);
    connect(actionApkClose, &QAction::triggered, projectsWidget, &ProjectsWidget::closeCurrentProject);
    connect(actionExit, &QAction::triggered, this, &MainWindow::close);
    connect(actionRecentClear, &QAction::triggered, app->recent, &Recent::clear);
    connect(actionTitleEditor, &QAction::triggered, projectsWidget, &ProjectsWidget::openTitlesTab);
    connect(actionProjectManager, &QAction::triggered, projectsWidget, &ProjectsWidget::openProjectTab);
    connect(actionKeyManager, &QAction::triggered, [=]() {
        KeyManager keyManager(this);
        keyManager.exec();
    });
    connect(actionDeviceManager, &QAction::triggered, [=]() {
        DeviceManager deviceManager(this);
        deviceManager.exec();
    });
    connect(actionOptions, &QAction::triggered, [=]() {
        OptionsDialog settings(this);
        connect(&settings, &OptionsDialog::saved, [=]() {
            toolbar->reinitialize();
        });
        settings.exec();
    });
    connect(actionSettingsReset, &QAction::triggered, this, &MainWindow::resetSettings);
    connect(actionWebsite, &QAction::triggered, app, &Application::visitWebPage);
    connect(actionGithub, &QAction::triggered, app, &Application::visitSourcePage);
    connect(actionDonate, &QAction::triggered, app, &Application::visitDonatePage);
    connect(actionUpdate, &QAction::triggered, [=]() {
        Updater::check(true, this);
    });
    connect(actionAboutQt, &QAction::triggered, app, &Application::aboutQt);
    connect(actionAbout, &QAction::triggered, [=]() {
        AboutDialog about(this);
        about.exec();
    });
}

void MainWindow::initLanguages()
{
    qDebug() << "Initializing languages...";
    QList<Language> languages = app->getLanguages();

    actionsLanguage = new QActionGroup(this);
    actionsLanguage->setExclusive(true);
    for (const Language &language : languages) {
        const QString localeTitle = language.getTitle();
        const QString localeCode = language.getCode();
        const QPixmap localeFlag = language.getFlag();
        QAction *action = actionsLanguage->addAction(localeFlag, localeTitle);
        action->setCheckable(true);
        action->setProperty("locale", localeCode);
        connect(action, &QAction::triggered, [=]() {
            app->setLanguage(localeCode);
        });
    }
    menuLanguage->addActions(actionsLanguage->actions());
}

void MainWindow::retranslate()
{
    tr("Remove Temporary Files..."); // TODO For future use

    // Tool Bar:

    toolbar->setWindowTitle(tr("Tools"));
    dockProjects->setWindowTitle(tr("Projects"));
    dockResources->setWindowTitle(tr("Resources"));
    dockFilesystem->setWindowTitle(tr("File System"));
    dockManifest->setWindowTitle(tr("Manifest"));
    dockIcons->setWindowTitle(tr("Icons"));

    // Menu Bar:

    menuFile->setTitle(tr("&File"));
    menuEditor->setTitle(tr("&Editor"));
    menuTools->setTitle(tr("&Tools"));
    menuSettings->setTitle(tr("&Settings"));
    menuWindow->setTitle(tr("&Window"));
    menuHelp->setTitle(tr("&Help"));

    // File Menu:

    actionApkOpen->setText(tr("&Open APK..."));
    actionApkSave->setText(tr("&Save APK..."));
    actionApkInstall->setText(tr("&Install APK..."));
    actionApkInstallExternal->setText(tr("Install &External APK..."));
    actionApkExplore->setText(tr("O&pen Contents"));
    actionApkClose->setText(tr("&Close APK"));
    actionExit->setText(tr("E&xit"));

    // Recent Menu:

    menuRecent->setTitle(tr("Open &Recent"));
    actionRecentClear->setText(tr("&Clear List"));
    actionRecentNone->setText(tr("No Recent Files"));

    // Tools Menu:

    //: This string refers to multiple keys (as in "Manager of keys").
    actionKeyManager->setText(tr("&Key Manager..."));
    //: This string refers to multiple devices (as in "Manager of devices").
    actionDeviceManager->setText(tr("&Device Manager..."));
    //: This string refers to a single project (as in "Manager of a project").
    actionProjectManager->setText(tr("&Project Manager"));
    actionTitleEditor->setText(tr("Edit Application &Title"));

    // Settings Menu:

    actionOptions->setText(tr("&Options..."));
    menuLanguage->setTitle(tr("&Language"));
    actionSettingsReset->setText(tr("&Reset Settings..."));

    // Window Menu:

    menuWindow->clear();
    menuWindow->addActions(createPopupMenu()->actions());

    // Help Menu:

    actionWebsite->setText(tr("Visit &Website"));
    actionGithub->setText(tr("&Source Code"));
    actionDonate->setText(tr("Make a &Donation"));
    actionUpdate->setText(tr("Check for &Updates"));
    actionAbout->setText(tr("&About APK Editor Studio..."));
    actionAboutQt->setText(tr("About &Qt..."));
}

void MainWindow::loadSettings()
{
    qDebug() << "Loading settings...";
    restoreGeometry(app->settings->getMainWindowGeometry());
    restoreState(app->settings->getMainWindowState());
    toolbar->reinitialize();
}

void MainWindow::resetSettings()
{
    if (app->settings->reset(this)) {
        restoreGeometry(QByteArray());
        setInitialSize();
        toolbar->reinitialize();
        restoreState(defaultState);
    }
}

void MainWindow::saveSettings()
{
    app->settings->setMainWindowGeometry(saveGeometry());
    app->settings->setMainWindowState(saveState());
}

Viewer *MainWindow::openResource(const QModelIndex &index)
{
    if (!index.model()->hasChildren(index)) {
        return projectsWidget->openResourceTab(index);
    }
    return nullptr;
}

bool MainWindow::setCurrentProject(Project *project)
{
    updateWindowForProject(project);
    resourceTree->setModel(project ? &project->resourcesModel : nullptr);
    filesystemTree->setModel(project ? &project->filesystemModel : nullptr);
    iconList->setModel(project ? &project->iconsProxy : nullptr);
    logView->setModel(project ? &project->logModel : nullptr);
    manifestTable->setModel(project ? &project->manifestModel : nullptr);
    filesystemTree->getView<FilesystemTree *>()->setRootIndex(project
        ? project->filesystemModel.index(project->getContentsPath())
        : QModelIndex());
    return project;
}

void MainWindow::setActionsEnabled(const Project *project)
{
    actionApkSave->setEnabled(project ? project->getState().canSave() : false);
    actionApkInstall->setEnabled(project ? project->getState().canInstall() : false);
    actionApkExplore->setEnabled(project ? project->getState().canExplore() : false);
    actionApkClose->setEnabled(project ? project->getState().canClose() : false);
    actionTitleEditor->setEnabled(project ? project->getState().canEdit() : false);
    actionProjectManager->setEnabled(project);
}

void MainWindow::updateWindowForProject(const Project *project)
{
    if (project) {
        setWindowTitle(QString("%1 [*]").arg(project->getOriginalPath()));
        setWindowModified(project->getState().isModified());
        setActionsEnabled(project);
    } else {
        setWindowTitle(QString());
        setWindowModified(false);
        setActionsEnabled(nullptr);
    }
}

void MainWindow::updateRecentMenu()
{
    menuRecent->clear();
    auto recentList = app->recent->all();
    for (const RecentFile &recentEntry : recentList) {
        QAction *action = new QAction(recentEntry.thumbnail(), recentEntry.filename(), this);
        menuRecent->addAction(action);
        connect(action, &QAction::triggered, [=]() {
            app->openApk(recentEntry.filename());
        });
    }
    menuRecent->addSeparator();
    menuRecent->addAction(recentList.isEmpty() ? actionRecentNone : actionRecentClear);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
        retranslate();
        const QString currentLocale = app->settings->getLanguage();
        QIcon flag(app->getLocaleFlag(currentLocale));
        menuLanguage->setIcon(flag);
        for (QAction *action : actionsLanguage->actions()) {
            if (action->property("locale") == currentLocale) {
                action->setChecked(true);
                break;
            }
        }
    } else {
        QWidget::changeEvent(event);
    }
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        event->acceptProposedAction();
        const bool showRubberBand = mimeData->urls().first().toString().toLower().endsWith(".apk");
        rubberBand->setGeometry(rect());
        rubberBand->setVisible(showRubberBand);
    }
}

void MainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event)
    rubberBand->hide();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urls = mimeData->urls();
        for (const QUrl &url : urls) {
            const QString file = url.toLocalFile();
            const QMimeType mime = QMimeDatabase().mimeTypeForFile(file);
            if (mime.inherits("application/zip")) {
                app->openApk(file);
                event->acceptProposedAction();
            }
        }
    }
    rubberBand->hide();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    const bool unsaved = projectsWidget->hasUnsavedProjects();
    if (unsaved) {
        const QString question = tr("You have unsaved changes.\nDo you want to discard them and exit?");
        const int answer = QMessageBox::question(this, QString(), question, QMessageBox::Discard, QMessageBox::Cancel);
        if (answer != QMessageBox::Discard) {
            event->ignore();
            return;
        }
    }
    saveSettings();
    event->accept();
}
