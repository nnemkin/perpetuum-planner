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

#include <QMouseEvent>

#include "titlebar.h"
#include "util.h"


TitleBar::TitleBar(QWidget *parent) : QWidget(parent) {
    setAttribute(Qt::WA_StyledBackground);
    setupUi(this);
    buttonHelp->setVisible(false);

    connect(buttonHelp, SIGNAL(clicked()), this, SIGNAL(helpRequest()));
    connect(buttonMinimize, SIGNAL(clicked()), this, SIGNAL(showMinimizedRequest()));
    connect(buttonClose, SIGNAL(clicked()), this, SIGNAL(closeRequest()));

    label->setText(window()->windowTitle());
    window()->installEventFilter(this);
}

bool TitleBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::WindowTitleChange)
        label->setText(window()->windowTitle());

    return QWidget::eventFilter(watched, event);
}

void TitleBar::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        if (event->x() < height())
            emit closeRequest();
        else if (window()->windowState() & Qt::WindowMaximized)
            emit showNormalRequest();
        else
            emit showMaximizedRequest();
        event->accept();
    }
}

void TitleBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragPoint = event->globalPos() - window()->frameGeometry().topLeft();
        m_inDrag = true;
        event->accept();
    }
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    if (event->buttons() & Qt::LeftButton) {
        if (m_inDrag) {
            window()->move(event->globalPos() - m_dragPoint);
            event->accept();
        }
    }
}

void TitleBar::mouseReleaseEvent(QMouseEvent *event)
{
    m_inDrag = false;
    event->accept();
}
