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

#ifndef TITLEBAR_H
#define TITLEBAR_H

#include "ui_titlebar.h"

class TitleBar : public QWidget, private Ui::TitleBar
{
    Q_OBJECT

public:
    explicit TitleBar(QWidget *parent = 0);

protected:
    void mouseDoubleClickEvent(QMouseEvent *);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);

    void resizeEvent(QResizeEvent *);
    bool eventFilter(QObject *, QEvent *);

private:
    QPoint m_dragPoint;
    bool m_inDrag;
    int m_titleBarHeight;
};

#endif // TITLEBAR_H
