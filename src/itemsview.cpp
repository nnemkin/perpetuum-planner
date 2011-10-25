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

#include <QApplication>
#include <QSettings>
#include <QMenu>
#include <QClipboard>
#include <QPainter>
#include <QWidgetAction>
#include <QDebug>

#include "application.h"
#include "itemsview.h"
#include "gamedata.h"
#include "models.h"
#include "delegates.h"
#include "util.h"
#include "tierfilter.h"


// Helpers

static void modelToTextHelper(QString &text, QAbstractItemModel *model, const QModelIndex &parent = QModelIndex())
{
    int rowCount = model->rowCount(parent);
    int colCount = model->columnCount(parent) - 1;

    for (int i = 0; i < rowCount; ++i) {
        for (int j = 0; j < colCount; ++j) {
            QModelIndex index = model->index(i, j, parent);
            text += model->data(index, Qt::DisplayRole).toString();
            text += QLatin1Char('\t');
        }

        while (text.endsWith(QLatin1Char('\t')))
            text.chop(1);
        text += QLatin1Char('\n');

        QModelIndex index = model->index(i, 0, parent);
        modelToTextHelper(text, model, index);
    }
}

static QString modelToText(QAbstractItemModel *model) {
    QString text;
    text.reserve(1024);

    for (int j = 0; j < model->columnCount(); ++j) {
        text += model->headerData(j, Qt::Horizontal, Qt::DisplayRole).toString();
        text += QLatin1Char('\t');
    }
    while (text.endsWith(QLatin1Char('\t')))
        text.chop(1);

    if (!text.isEmpty())
        text += QLatin1Char('\n');

    modelToTextHelper(text, model);
    return text;
}


// ItemsView

ItemsView::ItemsView(QWidget *parent) : QWidget(parent), m_gameData(0), m_groupsModel(0), m_itemsModel(0)
{
    setupUi(this);

    treeGroups->addAction(actionGroupsExpandAll);
    treeGroups->addAction(actionGroupsCollapseAll);

    textDescription->document()->setDefaultStyleSheet(qApp->docStyleSheet());

    for (int i = 0; i < tabWidget->count(); ++i)
        m_tabs << tabWidget->widget(i);

    splitterInfo->handle(1)->installEventFilter(this);

    treeParameters->addAction(actionCopyToClipboard);
    treeComponents->addAction(actionCopyToClipboard);
    treeComponentUse->addAction(actionCopyToClipboard);

    connect(treeParameters, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(tableDoubleClicked(QModelIndex)));
    connect(treeComponents, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(tableDoubleClicked(QModelIndex)));
    connect(treeBonuses, SIGNAL(doubleClicked(QModelIndex)), this, SLOT(tableDoubleClicked(QModelIndex)));

    QWidgetAction *widgetAction = new QWidgetAction(this);
    m_tierFilter = new TierFilter(this);
    widgetAction->setDefaultWidget(m_tierFilter);

    QMenu *menu = new QMenu(this);
    menu->setObjectName("menuTiers");
    menu->addAction(widgetAction);
    buttonTiers->setMenu(menu);

    connect(m_tierFilter, SIGNAL(changed(QStringList)), this, SLOT(tierFilterChanged()));
}

