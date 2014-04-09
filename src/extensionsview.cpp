/*
    Copyright (C) 2010 Nikita Nemkin <nikita@nemkin.ru>

    This file is part of PerpetuumPlanner.

    PerpetuumPlanner is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    PerpetuumPlanner is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with PerpetuumPlanner. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QMouseEvent>
#include <QPushButton>
#include <QDialogButtonBox>
#include <QMenu>
#include <QSettings>
#include <QFileDialog>
#include <QPixmapCache>
#include <QPainter>
#include <QDebug>

#include "extensionsview.h"
#include "application.h"
#include "levelchooser.h"
#include "attributeeditor.h"
#include "delegates.h"
#include "extensionwindow.h"
#include "messagebox.h"
#include "agent.h"
#include "util.h"


// ExtensionsModel

ExtensionsModel::ExtensionsModel(Agent *agent, QObject *parent) : SimpleTreeModel(parent), m_agent(agent)
{
    beginResetModel();
    setSort(0, SortKeyRole);

    foreach (ExtensionCategory *category, m_agent->gameData()->extensionCategories()) {
        if (category->extensions().isEmpty() || category->hidden())
            continue;

        Node *categoryNode = rootNode()->addChild(category);
        foreach (Extension *extension, category->extensions())
            if (!extension->hidden())
                categoryNode->addChild(extension);
    }
    endResetModel();

    connect(agent, SIGNAL(extensionsChanged()), this, SLOT(agentExtensionsChanged()));
}

QVariant ExtensionsModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            //: entityinfo_bonus_extensionname
            case 0: return tr("Extension");
            //: contextmenu_definition_info
            case 1: return tr("Info");
            case 2: return tr("Current");
            //: knowledgebaseupdate_progress
            case 3: return tr("Progress");
            case 4: return tr("Planned");
            //: extension_nextlevelcost
            case 5: return tr("Next Lvl");
            }
        }
        else if (role == Qt::TextAlignmentRole) {
            return Qt::AlignCenter;
        }
    }
    return QVariant();
}

void ExtensionsModel::agentExtensionsChanged()
{
    emit dataChanged(index(0, 1), index(rowCount(), columnCount() - 1));
}

QVariant ExtensionsModel::data(const QModelIndex &index, int role) const
{
    if (Extension *extension = fromIndex<Extension *>(index)) {
        if (role == Qt::DisplayRole || role == SortKeyRole) {
            int points;
            switch (index.column()) {
            case 0:
                return extension->name();
            case 1:
                return QString("<div class=\"complexity\">[ %1 ]</div>").arg(extension->complexity());
            case 2:
                points = m_agent->currentExtensions()->points(extension);
                if (points) {
                    return QString("<div class=\"counter current\"><div class=\"ep\">%L1</div><div class=\"time\">%2</div></div>")
                        .arg(points).arg(Formatter::formatTimeSpanCompact(points * 60));
                }
                break;
            case 4:
                points = m_agent->plannedExtensions()->points(extension) - m_agent->currentExtensions()->points(extension);
                if (points) {
                    return QString("<div class=\"counter planned\"><div class=\"ep\">%L1</div><div class=\"time\">%2</div></div>")
                            .arg(points).arg(Formatter::formatTimeSpanCompact(points * 60));
                }
                break;
            case 5:
                points = m_agent->plannedExtensions()->nextLevelPoints(extension);
                if (points) {
                    points += m_agent->plannedExtensions()->reqPoints(extension);
                    return QString("<div class=\"counter next\"><div class=\"ep\">%L1</div><div class=\"time\">%2</div></div>")
                            .arg(points).arg(Formatter::formatTimeSpanCompact(points * 60));
                }
                break;
            }
        }
        else if (role == Qt::ToolTipRole) {
            if (index.column() == 5) {
                int reqPoints = m_agent->plannedExtensions()->reqPoints(extension);
                if (reqPoints) {
                    //: extensioninfo_requiredextensions
                    return tr("Prerequisites: %L1 EP").arg(reqPoints);
                }
            }
        }
        else if (role == Qt::TextAlignmentRole) {
            return QVariant((index.column() == 0 ? Qt::AlignLeft : Qt::AlignHCenter) | Qt::AlignVCenter);
        }
        else if (role == Qt::SizeHintRole) {
            return QSize(0, 32);
        }
        else if (role == Qt::UserRole+1) {
            return m_agent->currentExtensions()->level(extension);
        }
        else if (role == Qt::UserRole+2) {
            return m_agent->plannedExtensions()->level(extension);
        }
    }
    else if (ExtensionCategory *category = fromIndex<ExtensionCategory *>(index)) {
        if (role == Qt::DisplayRole) {
            int points;
            switch (index.column()) {
            case 0:
                return QString("<div class=\"group\">%1</div>").arg(category->name());
            case 2:
                points = m_agent->currentExtensions()->points(category);
                if (points)
                    return QString("<div class=\"group\"><span class=\"current\">%L1</span></div>").arg(points);
                break;
            case 4:
                points = m_agent->plannedExtensions()->points(category) - m_agent->currentExtensions()->points(category);
                if (points)
                    return QString("<div class=\"group\"><span class=\"planned\">%L1</span></div>").arg(points);
                break;
            }
        }
        else if (role == Qt::SizeHintRole) {
            return QSize(0, 25);
        }
        else if (role == Qt::TextAlignmentRole) {
            switch (index.column()) {
            case 0:
                return (int) Qt::AlignLeft | Qt::AlignVCenter;
            default:
                return (int) Qt::AlignCenter;
            }
        }
    }
    return QVariant();
}

bool ExtensionsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (Extension *extension = fromIndex<Extension *>(index)) {
        switch (role) {
        case Qt::UserRole + 1:
            m_agent->currentExtensions()->setLevel(extension, value.toInt());
            return true;
        case Qt::UserRole + 2:
            m_agent->plannedExtensions()->setLevel(extension, value.toInt());
            return true;
        }
    }
    return false;
}

Qt::ItemFlags ExtensionsModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}


// ExtensionsView

ExtensionsView::ExtensionsView(QWidget *parent) : QWidget(parent), m_agent(0), m_attributeEditor(0)
{
    setupUi(this);

    connect(buttonQuit, SIGNAL(clicked()), this, SIGNAL(closeRequest()));

    m_popupCurrent = new QMenu(tr("Current level"), this);
    m_popupPlanned = new QMenu(tr("Planned level"), this);

    for (int planned = 0; planned < 2; ++planned) {
        QMenu *menu = planned ? m_popupPlanned : m_popupCurrent;
        int modifier = planned ? 0 : Qt::CTRL;

        //: extension_upgrade
        QAction *action = new QAction(tr("Upgrade"), this);
        action->setData(1);
        action->setProperty("relative", true);
        action->setProperty("planned", (bool) planned);
        action->setShortcut(QKeySequence(modifier | Qt::Key_Plus));
        connect(action, SIGNAL(triggered()), this, SLOT(actionSetLevelTriggered()));
        menu->addAction(action);

        action = new QAction(tr("Degrade"), this);
        action->setData(-1);
        action->setProperty("relative", true);
        action->setProperty("planned", (bool) planned);
        action->setShortcut(QKeySequence(modifier | Qt::Key_Minus));
        connect(action, SIGNAL(triggered()), this, SLOT(actionSetLevelTriggered()));
        menu->addAction(action);

        menu->addSeparator();

        for (int i = 0; i <= 10; ++i) {
            int key;
            switch (i) {
            case 0: key = Qt::Key_Delete; break;
            case 10: key = Qt::Key_0; break;
            default: key = Qt::Key_0 + i;
            }

            action = new QAction(tr("Set %1").arg(i), this);
            action->setData(i);
            action->setProperty("planned", planned);
            action->setShortcut(QKeySequence(modifier | key));
            connect(action, SIGNAL(triggered()), this, SLOT(actionSetLevelTriggered()));
            menu->addAction(action);
        }
    }

    treeExtensions->installEventFilter(this);

    addActions(findChildren<QAction *>());
}

void ExtensionsView::initialize(QSettings &settings, GameData *gameData, const QString &fileName)
{
    m_agent = new Agent(gameData, this);

    QFileInfo defaultAgent(QFileInfo(settings.fileName()).dir(), "default.agent");
    QString startFileName = !fileName.isEmpty() ? fileName : settings.value("LastFileName", defaultAgent.filePath()).toString();
    if (!startFileName.isEmpty()) {
        if (!m_agent->load(startFileName)) {
            //: combatlog_error
            MessageBox::exec(this, tr("Error"), tr("Unable to load\n%1\n%2").arg(
                                 QDir::toNativeSeparators(startFileName), m_agent->lastError()));
        }
    }

    connect(m_agent, SIGNAL(extensionsChanged()), this, SLOT(agentExtensionsChanged()));
    connect(m_agent, SIGNAL(persistenceChanged()), this, SLOT(agentPersistenceChanged()));

    treeExtensions->setModel(new ExtensionsModel(m_agent, this));
    // treeExtensions->setHeader(new FixedFirstHeader());

    RichTextDelegate *richTextDelegate = new RichTextDelegate(treeExtensions);
    treeExtensions->setItemDelegateForColumn(0, richTextDelegate);
    treeExtensions->setItemDelegateForColumn(1, richTextDelegate);
    treeExtensions->setItemDelegateForColumn(2, richTextDelegate);
    treeExtensions->setItemDelegateForColumn(3, new LevelChooserDelegate(treeExtensions));
    treeExtensions->setItemDelegateForColumn(4, richTextDelegate);
    treeExtensions->setItemDelegateForColumn(5, richTextDelegate);

    treeExtensions->header()->restoreState(settings.value("ExtensionsView/Header").toByteArray());

    bool bigFonts = QSettings().value("BigFonts").toBool();
    treeExtensions->header()->resizeSection(1, 75);
    treeExtensions->header()->resizeSection(2, bigFonts ? 70 : 65);
    treeExtensions->header()->resizeSection(3, 160);
    treeExtensions->header()->resizeSection(4, bigFonts ? 70 : 65);
    treeExtensions->header()->resizeSection(5, bigFonts ? 75 : 70);
    treeExtensions->header()->resizeSection(6, bigFonts ? 70 : 65);
    treeExtensions->header()->setResizeMode(QHeaderView::Fixed);
    treeExtensions->header()->setResizeMode(0, QHeaderView::Stretch);

    restoreTreeState(treeExtensions, settings.value("ExtensionsView/ExpandedGroups").toBitArray(), true);

    labelCurrentPoints->setText("0"); // any non-empty string to create internal QTextDocument
    labelPlannedPoints->setText("0");
    labelCurrentPoints->findChild<QTextDocument *>()->setDefaultStyleSheet(qApp->docStyleSheet());
    labelPlannedPoints->findChild<QTextDocument *>()->setDefaultStyleSheet(qApp->docStyleSheet());

    m_headerActions = new HeaderActions(treeExtensions->header(), this);

    agentExtensionsChanged();
    agentPersistenceChanged();
}

void ExtensionsView::finalize(QSettings &settings)
{
    if (m_attributeEditor)
        settings.setValue("Geometry/AttributeEditor", m_attributeEditor->saveGeometry());

    settings.setValue("ExtensionsView/ExpandedGroups", saveTreeState(treeExtensions));
    settings.setValue("ExtensionsView/Header", treeExtensions->header()->saveState());

    settings.setValue("LastFileName", m_agent->fileName());
}

void ExtensionsView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    switch (event->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        agentExtensionsChanged();
        agentPersistenceChanged();
        break;
    case QEvent::WindowTitleChange:
        emit windowTitleChange(windowTitle());
        break;
    }
}

void ExtensionsView::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged) {
        bool bigFonts = QSettings().value("BigFonts").toBool();
        treeExtensions->header()->resizeSection(2, bigFonts ? 70 : 65);
        treeExtensions->header()->resizeSection(4, bigFonts ? 70 : 65);
        treeExtensions->header()->resizeSection(5, bigFonts ? 75 : 70);

        for (int i = 0; i < 5; ++i)
            Application::sendEvent(treeExtensions->itemDelegateForColumn(i), event);
    }
}

void ExtensionsView::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void ExtensionsView::dropEvent(QDropEvent *event)
{
    QList<QUrl> urls = event->mimeData()->urls();
    if (!urls.isEmpty()) {
        if (canClose()) {
            m_agent->load(urls.at(0).toLocalFile());

            QWidget *w = window();
            w->activateWindow();
            w->raise();
        }
    }
}

bool ExtensionsView::eventFilter(QObject *object, QEvent *event)
{
    if (object == treeExtensions) {
        if (event->type() == QEvent::KeyPress) {
            QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
            if (keyEvent->key() == Qt::Key_A && (keyEvent->modifiers() & Qt::CTRL)) {
                // Select All (Ctrl+A) won't work until all groups are expanded
                treeExtensions->expandToDepth(0);
            }
        }
    }
    return false;
}



bool ExtensionsView::canClose()
{
    if (m_agent->isModified()) {
        //: window_confirm
        int result = MessageBox::exec(this, tr("Confirmation"), tr("Save changes to '%1'?").arg(m_agent->name()),
                                      QDialogButtonBox::Yes | QDialogButtonBox::No /*| QDialogButtonBox::Cancel*/);
        if (result == QDialogButtonBox::Cancel)
            return false;

        if (result == QDialogButtonBox::Yes)
            actionSave->trigger();
    }
    return true;
}

