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
#include <QVector>
#include <QMap>
#include <QHash>
#include <QPixmap>

class GameData;
class GameObject;
class ExtensionCategory;
class Extension;
class FieldCategory;
class AggregateField;
class Category;
class Tier;
class Definition;
class ExtensionLevelMap;
class CharacterWizardStep;


class GameData : public QObject {
    Q_OBJECT

public:
    enum ModuleAttributeFlags {
        PassiveModule = 0x10, WeaponModule = 0x40000
    };
    enum ModuleFlags {
        Turret = 0x1, Missile = 0x2, Industiral = 0x200, Ewar = 0x400,
        Head = 0x8, Leg = 0x20,
        Small = 0x40, Medium = 0x80, Large = 0x100
    };
    enum CharWizSteps { Race, School, Major, Corporation, Spark, NumCharWizSteps };

    GameData(QObject *parent = 0) : QObject(parent) {}

    QString version() { return m_version; }

    bool load(QIODevice *dataFile, QIODevice *translationFile);
    bool loadTranslation(const QString &languageCode, QIODevice *translationFile);

    const QMap<int, ExtensionCategory *> &extensionCategories() const { return m_extensionCategories; }
    const QMap<int, Extension *> &extensions() const { return m_extensions; }
    const QMap<int, FieldCategory *> &fieldCategories() const { return m_fieldCategories; }
    const QMap<int, AggregateField *> &aggregateFields() const { return m_aggregateFields; }
    const QMap<quint64, Category *> &categories() const { return m_categories; }
    const QMap<int, Definition *> &definitions() const { return m_definitions; }
    const QMap<int, CharacterWizardStep *> &charWizSteps(CharWizSteps step) { return m_charWizSteps[step]; }

    const QHash<QString, Tier *> &tiers() const { return m_tiers; }
    const QMap<int, QString> &slotTypes() const { return m_slotTypes; }

    Category *rootCategory() const { return m_categories.value(0); }

    template <class T>
    T findByName(const QString &name) { return qobject_cast<T>(m_objectsByName.value(name)); }

    QList<Definition *> definitionsInCategory(const Category *category) const;

    ExtensionLevelMap starterExtensions(QString steps) const;
    QString starterName(QString steps) const;

    QString translate(const QString &key) const { return m_translation.value(key, key); }
    bool hasTranslation(const QString &key) const { return m_translation.contains(key); }
    QString persistentName(const QString &name) const { return m_persistentNames.value(name, name); }

    QString wikiToHtml(const QString &wiki) const;

private:
    QString m_version;
    QString m_languageCode;

    QHash<QString, QString> m_translation;

    QVariantMap loadVariantMap(QIODevice *io);

    QHash<QString, GameObject *> m_objectsByName;
    QHash<QString, QString> m_persistentNames;

    QMap<int, ExtensionCategory *> m_extensionCategories;
    QMap<int, Extension *> m_extensions;
    QMap<int, FieldCategory *> m_fieldCategories;
    QMap<int, AggregateField *> m_aggregateFields;
    QMap<quint64, Category *> m_categories;
    QMap<int, Definition *> m_definitions;

    QHash<QString, Tier *> m_tiers;
    QMap<int, QString> m_slotTypes;

    QMap<quint64, Definition *> m_definitionsByCategory;

    QMap<int, CharacterWizardStep *> m_charWizSteps[NumCharWizSteps];

    template <class ItemType, class ObjectMap>
    bool loadObjects(ObjectMap& objectMap, const QVariantMap &dataMap, const QString &idKey, bool create = true);
    bool loadConfigurationUnits(const QVariantMap &dataMap);

    QVector<CharacterWizardStep *> decodeCharWizSteps(const QString &choices) const;
};


class ExtensionLevelMap : public QMap<Extension *, int> {
public:
    inline QList<Extension *> extensions() const { return keys(); }

    void sum(const ExtensionLevelMap &other);
    void merge(const ExtensionLevelMap &other);
};


class GameObject : public QObject {
    Q_OBJECT

public:
    GameObject(GameData *gameData, const QString &name = QString()) : m_gameData(gameData), m_name(name), m_hidden(false) {}

    virtual bool load(const QVariantMap &dataMap) = 0;

    GameData *gameData() const { return m_gameData; }

    QString systemName() const { return m_name; }
    QString name() const { return m_gameData->translate(m_name); }
    QString persistentName() const { return m_gameData->persistentName(m_name); }
    bool hidden() const { return m_hidden; }

protected:
    QString m_name;
    bool m_hidden;
    GameData *m_gameData;
};


class ExtensionCategory : public GameObject {
    Q_OBJECT

public:
    ExtensionCategory(GameData *gameData) : GameObject(gameData), m_id(0) {}

