# See COPYING for license.

TEMPLATE = app
TARGET = primecoin-qt
VERSION = 2.0.0
QT += network widgets
INCLUDEPATH += .
DEFINES += \
    BOOST_NO_CXX11_SCOPED_ENUMS \
    BOOST_SPIRIT_THREADSAFE \
    BOOST_THREAD_USE_LIB \
    USE_IPV6=1
CONFIG += no_include_pwd thread
QMAKE_CXXFLAGS += -std=c++11

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

QMAKE_CXXFLAGS_WARN_ON = -fdiagnostics-show-option -Wall -Wextra -Werror -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector

# Input
HEADERS += \
    primecoingui.h \
    transactiontablemodel.h \
    addresstablemodel.h \
    sendcoinsdialog.h \
    addressbookpage.h \
    editaddressdialog.h \
    primecoinaddressvalidator.h \
    base58.h \
    bignum.h \
    compat.h \
    sync.h \
    util.h \
    hash.h \
    uint256.h \
    serialize.h \
    main.h \
    net.h \
    network_peer.h \
    network_peer_database.h \
    network_peer_manager.h \
    key.h \
    db.h \
    walletdb.h \
    script.h \
    init.h \
    bloom.h \
    mruset.h \
    checkqueue.h \
    clientmodel.h \
    guiutil.h \
    transactionrecord.h \
    guiconstants.h \
    monitoreddatamapper.h \
    transactiondesc.h \
    transactiondescdialog.h \
    primecoinamountfield.h \
    wallet.h \
    keystore.h \
    transactionfilterproxy.h \
    transactionview.h \
    walletmodel.h \
    walletview.h \
    walletstack.h \
    walletframe.h \
    overviewpage.h \
    crypter.h \
    sendcoinsentry.h \
    qvalidatedlineedit.h \
    primecoinunits.h \
    askpassphrasedialog.h \
    protocol.h \
    paymentserver.h \
    allocators.h \
    ui_interface.h \
    version.h \
    netbase.h \
    txdb.h \
    leveldb.h \
    limitedmap.h \
    prime.h

SOURCES += \
    primecoin.cpp \
    primecoingui.cpp \
    transactiontablemodel.cpp \
    addresstablemodel.cpp \
    sendcoinsdialog.cpp \
    addressbookpage.cpp \
    editaddressdialog.cpp \
    primecoinaddressvalidator.cpp \
    version.cpp \
    sync.cpp \
    util.cpp \
    hash.cpp \
    netbase.cpp \
    key.cpp \
    script.cpp \
    main.cpp \
    network_peer.cpp \
    network_peer_database.cpp \
    network_peer_manager.cpp \
    init.cpp \
    net.cpp \
    bloom.cpp \
    db.cpp \
    walletdb.cpp \
    clientmodel.cpp \
    guiutil.cpp \
    transactionrecord.cpp \
    monitoreddatamapper.cpp \
    transactiondesc.cpp \
    transactiondescdialog.cpp \
    primecoinamountfield.cpp \
    wallet.cpp \
    keystore.cpp \
    transactionfilterproxy.cpp \
    transactionview.cpp \
    walletmodel.cpp \
    walletview.cpp \
    walletstack.cpp \
    walletframe.cpp \
    overviewpage.cpp \
    crypter.cpp \
    sendcoinsentry.cpp \
    qvalidatedlineedit.cpp \
    primecoinunits.cpp \
    askpassphrasedialog.cpp \
    protocol.cpp \
    paymentserver.cpp \
    noui.cpp \
    leveldb.cpp \
    txdb.cpp \
    prime.cpp

RESOURCES += primecoin.qrc

FORMS += \
    forms/sendcoinsdialog.ui \
    forms/addressbookpage.ui \
    forms/editaddressdialog.ui \
    forms/transactiondescdialog.ui \
    forms/overviewpage.ui \
    forms/sendcoinsentry.ui \
    forms/askpassphrasedialog.ui

contains(PRIMECOIN_QT_TEST, 1) {
SOURCES += \
    test/test_main.cpp \
    test/uritests.cpp
HEADERS += test/uritests.h
QT += testlib
TARGET = primecoin-qt_test
DEFINES += PRIMECOIN_QT_TEST
}

LIBS += \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_system \
    -lboost_thread \
    -lcrypto \
    -ldb_cxx \
    -lleveldb \
    -lmemenv \
    -lssl
