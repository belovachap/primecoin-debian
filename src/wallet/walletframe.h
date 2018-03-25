// Copyright (c) 2011-2012 W.J. van der Laan
// Copyright (c) 2011-2013 The Bitcoin Developers
// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __WALLETFRAME_H__
#define __WALLETFRAME_H__

#include <QFrame>


class PrimecoinGUI;
class ClientModel;
class WalletModel;
class WalletStack;

class WalletFrame : public QFrame
{
    Q_OBJECT
public:
    explicit WalletFrame(PrimecoinGUI *_gui);
    ~WalletFrame();

    void setClientModel(ClientModel *clientModel);

    bool addWallet(const QString& name, WalletModel *walletModel);
    bool setCurrentWallet(const QString& name);

    void removeAllWallets();

    bool handleURI(const QString &uri);

    void showOutOfSyncWarning(bool fShow);

private:
    PrimecoinGUI *gui;
    ClientModel *clientModel;
    WalletStack *walletStack;

public slots:
    /** Switch to overview (home) page */
    void gotoOverviewPage();
    /** Switch to history (transactions) page */
    void gotoHistoryPage();
    /** Switch to address book page */
    void gotoAddressBookPage();
    /** Switch to receive coins page */
    void gotoReceiveCoinsPage();
    /** Switch to send coins page */
    void gotoSendCoinsPage(QString addr = "");

    /** Encrypt the wallet */
    void encryptWallet(bool status);
    /** Backup the wallet */
    void backupWallet();
    /** Change encrypted wallet passphrase */
    void changePassphrase();
    /** Ask for passphrase to unlock wallet temporarily */
    void unlockWallet();

    /** Set the encryption status as shown in the UI.
     @param[in] status            current encryption status
     @see WalletModel::EncryptionStatus
     */
    void setEncryptionStatus();
};

#endif // __WALLETFRAME_H__
