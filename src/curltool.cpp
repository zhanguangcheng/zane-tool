#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSet>
#include <QVector>
#include <QSslError>
#include <QResizeEvent>
#include <QFont>
#include <QJsonArray>

#include "curltool.h"
#include "utils.h"
#include "jsontool.h"
#include "xlsxwriter.h"

namespace {

QStringList tokenizeShell(const QString &text)
{
    QStringList tokens;
    QString cur;
    int n = text.size();
    int i = 0;

    auto push = [&]() {
        if (!cur.isEmpty()) {
            tokens.append(cur);
            cur.clear();
        }
    };

    while (i < n) {
        const QChar c = text.at(i);

        if (c == QLatin1Char('\\') && i + 1 < n) {
            const QChar nxt = text.at(i + 1);
            cur += nxt;
            i += 2;
            continue;
        }

        if (c.isSpace()) {
            push();
            ++i;
            continue;
        }

        if (c == QLatin1Char('\'')) {
            int j = i + 1;
            while (j < n && text.at(j) != QLatin1Char('\'')) {
                cur += text.at(j);
                ++j;
            }
            i = (j < n) ? j + 1 : j;
            continue;
        }

        if (c == QLatin1Char('"')) {
            int j = i + 1;
            while (j < n) {
                const QChar d = text.at(j);
                if (d == QLatin1Char('"'))
                    break;
                if (d == QLatin1Char('\\') && j + 1 < n) {
                    const QChar esc = text.at(j + 1);
                    if (esc == QLatin1Char('"') || esc == QLatin1Char('\\')
                        || esc == QLatin1Char('$') || esc == QLatin1Char('`')) {
                        cur += esc;
                        j += 2;
                        continue;
                    }
                    cur += QLatin1Char('\\');
                    cur += esc;
                    j += 2;
                    continue;
                }
                cur += d;
                ++j;
            }
            i = (j < n) ? j + 1 : j;
            continue;
        }

        if (c == QLatin1Char('$') && i + 1 < n && text.at(i + 1) == QLatin1Char('\'')) {
            int j = i + 2;
            while (j < n) {
                const QChar d = text.at(j);
                if (d == QLatin1Char('\''))
                    break;
                if (d == QLatin1Char('\\') && j + 1 < n) {
                    const QChar esc = text.at(j + 1);
                    switch (esc.toLatin1()) {
                    case 'n': cur += QLatin1Char('\n'); break;
                    case 't': cur += QLatin1Char('\t'); break;
                    case 'r': cur += QLatin1Char('\r'); break;
                    case 'a': cur += QLatin1Char('\a'); break;
                    case 'b': cur += QLatin1Char('\b'); break;
                    case 'f': cur += QLatin1Char('\f'); break;
                    case 'v': cur += QLatin1Char('\v'); break;
                    default: cur += esc; break;
                    }
                    j += 2;
                    continue;
                }
                cur += d;
                ++j;
            }
            i = (j < n) ? j + 1 : j;
            continue;
        }

        cur += c;
        ++i;
    }

    push();
    return tokens;
}

bool headerNameExists(const QList<QPair<QString, QString>> &headers, const QString &name)
{
    for (const auto &h : headers) {
        if (h.first.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

bool longOptionTakesValue(const QString &name)
{
    static const QSet<QString> s = {
        QStringLiteral("url"), QStringLiteral("request"), QStringLiteral("header"), QStringLiteral("cookie"),
        QStringLiteral("cookie-jar"), QStringLiteral("data"), QStringLiteral("data-ascii"),
        QStringLiteral("data-raw"), QStringLiteral("data-binary"), QStringLiteral("data-urlencode"),
        QStringLiteral("json"), QStringLiteral("user"), QStringLiteral("user-agent"),
        QStringLiteral("referer"), QStringLiteral("resolve"), QStringLiteral("proxy"),
        QStringLiteral("output"), QStringLiteral("form"), QStringLiteral("retry"),
        QStringLiteral("connect-timeout"), QStringLiteral("max-time"), QStringLiteral("limit-rate"),
        QStringLiteral("speed-limit"), QStringLiteral("speed-time"), QStringLiteral("cert"),
        QStringLiteral("key"), QStringLiteral("pass"), QStringLiteral("cacert"), QStringLiteral("capath"),
        QStringLiteral("keepalive-time"), QStringLiteral("engine"), QStringLiteral("trace"),
        QStringLiteral("trace-ascii"), QStringLiteral("parallel-max")};
    return s.contains(name);
}

QString reasonPhrase(int code)
{
    switch (code) {
    case 200: return QStringLiteral("OK");
    case 201: return QStringLiteral("Created");
    case 202: return QStringLiteral("Accepted");
    case 203: return QStringLiteral("Non-Authoritative Information");
    case 204: return QStringLiteral("No Content");
    case 206: return QStringLiteral("Partial Content");
    case 301: return QStringLiteral("Moved Permanently");
    case 302: return QStringLiteral("Found");
    case 303: return QStringLiteral("See Other");
    case 304: return QStringLiteral("Not Modified");
    case 307: return QStringLiteral("Temporary Redirect");
    case 308: return QStringLiteral("Permanent Redirect");
    case 400: return QStringLiteral("Bad Request");
    case 401: return QStringLiteral("Unauthorized");
    case 403: return QStringLiteral("Forbidden");
    case 404: return QStringLiteral("Not Found");
    case 405: return QStringLiteral("Method Not Allowed");
    case 406: return QStringLiteral("Not Acceptable");
    case 408: return QStringLiteral("Request Timeout");
    case 409: return QStringLiteral("Conflict");
    case 410: return QStringLiteral("Gone");
    case 413: return QStringLiteral("Payload Too Large");
    case 415: return QStringLiteral("Unsupported Media Type");
    case 422: return QStringLiteral("Unprocessable Entity");
    case 429: return QStringLiteral("Too Many Requests");
    case 500: return QStringLiteral("Internal Server Error");
    case 501: return QStringLiteral("Not Implemented");
    case 502: return QStringLiteral("Bad Gateway");
    case 503: return QStringLiteral("Service Unavailable");
    case 504: return QStringLiteral("Gateway Timeout");
    default: return QString();
    }
}

} // namespace

CurlTool::CurlTool(QObject *parent)
    : QObject(parent)
    , m_inputEdit(nullptr)
    , m_parseBtn(nullptr)
    , m_sendBtn(nullptr)
    , m_stopBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_methodValue(nullptr)
    , m_urlValue(nullptr)
    , m_warningLabel(nullptr)
    , m_headersTable(nullptr)
    , m_bodyPreview(nullptr)
    , m_statusLabel(nullptr)
    , m_responseTabs(nullptr)
    , m_responseBody(nullptr)
    , m_responseHeaders(nullptr)
    , m_jsonTree(nullptr)
    , m_autoPrettyCheck(nullptr)
    , m_copyBodyBtn(nullptr)
    , m_exportExcelBtn(nullptr)
    , m_manager(new QNetworkAccessManager(this))
    , m_reply(nullptr)
    , m_pageWidget(nullptr)
{
}

QWidget *CurlTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    const QString monoQss = QStringLiteral(
        "QTextEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "}"
        "QTextEdit:focus { border-color: #86b7fe; }");

    // ---- 请求输入 ----
    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("Curl \u547D\u4EE4"), page);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(8);