void ExtensionsView::agentExtensionsChanged()
{
    int currentPoints = m_agent->currentExtensions()->points(false);
    int plannedPoints = m_agent->plannedExtensions()->points(false);

    labelCurrentPoints->setText(QString("<span class=\"current\">%L1</span><br/>%2")
                                .arg(currentPoints).arg(Formatter::formatTimeSpan(currentPoints * 60)));
    labelPlannedPoints->setText(QString("<span class=\"planned\">%L1</span><br/>%2")
                                .arg(plannedPoints).arg(Formatter::formatTimeSpan(plannedPoints * 60)));
}

void ExtensionsView::agentPersistenceChanged()
{
    QString modifiedFlag = m_agent->isModified() ? tr(" [Modified]") : QString();
    setWindowTitle(QString("%1%2 - %3 %4").arg(m_agent->name(), modifiedFlag, qApp->applicationName(), qApp->applicationVersion()));
}

void ExtensionsView::actionSetLevelTriggered()
{
    QAction *action = static_cast<QAction *>(sender());

    int level = action->data().toInt();
    bool relative = action->property("relative").toBool();
    ExtensionLevels *extensionLevels = action->property("planned").toBool() ? m_agent->plannedExtensions() : m_agent->currentExtensions();

    foreach (QModelIndex index, treeExtensions->selectionModel()->selectedRows(0)) {
        Extension *extension = static_cast<ExtensionsModel *>(treeExtensions->model())->fromIndex<Extension *>(index);
        if (extension)
            extensionLevels->setLevel(extension, level + relative * extensionLevels->level(extension));
    }
}

