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

#include <QPaintEvent>
#include <QPainter>
#include <QHeaderView>
#include <QScrollBar>

#include "gridtreeview.h"


// FastPopupTreeView

void FastPopupTreeView::focusInEvent(QFocusEvent *event)
{
    if (event->reason() < Qt::OtherFocusReason)
        silentDisableUpdates(true);
    QTreeView::focusInEvent(event);
    if (event->reason() < Qt::OtherFocusReason)
        silentDisableUpdates(false);
}

void FastPopupTreeView::focusOutEvent(QFocusEvent *event)
{
    silentDisableUpdates(true);
    QTreeView::focusOutEvent(event);
    silentDisableUpdates(false);
}

void FastPopupTreeView::silentDisableUpdates(bool disable)
{
    setAttribute(Qt::WA_UpdatesDisabled, disable);
    viewport()->setAttribute(Qt::WA_UpdatesDisabled, disable);
}


// GridTreeView

void GridTreeView::paintEvent(QPaintEvent *event)
{
    QTreeView::paintEvent(event);

    QStyleOptionViewItem opt = viewOptions();
    const int gridHint = style()->styleHint(QStyle::SH_Table_GridLineColor, &opt, this);
    const QColor gridColor = static_cast<QRgb>(gridHint);

    const QPoint offset = dirtyRegionOffset();
    const QVector<QRect> dirtyRects = event->region().translated(offset).rects();

    QPainter painter(viewport());
    painter.setPen(QPen(gridColor));

    foreach (const QRect &dirtyRect, dirtyRects) {
        // Paint each column
        for (int h = 0; h < header()->count(); ++h) {
            int col = header()->logicalIndex(h);
            if (!header()->isSectionHidden(col)) {
                int colPos = columnViewportPosition(col) + offset.x() + columnWidth(col) - 1; // 1 is gridWidth
                painter.drawLine(colPos, dirtyRect.top(), colPos, dirtyRect.bottom());
            }
        }
    }
}