    m_inputEdit = new QTextEdit(inputGroup);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setPlaceholderText(QStringLiteral("\u7C98\u8D34\u6D4F\u89C8\u5668\u590D\u5236\u7684 curl \u547D\u4EE4\uFF0C\u70B9\u51FB\u201C\u89E3\u6790\u201D\u6216\u201C\u53D1\u9001\u201D\u539F\u5473\u6267\u884C\u2026"));
    m_inputEdit->setMaximumHeight(90);
    m_inputEdit->setStyleSheet(monoQss);
    inputLayout->addWidget(m_inputEdit);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);
    m_sendBtn = new QPushButton(QStringLiteral("\u53D1\u9001"), inputGroup);
    m_sendBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn = new QPushButton(QStringLiteral("\u505C\u6B62"), inputGroup);
    m_stopBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_stopBtn->setCursor(Qt::PointingHandCursor);
    m_stopBtn->setEnabled(false);
    m_parseBtn = new QPushButton(QStringLiteral("\u89E3\u6790"), inputGroup);
    m_parseBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn = new QPushButton(QStringLiteral("\u6E05\u7A7A"), inputGroup);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setObjectName(QStringLiteral("dangerBtn"));
    btnRow->addWidget(m_sendBtn);
    btnRow->addWidget(m_stopBtn);
    btnRow->addWidget(m_parseBtn);
    btnRow->addWidget(m_clearBtn);
    btnRow->addStretch(1);
    inputLayout->addLayout(btnRow);

    mainLayout->addWidget(inputGroup);

    // ---- 请求预览 ----
    QGroupBox *previewGroup = new QGroupBox(QStringLiteral("\u8BF7\u6C42\u9884\u89C8"), page);
    QVBoxLayout *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setSpacing(8);

    QHBoxLayout *urlRow = new QHBoxLayout();
    QLabel *methodLabel = new QLabel(QStringLiteral("\u65B9\u6CD5:"), previewGroup);
    m_methodValue = new QLabel(QStringLiteral("-"), previewGroup);
    m_methodValue->setStyleSheet(QStringLiteral(
        "font-family: 'Consolas','Courier New',monospace;"
        "font-weight: bold; color: #0d6efd; background: #e7f1ff;"
        "border: 1px solid #86b7fe; border-radius: 4px; padding: 2px 10px;"));
    QLabel *urlLabel = new QLabel(QStringLiteral(" URL:"), previewGroup);
    m_urlValue = new QLineEdit(previewGroup);
    m_urlValue->setReadOnly(true);
    m_urlValue->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    m_urlValue->setPlaceholderText(QStringLiteral("\u89E3\u6790\u540E\u5C55\u793A\u8BF7\u6C42 URL"));
    m_urlValue->setStyleSheet(QStringLiteral("font-family: 'Consolas','Courier New',monospace; font-size: 12px;"));
    urlRow->addWidget(methodLabel);
    urlRow->addWidget(m_methodValue);
    urlRow->addWidget(urlLabel);
    urlRow->addWidget(m_urlValue, 1);
    previewLayout->addLayout(urlRow);

    m_headersTable = new QTableWidget(previewGroup);
    m_headersTable->setColumnCount(2);
    m_headersTable->setHorizontalHeaderLabels({QStringLiteral("\u5934\u540D\u79F0"), QStringLiteral("\u5934\u503C")});
    m_headersTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_headersTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_headersTable->verticalHeader()->setVisible(false);
    m_headersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_headersTable->setFocusPolicy(Qt::NoFocus);
    m_headersTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_headersTable->setMaximumHeight(110);
    previewLayout->addWidget(m_headersTable);

    m_bodyPreview = new QTextEdit(previewGroup);
    m_bodyPreview->setReadOnly(true);
    m_bodyPreview->setMaximumHeight(70);
    m_bodyPreview->setStyleSheet(monoQss);
    m_bodyPreview->setPlaceholderText(QStringLiteral("\u65E0\u8BF7\u6C42\u4F53"));
    previewLayout->addWidget(m_bodyPreview);

    m_warningLabel = new QLabel(previewGroup);
    m_warningLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 12px;"));
    m_warningLabel->setWordWrap(true);
    previewLayout->addWidget(m_warningLabel);

    mainLayout->addWidget(previewGroup);

    // ---- 响应 ----
    QGroupBox *respGroup = new QGroupBox(QStringLiteral("\u54CD\u5E94"), page);
    QVBoxLayout *respLayout = new QVBoxLayout(respGroup);
    respLayout->setSpacing(8);

    QHBoxLayout *respTopRow = new QHBoxLayout();
    m_statusLabel = new QLabel(QStringLiteral("\u5F85\u53D1\u9001"), respGroup);
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
    respTopRow->addWidget(m_statusLabel, 1);
    m_autoPrettyCheck = new QCheckBox(QStringLiteral("\u81EA\u52A8\u683C\u5F0F\u5316 JSON"), respGroup);
    m_autoPrettyCheck->setChecked(true);
    respTopRow->addWidget(m_autoPrettyCheck);
    m_copyBodyBtn = new QPushButton(QStringLiteral("\u590D\u5236\u54CD\u5E94\u4F53"), respGroup);
    m_copyBodyBtn->setCursor(Qt::PointingHandCursor);
    respTopRow->addWidget(m_copyBodyBtn);
    m_exportExcelBtn = new QPushButton(QStringLiteral("导出 Excel"), respGroup);
    m_exportExcelBtn->setCursor(Qt::PointingHandCursor);
    m_exportExcelBtn->setEnabled(false);
    m_exportExcelBtn->setToolTip(QStringLiteral("\u5728\u201CJSON \u6811\u201D\u4E2D\u9009\u4E2D\u6570\u7EC4\u8282\u70B9\u540E\u53EF\u5BFC\u51FA\u4E3A Excel"));
    respTopRow->addWidget(m_exportExcelBtn);
    respLayout->addLayout(respTopRow);

    m_responseTabs = new QTabWidget(respGroup);

    m_jsonTree = new QTreeWidget(respGroup);
    m_jsonTree->setColumnCount(2);
    m_jsonTree->setHeaderLabels({QStringLiteral("名称"), QStringLiteral("值")});
    m_jsonTree->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_jsonTree->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_jsonTree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_jsonTree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_jsonTree->setFocusPolicy(Qt::NoFocus);
    connect(m_jsonTree, &QTreeWidget::currentItemChanged,
            this, &CurlTool::onJsonTreeSelectionChanged);

    m_responseBody = new QTextEdit(respGroup);
    m_responseBody->setReadOnly(true);
    m_responseBody->setStyleSheet(monoQss);
    m_responseBody->setPlaceholderText(QStringLiteral("\u54CD\u5E94\u4F53\u5C06\u663E\u793A\u5728\u8FD9\u91CC"));
    m_responseHeaders = new QTextEdit(respGroup);
    m_responseHeaders->setReadOnly(true);
    m_responseHeaders->setStyleSheet(monoQss);
    m_responseTabs->addTab(m_responseBody, QStringLiteral("\u54CD\u5E94\u4F53"));
    m_responseTabs->addTab(m_jsonTree, QStringLiteral("JSON \u6811"));
    m_responseTabs->addTab(m_responseHeaders, QStringLiteral("\u54CD\u5E94\u5934"));
    m_responseTabs->setCurrentIndex(0);
    respLayout->addWidget(m_responseTabs, 1);

    mainLayout->addWidget(respGroup, 1);

    connect(m_sendBtn, &QPushButton::clicked, this, &CurlTool::onSend);
    connect(m_stopBtn, &QPushButton::clicked, this, &CurlTool::onStop);
    connect(m_parseBtn, &QPushButton::clicked, this, &CurlTool::onParse);
    connect(m_clearBtn, &QPushButton::clicked, this, &CurlTool::onClear);
    connect(m_copyBodyBtn, &QPushButton::clicked, this, &CurlTool::onCopyResponseBody);
    connect(m_exportExcelBtn, &QPushButton::clicked, this, &CurlTool::onExportExcel);
    connect(m_autoPrettyCheck, &QCheckBox::toggled, this, &CurlTool::onAutoPrettyToggled);

    m_pageWidget = page;
    page->installEventFilter(this);

    return page;
}

