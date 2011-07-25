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
#include <QBitmap>
#include <QPixmapCache>

#include "gamedata.h"


// AttributeSet

AttributeSet::AttributeSet()
{
    qMemSet(m_attributes, 0, sizeof(m_attributes));
}

AttributeSet::AttributeSet(QVariant data)
{
    QVariantList dataList = data.toList();
    for (int i = 0; i < NumAttributes; ++i)
        m_attributes[i] = dataList.at(i).toInt();
}

AttributeSet::AttributeSet(const AttributeSet& other)
{
    qMemCopy(m_attributes, other.m_attributes, sizeof(m_attributes));
}

AttributeSet::operator QVariant() const
{
    QVariantList dataList;
    for (int i = 0; i < NumAttributes; ++i)
        dataList << m_attributes[i];
    return dataList;
}

AttributeSet AttributeSet::operator+(const AttributeSet& other) const
{
    AttributeSet sum(other);
    for (int i = 0; i < NumAttributes; ++i)
        sum.m_attributes[i] += m_attributes[i];
    return sum;
}


// GameObject

void GameObject::load(const QVariantMap &dataMap)
{
    m_descToken = dataMap.value("desc").toString();
    if (m_descToken.isEmpty())
        m_descToken = objectName() + QLatin1String("_desc");
}


// Extension

void Extension::load(const QVariantMap &dataMap)
{
    GameObject::load(dataMap);

    m_complexity = dataMap.value("complexity").toInt();
    QString names = QLatin1String("TMHRPE");

    QString attributes = dataMap.value("attributes").toString();
    m_primaryAttribute = static_cast<Attribute>(names.indexOf(attributes.at(0)));
    m_secondaryAttribute = static_cast<Attribute>(names.indexOf(attributes.at(1)));

    m_requirements.load(gameData(), dataMap.value("requirements").toMap());
}

const ExtensionSet *Extension::transitiveReqs() const
{
    if (!m_requirements.isEmpty() && m_transitiveReqs.isEmpty()) {
        m_transitiveReqs = m_requirements;
        for (ExtensionSet::const_iterator i = m_requirements.constBegin(); i != m_requirements.constEnd(); ++i)
            if (i.key()->transitiveReqs())
                m_transitiveReqs += *i.key()->transitiveReqs();
    }
    return m_transitiveReqs.isEmpty() ? 0 : &m_transitiveReqs;
}

// Parameter

void Parameter::load(const QVariantMap &dataMap)
{
    GameObject::load(dataMap);

    QString unitToken = objectName() + QLatin1String("_unit");
    if (!gameData()->translate(unitToken).isEmpty())
        m_unitToken = unitToken;

    m_precision = dataMap.value("prec").toInt();
    m_lessIsBetter = dataMap.value("less_is_better").toBool();
    m_isMeta = dataMap.value("meta").toBool();
}

QString Parameter::format(const QVariant &value) const
{
    QString sValue;
    if (value.type() == QVariant::String) {
        sValue = gameData()->translate(value.toString());
    }
    else {
        sValue = QString::number(value.toDouble(), 'f', m_precision);

        if (!m_unitToken.isEmpty())
            sValue = QString("%1 %2").arg(sValue, gameData()->translate(m_unitToken));
    }
    return sValue;
}


// Item

void Item::load(const QVariantMap &dataMap)
{
    GameObject::load(dataMap);

    QVariantMap parameters = dataMap.value("parameters").toMap();
    m_parameters.load(gameData(), parameters);
    m_components.load(gameData(), dataMap.value("components").toMap());
    m_requirements.load(gameData(), dataMap.value("requirements").toMap());

    m_icon = dataMap.value("icon").toString();
    m_decoderLevel = parameters.value("entityinfo_researchlevel", 0).toInt();
    m_bonusExtension = gameData()->findObject<Extension *>(dataMap.value("entityinfo_bonus_extensionname").toString());
}

