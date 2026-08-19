#include <QNetworkReply>
#include <QNetworkRequest>
#include <QCoreApplication>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDesktopServices>
#include <QUrl>
#include <QVersionNumber>

#include "updatetool.h"

namespace {

const QStringList kVersionSources = {
    QStringLiteral("https://gh-proxy.com/https://github.com/zhanguangcheng/zane-tool/raw/refs/heads/main/version.txt"),
    QStringLiteral("https://gh.catmak.name/https://raw.githubusercontent.com/zhanguangcheng/zane-tool/main/version.txt"),
    QStringLiteral("https://github.com/zhanguangcheng/zane-tool/raw/refs/heads/main/version.txt"),
};

const QString kDownloadProxyTemplate = QStringLiteral(
    "https://gh-proxy.com/https://github.com/zhanguangcheng/zane-tool/releases/download/v%1/ZaneTool-%1-setup.exe");
const QString kDownloadGithubTemplate = QStringLiteral(
    "https://github.com/zhanguangcheng/zane-tool/releases/download/v%1/ZaneTool-%1-setup.exe");

} // namespace

void UpdateTool::checkForUpdate(QWidget *parent, bool silentNoUpdate)
{
    UpdateTool *tool = new UpdateTool(parent, silentNoUpdate);
    QObject::connect(tool, &UpdateTool::done, tool, &QObject::deleteLater);

    if (silentNoUpdate)
        tool->processNextVersionSource();
    else
        tool->manualCheck();
}

UpdateTool::UpdateTool(QWidget *parent, bool silentNoUpdate)
    : QObject(nullptr)
    , m_parent(parent)
    , m_manager(new QNetworkAccessManager(this))
    , m_silentNoUpdate(silentNoUpdate)
{
}

void UpdateTool::manualCheck()
{
    buildDialog();
    processNextVersionSource();
    m_dialog->exec();
    emit done();
}

void UpdateTool::processNextVersionSource()
{
    if (m_sourceIndex >= kVersionSources.size()) {
        setDialogFailed();
        finish();
        return;
    }

    QNetworkRequest request(QUrl(kVersionSources.at(m_sourceIndex++)));
    request.setTransferTimeout(10000);
    QNetworkReply *reply = m_manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        versionReplyFinished(reply);
    });
}

void UpdateTool::versionReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        processNextVersionSource();
        return;
    }

    QString text = QString::fromUtf8(reply->readAll()).trimmed();
    int start = 0;
    while (start < text.size() && !text.at(start).isDigit())
        ++start;
    text = text.mid(start);

    const QVersionNumber latest = QVersionNumber::fromString(text);
    if (latest.isNull()) {
        processNextVersionSource();
        return;
    }

    m_latest = latest.toString();
    handleLatest();
}

void UpdateTool::handleLatest()
{
    const QVersionNumber latest = QVersionNumber::fromString(m_latest);
    const QVersionNumber current = QVersionNumber::fromString(QCoreApplication::applicationVersion());

    if (m_silentNoUpdate) {
        if (latest > current) {
            buildDialog();
            setDialogSuccess();
            m_dialog->exec();
        }
        finish();
        return;
    }

    setDialogSuccess();
}

void UpdateTool::buildDialog()
{
    m_dialog = new QDialog(m_parent);
    m_dialog->setWindowTitle(QStringLiteral("检查更新"));
    m_dialog->setModal(true);
    m_dialog->resize(480, 280);

    auto *layout = new QVBoxLayout(m_dialog);
    layout->setContentsMargins(16, 16, 16, 12);
    layout->setSpacing(12);

    m_label = new QLabel(QStringLiteral("正在检测最新版本，请稍候..."), m_dialog);
    m_label->setWordWrap(true);
    m_label->setTextFormat(Qt::RichText);
    m_label->setTextInteractionFlags(Qt::TextBrowserInteraction);
    m_label->setStyleSheet(QStringLiteral(
        "QLabel { font-size: 13px; color: #212529; }"
        "QLabel a { color: #0d6efd; text-decoration: none; }"));
    connect(m_label, &QLabel::linkActivated, this, [this](const QString &link) {
        QDesktopServices::openUrl(QUrl(link));
    });
    layout->addWidget(m_label);

    auto *btnRow = new QHBoxLayout();
    btnRow->addStretch(1);
    m_proxyBtn = new QPushButton(QStringLiteral("国内下载"), m_dialog);
    m_githubBtn = new QPushButton(QStringLiteral("GitHub 下载"), m_dialog);
    auto *closeBtn = new QPushButton(QStringLiteral("关闭"), m_dialog);
    m_proxyBtn->setEnabled(false);
    m_githubBtn->setEnabled(false);
    connect(m_proxyBtn, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(m_proxyUrl));
    });
    connect(m_githubBtn, &QPushButton::clicked, this, [this]() {
        QDesktopServices::openUrl(QUrl(m_githubUrl));
    });
    connect(closeBtn, &QPushButton::clicked, m_dialog, &QDialog::accept);
    btnRow->addWidget(m_proxyBtn);
    btnRow->addWidget(m_githubBtn);
    btnRow->addWidget(closeBtn);
    layout->addLayout(btnRow);
}

void UpdateTool::setDialogSuccess()
{
    m_proxyUrl = kDownloadProxyTemplate.arg(m_latest);
    m_githubUrl = kDownloadGithubTemplate.arg(m_latest);

    const QString current = QCoreApplication::applicationVersion();
    const QVersionNumber cur = QVersionNumber::fromString(current);
    const QVersionNumber newest = QVersionNumber::fromString(m_latest);
    const QString heading = newest > cur
        ? QStringLiteral("发现新版本，可下载更新")
        : QStringLiteral("当前已是最新版本，仍可下载安装包");

    m_label->setText(QStringLiteral(
        "<h3>%1</h3>"
        "<p>当前版本：<b>v%2</b><br>"
        "最新版本：<b style=\"color:#dc3545;\">v%3</b></p>"
        "<p>下载地址：<br>"
        "<a href=\"%4\">%4</a><br>"
        "<a href=\"%5\">%5</a></p>")
        .arg(heading, current, m_latest, m_proxyUrl, m_githubUrl));
    m_proxyBtn->setEnabled(true);
    m_githubBtn->setEnabled(true);
}

void UpdateTool::setDialogFailed()
{
    if (m_dialog)
        m_label->setText(QStringLiteral("检查更新失败，请检查网络连接后重试。"));
}

void UpdateTool::finish()
{
    if (m_silentNoUpdate)
        emit done();
}