bool CurlTool::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_pageWidget && event->type() == QEvent::Resize) {
        const int pageHeight = static_cast<QWidget *>(obj)->height();
        if (pageHeight >= 700)
            m_inputEdit->setMaximumHeight(200);
        else
            m_inputEdit->setMaximumHeight(90);
    }
    return QObject::eventFilter(obj, event);
}

void CurlTool::addHeader(ParsedRequest &req, const QString &headerLine, QStringList &warnings)
{
    QString line = headerLine.trimmed();
    if (line.isEmpty())
        return;
    if (line.startsWith(QLatin1Char('@'))) {
        warnings.append(QStringLiteral("\u5FFD\u7565\u4ECE\u6587\u4EF6\u8BFB\u53D6\u5934 (-H @file)"));
        return;
    }
    int colon = line.indexOf(QLatin1Char(':'));
    if (colon < 0) {
        warnings.append(QStringLiteral("\u5FFD\u7565\u65E0\u5192\u53F7\u5934: %1").arg(line.left(40)));
        return;
    }
    QString name = line.left(colon).trimmed();
    QString value = line.mid(colon + 1).trimmed();
    if (name.isEmpty()) {
        warnings.append(QStringLiteral("\u5FFD\u7565\u7A7A\u5934\u540D"));
        return;
    }
    for (int i = 0; i < req.headers.size(); ++i) {
        if (req.headers.at(i).first.compare(name, Qt::CaseInsensitive) == 0) {
            req.headers[i].second = value;
            return;
        }
    }
    req.headers.append({name, value});
}

