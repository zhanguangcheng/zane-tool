#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDateTime>
#include <QUrl>
#include <QRegularExpression>
#include <QCryptographicHash>

#include "codectool.h"

static const QString CONSOLA_STYLE = QStringLiteral(
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

static const QString READONLY_STYLE = QStringLiteral(
    "QTextEdit {"
    "  font-family: 'Consolas', 'Courier New', monospace;"
    "  font-size: 12px;"
    "  border: none;"
    "  padding: 10px;"
    "  background-color: #ffffff;"
    "  color: #212529;"
    "}");

CodecTool::CodecTool(QObject *parent)
    : QObject(parent)
    , m_decodeTypeGroup(nullptr)
    , m_decodeType(0)
    , m_decodeInput(nullptr)
    , m_decodeBtn(nullptr)
    , m_decodeClearBtn(nullptr)
    , m_decodeCopyBtn(nullptr)
    , m_decodeOutputStack(nullptr)
    , m_decodeTextOutput(nullptr)
    , m_jwtResultTabs(nullptr)
    , m_jwtHeaderEdit(nullptr)
    , m_jwtPayloadEdit(nullptr)
    , m_jwtSignatureEdit(nullptr)
    , m_encodeTypeGroup(nullptr)
    , m_encodeType(0)
    , m_encodeInput(nullptr)
    , m_encodeBtn(nullptr)
    , m_encodeClearBtn(nullptr)
    , m_encodeCopyBtn(nullptr)
    , m_encodeOutput(nullptr)
{
}

QByteArray CodecTool::base64UrlDecode(const QByteArray &input)
{
    QByteArray data = input;
    data.replace('-', '+');
    data.replace('_', '/');
    while (data.size() % 4 != 0)
        data.append('=');
    return QByteArray::fromBase64(data);
}

QByteArray CodecTool::base64UrlEncode(const QByteArray &input)
{
    QByteArray b64 = input.toBase64(QByteArray::OmitTrailingEquals);
    b64.replace('+', '-');
    b64.replace('/', '_');
    return b64;
}

static QString unicodeEncode(const QString &input)
{
    QString result;
    result.reserve(input.size() * 6);
    for (const QChar &ch : input) {
        ushort u = ch.unicode();
        if (u > 0x7f)
            result += QStringLiteral("\\u%1").arg(u, 4, 16, QLatin1Char('0'));
        else
            result += ch;
    }
    return result;
}

static QString unicodeDecode(const QString &input)
{
    static const QRegularExpression re(QStringLiteral("\\\\u([0-9A-Fa-f]{4})"));
    if (!input.contains(re))
        return input;

    QString result;
    int lastPos = 0;
    QRegularExpressionMatchIterator it = re.globalMatch(input);
    while (it.hasNext()) {
        QRegularExpressionMatch m = it.next();
        result += input.mid(lastPos, m.capturedStart() - lastPos);
        bool ok = false;
        ushort codeUnit = m.captured(1).toUShort(&ok, 16);
        if (ok)
            result += QChar(codeUnit);
        lastPos = m.capturedEnd();
    }
    result += input.mid(lastPos);
    return result;
}

QString CodecTool::phpEncode(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::Null:
        return QStringLiteral("N;");
    case QJsonValue::Bool:
        return value.toBool() ? QStringLiteral("b:1;") : QStringLiteral("b:0;");
    case QJsonValue::Double: {
        double d = value.toDouble();
        if (d == static_cast<qint64>(d)) {
            return QStringLiteral("i:%1;").arg(static_cast<qint64>(d));
        }
        return QStringLiteral("d:%1;").arg(d, 0, 'f', 6);
    }
    case QJsonValue::String: {
        QByteArray utf8 = value.toString().toUtf8();
        return QStringLiteral("s:%1:\"%2\";").arg(utf8.size()).arg(QString::fromUtf8(utf8));
    }
    case QJsonValue::Array: {
        QJsonArray arr = value.toArray();
        QString result = QStringLiteral("a:%1:{").arg(arr.size());
        for (int i = 0; i < arr.size(); ++i) {
            result += QStringLiteral("i:%1;").arg(i) + phpEncode(arr[i]);
        }
        result += QStringLiteral("}");
        return result;
    }
    case QJsonValue::Object: {
        QJsonObject obj = value.toObject();
        QStringList keys = obj.keys();
        QString result = QStringLiteral("a:%1:{").arg(keys.size());
        for (const QString &key : keys) {
            QByteArray keyUtf8 = key.toUtf8();
            result += QStringLiteral("s:%1:\"%2\";").arg(keyUtf8.size()).arg(key);
            result += phpEncode(obj.value(key));
        }
        result += QStringLiteral("}");
        return result;
    }
    default:
        return QStringLiteral("N;");
    }
}

