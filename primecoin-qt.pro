TEMPLATE = app
TARGET = primecoin-qt
VERSION = 1.0.0
INCLUDEPATH += src src/qt
QT += network
DEFINES += QT_GUI BOOST_THREAD_USE_LIB BOOST_SPIRIT_THREADSAFE
CONFIG += no_include_pwd
CONFIG += thread

# Dependency library locations can be customized with:
#    BOOST_INCLUDE_PATH, BOOST_LIB_PATH, BDB_INCLUDE_PATH,
#    BDB_LIB_PATH, OPENSSL_INCLUDE_PATH and OPENSSL_LIB_PATH respectively

OBJECTS_DIR = build
MOC_DIR = build
UI_DIR = build

# use: qmake "USE_IPV6=1" ( enabled by default; default)
#  or: qmake "USE_IPV6=0" (disabled by default)
#  or: qmake "USE_IPV6=-" (not supported)
contains(USE_IPV6, -) {
    message(Building without IPv6 support)
} else {
    count(USE_IPV6, 0) {
        USE_IPV6=1
    }
    DEFINES += USE_IPV6=$$USE_IPV6
}

INCLUDEPATH += src/leveldb/include src/leveldb/helpers
LIBS += $$PWD/src/leveldb/libleveldb.a $$PWD/src/leveldb/libmemenv.a
# we use QMAKE_CXXFLAGS_RELEASE even without RELEASE=1 because we use RELEASE to indicate linking preferences not -O preferences
genleveldb.commands = cd $$PWD/src/leveldb && CC=$$QMAKE_CC CXX=$$QMAKE_CXX $(MAKE) OPT=\"$$QMAKE_CXXFLAGS $$QMAKE_CXXFLAGS_RELEASE\" libleveldb.a libmemenv.a
genleveldb.target = $$PWD/src/leveldb/libleveldb.a
genleveldb.depends = FORCE
PRE_TARGETDEPS += $$PWD/src/leveldb/libleveldb.a
QMAKE_EXTRA_TARGETS += genleveldb
# Gross ugly hack that depends on qmake internals, unfortunately there is no other way to do it.
QMAKE_CLEAN += $$PWD/src/leveldb/libleveldb.a; cd $$PWD/src/leveldb ; $(MAKE) clean

QMAKE_CXXFLAGS_WARN_ON = -fdiagnostics-show-option -Wall -Wextra -Wformat -Wformat-security -Wno-unused-parameter -Wstack-protector

# Input
HEADERS += src/qt/primecoingui.h \
    src/qt/transactiontablemodel.h \
    src/qt/addresstablemodel.h \
    src/qt/sendcoinsdialog.h \
    src/qt/addressbookpage.h \
    src/qt/editaddressdialog.h \
    src/qt/primecoinaddressvalidator.h \
    src/addrman.h \
    src/base58.h \
    src/bignum.h \
    src/compat.h \
    src/sync.h \
    src/util.h \
    src/hash.h \
    src/uint256.h \
    src/serialize.h \
    src/main.h \
    src/net.h \
    src/key.h \
    src/db.h \
    src/walletdb.h \
    src/script.h \
    src/init.h \
    src/bloom.h \
    src/mruset.h \
    src/checkqueue.h \
    src/qt/clientmodel.h \
    src/qt/guiutil.h \
    src/qt/transactionrecord.h \
    src/qt/guiconstants.h \
    src/qt/monitoreddatamapper.h \
    src/qt/transactiondesc.h \
    src/qt/transactiondescdialog.h \
    src/qt/primecoinamountfield.h \
    src/wallet.h \
    src/keystore.h \
    src/qt/transactionfilterproxy.h \
    src/qt/transactionview.h \
    src/qt/walletmodel.h \
    src/qt/walletview.h \
    src/qt/walletstack.h \
    src/qt/walletframe.h \
    src/qt/overviewpage.h \
    src/crypter.h \
    src/qt/sendcoinsentry.h \
    src/qt/qvalidatedlineedit.h \
    src/qt/primecoinunits.h \
    src/qt/askpassphrasedialog.h \
    src/protocol.h \
    src/qt/paymentserver.h \
    src/allocators.h \
    src/ui_interface.h \
    src/version.h \
    src/netbase.h \
    src/txdb.h \
    src/leveldb.h \
    src/limitedmap.h \
    src/prime.h

