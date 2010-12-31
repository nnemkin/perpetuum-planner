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

#include <QDebug>
#include <QFont>
#include <QEvent>

#include "models.h"
#include "gamedata.h"
#include "delegates.h"
#include "modeltest.h"

using namespace std;


// SimpleTreeModel

SimpleTreeModel::Node *SimpleTreeModel::nodeFromIndex(const QModelIndex &index) const
{
    return static_cast<Node *>(index.internalPointer());
}

template<typename T>
T SimpleTreeModel::fromIndex(const QModelIndex &index) const
{
    Node *node = nodeFromIndex(index);
    return node ? qobject_cast<T>(node->object) : 0;
}

int SimpleTreeModel::rowCount(const QModelIndex &parent) const
{
    Node *node = 0;
    if (!parent.isValid())
        node = m_root;
    else if (parent.column() == 0)
        node = nodeFromIndex(parent);
    return node ? node->children.size() : 0;
}

QModelIndex SimpleTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    Node *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root;
    if (parentNode && row >= 0 && column >= 0 && row < parentNode->children.size() && column < columnCount(parent))
        return createIndex(row, column, parentNode->children.at(row));
    return QModelIndex();
}

QModelIndex SimpleTreeModel::parent(const QModelIndex &child) const
{
    Node *node = nodeFromIndex(child);
    if (node && node->parent != m_root)
        return createIndex(node->parent->row(), 0, node->parent);
    return QModelIndex();
}


// Predicate adapters

template<typename Pred>
class OrderAdapter : public binary_function<typename Pred::first_argument_type, typename Pred::second_argument_type, bool> {
public:
    OrderAdapter(Pred pred, bool invert) : m_pred(pred), m_invert(invert) {}

    inline bool operator()(typename Pred::first_argument_type a1, typename Pred::second_argument_type a2) {
        return m_pred(a1, a2) ^ m_invert;
    }

private:
    Pred m_pred;
    bool m_invert;
};

template<typename Pred>
inline OrderAdapter<Pred> orderAdapter(Pred pred, Qt::SortOrder order) {
    return OrderAdapter<Pred>(pred, order == Qt::DescendingOrder);
}

template<typename Less>
SimpleTreeModel::NodeObjectBinaryPredicate<Less> SimpleTreeModel::sortAdapter(Less less) {
    return NodeObjectBinaryPredicate<Less>(less);
}


// Predicates

static bool variantLessThan(const QVariant &v1, const QVariant &v2) {
    if (v1 == v2)
        return 0;
    if (!v1.isValid())
        return true;
    if (!v2.isValid())
        return false;

    bool ok = true;
    float f1 = v1.toFloat(&ok);
    if (ok) {
        float f2 = v2.toFloat(&ok);
        if (ok)
            return f1 < f2;
    }
    return v1.toString().compare(v2.toString(), Qt::CaseInsensitive) < 0;
}

class ObjectNameLessThan : public std::binary_function<GameObject *, GameObject *, bool> {
public:
    inline bool operator()(GameObject *object1, GameObject *object2) const {
        return object1->name() < object2->name();
    }
};

class ComponentAmountLessThan : public binary_function<Item *, Item *, bool> {
public:
    ComponentAmountLessThan(const ComponentSet *components) : m_components(components) {}

    inline bool operator()(Item *component1, Item *component2) const {
        return m_components->amount(component1) < m_components->amount(component2);
    }

private:
    const ComponentSet *m_components;
};

class ItemsParameterLessThan : public binary_function<Item *, Item *, bool> {
public:
    ItemsParameterLessThan(Parameter *parameter) : m_parameter(parameter) {}

    inline bool operator()(Item *item1, Item *item2) const {
        QVariant p1 = item1->parameters()->value(m_parameter);
        QVariant p2 = item2->parameters()->value(m_parameter);
        return variantLessThan(p1, p2);
    }

private:
    Parameter *m_parameter;
};

class ItemParametersLessThan : public binary_function<Parameter *, Parameter *, bool> {
public:
    ItemParametersLessThan(Item *item) : m_item(item) {}

    inline bool operator()(Parameter *p1, Parameter *p2) const {
        return variantLessThan(m_item->parameters()->value(p1), m_item->parameters()->value(p2));
    }

private:
    Item *m_item;
};

class ItemsComponentLessThan : public binary_function<Item *, Item *, bool> {
public:
    ItemsComponentLessThan(Item *component) : m_component(component) {}

    inline bool operator()(Item *item1, Item *item2) const {
        int a1 = (item1->components()) ? item1->components()->amount(m_component) : 0;
        int a2 = (item2->components()) ? item2->components()->amount(m_component) : 0;
        return a1 < a2;
    }

private:
    Item *m_component;
};


