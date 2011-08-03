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


class SimpleTreeModel : public QAbstractItemModel {
    Q_OBJECT

public:
    SimpleTreeModel(QObject *parent) : QAbstractItemModel(parent), m_root(new Node(0)) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent) return 1; }
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

    template<typename T>
    inline T fromIndex(const QModelIndex &index) const;

protected:
    struct Node {
        Node *parent;
        QObject *object;
        QVector<Node *> children;
        QVariantList data;

        Node(Node *aParent, QObject *aObject = 0) : parent(aParent), object(aObject) {}
        ~Node() { qDeleteAll(children); }

        inline int row() { return parent->children.indexOf(this); }
        inline Node *addChild(QObject *object) { Node *node = new Node(this, object); children << node; return node; }
    };

    Node *rootNode() const { return m_root; }
    Node *createRootNode(QObject *object = 0) { delete m_root; return m_root = new Node(0, object); }
    inline Node *nodeFromIndex(const QModelIndex &index) const;


    template<typename Pred>
    class NodeObjectBinaryPredicate : public std::binary_function<Node *, Node *, typename Pred::result_type> {
    public:
        NodeObjectBinaryPredicate(Pred pred) : m_pred(pred) {}

        inline typename Pred::result_type operator()(Node *n1, Node *n2) const {
            return m_pred(static_cast<typename Pred::first_argument_type>(n1->object),
                          static_cast<typename Pred::second_argument_type>(n2->object));
        }
    private:
        Pred m_pred;
    };

    template<typename Less> inline NodeObjectBinaryPredicate<Less> sortAdapter(Less less);

private:
    Node *m_root;
};


class ItemGroupsModel : public QAbstractItemModel {
    Q_OBJECT

public:
    ItemGroupsModel(ObjectGroup *rootGroup, QObject *parent);

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    inline ObjectGroup *groupFromIndex(const QModelIndex &index) const {
        return static_cast<ObjectGroup *>(index.internalPointer());
    }

    QModelIndex groupIndex(ObjectGroup *group) const;

private:
    ObjectGroup *m_rootGroup;
};


class ItemsListModel : public QAbstractListModel {
    Q_OBJECT

public:
    ItemsListModel(QObject *parent)
        : QAbstractListModel(parent), m_group(0), m_hidePrototypes(false), m_logicalOrder(false) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    ObjectGroup *group() const { return m_group; }
    void setGroup(ObjectGroup *group) { m_group = group; updateObjects(true); }

    GameObject *objectFromIndex(const QModelIndex &index) const;

    bool hidePrototypes() const { return m_hidePrototypes; }
    QString nameFilter() const { return m_nameFilter; }

    QModelIndex objectIndex(GameObject *object) const;

    bool event(QEvent *event);

public slots:
    void setHidePrototypes(bool hide) { m_hidePrototypes = hide; updateObjects(false); }
    void setLogicalOrder(bool logical) { m_logicalOrder = logical; updateObjects(false); }
    void setNameFilter(const QString& filter) { m_nameFilter = filter; updateObjects(false); }

private:
    ObjectGroup *m_group;
    QList<GameObject *> m_gameObjects;

    bool m_hidePrototypes;
    bool m_logicalOrder;
    QString m_nameFilter;

    void updateObjects(bool reset);
};


class ItemComparisonModel : public SimpleTreeModel {
    Q_OBJECT

public:
    ItemComparisonModel(QObject *parent) : SimpleTreeModel(parent), m_reference(0) {}

    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

    const QList<Item *> &items() const{ return m_items; }
    virtual void setItems(const QList<Item *> &items) = 0;

    virtual bool canSetReference(const QModelIndex &index) { return index.column() > 0 && m_items.size() > 1; }
    virtual void setReference(const QModelIndex &index);

    virtual bool canSortRow(const QModelIndex &index) const { Q_UNUSED(index); return m_items.size() > 1; }
    virtual void toggleRowSort(const QModelIndex &index) = 0;

    bool isEmpty() const { return m_items.isEmpty() || rootNode()->children.isEmpty(); }

protected:
    Qt::SortOrder m_sortOrder;
    QList<Item *> m_items;
    QList<Item *> m_originalRowOrder;
    Item *m_reference;

    virtual QString columnName(int index) const { Q_UNUSED(index); return QString(); }
};


class ParametersModel : public ItemComparisonModel {
    Q_OBJECT

public:
    ParametersModel(QObject *parent) : ItemComparisonModel(parent), m_sortParameter(0) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setItems(const QList<Item *> &items);

    bool canSortRow(const QModelIndex &index) const { return m_items.size() > 1 && fromIndex<Parameter *>(index); }
    void toggleRowSort(const QModelIndex &index);

protected:
    QString columnName(int index) const;

private:
    Parameter *m_sortParameter;
};


class ComponentsModel : public ItemComparisonModel {
    Q_OBJECT

public:
    ComponentsModel(QObject *parent) : ItemComparisonModel(parent), m_sortComponent(0), m_smallComponentIcons(false) {}

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void setItems(const QList<Item *> &items);

    void sort(int column, Qt::SortOrder order);
    void toggleRowSort(const QModelIndex &index);

    bool smallComponentIcons() const { return m_smallComponentIcons; }
    void setSmallComponentIcons(bool small);

protected:
    QString columnName(int index) const;

private:
    Item *m_sortComponent;
    QVector<Node *> m_originalOrder;

    bool m_smallComponentIcons;
};


class BonusesModel : public ParametersModel {
    Q_OBJECT

public:
    BonusesModel(QObject *parent)
        : ParametersModel(parent) {}

    void setItems(const QList<Item *> &items);

    void sort(int column, Qt::SortOrder order);

protected:
    QString columnName(int index) const;

private:
    QVector<Node *> m_originalOrder;
};


class RequirementsModel : public SimpleTreeModel {
    Q_OBJECT

public:
    RequirementsModel(QObject *parent) : SimpleTreeModel(parent) {}
    RequirementsModel(const ExtensionSet *requirements, QObject *parent=0): SimpleTreeModel(parent) {
        setRequirements(requirements);
    }

    void setRequirements(const ExtensionSet *requirements);

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    Qt::ItemFlags flags(const QModelIndex &index) const;

private:
    void fillNode(Node *parentNode, const ExtensionSet *requirements);

    const ExtensionSet *m_requirements;
};


class ComponentUseModel : public QAbstractTableModel {
    Q_OBJECT

public:
    ComponentUseModel(QObject *parent) : QAbstractTableModel(parent), m_component(0) {}

    int rowCount(const QModelIndex &parent = QModelIndex()) const;
    int columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;

    void sort(int column, Qt::SortOrder order);

    void setComponent(Item *component);

    bool isEmpty() const { return m_usedIn.isEmpty(); }

public slots:
    void setHidePrototypes(bool hide) { m_hidePrototypes = hide; setComponent(m_component); }

private:
    Item *m_component;
    QList<Item *> m_usedIn;
    QList<Item *> m_originalOrder;
    bool m_hidePrototypes;
};


#endif // MODELS_H