void ItemsView::initialize(QSettings &settings, GameData *gameData)
{
    m_groupsModel = new MarketTreeModel(gameData->rootCategory(), this);
    treeGroups->setModel(m_groupsModel);

    m_itemsModel = new DefinitionListModel(this);
    listItems->setModel(m_itemsModel);

    QStringList hiddenTiers = settings.value("ItemsView/HideTiers").toStringList();
    bool logicalOrder = settings.value("ItemsView/LogicalOrder").toBool();
    bool tierIcons = settings.value("ItemsView/TierIcons").toBool();
    if (settings.contains("ItemsView/HidePrototypes")) {
        if (settings.value("ItemsView/HidePrototypes").toBool())
            hiddenTiers << "tierlevel_t2_pr" << "tierlevel_t3_pr" << "tierlevel_t4_pr";
        settings.remove("ItemsView/HidePrototypes");
    }
    m_itemsModel->setHiddenTiers(hiddenTiers);
    m_itemsModel->setLogicalOrder(logicalOrder);
    m_itemsModel->setShowTierIcons(tierIcons);
    checkLogicalOrder->setChecked(logicalOrder);
    m_tierFilter->setHiddenTiers(hiddenTiers);

    ForegroundFixDelegate *fgFixDelefate = new ForegroundFixDelegate(this);

    m_parameters = new AggregatesModel(this);
    treeParameters->setModel(m_parameters);
    treeParameters->setHeader(new ItemTableHeader(220, 60, 200, treeParameters));
    treeParameters->setItemDelegateForColumn(0, new SortableRowDelegate(treeParameters));
    treeParameters->setItemDelegate(fgFixDelefate);

    m_components = new ComponentsModel(this);
    m_components->setSmallComponentIcons(settings.value(QLatin1String("ItemsView/SmallComponentIcons")).toBool());
    treeComponents->setModel(m_components);
    treeComponents->setHeader(new ItemTableHeader(220, 45, 140, treeComponents));
    treeComponents->setItemDelegateForColumn(0, new SortableRowDelegate(treeComponents));

    m_requirements = new RequirementsModel(this);
    treeRequirements->setModel(m_requirements);

    m_bonuses = new BonusesModel(this);
    treeBonuses->setModel(m_bonuses);
    treeBonuses->setHeader(new ItemTableHeader(220, 45, 80, treeBonuses));
    treeBonuses->setItemDelegate(fgFixDelefate);
    treeBonuses->setItemDelegateForColumn(0, new SortableRowDelegate(treeBonuses));

    m_componentUse = new DefinitionUseModel(this);
    treeComponentUse->setModel(m_componentUse);
    treeComponentUse->setHeader(new ItemTableHeader(220, 60, 140, treeComponentUse));

    Category *selectedCat = gameData->findByName<Category *>(settings.value(QLatin1String("ItemsView/SelectedGroup")).toString());
    if (selectedCat) {
        m_itemsModel->setCategory(selectedCat);
        QModelIndex groupIndex = m_groupsModel->objectIndex(selectedCat);
        if (groupIndex.isValid()) {
            treeGroups->selectionModel()->select(groupIndex, QItemSelectionModel::Select);

            QStringList defNames = settings.value(QLatin1String("ItemsView/SelectedItems")).toStringList();
            QList<Definition *> definitions;
            foreach (const QString &defName, defNames) {
                Definition *definition = gameData->findByName<Definition *>(defName);
                if (definition)
                    definitions << definition;
            }
            if (!definitions.isEmpty()) {
                foreach (Definition *item, definitions)
                    listItems->selectionModel()->select(m_itemsModel->objectIndex(item), QItemSelectionModel::Select);
                setDefinitions(definitions);
            }
        }
    }

    restoreTreeState(treeGroups, settings.value(QLatin1String("ItemsView/ExpandedGroups")).toBitArray());

    splitterInfo->restoreState(settings.value(QLatin1String("ItemsView/Splitter1")).toByteArray());
    splitterItems->restoreState(settings.value(QLatin1String("ItemsView/Splitter2")).toByteArray());
    stackInfo->setCurrentIndex(!m_definitions.isEmpty());

    tabWidget->setCurrentIndex(settings.value(QLatin1String("ItemsView/ActiveTab")).toInt());

    // NB: connect after the above initialization
    connect(treeGroups->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(groupSelectionChanged(QItemSelection)));
    connect(listItems->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(itemSelectionChanged()));
    connect(m_itemsModel, SIGNAL(modelAboutToBeReset()), listItems, SLOT(clearSelection()));
    connect(m_itemsModel, SIGNAL(layoutChanged()), this, SLOT(itemSelectionChanged()));

    connect(textFilter, SIGNAL(textChanged(QString)), m_itemsModel, SLOT(setNameFilter(QString)));
    connect(checkLogicalOrder, SIGNAL(clicked(bool)), m_itemsModel, SLOT(setLogicalOrder(bool)));
    connect(m_tierFilter, SIGNAL(changed(QStringList)), m_itemsModel, SLOT(setHiddenTiers(QStringList)));

    connect(treeParameters, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
    connect(treeComponents, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
    connect(treeBonuses, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
}

void ItemsView::finalize(QSettings &settings)
{
    settings.setValue(QLatin1String("ItemsView/LogicalOrder"), checkLogicalOrder->isChecked());
    settings.setValue(QLatin1String("ItemsView/Splitter1"), splitterInfo->saveState());
    settings.setValue(QLatin1String("ItemsView/Splitter2"), splitterItems->saveState());
    settings.setValue(QLatin1String("ItemsView/ActiveTab"), tabWidget->currentIndex());
    settings.setValue(QLatin1String("ItemsView/ExpandedGroups"), saveTreeState(treeGroups));

    QStringList defNames;
    foreach (Definition *definition, m_definitions)
        defNames << definition->systemName();
    settings.setValue(QLatin1String("ItemsView/SelectedItems"), defNames);

    if (treeGroups->selectionModel()->hasSelection()) {
        QModelIndex catIndex = treeGroups->selectionModel()->selectedIndexes().at(0);
        Category *category = m_groupsModel->fromIndex<Category *>(catIndex);
        settings.setValue(QLatin1String("ItemsView/SelectedGroup"), category ? category->systemName() : 0);
    }

    settings.setValue(QLatin1String("ItemsView/HideTiers"), m_tierFilter->hiddenTiers());
}

void ItemsView::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LanguageChange) {
        // add hidden tabs for retranslate to have effect on them
        foreach (QWidget *tab, m_tabs)
            if (tabWidget->indexOf(tab) == -1)
                tabWidget->addTab(tab, tab->windowTitle());

        retranslateUi(this);

        if (!m_definitions.isEmpty())
            setDefinitions(m_definitions, true);
    }
}

void ItemsView::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged) {
        QSettings settings;
        m_itemsModel->setShowTierIcons(settings.value(QLatin1String("ItemsView/TierIcons")).toBool());
        m_components->setSmallComponentIcons(settings.value(QLatin1String("ItemsView/SmallComponentIcons")).toBool());

        textDescription->document()->setDefaultStyleSheet(qApp->docStyleSheet());
        textDescription->setHtml(textDescription->toHtml());
    }
}

