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

#define _USE_MATH_DEFINES
#include <cmath>
#include <cstdlib> // for _byteswap_uint64

#include <QDebug>
#include <QBitmap>
#include <QPainter>
#include <QPixmapCache>
#include <QRegExp>

#include "gamedata.h"
#include "json.h"

using namespace std;


// GameData

template <class ItemType, class ObjectMap>
bool GameData::loadObjects(ObjectMap& objectMap, const JsonValue &value, const char *idKey, bool create)
{
    foreach (JsonValue entry, value) {
        typename ObjectMap::key_type id = entry[idKey].value<typename ObjectMap::key_type>();

        typename ObjectMap::mapped_type object = objectMap.value(id);
        if (!object && create) {
            object = new ItemType(this);
            objectMap.insert(id, object);
        }
        if (object) {
            if (!object->load(entry))
                return false;
            if (create)
                m_objectsByName.insert(object->systemName(), object);
        }
    }
    return true;
}

bool GameData::loadConfigurationUnits(const JsonValue &value)
{
    int confUnitId = AggregateField::CU_Base;
    for (JsonValue::const_iterator i = value.begin(); i != value.end(); ++i, ++confUnitId) {
        AggregateField *aggregate = new AggregateField(this, confUnitId, i.key());
        if (!aggregate->load(i.value()))
            return false;

        m_aggregateFields.insert(confUnitId, aggregate);
        m_objectsByName.insert(aggregate->systemName(), aggregate);
    }
    return true;
}

bool GameData::load(QIODevice *dataFile, QIODevice *translationFile)
{
    if (!dataFile->open(QIODevice::ReadOnly) || !translationFile->open(QIODevice::ReadOnly))
        return false;

    JsonDocument gameData(dataFile->readAll());
    if (gameData.hasError()) {
        qDebug() << gameData.errorString();
        return false;
    }

    m_version = gameData["version"].toString();
    if (m_version.isEmpty())
        return false;

    JsonValue custom = gameData["_custom"];

    // Note: QVariantMap forces string type and string sorting on keys, therefore loading order
    // may be different from the source file.

    // Slot types
    foreach (JsonValue slotType, custom["slotTypes"])
        if (slotType.isObject())
            m_slotTypes.insert(slotType["flag"].toInt(), slotType["name"].toString());

    m_categories.insert(0, new Category(this)); // root category

    // NB: loading order is important.
    bool ok = loadObjects<ExtensionCategory>(m_extensionCategories, gameData["extensionCategoryList"], "ID")
            && loadObjects<Extension>(m_extensions, gameData["extensionGetAll"], "extensionID")
            && loadObjects<Extension>(m_extensions, gameData["extensionPrerequireList"], "extensionID", false)

            && loadObjects<Category>(m_categories, gameData["categoryFlags"], "value")
            // resolve forward refs
            && loadObjects<Category>(m_categories, gameData["categoryFlags"], "value", false)

            && loadObjects<FieldCategory>(m_fieldCategories, custom["aggregateCategories"], "ID")
            && loadObjects<StandardAggregateField>(m_aggregateFields, gameData["getAggregateFields"], "ID")
            && loadObjects<StandardAggregateField>(m_aggregateFields, custom["genericFields"], "ID")
            && loadObjects<StandardAggregateField>(m_aggregateFields, custom["aggregateDetails"], "ID", false)
            && loadConfigurationUnits(gameData["getDefinitionConfigUnits"])

            && loadObjects<Tier>(m_tiers, custom["tiers"], "name")

            && loadObjects<Definition>(m_definitions, gameData["getEntityDefaults"], "definition")
            // resolve IDs in options
            && loadObjects<Definition>(m_definitions, gameData["getEntityDefaults"], "definition", false)
            && loadObjects<Definition>(m_definitions, gameData["definitionProperties"], "definition", false)
            // resolve copyfrom's
            && loadObjects<Definition>(m_definitions, gameData["definitionProperties"], "definition", false)
            && loadObjects<Definition>(m_definitions, gameData["productionComponentsList"], "definition", false)
            && loadObjects<Definition>(m_definitions, gameData["getResearchLevels"], "definition", false);

    if (!ok) return false;

    // Load character wizard steps
    JsonValue charWiz = gameData["characterWizardData"];
    ok = loadObjects<CharacterWizardStep>(m_charWizSteps[Race], charWiz["race"], "ID")
            && loadObjects<CharacterWizardStep>(m_charWizSteps[School], charWiz["school"], "ID")
            && loadObjects<CharacterWizardStep>(m_charWizSteps[Major], charWiz["major"], "ID")
            && loadObjects<CharacterWizardStep>(m_charWizSteps[Corporation], charWiz["corporation"], "ID")
            && loadObjects<CharacterWizardStep>(m_charWizSteps[Spark], charWiz["spark"], "ID");

    if (!ok) return false;

    // Build definition index in category tree order
    foreach (Definition *definition, m_definitions)
        m_definitionsByCategory.insertMulti(definition->category()->order(), definition);

    // Persistent names
    JsonDocument transMap(translationFile->readAll());
    if (transMap.hasError()) {
        qDebug() << transMap.errorString();
        return false;
    }

    m_persistentNames.reserve(m_extensions.size() + 10);
    foreach (Extension *extension, m_extensions) {
        QByteArray sysName = extension->systemName().toUtf8(); // XXX
        QString persistentName = transMap[sysName.constData()].toString();
        m_persistentNames.insert(extension->systemName(), persistentName);
        m_objectsByName.insert(persistentName, extension);
    }
    foreach (JsonValue entry, custom["historicNames"]) {
        QString name = entry["name"].toString();
        QString persistentName = entry["value"].toString();
        if (Extension *extension = findByName<Extension *>(name))
            m_objectsByName.insert(persistentName, extension);
        else
            qDebug() << "Invalid historic name:" << name;
    }

    return true;
}

