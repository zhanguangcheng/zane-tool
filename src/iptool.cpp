#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QNetworkReply>
#include <QNetworkInterface>
#include <QJsonDocument>
#include <QJsonObject>

#include "iptool.h"

IpTool::IpTool(QWidget *parent)
    : QWidget(parent)
    , m_ipNetworkManager(new QNetworkAccessManager(this))
{
    setupUi();
}

void IpTool::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(16);
    mainLayout->setContentsMargins(20, 20, 20, 20);

    QGroupBox *lanGroup = new QGroupBox(QStringLiteral("\u5C40\u57DF\u7F51IP"), this);
    QVBoxLayout *lanLayout = new QVBoxLayout(lanGroup);
    lanLayout->setSpacing(8);

    QHBoxLayout *lanRow = new QHBoxLayout();
    m_ipLanEdit = new QTextEdit(lanGroup);
    m_ipLanEdit->setReadOnly(true);
    m_ipLanEdit->setFocusPolicy(Qt::NoFocus);
    m_ipLanEdit->setPlaceholderText(QStringLiteral("\u81EA\u52A8\u68C0\u6D4B\u4E2D..."));
    m_ipLanEdit->setMaximumHeight(144);
    m_ipLanEdit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_ipLanEdit->setStyleSheet(QStringLiteral(
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
    m_ipCopyLanBtn = new QPushButton(QStringLiteral("\u590D\u5236"), lanGroup);
    m_ipCopyLanBtn->setCursor(Qt::PointingHandCursor);
    m_ipCopyLanBtn->setFixedHeight(28);
    m_ipCopyLanBtn->setFixedWidth(60);
    m_ipCopyLanBtn->setEnabled(false);
    m_ipCopyLanBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipCopyLanBtn, &QPushButton::clicked, this, &IpTool::onIpCopyLan);
    lanRow->addWidget(m_ipLanEdit, 1);
    lanRow->addWidget(m_ipCopyLanBtn);
    lanLayout->addLayout(lanRow);

    mainLayout->addWidget(lanGroup);

    QGroupBox *wanGroup = new QGroupBox(QStringLiteral("\u5916\u7F51IP"), this);
    QVBoxLayout *wanLayout = new QVBoxLayout(wanGroup);
    wanLayout->setSpacing(8);

    QHBoxLayout *wanSourceRow = new QHBoxLayout();
    QLabel *wanSourceLabel = new QLabel(QStringLiteral("\u67E5\u8BE2\u6E90:"), wanGroup);
    wanSourceLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    m_ipWanSourceCombo = new QComboBox(wanGroup);
    m_ipWanSourceCombo->addItem(QStringLiteral("icanhazip.com"), QStringLiteral("http://icanhazip.com"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ifconfig.me/ip"), QStringLiteral("http://ifconfig.me/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ipinfo.io/ip"), QStringLiteral("http://ipinfo.io/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ipecho.net/plain"), QStringLiteral("http://ipecho.net/plain"));
    m_ipWanSourceCombo->addItem(QStringLiteral("www.trackip.net/ip"), QStringLiteral("http://www.trackip.net/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("httpbin.org/ip"), QStringLiteral("http://httpbin.org/ip"));
    m_ipWanSourceCombo->addItem(QStringLiteral("ip.sb"), QStringLiteral("http://ip.sb"));
    m_ipWanQueryBtn = new QPushButton(QStringLiteral("\u67E5\u8BE2"), wanGroup);
    m_ipWanQueryBtn->setCursor(Qt::PointingHandCursor);
    m_ipWanQueryBtn->setFixedHeight(28);
    m_ipWanQueryBtn->setFixedWidth(60);
    m_ipWanQueryBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipWanQueryBtn, &QPushButton::clicked, this, &IpTool::onIpWanQuery);
    wanSourceRow->addWidget(wanSourceLabel);
    wanSourceRow->addWidget(m_ipWanSourceCombo, 1);
    wanSourceRow->addWidget(m_ipWanQueryBtn);
    wanLayout->addLayout(wanSourceRow);

    QHBoxLayout *wanRow = new QHBoxLayout();
    m_ipWanEdit = new QLineEdit(wanGroup);
    m_ipWanEdit->setReadOnly(true);
    m_ipWanEdit->setFocusPolicy(Qt::NoFocus);
    m_ipWanEdit->setPlaceholderText(QStringLiteral("\u70B9\u51FB\u67E5\u8BE2\u83B7\u53D6\u5916\u7F51IP"));
    m_ipCopyWanBtn = new QPushButton(QStringLiteral("\u590D\u5236"), wanGroup);
    m_ipCopyWanBtn->setCursor(Qt::PointingHandCursor);
    m_ipCopyWanBtn->setFixedHeight(28);
    m_ipCopyWanBtn->setFixedWidth(60);
    m_ipCopyWanBtn->setEnabled(false);
    m_ipCopyWanBtn->setStyleSheet(QStringLiteral("font-size: 12px;"));
    connect(m_ipCopyWanBtn, &QPushButton::clicked, this, &IpTool::onIpCopyWan);
    wanRow->addWidget(m_ipWanEdit, 1);
    wanRow->addWidget(m_ipCopyWanBtn);
    wanLayout->addLayout(wanRow);

    mainLayout->addWidget(wanGroup);
    mainLayout->addStretch(1);

    QTimer::singleShot(0, this, [this]() { refreshLanIps(); });
}

void IpTool::refreshLanIps()
{
    QStringList lanIps;
    const QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : interfaces) {
        if (iface.flags().testFlag(QNetworkInterface::IsUp)
            && iface.flags().testFlag(QNetworkInterface::IsRunning)
            && !iface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            const QList<QNetworkAddressEntry> entries = iface.addressEntries();
            for (const QNetworkAddressEntry &entry : entries) {
                QHostAddress addr = entry.ip();
                if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
                    lanIps.append(iface.humanReadableName() + QStringLiteral(": ") + addr.toString());
                }
            }
        }
    }

    if (lanIps.isEmpty()) {
        m_ipLanText.clear();
        m_ipLanEdit->setPlainText(QStringLiteral("\u672A\u68C0\u6D4B\u5230\u5C40\u57DF\u7F51IP"));
        m_ipLanEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: 1px solid #ced4da;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #dc3545;"
            "}"
            "QTextEdit:focus { border-color: #86b7fe; }"));
        m_ipCopyLanBtn->setEnabled(false);
    } else {
        m_ipLanText = lanIps.join(QStringLiteral("\n"));
        m_ipLanEdit->setPlainText(m_ipLanText);
        m_ipLanEdit->setStyleSheet(QStringLiteral(
            "QTextEdit {"
            "  font-family: 'Consolas', 'Courier New', monospace;"
            "  font-size: 12px;"
            "  border: 1px solid #ced4da;"
            "  border-radius: 6px;"
            "  padding: 10px;"
            "  background-color: #ffffff;"
            "  color: #198754;"
            "}"
            "QTextEdit:focus { border-color: #86b7fe; }"));
        m_ipCopyLanBtn->setEnabled(true);
    }
}