    bool load(const QVariantMap &dataMap);

    int id() const { return m_id; }
    const QList<Extension *> &extensions() const { return m_extensions; }

private:
    int m_id;
    QList<Extension *> m_extensions;

    friend class Extension;
};


class Extension : public GameObject {
    Q_OBJECT

public:
    Extension(GameData *gameData) : GameObject(gameData), m_id(0), m_rank(0), m_price(0), m_category(0) {}

    bool load(const QVariantMap &dataMap);

    QString description() const;
    int complexity() const { return m_rank; }
    int price() const { return m_price; }

    ExtensionCategory *category() const { return m_category; }

    const ExtensionLevelMap &requirements() const { return m_requirements; }
    const ExtensionLevelMap &transitiveReqs() const;

private:
    int m_id;
    QString m_description;
    int m_rank, m_price;
    float m_bonus;
    ExtensionCategory *m_category;
    ExtensionLevelMap m_requirements;
    mutable ExtensionLevelMap m_transitiveReqs;
};


class FieldCategory : public GameObject {
    Q_OBJECT

public:
    FieldCategory(GameData *gameData) : GameObject(gameData) {}

    bool load(const QVariantMap &dataMap);

    int id() const { return m_id; }
    const QList<AggregateField *> &aggregates() const { return m_aggregates; }

private:
    int m_id;
    QList<AggregateField *> m_aggregates;

    friend class AggregateField;
};


class AggregateField : public GameObject {
    Q_OBJECT

public:
    enum {
        // Some aggregate IDs
        ShieldAbsorbtion = 318, Slope = 326,
        ResistChemical = 0x12f, ResistExplosive = 0x132, ResistKinetic = 0x135, ResistThermal = 0x138,

        // Artificial IDs for generic entity info fields
        EI_Mass = 9001, EI_Volume, EI_RepackedVolume, EI_Quantity, EI_Capacity, EI_Tier, EI_AmmoCapacity, EI_AmmoType,
        EI_ActiveModule, EI_SlotType,
        EI_ChassisSlot0, EI_ChassisSlot1, EI_ChassisSlot2, EI_ChassisSlot3, EI_ChassisSlot4,
        EI_ChassisSlot5, EI_ChassisSlot6, EI_ChassisSlot7, EI_ChassisSlotLast = EI_ChassisSlot7,
        EI_HeadSlots, EI_ChassisSlots, EI_LegSlots,

        // Definition configuration units (no fixed ID assignment)
        CU_Base = 10001
    };

    enum Categories { CategoryInfo = 6 };

    enum CompareLevel { CanCompare, NoCompare, OnlyCompare };

    AggregateField(GameData *gameData, int id, const QString &name)
        : GameObject(gameData, name),
          m_id(id), m_multiplier(1), m_offset(0), m_digits(0), m_compareLevel(CanCompare), m_category(0), m_lessIsBetter(false) {}

    bool load(const QVariantMap &dataMap);

    QString name() const;

    QString unit() const { return m_unitName.isEmpty() ? QString() : gameData()->translate(m_unitName); }
    int digits() const { return m_digits; }
    bool lessIsBetter(Definition *definition = 0) const;

    FieldCategory *category() const { return m_category; }

    QString format(const QVariant &) const;

    bool isAggregate() const { return m_id < EI_Mass; }

    CompareLevel compareLevel() const { return m_compareLevel; }

protected:
    bool m_lessIsBetter;

    int m_id;
    QString m_unitName;
    float m_multiplier, m_offset;
    int m_digits;
    CompareLevel m_compareLevel;
    FieldCategory *m_category;

    QString formatAuto(float value) const;
};


class StandardAggregateField : public AggregateField
{
public:
    StandardAggregateField(GameData *gameData) : AggregateField(gameData, 0, QString()) {}

    bool load(const QVariantMap &dataMap);
};


class Category : public GameObject {
    Q_OBJECT

public:
    Category(GameData *gameData)
        : GameObject(gameData), m_id(0), m_order(0), m_inMarket(false), m_marketCount(-1), m_parent(0) {}

    bool load(const QVariantMap &dataMap);

    quint64 id() const { return m_id; }
    quint64 order() const { return m_order; }
    quint64 orderHigh() const;
    bool inMarket() const { return m_inMarket; }
    Category *parent() const { return m_parent; }

    const QList<Category *> &categories() const { return m_categories; }
    QList<Definition *> definitions() const { return gameData()->definitionsInCategory(this); }

    void setInMarket(bool inMarket);
    int marketCount() const;

    bool hasCategory(const Category *other); // aka selfOrDescendant

private:
    quint64 m_id, m_order;
    bool m_inMarket;
    mutable int m_marketCount;

    friend class Definition;