bool GameData::loadTranslation(const QString &languageCode, QIODevice *translationFile)
{
    if (!translationFile->open(QIODevice::ReadOnly))
        return false;

    JsonDocument transMap(translationFile->readAll());
    if (transMap.hasError()) {
        qDebug() << transMap.errorString();
        return false;
    }

    m_translation.clear();
    m_translation.reserve(transMap.size() + 10);
    for (JsonValue::const_iterator i = transMap.begin(); i != transMap.end(); ++i) {
        m_translation.insert(i.key(), i.value().toString());
        m_translation.insert(i.key().toLower(), i.value().toString());
    }

    m_languageCode = languageCode;
    return true;
}

QVector<CharacterWizardStep *> GameData::decodeCharWizSteps(const QString &choices) const
{
    // This code relies on steps being ordered (by parent and mil/ind/soc) and having sequential ids.

    QVector<CharacterWizardStep *> result(NumCharWizSteps);

    QByteArray codes = choices.toAscii().leftJustified(6, '\x7f', true);
    codes.replace(' ', '\x7f');
    codes.replace('T', '\x00').replace('I', '\x01').replace('A', '\x02');
    codes.replace('M', '\x00').replace('I', '\x01').replace('L', '\x02');

    int raceNo = codes.at(0);
    int schoolNo = raceNo * 3 + codes.at(1);
    int majorNo = schoolNo * 3 + codes.at(2);
    int corpNo = schoolNo * 3 + codes.at(3);
    int sparkNo = codes.at(4) * 3 + codes.at(5);

    result[Race] = m_charWizSteps[Race].value(raceNo + 1);
    result[School] = m_charWizSteps[School].value(schoolNo + 1);
    result[Major] = m_charWizSteps[Major].value(majorNo + 1);
    result[Corporation] = m_charWizSteps[Corporation].value(corpNo + 495);
    result[Spark] = m_charWizSteps[Spark].value(sparkNo + 1);

    return result;
}

QString GameData::starterName(QString choices) const
{
    CharacterWizardStep *step = decodeCharWizSteps(choices).at(choices.size() - 1);
    return step ? step->name() : QString();
}

ExtensionLevelMap GameData::starterExtensions(QString choices) const
{
    ExtensionLevelMap extensions;
    foreach (CharacterWizardStep *step, decodeCharWizSteps(choices))
        if (step)
            extensions.sum(step->extensions());
    return extensions;
}

QList<Definition *> GameData::definitionsInCategory(const Category *category) const
{
    QList<Definition *> result;
    QMap<quint64, Definition *>::const_iterator
            i = m_definitionsByCategory.lowerBound(category->order()),
            iEnd = m_definitionsByCategory.upperBound(category->orderHigh());
    for (; i != iEnd; ++i)
        result << *i;
    return result;
}

