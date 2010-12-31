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

#ifndef GAMEDATA_H
#define GAMEDATA_H

#include <QVariant>
#include <QPixmap>
#include <QHash>

class GameData;
class GameObject;
class Extension;
class Item;
class Parameter;
class ExtensionSet;
class ParameterSet;
class ObjectGroup;
class ExtensionLevels;
class Agent;
class QUndoCommand;


typedef QHash<Extension *, int> LevelMap;


enum Attribute { Tactics, Mechatronics, Industry, Research, Politics, Economics, NumAttributes, UnknownAttribute=-1 };

class AttributeSet {
public:
    AttributeSet();
    AttributeSet(QVariant data);
    AttributeSet(const AttributeSet& other);

    operator QVariant() const;
    AttributeSet operator+(const AttributeSet& other) const;

    int operator[](Attribute a) const { return m_attributes[a]; }

private:
    unsigned char m_attributes[NumAttributes];
};


class GameData : public QObject {
    Q_OBJECT

public:
    GameData(QObject *parent=0) : QObject(parent) {}

    QString version() { return m_version; }

    bool load(QIODevice *io);
    bool loadTranslation(QIODevice *io);
    QString lastError() { return m_lastError; }

    GameObject *findObject(const QString& id);
    template<class T>
    T findObject(const QString& id) { return qobject_cast<T>(findObject(id)); }

    ObjectGroup *items() { return m_itemsGroup; }
    ObjectGroup *extensions() { return m_extensionsGroup; }
    ObjectGroup *parameters() { return m_parametersGroup; }
    ObjectGroup *components() { return m_componentsGroup; }
    ObjectGroup *bonuses() { return m_bonusesGroup; }

    ExtensionSet starterExtensions(QString choices) const;
    AttributeSet starterAttributes(QString choices) const;
    QString starterName(QString choices) const { return translate(m_starterNames.value(choices)); }

    QString translate(const QString &key) const { return m_translation.value(key); }
    QString storedNameFromId(const QString &key) const { return m_storedNameFromId.value(key, key); }

private:
    QString m_lastError;
    QString m_version;

    QHash<QString, QString> m_translation;
    QHash<QString, QString> m_storedNameToId, m_storedNameFromId;

    ObjectGroup *m_itemsGroup;
    ObjectGroup *m_extensionsGroup;
    ObjectGroup *m_parametersGroup;
    ObjectGroup *m_componentsGroup;
    ObjectGroup *m_bonusesGroup;

    QHash<QString, GameObject *> m_objects;

    QHash<QString, AttributeSet> m_starterAttributes;
    QHash<QString, AttributeSet> m_sparkBonuses;
    QHash<QString, ExtensionSet> m_starterExtensions;
    QHash<QString, QString> m_starterNames;

    QVariantMap loadVariantMap(QIODevice *io);

    template <class T>
    void loadObjects(const QVariantMap &dataMap);
};


class ExtensionSet {
public:
    typedef LevelMap::const_iterator const_iterator;

    ExtensionSet() {}
    ExtensionSet(const ExtensionSet &other) : m_levels(other.m_levels) {}
    const ExtensionSet &operator=(const ExtensionSet &other) { m_levels = other.m_levels; return *this; }

    void load(GameData *gameData, const QVariantMap &dataMap);

    bool isEmpty() const { return m_levels.isEmpty(); }
    int level(Extension *extension) const { return m_levels.value(extension); }
    QList<Extension *> extensions() const { return m_levels.keys(); }

    const_iterator constBegin() const { return m_levels.constBegin(); }
    const_iterator constEnd() const { return m_levels.constEnd(); }

    ExtensionSet &operator+=(const ExtensionSet &other);

private:
    typedef LevelMap::iterator iterator;

    LevelMap m_levels;

    friend class ExtensionLevels; // for fast copying of starter attributes
};


class ParameterSet {
public:
    ParameterSet() {}
    ParameterSet(const ParameterSet &other) : m_values(other.m_values) {}
    const ParameterSet &operator=(const ParameterSet &other) { m_values = other.m_values; return *this; }

    void load(GameData *gameData, const QVariantMap &dataMap);

    bool isEmpty() const { return m_values.isEmpty(); }
    QVariant value(Parameter *parameter) const { return m_values.value(parameter); }
    QList<Parameter *> parameters() const { return m_values.keys(); }

private:
    QHash<Parameter *, QVariant> m_values;
};
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



class ComponentSet {
public:
    ComponentSet() {}
    ComponentSet(const ComponentSet& other) : m_components(other.m_components) {}
    const ComponentSet& operator=(const ComponentSet& other) { m_components = other.m_components; return *this; }