bool CurlTool::parseCurl(const QString &text, ParsedRequest &req, QString &error, QStringList &warnings)
{
    warnings.clear();
    req = ParsedRequest();
    req.method = QStringLiteral("GET");

    if (text.trimmed().isEmpty()) {
        error = QStringLiteral("\u8BF7\u5148\u7C98\u8D34 curl \u547D\u4EE4");
        return false;
    }

    QString t = text;
    t.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    t.replace(QStringLiteral("\\\n"), QStringLiteral(" "));

    const QStringList args = tokenizeShell(t);
    if (args.isEmpty()) {
        error = QStringLiteral("\u547D\u4EE4\u89E3\u6790\u5931\u8D25\uFF1A\u65E0\u6709\u6548\u53C2\u6570");
        return false;
    }

    int i = 0;
    if (args.at(0).compare(QStringLiteral("curl"), Qt::CaseInsensitive) == 0)
        ++i;

    QString url;
    bool hasUrl = false;
    bool explicitMethod = false;
    bool getFlag = false;
    bool headFlag = false;
    bool haveBody = false;
    QString cookieValue;
    QVector<BodyPart> bodyParts;
    QString jsonContentType;

    auto addCookie = [&](const QString &v) {
        QString val = v.trimmed();
        if (val.startsWith(QLatin1Char('@'))) {
            warnings.append(QStringLiteral("\u5FFD\u7565\u4ECE\u6587\u4EF6\u8BFB\u53D6 Cookie (-b @file)"));
            return;
        }
        if (!cookieValue.isEmpty())
            cookieValue += QStringLiteral("; ");
        cookieValue += val;
    };

    auto addBody = [&](int kind, const QString &v) {
        QString val = v;
        QString raw = v;
        if (raw.startsWith(QLatin1Char('@'))) {
            QFile f(raw.mid(1));
            if (f.open(QIODevice::ReadOnly)) {
                val = QString::fromUtf8(f.readAll());
            } else {
                warnings.append(QStringLiteral("\u65E0\u6CD5\u8BFB\u53D6\u6570\u636E\u6587\u4EF6: %1").arg(raw.mid(1)));
                return;
            }
        }
        BodyPart part;
        part.kind = kind;
        part.data = val.toUtf8();
        bodyParts.append(part);
        haveBody = true;
    };

    auto addAuth = [&](const QString &v) {
        const QString auth = QStringLiteral("Authorization: Basic ")
            + QString::fromLatin1(v.toUtf8().toBase64());
        addHeader(req, auth, warnings);
    };

    auto setFlag = [&](const QString &name) {
        if (name == QStringLiteral("get")) {
            getFlag = true;
        } else if (name == QStringLiteral("location") || name == QStringLiteral("location-trusted")) {
            req.followRedirects = true;
        } else if (name == QStringLiteral("insecure")) {
            req.insecure = true;
        } else if (name == QStringLiteral("head")) {
            headFlag = true;
        } else if (name == QStringLiteral("compressed")) {
            if (!headerNameExists(req.headers, QStringLiteral("Accept-Encoding")))
                addHeader(req, QStringLiteral("Accept-Encoding: gzip, deflate"), warnings);
        }
    };

    while (i < args.size()) {
        const QString a = args.at(i);

        if (a == QStringLiteral("--")) {
            ++i;
            while (i < args.size()) {
                if (!hasUrl) {
                    url = args.at(i);
                    hasUrl = true;
                }
                ++i;
            }
            break;
        }

        if (a.startsWith(QStringLiteral("--"))) {
            QString name = a.mid(2);
            QString value;
            int eq = name.indexOf(QLatin1Char('='));
            if (eq >= 0) {
                value = name.mid(eq + 1);
                name = name.left(eq);
            } else if (i + 1 < args.size() && longOptionTakesValue(name)) {
                value = args.at(++i);
            }
            name = name.toLower();

            if (name == QStringLiteral("url")) {
                url = value;
                hasUrl = true;
            } else if (name == QStringLiteral("request")) {
                explicitMethod = true;
                req.method = value.toUpper();
            } else if (name == QStringLiteral("header")) {
                addHeader(req, value, warnings);
            } else if (name == QStringLiteral("referer")) {
                addHeader(req, QStringLiteral("Referer: ") + value, warnings);
            } else if (name == QStringLiteral("user-agent")) {
                addHeader(req, QStringLiteral("User-Agent: ") + value, warnings);
            } else if (name == QStringLiteral("user")) {
                addAuth(value);
            } else if (name == QStringLiteral("cookie")) {
                addCookie(value);
            } else if (name == QStringLiteral("cookie-jar")) {
                // 仅用于接收 jar 文件路径，无需写回
            } else if (name == QStringLiteral("data") || name == QStringLiteral("data-ascii")
                       || name == QStringLiteral("data-raw")) {
                addBody(0, value);
            } else if (name == QStringLiteral("data-binary")) {
                addBody(2, value);
            } else if (name == QStringLiteral("data-urlencode")) {
                addBody(1, value);
            } else if (name == QStringLiteral("json")) {
                jsonContentType = QStringLiteral("application/json");
                addBody(0, value);
            } else if (name == QStringLiteral("get") || name == QStringLiteral("location")
                       || name == QStringLiteral("location-trusted") || name == QStringLiteral("insecure")
                       || name == QStringLiteral("head") || name == QStringLiteral("compressed")) {
                setFlag(name);
            }
            // 其它长选项（--silent/--fail/--output/--max-time 等）与本工具执行无关，直接忽略
            ++i;
            continue;
        }

        if (a.startsWith(QLatin1Char('-')) && a.size() > 1) {
            int pos = 1;
            while (pos < a.size()) {
                const char c = a.at(pos).toLatin1();
                const bool takesValue = (c == 'X' || c == 'H' || c == 'b' || c == 'c' || c == 'd'
                    || c == 'u' || c == 'A' || c == 'e' || c == 'x' || c == 'o' || c == 'T'
                    || c == 'F' || c == 'z' || c == 'K' || c == 'U' || c == 'C' || c == 'D'
                    || c == 'P');
                if (takesValue) {
                    QString value;
                    if (pos + 1 < a.size()) {
                        value = a.mid(pos + 1);
                    } else if (i + 1 < args.size()) {
                        value = args.at(++i);
                    }
                    switch (c) {
                    case 'X': explicitMethod = true; req.method = value.toUpper(); break;
                    case 'H': addHeader(req, value, warnings); break;
                    case 'b': addCookie(value); break;
                    case 'u': addAuth(value); break;
                    case 'A': addHeader(req, QStringLiteral("User-Agent: ") + value, warnings); break;
                    case 'e': addHeader(req, QStringLiteral("Referer: ") + value, warnings); break;
                    case 'd': addBody(0, value); break;
                    default: break; // 无关选项（c/x/o/T/F/z/K/U/C/D/P 等）丢弃
                    }
                    pos = a.size();
                    continue;
                }
                switch (c) {
                case 'G': getFlag = true; break;
                case 'L': req.followRedirects = true; break;
                case 'k': req.insecure = true; break;
                case 'I': headFlag = true; break;
                case 's': case 'S': case 'f': case 'v': case 'O': case 'i':
                case 'q': case 'n': case '#': case '0': case '1': case '2': case '3':
                    break; // 无关短选项
                default:
                    break;
                }
                ++pos;
            }
            ++i;
            continue;
        }

        if (!hasUrl) {
            url = a;
            hasUrl = true;
        }
        ++i;
    }

    if (!bodyParts.isEmpty()) {
        QStringList encoded;
        for (const BodyPart &p : bodyParts) {
            if (p.kind == 1) {
                const QString raw = QString::fromUtf8(p.data);
                int eq = raw.indexOf(QLatin1Char('='));
                if (eq >= 0) {
                    encoded.append(raw.left(eq) + QString::fromUtf8(QUrl::toPercentEncoding(raw.mid(eq + 1))));
                } else {
                    encoded.append(QString::fromUtf8(QUrl::toPercentEncoding(raw)));
                }
            } else {
                encoded.append(QString::fromUtf8(p.data));
            }
        }
        QString body = encoded.join(QLatin1Char('&'));
        if (getFlag) {
            url += url.contains(QLatin1Char('?')) ? QLatin1Char('&') : QLatin1Char('?');
            url += body;
        } else {
            req.body = body.toUtf8();
        }
    }

    if (haveBody || !req.body.isEmpty()) {
        if (!explicitMethod && !getFlag)
            req.method = QStringLiteral("POST");
        else if (getFlag && !explicitMethod)
            req.method = QStringLiteral("GET");
        if (!headerNameExists(req.headers, QStringLiteral("Content-Type"))) {
            if (!jsonContentType.isEmpty()) {
                addHeader(req, QStringLiteral("Content-Type: application/json"), warnings);
            } else {
                addHeader(req, QStringLiteral("Content-Type: application/x-www-form-urlencoded"), warnings);
            }
        }
    } else {
        if (!explicitMethod && headFlag)
            req.method = QStringLiteral("HEAD");
        else if (!explicitMethod)
            req.method = QStringLiteral("GET");
    }

    if (!cookieValue.isEmpty()) {
        addHeader(req, QStringLiteral("Cookie: ") + cookieValue, warnings);
    }

    if (!hasUrl) {
        error = QStringLiteral("\u672A\u89E3\u6790\u5230 URL\uFF0C\u8BF7\u68C0\u67E5\u547D\u4EE4");
        return false;
    }

    req.url = url;
    m_currentRequest = req;
    return true;
}