// ItemGroupsModel

ItemGroupsModel::ItemGroupsModel(ObjectGroup *rootGroup, QObject *parent)
    : QAbstractItemModel(parent), m_rootGroup(rootGroup)
{
    Q_ASSERT_MODEL(this);
}

QModelIndex ItemGroupsModel::index(int row, int column, const QModelIndex &parent) const
{
    if (column == 0 && row >= 0) {
        ObjectGroup *parentGroup = parent.isValid() ? groupFromIndex(parent) : m_rootGroup;
        if (parentGroup && row < parentGroup->groups().size())
            return createIndex(row, column, parentGroup->groups().at(row));
    }
    return QModelIndex();
}

QModelIndex ItemGroupsModel::parent(const QModelIndex &child) const
{
    ObjectGroup *group = groupFromIndex(child);
    if (group && group->parentGroup() != m_rootGroup) {
        group = group->parentGroup();
        return createIndex(group->parentGroup()->groups().indexOf(group), 0, group);
    }
    return QModelIndex();
}

int ItemGroupsModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 1;
}

int ItemGroupsModel::rowCount(const QModelIndex &parent) const
{
    ObjectGroup *group = groupFromIndex(parent);
    if (group || !parent.isValid())
        return (group ? group : m_rootGroup)->groups().size();
    return 0;
}

QVariant ItemGroupsModel::data(const QModelIndex &index, int role) const
{
    ObjectGroup *group = groupFromIndex(index);
    if (group && role == Qt::DisplayRole)
        return QString("%1 (%2)").arg(group->name()).arg(group->allObjects().size());
    return QVariant();
}

QModelIndex ItemGroupsModel::groupIndex(ObjectGroup *group) const
{
    if (group->parentGroup()) {
        int row = group->parentGroup()->groups().indexOf(group);
        if (row != -1)
            return createIndex(row, 0, group);
    }
    return QModelIndex();
}


// ItemsListModel

int ItemsListModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return m_gameObjects.size();
    return 0;
}

QVariant ItemsListModel::data(const QModelIndex &index, int role) const
{
    if (role == Qt::DisplayRole) {
        GameObject *object = objectFromIndex(index);
        if (object) {
            int prodLevel = static_cast<Item *>(object)->parameter("Production Level").toInt();
            if (prodLevel)
                return QString("%1 [%2]").arg(object->name()).arg(prodLevel);
            return object->name();
        }
    }
    return QVariant();
}

GameObject *ItemsListModel::objectFromIndex(const QModelIndex &index) const
{
    if (index.column() == 0 && index.row() >= 0 && index.row() < m_gameObjects.size())
        return m_gameObjects.at(index.row());
    return 0;
}

void ItemsListModel::updateObjects(bool reset)
{
    QList<GameObject *> newGameObjects;
    if (m_group) {
        if (!m_hidePrototypes && m_nameFilter.isEmpty()) {
            newGameObjects = m_group->allObjects();
        }
        else {
            foreach (GameObject *object, m_group->allObjects()) {
                if (!object->name().contains(m_nameFilter, Qt::CaseInsensitive))
                    continue;
                if (m_hidePrototypes && object->objectName().endsWith(QLatin1String("_pr")))
                    continue;
                newGameObjects << object;
            }
        }
        if (!m_logicalOrder)
            qStableSort(newGameObjects.begin(), newGameObjects.end(), ObjectNameLessThan());
    }

    if (reset) {
        beginResetModel();
        m_gameObjects = newGameObjects;
        endResetModel();
    }
    else {
        emit layoutAboutToBeChanged();

        QModelIndexList oldIndexes = persistentIndexList();
        QModelIndexList newIndexes;
        newIndexes.reserve(oldIndexes.size());
        foreach (const QModelIndex &index, oldIndexes) {
            int newRow = newGameObjects.indexOf(objectFromIndex(index));
            newIndexes << ((newRow == -1) ? QModelIndex() : createIndex(newRow, 0));
        }
        changePersistentIndexList(oldIndexes, newIndexes);

        m_gameObjects = newGameObjects;
        emit layoutChanged();
    }

    Q_ASSERT_MODEL(this);
}

QModelIndex ItemsListModel::objectIndex(GameObject *object) const
{
    return index(m_gameObjects.indexOf(object), 0);
}

bool ItemsListModel::event(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        updateObjects(false);

    return QAbstractListModel::event(event);
}


// ItemComparisonModel

