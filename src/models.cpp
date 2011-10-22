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

#include <QFont>
#include <QEvent>
#include <QDateTime>
#include <QSet>
#include <QDebug>

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
        QVariant l = left.data(m_role), r = right.data(m_role);
        if (m_order == Qt::AscendingOrder)
            return variantCompare(l, r);
        else
            return variantCompare(r, l);
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
            return l.toString().localeAwareCompare(r.toString()) < 0;
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
    m_sortColumn = -1;
    m_sortOrder = Qt::AscendingOrder;
}

void SimpleTreeModel::endResetModel()
{
    applySortFilter(m_root, m_sortColumn != -1);
    Q_ASSERT_MODEL(this);
    QAbstractItemModel::endResetModel();
}

void SimpleTreeModel::sort(int column, Qt::SortOrder order)
{
    if (m_sortColumn != column || m_sortOrder != order) {
        m_sortColumn = column;
        m_sortOrder = order;
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

void SimpleTreeModel::setSort(int column, int role, Qt::SortOrder order)
{
    m_sortColumn = column;
    m_sortRole = role;
    m_sortOrder = order;
}

void SimpleTreeModel::applySortFilter(Node *node, bool permanent)
{
    int numChildren = node->children.size();
    if (!numChildren)
        return;

    if (m_sortColumn == -1 && !m_filterEnabled) {
        node->displayChildren = node->children;
        foreach (Node *child, node->children)
            applySortFilter(child);
    }
    else {
        QVector<QModelIndex> indexes;
        indexes.reserve(numChildren);

        for (int i = 0; i < numChildren; ++i)
            indexes << createIndex(i, m_sortColumn, node->children.at(i));

        if (m_sortColumn != -1)
            qStableSort(indexes.begin(), indexes.end(), IndexCompare(m_sortRole, m_sortOrder));

        node->displayChildren.clear();
        node->displayChildren.reserve(numChildren);

        for (int i = 0; i < numChildren; ++i) {
            Node *child = nodeFromIndex(indexes.at(i));

            if (permanent)
                node->children[i] = child;

            if (!m_filterEnabled || filterAcceptRow(child)) {
                applySortFilter(child, permanent);
                node->displayChildren << child;
            }
        }
    }
}

bool SimpleTreeModel::event(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        invalidateSortFilter();

    return QAbstractItemModel::event(event);
}


// MarketTreeModel

MarketTreeModel::MarketTreeModel(Category *category, QObject *parent) : SimpleTreeModel(parent)
{
    beginResetModel();
    setSort(0, SortKeyRole);
    fillNode(rootNode(), category);
    endResetModel();
}

void MarketTreeModel::fillNode(Node *node, Category *category)
{
    foreach (Category *subcat, category->categories())
        if (subcat->inMarket() && !subcat->hidden())
            fillNode(node->addChild(subcat), subcat);
}

QVariant MarketTreeModel::data(const QModelIndex &index, int role) const
{
    if (Category *category = fromIndex<Category *>(index)) {
        if (role == Qt::DisplayRole)
            return QString("%1 (%2)").arg(category->name()).arg(category->marketCount());
        else if (role == SortKeyRole)
            return category->id();
    }
    return QVariant();
}


// DefinitionListModel

DefinitionListModel::DefinitionListModel(QObject *parent)
    : SimpleTreeModel(parent), m_category(0), m_hidePrototypes(false), m_logicalOrder(false), m_showTierIcons(true)
{
    setFilterEnabled(true);
}

void DefinitionListModel::setLogicalOrder(bool logical)
{
    if (m_logicalOrder != logical) {
        m_logicalOrder = logical;
        setSort(0, logical ? SortKeyRole : Qt::DisplayRole);
        invalidateSortFilter();
    }
}

QVariant DefinitionListModel::data(const QModelIndex &index, int role) const
{
    if (Definition *definition = fromIndex<Definition *>(index)) {
        Tier *tier = definition->isRobot() ? 0 : definition->tier();

        if (role == Qt::DisplayRole) {
            if (tier && !m_showTierIcons)
                return QString("%1 [%2]").arg(definition->name()).arg(tier->name());
            return definition->name();
        }
        else if (role == Qt::DecorationRole) {
            if (tier && m_showTierIcons)
                return tier->icon();
        }
        else if (role == SortKeyRole) {
            return definition->category()->order() + (tier ? tier->id() : definition->id());
        }
    }
    return QVariant();
}

bool DefinitionListModel::filterAcceptRow(Node *node)
{
    Definition *definition = static_cast<Definition *>(node->object);
    if (!definition->name().contains(m_nameFilter, Qt::CaseInsensitive))
        return false;
    if (m_hidePrototypes && definition->isPrototype())
        return false;
    return true;
}

void DefinitionListModel::setCategory(Category *category)
{
    beginResetModel();
    if (category) {
        setSort(0, m_logicalOrder ? SortKeyRole : Qt::DisplayRole);

        foreach (Definition *definition, category->definitions())
            if (definition->inMarket())
                rootNode()->addChild(definition);
    }
    endResetModel();
}

void DefinitionListModel::setShowTierIcons(bool show)
{
    m_showTierIcons = show;
    emit dataChanged(index(0, 0), index(rowCount() - 1, 0));
}


// ComparisonModel

QVariant ComparisonModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            if (section == 0 || m_definitions.size() == 1 && section < columnCount() - 1)
                return columnName(section);
            if (section > 0 && section <= m_displayDefs.size())
                return m_displayDefs.at(section - 1)->name();
        }
        else if (role == Qt::TextAlignmentRole) {
            return (int) (Qt::AlignLeft | Qt::AlignVCenter);
        }
    }
    return QVariant();
}

