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

#include "application.h"
#include "itemsview.h"
#include "gamedata.h"
#include "models.h"
#include "delegates.h"
#include "util.h"


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
}

void ItemsView::initialize(QSettings &settings, GameData *gameData)
{
    m_groupsModel = new ItemGroupsModel(gameData->items(), this);
    treeGroups->setModel(m_groupsModel);

    m_itemsModel = new ItemsListModel(this);
    listItems->setModel(m_itemsModel);

    bool hidePrototypes = settings.value(QLatin1String("ItemsView/HidePrototypes")).toBool();
    bool logicalOrder = settings.value(QLatin1String("ItemsView/LogicalOrder")).toBool();
    m_itemsModel->setHidePrototypes(hidePrototypes);
    m_itemsModel->setLogicalOrder(logicalOrder);
    checkHidePrototypes->setChecked(hidePrototypes);
    checkLogicalOrder->setChecked(logicalOrder);

    ForegroundFixDelegate *fgFixDelefate = new ForegroundFixDelegate(this);

    m_parameters = new ParametersModel(this);
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

    m_componentUse = new ComponentUseModel(this);
    m_componentUse->setHidePrototypes(hidePrototypes);
    treeComponentUse->setModel(m_componentUse);
    treeComponentUse->setHeader(new ItemTableHeader(220, 60, 140, treeComponentUse));

    ObjectGroup *selectedGroup = gameData->findObject<ObjectGroup *>(settings.value(QLatin1String("ItemsView/SelectedGroup")).toString());
    if (selectedGroup) {
        m_itemsModel->setGroup(selectedGroup);
        QModelIndex groupIndex = m_groupsModel->objectIndex(selectedGroup);
        if (groupIndex.isValid()) {
            treeGroups->selectionModel()->select(groupIndex, QItemSelectionModel::Select);

            QStringList itemIds = settings.value(QLatin1String("ItemsView/SelectedItems")).toStringList();
            QList<Item *> items;
            foreach (const QString &itemId, itemIds) {
                Item *item = gameData->findObject<Item *>(itemId);
                if (item)
                    items << item;
            }
            if (!items.isEmpty()) {
                foreach (Item *item, items)
                    listItems->selectionModel()->select(m_itemsModel->objectIndex(item), QItemSelectionModel::Select);
                setItems(items);
            }
        }
    }

    restoreTreeState(treeGroups, settings.value(QLatin1String("ItemsView/ExpandedGroups")).toBitArray());

    splitterInfo->restoreState(settings.value(QLatin1String("ItemsView/Splitter1")).toByteArray());
    splitterItems->restoreState(settings.value(QLatin1String("ItemsView/Splitter2")).toByteArray());
    stackInfo->setCurrentIndex(!m_items.isEmpty());

    tabWidget->setCurrentIndex(settings.value(QLatin1String("ItemsView/ActiveTab")).toInt());

    // NB: connect after the above initialization
    connect(treeGroups->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(groupSelectionChanged(QItemSelection)));
    connect(listItems->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
            this, SLOT(itemSelectionChanged()));
    connect(m_itemsModel, SIGNAL(modelAboutToBeReset()), listItems, SLOT(clearSelection()));
    connect(m_itemsModel, SIGNAL(layoutChanged()), this, SLOT(itemSelectionChanged()));

    connect(textFilter, SIGNAL(textChanged(QString)), m_itemsModel, SLOT(setNameFilter(QString)));
    connect(checkHidePrototypes, SIGNAL(clicked(bool)), m_itemsModel, SLOT(setHidePrototypes(bool)));
    connect(checkHidePrototypes, SIGNAL(clicked(bool)), m_componentUse, SLOT(setHidePrototypes(bool)));
    connect(checkLogicalOrder, SIGNAL(clicked(bool)), m_itemsModel, SLOT(setLogicalOrder(bool)));

    connect(treeParameters, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
    connect(treeComponents, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
    connect(treeBonuses, SIGNAL(customContextMenuRequested(QPoint)), this, SLOT(tableContextMenuRequested(QPoint)));
}

void ItemsView::finalize(QSettings &settings)
{
    settings.setValue(QLatin1String("ItemsView/HidePrototypes"), checkHidePrototypes->isChecked());
    settings.setValue(QLatin1String("ItemsView/LogicalOrder"), checkLogicalOrder->isChecked());
    settings.setValue(QLatin1String("ItemsView/Splitter1"), splitterInfo->saveState());
    settings.setValue(QLatin1String("ItemsView/Splitter2"), splitterItems->saveState());
    settings.setValue(QLatin1String("ItemsView/ActiveTab"), tabWidget->currentIndex());
    settings.setValue(QLatin1String("ItemsView/ExpandedGroups"), saveTreeState(treeGroups));

    QStringList itemIds;
    foreach (Item *item, m_items)
        itemIds << item->id();

    settings.setValue(QLatin1String("ItemsView/SelectedItems"), itemIds);
    if (treeGroups->selectionModel()->hasSelection()) {
        QModelIndex groupIndex = treeGroups->selectionModel()->selectedIndexes().at(0);
        ObjectGroup *group = m_groupsModel->fromIndex<ObjectGroup *>(groupIndex);
        settings.setValue(QLatin1String("ItemsView/SelectedGroup"), group ? group->id() : QString());
    }
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

        if (!m_items.isEmpty())
            setItems(m_items);
    }
}

void ItemsView::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged) {
        QSettings settings;
        m_components->setSmallComponentIcons(settings.value(QLatin1String("ItemsView/SmallComponentIcons")).toBool());

        textDescription->document()->setDefaultStyleSheet(qApp->docStyleSheet());
        textDescription->setHtml(textDescription->toHtml());
    }
}

