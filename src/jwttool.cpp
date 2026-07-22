#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QDateTime>

#include "jwttool.h"

JwtTool::JwtTool(QObject *parent)
    : QObject(parent)
    , m_inputEdit(nullptr)
    , m_parseBtn(nullptr)
    , m_clearBtn(nullptr)
    , m_resultTabs(nullptr)
    , m_headerEdit(nullptr)
    , m_payloadEdit(nullptr)
    , m_signatureEdit(nullptr)
    , m_infoLabel(nullptr)
    , m_copyCurrentBtn(nullptr)
    , m_copyAllBtn(nullptr)
{
}

QByteArray JwtTool::base64UrlDecode(const QByteArray &input)
{
    QByteArray data = input;
    data.replace('-', '+');
    data.replace('_', '/');
    while (data.size() % 4 != 0)
        data.append('=');
    return QByteArray::fromBase64(data);
}

QWidget *JwtTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *inputGroup = new QGroupBox(QStringLiteral("JWT 输入"), page);
    QVBoxLayout *inputLayout = new QVBoxLayout(inputGroup);
    inputLayout->setSpacing(12);

    m_inputEdit = new QTextEdit(inputGroup);
    m_inputEdit->setPlaceholderText(QStringLiteral("请输入或粘贴 JWT 字符串 (header.payload.signature)..."));
    m_inputEdit->setMaximumHeight(100);
    m_inputEdit->setAcceptRichText(false);
    m_inputEdit->setStyleSheet(QStringLiteral(
        "QTextEdit {"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  font-size: 12px;"
        "  border: 1px solid #ced4da;"
        "  border-radius: 6px;"
        "  padding: 10px;"
        "  background-color: #ffffff;"
        "  color: #212529;"
        "}"
        "QTextEdit:focus { border-color: #86b7fe; }"));

    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->setSpacing(8);

    m_parseBtn = new QPushButton(QStringLiteral("解析"), inputGroup);
    m_parseBtn->setFixedHeight(34);
    m_parseBtn->setCursor(Qt::PointingHandCursor);
    connect(m_parseBtn, &QPushButton::clicked, this, &JwtTool::onParse);

    m_clearBtn = new QPushButton(QStringLiteral("清除"), inputGroup);
    m_clearBtn->setFixedHeight(34);
    m_clearBtn->setCursor(Qt::PointingHandCursor);
    m_clearBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 6px; font-size: 13px; padding: 0 16px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_clearBtn, &QPushButton::clicked, this, &JwtTool::onClear);

    btnRow->addStretch();
    btnRow->addWidget(m_parseBtn);
    btnRow->addWidget(m_clearBtn);

    inputLayout->addWidget(m_inputEdit);
    inputLayout->addLayout(btnRow);

    QGroupBox *resultGroup = new QGroupBox(QStringLiteral("解析结果"), page);
    QVBoxLayout *resultLayout = new QVBoxLayout(resultGroup);
    resultLayout->setSpacing(10);

    m_resultTabs = new QTabWidget(resultGroup);
    m_resultTabs->setStyleSheet(QStringLiteral(
        "QTabWidget::pane {"
        "  border: 1px solid #ced4da;"
        "  border-radius: 4px;"
        "  background-color: #ffffff;"
        "}"
        "QTabBar::tab {"
        "  padding: 8px 20px;"
        "  border: 1px solid #ced4da;"
        "  border-bottom: none;"
        "  border-top-left-radius: 4px;"
        "  border-top-right-radius: 4px;"
        "  background-color: #f1f3f5;"
        "  color: #495057;"
        "  font-size: 13px;"
        "}"
        "QTabBar::tab:selected {"
        "  background-color: #ffffff;"
        "  color: #0d6efd;"
        "  font-weight: bold;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background-color: #e9ecef;"
        "}"));

    auto createResultTab = [](QTabWidget *parent) -> QTextEdit * {
        QTextEdit *edit = new QTextEdit(parent);
        edit->setReadOnly(true);
        edit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: none;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #212529;"
            "}"));
        return edit;
    };

    m_headerEdit = createResultTab(m_resultTabs);
    m_payloadEdit = createResultTab(m_resultTabs);
    m_signatureEdit = createResultTab(m_resultTabs);

    m_resultTabs->addTab(m_headerEdit, QStringLiteral("Header"));
    m_resultTabs->addTab(m_payloadEdit, QStringLiteral("Payload"));
    m_resultTabs->addTab(m_signatureEdit, QStringLiteral("Signature"));

    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->setSpacing(16);

    m_infoLabel = new QLabel(resultGroup);
    m_infoLabel->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 13px;"));
    m_infoLabel->setWordWrap(true);

    m_copyCurrentBtn = new QPushButton(QStringLiteral("复制当前"), resultGroup);
    m_copyCurrentBtn->setFixedHeight(34);
    m_copyCurrentBtn->setCursor(Qt::PointingHandCursor);
    m_copyCurrentBtn->setEnabled(false);
    connect(m_copyCurrentBtn, &QPushButton::clicked, this, &JwtTool::onCopyCurrent);

    m_copyAllBtn = new QPushButton(QStringLiteral("复制全部"), resultGroup);
    m_copyAllBtn->setFixedHeight(34);
    m_copyAllBtn->setCursor(Qt::PointingHandCursor);
    m_copyAllBtn->setEnabled(false);
    connect(m_copyAllBtn, &QPushButton::clicked, this, &JwtTool::onCopyAll);

    bottomRow->addWidget(m_infoLabel, 1);
    bottomRow->addWidget(m_copyCurrentBtn);
    bottomRow->addWidget(m_copyAllBtn);

    resultLayout->addWidget(m_resultTabs, 1);
    resultLayout->addLayout(bottomRow);

    mainLayout->addWidget(inputGroup);
    mainLayout->addWidget(resultGroup, 1);

    return page;
}

