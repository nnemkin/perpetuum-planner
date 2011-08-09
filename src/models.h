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

#ifndef MODELS_H
#define MODELS_H

#include <QAbstractItemModel>
#include <QVector>
#include <functional>

class GameData;
class GameObject;
class ObjectGroup;
class ExtensionSet;
class Item;
class Parameter;
class ParameterSet;


#include <QSortFilterProxyModel>

class SimpleTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum { SortKeyRole = Qt::UserRole + 100 };

    SimpleTreeModel(QObject *parent) : QAbstractItemModel(parent), m_root(new Node(0)),
        m_sortColumn(-1), m_rowSortOrder(Qt::AscendingOrder), m_sortRole(Qt::DisplayRole) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent) return 1; }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

    template<typename T>
    inline T fromIndex(const QModelIndex &index) const;

    void sort(int column, Qt::SortOrder order);

    int sortColumn() { return m_sortColumn; }
    int sortRole() { return m_sortRole; }
    int rowSortOrder() { return m_rowSortOrder; }

    QModelIndex objectIndex(QObject *object) const;

    bool event(QEvent *event);

protected:
    struct Node {
        Node *parent;
        QObject *object;
        QVector<Node *> children;
        QVector<Node *> displayChildren;
        QVariantList data;

        Node(Node *aParent, QObject *aObject = 0) : parent(aParent), object(aObject) {}
        ~Node() { qDeleteAll(children); }

        inline int row() { return parent->displayChildren.indexOf(this); }
        inline Node *addChild(QObject *object) { Node *node = new Node(this, object); children << node; return node; }
    };

    Node *rootNode() const { return m_root; }
    inline Node *nodeFromIndex(const QModelIndex &index) const;
    void beginResetModel();
    void endResetModel();

    void setRowSort(int column, int role, Qt::SortOrder order = Qt::AscendingOrder);
    void invalidateSortFilter();
    virtual bool filterAcceptRow(Node *node) { Q_UNUSED(node) return true; }

    Node *findNode(QObject *object, Node *parent) const;

private:
    Node *m_root;
    int m_sortRole, m_sortColumn;
    Qt::SortOrder m_rowSortOrder;

    void applySortFilter(Node *node);
};


class ItemGroupsModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ItemGroupsModel(ObjectGroup *rootGroup, QObject *parent);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:
    void fillNode(Node *node, ObjectGroup *group);
};


class ItemsListModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ItemsListModel(QObject *parent);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    ObjectGroup *group() const { return m_group; }
    void setGroup(ObjectGroup *group);

    bool hidePrototypes() const { return m_hidePrototypes; }
    QString nameFilter() const { return m_nameFilter; }

public slots:
    void setHidePrototypes(bool hide) { m_hidePrototypes = hide; invalidateSortFilter(); }
    void setLogicalOrder(bool logical) { setRowSort(logical ? -1 : 0, Qt::DisplayRole); invalidateSortFilter(); }
    void setNameFilter(const QString& filter) { m_nameFilter = filter; invalidateSortFilter(); }

protected:
    bool filterAcceptRow(Node *node);

private:
    ObjectGroup *m_group;

    bool m_hidePrototypes;
    QString m_nameFilter;
};


class ItemComparisonModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ItemComparisonModel(QObject *parent) : SimpleTreeModel(parent), m_reference(0) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    const QList<Item *> &items() const{ return m_displayItems; }
    virtual void setItems(const QList<Item *> &items);

    virtual bool canSetReference(const QModelIndex &index) { return index.column() > 0 && m_displayItems.size() > 1; }
    virtual void setReference(const QModelIndex &index);

    virtual bool canSortRow(const QModelIndex &index) const { Q_UNUSED(index); return m_displayItems.size() > 1; }
    virtual void toggleRowSort(const QModelIndex &index);

    bool isEmpty() const { return m_displayItems.isEmpty() || rootNode()->children.isEmpty(); }

protected:
    QList<Item *> m_items;
    Item *m_reference;

    Qt::SortOrder m_columnSortOrder;
    QPersistentModelIndex m_columnSortBase;

    inline Item *fromColumn(const QModelIndex &index) const;

    void invalidateColumnSort();

    virtual QString columnName(int index) const { Q_UNUSED(index); return QString(); }

private:
    QList<Item *> m_displayItems;
};


class ParametersModel : public ItemComparisonModel {
    Q_OBJECT

public:
    ParametersModel(QObject *parent) : ItemComparisonModel(parent) { setRowSort(0, SortKeyRole); }

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setItems(const QList<Item *> &items);
    bool canSortRow(const QModelIndex &index) const;

protected:
    QString columnName(int index) const;
};


class ComponentsModel : public ItemComparisonModel {
    Q_OBJECT

public:
    ComponentsModel(QObject *parent) : ItemComparisonModel(parent), m_smallComponentIcons(false) {}

    void setItems(const QList<Item *> &items);
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    bool smallComponentIcons() const { return m_smallComponentIcons; }
    void setSmallComponentIcons(bool small);

protected:
    bool m_smallComponentIcons;

    QString columnName(int index) const;
};


class BonusesModel : public ParametersModel {
    Q_OBJECT

public:
    BonusesModel(QObject *parent) : ParametersModel(parent) {}

    void setItems(const QList<Item *> &items);

protected:
    QString columnName(int index) const;
};


class RequirementsModel : public SimpleTreeModel {
    Q_OBJECT

public:
    RequirementsModel(QObject *parent) : SimpleTreeModel(parent) {}

    void setRequirements(const ExtensionSet *requirements);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

private:
    void fillNode(Node *parentNode, const ExtensionSet *requirements);

    const ExtensionSet *m_requirements;
};


class ComponentUseModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ComponentUseModel(QObject *parent) : SimpleTreeModel(parent), m_component(0) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setComponent(Item *component);

    bool isEmpty() const { return rootNode()->children.isEmpty(); }

public slots:
    void setHidePrototypes(bool hide) { m_hidePrototypes = hide; invalidateSortFilter(); }

protected:
    bool filterAcceptRow(Node *node);

private:
    Item *m_component;
    bool m_hidePrototypes;
};


#endif // MODELS_H