bool ItemsView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
        // Draw a joint between vertical and horizontal splitter.

        QSplitterHandle *handle = static_cast<QSplitterHandle *>(object);

        QColor lineColor(0x494645);
        QPainter painter(handle);

        QRect rect = splitterItems->handle(1)->rect();
        QPoint p(rect.right(), rect.height() / 2);
        p = splitterItems->handle(1)->mapTo(this, p);
        p = handle->mapFrom(this, p);
        painter.setPen(lineColor);
        painter.drawLine(p.x(), p.y(), p.x() + handle->width() / 2, p.y());

        if (stackInfo->currentIndex()) { // if info tabs are visible
            QPixmap tabBg(QLatin1String(":/tab-bg.png"));
            p = tabWidget->mapTo(this, QPoint(-handle->width() / 2, 0));
            p = handle->mapFrom(this, p);
            painter.drawPixmap(p, tabBg);
        }
    }
    return false;
}

void ItemsView::tierFilterChanged()
{
    buttonTiers->setProperty("role", m_tierFilter->isActive() ? "active" : "");
    buttonTiers->setStyleSheet(" ");
}

void ItemsView::on_splitterItems_splitterMoved(int, int index)
{
    if (index == 1)
        splitterInfo->handle(1)->update();
}

void ItemsView::groupSelectionChanged(const QItemSelection &selected)
{
    if (!selected.isEmpty()) {
        Category *category = m_groupsModel->fromIndex<Category *>(selected.indexes().at(0));
        m_itemsModel->setCategory(category);

        QModelIndex firstIndex = m_itemsModel->index(0, 0);
        if (firstIndex.isValid())
            listItems->selectionModel()->select(firstIndex, QItemSelectionModel::Select);
    }
}