static QString phpUnserializeOne(const QByteArray &data, int &pos)
{
    if (pos >= data.size())
        return QStringLiteral("null");

    char type = data.at(pos);
    pos++;

    switch (type) {
    case 'N': {
        if (pos < data.size() && data.at(pos) == ';')
            pos++;
        return QStringLiteral("null");
    }
    case 'b': {
        if (pos + 2 < data.size() && data.at(pos) == ':') {
            char val = data.at(pos + 1);
            pos += 2;
            if (pos < data.size() && data.at(pos) == ';')
                pos++;
            return val == '0' ? QStringLiteral("false") : QStringLiteral("true");
        }
        return QStringLiteral("null");
    }
    case 'i': {
        if (pos >= data.size() || data.at(pos) != ':')
            return QStringLiteral("null");
        pos++;
        int end = data.indexOf(';', pos);
        if (end < 0)
            return QStringLiteral("null");
        QByteArray num = data.mid(pos, end - pos);
        pos = end + 1;
        return QString::fromUtf8(num);
    }
    case 'd': {
        if (pos >= data.size() || data.at(pos) != ':')
            return QStringLiteral("null");
        pos++;
        int end = data.indexOf(';', pos);
        if (end < 0)
            return QStringLiteral("null");
        QByteArray num = data.mid(pos, end - pos);
        pos = end + 1;
        return QString::fromUtf8(num);
    }
    case 's': {
        if (pos >= data.size() || data.at(pos) != ':')
            return QStringLiteral("\"\"");
        pos++;
        int lenEnd = data.indexOf(':', pos);
        if (lenEnd < 0)
            return QStringLiteral("\"\"");
        int len = data.mid(pos, lenEnd - pos).toInt();
        pos = lenEnd + 1;
        if (pos < data.size() && data.at(pos) == '"')
            pos++;
        if (pos + len > data.size())
            return QStringLiteral("\"\"");
        QByteArray str = data.mid(pos, len);
        pos += len;
        if (pos < data.size() && data.at(pos) == '"')
            pos++;
        if (pos < data.size() && data.at(pos) == ';')
            pos++;
        return QStringLiteral("\"%1\"").arg(QString::fromUtf8(str).replace('\\', QStringLiteral("\\\\"))
            .replace('"', QStringLiteral("\\\""))
            .replace('\n', QStringLiteral("\\n"))
            .replace('\r', QStringLiteral("\\r"))
            .replace('\t', QStringLiteral("\\t")));
    }
    case 'a': {
        if (pos >= data.size() || data.at(pos) != ':')
            return QStringLiteral("[]");
        pos++;
        int countEnd = data.indexOf(':', pos);
        if (countEnd < 0)
            return QStringLiteral("[]");
        int count = data.mid(pos, countEnd - pos).toInt();
        pos = countEnd + 1;
        if (pos < data.size() && data.at(pos) == '{')
            pos++;

        QStringList items;
        bool isAssoc = false;
        for (int i = 0; i < count; ++i) {
            QString key = phpUnserializeOne(data, pos);
            QString val = phpUnserializeOne(data, pos);
            if (!key.startsWith(QStringLiteral("\"i:0\"")) || key.left(1) != "\"" || i != key.toInt()) {
                // Check if key is a non-sequential integer
            }
            if (key != QStringLiteral("\"%1\"").arg(i)) {
                isAssoc = true;
            }
            if (isAssoc) {
                items.append(key + QStringLiteral(": ") + val);
            } else {
                items.append(val);
            }
        }

        if (pos < data.size() && data.at(pos) == '}')
            pos++;

        if (isAssoc) {
            return QStringLiteral("{%1}").arg(items.join(QStringLiteral(", ")));
        } else {
            return QStringLiteral("[%1]").arg(items.join(QStringLiteral(", ")));
        }
    }
    default:
        return QStringLiteral("null");
    }
}

