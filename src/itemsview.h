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

#ifndef ITEMSVIEW_H
#define ITEMSVIEW_H

#include "ui_itemsview.h"

class QSettings;
class QMenu;
class GameData;
class Definition;
class DefinitionListModel;
class MarketTreeModel;
class AggregatesModel;
class ComponentsModel;
class BonusesModel;
class RequirementsModel;
class DefinitionUseModel;


class ItemsView : public QWidget, private Ui::ItemsView
{
    Q_OBJECT

public:
    explicit ItemsView(QWidget *parent = 0);

    void initialize(QSettings &settings, GameData *gameData);
    void finalize(QSettings &settings);

protected:
    void changeEvent(QEvent *event);
    void customEvent(QEvent *event);
    bool eventFilter(QObject *, QEvent *);

private slots:
    void on_splitterItems_splitterMoved(int pos, int index);
    void on_actionSortColumn_triggered();
    void on_actionSortRow_triggered();
    void on_actionSetReference_triggered();
    void on_actionCopyToClipboard_triggered();

    void tableContextMenuRequested(QPoint pos);
    void itemSelectionChanged();
    void groupSelectionChanged(const QItemSelection &selected);

    void tableDoubleClicked(const QModelIndex &index);

private:
    GameData *m_gameData;

    MarketTreeModel *m_groupsModel;
    DefinitionListModel *m_itemsModel;

    AggregatesModel *m_parameters;
    ComponentsModel *m_components;
    RequirementsModel *m_requirements;
    BonusesModel *m_bonuses;
    DefinitionUseModel *m_componentUse;

    QList<QWidget *> m_tabs;
    QList<Definition *> m_definitions;

    void setDefinitions(QList<Definition *> items, bool retranslate = false);

    void setTabVisible(QWidget *tab, bool visible = true);
};


class ItemTableHeader : public QHeaderView {
    Q_OBJECT

public:
    ItemTableHeader(int firstColumnWidth, int minColumnWidth, int maxColumnWidth, QWidget *parent=0);

    void toggleSort(int section);

    void setModel(QAbstractItemModel *model);

protected slots:
    void updateGeometries();

private slots:
    void modelReset();

private:
    int m_firstColumnWidth, m_minColumnWidth;
};


#endif // ITEMSVIEW_H
