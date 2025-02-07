#include "windows/optionsdialog.h"
#include "windows/devicemanager.h"
#include "windows/keymanager.h"
#include "widgets/toolbar.h"
#include "base/application.h"
#include <QLabel>
#include <QPushButton>
#include <QAbstractButton>
#include <QDialogButtonBox>

#ifdef QT_DEBUG
    #include <QDebug>
#endif

OptionsDialog::OptionsDialog(QWidget *parent) : QDialog(parent)
{
    widget = nullptr;
    layout = new QVBoxLayout(this);
    initialize();
}

void OptionsDialog::addPage(const QString &title, QLayout *page, bool stretch)
{
    QWidget *container = new QWidget(this);
    QVBoxLayout *containerLayout = new QVBoxLayout(container);
    QLabel *titleLabel = new QLabel(QString("%1").arg(title), this);
    QFrame *titleLine = new QFrame(this);
    titleLine->setFrameShape(QFrame::HLine);
    titleLine->setFrameShadow(QFrame::Sunken);
    QFont titleFont = titleLabel->font();
#ifndef Q_OS_OSX
    titleFont.setPointSize(13);
#else
    titleFont.setPointSize(17);
#endif
    titleLabel->setFont(titleFont);
    containerLayout->addWidget(titleLabel);
    containerLayout->addWidget(titleLine);
    containerLayout->addLayout(page);
    if (stretch) {
        containerLayout->addStretch();
    }
    pageList->addItem(title);
    pageStack->addWidget(container);
}

void OptionsDialog::load()
{
    // General

    checkboxUpdates->setChecked(app->settings->getAutoUpdates());
    spinboxRecent->setValue(app->settings->getRecentLimit());

    // Languages

    QList<Language> languages = app->getLanguages();
    comboLanguages->clear();
    const QString currentLocale = app->settings->getLanguage();
    for (const Language &language : languages) {
        const QPixmap flag = language.getFlag();
        const QString title = language.getTitle();
        const QString code = language.getCode();
        comboLanguages->addItem(flag, title, code);
        if (code == currentLocale) {
            comboLanguages->setCurrentIndex(comboLanguages->count() - 1);
        }
    }
    comboLanguages->setCurrentText(app->settings->getLanguage());

    // Repacking

    fileboxApktool->setCurrentPath(app->settings->getApktoolPath());
    fileboxApktool->setDefaultPath(app->getSharedPath("tools/apktool.jar"));
    fileboxOutput->setCurrentPath(app->settings->getOutputDirectory());
    fileboxOutput->setDefaultPath(app->settings->getDefaultOutputDirectory());
    fileboxFrameworks->setCurrentPath(app->settings->getFrameworksDirectory());
    fileboxFrameworks->setDefaultPath(app->getLocalConfigPath("frameworks"));
    checkboxSources->setChecked(app->settings->getDecompileSources());

    // Signing

    groupSign->setChecked(app->settings->getSignApk());
    fileboxApksigner->setCurrentPath(app->settings->getApksignerPath());
    fileboxApksigner->setDefaultPath(app->getSharedPath("tools/apksigner.jar"));

    // Optimizing

    groupZipalign->setChecked(app->settings->getOptimizeApk());
    fileboxZipalign->setCurrentPath(app->settings->getZipalignPath());
    fileboxZipalign->setDefaultPath(app->getBinaryPath("zipalign"));

    // Installing

    fileboxAdb->setCurrentPath(app->settings->getAdbPath());
    fileboxAdb->setDefaultPath(app->getBinaryPath("adb"));

    // Toolbar

    listToolbarUsed->clear();
    QMap<QString, QAction *> unusedToolbarActions = Toolbar::all();
    QStringList usedToolbarActions = app->settings->getToolbar();
    for (const QString &identifier : usedToolbarActions) {
        if (identifier == "separator") {
            listToolbarUsed->addItem(createToolbarSeparatorItem());
        } else if (identifier == "spacer") {
            listToolbarUsed->addItem(createToolbarSpacerItem());
        } else {
            const QAction *action = unusedToolbarActions.value(identifier);
            unusedToolbarActions.remove(identifier);
            if (action) {
                QListWidgetItem *item = new QListWidgetItem(action->icon(), action->text().remove('&'));
                item->setData(PoolListWidget::IdentifierRole, identifier);
                item->setData(PoolListWidget::ReusableRole, false);
                listToolbarUsed->addItem(item);
            }
        }
    }

    listToolbarUnused->clear();
    QMapIterator<QString, QAction *> it(unusedToolbarActions);
    while (it.hasNext()) {
        it.next();
        const QString identifier = it.key();
        const QAction *action = it.value();
        QListWidgetItem *item = new QListWidgetItem(action->icon(), action->text().remove('&'));
        item->setData(PoolListWidget::IdentifierRole, identifier);
        listToolbarUnused->addItem(item, false);
    }
    listToolbarUnused->addItem(createToolbarSeparatorItem());
    listToolbarUnused->addItem(createToolbarSpacerItem());

    emit loaded();
}