QWidget *CodecTool::createDecodeTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(12);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *typeRow = new QHBoxLayout();
    typeRow->setSpacing(0);
    QLabel *typeLabel = new QLabel(QStringLiteral("解码类型："), tab);
    typeRow->addWidget(typeLabel);
    m_decodeTypeGroup = new QButtonGroup(tab);
    m_decodeTypeGroup->setExclusive(true);
    QStringList decodeLabels = {
        QStringLiteral("URL 解码"),
        QStringLiteral("Base64 解码"),
        QStringLiteral("JWT 解码"),
        QStringLiteral("PHP 反序列化"),
        QStringLiteral("Unicode 解码")
    };
    int n = decodeLabels.size();
    for (int i = 0; i < n; ++i) {
        QPushButton *btn = new QPushButton(decodeLabels[i], tab);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("codec", true);
        if (i == 0) btn->setProperty("first", true);
        if (i == n - 1) btn->setProperty("last", true);
        m_decodeTypeGroup->addButton(btn, i);
        typeRow->addWidget(btn);
        if (i == 0) btn->setChecked(true);
    }
    typeRow->addStretch();

    m_decodeInput = new QTextEdit(tab);
    m_decodeInput->setPlaceholderText(QStringLiteral("请输入要解码的内容..."));
    m_decodeInput->setMaximumHeight(120);
    m_decodeInput->setAcceptRichText(false);
    m_decodeInput->setStyleSheet(CONSOLA_STYLE);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_decodeBtn = new QPushButton(QStringLiteral("解码"), tab);
    m_decodeBtn->setFixedHeight(34);
    m_decodeBtn->setCursor(Qt::PointingHandCursor);

    m_decodeClearBtn = new QPushButton(QStringLiteral("清除"), tab);
    m_decodeClearBtn->setFixedHeight(34);
    m_decodeClearBtn->setCursor(Qt::PointingHandCursor);
    m_decodeClearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));

    btnRow->addStretch();
    btnRow->addWidget(m_decodeBtn);
    btnRow->addWidget(m_decodeClearBtn);

    m_decodeOutputStack = new QStackedWidget(tab);

    m_decodeTextOutput = new QTextEdit(tab);
    m_decodeTextOutput->setReadOnly(true);
    m_decodeTextOutput->setStyleSheet(READONLY_STYLE);

    m_jwtResultTabs = new QTabWidget(tab);
    m_jwtResultTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane {"
        "  border: 1px solid #ced4da;"
        "  border-radius: 4px;"
        "  background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  padding: 6px 16px;"
        "  border: 1px solid #ced4da;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  background-color: #f1f3f5;"
        "  color: #495057;"
        "  font-size: 12px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  color: #0d6efd;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #e9ecef;"
        "}"));

    auto createTab = [](QTabWidget *parent) -> QTextEdit * {
        QTextEdit *edit = new QTextEdit(parent);
        edit->setReadOnly(true);
        edit->setStyleSheet(READONLY_STYLE);
        return edit;
    };

    m_jwtHeaderEdit = createTab(m_jwtResultTabs);
    m_jwtPayloadEdit = createTab(m_jwtResultTabs);
    m_jwtSignatureEdit = createTab(m_jwtResultTabs);
    m_jwtResultTabs->addTab(m_jwtHeaderEdit, QStringLiteral("Header"));
    m_jwtResultTabs->addTab(m_jwtPayloadEdit, QStringLiteral("Payload"));
    m_jwtResultTabs->addTab(m_jwtSignatureEdit, QStringLiteral("Signature"));

    m_decodeOutputStack->addWidget(m_decodeTextOutput);
    m_decodeOutputStack->addWidget(m_jwtResultTabs);
    m_decodeOutputStack->setCurrentIndex(0);

    m_decodeCopyBtn = new QPushButton(QStringLiteral("复制结果"), tab);
    m_decodeCopyBtn->setFixedHeight(34);
    m_decodeCopyBtn->setCursor(Qt::PointingHandCursor);
    m_decodeCopyBtn->setEnabled(false);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_decodeCopyBtn);

    layout->addLayout(typeRow);
    layout->addWidget(m_decodeInput);
    layout->addLayout(btnRow);
    layout->addWidget(m_decodeOutputStack, 1);
    layout->addLayout(bottomRow);

    connect(m_decodeBtn, &QPushButton::clicked, this, &CodecTool::onDecode);
    connect(m_decodeClearBtn, &QPushButton::clicked, this, &CodecTool::onClearDecode);
    connect(m_decodeCopyBtn, &QPushButton::clicked, this, &CodecTool::onCopyDecode);
    connect(m_decodeTypeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) {
        m_decodeType = id;
        onDecodeTypeChanged(id);
    });

    return tab;
}

