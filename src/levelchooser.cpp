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

#include <QApplication>
#include <QPainter>
#include <QVariant>
#include <QPainter>
#include <QPixmapCache>
#include <QTextDocument>
#include <QAbstractTextDocumentLayout>
#include <QTreeView>
#include <QPushButton>

#include "levelchooser.h"
#include "application.h"
#include "gamedata.h"


class LevelChooserLayout {
public:
    QRect bars, text, buttons[4];

    LevelChooserLayout(const QRect &base = QRect()) {
        QPoint t;
        if (!base.isNull())
            t = base.topLeft() + QPoint(base.width() - size().width(), base.height() - size().height()) / 2;

        bars = QRect(15+3, 0, 120, 14).translated(t);
        text = QRect(15+3, 14, 120, 10).translated(t);
        buttons[0] = QRect(0, 0, 15, 12).translated(t);
        buttons[1]= QRect(0, 12+1, 15, 12).translated(t);
        buttons[2] = QRect(15+120+6, 0, 15, 12).translated(t);
        buttons[3] = QRect(15+120+6, 12+1, 15, 12).translated(t);
    }

    static QSize size() { return QSize(30+120+6, 10+14+1); }
};



LevelChooserDelegate::LevelChooserDelegate(QTreeView *parent) : QStyledItemDelegate(parent), m_pressedButton(-1)
{
    m_doc = new QTextDocument(this);
    m_doc->setDefaultStyleSheet(qApp->docStyleSheet());
    m_doc->setDocumentMargin(0);

    QTextOption to = m_doc->defaultTextOption();
    to.setAlignment(Qt::AlignBottom | Qt::AlignLeft);
    to.setWrapMode(QTextOption::NoWrap);
    m_doc->setDefaultTextOption(to);

    m_currentButton = new QPushButton(parent);
    m_currentButton->setVisible(false);
    m_currentButton->setProperty("role", QLatin1String("level-button current"));

    m_plannedButton = new QPushButton(parent);
    m_plannedButton->setVisible(false);
    m_plannedButton->setProperty("role", QLatin1String("level-button planned"));

    m_pmBars.load(QLatin1String(":/bars.png"));
}

void LevelChooserDelegate::customEvent(QEvent *event)
{
    if (event->type() == Application::SettingsChanged)
        m_doc->setDefaultStyleSheet(qApp->docStyleSheet());
}

void LevelChooserDelegate::paintBars(QPainter *painter, const QRect &rect, int current, int planned) const
{
    int sectorWidth = m_pmBars.width() / 10, sectorHeight = m_pmBars.height() / 3;

    if (current > 0)
        painter->drawPixmap(rect.left(), rect.top(),
                            m_pmBars, 0, 0, current * sectorWidth, m_pmBars.height() / 3);

    if (planned > current)
        painter->drawPixmap(rect.left() + current * sectorWidth, rect.top(),
                            m_pmBars, current * sectorWidth, sectorHeight, (planned - current) * sectorWidth, m_pmBars.height() / 3);

    if (planned < 10)
        painter->drawPixmap(rect.left() + planned * sectorWidth, rect.top(),
                            m_pmBars, planned * sectorWidth, sectorHeight * 2, (10 - planned) * sectorWidth, m_pmBars.height() / 3);
}

void LevelChooserDelegate::paintText(QPainter *painter, const QRect &rect, int current, int planned) const
{
    m_doc->setTextWidth(rect.width() - 15);
    if (planned > current)
        m_doc->setHtml(QString("<span class=\"levelchooser\"><span class=\"current\">%1</span> :%2 &nbsp; %4: <span class=\"planned\">%3</span></span>")
                       .arg(current).arg(tr("CURRENT")).arg(planned).arg(tr("PLANNED")));
    else
        m_doc->setHtml(QString("<span class=\"levelchooser\"><span class=\"current\">%1</span> :%2</span>")
                       .arg(current).arg(tr("CURRENT")));

    painter->translate(rect.topLeft());
    QAbstractTextDocumentLayout::PaintContext context;
    m_doc->documentLayout()->draw(painter, context);
    painter->translate(-rect.topLeft());
}

void LevelChooserDelegate::paintButton(QPainter *painter, const QRect &rect, const QString &text, QPushButton *button) const
{
    QStyleOptionButton opt;
    opt.state = QStyle::State_Enabled;
    opt.rect = rect;
    opt.text = text;
    button->style()->drawControl(QStyle::CE_PushButton, &opt, painter, button);
}

void LevelChooserDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const
{
    QStyledItemDelegate::paint(painter, option, index);

    QVariant vc = index.data(Qt::UserRole+1);
    if (!vc.isValid())
        return;

    int c = vc.toInt();
    int p = index.data(Qt::UserRole+2).toInt();

    QPixmap pm;
    QString key = QString("levelchooser-%1-%2").arg(c).arg(p);
    if (!QPixmapCache::find(key, &pm)) {
        pm = QPixmap(LevelChooserLayout::size());
        pm.fill(Qt::transparent);
        QPainter pmPainter(&pm);

        LevelChooserLayout layout;
        paintBars(&pmPainter, layout.bars, c, p);
        paintText(&pmPainter, layout.text, c, p);
        if (c < 10) paintButton(&pmPainter, layout.buttons[0], QChar(L'+'), m_currentButton);
        if (c > 0) paintButton(&pmPainter, layout.buttons[1], QChar(L'\x2013'), m_currentButton);
        if (p < 10 && c < 10) paintButton(&pmPainter, layout.buttons[2], QChar(L'+'), m_plannedButton);
        if (p > 0 && c < 10) paintButton(&pmPainter, layout.buttons[3], QChar(L'\x2013'), m_plannedButton);

        QPixmapCache::insert(key, pm);
    }

    painter->drawPixmap(option.rect.topLeft() + QPoint(option.rect.width() - pm.width(), option.rect.height() - pm.height()) / 2, pm);
}

QSize LevelChooserDelegate::sizeHint(const QStyleOptionViewItem &, const QModelIndex &) const
{
    return LevelChooserLayout::size();
}

bool LevelChooserDelegate::editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index)
{
    // MSDN: Double-clicking the left mouse button actually generates
    // a sequence of four messages: WM_LBUTTONDOWN, WM_LBUTTONUP, WM_LBUTTONDBLCLK, and WM_LBUTTONUP.

    if (event->type() == QEvent::MouseButtonPress || event->type() == QEvent::MouseButtonDblClick || event->type() == QEvent::MouseButtonRelease) {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent *>(event);

        LevelChooserLayout layout(option.rect);
        for (int i = 0; i < 4; ++i) {
            if (layout.buttons[i].contains(mouseEvent->pos())) {
                if (mouseEvent->type() != QEvent::MouseButtonRelease) {
                    m_pressedButton = i;
                }
                else { // release
                    if (i == m_pressedButton) {
                        Qt::ItemDataRole role = static_cast<Qt::ItemDataRole>((i < 2) ? (Qt::UserRole + 1) : (Qt::UserRole + 2));
                        int change = (i % 2 == 0) ? 1 : -1;
                        model->setData(index, model->data(index, role).toInt() + change, role);
                    }
                    m_pressedButton = -1;
                }

                QItemSelection rowSelection(index.sibling(index.row(), 0), index.sibling(index.row(), model->columnCount() - 1));
                static_cast<QTreeView *>(parent())->selectionModel()->select(rowSelection, QItemSelectionModel::SelectCurrent);

                return true;
            }
        }
    }
    return false;
}
