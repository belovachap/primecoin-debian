// See COPYING for license.

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSplashScreen>
#include <QTextCodec>
#include <QTimer>
#include <QTranslator>

#include "clientmodel.h"
#include "guiconstants.h"
#include "guiutil.h"
#include "init.h"
#include "paymentserver.h"
#include "primecoingui.h"
#include "ui_interface.h"
#include "util.h"
#include "walletmodel.h"


// Declare meta types used for QMetaObject::invokeMethod
Q_DECLARE_METATYPE(bool*)

#ifndef PRIMECOIN_QT_TEST

// Need a global reference for the notifications to find the GUI
static PrimecoinGUI *guiref;
static QSplashScreen *splash_screen;

static bool ThreadSafeMessageBox(const std::string& message, const std::string& caption, unsigned int style)
{
    // Message from network thread
    if(guiref)
    {
        bool modal = (style & CClientUIInterface::MODAL);
        bool ret = false;
        // In case of modal message, use blocking connection to wait for user to click a button
        QMetaObject::invokeMethod(guiref, "message",
                                   modal ? GUIUtil::blockingGUIThreadConnection() : Qt::QueuedConnection,
                                   Q_ARG(QString, QString::fromStdString(caption)),
                                   Q_ARG(QString, QString::fromStdString(message)),
                                   Q_ARG(unsigned int, style),
                                   Q_ARG(bool*, &ret));
        return ret;
    }
    else
    {
        printf("%s: %s\n", caption.c_str(), message.c_str());
        fprintf(stderr, "%s: %s\n", caption.c_str(), message.c_str());
        return false;
    }
}

static bool ThreadSafeAskFee(int64 nFeeRequired)
{
    if(!guiref)
        return false;
    if(nFeeRequired < CTransaction::nMinTxFee || nFeeRequired <= nTransactionFee || fDaemon)
        return true;

    bool payFee = false;

    QMetaObject::invokeMethod(guiref, "askFee", GUIUtil::blockingGUIThreadConnection(),
                               Q_ARG(qint64, nFeeRequired),
                               Q_ARG(bool*, &payFee));

    return payFee;
}

static void InitMessage(const std::string &message)
{
    if(splash_screen)
    {
        splash_screen->showMessage(QString::fromStdString(message), Qt::AlignBottom|Qt::AlignHCenter, QColor(55,55,55));
        qApp->processEvents();
    }
    printf("init message: %s\n", message.c_str());
}

/* Handle runaway exceptions. Shows a message box with the problem and quits the program.
 */
static void handleRunawayException(std::exception *e)
{
    PrintExceptionContinue(e, "Runaway exception");
    QMessageBox::critical(0, "Runaway exception", PrimecoinGUI::tr("A fatal error occurred. Primecoin can no longer continue safely and will quit.") + QString("\n\n") + QString::fromStdString(strMiscWarning));
    exit(1);
}

int main(int argc, char *argv[])
{
    // Command-line options take precedence:
    ParseParameters(argc, argv);

    // Internal string conversion is all UTF-8
    QTextCodec::setCodecForTr(QTextCodec::codecForName("UTF-8"));
    QTextCodec::setCodecForCStrings(QTextCodec::codecForTr());

    Q_INIT_RESOURCE(primecoin);
    QApplication app(argc, argv);

    // Register meta types used for QMetaObject::invokeMethod
    qRegisterMetaType< bool* >();

    // Do this early as we don't want to bother initializing if we are just calling IPC
    // ... but do it after creating app, so QCoreApplication::arguments is initialized:
    if (PaymentServer::ipcSendCommandLine())
        exit(0);
    PaymentServer* paymentServer = new PaymentServer(&app);

    // Install global event filter that makes sure that long tooltips can be word-wrapped
    app.installEventFilter(new GUIUtil::ToolTipToRichTextFilter(TOOLTIP_WRAP_THRESHOLD, &app));

    // ... then primecoin.conf:
    if (!boost::filesystem::is_directory(GetDataDir(false)))
    {   
        QMessageBox::critical(0, "Primecoin",
                              QString("Error: Specified data directory \"%1\" does not exist.").arg(QString::fromStdString(mapArgs["-datadir"])));
        return 1;
    }
    ReadConfigFile(mapArgs, mapMultiArgs);

    QApplication::setOrganizationName("Chapman Shoop");
    QApplication::setOrganizationDomain("https://github.com/belovachap/primecoin-debian/tree/release-v1.0");
    QApplication::setApplicationName("Primecoin Debian 7");

    // Subscribe to global signals from core
    uiInterface.ThreadSafeMessageBox.connect(ThreadSafeMessageBox);
    uiInterface.ThreadSafeAskFee.connect(ThreadSafeAskFee);
    uiInterface.InitMessage.connect(InitMessage);

    // Show help message and exit.
    if (mapArgs.count("--help")) {
        GUIUtil::HelpMessageBox help;
        help.showOrPrint();
        return 1;
    }

    splash_screen = new QSplashScreen(QPixmap(":/images/splash_primecoin"));
    splash_screen->show();
    splash_screen->setAutoFillBackground(true);

    app.processEvents();
    app.setQuitOnLastWindowClosed(true);

    try
    {
        boost::thread_group threadGroup;

        PrimecoinGUI window;
        guiref = &window;

        QTimer* pollShutdownTimer = new QTimer(guiref);
        QObject::connect(pollShutdownTimer, SIGNAL(timeout()), guiref, SLOT(detectShutdown()));
        pollShutdownTimer->start(200);

        if(AppInit2(threadGroup))
        {
            {
                // Put this in a block, so that the Model objects are cleaned up before
                // calling Shutdown().
                if (splash_screen) {
                    splash_screen->finish(&window);
                }

                ClientModel clientModel;
                WalletModel walletModel(pwalletMain);

                window.setClientModel(&clientModel);
                window.addWallet("~Default", &walletModel);
                window.setCurrentWallet("~Default");

                window.show();

                // Now that initialization/startup is done, process any command-line
                // primecoin: URIs
                QObject::connect(paymentServer, SIGNAL(receivedURI(QString)), &window, SLOT(handleURI(QString)));
                QTimer::singleShot(100, paymentServer, SLOT(uiReady()));

                app.exec();

                window.hide();
                window.setClientModel(0);
                window.removeAllWallets();
                guiref = 0;
            }
            // Shutdown the core and its threads, but don't exit primecoin-qt here
            threadGroup.interrupt_all();
            threadGroup.join_all();
            Shutdown();
        }
        else
        {
            threadGroup.interrupt_all();
            threadGroup.join_all();
            Shutdown();
            return 1;
        }
    } catch (std::exception& e) {
        handleRunawayException(&e);
    } catch (...) {
        handleRunawayException(NULL);
    }
    return 0;
}
#endif // PRIMECOIN_QT_TEST
