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

#ifndef GRIDTREEVIEW_H
#define GRIDTREEVIEW_H

#include <QTreeView>


class FastPopupTreeView : public QTreeView
{
    Q_OBJECT

public:
    explicit FastPopupTreeView(QWidget *parent = 0) : QTreeView(parent) {}

protected:
    void focusInEvent(QFocusEvent *event);
    void focusOutEvent(QFocusEvent *event);

private:
    void silentDisableUpdates(bool disable);

};


class GridTreeView : public FastPopupTreeView
{
    Q_OBJECT

public:
    explicit GridTreeView(QWidget *parent = 0) : FastPopupTreeView(parent) {}

    void paintEvent(QPaintEvent *event);
};

#endif // GRIDTREEVIEW_H