void OptionsDialog::save()
{
    // General

    app->settings->setAutoUpdates(checkboxUpdates->isChecked());
    app->setLanguage(comboLanguages->currentData().toString());
    app->recent->setLimit(spinboxRecent->value());

    // Repacking

    app->settings->setApktoolPath(fileboxApktool->getCurrentPath());
    app->settings->setOutputDirectory(fileboxOutput->getCurrentPath());
    app->settings->setFrameworksDirectory(fileboxFrameworks->getCurrentPath());
    app->settings->setDecompileSources(checkboxSources->isChecked());

    // Signing

    app->settings->setSignApk(groupSign->isChecked());
    app->settings->setApksignerPath(fileboxApksigner->getCurrentPath());

    // Optimizing

    app->settings->setOptimizeApk(groupZipalign->isChecked());
    app->settings->setZipalignPath(fileboxZipalign->getCurrentPath());

    // Installing

    app->settings->setAdbPath(fileboxAdb->getCurrentPath());

    // Toolbar

    QStringList toolbar;
    for (int i = 0; i < listToolbarUsed->count(); ++i) {
        const QString identifier = listToolbarUsed->item(i)->data(PoolListWidget::IdentifierRole).toString();
        toolbar.append(identifier);
    }
    app->settings->setToolbar(toolbar);

    emit saved();
}

void OptionsDialog::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange) {
#ifdef QT_DEBUG
        qDebug() << "TODO Unwanted behavior: this event is fired twice";
#endif
        const int currentPage = pageList->currentRow();
        initialize();
        pageList->setCurrentRow(currentPage);
    } else {
        QWidget::changeEvent(event);
    }
}

void OptionsDialog::initialize()
{
    // Clear layout:

    if (widget) {
        delete widget;
    }

    widget = new QWidget(this);
    layout->addWidget(widget);

    setWindowTitle(tr("Options"));
    setWindowIcon(app->icons.get("settings.png"));
    setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);
    resize(app->scale(800, 400));

    // General

    QFormLayout *pageGeneral = new QFormLayout;
    checkboxUpdates = new QCheckBox(tr("Check for updates automatically"), this);
    btnAssociate = new QPushButton(tr("Set as default program for APK files"), this);
    btnAssociate->setIcon(app->icons.get("application.png"));
    btnAssociate->setMinimumHeight(app->scale(30));
    connect(btnAssociate, &QPushButton::clicked, [this]() {
        if (app->associate()) {
            QMessageBox::information(this, QString(), tr("File association has been created."));
        } else {
            QMessageBox::warning(this, QString(), tr("Could not register file assocation."));
        }
    });
#ifndef Q_OS_WIN
    btnAssociate->hide();
