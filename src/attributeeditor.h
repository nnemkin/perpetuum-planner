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

#ifndef ATTRIBUTEEDITOR_H
#define ATTRIBUTEEDITOR_H

#include "ui_attributeeditor.h"


class Agent;

class AttributeEditor : public QDialog, private Ui::AttributeEditor
{
    Q_OBJECT

public:
    explicit AttributeEditor(Agent *agent, QWidget *parent = 0);

    Agent* agent() { return m_agent; }

protected:
    void showEvent(QShowEvent *);
    void changeEvent(QEvent *);

private slots:
    void buttonGroupClicked(QAbstractButton *);
    void agentInfoChanged();
    void agentPersistenceChanged();

private:
    Agent* m_agent;
};

#endif // ATTRIBUTEEDITOR_H
