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

using namespace std;


// AttributeSet

AttributeSet::AttributeSet()
{
    memset(m_attributes, 0, sizeof(m_attributes));
}

AttributeSet::AttributeSet(QVariant data)
{
    QVariantList dataList = data.toList();
    for (int i = 0; i < NumAttributes; ++i)
        m_attributes[i] = dataList.at(i).toInt();
}

AttributeSet::AttributeSet(const AttributeSet& other)
{
    memcpy(m_attributes, other.m_attributes, sizeof(m_attributes));
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

void AttributeSet::load(const QVariantMap &dataMap)
{
    for (int i = 0; i < NumAttributes; ++i)
        m_attributes[i] = dataMap.value(QString("attribute") + QChar('A' + i)).toInt();
}


// GameData

GameData::GameData(QObject *parent) : QObject(parent)
{
}

GameData::~GameData()
{
}

QVariantMap GameData::loadVariantMap(QIODevice *io)
{
    if (!io->isOpen())
        if (!io->open(QIODevice::ReadOnly))
            return QVariantMap();

    QDataStream stream(io);
    stream.setVersion(QDataStream::Qt_4_7);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream.setFloatingPointPrecision(QDataStream::SinglePrecision);

    QVariant data;
    stream >> data;
    return data.toMap();
}

template <class T> struct remove_pointer {};
template <class T> struct remove_pointer<T *> { typedef T type; };

template <class ObjectMap>
void GameData::loadObjects(ObjectMap& objectMap, const QVariantMap &dataMap, const QString &idKey)
{
    bool firstLoad = objectMap.isEmpty();

    foreach (QVariant entry, dataMap) {
        QVariantMap entryMap = entry.toMap();
        typename ObjectMap::key_type id = entryMap.value(idKey).value<typename ObjectMap::key_type>();

        typename ObjectMap::mapped_type object = objectMap.value(id);
        if (!object && firstLoad) {
            object = new remove_pointer<typename ObjectMap::mapped_type>::type(this);
            objectMap.insert(id, object);
        }
        if (object) {
            object->load(entryMap);
            if (firstLoad)
                m_objectsByName.insert(object->systemName(), object);
        }
    }
}

bool GameData::load(QIODevice *dataFile, QIODevice *translationFile)
{
    QVariantMap dataMap = loadVariantMap(dataFile);

    m_version = dataMap.value("version").toString();
    if (m_version.isEmpty())
        return false;

    QVariantMap custom = dataMap.value("_custom").toMap();

    // Note: QVariantMap forces string type and string sorting on keys, therefore loading order
    // may be different from the source file.

    loadObjects(m_extensionCategories, dataMap.value("extensionCategoryList").toMap(), "ID");
    loadObjects(m_extensions, dataMap.value("extensionGetAll").toMap(), "extensionID");
    loadObjects(m_extensions, dataMap.value("extensionPrerequireList").toMap(), "extensionID");

    loadObjects(m_categories, dataMap.value("categoryFlags").toMap(), "value");
    m_categories.insert(0, new Category(this)); // root category
    loadObjects(m_categories, dataMap.value("categoryFlags").toMap(), "value"); // resolve forward refs

    loadObjects(m_fieldCategories, custom.value("aggregateCategories").toMap(), "ID");
    loadObjects(m_aggregateFields, dataMap.value("getAggregateFields").toMap(), "ID");
    loadObjects(m_aggregateFields, custom.value("hiddenAggregates").toMap(), "ID");

    loadObjects(m_tiers, custom.value("tiers").toMap(), "name");

    loadObjects(m_definitions, dataMap.value("getEntityDefaults").toMap(), "definition");
    loadObjects(m_definitions, dataMap.value("getEntityDefaults").toMap(), "definition"); // resolve IDs in options
    loadObjects(m_definitions, dataMap.value("definitionProperties").toMap(), "definition");
    loadObjects(m_definitions, dataMap.value("definitionProperties").toMap(), "definition"); // resolve copyfrom's
    loadObjects(m_definitions, dataMap.value("productionComponentsList").toMap(), "definition");
    loadObjects(m_definitions, dataMap.value("getResearchLevels").toMap(), "definition");

    // Update market presence flags
    const QVariantMap &market = dataMap.value("marketAvailableItems").toMap();
    foreach (const QVariant &definitionId, market.value("definition").toList())
        if (Definition *definition = m_definitions.value(definitionId.toInt()))
            definition->setInMarket(true);

    foreach (const QVariant &categoryFlag, market.value("categoryflags").toList())
        m_categories.value(categoryFlag.toLongLong())->setInMarket(true);

    // Load character wizard steps
    QVariantMap charWiz = dataMap.value("characterWizardData").toMap();
    loadObjects(m_charWizSteps[Race], charWiz.value("race").toMap(), "ID");
    loadObjects(m_charWizSteps[School], charWiz.value("school").toMap(), "ID");
    loadObjects(m_charWizSteps[Major], charWiz.value("major").toMap(), "ID");
    loadObjects(m_charWizSteps[Corporation], charWiz.value("corporation").toMap(), "ID");
    loadObjects(m_charWizSteps[Spark], charWiz.value("spark").toMap(), "ID");

    // Build definition index in category tree order
    foreach (Definition *definition, m_definitions)
        m_definitionsByCategory.insertMulti(definition->category()->order(), definition);

    // Persistent names
    QVariantMap transMap = loadVariantMap(translationFile);
    if (transMap.isEmpty())
        return false;

    m_persistentNames.reserve(m_extensions.size() + 10);
    foreach (Extension *extension, m_extensions) {
        QString persistentName = transMap.value(extension->systemName()).toString();
        m_persistentNames.insert(extension->systemName(), persistentName);
        m_objectsByName.insert(persistentName, extension);
    }
    foreach (const QVariant &entry, custom.value("historicNames").toMap()) {
        QVariantMap &entryMap = entry.toMap();
        QString name = entryMap.value("name").toString();
        QString persistentName = entryMap.value("value").toString();
        m_persistentNames.insert(name, persistentName);
        m_objectsByName.insert(persistentName, findByName<Extension *>(name));
    }

    return true;
}

bool GameData::loadTranslation(const QString &languageCode, QIODevice *translationFile)
{
    QVariantMap transMap = loadVariantMap(translationFile);
    if (transMap.isEmpty())
        return false;

    m_translation.clear();
    m_translation.reserve(transMap.size());
    for (QVariantMap::const_iterator i = transMap.constBegin(); i != transMap.constEnd(); ++i)
        m_translation.insert(i.key(), i.value().toString());

    m_languageCode = languageCode;
    return true;
}

QVector<CharacterWizardStep *> GameData::decodeCharWizSteps(const QString &steps) const
{
    // This code relies on steps being ordered (by parent and mil/ind/soc) and having sequential ids.

    QVector<CharacterWizardStep *> result(NumCharWizSteps);

    QByteArray codes = steps.toAscii().leftJustified(6, '\x7f', true);
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
    return step ? (step->name() + QString(" (ID=%1)").arg(step->id())) : QString();
}

ExtensionLevelMap GameData::starterExtensions(QString choices) const
{
    ExtensionLevelMap extensions;
    foreach (CharacterWizardStep *step, decodeCharWizSteps(choices))
        if (step)
            extensions += step->extensions();
    return extensions;
}

AttributeSet GameData::starterAttributes(QString choices) const
{
    AttributeSet attributes;
    foreach (CharacterWizardStep *step, decodeCharWizSteps(choices))
        if (step)
            attributes += step->attributes();
    return attributes;
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

ExtensionLevelMap &ExtensionLevelMap::operator+=(const ExtensionLevelMap &other)
{
    for (const_iterator i = other.constBegin(); i != other.constEnd(); ++i) {
        iterator j = find(i.key());
        if (j != end())
            j.value() = qMax(j.value(), i.value());
        else
            insert(i.key(), i.value());
    }
    return *this;
}


// ExtensionCategory

void ExtensionCategory::load(const QVariantMap &dataMap)
{
    m_id = dataMap.value("ID").toInt();
    m_name = dataMap.value("name").toString();
}


// Extension

void Extension::load(const QVariantMap &dataMap)
{
    if (dataMap.contains("name")) {
        m_id = dataMap.value("extensionId").toInt();
        m_name = dataMap.value("name").toString();
        m_description = dataMap.value("description").toString();
        m_description = dataMap.value("description").toString();
        m_rank = dataMap.value("rank").toInt();
        m_price = dataMap.value("price").toInt();
        m_bonus = dataMap.value("bonus").toFloat();
        m_primaryAttribute = dataMap.value("learningAttributePrimary").toString().at(9).toLatin1() - 'A';
        m_secondaryAttribute = dataMap.value("learningAttributeSecondary").toString().at(9).toLatin1() - 'A';

        m_category = m_gameData->extensionCategories().value(dataMap.value("category").toInt());
        m_category->m_extensions.append(this);
    }
    else if (dataMap.contains("requiredExtension")) {
        m_requirements.insert(m_gameData->extensions().value(dataMap.value("requiredExtension").toInt()),
                              dataMap.value("requiredLevel").toInt());
    }
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
            m_transitiveReqs += i.key()->transitiveReqs();
    }
    return m_transitiveReqs;
}


// Category

void Category::load(const QVariantMap &dataMap)
{
    if (!m_id) {
        m_id = dataMap.value("value").toInt();
        m_name = dataMap.value("name").toString();
        m_order = _byteswap_uint64(m_id);
    }
    else {
        quint64 parentId = m_id;
        quint64 mask = 0xff;
        while (parentId & mask)
            mask <<= 8;
        parentId &= ~(mask >> 8);

        m_parent = m_gameData->categories().value(parentId);
        m_parent->m_categories.append(this);
    }
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

void FieldCategory::load(const QVariantMap &dataMap)
{
    m_id = dataMap.value("ID").toInt();
    m_name = dataMap.value("name").toString();
}


// AggregateField

void AggregateField::load(const QVariantMap &dataMap)
{
    if (dataMap.contains("name")) {
        m_id = dataMap.value("ID").toInt();
        m_name = dataMap.value("name").toString();
        m_unitName = dataMap.value("measurementUnit").toString();
        m_multiplier = dataMap.value("measurementMultiplier").toFloat();
        m_offset = dataMap.value("measurementOffset").toFloat();
        m_digits = dataMap.value("digits").toInt();
        m_category = m_gameData->fieldCategories().value(dataMap.value("category").toInt());
        m_category->m_aggregates.append(this);

        m_lessIsBetter = QRegExp("(blob_level)|(_time|_usage|_miss|signature_radius|blob_emission(_radius)?)$").indexIn(m_name) != -1;
    }
    else {
        m_hidden = dataMap.value("hidden").toBool();
    }
}

QString AggregateField::format(const QVariant &value) const
{
    QString sValue;
    if (value.type() == QVariant::String) {
        sValue = m_gameData->translate(value.toString());
    }
    else {
        float fValue = value.toFloat();
        switch (m_id) {
        case 318: // shield_absorbtion
            fValue = 1.f / fValue;
            break;
        case 326: // slope
            fValue = atan(fValue * 0.25f) / M_PI * 180.f;
            break;
        }
        sValue = QString::number(fValue * m_multiplier + m_offset, 'f', m_digits);

        if (!m_unitName.isEmpty())
            sValue = QString("%1 %2").arg(sValue, m_gameData->translate(m_unitName));
    }
    return sValue;
}


// Tier

void Tier::load(const QVariantMap &dataMap)
{
    m_id = dataMap.value("ID").toInt();
    m_name = dataMap.value("name").toString();
    m_color = dataMap.value("color").value<QColor>();
}

QPixmap Tier::icon() const
{
    QPixmap pixmap;
    QString key = QString("TierIcon-%1").arg(m_name);

    if (!QPixmapCache::find(key, pixmap)) {
        QPixmap bg(":/tier_bg_short.png");

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

Bonus::Bonus(GameData *gameData, const QVariantMap &dataMap)
{
    m_aggregate = gameData->aggregateFields().value(dataMap.value("aggregate").toInt());
    m_extension = gameData->extensions().value(dataMap.value("extensionID").toInt());
    m_bonus = dataMap.value("bonus").toFloat();
    m_effectEnhancer = dataMap.value("effectenhancer").toBool();
}

QString Bonus::format() const
{
    return QString("%1%").arg((int) fabs(floor(m_bonus * 100.f)));
}


// Definition

void Definition::load(const QVariantMap &dataMap)
{
    if (dataMap.contains("definitionname")) {
        if (!m_id) {
            m_id = dataMap.value("definition").toInt();
            m_name = dataMap.value("definitionname").toString();
            m_description = dataMap.value("descriptiontoken").toString();
            m_quantity = dataMap.value("quantity").toInt();

            m_attributeFlags = dataMap.value("attributeflags").toULongLong();

            m_categoryFlags = dataMap.value("categoryflags").toULongLong();
            m_category = m_gameData->categories().value(m_categoryFlags);
            if (!m_category)
                m_category = m_gameData->rootCategory();
            m_category->m_definitions.append(this);

            // m_hidden = dataMap.value("hidden").toBool();
            m_inMarket = dataMap.value("purchasable").toBool() || dataMap.value("market").toBool();

            // m_health = dataMap.value("health").toFloat();
            m_mass = dataMap.value("mass").toFloat();
            m_volume = dataMap.value("volume").toFloat();
            m_repackedVolume = dataMap.value("repackedvolume").toFloat();

            foreach (const QVariant &entry, dataMap.value("enablerExtension").toMap()) {
                QVariantMap entryMap = entry.toMap();
                m_enablerExtensions.insert(m_gameData->extensions().value(entryMap.value("extensionID").toInt()),
                                           entryMap.value("extensionLevel").toInt());
            }

            foreach (const QVariant &entry, dataMap.value("accumulator").toMap()) {
                QVariantMap entryMap = entry.toMap();
                m_aggregates.insert(m_gameData->aggregateFields().value(entryMap.value("ID").toInt()),
                                    entryMap.value("value").toFloat());
            }

            foreach (const QVariant &entry, dataMap.value("bonus").toMap()) {
                Bonus *bonus = new Bonus(m_gameData, entry.toMap());
                m_bonuses.insert(bonus->aggregate(), bonus);
            }

            foreach (const QVariant &entry, dataMap.value("extensions").toList())
                m_extensions.append(m_gameData->extensions().value(entry.toInt()));

            QVariantMap options = dataMap.value("options").toMap();
            m_tier = m_gameData->tiers().value(options.value("tier").toString());
            foreach (const QVariant &entry, options.value("slotFlags").toList())
                m_slotFlags.append(entry.toInt());

            m_moduleFlag = options.value("moduleFlag").toInt();

            m_researchLevel = 0;
            m_calibrationProgram = 0;
        }
        else {
            QVariantMap options = dataMap.value("options").toMap();

            if (options.contains("head")) {
                Definition *head = m_gameData->definitions().value(options.value("head").toInt());
                Definition *chassis = m_gameData->definitions().value(options.value("chassis").toInt());
                Definition *leg = m_gameData->definitions().value(options.value("leg").toInt());
                Definition *inventory = m_gameData->definitions().value(options.value("inventory").toInt());

                m_bonuses.unite(head->bonuses());
                m_bonuses.unite(chassis->bonuses());
                m_bonuses.unite(leg->bonuses());
            }
        }
    }
            /*
          simple float format (SFF):
          if (v >= 0.01 || value <= 0.0) "%.2f" else "%.5f", (name)_unit unit

          Not robot (no head)
            5, mass (all), SFF  + "(%d %s)"(quantity, repackagedvolume_unit) if quantity > 1
            5, volume (all), SFF + "(%d %s)"(quantity, repackagedvolume_unit) if quantity > 1
            cf_robot_equipment, cf_robot_components:
              5, repackedVolume, "%.2f %s", repackagedvolume_unit

          if options->containerId:
            5, container->options->capacity, containervolume (SFF)

          if options->ammoCapacity:
            5, options->ammoCapacity, ammocapacity (SFF)

          aggregates

          cf_robot_equipment:
            4, entityinfo_activemodule, (active, passive)[attributeFlags & 0x10]
            4, entityinfo_slottype, options->moduleFlags

          cf_robots:
            4, entityinfo_robot_slots_head, len(options->headId->slotFlags)
            foreach i, options->chassisId->slotFlags:
              4, %s #%d, entityinfo_robot_slots_chassis, i | comma-separated slottypes from slotFlags (/-separated slottypes)
            4, entityinfo_robot_slots_leg, len(options->legId->slotFlags)


          if (attributeFlags & 0x40000) // ammo
            4, entityinfo_ammotype, options->ammoType (categoryId)

          if options->tier
            4, entityinfo_tier, options->tier


            shield_absorbtion (318) is 1/value
            slope (326) is atan(value * 0.25) / 3.141592741012573 * 180.0;

        Options: moduleFlag (int), ammoCapacity (int), ammoType (defId), tier (str)
        slotFlags - array of int
        head, chassis, leg, inventory - bot components (defId)
        capacity - bot inventory
        */
    else if (dataMap.contains("icon")) {
        m_icon = dataMap.value("icon").toString();
    }
    else if (dataMap.contains("copyfrom")) {
        if (m_icon.isEmpty())
            m_icon = m_gameData->definitions().value(dataMap.value("copyfrom").toInt())->m_icon;
    }
    else if (dataMap.contains("components")) {
        foreach (const QVariant &entry, dataMap.value("components").toMap()) {
            QVariantMap entryMap = entry.toMap();
            m_components.insert(m_gameData->definitions().value(entryMap.value("definition").toInt()),
                                entryMap.value("amount").toInt());
        }
    }
    else if (dataMap.contains("researchLevel")) {
        m_researchLevel = dataMap.value("researchLevel").toInt();
        m_calibrationProgram = m_gameData->definitions().value(dataMap.value("calibrationProgram").toInt());
    }
}

QPixmap Definition::icon(int size, bool decorated) const
{
    QPixmap pixmap;
    if (!m_icon.isEmpty()) {
        QString key = QString("icon-%1-%2-%3").arg(m_icon).arg(size).arg(decorated);
        if (!QPixmapCache::find(key, &pixmap)) {
            pixmap.load(QString(":/icons/%1.png").arg(m_icon), 0, Qt::NoOpaqueDetection);
            if (size != -1 && pixmap.width() != size)
                pixmap = pixmap.scaledToWidth(size, Qt::SmoothTransformation);

            if (decorated) {
                // TODO
            }
            QPixmapCache::insert(key, pixmap);
        }
    }
    return pixmap;
}

bool Definition::hasCategory(const QString &name) const
{
    Category *other = m_gameData->findByName<Category *>(name);
    return other && category()->hasCategory(other);
}


// CharacterWizardStep

void CharacterWizardStep::load(const QVariantMap &dataMap)
{
    m_id = dataMap.value("ID").toInt();
    m_name = dataMap.value("name").toString();
    m_description = dataMap.value("description").toString();
    m_baseEID = dataMap.value("baseEID").toInt();

    foreach (const QVariant &entry, dataMap.value("extension").toMap()) {
        QVariantMap entryMap = entry.toMap();
        m_extensions.insert(m_gameData->extensions().value(entryMap.value("extensionID").toInt()),
                            entryMap.value("add").toInt());
    }

    m_attributes.load(dataMap);

    if (dataMap.contains("raceID"))
        m_baseStep = m_gameData->charWizSteps(GameData::Race).value(dataMap.value("raceID").toInt());
    else if (dataMap.contains("schoolID"))
        m_baseStep = m_gameData->charWizSteps(GameData::School).value(dataMap.value("schoolID").toInt());
}