void CurlTool::applyPreview()
{
    m_methodValue->setText(m_currentRequest.method);
    m_urlValue->setText(m_currentRequest.url);

    m_headersTable->setRowCount(m_currentRequest.headers.size());
    for (int r = 0; r < m_currentRequest.headers.size(); ++r) {
        QTableWidgetItem *nameItem = new QTableWidgetItem(m_currentRequest.headers.at(r).first);
        QFont nf = nameItem->font();
        nf.setFamily(QStringLiteral("Consolas"));
        nameItem->setFont(nf);
        nameItem->setForeground(QColor(QStringLiteral("#0d6efd")));
        QTableWidgetItem *valueItem = new QTableWidgetItem(m_currentRequest.headers.at(r).second);
        valueItem->setFont(nf);
        m_headersTable->setItem(r, 0, nameItem);
        m_headersTable->setItem(r, 1, valueItem);
    }

    if (m_currentRequest.body.isEmpty()) {
        m_bodyPreview->clear();
    } else {
        m_bodyPreview->setPlainText(QString::fromUtf8(m_currentRequest.body));
    }
}

void CurlTool::resetResponse()
{
    m_lastBodyRaw.clear();
    m_jsonDoc = QJsonDocument();
    m_jsonTree->clear();
    m_responseBody->clear();
    m_responseHeaders->clear();
    updateExportEnabled();
}

