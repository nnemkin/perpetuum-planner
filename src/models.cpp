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
#include <QDateTime>

#include "models.h"
#include "gamedata.h"
#include "delegates.h"
#include "modeltest.h"

using namespace std;


// IndexCompare

class IndexCompare
{
public:
    IndexCompare(int role, Qt::SortOrder order) : m_role(role), m_order(order) {}

    inline bool operator()(const QModelIndex &left, const QModelIndex &right) const
    {
        return variantCompare(left.data(m_role), right.data(m_role)) ^ (m_order == Qt::DescendingOrder);
    }

private:
    inline bool variantCompare(const QVariant &l, const QVariant &r) const
    {
        switch (l.userType()) {
        case QVariant::Invalid:
            return (r.type() != QVariant::Invalid);
        case QVariant::Int:
            return l.toInt() < r.toInt();
        case QVariant::UInt:
            return l.toUInt() < r.toUInt();
        case QVariant::LongLong:
            return l.toLongLong() < r.toLongLong();
        case QVariant::ULongLong:
            return l.toULongLong() < r.toULongLong();
        case QMetaType::Float:
            return l.toFloat() < r.toFloat();
        case QVariant::Double:
            return l.toDouble() < r.toDouble();
        case QVariant::Char:
            return l.toChar() < r.toChar();
        case QVariant::Date:
            return l.toDate() < r.toDate();
        case QVariant::Time:
            return l.toTime() < r.toTime();
        case QVariant::DateTime:
            return l.toDateTime() < r.toDateTime();
        case QVariant::String:
        default:
            return l.toString().compare(r.toString(), Qt::CaseInsensitive) < 0;
        }
    }

    int m_role;
    Qt::SortOrder m_order;
};


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

QModelIndex SimpleTreeModel::objectIndex(QObject *object) const
{
    if (object)
        if (Node *node = findNode(object, rootNode()))
            return createIndex(node->row(), 0, node);
    return QModelIndex();
}

SimpleTreeModel::Node *SimpleTreeModel::findNode(QObject *object, Node *parent) const
{
    Q_ASSERT(object);

    foreach (Node *node, parent->displayChildren) {
        if (node->object == object)
            return node;
        node = findNode(object, node);
        if (node)
            return node;
    }
    return 0;
}

int SimpleTreeModel::rowCount(const QModelIndex &parent) const
{
    Node *node = 0;
    if (!parent.isValid())
        node = m_root;
    else if (parent.column() == 0)
        node = nodeFromIndex(parent);
    return node ? node->displayChildren.size() : 0;
}

QModelIndex SimpleTreeModel::index(int row, int column, const QModelIndex &parent) const
{
    Node *parentNode = parent.isValid() ? nodeFromIndex(parent) : m_root;
    if (parentNode && row >= 0 && column >= 0 && row < parentNode->displayChildren.size() && column < columnCount(parent))
        return createIndex(row, column, parentNode->displayChildren.at(row));
    return QModelIndex();
}

QModelIndex SimpleTreeModel::parent(const QModelIndex &child) const
{
    Node *node = nodeFromIndex(child);
    if (node && node->parent != m_root)
        return createIndex(node->parent->row(), 0, node->parent);
    return QModelIndex();
}

void SimpleTreeModel::beginResetModel()
{
    QAbstractItemModel::beginResetModel();
    delete m_root;
    m_root = new Node(0);
}

void SimpleTreeModel::endResetModel()
{
    applySortFilter(m_root);
    Q_ASSERT_MODEL(this);
    QAbstractItemModel::endResetModel();
}

void SimpleTreeModel::sort(int column, Qt::SortOrder order)
{
    if (m_sortColumn != column || m_rowSortOrder != order) {
        m_sortColumn = column;
        m_rowSortOrder = order;
        invalidateSortFilter();
    }
}

void SimpleTreeModel::invalidateSortFilter()
{
    emit layoutAboutToBeChanged();

    applySortFilter(m_root);

    QModelIndexList oldIndexes = persistentIndexList();
    QModelIndexList newIndexes;
    newIndexes.reserve(oldIndexes.size());

    int row = -1;
    Node *prevNode = 0;
    foreach (const QModelIndex &index, oldIndexes) {
        Node *node = nodeFromIndex(index);
        if (node != prevNode)
            row = node->row();
        if (row != -1)
            newIndexes << createIndex(row, index.column(), node);
        else
            newIndexes << QModelIndex();
    }
    changePersistentIndexList(oldIndexes, newIndexes);

    emit layoutChanged();
}