void JwtTool::onParse()
{
    QString input = m_inputEdit->toPlainText().trimmed();
    if (input.isEmpty())
        return;

    m_headerEdit->clear();
    m_payloadEdit->clear();
    m_signatureEdit->clear();
    m_infoLabel->clear();

    QStringList parts = input.split('.');
    if (parts.size() != 3) {
        m_headerEdit->setPlainText(QStringLiteral("不是有效的 JWT 格式\n\nJWT 应包含三部分，以点号分隔：header.payload.signature"));
        m_copyCurrentBtn->setEnabled(false);
        m_copyAllBtn->setEnabled(false);
        return;
    }

    {
        QByteArray decoded = base64UrlDecode(parts[0].toUtf8());
        if (decoded.isEmpty() && !parts[0].isEmpty()) {
            m_headerEdit->setPlainText(QStringLiteral("无法解码 Header (Base64url 解码失败)"));
        } else {
            QJsonDocument doc = QJsonDocument::fromJson(decoded);
            if (doc.isObject()) {
                m_headerEdit->setPlainText(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
            } else {
                m_headerEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
            }
        }
    }

    {
        QByteArray decoded = base64UrlDecode(parts[1].toUtf8());
        if (decoded.isEmpty() && !parts[1].isEmpty()) {
            m_payloadEdit->setPlainText(QStringLiteral("无法解码 Payload (Base64url 解码失败)"));
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

                m_payloadEdit->setPlainText(payloadStr);
            } else {
                m_payloadEdit->setPlainText(QString::fromUtf8(decoded) + QStringLiteral("\n\n[警告: 不是有效的 JSON]"));
            }
        }
    }

    m_signatureEdit->setPlainText(parts[2]);

    m_resultTabs->setCurrentIndex(1);
    m_copyCurrentBtn->setEnabled(true);
    m_copyAllBtn->setEnabled(true);
}

void JwtTool::onClear()
{
    m_inputEdit->clear();
    m_headerEdit->clear();
    m_payloadEdit->clear();
    m_signatureEdit->clear();
    m_infoLabel->clear();
    m_copyCurrentBtn->setEnabled(false);
    m_copyAllBtn->setEnabled(false);
}

void JwtTool::onCopyCurrent()
{
    QTextEdit *current = qobject_cast<QTextEdit *>(m_resultTabs->currentWidget());
    if (!current)
        return;
    QString text = current->toPlainText();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_copyCurrentBtn->text();
    m_copyCurrentBtn->setText(QStringLiteral("已复制"));
    m_copyCurrentBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyCurrentBtn->setText(original);
        m_copyCurrentBtn->setEnabled(true);
    });
}

void JwtTool::onCopyAll()
{
    QString text = QStringLiteral("=== Header ===\n%1\n\n=== Payload ===\n%2\n\n=== Signature ===\n%3")
        .arg(m_headerEdit->toPlainText(), m_payloadEdit->toPlainText(), m_signatureEdit->toPlainText());
    QApplication::clipboard()->setText(text);
    QString original = m_copyAllBtn->text();
    m_copyAllBtn->setText(QStringLiteral("已复制"));
    m_copyAllBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_copyAllBtn->setText(original);
        m_copyAllBtn->setEnabled(true);
    });
}
