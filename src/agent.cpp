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

#include <QFile>
#include <QUndoCommand>
#include <QTime>
#include <QDebug>
#include <QFileInfo>

#include "parser.h"
#include "serializer.h"
#include "gamedata.h"
#include "agent.h"
#include "versions.h"


// ExtensionLevels

QList<Extension *> ExtensionLevels::extensions() const
{
    QList<Extension *> list = m_levels.keys();
    if (m_base)
        list.append(m_base->extensions());
    return list;
}

void ExtensionLevels::setBase(const ExtensionLevels *base)
{
    if (m_base)
        disconnect(m_base, 0, this, 0);
    m_base = base;
    if (m_base)
        connect(m_base, SIGNAL(levelChanged(Extension*,int)), this, SLOT(baseLevelChanged(Extension*,int)));

    baseLevelChanged(0, 0);
}

void ExtensionLevels::baseLevelChanged(Extension *extension, int level)
{
    if (extension) {
        if (m_levels.value(extension) <= level)
            m_levels.remove(extension);
    }
    else {
        foreach (Extension *extension, m_base->extensions())
            if (m_levels.value(extension) < m_base->level(extension))
                m_levels.remove(extension);
    }
    emit levelChanged(extension, level);
}

void ExtensionLevels::setLevel(Extension *extension, int lvl)
{
    lvl = qBound(0, lvl, 10);
    // add required extensions
    if (lvl > 0) {
        foreach (Extension *req, extension->requirements().extensions()) {
            int reqLevel = extension->requirements().value(req);
            if (reqLevel > level(req))
                setLevel(req, reqLevel);
        }
    }
    // drop extensions with broken requirements
    if (lvl < 10) {
        foreach (Extension *ext, m_levels.keys())
            if (ext->requirements().value(extension) > lvl)
                setLevel(ext, 0);
    }
    if (lvl > baseLevel(extension))
        m_levels.insert(extension, lvl);
    else
        m_levels.remove(extension);

    emit levelChanged(extension, lvl);
}

ExtensionLevelMap ExtensionLevels::requirements(Extension *extension) const
{
    Q_UNUSED(extension)

    ExtensionLevelMap reqs;
    /*
    foreach (Extension *reqExtension, extension->requirements()->extensions()) {
        int reqLevel = extension->requirements()->level(extension);
        if (reqLevel > level(reqExtension)) {
            reqs.insert(reqExtension, reqLevel);
        }
        reqs.unite(reqExtension->requirements()->checkRequirements(reqExtension));
    }
    */
    return reqs;
}

bool ExtensionLevels::load(GameData *gameData, const QVariantMap &dataMap)
{
    m_levels.clear();
    for (QVariantMap::const_iterator i = dataMap.constBegin(); i != dataMap.constEnd(); ++i) {
        Extension *extension = gameData->findByName<Extension *>(i.key());
        if (extension)
            m_levels.insert(extension, i.value().toInt());
        else
            qDebug() << "Unknown extension:" << i.key();
    }
    return true;
}

QVariantMap ExtensionLevels::save() const
{
    QVariantMap dataMap;
    for (ExtensionLevelMap::const_iterator i = m_levels.constBegin(); i != m_levels.constEnd(); ++i)
        if (i.value() > baseLevel(i.key()))
            dataMap.insert(i.key()->persistentName(), i.value());

    return dataMap;
}

int ExtensionLevels::points() const
{
    int total = m_base ? m_base->points() : 0;
    for (ExtensionLevelMap::const_iterator i = m_levels.constBegin(); i != m_levels.constEnd(); ++i) {
        total += points(i.key(), i.value());
        if (m_base)
            total -= m_base->points(i.key());
    }
    return total;
}

int ExtensionLevels::points(ExtensionCategory *category) const
{
    int total = 0;
    foreach (Extension *extension, category->extensions())
        total += points(extension);
    return total;
}

int ExtensionLevels::nextLevelPoints(Extension *extension, int lvl) const
{
    if (lvl < 0)
        lvl = level(extension);
    if (lvl >= 10)
        return 0;

    return points(extension, lvl + 1) - points(extension, lvl);
}

int ExtensionLevels::reqPoints(Extension *extension) const
{
    int total = 0;
    if (level(extension) == 0 && !extension->transitiveReqs().isEmpty()) {
        foreach (Extension *req, extension->transitiveReqs().extensions()) {
            int reqLevel = extension->transitiveReqs().value(req);
            int hasLevel = level(req);
            if (reqLevel > hasLevel)
                total += points(req, reqLevel) - points(req, hasLevel);
        }
    }
    return total;
}

