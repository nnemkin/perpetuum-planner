#ifndef TIERFILTER_H
#define TIERFILTER_H

#include <QPushButton>
#include "ui_tierfilter.h"


class TierFilter : public QWidget, private Ui::TierFilter
{
    Q_OBJECT

public:
    explicit TierFilter(QWidget *parent = 0);

    bool isActive() { return !checkAllTiers->isChecked(); }

    QStringList hiddenTiers();
    void setHiddenTiers(const QStringList &tiers);

signals:
    void changed(const QStringList &tiers);

private slots:
    void on_checkAllTiers_clicked(bool checked);
    void on_groupTiers_buttonClicked(QAbstractButton *button);

protected:
    void changeEvent(QEvent *e);
};

#endif // TIERFILTER_H
