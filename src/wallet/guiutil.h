// Copyright (c) 2018 Chapman Shoop
// See COPYING for license.

#ifndef __GUIUTIL_H__
#define __GUIUTIL_H__

#include <QString>
#include <QObject>
#include <QMessageBox>

class SendCoinsRecipient;

QT_BEGIN_NAMESPACE
class QFont;
class QLineEdit;
class QWidget;
class QDateTime;
class QUrl;
class QAbstractItemView;
QT_END_NAMESPACE

/** Utility functions used by the Primecoin Qt UI.
 */
namespace GUIUtil
{
    // Create human-readable string from date
    QString dateTimeStr(const QDateTime &datetime);
    QString dateTimeStr(qint64 nTime);

    // Render Primecoin addresses in monospace font
    QFont primecoinAddressFont();

    // Set up widgets for address and amounts
    void setupAddressWidget(QLineEdit *widget, QWidget *parent);
    void setupAmountWidget(QLineEdit *widget, QWidget *parent);

    // Parse "primecoin:" URI into recipient object, return true on successful parsing.
    // See Bitcoin URI definition discussion here: https://bitcointalk.org/index.php?topic=33490.0
    bool parsePrimecoinURI(const QUrl &uri, SendCoinsRecipient *out);
    bool parsePrimecoinURI(QString uri, SendCoinsRecipient *out);

    // HTML escaping for rich text controls
    QString HtmlEscape(const QString& str, bool fMultiLine=false);
    QString HtmlEscape(const std::string& str, bool fMultiLine=false);

    /** Copy a field of the currently selected entry of a view to the clipboard. Does nothing if nothing
        is selected.
       @param[in] column  Data column to extract from the model
       @param[in] role    Data role to extract from the model
       @see  TransactionView::copyLabel, TransactionView::copyAmount, TransactionView::copyAddress
     */
    void copyEntryData(QAbstractItemView *view, int column, int role=Qt::EditRole);

    /** Get connection type to call object slot in GUI thread with invokeMethod. The call will be blocking.

       @returns If called from the GUI thread, return a Qt::DirectConnection.
                If called from another thread, return a Qt::BlockingQueuedConnection.
    */
    Qt::ConnectionType blockingGUIThreadConnection();

    // Determine whether a widget is hidden behind other windows
    bool isObscured(QWidget *w);

    // Open debug.log
    void openDebugLogfile();

    /** Qt event filter that intercepts ToolTipChange events, and replaces the tooltip with a rich text
      representation if needed. This assures that Qt can word-wrap long tooltip messages.
      Tooltips longer than the provided size threshold (in characters) are wrapped.
     */
    class ToolTipToRichTextFilter : public QObject
    {
        Q_OBJECT

    public:
        explicit ToolTipToRichTextFilter(int size_threshold, QObject *parent = 0);

    protected:
        bool eventFilter(QObject *obj, QEvent *evt);

    private:
        int size_threshold;
    };

    /** Help message for primecoin-qt, shown with --help. */
    class HelpMessageBox : public QMessageBox
    {
        Q_OBJECT

    public:
        HelpMessageBox(QWidget *parent = 0);

        /** Show message box or print help message to standard output, based on operating system. */
        void showOrPrint();

        /** Print help message to console */
        void printToConsole();

    private:
        QString header;
        QString coreOptions;
    };

} // namespace GUIUtil

#endif // __GUIUTIL_H__