int ExtensionLevels::points(Extension *extension, int lvl) const
{
    // increments are 1, 2, 3, 4, 5, 12, 21, 32, 45, 100
    static int multipliers[] = { 0, 1, 3, 6, 10, 15, 27, 48, 80, 125, 225 };

    if (lvl < 0)
        lvl = level(extension);
    if (lvl == 0)
        return 0;

    Agent *a = agent();
    int base = a ? (200 - 2 * a->attributes()[extension->primaryAttribute()] - a->attributes()[extension->secondaryAttribute()]) : 200;

    return base * extension->complexity() * multipliers[lvl] / 2;
}


// Agent

Agent::Agent(GameData *gameData, QObject *parent)
    : QObject(parent), m_gameData(gameData), m_starterChoices("      "), m_starterExtensions(0), m_modified(false)
{
    m_starterExtensions = new ExtensionLevels(this);
    m_currentExtensions = new ExtensionLevels(this);
    m_plannedExtensions = new ExtensionLevels(this);

    m_currentExtensions->setBase(m_starterExtensions);
    m_plannedExtensions->setBase(m_currentExtensions);

    connect(m_plannedExtensions, SIGNAL(levelChanged(Extension*,int)), this, SLOT(plannedExtensionsChanged()));
}

QString Agent::name()
{
    if (!m_fileName.isEmpty())
        return QFileInfo(m_fileName).baseName();
    return tr("New Agent");
}

void Agent::plannedExtensionsChanged()
{
    m_modified = true;
    emit persistenceChanged();
    emit extensionsChanged();
}

bool Agent::load(QIODevice *io)
{
    QJson::Parser parser;
    bool ok = true;

    QVariantMap data = parser.parse(io, &ok).toMap();
    if (!ok) {
        m_lastError = tr("Failed to load agent data.\nError on line %1: %2").arg(parser.errorLine()).arg(parser.errorString());
        return false;
    }

    setStarterChoices(data.value("starterChoices").toString());

    m_currentExtensions->load(m_gameData, data.value("currentExtensions").toMap());
    m_plannedExtensions->load(m_gameData, data.value("plannedExtensions").toMap());

    QFile *file = qobject_cast<QFile *>(io);
    m_fileName = file ? file->fileName() : QString();
    m_modified = false;
    emit persistenceChanged();

    return true;
}

bool Agent::load(const QString& fileName)
{
    QFile file(fileName);
    return load(&file);
}

bool Agent::save(QIODevice *io)
{
    QVariantMap dataMap;

    dataMap.insert("version", VER_PRODUCTVERSION_STR);
    dataMap.insert("starterChoices", m_starterChoices);
    dataMap.insert("currentExtensions", m_currentExtensions->save());
    dataMap.insert("plannedExtensions", m_plannedExtensions->save());

    QJson::Serializer serializer;
    bool ok = true;
    serializer.serialize(dataMap, io, &ok);
    if (!ok) {
        m_lastError = tr("Failed to save agent data.");
        return false;
    }

    QFile *file = qobject_cast<QFile* >(io);
    m_fileName = file ? file->fileName() : QString();
    m_modified = false;
    emit persistenceChanged();

    return true;
}

bool Agent::save(const QString& fileName)
{
    QString saveFileName = !fileName.isEmpty() ? fileName : m_fileName;
    if (saveFileName.isEmpty())
        return false;

    QFile file(saveFileName);
    return save(&file);
}

void Agent::setStarterChoices(const QString &choices)
{
    m_starterChoices = choices.leftJustified(6, QLatin1Char(' '), true);
    m_attributes = m_gameData->starterAttributes(choices);
    m_starterExtensions->setFrom(m_gameData->starterExtensions(choices));
    emit statsChanged();

    m_modified = true;
    emit persistenceChanged();
}

void Agent::resetAttributes()
{
    setStarterChoices(QString());
}

void Agent::reset()
{
    setStarterChoices(QString());
    m_plannedExtensions->reset();
    m_currentExtensions->reset();
    m_modified = false;
    m_fileName.clear();
    emit persistenceChanged();
}