QVariant ComparisonModel::data(const QModelIndex &index, int role) const
{
    // provide row sort order information for delegate to draw an arrow
    if (index.column() == 0 && role == SortableRowDelegate::SortOrderRole) {
        if (index == m_columnSortBase)
            return m_columnSortOrder;
    }
    return QVariant();
}

int ComparisonModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 2 + m_displayDefs.size();
}

Qt::ItemFlags ComparisonModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}

void ComparisonModel::setReference(const QModelIndex &idx)
{
    if (idx.column() > 0 && idx.column() <= m_displayDefs.size())
        m_reference = m_displayDefs.at(idx.column() - 1);
    else if (!idx.isValid())
        m_reference = 0;

    emit dataChanged(index(0, 0), index(rowCount() - 1, columnCount() - 1));
}

void ComparisonModel::setDefinitions(const QList<Definition *> &items)
{
    m_definitions = m_displayDefs = items;
}

Definition *ComparisonModel::fromColumn(const QModelIndex &index) const
{
    if (index.column() > 0 && index.column() <= m_displayDefs.size())
        return m_displayDefs.at(index.column() - 1);
    return 0;
}

void ComparisonModel::invalidateColumnSort()
{
    emit layoutAboutToBeChanged();
    m_displayDefs = m_definitions;
    if (m_columnSortBase.isValid()) {
        int numItems = m_definitions.size();

        QModelIndexList indexes;
        for (int i = 0; i < numItems; ++i)
            indexes << m_columnSortBase.sibling(m_columnSortBase.row(), i + 1);

        // NB: displayItems are used for data()
        qStableSort(indexes.begin(), indexes.end(), IndexCompare(sortRole(), m_columnSortOrder));

        m_displayDefs.clear();
        m_displayDefs.reserve(numItems);
        for (int i = 0; i < numItems; ++i)
            m_displayDefs << m_definitions.at(indexes.at(i).column() - 1);
    }
    emit layoutChanged();
}

void ComparisonModel::toggleRowSort(const QModelIndex &index)
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


// AggregatesModel

bool AggregatesModel::canSortRow(const QModelIndex &index) const
{
    return ComparisonModel::canSortRow(index) && fromIndex<AggregateField *>(index);
}

QString AggregatesModel::columnName(int index) const
{
    if (index) {
        //: entityinfo_value
        return tr("Value");
    }
    //: entityinfo_parameters
    return tr("Parameters");
}