void CurlTool::onParse()
{
    QString error;
    QStringList warnings;
    if (!parseCurl(m_inputEdit->toPlainText(), m_currentRequest, error, warnings)) {
        m_statusLabel->setText(error);
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }
    applyPreview();
    if (warnings.isEmpty()) {
        m_warningLabel->setText(QString());
    } else {
        m_warningLabel->setText(QStringLiteral("\u5DF2\u5FFD\u7565\u4E0D\u53EF\u6267\u884C\u7684\u9009\u9879: ") + warnings.join(QStringLiteral("; ")));
    }
    m_statusLabel->setText(QStringLiteral("\u89E3\u6790\u6210\u529F\uFF0C\u53EF\u70B9\u51FB\u53D1\u9001"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #198754; font-size: 13px;"));
}

void CurlTool::onSend()
{
    QString error;
    QStringList warnings;
    if (!parseCurl(m_inputEdit->toPlainText(), m_currentRequest, error, warnings)) {
        m_statusLabel->setText(error);
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }

    if (warnings.isEmpty()) {
        m_warningLabel->setText(QString());
    } else {
        m_warningLabel->setText(QStringLiteral("\u5DF2\u5FFD\u7565: ") + warnings.join(QStringLiteral("; ")));
    }

    applyPreview();
    resetResponse();

    const ParsedRequest &req = m_currentRequest;

    QUrl url(req.url);
    if (!url.isValid() || url.host().isEmpty()) {
        m_statusLabel->setText(QStringLiteral("URL \u65E0\u6548: %1").arg(req.url));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }

    QNetworkRequest request(url);
    request.setTransferTimeout(30000);
    for (const auto &h : req.headers)
        request.setRawHeader(h.first.toUtf8(), h.second.toUtf8());
    if (req.followRedirects)
        request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                             QNetworkRequest::NoLessSafeRedirectPolicy);

    const QByteArray verb = req.method.toUpper().toUtf8();
    QNetworkReply *reply = nullptr;
    if (verb == QByteArrayLiteral("GET") && req.body.isEmpty()) {
        reply = m_manager->get(request);
    } else {
        reply = m_manager->sendCustomRequest(request, verb, req.body);
    }

    if (req.insecure) {
        connect(reply, &QNetworkReply::sslErrors, reply, [reply](const QList<QSslError> &) {
            reply->ignoreSslErrors();
        });
    }

    m_reply = reply;
    connect(reply, &QNetworkReply::finished, this, &CurlTool::onRequestFinished);

    m_elapsedTimer.start();
    m_statusLabel->setText(QStringLiteral("\u53D1\u9001\u4E2D (%1)\u2026").arg(req.method));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));
    m_sendBtn->setEnabled(false);
    m_stopBtn->setEnabled(true);
    m_responseBody->setPlainText(QStringLiteral("\u8BF7\u6C42\u53D1\u9002\u4E2D\u2026"));
}