    void load(GameData *gameData, const QVariantMap &dataMap);

    bool isEmpty() const { return m_components.isEmpty(); }
    int amount(Item *item) const { return m_components.value(item); }
    QList<Item *> components() const { return m_components.keys(); }

private:
    QHash<Item *, int> m_components;
};


class GameObject : public QObject {
    Q_OBJECT

public:
    GameObject(GameData *gameData, const QString &id) : QObject(gameData), m_parentGroup(0) { setObjectName(id); }

    virtual void load(const QVariantMap &dataMap);

    GameData *gameData() const { return static_cast<GameData *>(parent()); }

    QString id() const { return objectName(); }
    QString name() const { return gameData()->translate(objectName()); }
    QString storedName() const { return gameData()->storedNameFromId(objectName()); }
    QString description() const { return gameData()->translate(m_descToken); }

    int order() const { return m_order; }
    void setOrder(int order) { m_order = order; }

    ObjectGroup *parentGroup() const { return m_parentGroup; }
    void setParentGroup(ObjectGroup *group) { m_parentGroup = group; }

    virtual const ExtensionSet *requirements() const { return 0; }

private:
    QString m_descToken;
    ObjectGroup *m_parentGroup;
    int m_order;
};


class Extension : public GameObject {
    Q_OBJECT

public:
    Extension(GameData *gameData, const QString &id) : GameObject(gameData, id) {}

    void load(const QVariantMap &dataMap);

    Attribute primaryAttribute() const { return m_primaryAttribute; }
    Attribute secondaryAttribute() const { return m_secondaryAttribute; }
    int complexity() const { return m_complexity; }

    const ExtensionSet *requirements() const { return m_requirements.isEmpty() ? 0 : &m_requirements; }
    const ExtensionSet *transitiveReqs() const;

private:
    Attribute m_primaryAttribute;
    Attribute m_secondaryAttribute;
    int m_complexity;
    QString m_modifierName;
    float m_modifierValue;

    ExtensionSet m_requirements;
    mutable ExtensionSet m_transitiveReqs;
};

class Parameter : public GameObject {
    Q_OBJECT

public:
    Parameter(GameData *gameData, const QString &id) : GameObject(gameData, id) {}

    void load(const QVariantMap &dataMap);

    QString unit() const { return m_unitToken.isEmpty() ? QString() : gameData()->translate(m_unitToken); }
    int precision() const { return m_precision; }
    bool lessIsBetter() const { return m_lessIsBetter; }
    bool isMeta() const { return m_isMeta; }

    QString format(const QVariant &) const;

private:
    QString m_unitToken;
    int m_precision;
    bool m_lessIsBetter : 1;
    bool m_isMeta : 1;
};


class Item : public GameObject {
    Q_OBJECT

public:
    Item(GameData *gameData, const QString &id) : GameObject(gameData, id) {}

    void load(const QVariantMap &dataMap);

    const ExtensionSet *requirements() const { return m_requirements.isEmpty() ? 0 : &m_requirements; }
    const ParameterSet *parameters() const { return m_parameters.isEmpty() ? 0 : &m_parameters; }
    const ComponentSet *components() const { return m_components.isEmpty() ? 0 : &m_components; }

    QVariant parameter(const QString &name) const { return m_parameters.value(gameData()->findObject<Parameter *>(name)); }

    Extension *bonusExtension() const { return m_bonusExtension; }
    int decoderLevel() const { return m_decoderLevel; }

    QPixmap icon(int size = 0) const;

private:
    QString m_icon;
    int m_decoderLevel;
    Extension *m_bonusExtension;

    ParameterSet m_parameters;
    ComponentSet m_components;
    ExtensionSet m_requirements;
};


class ObjectGroup : public GameObject {
    Q_OBJECT

public:
    ObjectGroup(GameData *gameData, const QString &id) : GameObject(gameData, id), m_allObjectsInitialized(false) {}

    void load(const QVariantMap &dataMap);

    const QList<ObjectGroup *> &groups() const { return m_groups; }
    const QList<GameObject *> &objects() const { return m_objects; }
    const QList<GameObject *> &allObjects() const;

private:
    QList<ObjectGroup *> m_groups;
    QList<GameObject *> m_objects;

    mutable QList<GameObject *> m_allObjects;
    mutable bool m_allObjectsInitialized;
};


Q_DECLARE_METATYPE(ObjectGroup *)
Q_DECLARE_METATYPE(Parameter *)
Q_DECLARE_METATYPE(Extension *)
Q_DECLARE_METATYPE(Item *)


#endif // GAMEDATA_H
