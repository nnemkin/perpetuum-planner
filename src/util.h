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

#ifndef UTIL_H
#define UTIL_H

#include <QString>
#include <QBitArray>
#include <QCoreApplication>
#include <QMenu>

class QWidget;
class QTreeView;
class QHeaderView;


class Formatter {
    Q_DECLARE_TR_FUNCTIONS(Formatter)

public:
    static QString formatTimeSpan(int seconds);
    static QString formatTimeSpanCompact(int seconds);
};


int compareVersion(const QString &v1, const QString &v2);

QBitArray saveTreeState(QTreeView *tree);
void restoreTreeState(QTreeView *tree, const QBitArray &status, bool initialExpand = false);


#endif // UTIL_H