QVariant ItemComparisonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            if (section == 0)
                return columnName(0);
            if (section == 1 && m_items.size() == 1)
                return columnName(1);
            if (section > 0 && section <= m_items.size())
                return m_items.at(section - 1)->name();
        }
        else if (role == Qt::TextAlignmentRole) {
            return (int) (Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

int ItemComparisonModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2 + m_items.size();
}

Qt::ItemFlags ItemComparisonModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}

void ItemComparisonModel::setReference(const QModelIndex &idx)
{
    if (idx.column() > 0 && idx.column() <= m_items.size())
        m_reference = m_items.at(idx.column() - 1);
    else if (!idx.isValid())
        m_reference = 0;

    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}


// ParametersModel

QString ParametersModel::columnName(int index) const
{
    if (index) {
        //: entityinfo_value
        return tr("Value");
    }
    //: entityinfo_parameters
    return tr("Parameters");
}

void ParametersModel::setItems(const QList<Item *> &items)
{
    beginResetModel();
    m_items = m_originalRowOrder = items;
    m_sortParameter = 0;

    Node *root = createRootNode();
    if (!m_items.isEmpty()) {
        GameData *gameData = items.at(0)->gameData();
        ObjectGroup *parameters = gameData->parameters();
        foreach (ObjectGroup *group, parameters->groups()) {
            Node *groupNode = 0;

            foreach (GameObject *object, group->objects()) {
                Parameter *parameter = static_cast<Parameter *>(object);
                Node *parameterNode = 0;
                float minValue = 1e8, maxValue = -1e8;

                foreach (Item *item, items) {
                    QVariant value = item->parameters()->value(parameter);
                    if (value.isValid() && !(item->bonusExtension() && gameData->bonuses()->objects().contains(parameter))) {
                        if (!groupNode)
                            groupNode = root->addChild(group);

                        if (!parameterNode) {
                            parameterNode = groupNode->addChild(parameter);

                            if (parameter->isMeta())
                                break;
                        }

                        bool ok = true;
                        float fValue = value.toFloat(&ok);
                        if (ok) {
                            if (fValue < minValue) minValue = fValue;
                            if (fValue > maxValue) maxValue = fValue;
                        }
                    }
                }

                if (parameterNode && minValue < maxValue) {
                    if (parameter->lessIsBetter())
                        qSwap(minValue, maxValue);
                    parameterNode->data << minValue << maxValue;
                }
            }
        }
    }
    endResetModel();

    Q_ASSERT_MODEL(this);
}

QVariant ParametersModel::data(const QModelIndex &index, int role) const
{
    static QColor winnerFg(0x7dc47f), loserFg(0xd16e6e);

    if (index.column() == 0) {
        if (Parameter *parameter = fromIndex<Parameter *>(index)) {
            if (role == Qt::DisplayRole) {
                //if (m_items.size() == 1 || parameter->unit().isEmpty())
                    return parameter->name();
                //return QString("%1 (%2)").arg(parameter->name(), parameter->unit());
            }
            else if (role == SortableRowDelegate::SortOrderRole) {
                if (parameter == m_sortParameter)
                    return m_sortOrder;
            }
        }
        else if (ObjectGroup *group = fromIndex<ObjectGroup *>(index)) {
            if (role == Qt::DisplayRole) {
                return group->name();
            }
            else if (role == Qt::FontRole) {
                QFont font;
                font.setBold(true);
                return font;
            }
        }
    }
    else if (index.column() > 0 && index.column() <= m_items.size()) {
        if (role == Qt::DisplayRole || role == Qt::ForegroundRole) {
            if (Parameter *parameter = fromIndex<Parameter *>(index)) {
                Item *item = m_items.at(index.column() - 1);
                QVariant value = item->parameters()->value(parameter);
                if (value.isValid()) {
                    if (role == Qt::DisplayRole) {
                        //if (m_items.size() == 1)
                            return parameter->format(value);
                        //return value.toString();
                    }
                    else {
                        const QVariantList &data = nodeFromIndex(index)->data;
                        if (!data.isEmpty()) {
                            if (value == data.at(0))
                                return loserFg;
                            if (value == data.at(1))
                                return winnerFg;
                        }
                    }
                }
            }
        }
    }
    return QVariant();
}

void ParametersModel::toggleRowSort(const QModelIndex &index)
{
    if (m_items.size() > 1) {
        if (Parameter *parameter = fromIndex<Parameter *>(index)) {
            Qt::SortOrder order = Qt::AscendingOrder;
            if (parameter == m_sortParameter) {
                if (m_sortOrder == Qt::DescendingOrder)
                    parameter = 0;
                else order = Qt::DescendingOrder;
            }

            emit layoutAboutToBeChanged();
            m_sortParameter = parameter;
            m_sortOrder = order;
            if (parameter)
                qStableSort(m_items.begin(), m_items.end(), orderAdapter(ItemsParameterLessThan(parameter), order));
            else
                m_items = m_originalRowOrder;
            emit layoutChanged();
        }
    }
}