void AggregatesModel::setDefinitions(const QList<Definition *> &definitions)
{
    beginResetModel();
    setSort(0, SortKeyRole);
    ComparisonModel::setDefinitions(definitions);

    if (!definitions.isEmpty()) {
        QHash<AggregateField *, Node *> aggregateNodes;
        QHash<FieldCategory *, Node *> categoryNodes;

        foreach (Definition *definition, definitions) {
            foreach (AggregateField *aggregate, definition->aggregates().keys()) {
                if (aggregate->hidden())
                    continue;

                Node *aggregateNode = aggregateNodes.value(aggregate);
                if (!aggregateNode) {
                    FieldCategory *category = aggregate->category();
                    Node *categoryNode = categoryNodes.value(category);
                    if (!categoryNode) {
                        categoryNode = rootNode()->addChild(category);
                        categoryNodes.insert(category, categoryNode);
                    }

                    aggregateNode = categoryNode->addChild(aggregate);
                    aggregateNode->data << QVariant(1e30f) << QVariant(-1e30f);
                    aggregateNodes.insert(aggregate, aggregateNode);
                }

                bool ok;
                float value = definition->aggregates().value(aggregate).toFloat(&ok);
                if (ok) {
                    if (value < aggregateNode->data.at(0).toFloat())
                        aggregateNode->data[0] = value;
                    if (value > aggregateNode->data.at(1).toFloat())
                        aggregateNode->data[1] = value;
                }
            }
        }

        foreach (Node *aggregateNode, aggregateNodes) {
            if (aggregateNode->data.at(0) == aggregateNode->data.at(1))
                aggregateNode->data.clear();
            else if (static_cast<AggregateField *>(aggregateNode->object)->lessIsBetter())
                qSwap(aggregateNode->data[0], aggregateNode->data[1]);
        }
    }
    endResetModel();
}