    Category *m_parent;
    QList<Category *> m_categories;
    QList<Definition *> m_definitions;
};


class Tier : public GameObject {
    Q_OBJECT

public:
    Tier(GameData *gameData) : GameObject(gameData), m_id(0) {}

    bool load(const QVariantMap &dataMap);

    int id() const { return m_id; }
    QColor color() const { return m_color; }

    QPixmap icon(bool overlay = false) const;
private:
    int m_id;
    QColor m_color;
};


class Bonus {
public:
    Bonus(GameData *gameData, const QVariantMap &dataMap);

    AggregateField *aggregate() const { return m_aggregate; }
    Extension *extension() const { return m_extension; }
    float bonus() const { return m_bonus; }
    bool effectEnhancer() const { return m_effectEnhancer; }

    bool isValid() const { return m_aggregate != 0 && m_extension != 0; }
    QString format() const;

private:
    bool load(GameData *gameData, const QVariantMap &dataMap);

    AggregateField * m_aggregate;
    Extension *m_extension;
    float m_bonus;
    bool m_effectEnhancer;

    friend class Definition;
};


class Definition : public GameObject {
    Q_OBJECT

public:
    enum AttributeFlags {
        Active = 0x10, HasAmmo = 0x40000
    };
    enum ModuleFlags {
        TurretSlot = 0x1, MissileSlot = 0x2, IndustrialSlot = 0x200, OtherSlot = 0x400, HeadSlot = 0x8, LegSlot = 0x20,
        Small = 0x40, Medium = 0x80, Large = 0x100,

        SlotMask = 0x62b, SizeMask = 0x1c0
    };

    Definition(GameData *gameData) : GameObject(gameData), m_id(0), m_quantity(0),
        m_attributeFlags(0), m_category(0), m_categoryFlags(0), m_mass(0), m_volume(0), m_repackedVolume(0), m_inMarket(false),
        m_researchLevel(0), m_calibrationProgram(0), m_tier(0), m_head(0), m_chassis(0), m_leg(0) {}
    ~Definition() { qDeleteAll(m_bonuses); }

    bool load(const QVariantMap &dataMap);

    int id() const { return m_id; }
    QString description() const { return m_gameData->wikiToHtml(m_gameData->translate(m_description)); }
    int researchLevel() const { return m_researchLevel; }

    Category *category() const { return m_category; }
    bool hasCategory(const QString &name) const;

    QPixmap icon(int size = -1, bool decorated = false) const;
    Tier *tier() const { return m_tier; }

    bool inMarket() const { return m_inMarket; }
    void setInMarket(bool inMarket) { m_inMarket = inMarket; }

    bool isPrototype() const { return m_name.endsWith("_pr"); }
    bool isRobot() const { return m_head != 0; }

    Definition *head() const { return m_head; }
    Definition *chassis() const { return m_chassis; }
    Definition *leg() const { return m_leg; }

    int slotFlag() const { return m_moduleFlag & SlotMask; }
    int sizeFlag() const { return m_moduleFlag & SizeMask; }

    const ExtensionLevelMap &requirements() const { return m_enablerExtensions; }
    const QMap<AggregateField *, QVariant> &aggregates() const { return m_aggregates; }
    const QMap<Definition *, int> &components() const { return m_components; }
    const QMap<AggregateField *, Bonus *> &bonuses() const { return m_bonuses; }

private:
    int m_id;
    QString m_description, m_icon;

    int m_quantity;
    quint64 m_attributeFlags, m_categoryFlags;
    int m_moduleFlag;

    float m_mass, m_volume, m_repackedVolume;
    float m_capacity;

    Definition *m_head, *m_chassis, *m_leg;
    Category *m_ammoType;

    bool m_inMarket;

    QVector<int> m_slotFlags;
    Tier *m_tier;

    QMap<AggregateField *, QVariant> m_aggregates;
    QMap<AggregateField *, Bonus *> m_bonuses;

    ExtensionLevelMap m_enablerExtensions;
    QList<Extension *> m_extensions;

    int m_researchLevel;
    Definition *m_calibrationProgram;
    QMap<Definition *, int> m_components;

    Category *m_category;

    void addAggregate(int id, const QVariant &value);
};


class CharacterWizardStep : public GameObject
{
public:
    CharacterWizardStep(GameData *gameData) : GameObject(gameData), m_id(0), m_baseStep(0), m_baseEID(0) {}

    bool load(const QVariantMap &dataMap);

    int id() const { return m_id; }
    QString description() const { return m_gameData->translate(m_description); }
    const ExtensionLevelMap &extensions() const { return m_extensions; }

private:
    int m_id, m_baseEID;
    QString m_description;
    ExtensionLevelMap m_extensions;

    CharacterWizardStep *m_baseStep;
};


#endif // GAMEDATA_H