void CurlTool::onStop()
{
    if (m_reply) {
        m_reply->abort();
    }
}

void CurlTool::onRequestFinished()
{
    QNetworkReply *reply = m_reply;
    if (!reply)
        return;
    m_reply = nullptr;
    reply->deleteLater();

    m_sendBtn->setEnabled(true);
    m_stopBtn->setEnabled(false);

    const qint64 ms = m_elapsedTimer.elapsed();
    const QNetworkReply::NetworkError err = reply->error();
    const bool cancelled = (err == QNetworkReply::OperationCanceledError);

    QStringList headerLines;
    const auto pairs = reply->rawHeaderPairs();
    for (const auto &pair : pairs)
        headerLines.append(QString::fromLatin1(pair.first) + QStringLiteral(": ") + QString::fromUtf8(pair.second));
    m_responseHeaders->setPlainText(headerLines.join(QLatin1Char('\n')));

    m_lastBodyRaw = reply->readAll();
    const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString sizeText = Utils::formatFileSize(m_lastBodyRaw.size());

    QString status;
    QString color = QStringLiteral("#198754");
    if (cancelled) {
        status = QStringLiteral("\u5DF2\u53D6\u6D88");
        color = QStringLiteral("#6c757d");
    } else if (err != QNetworkReply::NoError && code == 0) {
        status = QStringLiteral("\u8BF7\u6C42\u5931\u8D25: %1").arg(reply->errorString());
        color = QStringLiteral("#dc3545");
    } else {
        status = QStringLiteral("HTTP %1%2 \u00B7 %3 ms \u00B7 %4")
                    .arg(code)
                    .arg(statusText(code))
                    .arg(ms)
                    .arg(sizeText);
        if (code >= 400)
            color = QStringLiteral("#dc3545");
    }
    m_statusLabel->setText(status);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(color));

    renderBody();

    m_jsonDoc = QJsonDocument();
    m_jsonTree->clear();
    QJsonParseError parseError;
    m_jsonDoc = QJsonDocument::fromJson(m_lastBodyRaw, &parseError);
    if (parseError.error == QJsonParseError::NoError && (m_jsonDoc.isObject() || m_jsonDoc.isArray())) {
        JsonTool::populateTree(m_jsonTree, m_jsonDoc);
        if (m_jsonDoc.isArray() && m_jsonTree->topLevelItemCount() > 0)
            m_jsonTree->setCurrentItem(m_jsonTree->topLevelItem(0));
    }
    updateExportEnabled();
}