void ItemsView::itemSelectionChanged()
{
    QMap<int, Definition *> oldDefs;
    QList<Definition *> newDefs;

    foreach (const QModelIndex &index, listItems->selectionModel()->selectedIndexes()) {
        Definition *definition = m_itemsModel->fromIndex<Definition *>(index);
        int pos = m_definitions.indexOf(definition);
        if (pos != -1)
            oldDefs.insert(pos, definition);
        else
            newDefs << definition;
    }

    setDefinitions(oldDefs.values() + newDefs);
}

void ItemsView::setTabVisible(QWidget *tab, bool visible)
{
    int actualIndex = tabWidget->indexOf(tab);

    if (visible && actualIndex == -1) {
        int index = 0;
        for (int i = 0; i < m_tabs.size(); ++i) {
            if (m_tabs.at(i) == tab)
                break;
            if (tabWidget->indexOf(m_tabs.at(i)) != -1)
                ++index;
        }
        tabWidget->insertTab(index, tab, tab->windowTitle());
    }
    else if (!visible && actualIndex != -1) {
        tabWidget->widget(actualIndex)->setWindowTitle(tabWidget->tabText(actualIndex));
        tabWidget->removeTab(actualIndex);
    }

}

void ItemsView::setDefinitions(QList<Definition *> definitions, bool retranslate)
{
    if (!retranslate && m_definitions == definitions)
        return;

    m_definitions = definitions.mid(0, 30); // limit to 30 items

    if (!retranslate) {
        m_parameters->setDefinitions(m_definitions);
        m_components->setDefinitions(m_definitions);
        m_bonuses->setDefinitions(m_definitions);
    }

    if (!m_definitions.isEmpty()) {
        Definition *definition = (m_definitions.size() == 1) ? m_definitions.at(0) : 0;
        if (definition) {
            labelName->setText(definition->name());
            textDescription->setText(definition->description());
            labelIcon->setVisible(true);
            labelIcon->setPixmap(definition->icon());

            if (definition->researchLevel()) {
                //: def_research_kit_1
                labelDecoderLevel->setText(tr("Level %1 decoder").arg(definition->researchLevel()));
            }

            if (!retranslate) {
                if (!definition->requirements().isEmpty()) {
                    m_requirements->setRequirements(definition->requirements());
                    treeRequirements->expandAll();
                }
                m_componentUse->setComponent(definition);
            }
        }
        else {
            labelName->setText(tr("%n items", 0, m_definitions.size()));
            QStringList names;
            foreach (Definition *item, m_definitions)
                names << item->name();
            textDescription->setText(QString("<p>%1</p>").arg(names.join(QLatin1String(", ")) + QLatin1Char('.')));

            labelIcon->setVisible(false);
        }

        setTabVisible(tabComponents, !m_components->isEmpty());
        setTabVisible(tabComponentUse, definition && definition->hasCategory("cf_material") && !m_componentUse->isEmpty());
        setTabVisible(tabRequirements, definition && !definition->requirements().isEmpty());
        setTabVisible(tabBonuses, !m_bonuses->isEmpty());

        widgetDecodingLevel->setVisible(definition && definition->researchLevel());

        treeParameters->setHeaderHidden(definition);
        treeParameters->expandToDepth(0);
    }

    stackInfo->setCurrentIndex(!m_definitions.isEmpty());
}

void ItemsView::tableDoubleClicked(const QModelIndex &index)
{
    QTreeView *treeView = static_cast<QTreeView *>(focusWidget());
    ComparisonModel *model = static_cast<ComparisonModel *>(treeView->model());
    model->toggleRowSort(index);
}

void ItemsView::tableContextMenuRequested(QPoint pos)
{
    QTreeView *treeView = qobject_cast<QTreeView *>(focusWidget());
    QModelIndex index = treeView->currentIndex();
    ComparisonModel *comparisonModel = qobject_cast<ComparisonModel *>(treeView->model());

    QMenu menu;
    // if (comparisonModel && comparisonModel->canSetReference(index))
    //     menu.addAction(actionSetReference);
    if (comparisonModel && comparisonModel->canSortRow(index))
        menu.addAction(actionSortRow);
    if (treeView->isSortingEnabled())
        menu.addAction(actionSortColumn);
    menu.addSeparator();
    menu.addAction(actionCopyToClipboard);
    menu.exec(treeView->viewport()->mapToGlobal(pos));
}