void IpTool::onIpWanQuery()
{
    QString sourceUrl = m_ipWanSourceCombo->currentData().toString();
    m_ipWanEdit->setText(QStringLiteral("\u67E5\u8BE2\u4E2D..."));
    m_ipWanEdit->setStyleSheet(QStringLiteral("color: #6c757d;"));
    m_ipCopyWanBtn->setEnabled(false);
    m_ipWanQueryBtn->setEnabled(false);

    QUrl url(sourceUrl);
    QNetworkRequest request(url);
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_ipNetworkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        m_ipWanQueryBtn->setEnabled(true);

        if (reply->error() != QNetworkReply::NoError) {
            m_ipWanEdit->setText(QStringLiteral("\u67E5\u8BE2\u5931\u8D25: %1").arg(reply->errorString()));
            m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
            m_ipCopyWanBtn->setEnabled(false);
            return;
        }

        QByteArray data = reply->readAll();
        QString sourceUrl = reply->url().toString();

        if (sourceUrl.contains(QStringLiteral("httpbin.org"))) {
            QJsonDocument doc = QJsonDocument::fromJson(data);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                QString ip = obj.value(QStringLiteral("origin")).toString();
                if (!ip.isEmpty()) {
                    int commaPos = ip.indexOf(',');
                    if (commaPos > 0)
                        ip = ip.left(commaPos).trimmed();
                    m_ipWanEdit->setText(ip);
                    m_ipWanEdit->setStyleSheet(QStringLiteral("color: #198754;"));
                    m_ipCopyWanBtn->setEnabled(true);
                    return;
                }
            }
            m_ipWanEdit->setText(QStringLiteral("\u89E3\u6790\u5931\u8D25"));
            m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
        } else {
            QString ip = QString::fromUtf8(data).trimmed();
            if (ip.isEmpty()) {
                m_ipWanEdit->setText(QStringLiteral("\u67E5\u8BE2\u7ED3\u679C\u4E3A\u7A7A"));
                m_ipWanEdit->setStyleSheet(QStringLiteral("color: #dc3545;"));
                m_ipCopyWanBtn->setEnabled(false);
            } else {
                m_ipWanEdit->setText(ip);
                m_ipWanEdit->setStyleSheet(QStringLiteral("color: #198754;"));
                m_ipCopyWanBtn->setEnabled(true);
            }
        }
    });
}

void IpTool::onIpCopyLan()
{
    QString text = m_ipLanEdit->toPlainText();
    if (text.isEmpty() || m_ipLanText.isEmpty())
        return;
    QApplication::clipboard()->setText(m_ipLanText);
    QString original = m_ipCopyLanBtn->text();
    m_ipCopyLanBtn->setText(QStringLiteral("\u5DF2\u590D\u5236"));
    m_ipCopyLanBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_ipCopyLanBtn->setText(original);
        m_ipCopyLanBtn->setEnabled(true);
    });
}

void IpTool::onIpCopyWan()
{
    QString text = m_ipWanEdit->text();
    if (text.isEmpty())
        return;
    QApplication::clipboard()->setText(text);
    QString original = m_ipCopyWanBtn->text();
    m_ipCopyWanBtn->setText(QStringLiteral("\u5DF2\u590D\u5236"));
    m_ipCopyWanBtn->setEnabled(false);
    QTimer::singleShot(1500, this, [this, original]() {
        m_ipCopyWanBtn->setText(original);
        m_ipCopyWanBtn->setEnabled(true);
    });
}
