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

#ifndef EXTENSIONSVIEW_H
#define EXTENSIONSVIEW_H

#include <QAbstractItemModel>
#include <QPixmap>
#include <QMenu>

#include "models.h"
#include "ui_extensionsview.h"

class QMenu;
class QSettings;
class Agent;
class GameData;
class Extension;
class GameObject;
class ObjectGroup;
class ExtensionWindow;
class AttributeEditor;
class HeaderActions;


class ExtensionsModel : public SimpleTreeModel
{
    Q_OBJECT
public:
    explicit ExtensionsModel(Agent *agent, QObject *parent = 0);

    int columnCount(const QModelIndex &parent = QModelIndex()) const { Q_UNUSED(parent); return 6; }

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole);

    Qt::ItemFlags flags(const QModelIndex &index) const;

    bool event(QEvent *event);

private slots:
    void agentExtensionsChanged();

private:
    Agent *m_agent;

    void updateObjects();
    QPixmap createAttributesMarker(Extension *extension) const;
};


class ExtensionsView : public QWidget, private Ui::ExtensionsView
{
    Q_OBJECT

public:
    explicit ExtensionsView(QWidget *parent = 0);

    void initialize(QSettings &settings, GameData *gameData, const QString &fileName = QString());
    void finalize(QSettings &settings);

    bool eventFilter(QObject *, QEvent *);

    Agent *agent() { return m_agent; }

    bool canClose();

signals:
    void closeRequest();
    void windowTitleChange(const QString &newTitle);

protected:
    void changeEvent(QEvent *);
    void dragEnterEvent(QDragEnterEvent *);
    void dropEvent(QDropEvent *);
    void customEvent(QEvent *);

private slots:
    void on_actionSaveAs_triggered();
    void on_actionNew_triggered();
    void on_actionLoad_triggered();
    void on_actionSave_triggered();
    void on_actionInformation_triggered();
    void on_actionAttributeEditor_triggered();
    void on_treeExtensions_doubleClicked(QModelIndex index);
    void on_treeExtensions_customContextMenuRequested(QPoint pos);

    void agentStatsChanged();
    void agentExtensionsChanged();
    void agentPersistenceChanged();
    void actionSetLevelTriggered();

private:
    Agent *m_agent;

    AttributeEditor *m_attributeEditor;

    QMenu *m_popupCurrent;
    QMenu *m_popupPlanned;
    HeaderActions *m_headerActions;
};


class FixedFirstHeader : public QHeaderView {
    Q_OBJECT

public:
    FixedFirstHeader(QWidget *parent=0);

protected:
    void mousePressEvent(QMouseEvent *e);

private slots:
    void handleSectionMoved(int logicalIndex, int oldVisualIndex, int newVisualIndex);
};


class HeaderActions : public QMenu {
    Q_OBJECT

public:
    HeaderActions(QHeaderView *header, QWidget *parent=0);

private slots:
    void menuRequested(const QPoint &pos);
    void actionTriggered(bool checked);
    void actionResetTriggered();

private:
    QHeaderView *m_header;
};


#endif // EXTENSIONSVIEW_H