void ItemsView::on_actionSortRow_triggered()
{
    QTreeView *treeView = qobject_cast<QTreeView *>(focusWidget());

    ComparisonModel *model = static_cast<ComparisonModel *>(treeView->model());
    model->toggleRowSort(treeView->currentIndex());
}

void ItemsView::on_actionSortColumn_triggered()
{
    QTreeView *treeView = qobject_cast<QTreeView *>(focusWidget());
    int section = treeView->currentIndex().column();

    static_cast<ItemTableHeader *>(treeView->header())->toggleSort(section);
}

void ItemsView::on_actionCopyToClipboard_triggered()
{
    QTreeView *view = qobject_cast<QTreeView *>(focusWidget());
    if (view)
        QApplication::clipboard()->setText(modelToText(view->model()));
}

void ItemsView::on_actionSetReference_triggered()
{
    QTreeView *treeView = qobject_cast<QTreeView *>(focusWidget());
    ComparisonModel *comparisonModel = static_cast<ComparisonModel *>(treeView->model());
    comparisonModel->setReference(treeView->currentIndex());
}


// ItemTableHeader

ItemTableHeader::ItemTableHeader(int firstColumnWidth, int minColumnWidth, int maxColumnWidth, QWidget *parent)
    : QHeaderView(Qt::Horizontal, parent), m_firstColumnWidth(firstColumnWidth), m_minColumnWidth(minColumnWidth)
{
    setMinimumSectionSize(0);
    setDefaultSectionSize(maxColumnWidth);
}

void ItemTableHeader::updateGeometries()
{
    QHeaderView::updateGeometries();

    int colCount = count();
    if (colCount >= 2) {
        setResizeMode(Fixed);
        setResizeMode(0, Interactive);
        setResizeMode(colCount - 1, Stretch); // stretch placeholder

        int availWidth = viewport()->width() - sectionSize(0) - 3;
        int usedWidth = 0;

        for (int i = 1; i < colCount - 1; ++i) {
            int colWidth;
            QVariant vSizeHint = model()->headerData(i, orientation(), Qt::SizeHintRole);
            if (vSizeHint.isValid()) {
                colWidth = qBound(m_minColumnWidth, vSizeHint.value<QSize>().width(), availWidth - usedWidth);
            }
            else {
                colWidth = qMax(1, availWidth / (colCount - 2));
                colWidth += (i-1 < availWidth % colWidth); // distribute remainder
                colWidth = qBound(m_minColumnWidth, colWidth, defaultSectionSize());
            }
            usedWidth += colWidth;
            resizeSection(i, colWidth);
        }
        resizeSection(colCount - 1, qMax(0, availWidth - usedWidth));
    }
}

void ItemTableHeader::toggleSort(int section)
{
    Qt::SortOrder order = Qt::AscendingOrder;
    if (sortIndicatorSection() != section)
        order = (section == 0) ? Qt::AscendingOrder : Qt::DescendingOrder;
    else if ((section == 0) && sortIndicatorOrder() == Qt::DescendingOrder
            || (section != 0) && sortIndicatorOrder() == Qt::AscendingOrder)
        section = -1;
    else
        order = (sortIndicatorOrder() == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;

    setSortIndicator(section, order);
}

void ItemTableHeader::modelReset()
{
    setSortIndicator(-1, Qt::AscendingOrder); // XXX remove

    resizeSection(0, m_firstColumnWidth);
}

void ItemTableHeader::setModel(QAbstractItemModel *m)
{
    if (model())
        disconnect(model(), SIGNAL(modelReset()), this, SLOT(modelReset()));

    QHeaderView::setModel(m);

    if (model())
        connect(m, SIGNAL(modelReset()), this, SLOT(modelReset()));
}
