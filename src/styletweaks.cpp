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

#include <QStyleFactory>
#include <QStyleOption>
#include <QPainter>
#include <QPixmapCache>
#include <QTreeView>
#include <QDebug>

#include "styletweaks.h"


StyleTweaks::StyleTweaks() : QProxyStyle(QStyleFactory::create("windows"))
{
}

void StyleTweaks::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == PE_FrameFocusRect)
        return; // do not draw the focus rect

    // Force full-width background on indented tree items
    if (element == PE_PanelItemViewRow) {
        if (const QStyleOptionViewItemV4 *vopt = qstyleoption_cast<const QStyleOptionViewItemV4 *>(option)) {
            if (!vopt->index.isValid() && !vopt->rect.isEmpty()) { // painting a ::branch background
                // call back to QStyleSheetStyle
                widget->style()->drawPrimitive(PE_PanelItemViewItem, vopt, painter, widget);
                return;
            }
        }
    }

    if (element == PE_Widget) {
        if (qobject_cast<const QAbstractScrollArea *>(widget)
                && widget->metaObject()->className() != QLatin1String("QComboBoxListView")) {
            QPixmap pm(":/frame-bg.png");
            painter->drawPixmap(option->rect.adjusted(1, 1, -1, -1), pm, pm.rect().adjusted(1, 1, -1, -1));
        }
    }

    QProxyStyle::drawPrimitive(element, option, painter, widget);
}

void StyleTweaks::drawControl(ControlElement element, const QStyleOption *option, QPainter *painter, const QWidget *widget) const
{
    if (element == CE_HeaderLabel) {
        // prevent vertically aligned text to go above the top line
        if (const QStyleOptionHeader *vopt = qstyleoption_cast<const QStyleOptionHeader *>(option)) {
            if (vopt->icon.isNull()) {
                if (vopt->state & QStyle::State_On) {
                    QFont fnt = painter->font();
                    fnt.setBold(true);
                    painter->setFont(fnt);
                }

                int textFlags = vopt->textAlignment | Qt::TextWordWrap;
                if (textFlags & Qt::AlignVCenter) {
                    QRect rect = option->rect;
                    rect = painter->fontMetrics().boundingRect(rect.left(), rect.top(), rect.width(), rect.height(),
                                                               textFlags, vopt->text);
                    if (rect.height() > option->rect.height())
                        textFlags = (textFlags & ~Qt::AlignVertical_Mask) | Qt::AlignTop;
                }

                drawItemText(painter, vopt->rect, textFlags, vopt->palette,
                             (vopt->state & State_Enabled), vopt->text, QPalette::ButtonText);
                return;
            }
        }
    }
    QProxyStyle::drawControl(element, option, painter, widget);
}

void StyleTweaks::drawItemText(QPainter *painter, const QRect &rect, int flags, const QPalette &pal,
                               bool enabled, const QString &text, QPalette::ColorRole textRole) const
{
    flags |= Qt::TextBypassShaping;
    QProxyStyle::drawItemText(painter, rect, flags, pal, enabled, text, textRole);
}