QString GameData::wikiToHtml(const QString &wiki) const
{
    QString html = wiki;

    html.replace(QRegExp("'''([^']+)'''"), "<b>\\1</b>");
    html.replace(QRegExp("''([^']+)''"), "<i>\\1</i>");
    html.replace(QRegExp("\\[\\[Help:([^\\|]+)\\|([^\\]]+)\\]\\]"),
                 "<a href=\"http://www.perpetuum-online.com/Help:\\1\">\\2</a>");
    html.replace(QRegExp("\\[color=([^\\[]+)\\]([^\\[]+)\\[/color\\]"), "<font color=\"\\1\">\\2</font>");
    html.replace(QRegExp("\r?\n\r?\n"), "</p><p>");
    html = "<p>" + html + "</p>";
    return html;
}


// ExtensionLevelMap

void ExtensionLevelMap::merge(const ExtensionLevelMap &other)
{
    for (const_iterator i = other.constBegin(); i != other.constEnd(); ++i)
        insert(i.key(), qMax(value(i.key()), i.value()));
}

void ExtensionLevelMap::sum(const ExtensionLevelMap &other)
{
    for (const_iterator i = other.constBegin(); i != other.constEnd(); ++i)
        insert(i.key(), value(i.key()) + i.value());
}


// ExtensionCategory

bool ExtensionCategory::load(const JsonValue &value)
{
    m_id = value["ID"].toInt();
    m_name = value["name"].toString();
    m_hidden = value["hidden"].toInt();
    return true;
}


// Extension

bool Extension::load(const JsonValue &value)
{
    if (value.contains("name")) {
        m_id = value["extensionId"].toInt();
        m_name = value["name"].toString();
        m_description = value["description"].toString();
        m_description = value["description"].toString();
        m_rank = value["rank"].toInt();
        m_price = value["price"].toInt();
        m_bonus = value["bonus"].toFloat();
        m_hidden = value["hidden"].toInt();

        m_category = m_gameData->extensionCategories().value(value["category"].toInt());
        if (!m_category) {
            qDebug() << "Invalid extension category:" << m_name;
            return false;
        }
        m_category->m_extensions.append(this);
    }
    else if (value.contains("requiredExtension")) {
        Extension *extension = m_gameData->extensions().value(value["requiredExtension"].toInt());
        if (extension)
            m_requirements.insert(extension, value["requiredLevel"].toInt());
    }
    return true;
}

QString Extension::description() const
{
    QString desc = m_gameData->translate(m_description);
    desc = m_gameData->wikiToHtml(desc);

    desc.replace("{%BONUS001%}", QString::number(fabs(m_bonus), 'f', 2))
            .replace("{%BONUS01%}", QString::number(fabs(m_bonus), 'f', 1))
            .replace("{%BONUS%}", QString::number((int) fabs(floor(m_bonus))))
            .replace("{%BONUS10%}", QString::number((int) fabs(m_bonus * 10.f)))
            .replace("{%BONUS100%}", QString::number((int) fabs(m_bonus * 100.1f)))
            .replace("{%BONUS100.1%}",QString::number(fabs(m_bonus * 100.f), 'f', 1))
            .replace("{%BONUS100.2%}", QString::number(fabs(m_bonus * 100.f), 'f', 2));

    return desc;
}

const ExtensionLevelMap &Extension::transitiveReqs() const
{
    if (m_transitiveReqs.isEmpty() && !m_requirements.isEmpty()) {
        m_transitiveReqs = m_requirements;
        for (ExtensionLevelMap::const_iterator i = m_requirements.constBegin(); i != m_requirements.constEnd(); ++i)
            m_transitiveReqs.merge(i.key()->transitiveReqs());
    }
    return m_transitiveReqs;
}


// Category

bool Category::load(const JsonValue &value)
{
    if (!m_id) {
        m_id = value["value"].toUInt64();
        m_name = value["name"].toString();
        m_hidden = value["hidden"].toInt();
        m_order = _byteswap_uint64(m_id);
    }
    else {
        quint64 parentId = m_id;
        quint64 mask = 0xff;
        while (parentId & mask)
            mask <<= 8;
        parentId &= ~(mask >> 8);

        m_parent = m_gameData->categories().value(parentId);
        if (!m_parent) {
            qDebug() << "Invalid category parent:" << m_name;
            return false;
        }
        m_parent->m_categories.append(this);
    }
    return true;
}