bool ItemsView::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::Paint) {
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

void ItemsView::on_splitterItems_splitterMoved(int, int index)
{
    if (index == 1)
        splitterInfo->handle(1)->update();
}

void ItemsView::groupSelectionChanged(const QItemSelection &selected)
{
    if (!selected.isEmpty()) {
        ObjectGroup *group = m_groupsModel->fromIndex<ObjectGroup *>(selected.indexes().at(0));
        m_itemsModel->setGroup(group);

        QModelIndex firstIndex = m_itemsModel->index(0, 0);
        if (firstIndex.isValid())
            listItems->selectionModel()->select(firstIndex, QItemSelectionModel::Select);
    }
}

void ItemsView::itemSelectionChanged()
{
    QMap<int, Item *> oldItems;
    QList<Item *> newItems;

    foreach (const QModelIndex &index, listItems->selectionModel()->selectedIndexes()) {
        Item *item = m_itemsModel->fromIndex<Item *>(index);
        int pos = m_items.indexOf(item);
        if (pos != -1)
            oldItems.insert(pos, item);
        else
            newItems << item;
    }
    setItems(oldItems.values() + newItems);
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

void ItemsView::setItems(QList<Item *> items)
{
    m_items = items.mid(0, 30); // limit to 30 items

    m_parameters->setItems(m_items);
    m_components->setItems(m_items);
    m_bonuses->setItems(m_items);

    if (!m_items.isEmpty()) {
        Item *item = (m_items.size() == 1) ? m_items.at(0) : 0;
        if (item) {
            labelName->setText(item->name());
            textDescription->setText(item->description());
            labelIcon->setVisible(true);
            labelIcon->setPixmap(item->icon());

            if (item->bonusExtension()) {
                //: entityinfo_bonus_extension
                labelBonusExtension->setText(tr("Relevant extension: %1").arg(item->bonusExtension()->name()));
            }

            if (item->decoderLevel()) {
                //: def_research_kit_1
                labelDecoderLevel->setText(tr("Level %1 decoder").arg(item->decoderLevel()));
            }

            if (item->requirements()) {
                m_requirements->setRequirements(item->requirements());
                treeRequirements->expandAll();
            }

            m_componentUse->setComponent(item);
        }
        else {
            labelName->setText(tr("%n items", 0, m_items.size()));
            QStringList names;
            foreach (Item *item, m_items) {
                names << item->name();
            }
            textDescription->setText(names.join(QLatin1String(", ")) + QLatin1Char('.'));

            labelIcon->setVisible(false);
        }

        setTabVisible(tabComponents, !m_components->isEmpty());
        setTabVisible(tabComponentUse, item && !m_componentUse->isEmpty());
        setTabVisible(tabRequirements, item && item->requirements());
        setTabVisible(tabBonuses, !m_bonuses->isEmpty());

        widgetDecodingLevel->setVisible(item && item->decoderLevel());
        labelBonusExtension->setVisible(item && item->bonusExtension());

        treeParameters->setHeaderHidden(item);
        treeParameters->expandToDepth(0);
    }

    stackInfo->setCurrentIndex(!m_items.isEmpty());
}

void ItemsView::tableDoubleClicked(const QModelIndex &index)
{
    QTreeView *treeView = static_cast<QTreeView *>(focusWidget());
    ItemComparisonModel *model = static_cast<ItemComparisonModel *>(treeView->model());
    model->toggleRowSort(index);
}

void ItemsView::tableContextMenuRequested(QPoint pos)
{
    QTreeView *treeView = qobject_cast<QTreeView *>(focusWidget());
    QModelIndex index = treeView->currentIndex();
    ItemComparisonModel *comparisonModel = qobject_cast<ItemComparisonModel *>(treeView->model());

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

    ItemComparisonModel *model = static_cast<ItemComparisonModel *>(treeView->model());
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
    ItemComparisonModel *comparisonModel = static_cast<ItemComparisonModel *>(treeView->model());
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
        setResizeMode(colCount - 1, Stretch);
        resizeSection(0, m_firstColumnWidth);
        resizeSection(colCount - 1, 0);

        int availWidth = viewport()->width() - sectionSize(0) - 3;

        for (int i = 1; i < colCount - 1; ++i) {
            int colWidth = qMax(1, availWidth / (colCount - 2));
            colWidth += (i-1 < availWidth % colWidth); // distribute remainder
            colWidth = qBound(m_minColumnWidth, colWidth, defaultSectionSize());
            resizeSection(i, colWidth);
        }
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
    setSortIndicator(-1, Qt::AscendingOrder);
}

void ItemTableHeader::setModel(QAbstractItemModel *m)
{
    if (model())
        disconnect(model(), SIGNAL(modelReset()), this, SLOT(modelReset()));

    QHeaderView::setModel(m);

    if (model())
        connect(m, SIGNAL(modelReset()), this, SLOT(modelReset()));
}
