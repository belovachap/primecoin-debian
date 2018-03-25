// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __PRIMECOINADDRESSVALIDATOR_H__
#define __PRIMECOINADDRESSVALIDATOR_H__

#include <QValidator>

/** Base58 entry widget validator.
   Corrects near-miss characters and refuses characters that are not part of base58.
 */
class PrimecoinAddressValidator : public QValidator
{
    Q_OBJECT

public:
    explicit PrimecoinAddressValidator(QObject *parent = 0);

    State validate(QString &input, int &pos) const;

    static const int MaxAddressLength = 35;
};

#endif // __PRIMECOINADDRESSVALIDATOR_H__