#endif
    comboLanguages = new QComboBox(this);
    spinboxRecent = new QSpinBox(this);
    spinboxRecent->setMinimum(0);
    spinboxRecent->setMaximum(50);
    pageGeneral->addRow(checkboxUpdates);
    pageGeneral->addRow(tr("Language:"), comboLanguages);
    pageGeneral->addRow(tr("Maximum recent files:"), spinboxRecent);
    pageGeneral->addRow(btnAssociate);

    // Repacking

    QFormLayout *pageRepack = new QFormLayout;
    fileboxApktool = new FileBox(QString(), QString(), false, this);
    fileboxOutput = new FileBox(QString(), QString(), true, this);
    fileboxFrameworks = new FileBox(QString(), QString(), true, this);
    checkboxSources = new QCheckBox(tr("Decompile source code (smali)"), this);
    //: "Apktool" is the name of the tool, don't translate it.
    pageRepack->addRow(tr("Apktool path:"), fileboxApktool);
    pageRepack->addRow(tr("Extraction path:"), fileboxOutput);
    pageRepack->addRow(tr("Frameworks path:"), fileboxFrameworks);
    pageRepack->addRow(checkboxSources);
    pageRepack->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    tr("Java path:"); // TODO For future usage

    // Signing

    QVBoxLayout *pageSign = new QVBoxLayout;
    groupSign = new QGroupBox(tr("Enable"), this);
    groupSign->setCheckable(true);
    fileboxApksigner = new FileBox(QString(), QString(), false, this);
    QFormLayout *layoutSign = new QFormLayout(groupSign);
    //: This string refers to multiple keys (as in "Manager of keys").
    QPushButton *btnKeyManager = new QPushButton(tr("Open Key Manager"), this);
    btnKeyManager->setIcon(app->icons.get("key.png"));
    btnKeyManager->setMinimumHeight(app->scale(30));
    connect(btnKeyManager, &QPushButton::clicked, [=]() {
        KeyManager keyManager(this);
        keyManager.exec();
    });
    //: "Apksigner" is the name of the tool, don't translate it.
    layoutSign->addRow(tr("Apksigner path:"), fileboxApksigner);
    layoutSign->addRow(btnKeyManager);
    layoutSign->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    pageSign->addWidget(groupSign);

    // Optimizing

    QVBoxLayout *pageZipalign = new QVBoxLayout;
    groupZipalign = new QGroupBox(tr("Enable"), this);
    groupZipalign->setCheckable(true);
    fileboxZipalign = new FileBox(QString(), QString(), false, this);
    QFormLayout *layoutZipalign = new QFormLayout(groupZipalign);
    //: "Zipalign" is the name of the tool, don't translate it.
    layoutZipalign->addRow(tr("Zipalign path:"), fileboxZipalign);
    layoutZipalign->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);
    pageZipalign->addWidget(groupZipalign);

    // Installing

    QFormLayout *pageInstall = new QFormLayout;
    fileboxAdb = new FileBox(QString(), QString(), false, this);
    //: This string refers to multiple devices (as in "Manager of devices").
    QPushButton *btnDeviceManager = new QPushButton(tr("Open Device Manager"), this);
    btnDeviceManager->setIcon(app->icons.get("devices.png"));
    btnDeviceManager->setMinimumHeight(app->scale(30));
    connect(btnDeviceManager, &QPushButton::clicked, [=]() {
        DeviceManager deviceManager(this);
        deviceManager.exec();
    });
    //: "ADB" is the name of the tool, don't translate it.
    pageInstall->addRow(tr("ADB path:"), fileboxAdb);
    pageInstall->addRow(btnDeviceManager);
    pageInstall->setFieldGrowthPolicy(QFormLayout::ExpandingFieldsGrow);

    // Toolbar

    QHBoxLayout *pageToolbar = new QHBoxLayout;
    listToolbarUsed = new QListWidget(this);
    listToolbarUsed->setIconSize(app->scale(20, 20));
    listToolbarUsed->setDragDropMode(QAbstractItemView::DragDrop);
    listToolbarUsed->setDefaultDropAction(Qt::MoveAction);
    listToolbarUnused = new PoolListWidget(this);
    listToolbarUnused->setIconSize(app->scale(20, 20));
    connect(listToolbarUsed, &QListWidget::doubleClicked, [=](const QModelIndex &index) {
        QListWidgetItem *item = listToolbarUsed->takeItem(index.row());
        const bool reusable = item->data(PoolListWidget::ReusableRole).toBool();
        if (!reusable) {
            listToolbarUnused->addItem(item);
        }
    });
    connect(listToolbarUnused, &QListWidget::doubleClicked, [=](const QModelIndex &index) {
        QListWidgetItem *item = listToolbarUnused->item(index.row());
        const bool reusable = item->data(PoolListWidget::ReusableRole).toBool();
        listToolbarUsed->addItem(new QListWidgetItem(*item));
        if (!reusable) {
            delete item;
        }
    });

    pageToolbar->addWidget(listToolbarUsed);
    pageToolbar->addWidget(listToolbarUnused);

    // Initialize

    pageStack = new QStackedWidget(this);
    pageStack->setFrameShape(QFrame::StyledPanel);
    pageList = new QListWidget(this);
    addPage(tr("General"), pageGeneral);
    addPage(tr("Repacking"), pageRepack);
    addPage(tr("Signing APK"), pageSign);
    addPage(tr("Optimizing APK"), pageZipalign);
    addPage(tr("Installing APK"), pageInstall);
    addPage(tr("Toolbar"), pageToolbar, false);
    pageList->setCurrentRow(0);
    pageList->setMaximumWidth(pageList->sizeHintForColumn(0) + 60);

    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply);
    QPushButton *btnApply = buttons->button(QDialogButtonBox::Apply);

    QGridLayout *layoutPages = new QGridLayout(widget);
    layoutPages->addWidget(pageList, 0, 0);
    layoutPages->addWidget(pageStack, 0, 1);
    layoutPages->addWidget(buttons, 1, 0, 1, 2);

    load();

    connect(pageList, &QListWidget::currentRowChanged, pageStack, &QStackedWidget::setCurrentIndex);
    connect(buttons, &QDialogButtonBox::accepted, this, &OptionsDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &OptionsDialog::reject);
    connect(btnApply, &QPushButton::clicked, this, &OptionsDialog::save);
    connect(this, &OptionsDialog::accepted, this, &OptionsDialog::save);
}

QListWidgetItem *OptionsDialog::createToolbarSeparatorItem() const
{
    QIcon icon = app->icons.get("separator.png");
    //: Separator is a toolbar element which divides buttons with a vertical line.
    QListWidgetItem *separator = new QListWidgetItem(icon, tr("Separator"));
    separator->setData(PoolListWidget::IdentifierRole, "separator");
    separator->setData(PoolListWidget::ReusableRole, true);
    return separator;
}

QListWidgetItem *OptionsDialog::createToolbarSpacerItem() const
{
    QIcon icon = app->icons.get("spacer.png");
    //: Spacer is a toolbar element which divides buttons with an empty space.
    QListWidgetItem *spacer = new QListWidgetItem(icon, tr("Spacer"));
    spacer->setData(PoolListWidget::IdentifierRole, "spacer");
    spacer->setData(PoolListWidget::ReusableRole, true);
    return spacer;
}
