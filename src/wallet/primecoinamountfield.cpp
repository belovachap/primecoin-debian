// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#include <QApplication>
#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <qmath.h>

#include "guiconstants.h"
#include "primecoinunits.h"

#include "primecoinamountfield.h"


PrimecoinAmountField::PrimecoinAmountField(QWidget *parent):
    QWidget(parent),
    amount(0)
{
    QLabel *units = new QLabel(QString("XPM"), this);

    this->amount = new QDoubleSpinBox(this);
    this->amount->setDecimals(PrimecoinUnits::decimals(PrimecoinUnits::XPM));
    this->amount->setMaximum(qPow(10, PrimecoinUnits::amountDigits(PrimecoinUnits::XPM)) - qPow(10, -this->amount->decimals()));
    this->amount->installEventFilter(this);
    this->amount->setMaximumWidth(2000);
    this->amount->setSingleStep(1.0);

    QHBoxLayout *layout = new QHBoxLayout(this);
    layout->addWidget(amount);
    layout->addWidget(units);
    layout->addStretch(1);
    layout->setContentsMargins(0,0,0,0);

    this->setLayout(layout);
    this->setFocusPolicy(Qt::TabFocus);
    this->setFocusProxy(amount);
    this->connect(amount, SIGNAL(valueChanged(QString)), this, SIGNAL(textChanged()));
}

void PrimecoinAmountField::setText(const QString &text)
{
    if (text.isEmpty())
        amount->clear();
    else
        amount->setValue(text.toDouble());
}

void PrimecoinAmountField::clear()
{
    amount->clear();
}

bool PrimecoinAmountField::validate()
{
    bool valid = true;
    if (amount->value() == 0.0)
        valid = false;
    if (valid && !PrimecoinUnits::parse(PrimecoinUnits::XPM, text(), 0))
        valid = false;

    setValid(valid);

    return valid;
}

void PrimecoinAmountField::setValid(bool valid)
{
    if (valid)
        amount->setStyleSheet("");
    else
        amount->setStyleSheet(STYLE_INVALID);
}

QString PrimecoinAmountField::text() const
{
    if (amount->text().isEmpty())
        return QString();
    else
        return amount->text();
}

bool PrimecoinAmountField::eventFilter(QObject *object, QEvent *event)
{
    if (event->type() == QEvent::FocusIn) {
        // Clear invalid flag on focus
        setValid(true);
    }
    return QWidget::eventFilter(object, event);
}

QWidget *PrimecoinAmountField::setupTabChain(QWidget *prev)
{
    QWidget::setTabOrder(prev, amount);
    return amount;
}

qint64 PrimecoinAmountField::value(bool *valid_out) const
{
    qint64 val_out = 0;
    bool valid = PrimecoinUnits::parse(PrimecoinUnits::XPM, text(), &val_out);
    if(valid_out)
    {
        *valid_out = valid;
    }
    return val_out;
}

void PrimecoinAmountField::setValue(qint64 value)
{
    setText(PrimecoinUnits::format(PrimecoinUnits::XPM, value));
}
