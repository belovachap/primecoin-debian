// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __CLIENTMODEL_H__
#define __CLIENTMODEL_H__

#include <QObject>


class AddressTableModel;
class TransactionTableModel;
class CWallet;

QT_BEGIN_NAMESPACE
class QDateTime;
class QTimer;
QT_END_NAMESPACE

enum BlockSource {
    BLOCK_SOURCE_NONE,
    BLOCK_SOURCE_REINDEX,
    BLOCK_SOURCE_DISK,
    BLOCK_SOURCE_NETWORK
};

/** Model for Primecoin network client. */
class ClientModel : public QObject
{
    Q_OBJECT

public:
    explicit ClientModel(QObject *parent = 0);
    ~ClientModel();

    int getNumConnections() const;
    int getNumBlocks() const;
    int getNumBlocksAtStartup();

    double getVerificationProgress() const;
    QDateTime getLastBlockDate() const;

    //! Return true if core is doing initial block download
    bool inInitialBlockDownload() const;
    //! Return true if core is importing blocks
    enum BlockSource getBlockSource() const;
    //! Return conservative estimate of total number of blocks, or 0 if unknown
    int getNumBlocksOfPeers() const;
    //! Return warnings to be displayed in status bar
    QString getStatusBarWarnings() const;

    QString formatFullVersion() const;
    QString clientName() const;
    QString formatClientStartupTime() const;

private:
    int cachedNumBlocks;
    int cachedNumBlocksOfPeers;
	bool cachedReindexing;
	bool cachedImporting;

    int numBlocksAtStartup;

    QTimer *pollTimer;

    void subscribeToCoreSignals();
    void unsubscribeFromCoreSignals();

signals:
    void numConnectionsChanged(int count);
    void numBlocksChanged(int count, int countOfPeers);

    //! Asynchronous message notification
    void message(const QString &title, const QString &message, unsigned int style);

public slots:
    void updateTimer();
    void updateNumConnections(int numConnections);
};

#endif // __CLIENTMODEL_H__
