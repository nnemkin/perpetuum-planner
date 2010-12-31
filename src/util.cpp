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

#include <QStringList>
#include <QStack>
#include <QTreeView>
#include <QHeaderView>

#include "util.h"


QString Formatter::formatTimeSpan(int seconds) {
    QString span;

    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;

    if (days) {
        span += QLatin1Char(' ') + tr("%n day(s)", 0, days);
    }
    if (hours) {
        span += QLatin1Char(' ') + tr("%n hour(s)", 0, hours);
    }
    if (minutes || !days && !hours) {
        span += QLatin1Char(' ') + tr("%n minute(s)", 0, minutes);
    }
    return span.mid(1);
}

QString Formatter::formatTimeSpanCompact(int seconds) {
    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;

    QString span;
    if (days) {
        span += QLatin1Char(' ') + tr("%nd", 0, days);
    }
    if (hours) {
        span += QLatin1Char(' ') + tr("%nh", 0, hours);
    }
    if (minutes || !days && !hours) {
        span += QLatin1Char(' ') + tr("%nm", 0, minutes);
    }
    return span.mid(1);
}


int compareVersion(const QString &v1, const QString &v2)
{
    if (v1.isEmpty())
        return v2.isEmpty() ? 0 : -1;
    else if (v2.isEmpty())
        return 1;

    QStringList v1parts = v1.split(QLatin1Char('.'));
    QStringList v2parts = v2.split(QLatin1Char('.'));

    for (int i = 0; i < qMin(v1parts.size(), v2parts.size()); ++i) {
        bool v1ok, v2ok;
        int cmp = v1parts.at(i).toInt(&v1ok) - v2parts.at(i).toInt(&v2ok);

        if (!v1ok)
            cmp = v2ok ? -1 : QString::compare(v1parts.at(i), v2parts.at(i));
        else if (!v2ok)
            cmp = 1;
        if (cmp)
            return cmp;
    }

    return v1parts.size() - v2parts.size();
}

static QModelIndexList parentIndexes(QAbstractItemModel *model)
{
    QModelIndexList parents;

    QModelIndexList stack;
    stack << QModelIndex();

    while (!stack.isEmpty()) {
        QModelIndex parent = stack.takeLast();
        int rowCount = model->rowCount(parent);
        for (int i = 0; i < rowCount; ++i)
            stack << model->index(i, 0, parent);
        if (rowCount)
            parents << parent;
    }
    return parents;
}

QBitArray saveTreeState(QTreeView *tree)
{
    QModelIndexList parents = parentIndexes(tree->model());
    QBitArray status(parents.size());
    for (int i = 0; i < parents.size(); ++i)
        status.setBit(i, tree->isExpanded(parents.at(i)));
    if (!status.count(true))
        status.clear();
    return status;
}

void restoreTreeState(QTreeView *tree, const QBitArray &status, bool initialExpand)
{
    if (!status.isEmpty()) {
        QModelIndexList parents = parentIndexes(tree->model());
        if (parents.size() == status.size())
            for (int i = 0; i < parents.size(); ++i)
                if (status.testBit(i))
                    tree->expand(parents.at(i));
    }
    else if (initialExpand) {
        tree->expandAll();
    }
}