QWidget *CodecTool::createEncodeTab()
{
    QWidget *tab = new QWidget();
    QVBoxLayout *layout = new QVBoxLayout(tab);
    layout->setSpacing(12);
    layout->setContentsMargins(0, 0, 0, 0);

    QHBoxLayout *typeRow = new QHBoxLayout();
    typeRow->setSpacing(0);
    QLabel *typeLabel = new QLabel(QStringLiteral("编码类型："), tab);
    typeRow->addWidget(typeLabel);
    m_encodeTypeGroup = new QButtonGroup(tab);
    m_encodeTypeGroup->setExclusive(true);
    QStringList encodeLabels = {
        QStringLiteral("URL 编码"),
        QStringLiteral("Base64 编码"),
        QStringLiteral("PHP 序列化"),
        QStringLiteral("MD5"),
        QStringLiteral("Unicode 编码")
    };
    int n = encodeLabels.size();
    for (int i = 0; i < n; ++i) {
        QPushButton *btn = new QPushButton(encodeLabels[i], tab);
        btn->setCheckable(true);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setProperty("codec", true);
        if (i == 0) btn->setProperty("first", true);
        if (i == n - 1) btn->setProperty("last", true);
        m_encodeTypeGroup->addButton(btn, i);
        typeRow->addWidget(btn);
        if (i == 0) btn->setChecked(true);
    }
    typeRow->addStretch();

    m_encodeInput = new QTextEdit(tab);
    m_encodeInput->setPlaceholderText(QStringLiteral("请输入要编码的内容..."));
    m_encodeInput->setMaximumHeight(120);
    m_encodeInput->setAcceptRichText(false);
    m_encodeInput->setStyleSheet(CONSOLA_STYLE);

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_encodeBtn = new QPushButton(QStringLiteral("编码"), tab);
    m_encodeBtn->setFixedHeight(34);
    m_encodeBtn->setCursor(Qt::PointingHandCursor);

    m_encodeClearBtn = new QPushButton(QStringLiteral("清除"), tab);
    m_encodeClearBtn->setFixedHeight(34);
    m_encodeClearBtn->setCursor(Qt::PointingHandCursor);
    m_encodeClearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));

    btnRow->addStretch();
    btnRow->addWidget(m_encodeBtn);
    btnRow->addWidget(m_encodeClearBtn);

    m_encodeOutput = new QTextEdit(tab);
    m_encodeOutput->setReadOnly(true);
    m_encodeOutput->setStyleSheet(CONSOLA_STYLE);

    m_encodeCopyBtn = new QPushButton(QStringLiteral("复制结果"), tab);
    m_encodeCopyBtn->setFixedHeight(34);
    m_encodeCopyBtn->setCursor(Qt::PointingHandCursor);
    m_encodeCopyBtn->setEnabled(false);

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    bottomRow->addWidget(m_encodeCopyBtn);

    layout->addLayout(typeRow);
    layout->addWidget(m_encodeInput);
    layout->addLayout(btnRow);
    layout->addWidget(m_encodeOutput, 1);
    layout->addLayout(bottomRow);

    connect(m_encodeBtn, &QPushButton::clicked, this, &CodecTool::onEncode);
    connect(m_encodeClearBtn, &QPushButton::clicked, this, &CodecTool::onClearEncode);
    connect(m_encodeCopyBtn, &QPushButton::clicked, this, &CodecTool::onCopyEncode);
    connect(m_encodeTypeGroup, QOverload<int>::of(&QButtonGroup::idClicked),
            this, [this](int id) { m_encodeType = id; });

    return tab;
}