void CurlTool::updateExportEnabled()
{
    if (!m_exportExcelBtn)
        return;
    bool enabled = false;
    if (QTreeWidgetItem *item = m_jsonTree->currentItem()) {
        const QStringList path = item->data(0, Qt::UserRole).toStringList();
        const QJsonValue value = JsonTool::valueAtPath(m_jsonDoc, path);
        enabled = value.isArray() && !value.toArray().isEmpty();
    }
    m_exportExcelBtn->setEnabled(enabled);
}

QString CurlTool::defaultExportFileName() const
{
    QString base;
    const QUrl url(m_currentRequest.url);
    const QString path = url.path();
    if (!path.isEmpty()) {
        const QStringList parts = path.split(QLatin1Char('/'), Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            base = parts.last();
            if (base.contains(QLatin1Char('.')))
                base = QFileInfo(base).completeBaseName();
        }
    }
    if (base.isEmpty())
        base = QStringLiteral("data");
    return base + QStringLiteral(".xlsx");
}

void CurlTool::onExportExcel()
{
    QTreeWidgetItem *item = m_jsonTree->currentItem();
    if (!item || m_jsonDoc.isNull())
        return;

    const QStringList path = item->data(0, Qt::UserRole).toStringList();
    const QJsonValue value = JsonTool::valueAtPath(m_jsonDoc, path);
    if (!value.isArray())
        return;

    const QJsonArray array = value.toArray();
    if (array.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("\u5BFC\u51FA\u5931\u8D25\uFF1A\u6570\u7EC4\u4E3A\u7A7A"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }

    QStringList headers;
    QList<QList<XlsxWriter::Cell>> rows;
    if (!JsonTool::arrayToRows(array, headers, rows)) {
        m_statusLabel->setText(QStringLiteral("\u5BFC\u51FA\u5931\u8D25: \u6570\u7EC4\u4E3A\u7A7A"));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }

    const QString filePath = QFileDialog::getSaveFileName(
        m_pageWidget, QStringLiteral("导出 Excel"), defaultExportFileName(),
        QStringLiteral("Excel 文件 (*.xlsx)"));
    if (filePath.isEmpty())
        return;

    QString outPath = filePath;
    if (!outPath.endsWith(QStringLiteral(".xlsx"), Qt::CaseInsensitive))
        outPath += QStringLiteral(".xlsx");

    QString error;
    if (!XlsxWriter::writeSheet(outPath, headers, rows, &error)) {
        m_statusLabel->setText(QStringLiteral("\u5BFC\u51FA\u5931\u8D25：%1").arg(error));
        m_statusLabel->setStyleSheet(QStringLiteral("color: #dc3545; font-size: 13px;"));
        return;
    }

    m_statusLabel->setText(QStringLiteral("\u5DF2\u5BFC\u51FA %1 \u884C \u00D7 %2 \u5217 \u2192 %3")
                               .arg(rows.size()).arg(headers.size()).arg(outPath));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #198754; font-size: 13px;"));
}

QString CurlTool::statusText(int code) const
{
    const QString phrase = reasonPhrase(code);
    return phrase.isEmpty() ? QString() : QStringLiteral(" %1").arg(phrase);
}

void CurlTool::renderBody()
{
    if (m_lastBodyRaw.isEmpty()) {
        m_responseBody->setPlainText(QString());
        return;
    }

    QString out;
    if (m_autoPrettyCheck->isChecked()) {
        QJsonParseError parseError;
        const QJsonDocument doc = QJsonDocument::fromJson(m_lastBodyRaw, &parseError);
        if (parseError.error == QJsonParseError::NoError && (doc.isObject() || doc.isArray())) {
            out = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
        }
    }
    if (out.isEmpty())
        out = QString::fromUtf8(m_lastBodyRaw);
    m_responseBody->setPlainText(out);
}

void CurlTool::onAutoPrettyToggled()
{
    if (!m_responseBody)
        return;
    renderBody();
}

void CurlTool::onJsonTreeSelectionChanged()
{
    updateExportEnabled();
}

void CurlTool::onClear()
{
    m_inputEdit->clear();
    m_methodValue->setText(QStringLiteral("-"));
    m_urlValue->clear();
    m_headersTable->setRowCount(0);
    m_bodyPreview->clear();
    m_warningLabel->clear();
    resetResponse();
    m_statusLabel->setText(QStringLiteral("\u5F85\u53D1\u9001"));
    m_statusLabel->setStyleSheet(QStringLiteral("font-size: 13px;"));
}

void CurlTool::onCopyResponseBody()
{
    if (m_responseBody->toPlainText().isEmpty())
        return;
    QApplication::clipboard()->setText(m_responseBody->toPlainText());
    const QString original = m_copyBodyBtn->text();
    m_copyBodyBtn->setText(QStringLiteral("\u5DF2\u590D\u5236"));
    m_copyBodyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyBodyBtn->setText(original);
        m_copyBodyBtn->setEnabled(true);
    });
}