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

#ifndef LEVELCHOOSER_H
#define LEVELCHOOSER_H

#include <QStyledItemDelegate>


class QTreeView;
class QPushButton;
class QTextDocument;

class LevelChooserDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    LevelChooserDelegate(QTreeView *parent=0);

    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const;
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const;

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option, const QModelIndex &index);

protected:
    void customEvent(QEvent *);

private:
    QTextDocument *m_doc;
    QPushButton *m_currentButton, *m_plannedButton;

    QPixmap m_pmBars;

    int m_pressedButton;

    void paintBars(QPainter *painter, const QRect &rect, int current, int planned) const;
    void paintButton(QPainter *painter, const QRect &rect, const QString &text, QPushButton *button) const;
    void paintText(QPainter *painter, const QRect &rect, int current, int planned) const;
};


#endif // LEVELCHOOSER_H
