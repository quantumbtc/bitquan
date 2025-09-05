// Copyright (c) 2011-2020 The Bitquantum Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITQUANTUM_QT_BITQUANTUMADDRESSVALIDATOR_H
#define BITQUANTUM_QT_BITQUANTUMADDRESSVALIDATOR_H

#include <QValidator>

/** Base58 entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class BitquantumAddressEntryValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitquantumAddressEntryValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

/** Bitquantum address widget validator, checks for a valid bitquantum address.
 */
class BitquantumAddressCheckValidator : public QValidator
{
    Q_OBJECT

public:
    explicit BitquantumAddressCheckValidator(QObject *parent);

    State validate(QString &input, int &pos) const override;
};

#endif // BITQUANTUM_QT_BITQUANTUMADDRESSVALIDATOR_H