void Category::setInMarket(bool inMarket)
{
    m_inMarket = inMarket;
    if (inMarket && m_parent)
        m_parent->setInMarket(inMarket);
}

int Category::marketCount() const
{
    if (m_marketCount == -1) {
        m_marketCount = 0;
        foreach (Definition *definition, m_definitions)
            m_marketCount += definition->inMarket();
        foreach (Category *subcat, m_categories)
            m_marketCount += subcat->marketCount();
    }
    return m_marketCount;
}

quint64 Category::orderHigh() const
{
    quint64 endFlags = m_order;
    quint64 mask = 0xff;
    while (!(mask & endFlags)) {
        endFlags |= mask;
        mask <<= 8;
    }
    return endFlags;
}

bool Category::hasCategory(const Category *other)
{
    return m_order >= other->m_order && m_order <= other->orderHigh();
}


// FieldCategory

bool FieldCategory::load(const JsonValue &value)
{
    m_id = value["ID"].toInt();
    m_name = value["name"].toString();
    return true;
}


// AggregateField/StandardAggregateField

bool AggregateField::load(const JsonValue &value)
{
    m_unitName = value["measurementUnit"].toString();
    if (m_unitName.isNull()) {
        m_unitName = m_name + "_unit";
        if (!m_gameData->hasTranslation(m_unitName))
            m_unitName.clear();
    }
    m_multiplier = value["measurementMultiplier"].toFloat(1.f);
    m_offset = value["measurementOffset"].toFloat();
    m_digits = value["digits"].toInt(-1); // -1 = auto
    m_category = m_gameData->fieldCategories().value(value["category"].toInt(CategoryInfo));
    if (!m_category) {
        qDebug() << "Invalid aggregate category:" << m_name;
        return false;
    }
    m_category->m_aggregates.append(this);
    return true;
}

bool StandardAggregateField::load(const JsonValue &value)
{
    if (value.contains("name")) {
        m_id = value["ID"].toInt();
        m_name = value["name"].toString();
        AggregateField::load(value);
    }
    else {
        m_hidden = value["hidden"].toInt();
        m_lessIsBetter = value["lessIsBetter"].toInt();
        m_compareLevel = static_cast<CompareLevel>(value["compareLevel"].toInt(CanCompare));
    }
    return true;
}

QString AggregateField::name() const
{
    if (m_id >= EI_ChassisSlot0 && m_id <= EI_ChassisSlotLast)
        return QString("%1 #%2").arg(GameObject::name()).arg(m_id - EI_ChassisSlot0 + 1);
    return GameObject::name();
}

QString AggregateField::format(const QVariant &value) const
{
    QString sValue;

    if (value.type() == QVariant::String)
        return m_gameData->translate(value.toString());

    if (m_id >= EI_ChassisSlot0 && m_id <= EI_ChassisSlotLast) {
        int slotFlags = value.toInt();
        QStringList slotType, slotSize;

        if (slotFlags & GameData::Turret)
            slotType << m_gameData->translate("entityinfo_robot_slots_turret");
        if (slotFlags & GameData::Missile)
            slotType << m_gameData->translate("entityinfo_robot_slots_missile");
        if (slotFlags & GameData::Industiral)
            slotType << m_gameData->translate("entityinfo_robot_slots_industrial");
        if (slotFlags & GameData::Ewar)
            slotType << m_gameData->translate("entityinfo_robot_slots_ew");

        if (slotFlags & GameData::Small)
            slotSize << QString::fromWCharArray(L"\u2022");
        if (slotFlags & GameData::Medium)
            slotSize << QString::fromWCharArray(L"\u2022\u2022");
        if (slotFlags & GameData::Large)
            slotSize << QString::fromWCharArray(L"\u2022\u2022\u2022");

        return QString("%1 (%2)").arg(slotType.join(", "), slotSize.join("/"));
    }

    float fValue = value.toFloat();
    int digits = m_digits;

    if (digits == -1) {
        if (value.type() == QVariant::Int || value.type() == QVariant::UInt)
            digits = 0;
        else
            digits = (fValue >= 0.01f || fValue <= 0.f) ? 2 : 5;
    }

    switch (m_id) {
    case ShieldAbsorbtion:
        fValue = 1.f / fValue;
        break;
    case Slope:
        fValue = atan(fValue * 0.25f) / M_PI * 180.f;
        break;
    }

    sValue = QString::number(fValue * m_multiplier + m_offset, 'f', digits);
    if (!m_unitName.isEmpty())
        sValue = QString("%1 %2").arg(sValue, m_gameData->translate(m_unitName));

    switch (m_id) {
    case ResistChemical:
    case ResistExplosive:
    case ResistKinetic:
    case ResistThermal:
        float resistPercentage = 100.f * fValue / (fValue + 100.f);
        sValue = QString("%1 (%2%)").arg(sValue).arg(resistPercentage, 0, 'f', 2);
    }

    return sValue;
}

