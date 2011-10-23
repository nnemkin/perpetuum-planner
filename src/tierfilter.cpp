#include "tierfilter.h"


TierFilter::TierFilter(QWidget *parent) : QWidget(parent)
{
    setupUi(this);
}

void TierFilter::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    switch (e->type()) {
    case QEvent::LanguageChange:
        retranslateUi(this);
        break;
    default:
        break;
    }
}

void TierFilter::on_checkAllTiers_clicked(bool checked)
{
    foreach (QAbstractButton *button, groupTiers->buttons())
        button->setChecked(checked);
    emit changed(hiddenTiers());
}

void TierFilter::on_groupTiers_buttonClicked(QAbstractButton *)
{
    bool allChecked = true;
    foreach (QAbstractButton *button, groupTiers->buttons()) {
        if (!static_cast<QCheckBox *>(button)->isChecked()) {
            allChecked = false;
            break;
        }
    }
    checkAllTiers->setChecked(allChecked);
    emit changed(hiddenTiers());
}

QStringList TierFilter::hiddenTiers()
{
    QStringList result;
    foreach (QAbstractButton *checkBox, groupTiers->buttons()) {
        QString tierName = checkBox->property("tier").toString();
        if (!checkBox->isChecked())
            result << tierName;
    }
    return result;
}

void TierFilter::setHiddenTiers(const QStringList &tiers)
{
    bool allChecked = true;
    foreach (QAbstractButton *checkBox, groupTiers->buttons()) {
        QString tierName = checkBox->property("tier").toString();
        checkBox->setChecked(!tiers.contains(tierName));
        allChecked = allChecked && checkBox->isChecked();
    }
    checkAllTiers->setChecked(allChecked);
    emit changed(hiddenTiers());
}
