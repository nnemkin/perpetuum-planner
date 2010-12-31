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

#include <QTextOption>
#include <QTextLayout>
#include <QPainter>
#include <QHeaderView>
#include <qmath.h>
#include "private/qcommonstyle_p.h"
#include "private/qfixed_p.h"
#include "private/qheaderview_p.h"


/* Replacement for the uber-slow QCommonStyle text layout and rendering. */

void QCommonStylePrivate::viewItemDrawText(QPainter *painter, const QStyleOptionViewItemV4 *option, const QRect &rect) const
{
    Q_Q(const QCommonStyle);

    const int textMargin = q->pixelMetric(QStyle::PM_FocusFrameHMargin, 0, option->widget) + 1;
    const QRect textRect = rect.adjusted(textMargin, 0, -textMargin, 0); // remove width padding

    int textFlags = Qt::TextBypassShaping;
    textFlags |= QStyle::visualAlignment(option->direction, option->displayAlignment);
    if (option->features & QStyleOptionViewItemV2::WrapText)
        textFlags |= Qt::TextWordWrap;
    else if (!option->text.contains(QLatin1Char('\n')))
        textFlags |= Qt::TextSingleLine;

    painter->save();
    painter->setLayoutDirection(option->direction);
    painter->setFont(option->font);
    painter->drawText(textRect, textFlags, option->text);
    painter->restore();
}

QSize QCommonStylePrivate::viewItemSize(const QStyleOptionViewItemV4 *option, int role) const
{
    Q_Q(const QCommonStyle);

    const QWidget *widget = option->widget;
    switch (role) {
    case Qt::CheckStateRole:
        if (option->features & QStyleOptionViewItemV2::HasCheckIndicator)
            return QSize(q->pixelMetric(QStyle::PM_IndicatorWidth, option, widget),
                         q->pixelMetric(QStyle::PM_IndicatorHeight, option, widget));
        break;
    case Qt::DisplayRole:
        if (option->features & QStyleOptionViewItemV2::HasDisplay) {
            const int textMargin = q->pixelMetric(QStyle::PM_FocusFrameHMargin, option, widget) + 1;
            const bool wrapText = option->features & QStyleOptionViewItemV2::WrapText;

            if (!wrapText && !option->text.contains(QLatin1Char('\n'))) {
                // here comes the big speedup
                int width = option->fontMetrics.width(option->text, -1, Qt::TextBypassShaping);
                return QSize(width + 2*textMargin, option->fontMetrics.height());
            }

            QRect bounds = option->rect;
            switch (option->decorationPosition) {
            case QStyleOptionViewItem::Left:
            case QStyleOptionViewItem::Right:
                bounds.setWidth(wrapText && bounds.isValid() ? bounds.width() - 2 * textMargin : QFIXED_MAX);
                break;
            case QStyleOptionViewItem::Top:
            case QStyleOptionViewItem::Bottom:
                bounds.setWidth(wrapText ? option->decorationSize.width() : QFIXED_MAX);
                break;
            default:
                break;
            }
            int textFlags = Qt::TextBypassShaping;
            if (wrapText)
                textFlags |= Qt::TextWordWrap;
            else if (!option->text.contains(QLatin1Char('\n')))
                textFlags |= Qt::TextSingleLine;

            QSize size = option->fontMetrics.boundingRect(bounds, textFlags, option->text).size();
            size.rwidth() += 2 * textMargin;
            return size;
        }
        break;
    case Qt::DecorationRole:
        if (option->features & QStyleOptionViewItemV2::HasDecoration)
            return option->decorationSize;
        break;
    }

    return QSize(0, 0);
}


/* Tri-state QHeaderView sorting. */

enum SortSequence { BiState, TriStateAsc, TriStateDesc };

void QHeaderViewPrivate::flipSortIndicator(int section)
{
    Q_Q(QHeaderView);
    SortSequence sortSequence = static_cast<SortSequence>(q->parentWidget()->property("sortSequence").toInt());

    if (section == 0 && sortSequence == TriStateDesc)
        sortSequence = TriStateAsc;

    Qt::SortOrder order = Qt::AscendingOrder;
    if (sortIndicatorSection != section) {
        order = (sortSequence == TriStateDesc) ? Qt::DescendingOrder : Qt::AscendingOrder;
    }
    else if (sortSequence == TriStateDesc && sortIndicatorOrder == Qt::AscendingOrder
            || sortSequence == TriStateAsc && sortIndicatorOrder == Qt::DescendingOrder) {
        section = -1;
        order = Qt::AscendingOrder;
    }
    else {
        order = (sortIndicatorOrder == Qt::AscendingOrder) ? Qt::DescendingOrder : Qt::AscendingOrder;
    }
    q->setSortIndicator(section, order);
}