QWidget *CodecTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(0);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QTabWidget *mainTabs = new QTabWidget(page);
    mainTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane {"
        "  border: 1px solid #ced4da;"
        "  border-radius: 4px;"
        "  background-color: #ffffff;"
        "  padding: 16px;"
        "}"
        "QTabBar::tab {"
        "  padding: 10px 24px;"
        "  border: 1px solid #ced4da;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  background-color: #f1f3f5;"
        "  color: #495057;"
        "  font-size: 14px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  color: #0d6efd;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #e9ecef;"
        "}"));

    mainTabs->addTab(createDecodeTab(), QStringLiteral("解码"));
    mainTabs->addTab(createEncodeTab(), QStringLiteral("编码"));

    mainLayout->addWidget(mainTabs, 1);

    return page;
}

void CodecTool::onDecodeTypeChanged(int index)
{
    if (index == 2) {
        m_decodeOutputStack->setCurrentIndex(1);
    } else {
        m_decodeOutputStack->setCurrentIndex(0);
    }
}

void CodecTool::onDecode()
{
    QString input = m_decodeInput->toPlainText().trimmed();
    if (input.isEmpty())
        return;

    m_decodeTextOutput->clear();
    m_jwtHeaderEdit->clear();
    m_jwtPayloadEdit->clear();
    m_jwtSignatureEdit->clear();

    int type = m_decodeType;
    switch (type) {
    case 0: {
        m_decodeTextOutput->setPlainText(QUrl::fromPercentEncoding(input.toUtf8()));
        break;
    }
    case 1: {
        QByteArray result = QByteArray::fromBase64(input.toUtf8());
        m_decodeTextOutput->setPlainText(QString::fromUtf8(result));
        break;
    }
    case 2: {
        QStringList parts = input.split('.');
        if (parts.size() != 3) {
            m_jwtHeaderEdit->setPlainText(QStringLiteral("不是有效的 JWT 格式\n\nJWT 应包含三部分，以点号分隔：header.payload.signature"));
        } else {
            {
                QByteArray decoded = base64UrlDecode(parts[0].toUtf8());
                if (decoded.isEmpty() && !parts[0].isEmpty()) {
                    m_jwtHeaderEdit->setPlainText(QStringLiteral("无法解码 Header (Base64url 解码失败)"));
                } else {
                    QJsonDocument doc = QJsonDocument::fromJson(decoded);
                    if (doc.isObject()) {
                        m_jwtHeaderEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
                    } else {
                        m_jwtHeaderEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
                    }
                }
            }

            {
                QByteArray decoded = base64UrlDecode(parts[1].toUtf8());
                if (decoded.isEmpty() && !parts[1].isEmpty()) {
                    m_jwtPayloadEdit->setPlainText(QStringLiteral("无法解码 Payload (Base64url 解码失败)"));
                } else {
                    QJsonDocument doc = QJsonDocument::fromJson(decoded);
                    if (doc.isObject()) {
                        QString payloadStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
                        QJsonObject obj = doc.object();
                        QStringList timeInfo;
                        auto addTimeField = [&](const QString &key, const QString &label) {
                            if (obj.contains(key)) {
                                QJsonValue val = obj.value(key);
                                if (val.isDouble()) {
                                    qint64 ts = static_cast<qint64>(val.toDouble());
                                    QDateTime dt;
                                    if (ts > 1000000000000)
                                        dt = QDateTime::fromMSecsSinceEpoch(ts);
                                    else
                                        dt = QDateTime::fromSecsSinceEpoch(ts);
                                    if (dt.isValid()) {
                                        timeInfo.append(QStringLiteral("%1: %2").arg(label, dt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))));
                                    }
                                }
                            }
                        };
                        addTimeField(QStringLiteral("iat"), QStringLiteral("签发时间 (iat)"));
                        addTimeField(QStringLiteral("exp"), QStringLiteral("过期时间 (exp)"));
                        addTimeField(QStringLiteral("nbf"), QStringLiteral("生效时间 (nbf)"));

                        if (!timeInfo.isEmpty()) {
                            payloadStr += QStringLiteral("\n\n--- 时间戳转换 ---\n") + timeInfo.join('\n');
                        }
                        m_jwtPayloadEdit->setPlainText(payloadStr);
                    } else {
                        m_jwtPayloadEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
                    }
                }
            }
            m_jwtSignatureEdit->setPlainText(parts[2]);
            m_jwtResultTabs->setCurrentIndex(1);
        }
        break;
    }
    case 3: {
        int pos = 0;
        m_decodeTextOutput->setPlainText(phpUnserializeOne(input.toUtf8(), pos));
        break;
    }
    case 4: {
        m_decodeTextOutput->setPlainText(unicodeDecode(input));
        break;
    }
    }

    m_decodeOutputStack->setCurrentIndex(type == 2 ? 1 : 0);
    m_decodeCopyBtn->setEnabled(true);
}