QString AggregateField::formatAuto(float value) const
{
    int digits = (value >= 0.01f || value <= 0.f) ? 2 : 5;
    return QString("%1 %2").arg(value, 0, 'f', digits).arg(m_gameData->translate(m_unitName));
}

bool AggregateField::lessIsBetter(Definition *definition) const
{
    // less mass is better for mods, but not for bots
    if (m_id == EI_Mass && definition && definition->isRobot())
        return false;
    return m_lessIsBetter;
}

// Tier

bool Tier::load(const JsonValue &value)
{
    m_id = value["ID"].toInt();
    m_name = value["name"].toString();
    uint color = value["color"].toUInt();
    // Decode BGRA color value
    m_color = QColor(color & 0xff, (color >> 8) & 0xff,
                     (color >> 16) & 0xff, (color >> 24) & 0xff);
    return true;
}

QPixmap Tier::icon(bool overlay) const
{
    QPixmap pixmap;
    QString key = QString("TierIcon-%1-%2").arg(m_name).arg(overlay);

    if (!QPixmapCache::find(key, pixmap)) {
        QPixmap bg(overlay ? ":/tier_bg.png" : ":/tier_bg_short.png");

        pixmap = bg;
        QPainter painter(&pixmap);
        painter.setCompositionMode(QPainter::CompositionMode_Multiply);
        painter.fillRect(pixmap.rect(), m_color);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        painter.drawPixmap(0, 0, bg); // regain bg's alpha lost in Multiply
        painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

        QFont uniFont("uni 05_53");
        uniFont.setPixelSize(8);
        uniFont.setStyleStrategy(QFont::NoAntialias);
        painter.setFont(uniFont);
        painter.setPen(Qt::white);
        painter.drawText(pixmap.rect().translated(0, -1), Qt::AlignTop | Qt::AlignHCenter | Qt::TextSingleLine, name());
        painter.end();

        QPixmapCache::insert(key, pixmap);
    }
    return pixmap;
}


// Bonus

Bonus::Bonus(GameData *gameData, const JsonValue &value)
{
    m_aggregate = gameData->aggregateFields().value(value["aggregate"].toInt());
    m_extension = gameData->extensions().value(value["extensionID"].toInt());
    m_bonus = value["bonus"].toFloat();
    m_effectEnhancer = value["effectenhancer"].toInt();
}

QString Bonus::format() const
{
    return QString("%1%").arg((int) fabs(floor(m_bonus * 100.f)));
}


// Definition

void Definition::addAggregate(int id, const QVariant &value)
{
    AggregateField *field = m_gameData->aggregateFields().value(id);
    if (field)
        m_aggregates.insert(field, value);
    else
        qDebug() << "Aggregate not found:" << id;
}

