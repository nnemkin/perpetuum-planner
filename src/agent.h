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

#ifndef AGENT_H
#define AGENT_H

#include <QHash>
#include <QVariant>
#include "gamedata.h"

class QIODevice;
class QUndoCommand;
class ExtensionLevels;


class Agent : public QObject {
    Q_OBJECT

public:
    Agent(GameData *gameData, QObject *parent);

    bool load(QIODevice *io);
    bool load(const QString& fileName);
    bool importHistory(QIODevice *io);
    bool importHistory(const QString& fileName);
    bool save(QIODevice *io);
    bool save(const QString& fileName = QString());
    QString lastError() const { return m_lastError; }

    GameData *gameData() { return m_gameData; }

    QString starterChoices() { return m_starterChoices; }
    void setStarterChoices(const QString &choices);

    QUndoCommand *setStarterChoicesCommand(const QString &choices);
    QUndoCommand *setLevelsCommand(const ExtensionLevelMap &levels);

    const ExtensionLevels *starterExtensions() const { return m_starterExtensions; }
    ExtensionLevels *currentExtensions() { return m_currentExtensions; }
    ExtensionLevels *plannedExtensions() { return m_plannedExtensions; }

    bool isModified() { return m_modified; }
    QString fileName() { return m_fileName; }

    QString name();

public slots:
    void resetInfo();
    void reset();

signals:
    void infoChanged();
    void extensionsChanged();
    void persistenceChanged();

private slots:
    void plannedExtensionsChanged();

private:
    GameData *m_gameData;
    QString m_lastError;

    QString m_starterChoices;

    ExtensionLevels *m_starterExtensions;
    ExtensionLevels *m_currentExtensions;
    ExtensionLevels *m_plannedExtensions;

    bool m_modified;
    QString m_fileName;
};


class ExtensionLevels : public QObject {
    Q_OBJECT

public:
    ExtensionLevels(Agent *agent) : QObject(agent), m_base(0) {}
    ExtensionLevels(const ExtensionLevels* other, Agent *agent) : QObject(agent), m_base(other->m_base), m_levels(other->m_levels) {}

    Agent *agent() const { return static_cast<Agent *>(parent()); }

    QList<Extension *> extensions() const;

    void setFrom(const ExtensionLevelMap &extensions) { m_levels = extensions; emit levelChanged(0, 0); }

    int level(Extension *extension) const { return qMax(m_levels.value(extension), baseLevel(extension)); }
    void setLevel(Extension *extension, int level);

    ExtensionLevelMap requirements(Extension *extension) const;

    int points() const;
    int points(ExtensionCategory *category) const;
    int points(Extension *extension, int lvl=-1) const;
    int nextLevelPoints(Extension *extension, int lvl=-1) const;
    int reqPoints(Extension *extension) const;

    void setBase(const ExtensionLevels *base);

    bool load(GameData *gameData, const QVariantMap &dataMap);
    QVariantMap save() const;

signals:
    void levelChanged(Extension *extension, int level);

public slots:
    void reset() { m_levels.clear(); emit levelChanged(0, 0); }

private slots:
    void baseLevelChanged(Extension *extension, int level);

private:
    const ExtensionLevels *m_base;
    ExtensionLevelMap m_levels;

    int baseLevel(Extension *extension) const { return m_base ? m_base->level(extension) : 0; }
};


#endif // AGENT_H