QVariant AggregatesModel::data(const QModelIndex &index, int role) const
{
    static QColor winnerFg(0x7dc47f), loserFg(0xd16e6e);

    if (index.column() == 0) {
        if (AggregateField *aggregate = fromIndex<AggregateField *>(index)) {
            if (role == Qt::DisplayRole) {
                //if (m_items.size() == 1 || parameter->unit().isEmpty())
                    return aggregate->name();
                //return QString("%1 (%2)").arg(parameter->name(), parameter->unit());
            }
            else if (role == SortKeyRole) {
                 return aggregate->isAggregate() ? aggregate->name() : aggregate->systemName();
            }
        }
        else if (FieldCategory *category = fromIndex<FieldCategory *>(index)) {
            if (role == Qt::DisplayRole) {
                return category->name();
            }
            else if (role == SortKeyRole) {
                return category->id();
            }
            else if (role == Qt::FontRole) {
                QFont font;
                font.setBold(true);
                return font;
            }
        }
    }
    else if (Definition *definition = fromColumn(index)) {
        if (role == Qt::DisplayRole || role == SortKeyRole || role == Qt::ForegroundRole) {
            if (AggregateField *aggregate = fromIndex<AggregateField *>(index)) {
                QVariant value = definition->aggregates().value(aggregate);
                if (value.isValid()) {
                    if (role == Qt::DisplayRole) {
                        //if (m_items.size() == 1)
                            return aggregate->format(value);
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
    return ComparisonModel::data(index, role);
}


// BonusesModel

void BonusesModel::setDefinitions(const QList<Definition *> &definitions)
{
    beginResetModel();
    ComparisonModel::setDefinitions(definitions);

    QHash<AggregateField *, Node *> m_bonusNodes;

    foreach (Definition *definition, definitions) {
        foreach (Bonus *bonus, definition->bonuses()) {
            Node *bonusNode = m_bonusNodes.value(bonus->aggregate());
            if (!bonusNode) {
                bonusNode = rootNode()->addChild(bonus->aggregate());
                m_bonusNodes.insert(bonus->aggregate(), bonusNode);
            }
        }
    }
    endResetModel();
}

int BonusesModel::columnCount(const QModelIndex &parent) const
{
    if (m_definitions.size() == 1)
        return 4; // adds "Relevant extension" column
    return ComparisonModel::columnCount(parent);
}

QVariant BonusesModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal) {
        if (role == Qt::DisplayRole || role == Qt::ToolTipRole) {
            if (section == 0)
                //: entityinfo_bonus_aggregate
                return tr("Effect");
            else if (m_definitions.size() == 1)
                switch (section) {
                case 1:
                    //: entityinfo_bonus_extensionbonus
                    return tr("Bonus");
                case 2:
                    //: entityinfo_bonus_extensionname
                    return tr("Relevant extension");
                }
        }
        else if (role == Qt::SizeHintRole) {
            if (section == 2 && m_definitions.size() == 1)
                return QSize(200, 0);
        }
    }
    return ComparisonModel::headerData(section, orientation, role);
}

QVariant BonusesModel::data(const QModelIndex &index, int role) const
{
    if (AggregateField *aggregate = fromIndex<AggregateField *>(index)) {
        if (index.column() == 0) {
            if (role == Qt::DisplayRole || role == SortKeyRole)
                return aggregate->name();
        }
        else if (Definition *definition = fromColumn(index)) {
            if (role == Qt::DisplayRole || role == SortKeyRole) {
                Bonus *bonus = definition->bonuses().value(aggregate);
                if (bonus) {
                    if (role == Qt::DisplayRole)
                        return bonus->format();
                    else if (role == SortKeyRole)
                        return bonus->bonus();
                }
            }
        }
        else if (m_definitions.size() == 1 && index.column() == 2) {
            // 3rd column in single-item display is "Relevant extension"
            Definition *definition = m_definitions.at(0);
            Bonus *bonus = definition->bonuses().value(aggregate);
            if (bonus) {
                if (role == Qt::DisplayRole || role == SortKeyRole)
                    return bonus->extension()->name();
            }
        }
    }
    return ComparisonModel::data(index, role);
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

void ComponentsModel::setDefinitions(const QList<Definition *> &definitions)
{
    beginResetModel();
    setSort(0, SortKeyRole, Qt::DescendingOrder);
    ComparisonModel::setDefinitions(definitions);

    if (!definitions.isEmpty()) {
        QHash<Definition *, Node *> componentNodes;

        foreach (Definition *definition, definitions) {
            foreach (Definition *component, definition->components().keys()) {
                int amount = definition->components().value(component);

                Node *node = componentNodes.value(component);
                if (!node) {
                    node = rootNode()->addChild(component);
                    node->data << amount;
                    componentNodes.insert(component, node);
                }
                else {
                    node->data[0] = qMax(node->data.at(0).toInt(), amount);
                }
            }
        }
    }
    endResetModel();

    setSort(-1, Qt::DisplayRole); // role for header-triggered sorts
}

QVariant ComponentsModel::data(const QModelIndex &index, int role) const
{
    if (Definition *component = fromIndex<Definition *>(index)) {
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
            else if (role == SortKeyRole) {
                return nodeFromIndex(index)->data.at(0);
            }
        }
        else if (Definition *definition = fromColumn(index)) {
            if (role == Qt::DisplayRole) {
                int amount = definition->components().value(component);
                if (amount)
                    return amount;
            }
        }
    }
    return ComparisonModel::data(index, role);
}

void ComponentsModel::setSmallComponentIcons(bool small)
{
    emit layoutAboutToBeChanged();
    m_smallComponentIcons = small;
    emit layoutChanged();
}


// RequirementsModel

void RequirementsModel::setRequirements(const ExtensionLevelMap &requirements)
{
    beginResetModel();
    m_requirements = requirements;
    fillNode(rootNode(), requirements);
    endResetModel();
}

void RequirementsModel::fillNode(Node *parentNode, const ExtensionLevelMap &requirements)
{
    foreach (Extension *extension, requirements.extensions())
        fillNode(parentNode->addChild(extension), extension->requirements());
}

QVariant RequirementsModel::data(const QModelIndex &index, int role) const
{
    if (Node *node = nodeFromIndex(index)) {
        if (role == Qt::DisplayRole) {
            Extension *parent = static_cast<Extension *>(node->parent->object);
            Extension *extension = fromIndex<Extension *>(index);
            int level = parent ? parent->requirements().value(extension) : m_requirements.value(extension);

            return QString("%1 / %2 (%3)")
                    .arg(extension->category()->name()).arg(extension->name()).arg(level);
        }
    }
    return QVariant();
}

Qt::ItemFlags RequirementsModel::flags(const QModelIndex &index) const
{
    return index.isValid() ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : 0;
}


// DefinitionUseModel

DefinitionUseModel::DefinitionUseModel(QObject *parent) : SimpleTreeModel(parent), m_component(0)
{
    setFilterEnabled(true);
}

void DefinitionUseModel::setComponent(Definition *component)
{
    beginResetModel();
    m_component = component;
    if (component) {
        foreach (Definition *definition, component->gameData()->definitions()) {
            if (definition->inMarket())
                if (definition->components().value(m_component))
                    rootNode()->addChild(definition);
        }
    }
    endResetModel();
}

bool DefinitionUseModel::filterAcceptRow(Node *node)
{
    Definition *item = static_cast<Definition *>(node->object);
    return !m_hidePrototypes || !item->objectName().endsWith(QLatin1String("_pr"));
}

int DefinitionUseModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent)
    return 3;
}

QVariant DefinitionUseModel::headerData(int section, Qt::Orientation orientation, int role) const
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

QVariant DefinitionUseModel::data(const QModelIndex &index, int role) const
{
    if (Definition *definition = fromIndex<Definition *>(index)) {
        if (role == Qt::DisplayRole) {
            switch (index.column()) {
            case 0:
                return definition->name();
            case 1:
                return definition->components().value(m_component);
            }
        }
    }
    return QVariant();
}