void CodecTool::onClearDecode()
{
    m_decodeInput->clear();
    m_decodeTextOutput->clear();
    m_jwtHeaderEdit->clear();
    m_jwtPayloadEdit->clear();
    m_jwtSignatureEdit->clear();
    m_decodeCopyBtn->setEnabled(false);
}

void CodecTool::onCopyDecode()
{
    QString text;
    if (m_decodeOutputStack->currentIndex() == 1) {
        QTextEdit *current = qobject_cast<QTextEdit *>(m_jwtResultTabs->currentWidget());
        if (current)
            text = current->toPlainText();
    } else {
        text = m_decodeTextOutput->toPlainText();
    }
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_decodeCopyBtn->text();
    m_decodeCopyBtn->setText(QStringLiteral("已复制"));
    m_decodeCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_decodeCopyBtn->setText(original);
        m_decodeCopyBtn->setEnabled(true);
    });
}

void CodecTool::onEncode()
{
    QString input = m_encodeInput->toPlainText();
    if (input.isEmpty())
        return;

    m_encodeOutput->clear();

    int type = m_encodeType;
    switch (type) {
    case 0: {
        m_encodeOutput->setPlainText(QUrl::toPercentEncoding(input));
        break;
    }
    case 1: {
        m_encodeOutput->setPlainText(QString::fromUtf8(input.toUtf8().toBase64(QByteArray::OmitTrailingEquals)));
        break;
    }
    case 2: {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(input.toUtf8(), &err);
        if (err.error == QJsonParseError::NoError && (doc.isArray() || doc.isObject())) {
            if (doc.isArray())
                m_encodeOutput->setPlainText(phpEncode(QJsonValue(doc.array())));
            else
                m_encodeOutput->setPlainText(phpEncode(QJsonValue(doc.object())));
        } else {
            m_encodeOutput->setPlainText(phpEncode(QJsonValue(input)));
        }
        break;
    }
    case 3: {
        m_encodeOutput->setPlainText(
            QString::fromUtf8(QCryptographicHash::hash(input.toUtf8(), QCryptographicHash::Md5).toHex()));
        break;
    }
    case 4: {
        m_encodeOutput->setPlainText(unicodeEncode(input));
        break;
    }
    }

    m_encodeCopyBtn->setEnabled(true);
}

void CodecTool::onClearEncode()
{
    m_encodeInput->clear();
    m_encodeOutput->clear();
    m_encodeCopyBtn->setEnabled(false);
}

void CodecTool::onCopyEncode()
{
    QString text = m_encodeOutput->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_encodeCopyBtn->text();
    m_encodeCopyBtn->setText(QStringLiteral("已复制"));
    m_encodeCopyBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_encodeCopyBtn->setText(original);
        m_encodeCopyBtn->setEnabled(true);
    });
}