QPixmap Item::icon(int size) const
{
    QPixmap pixmap;
    if (!m_icon.isEmpty()) {
        if (!size)
            size = 120;
        QString key = QString("item-icon-%1-%2").arg(m_icon).arg(size);
        if (!QPixmapCache::find(key, &pixmap)) {
            pixmap.load(QLatin1String(":/icons/") + m_icon, 0, Qt::NoOpaqueDetection);
            if (pixmap.width() != size) {
                pixmap = pixmap.scaledToWidth(size, Qt::SmoothTransformation);
                pixmap.setMask(pixmap.createHeuristicMask());
            }
            QPixmapCache::insert(key, pixmap);
        }
    }
    return pixmap;
}


// ExtensionSet

void ExtensionSet::load(GameData *gameData, const QVariantMap &dataMap)
{
    m_levels.clear();
    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i) {
        Extension *extension = gameData->findObject<Extension *>(i.key());
        if (extension)
            m_levels.insert(extension, i.value().toInt());
        else
            qDebug() << "Unknown extension:" << i.key();
    }
}

ExtensionSet &ExtensionSet::operator+=(const ExtensionSet &other)
{
    for (const_iterator i = other.m_levels.constBegin(); i != other.m_levels.constEnd(); ++i) {
        iterator j = m_levels.find(i.key());
        if (j != m_levels.end())
            j.value() = qMax(j.value(), i.value());
        else
            m_levels.insert(i.key(), i.value());
    }
    return *this;
}

// ParameterSet

void ParameterSet::load(GameData *gameData, const QVariantMap &dataMap)
{
    m_values.clear();
    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i) {
        Parameter *parameter = gameData->findObject<Parameter *>(i.key());
        if (parameter) {
            QVariant value = i.value();
            if (parameter->precision())
                value.convert(QVariant::Double);
            m_values.insert(parameter, value);
        }
        else {
            qDebug() << "Unknown parameter:" << i.key();
        }
    }
}


// ComponentSet

void ComponentSet::load(GameData *gameData, const QVariantMap &dataMap)
{
    m_components.clear();
    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i) {
        Item *item = gameData->findObject<Item *>(i.key());
        if (item)
            m_components.insert(item, i.value().toInt());
        else
            qDebug() << "Unknown item:" << i.key();
    }
}


// ObjectGroup

void ObjectGroup::load(const QVariantMap &dataMap)
{
    foreach (const QVariant &objectName, dataMap.value("objects").toList()) {
        GameObject *object = gameData()->findObject(objectName.toString());
        if (object) {
            m_objects.append(object);
            object->setParentGroup(this);
        }
        else {
            qDebug() << "Unknown object" << objectName.toString() << "in group" << name();
        }
    }
    foreach (const QVariant &objectName, dataMap.value("groups").toList()) {
        ObjectGroup *group = gameData()->findObject<ObjectGroup *>(objectName.toString());
        if (group) {
            m_groups.append(group);
            group->setParentGroup(this);
        }
        else {
            qDebug() << "Unknown group" << objectName.toString() << "in group" << name();
        }
    }
}

const QList<GameObject *> &ObjectGroup::allObjects() const
{
    if (!m_allObjectsInitialized) {
        m_allObjects = m_objects;

        foreach (const ObjectGroup *group, m_groups)
            m_allObjects.append(group->allObjects());

        m_allObjectsInitialized = true;
    }
    return m_allObjects;
}


// GameData

GameObject *GameData::findObject(const QString& id)
{
    GameObject *object = m_objects.value(id);
    if (!object)
        object = m_objects.value(m_storedNameToId.value(id));
    return object;
}

template <class T>
void GameData::loadObjects(const QVariantMap &dataMap)
{
    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i)
        m_objects.insert(i.key(), new T(this, i.key()));

    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i)
        m_objects.value(i.key())->load(i.value().toMap());
}