SOURCES += src/qt/primecoin.cpp \
    src/qt/primecoingui.cpp \
    src/qt/transactiontablemodel.cpp \
    src/qt/addresstablemodel.cpp \
    src/qt/sendcoinsdialog.cpp \
    src/qt/addressbookpage.cpp \
    src/qt/editaddressdialog.cpp \
    src/qt/primecoinaddressvalidator.cpp \
    src/version.cpp \
    src/sync.cpp \
    src/util.cpp \
    src/hash.cpp \
    src/netbase.cpp \
    src/key.cpp \
    src/script.cpp \
    src/main.cpp \
    src/init.cpp \
    src/net.cpp \
    src/bloom.cpp \
    src/addrman.cpp \
    src/db.cpp \
    src/walletdb.cpp \
    src/qt/clientmodel.cpp \
    src/qt/guiutil.cpp \
    src/qt/transactionrecord.cpp \
    src/qt/monitoreddatamapper.cpp \
    src/qt/transactiondesc.cpp \
    src/qt/transactiondescdialog.cpp \
    src/qt/primecoinamountfield.cpp \
    src/wallet.cpp \
    src/keystore.cpp \
    src/qt/transactionfilterproxy.cpp \
    src/qt/transactionview.cpp \
    src/qt/walletmodel.cpp \
    src/qt/walletview.cpp \
    src/qt/walletstack.cpp \
    src/qt/walletframe.cpp \
    src/qt/overviewpage.cpp \
    src/crypter.cpp \
    src/qt/sendcoinsentry.cpp \
    src/qt/qvalidatedlineedit.cpp \
    src/qt/primecoinunits.cpp \
    src/qt/askpassphrasedialog.cpp \
    src/protocol.cpp \
    src/qt/paymentserver.cpp \
    src/noui.cpp \
    src/leveldb.cpp \
    src/txdb.cpp \
    src/prime.cpp

RESOURCES += src/qt/primecoin.qrc

FORMS += src/qt/forms/sendcoinsdialog.ui \
    src/qt/forms/addressbookpage.ui \
    src/qt/forms/editaddressdialog.ui \
    src/qt/forms/transactiondescdialog.ui \
    src/qt/forms/overviewpage.ui \
    src/qt/forms/sendcoinsentry.ui \
    src/qt/forms/askpassphrasedialog.ui

contains(PRIMECOIN_QT_TEST, 1) {
SOURCES += src/qt/test/test_main.cpp \
    src/qt/test/uritests.cpp
HEADERS += src/qt/test/uritests.h
QT += testlib
TARGET = primecoin-qt_test
DEFINES += PRIMECOIN_QT_TEST
}

isEmpty(BOOST_THREAD_LIB_SUFFIX) {
    BOOST_THREAD_LIB_SUFFIX = $$BOOST_LIB_SUFFIX
}

# Set libraries and includes at end, to use platform-defined defaults if not overridden
INCLUDEPATH += $$BOOST_INCLUDE_PATH $$BDB_INCLUDE_PATH $$OPENSSL_INCLUDE_PATH
LIBS += $$join(BOOST_LIB_PATH,,-L,) $$join(BDB_LIB_PATH,,-L,) $$join(OPENSSL_LIB_PATH,,-L,)

LIBS += \
    -lssl \
    -lcrypto \
    -ldb_cxx \
    -lboost_system \
    -lboost_filesystem \
    -lboost_program_options \
    -lboost_thread