void ExtensionsView::on_treeExtensions_customContextMenuRequested(QPoint pos)
{
    bool hasSelection = static_cast<ExtensionsModel *>(treeExtensions->model())->fromIndex<Extension *>(treeExtensions->currentIndex());

    QMenu menu;
    if (hasSelection) {
        menu.addAction(actionInformation);
        menu.addSeparator();
        menu.addAction(m_popupCurrent->menuAction());
        menu.addAction(m_popupPlanned->menuAction());
        menu.addSeparator();
    }
    menu.addAction(actionExpandAll);
    menu.addAction(actionCollapseAll);
    menu.addSeparator();
    menu.addMenu(m_headerActions);

    menu.exec(treeExtensions->viewport()->mapToGlobal(pos));
}

void ExtensionsView::on_treeExtensions_doubleClicked(QModelIndex index)
{
    if (index.column() > 0) {
        QModelIndex first = index.sibling(index.row(), 0);
        if (first.model()->hasChildren(first))
            treeExtensions->setExpanded(first, !treeExtensions->isExpanded(first));
    }
    if (index.column() != 3)
        actionInformation->trigger();
}

void ExtensionsView::on_actionAttributeEditor_triggered()
{
    if (!m_attributeEditor) {
        m_attributeEditor = new AttributeEditor(m_agent, this);

        QSettings settings;
        m_attributeEditor->restoreGeometry(settings.value("Geometry/AttributeEditor").toByteArray());
    }
    m_attributeEditor->setVisible(!m_attributeEditor->isVisible());
    if (m_attributeEditor->isVisible())
        m_attributeEditor->activateWindow();
}