// BonusesModel

QString BonusesModel::columnName(int index) const
{
    if (index) {
        //: entityinfo_bonus_extensionbonus
        return tr("Bonus");
    }
    //: entityinfo_bonus_aggregate
    return tr("Effect");
}

void BonusesModel::setItems(const QList<Item *> &items)
{
    beginResetModel();
    m_items = m_originalRowOrder = items;
    Node *root = createRootNode();
    if (!m_items.isEmpty()) {
        GameData *gameData = m_items.at(0)->gameData();

        foreach (GameObject *object, gameData->bonuses()->objects()) {
            Parameter *parameter = static_cast<Parameter *>(object);

            foreach (Item *item, items) {
                if (item->bonusExtension() && item->parameters()->value(parameter).isValid()) {
                    root->addChild(parameter);
                    break;
                }
            }
        }
    }
    m_originalOrder = rootNode()->children;
    endResetModel();

    Q_ASSERT_MODEL(this);
}

void BonusesModel::sort(int column, Qt::SortOrder order)
{
    if (column == -1) {
        emit layoutAboutToBeChanged();
        rootNode()->children = m_originalOrder;
        emit layoutChanged();
    }
    if (column == 0) {
        emit layoutAboutToBeChanged();
        qStableSort(rootNode()->children.begin(), rootNode()->children.end(), orderAdapter(sortAdapter(ObjectNameLessThan()), order));
        emit layoutChanged();
    }
    else if (column > 0 && column <= m_items.size()) {
        Item *item = m_items.at(column - 1);
        if (item->bonusExtension()) {
            emit layoutAboutToBeChanged();
            qStableSort(rootNode()->children.begin(), rootNode()->children.end(),
                        orderAdapter(sortAdapter(ItemParametersLessThan(item)), order));
            emit layoutChanged();
        }
    }

    Q_ASSERT_MODEL(this);
}


// ComponentsModel

QString ComponentsModel::columnName(int index) const
{
    if (index) {
        //: entityinfo_amount
        return tr("Amount");
    }
    //: entityinfo_components
    return tr("Components");
}

void ComponentsModel::setItems(const QList<Item *> &items)
{
    beginResetModel();
    m_items = m_originalRowOrder = items;
    m_sortComponent = 0;

    Node *root = createRootNode();
    if (!items.isEmpty()) {
        QList<GameObject *> commonComponents = items.at(0)->gameData()->components()->allObjects();

        foreach (GameObject *component, commonComponents) {
            foreach (Item *item, items) {
                if (item->components()) {
                    if (item->components()->amount(static_cast<Item *>(component)) > 0) {
                        root->addChild(component);
                        break;
                    }
                }
            }
        }
        foreach (Item *item, items) {
            if (item->components()) {
                foreach (Item *component, item->components()->components()) {
                    if (!commonComponents.contains(component)) {
                        commonComponents << component;
                        root->addChild(component);
                    }
                }
            }
        }
    }
    m_originalOrder = rootNode()->children;
    endResetModel();

    Q_ASSERT_MODEL(this);
}

QVariant ComponentsModel::data(const QModelIndex &index, int role) const
{
    if (Item *component = fromIndex<Item *>(index)) {
        if (index.column() == 0) {
            if (role == Qt::DisplayRole) {
                return component->name();
            }
            else if (role == Qt::DecorationRole) {
                return component->icon(m_smallComponentIcons ? 16 : 32);
            }
            else if (role == Qt::ForegroundRole) {
                return Qt::white;
            }
            else if (role == Qt::SizeHintRole) {
                if (!m_smallComponentIcons)
                    return QSize(0, 32);
            }
            else if (role == SortableRowDelegate::SortOrderRole) {
                if (component == m_sortComponent)
                    return m_sortOrder;
            }
        }
        else if (index.column() > 0 && index.column() <= m_items.size()) {
            if (role == Qt::DisplayRole) {
                const ComponentSet *components = m_items.at(index.column() - 1)->components();
                int amount = components ? components->amount(component) : 0;
                if (amount)
                    return amount;
            }
        }
    }
    return QVariant();
}

