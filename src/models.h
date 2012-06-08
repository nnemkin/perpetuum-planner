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

#include "gamedata.h"


class SimpleTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    enum { SortKeyRole = Qt::UserRole + 100 };

    SimpleTreeModel(QObject *parent) : QAbstractItemModel(parent), m_root(new Node(0)), m_filterEnabled(false),
        m_sortColumn(-1), m_sortRole(Qt::DisplayRole), m_sortOrder(Qt::AscendingOrder) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent) return 1; }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

    template<typename T>
    inline T fromIndex(const QModelIndex &index) const
    {
        Node *node = nodeFromIndex(index);
        return node ? qobject_cast<T>(node->object) : 0;
    }

    void sort(int column, Qt::SortOrder order);

    int sortColumn() const { return m_sortColumn; }
    int sortRole() const { return m_sortRole; }
    int sortOrder() const { return m_sortOrder; }

    QModelIndex objectIndex(QObject *object) const;

    bool isEmpty() const { return rootNode()->displayChildren.isEmpty(); }

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

    void setSort(int column, int role, Qt::SortOrder order = Qt::AscendingOrder);
    void invalidateSortFilter();

    void setFilterEnabled(bool enabled) { m_filterEnabled = enabled; }
    virtual bool filterAcceptRow(Node *node) { Q_UNUSED(node) return true; }

    Node *findNode(QObject *object, Node *parent) const;

private:
    Node *m_root;
    int m_sortRole, m_sortColumn;
    Qt::SortOrder m_sortOrder;
    bool m_filterEnabled;

    void applySortFilter(Node *node, bool permanent = false);
};


class MarketTreeModel : public SimpleTreeModel {
    Q_OBJECT

public:
    MarketTreeModel(Category *category, QObject *parent);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

private:
    void fillNode(Node *node, Category *category);
};


class DefinitionListModel : public SimpleTreeModel {
    Q_OBJECT

public:
    DefinitionListModel(QObject *parent);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setCategory(Category *category);

    QString nameFilter() const { return m_nameFilter; }
    bool showTierIcons() const { return m_showTierIcons; }

    void setShowTierIcons(bool show);

public slots:
    void setAlphabeticalOrder(bool alpha);
    void setNameFilter(const QString& filter) { m_nameFilter = filter; invalidateSortFilter(); }
    void setHiddenTiers(const QStringList &tiers) { m_hiddenTiers = tiers; invalidateSortFilter(); }

protected:
    bool filterAcceptRow(Node *node);

private:
    Category *m_category;

    QStringList m_hiddenTiers;
    bool m_alphabeticalOrder;
    bool m_showTierIcons;
    QString m_nameFilter;
};


class ComparisonModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ComparisonModel(QObject *parent) : SimpleTreeModel(parent), m_reference(0) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    const QList<Definition *> &definitions() const{ return m_displayDefs; }
    virtual void setDefinitions(const QList<Definition *> &definitions);

    virtual bool canSetReference(const QModelIndex &index) { return index.column() > 0 && m_displayDefs.size() > 1; }
    virtual void setReference(const QModelIndex &index);

    virtual bool canSortRow(const QModelIndex &index) const { Q_UNUSED(index); return m_displayDefs.size() > 1; }
    virtual void toggleRowSort(const QModelIndex &index);

    bool isEmpty() const { return SimpleTreeModel::isEmpty() || m_displayDefs.isEmpty(); }

protected:
    QList<Definition *> m_definitions;
    Definition *m_reference;

    Qt::SortOrder m_columnSortOrder;
    QPersistentModelIndex m_columnSortBase;

    inline Definition *fromColumn(const QModelIndex &index) const;

    void invalidateColumnSort();

    virtual QString columnName(int index) const { Q_UNUSED(index); return QString(); }

private:
    QList<Definition *> m_displayDefs;
};


class AggregatesModel : public ComparisonModel {
    Q_OBJECT

public:
    AggregatesModel(QObject *parent) : ComparisonModel(parent) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setDefinitions(const QList<Definition *> &definitions);
    bool canSortRow(const QModelIndex &index) const;

protected:
    QString columnName(int index) const;
};


class ComponentsModel : public ComparisonModel {
    Q_OBJECT

public:
    ComponentsModel(QObject *parent) : ComparisonModel(parent), m_smallComponentIcons(false) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setDefinitions(const QList<Definition *> &definitions);

    bool smallComponentIcons() const { return m_smallComponentIcons; }
    void setSmallComponentIcons(bool small);

protected:
    bool m_smallComponentIcons;

    QString columnName(int index) const;
};


class BonusesModel : public ComparisonModel {
    Q_OBJECT

public:
    BonusesModel(QObject *parent) : ComparisonModel(parent) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setDefinitions(const QList<Definition *> &definitions);
};


class RequirementsModel : public SimpleTreeModel {
    Q_OBJECT

public:
    RequirementsModel(QObject *parent) : SimpleTreeModel(parent) {}

    void setRequirements(const ExtensionLevelMap &requirements);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

private:
    void fillNode(Node *parentNode, const ExtensionLevelMap &requirements);

    ExtensionLevelMap m_requirements;
};


class DefinitionUseModel : public SimpleTreeModel {
    Q_OBJECT

public:
    DefinitionUseModel(QObject *parent);

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setComponent(Definition *component);

private:
    Definition *m_component;
};


#endif // MODELS_H