void SimpleTreeModel::setRowSort(int column, int role, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortRole = role;
    m_rowSortOrder = order;
}

void SimpleTreeModel::applySortFilter(Node *node)
{
    if (!node->children.isEmpty()) {
        int numChildren = node->children.size();

        QVector<QModelIndex> indexes;
        indexes.reserve(numChildren);

        for (int i = 0; i < numChildren; ++i) {
            Node *child = node->children.at(i);

            if (filterAcceptRow(child)) {
                applySortFilter(child);
                indexes << createIndex(i, m_sortColumn, child);
            }
        }

        if (m_sortColumn != -1)
            qStableSort(indexes.begin(), indexes.end(), IndexCompare(m_sortRole, m_rowSortOrder));

        if (m_sortColumn == -1 && indexes.size() == numChildren) {
            node->displayChildren = node->children;
        }
        else {
            numChildren = indexes.size();
            node->displayChildren.resize(numChildren);
            for (int i = 0; i < numChildren; ++i)
                node->displayChildren[i]  = nodeFromIndex(indexes.at(i));
        }
    }
}

bool SimpleTreeModel::event(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        invalidateSortFilter();

    return QAbstractItemModel::event(event);
}


// ItemGroupsModel

ItemGroupsModel::ItemGroupsModel(ObjectGroup *rootGroup, QObject *parent) : SimpleTreeModel(parent)
{
    beginResetModel();
    fillNode(rootNode(), rootGroup);
    endResetModel();
}

void ItemGroupsModel::fillNode(Node *node, ObjectGroup *group)
{
    foreach (ObjectGroup *childGroup, group->groups())
        fillNode(node->addChild(childGroup), childGroup);
}

QVariant ItemGroupsModel::data(const QModelIndex &index, int role) const
{
    if (ObjectGroup *group = fromIndex<ObjectGroup *>(index))
        if (role == Qt::DisplayRole)
            return QString("%1 (%2)").arg(group->name()).arg(group->allObjects().size());
    return QVariant();
}


// ItemsListModel

ItemsListModel::ItemsListModel(QObject *parent) : SimpleTreeModel(parent), m_group(0), m_hidePrototypes(false)
{
    setRowSort(0, Qt::DisplayRole);
}

QVariant ItemsListModel::data(const QModelIndex &index, int role) const
{
    if (GameObject *object = fromIndex<GameObject *>(index)) {
        if (role == Qt::DisplayRole) {
            int prodLevel = static_cast<Item *>(object)->parameter("Production Level").toInt();
            if (prodLevel)
                return QString("%1 [%2]").arg(object->name()).arg(prodLevel);
            return object->name();
        }
    }
    return QVariant();
}

bool ItemsListModel::filterAcceptRow(Node *node)
{
    GameObject *object = static_cast<GameObject *>(node->object);
    if (!object->name().contains(m_nameFilter, Qt::CaseInsensitive))
        return false;
    if (m_hidePrototypes && object->objectName().endsWith(QLatin1String("_pr")))
        return false;
    return true;
}

void ItemsListModel::setGroup(ObjectGroup *group)
{
    beginResetModel();
    m_group = group;
    if (group)
        foreach (GameObject *object, group->allObjects())
            rootNode()->addChild(object);
    endResetModel();
}


// ItemComparisonModel

QVariant ItemComparisonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            if (section == 0)
                return columnName(0);
            if (section == 1 && m_displayItems.size() == 1)
                return columnName(1);
            if (section > 0 && section <= m_displayItems.size())
                return m_displayItems.at(section - 1)->name();
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
    return 2 + m_displayItems.size();
}

Qt::ItemFlags ItemComparisonModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}

