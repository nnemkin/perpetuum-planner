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

#include <QPainter>
#include <QTextFrame>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QPixmapCache>
#include <QMouseEvent>
#include <QTreeView>
#include <QStyleOptionViewItemV4>
#include <QStaticText>
#include <QDebug>

#include "application.h"
#include "delegates.h"


// ForegroundFixDelegate

void ForegroundFixDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);

    QVariant vForeground = index.data(Qt::ForegroundRole);
    if (qVariantCanConvert<QBrush>(vForeground))
        option->palette.setBrush(QPalette::HighlightedText, qvariant_cast<QBrush>(vForeground));
}


// SortableRowDelegate

void SortableRowDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
}

void SortableRowDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QVariant vOrder = index.data(SortOrderRole);
    if (vOrder.isValid()) {
        QPixmap pixmap;
        switch (vOrder.toInt()) {
        case Qt::AscendingOrder:
            pixmap.load(":/row-right-arrow.png");
            break;
        case Qt::DescendingOrder:
            pixmap.load(":/row-left-arrow.png");
            break;
        }
        if (!pixmap.isNull()) {
            int x = option.rect.right() - pixmap.width() - 3;
            int y = option.rect.top() + (option.rect.height() - pixmap.height()) / 2;
            painter->drawPixmap(x, y, pixmap);
        }
    }
}


// RichTextDelegate

RichTextDelegate::RichTextDelegate(QTreeView *parent) : QStyledItemDelegate(parent)
{
    m_doc = new QTextDocument(this);
    m_doc->setUndoRedoEnabled(false);
    m_doc->setDefaultStyleSheet(qApp->docStyleSheet());
}

void RichTextDelegate::initStyleOption(QStyleOptionViewItem *option, const QModelIndex &index) const
{
    QStyledItemDelegate::initStyleOption(option, index);
    option->decorationPosition = QStyleOptionViewItem::Right;
}

void RichTextDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    if (!opt.text.contains(QLatin1Char('<'))) {
        QStyledItemDelegate::paint(painter, option, index);
        return;
    }

    QStyle *style = opt.widget ? opt.widget->style() : QApplication::style();

    QString text = opt.text;
    opt.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, opt.widget);

    QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &opt, opt.widget);

    QString key = QLatin1String("RichTextDelegate-") + text;
    QPixmap pm;
    if (!QPixmapCache::find(key, &pm)) {
        m_doc->setDefaultFont(opt.font);

        QTextOption textOpt = m_doc->defaultTextOption();
        textOpt.setAlignment(QStyle::visualAlignment(opt.direction, opt.displayAlignment));
        bool wrapText = opt.features & QStyleOptionViewItemV2::WrapText;
        textOpt.setWrapMode(wrapText ? QTextOption::WordWrap : QTextOption::ManualWrap);
        textOpt.setTextDirection(opt.direction);
        m_doc->setDefaultTextOption(textOpt);

        QTextFrameFormat fmt = m_doc->rootFrame()->frameFormat();
        fmt.setMargin(0);
        m_doc->rootFrame()->setFrameFormat(fmt);

        m_doc->setTextWidth(textRect.width());
        m_doc->setHtml(text);

        QAbstractTextDocumentLayout::PaintContext context;
        context.palette = opt.palette;
        context.palette.setColor(QPalette::Text, context.palette.light().color());

        pm = QPixmap(m_doc->size().width(), m_doc->size().height());
        pm.fill(Qt::transparent);
        QPainter pmPainter(&pm);
        m_doc->documentLayout()->draw(&pmPainter, context);

        QPixmapCache::insert(key, pm);
    }

    int hDelta = textRect.width() - pm.width();
    int vDelta = textRect.height() - pm.height();
    int x = textRect.left();
    int y = textRect.top();

    if (opt.displayAlignment & Qt::AlignVCenter)
        y += vDelta / 2;
    else if (opt.displayAlignment & Qt::AlignBottom)
        y += vDelta;

    if (opt.displayAlignment & Qt::AlignHCenter)
        x += hDelta / 2;
    else if (opt.displayAlignment & Qt::AlignRight)
        x += hDelta;

    painter->drawPixmap(x, y, pm);
}

QSize RichTextDelegate::sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyleOptionViewItemV4 opt = option;
    initStyleOption(&opt, index);

    if (!opt.text.contains(QLatin1Char('<')))
        return QStyledItemDelegate::sizeHint(option, index);

    QVariant vSizeHint = index.data(Qt::SizeHintRole);
    if (vSizeHint.isValid())
        return vSizeHint.value<QSize>();

    QVariant vSize = index.data(Qt::SizeHintRole);
    if (vSize.isValid())
        return vSize.value<QSize>();

    QString key = QLatin1String("RichTextDelegate-") + opt.text;
    QPixmap pm;
    if (QPixmapCache::find(key, &pm)) {
        return pm.size();
    }
    else {
        m_doc->setDefaultFont(opt.font);
        m_doc->setHtml(opt.text);
        return QSize(m_doc->idealWidth(), m_doc->size().height());
    }
}

void RichTextDelegate::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged)
        m_doc->setDefaultStyleSheet(qApp->docStyleSheet());
}

bool RichTextDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);
        if (mouseEvent->button() == Qt::LeftButton && mouseEvent->x() - option.rect.left() < 18 && model->hasChildren(index)) {
            QTreeView *tree = static_cast<QTreeView *>(parent());
            tree->setExpanded(index, !tree->isExpanded(index));
            return true;
        }
    }
    return false;
}
