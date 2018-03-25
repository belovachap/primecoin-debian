// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __SENDCOINSENTRY_H__
#define __SENDCOINSENTRY_H__

#include <QFrame>


namespace Ui {
    class SendCoinsEntry;
}
class WalletModel;
class SendCoinsRecipient;

/** A single entry in the dialog for sending primecoins. */
class SendCoinsEntry : public QFrame
{
    Q_OBJECT

public:
    explicit SendCoinsEntry(QWidget *parent = 0);
    ~SendCoinsEntry();

    void setModel(WalletModel *model);
    bool validate();
    SendCoinsRecipient getValue();

    /** Return whether the entry is still empty and unedited */
    bool isClear();

    void setValue(const SendCoinsRecipient &value);
    void setAddress(const QString &address);

    /** Set up the tab chain manually, as Qt messes up the tab chain by default in some cases (issue https://bugreports.qt-project.org/browse/QTBUG-10907).
     */
    QWidget *setupTabChain(QWidget *prev);

    void setFocus();

public slots:
    void setRemoveEnabled(bool enabled);
    void clear();

signals:
    void removeEntry(SendCoinsEntry *entry);

private slots:
    void on_deleteButton_clicked();
    void on_payTo_textChanged(const QString &address);
    void on_addressBookButton_clicked();
    void on_pasteButton_clicked();

private:
    Ui::SendCoinsEntry *ui;
    WalletModel *model;
};

#endif // __SENDCOINSENTRY_H__
