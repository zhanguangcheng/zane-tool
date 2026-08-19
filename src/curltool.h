#ifndef CURLTOOL_H
#define CURLTOOL_H

#include <QObject>
#include <QTextEdit>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QTableWidget>
#include <QTreeWidget>
#include <QTabWidget>
#include <QCheckBox>
#include <QNetworkAccessManager>
#include <QPair>
#include <QByteArray>
#include <QElapsedTimer>
#include <QJsonDocument>

class QNetworkReply;

class CurlTool : public QObject
{
    Q_OBJECT
public:
    explicit CurlTool(QObject *parent = nullptr);

    QWidget *createPage();

private slots:
    void onParse();
    void onSend();
    void onStop();
    void onClear();
    void onCopyResponseBody();
    void onExportExcel();
    void onJsonTreeSelectionChanged();
    void onRequestFinished();
    void onAutoPrettyToggled();

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;

private:
    struct BodyPart {
        int kind;                 // 0 = 原文, 1 = urlencode, 2 = 二进制
        QByteArray data;
    };
    struct ParsedRequest {
        QString method;
        QString url;
        QList<QPair<QString, QString>> headers;
        QByteArray body;
        bool followRedirects = false;
        bool insecure = false;
    };

    void setupUi();
    void addHeader(ParsedRequest &req, const QString &headerLine, QStringList &warnings);
    void applyPreview();
    void resetResponse();
    void renderBody();
    void updateExportEnabled();
    QString defaultExportFileName() const;
    QString statusText(int code) const;
    bool parseCurl(const QString &text, ParsedRequest &req, QString &error, QStringList &warnings);

    QTextEdit *m_inputEdit;
    QPushButton *m_parseBtn;
    QPushButton *m_sendBtn;
    QPushButton *m_stopBtn;
    QPushButton *m_clearBtn;

    QLabel *m_methodValue;
    QLineEdit *m_urlValue;
    QLabel *m_warningLabel;
    QTableWidget *m_headersTable;
    QTextEdit *m_bodyPreview;

    QLabel *m_statusLabel;
    QTabWidget *m_responseTabs;
    QTextEdit *m_responseBody;
    QTextEdit *m_responseHeaders;
    QTreeWidget *m_jsonTree;
    QCheckBox *m_autoPrettyCheck;
    QPushButton *m_copyBodyBtn;
    QPushButton *m_exportExcelBtn;

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_reply;
    QElapsedTimer m_elapsedTimer;
    QWidget *m_pageWidget;

    QByteArray m_lastBodyRaw;
    QJsonDocument m_jsonDoc;
    ParsedRequest m_currentRequest;
};

#endif // CURLTOOL_H