void ComponentsModel::sort(int column, Qt::SortOrder order)
{
    if (column == -1) {
        emit layoutAboutToBeChanged();
        rootNode()->children = m_originalOrder;
        emit layoutChanged();
    }
    if (column == 0) {
        emit layoutAboutToBeChanged();
        qStableSort(rootNode()->children.begin(), rootNode()->children.end(), orderAdapter(sortAdapter(ObjectNameLessThan()), order));
        emit layoutChanged();
    }
    else if (column > 0 && column <= m_items.size()) {
        Item *item = m_items.at(column - 1);
        if (item->components()) {
            emit layoutAboutToBeChanged();
            qStableSort(rootNode()->children.begin(), rootNode()->children.end(),
                        orderAdapter(sortAdapter(ComponentAmountLessThan(item->components())), order));
            emit layoutChanged();
        }
    }
}

void ComponentsModel::toggleRowSort(const QModelIndex &index)
{
    if (m_items.size() > 1) {
        if (Item *component = fromIndex<Item *>(index)) {
            Qt::SortOrder order = Qt::AscendingOrder;
            if (component == m_sortComponent) {
                if (m_sortOrder == Qt::DescendingOrder)
                    component = 0;
                else
                    order = Qt::DescendingOrder;
            }

            emit layoutAboutToBeChanged();
            m_sortComponent = component;
            m_sortOrder = order;
            if (component)
                qStableSort(m_items.begin(), m_items.end(), orderAdapter(ItemsComponentLessThan(m_sortComponent), order));
            else
                m_items = m_originalRowOrder;
            emit layoutChanged();
        }
    }
}

void ComponentsModel::setSmallComponentIcons(bool small)
{
    emit layoutAboutToBeChanged();
    m_smallComponentIcons = small;
    emit layoutChanged();
}


// RequirementsModel

void RequirementsModel::setRequirements(const ExtensionSet *requirements)
{
    beginResetModel();
    m_requirements = requirements;
    fillNode(createRootNode(), requirements);
    endResetModel();

    Q_ASSERT_MODEL(this);
}

void RequirementsModel::fillNode(Node *parentNode, const ExtensionSet *requirements)
{
    if (requirements) {
        foreach (Extension *extension, requirements->extensions())
            fillNode(parentNode->addChild(extension), extension->requirements());
    }
}

QVariant RequirementsModel::data(const QModelIndex &index, int role) const
{
    if (Node *node = nodeFromIndex(index)) {
        if (role == Qt::DisplayRole) {
            Extension *parent = static_cast<Extension *>(node->parent->object);
            Extension *extension = fromIndex<Extension *>(index);
            int level = parent ? parent->requirements()->level(extension) : m_requirements->level(extension);

            return QString("%1 / %2 (%3)")
                    .arg(extension->parentGroup()->name()).arg(extension->name()).arg(level);
        }
    }
    return QVariant();
}

Qt::ItemFlags RequirementsModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}


// ComponentUseModel

void ComponentUseModel::setComponent(Item *component)
{
    beginResetModel();
    m_component = component;
    m_usedIn.clear();

    if (component) {
        foreach (GameObject *object, component->gameData()->items()->allObjects()) {
            Item *item = static_cast<Item *>(object);
            if (item->components()
                    && item->components()->amount(m_component)
                    && (!m_hidePrototypes || !item->objectName().endsWith(QLatin1String("_pr"))))
                m_usedIn << item;
        }
    }
    m_originalOrder = m_usedIn;
    endResetModel();

    Q_ASSERT_MODEL(this);
}

int ComponentUseModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_usedIn.size();
}

int ComponentUseModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 3;
}

QVariant ComponentUseModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole) {
            switch (section) {
            case 0:
                return tr("Used in");
            case 1:
                //: entityinfo_amount
                return tr("Amount");
            }
        }
        else if (role == Qt::TextAlignmentRole) {
            return QVariant(Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

QVariant ComponentUseModel::data(const QModelIndex &index, int role) const
{
    if (index.isValid() && index.row() < m_usedIn.size()) {
        Item *item = m_usedIn.at(index.row());
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0:
                return item->name();
            case 1:
                return item->components()->amount(m_component);
            }
        }
    }
    return QVariant();
}

void ComponentUseModel::sort(int column, Qt::SortOrder order)
{
    emit layoutAboutToBeChanged();
    switch (column) {
    case -1:
        m_usedIn = m_originalOrder;
        break;
    case 0:
        qStableSort(m_usedIn.begin(), m_usedIn.end(), orderAdapter(ObjectNameLessThan(), order));
        break;
    case 1:
        qStableSort(m_usedIn.begin(), m_usedIn.end(), orderAdapter(ItemsComponentLessThan(m_component), order));
        break;
    }
    emit layoutChanged();
}