QUndoCommand *Agent::setStarterChoicesCommand(const QString &choices)
{
    // return new StarterChoicesCommand(this, choices);
    Q_UNUSED(choices)
    return 0;
}

QUndoCommand *Agent::setLevelsCommand(const ExtensionLevelMap &levels)
{
    Q_UNUSED(levels)
    return 0;
}

void Agent::optimizeAttributes()
{
    int minPoints = m_plannedExtensions->points() - m_currentExtensions->points();
    QString minChoices = m_starterChoices;

    ExtensionLevels *starterExtensions = new ExtensionLevels(m_currentExtensions, this);
    ExtensionLevels *currentExtensions = new ExtensionLevels(m_currentExtensions, this);
    ExtensionLevels *plannedExtensions = new ExtensionLevels(m_plannedExtensions, this);

    currentExtensions->setBase(starterExtensions);
    plannedExtensions->setBase(currentExtensions);

    for (int i = 0; i < 3*27*9; ++i) {
        QString choices(QLatin1Char("TIA"[i % 3]));
        for (int j = 0, ii = i / 3; j < 5; ++j, ii /= 3)
            choices += QLatin1Char("MIL"[ii % 3]);

        m_attributes = m_gameData->starterAttributes(choices);
        starterExtensions->setFrom(m_gameData->starterExtensions(choices));

        int points = plannedExtensions->points() - currentExtensions->points();

        if (points < minPoints) {
            minPoints = points;
            minChoices = choices;
        }
    }

    delete starterExtensions;
    delete currentExtensions;
    delete plannedExtensions;

    if (minChoices != m_starterChoices)
        setStarterChoices(minChoices);
    else
        m_attributes = m_gameData->starterAttributes(m_starterChoices);
}


// Undo (not used)

class StarterChoicesCommand : public QUndoCommand {
public:
    enum { ID = 1001 };

    StarterChoicesCommand(Agent *agent, const QString &choices) : m_agent(agent), m_choices(choices), m_prevChoices(agent->starterChoices()) {}

    void redo() { m_agent->setStarterChoices(m_choices); }
    void undo() { m_agent->setStarterChoices(m_prevChoices); }

    bool mergeWith(const QUndoCommand *other)
    {
        if (other->id() == ID) {
            const StarterChoicesCommand* otherCommand = static_cast<const StarterChoicesCommand*>(other);

            if (otherCommand->m_choices == otherCommand->m_prevChoices)
                return true;
        }
        return false;
    }

private:
    Agent *m_agent;
    QString m_choices, m_prevChoices;
};


class LevelChangeCommand : public QUndoCommand {
public:
    enum { ID = 1002 };

    LevelChangeCommand(ExtensionLevels *extensionLevels, const ExtensionLevelMap &levels)
        : m_extensionLevels(extensionLevels), m_time(QTime::currentTime()), m_levels(levels)
    {
        for (ExtensionLevelMap::const_iterator i = levels.constBegin(); i != levels.constEnd(); ++i)
            m_prevLevels.insert(i.key(), extensionLevels->level(i.key()));
    }

    LevelChangeCommand(ExtensionLevels *extensionLevels, Extension *extension, int level)
        : m_extensionLevels(extensionLevels), m_time(QTime::currentTime())
    {
        m_levels.insert(extension, level);
        m_prevLevels.insert(extension, extensionLevels->level(extension));
    }

    int id() const { return ID; }

    void redo() { }
    void undo() { }

    bool mergeWith(const QUndoCommand *other)
    {
        if (other->id() == ID) {
            const LevelChangeCommand* otherCommand = static_cast<const LevelChangeCommand*>(other);

            if (otherCommand->m_levels.isEmpty())
                return true;

            if (m_time.msecsTo(otherCommand->m_time) < 1000) {
                for (ExtensionLevelMap::const_iterator i = otherCommand->m_levels.constBegin(); i != otherCommand->m_levels.constEnd(); ++i)
                    m_levels.insert(i.key(), i.value());

                for (ExtensionLevelMap::const_iterator i = otherCommand->m_prevLevels.constBegin(); i != otherCommand->m_prevLevels.constEnd(); ++i)
                    if (!m_prevLevels.contains(i.key()))
                        m_prevLevels.insert(i.key(), i.value());

                return true;
            }
        }
        return false;
    }

private:
    QTime m_time;
    ExtensionLevelMap m_levels, m_prevLevels;
    ExtensionLevels *m_extensionLevels;
};