void ExtensionsView::on_actionInformation_triggered()
{
    Extension *extension = static_cast<ExtensionsModel *>(treeExtensions->model())->fromIndex<Extension *>(treeExtensions->currentIndex());
    if (extension) {
        ExtensionWindow *ew = new ExtensionWindow(extension, this);
        ew->show();
        ew->activateWindow();
    }
}

void ExtensionsView::on_actionSave_triggered()
{
    if (m_agent->fileName().isEmpty())
        actionSaveAs->trigger();
    else if (!m_agent->save())
        MessageBox::exec(this, tr("Error"), tr("Unable to save\n%1\n%2").arg(
                             QDir::toNativeSeparators(m_agent->fileName()), m_agent->lastError()));
}

void ExtensionsView::on_actionSaveAs_triggered()
{
    QSettings settings;
    QString lastUsedDir = settings.value("LastUsedDir").toString();
    QString fileName = QFileDialog::getSaveFileName(this, tr("Save plan"), lastUsedDir,
                                                    tr("Agents (*.agent);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        settings.setValue("LastUsedDir", QFileInfo(fileName).dir().canonicalPath());
        if (!m_agent->save(fileName))
            MessageBox::exec(this, tr("Error"), tr("Unable to save\n%1\n%2").arg(
                                 QDir::toNativeSeparators(fileName), m_agent->lastError()));
    }
}

void ExtensionsView::on_actionLoad_triggered()
{
    QSettings settings;
    QString lastUsedDir = settings.value("LastUsedDir").toString();
    QString fileName = QFileDialog::getOpenFileName(this, tr("Load plan"), lastUsedDir,
                                                    tr("Agents (*.agent);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        settings.setValue("LastUsedDir", QFileInfo(fileName).dir().canonicalPath());
        if (canClose())
            if (!m_agent->load(fileName))
                MessageBox::exec(this, tr("Error"), m_agent->lastError());
    }
}

void ExtensionsView::on_actionImport_triggered()
{
    QSettings settings;
    QString lastImportDir = settings.value("LastImportDir").toString();
    QString fileName = QFileDialog::getOpenFileName(this, tr("Import extension history"), lastImportDir,
                                                    tr("CSV files (*.csv);;All files (*.*)"));
    if (!fileName.isEmpty()) {
        settings.setValue("LastImportDir", QFileInfo(fileName).dir().canonicalPath());
        if (!m_agent->importHistory(fileName))
            MessageBox::exec(this, tr("Error"), m_agent->lastError());
    }
}

void ExtensionsView::on_actionNew_triggered()
{
    if (canClose())
        m_agent->reset();
}


// FixedFirstHeader

FixedFirstHeader::FixedFirstHeader(QWidget *parent) : QHeaderView(Qt::Horizontal, parent)
{
    connect(this, SIGNAL(sectionMoved(int,int,int)), this, SLOT(handleSectionMoved(int,int,int)));
}

void FixedFirstHeader::mousePressEvent(QMouseEvent *e)
{
    int visual = visualIndexAt(e->x());
    if (visual != -1) {
        int log = logicalIndex(visual);
        if (log == 0) {
            int pos = sectionViewportPosition(log);
            if (e->x() >= pos && e->x() < pos + sectionSize(log))
                return;
        }
    }
    QHeaderView::mousePressEvent(e);
}

void FixedFirstHeader::handleSectionMoved(int, int, int newVisualIndex)
{
    if (newVisualIndex == 0)
        moveSection(0, 1);
}


// HeaderActions

HeaderActions::HeaderActions(QHeaderView *header, QWidget *parent) : QMenu(tr("Columns"), parent), m_header(header)
{
    setProperty("role", QString("checks"));

    QAbstractItemModel *model = header->model();
    for (int i = 1; i < model->columnCount(); ++i) {
        QAction *action = addAction(model->headerData(i, Qt::Horizontal).toString());
        action->setData(i);
        action->setCheckable(true);
        action->setChecked(!header->isSectionHidden(i));
        connect(action, SIGNAL(triggered(bool)), this, SLOT(actionTriggered(bool)));
    }
    addSeparator();
    QAction *action = addAction(tr("Reset"));
    connect(action, SIGNAL(triggered()), this, SLOT(actionResetTriggered()));

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(menuRequested(QPoint)));
}

void HeaderActions::actionTriggered(bool checked)
{
    int logicalIndex = static_cast<QAction *>(sender())->data().toInt();
    m_header->setSectionHidden(logicalIndex, !checked);
}

void HeaderActions::actionResetTriggered()
{
    for (int i = 1; i < m_header->count(); ++i) {
        actions().at(i - 1)->setChecked(true);
        m_header->showSection(i);
    }
}

void HeaderActions::menuRequested(const QPoint &pos)
{
    QMenu::popup(m_header->viewport()->mapToGlobal(pos));
}