bool Definition::load(const JsonValue &value)
{
    if (value.contains("definitionname")) {
        if (!m_id) {
            m_id = value["definition"].toInt();
            m_name = value["definitionname"].toString();
            m_description = value["descriptiontoken"].toString();
            m_quantity = value["quantity"].toInt();

            m_attributeFlags = value["attributeflags"].toUInt64();

            m_categoryFlags = value["categoryflags"].toUInt64();
            m_category = m_gameData->categories().value(m_categoryFlags);
            if (m_category) {
                m_category->m_definitions.append(this);
            }
            else {
                qDebug() << "Invalid definition category:" << m_name;
                m_category = m_gameData->rootCategory();
            }

            // m_health = value["health"].toFloat();
            m_mass = value["mass"].toFloat();
            m_volume = value["volume"].toFloat();
            m_repackedVolume = value["repackedvolume"].toFloat();

            foreach (JsonValue  &entry, value["enablerExtension"]) {
                Extension *extension = m_gameData->extensions().value(entry["extensionID"].toInt());
                if (extension)
                    m_enablerExtensions.insert(extension, entry["extensionLevel"].toInt());
            }

            foreach (JsonValue entry, value["accumulator"])
                addAggregate(entry["ID"].toInt(), entry["value"].toVariant());

            foreach (JsonValue entry, value["bonus"]) {
                Bonus *bonus = new Bonus(m_gameData, entry);
                if (bonus->isValid())
                    m_bonuses.insert(bonus->aggregate(), bonus);
                else
                    delete bonus;
            }

            foreach (JsonValue entry, value["extensions"].asArray()) {
                Extension *extension = m_gameData->extensions().value(entry.toInt());
                if (extension)
                    m_extensions.append(extension);
            }

            JsonValue config = value["config"];
            for (JsonValue::const_iterator i = config.begin(); i != config.end(); ++i) {
                AggregateField *aggregate = m_gameData->findByName<AggregateField *>(i.key().toLower());
                if (aggregate)
                    m_aggregates.insert(aggregate, i.value().toVariant());
            }

            JsonValue options = value["options"];

            m_tier = m_gameData->tiers().value(options["tier"].toString());
            foreach (JsonValue entry, options["slotFlags"].asArray())
                m_slotFlags.append(entry.toInt());

            m_capacity = options["capacity"].toFloat();
            m_moduleFlag = options["moduleFlag"].toInt();

            m_ammoType = m_gameData->categories().value(options["ammoType"].toUInt64());
            if (!m_ammoType->id())
                m_ammoType = 0;

            m_researchLevel = 0;
            m_calibrationProgram = 0;

            m_hidden = value["hidden"].toInt();
            m_inMarket = value["purchasable"].toInt(true);

            if (!m_hidden && m_inMarket)
                m_category->setInMarket(m_inMarket);
        }
        else {
            JsonValue options = value["options"];

            addAggregate(AggregateField::EI_Mass, m_mass);
            addAggregate(AggregateField::EI_Volume, m_volume);
            if (m_quantity > 1)
                addAggregate(AggregateField::EI_Quantity, m_quantity);

            if (options.contains("head")) {
                // we are the robots

                m_head = m_gameData->definitions().value(options["head"].toInt());
                m_chassis = m_gameData->definitions().value(options["chassis"].toInt());
                m_leg = m_gameData->definitions().value(options["leg"].toInt());
                Definition *inventory = m_gameData->definitions().value(options["inventory"].toInt());

                if (!m_head || !m_chassis || !m_leg || !inventory) {
                    qDebug() << "Invalid bot:" << m_name;
                    return false;
                }

                m_bonuses.unite(m_head->m_bonuses);
                m_bonuses.unite(m_chassis->m_bonuses);
                m_bonuses.unite(m_leg->m_bonuses);

                for (int i = 0; i < m_chassis->m_slotFlags.size(); ++i)
                    addAggregate(AggregateField::EI_ChassisSlot0 + i, m_chassis->m_slotFlags.at(i));
                addAggregate(AggregateField::EI_HeadSlots, m_head->m_slotFlags.size());
                addAggregate(AggregateField::EI_ChassisSlots, m_chassis->m_slotFlags.size());
                addAggregate(AggregateField::EI_LegSlots, m_leg->m_slotFlags.size());

                m_capacity = inventory->m_capacity;
                addAggregate(AggregateField::EI_Capacity, m_capacity);

                // TODO: detailed chassis slots
                //foreach (int slotFlag, chassis->m_slotFlags)
                //    m_aggregates.insertMulti(m_gameData->aggregateFields().value(AggregateField::EI_ChassisSlots, slotFlag);
            }

            if (hasCategory("cf_robot_equipment")) {
                addAggregate(AggregateField::EI_RepackedVolume, m_repackedVolume);
                addAggregate(AggregateField::EI_ActiveModule, (m_attributeFlags & Active) ? "active" : "passive");

                QMap<int, QString>::const_iterator i = m_gameData->slotTypes().constBegin();
                for (; i != m_gameData->slotTypes().constEnd(); ++i) {
                    if (m_moduleFlag & i.key()) {
                        addAggregate(AggregateField::EI_SlotType, i.value());
                        break; // do not handle multislot modules (because there are none)
                    }
                }

                int ammoCapacity = options["ammoCapacity"].toInt();
                if (ammoCapacity)
                    addAggregate(AggregateField::EI_AmmoCapacity, ammoCapacity);
            }

            if (m_tier)
                addAggregate(AggregateField::EI_Tier, m_tier->systemName());
            if ((m_attributeFlags & HasAmmo) && m_ammoType)
                addAggregate(AggregateField::EI_AmmoType, m_ammoType->systemName());
        }
    }
    else if (value.contains("icon")) {
        m_icon = value["icon"].toString();
    }
    else if (value.contains("copyfrom")) {
        if (m_icon.isEmpty()) {
            Definition *definition = m_gameData->definitions().value(value["copyfrom"].toInt());
            if (definition)
                m_icon = definition->m_icon;
        }
    }
    else if (value.contains("components")) {
        foreach (JsonValue entry, value["components"]) {
            Definition *component = m_gameData->definitions().value(entry["definition"].toInt());
            if (component)
                m_components.insert(component, entry["amount"].toInt());
        }
    }
    else if (value.contains("researchLevel")) {
        m_researchLevel = value["researchLevel"].toInt();
        m_calibrationProgram = m_gameData->definitions().value(value["calibrationProgram"].toInt());
    }
    return true;
}

