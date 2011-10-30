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

#include <QTimer>

#include "attributeeditor.h"
#include "agent.h"


AttributeEditor::AttributeEditor(Agent *agent, QWidget *parent)
    : QDialog(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowSystemMenuHint | Qt::MSWindowsFixedSizeDialogHint),
      m_agent(agent)
{
    setupUi(this);

    connect(agent, SIGNAL(infoChanged()), this, SLOT(agentInfoChanged()));
    connect(agent, SIGNAL(persistenceChanged()), this, SLOT(agentPersistenceChanged()));
    connect(actionReset, SIGNAL(triggered()), agent, SLOT(resetInfo()));

    foreach (QButtonGroup *buttonGroup, findChildren<QButtonGroup *>())
        connect(buttonGroup, SIGNAL(buttonClicked(QAbstractButton*)), this, SLOT(buttonGroupClicked(QAbstractButton*)));

    QTimer::singleShot(0, this, SLOT(agentInfoChanged())); // wait for the final layout
}

void AttributeEditor::changeEvent(QEvent *e)
{
    QDialog::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);

        agentPersistenceChanged();
        agentInfoChanged();
        break;
    }
}

void AttributeEditor::showEvent(QShowEvent *)
{
    agentPersistenceChanged();
}

void AttributeEditor::agentPersistenceChanged()
{
    //: extension_attributes
    setWindowTitle(tr("%1 - Attributes").arg(m_agent->name()));
}

void AttributeEditor::agentInfoChanged()
{
    foreach (QToolButton *button, findChildren<QToolButton *>()) {
        QVariant vIndex = button->property("index");
        if (vIndex.isValid()) {
            int index = vIndex.toInt();
            QString value = button->property("value").toString();

            button->group()->setExclusive(false);
            button->setChecked(m_agent->starterChoices().mid(index, value.length()) == value);
            button->group()->setExclusive(true);

            if (index > 0 && index < 4) {
                QString buttonChoices = m_agent->starterChoices().left(index) + value;
                QString buttonName = m_agent->gameData()->starterName(buttonChoices);
                button->setToolTip(buttonName.isEmpty() ? tr("(Depends on previous choice)") : buttonName);
            }
        }
    }
}

void AttributeEditor::buttonGroupClicked(QAbstractButton *button)
{
    int index = button->property("index").toInt();
    QString value = button->property("value").toString();
    QString choices = m_agent->starterChoices().replace(index, value.length(), value);
    m_agent->setStarterChoices(choices);
}