void ItemComparisonModel::setReference(const QModelIndex &idx)
{
    if (idx.column() > 0 && idx.column() <= m_displayItems.size())
        m_reference = m_displayItems.at(idx.column() - 1);
    else if (!idx.isValid())
        m_reference = 0;

    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void ItemComparisonModel::setItems(const QList<Item *> &items)
{
    m_items = m_displayItems = items;
}

Item *ItemComparisonModel::fromColumn(const QModelIndex &index) const
{
    if (index.column() > 0 && index.column() <= m_displayItems.size())
        return m_displayItems.at(index.column() - 1);
    return 0;
}

void ItemComparisonModel::invalidateColumnSort()
{
    emit layoutAboutToBeChanged();
    m_displayItems = m_items;
    if (m_columnSortBase.isValid()) {
        int numItems = m_items.size();

        QModelIndexList indexes;
        for (int i = 0; i < numItems; ++i)
            indexes << m_columnSortBase.sibling(m_columnSortBase.row(), i + 1);

        // NB: displayItems are used for data()
        qStableSort(indexes.begin(), indexes.end(), IndexCompare(sortRole(), m_columnSortOrder));

        m_displayItems.clear();
        m_displayItems.reserve(numItems);
        for (int i = 0; i < numItems; ++i)
            m_displayItems << m_items.at(indexes.at(i).column() - 1);
    }
    emit layoutChanged();
}

void ItemComparisonModel::toggleRowSort(const QModelIndex &index)
{
    if (canSortRow(index)) {
        if (m_columnSortBase == index) {
            if (m_columnSortOrder == Qt::AscendingOrder)
                m_columnSortOrder = Qt::DescendingOrder;
            else
                m_columnSortBase = QPersistentModelIndex();
        }
        else {
            m_columnSortBase = index;
            m_columnSortOrder = Qt::AscendingOrder;
        }
        invalidateColumnSort();
    }
}


// ParametersModel

bool ParametersModel::canSortRow(const QModelIndex &index) const
{
    return ItemComparisonModel::canSortRow(index) && fromIndex<Parameter *>(index);
}

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
    ItemComparisonModel::setItems(items);

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
                            groupNode = rootNode()->addChild(group);

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
}

QVariant ParametersModel::data(const QModelIndex &index, int role) const
{
    static QColor winnerFg(0x7dc47f), loserFg(0xd16e6e);

    if (index.column() == 0) {
        if (Parameter *parameter = fromIndex<Parameter *>(index)) {
            if (role == Qt::DisplayRole || role == SortKeyRole) {
                //if (m_items.size() == 1 || parameter->unit().isEmpty())
                    return parameter->name();
                //return QString("%1 (%2)").arg(parameter->name(), parameter->unit());
            }
            else if (role == SortableRowDelegate::SortOrderRole) {
                if (index == m_columnSortBase)
                    return m_columnSortOrder;
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
    else if (Item *item = fromColumn(index)) {
        if (role == Qt::DisplayRole || role == SortKeyRole || role == Qt::ForegroundRole) {
            if (Parameter *parameter = fromIndex<Parameter *>(index)) {
                QVariant value = item->parameters()->value(parameter);
                if (value.isValid()) {
                    if (role == Qt::DisplayRole) {
                        //if (m_items.size() == 1)
                            return parameter->format(value);
                        //return value.toString();
                    }
                    else if (role == SortKeyRole) {
                        return value;
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
    ItemComparisonModel::setItems(items);
    if (!m_items.isEmpty()) {
        GameData *gameData = m_items.at(0)->gameData();

        foreach (GameObject *object, gameData->bonuses()->objects()) {
            Parameter *parameter = static_cast<Parameter *>(object);

            foreach (Item *item, items) {
                if (item->bonusExtension() && item->parameters()->value(parameter).isValid()) {
                    rootNode()->addChild(parameter);
                    break;
                }
            }
        }
    }
    endResetModel();
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
    ItemComparisonModel::setItems(items);

    if (!items.isEmpty()) {
        QList<GameObject *> commonComponents = items.at(0)->gameData()->components()->allObjects();

        foreach (GameObject *component, commonComponents) {
            foreach (Item *item, items) {
                if (item->components()) {
                    if (item->components()->amount(static_cast<Item *>(component)) > 0) {
                        rootNode()->addChild(component);
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
                        rootNode()->addChild(component);
                    }
                }
            }
        }
    }
    endResetModel();
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
                if (m_columnSortBase == index)
                    return m_columnSortOrder;
            }
        }
        else if (Item *item = fromColumn(index)) {
            if (role == Qt::DisplayRole) {
                int amount = item->components() ? item->components()->amount(component) : 0;
                if (amount)
                    return amount;
            }
        }
    }
    return QVariant();
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
    fillNode(rootNode(), requirements);
    endResetModel();
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
    if (component) {
        foreach (GameObject *object, component->gameData()->items()->allObjects()) {
            Item *item = static_cast<Item *>(object);
            if (item->components() && item->components()->amount(m_component))
                rootNode()->addChild(item);
        }
    }
    endResetModel();
}

bool ComponentUseModel::filterAcceptRow(Node *node)
{
    Item *item = static_cast<Item *>(node->object);
    return !m_hidePrototypes || !item->objectName().endsWith(QLatin1String("_pr"));
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
    if (Item *item = fromIndex<Item *>(index)) {
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