QPixmap Definition::icon(int size, bool decorated) const
{
    QPixmap pixmap;
    if (!pixmap.load(QString(":/icons/%1.png").arg(m_icon.isEmpty() ? "noIconAvailable" : m_icon), 0, Qt::NoOpaqueDetection))
        qDebug() << "Missing icon: " << m_icon;

    if (decorated && (m_tier || slotFlag() || sizeFlag())) {
        QPixmap tierIcon, slotIcon, sizeIcon;

        if (m_tier)
            tierIcon = m_tier->icon(true);

        switch (slotFlag()) {
        case TurretSlot:
            slotIcon.load(":/slot-turret.png"); break;
        case MissileSlot:
            slotIcon.load(":/slot-missile.png"); break;
        case IndustrialSlot:
            slotIcon.load(":/slot-industrial.png"); break;
        case OtherSlot:
            slotIcon.load(":/slot-other.png"); break;
        case HeadSlot:
            slotIcon.load(":/slot-head.png"); break;
        case LegSlot:
            slotIcon.load(":/slot-leg.png"); break;
        }

        switch (sizeFlag()) {
        case Small:
            sizeIcon.load(":/slot-small.png"); break;
        case Medium:
            sizeIcon.load(":/slot-medium.png"); break;
        case Large:
            sizeIcon.load(":/slot-large.png"); break;
        }

        QPainter painter(&pixmap);
        painter.drawPixmap((pixmap.width() - tierIcon.width()) / 2, 5, tierIcon);
        int x = pixmap.width() - slotIcon.width() - 4;
        painter.drawPixmap(x, pixmap.height() - slotIcon.height() - 4, slotIcon);
        x -= slotIcon.width() - 1;
        painter.drawPixmap(x, pixmap.height() - sizeIcon.height() - 4, sizeIcon);
    }

    if (size != -1 && pixmap.width() != size)
        pixmap = pixmap.scaledToWidth(size, Qt::SmoothTransformation);

    return pixmap;
}

bool Definition::hasCategory(const QString &name) const
{
    Category *other = m_gameData->findByName<Category *>(name);
    return other && category()->hasCategory(other);
}


// CharacterWizardStep

bool CharacterWizardStep::load(const JsonValue &value)
{
    m_id = value["ID"].toInt();
    m_name = value["name"].toString();
    m_description = value["description"].toString();
    m_baseEID = value["baseEID"].toInt();

    foreach (JsonValue entry, value["extension"]) {
        Extension *extension = m_gameData->extensions().value(entry["extensionID"].toInt());
        if (extension)
            m_extensions.insert(extension, entry["add"].toInt());
    }

    if (value.contains("raceID"))
        m_baseStep = m_gameData->charWizSteps(GameData::Race).value(value["raceID"].toInt());
    else if (value.contains("schoolID"))
        m_baseStep = m_gameData->charWizSteps(GameData::School).value(value["schoolID"].toInt());

    return true;
}