QVariantMap GameData::loadVariantMap(QIODevice *io)
{
    if (!io->isOpen()) {
        if (!io->open(QIODevice::ReadOnly)) {
            m_lastError = tr("Failed to open data file.");
            return QVariantMap();
        }
    }

    QVariant data;
    QDataStream stream(io);
    stream.setVersion(QDataStream::Qt_4_7);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);
    stream >> data;
    return data.toMap();
}

bool GameData::load(QIODevice *io)
{
    QVariantMap dataMap = loadVariantMap(io);

    m_version = dataMap.value("version").toString();
    if (m_version.isEmpty()) {
        m_lastError = tr("Invalid data file.");
        return false;
    }

    loadObjects<Extension>(dataMap.value("extensions").toMap());
    loadObjects<Parameter>(dataMap.value("parameters").toMap());
    loadObjects<Item>(dataMap.value("items").toMap());
    loadObjects<ObjectGroup>(dataMap.value("groups").toMap());

    m_itemsGroup = findObject<ObjectGroup *>("item_groups");
    m_extensionsGroup = findObject<ObjectGroup *>("extension_groups");
    m_parametersGroup = findObject<ObjectGroup *>("parameter_groups");
    m_componentsGroup = findObject<ObjectGroup *>("component_groups");
    m_bonusesGroup = findObject<ObjectGroup *>("bonus_groups");

    int order = 0;
    foreach (GameObject *object, m_extensionsGroup->allObjects())
        object->setOrder(order++);
    foreach (GameObject *object, m_parametersGroup->allObjects())
        object->setOrder(order++);
    foreach (GameObject *object, m_itemsGroup->allObjects())
        object->setOrder(order++);

    QVariantMap starterAttributes = dataMap.value("starter_attributes").toMap();
    for (QVariantMap::const_iterator i = starterAttributes.constBegin(); i != starterAttributes.constEnd(); ++i)
        m_starterAttributes.insert(i.key(), AttributeSet(i.value()));

    QVariantMap sparkBonuses = dataMap.value("spark_bonus").toMap();
    for (QVariantMap::const_iterator i = sparkBonuses.constBegin(); i != sparkBonuses.constEnd(); ++i)
        m_sparkBonuses.insert(i.key(), AttributeSet(i.value()));

    QVariantMap starterExtensions = dataMap.value("starter_extensions").toMap();
    for (QVariantMap::const_iterator i = starterExtensions.constBegin(); i != starterExtensions.constEnd(); ++i) {
        ExtensionSet extensions;
        extensions.load(this, i.value().toMap());
        m_starterExtensions.insert(i.key(), extensions);
    }
    QVariantMap starterNames = dataMap.value("starter_names").toMap();
    for (QVariantMap::const_iterator i = starterNames.constBegin(); i != starterNames.constEnd(); ++i)
        m_starterNames.insert(i.key(), i.value().toString());

    QVariantMap storedNames = dataMap.value("stored_names").toMap();
    for (QVariantMap::const_iterator i = storedNames.constBegin(); i != storedNames.constEnd(); ++i) {
        // keys are names, values are ids
        m_storedNameToId.insert(i.key(), i.value().toString());
        m_storedNameFromId.insert(i.value().toString(), i.key());
    }

    return true;
}

bool GameData::loadTranslation(QIODevice *io)
{
    QVariantMap transMap = loadVariantMap(io);
    if (transMap.isEmpty())
        return false;

    m_translation.clear();
    for (QVariantMap::const_iterator i = transMap.constBegin(); i != transMap.constEnd(); ++i)
        m_translation.insert(i.key(), i.value().toString());

    return true;
}

ExtensionSet GameData::starterExtensions(QString choices) const
{
    QString choice = choices.left(3);
    while (choice.endsWith(QLatin1Char(' ')))
        choice.chop(1);

    return m_starterExtensions.value(choice);
}

AttributeSet GameData::starterAttributes(QString choices) const
{
    QString choice = choices.mid(1, 3);
    while (choice.endsWith(QLatin1Char(' ')))
        choice.chop(1);

    return m_starterAttributes.value(choice) + m_sparkBonuses.value(choices.right(2));
}
