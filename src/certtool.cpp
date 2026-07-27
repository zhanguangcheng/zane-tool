#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QRegularExpression>

#include <windows.h>
#include <shellapi.h>

#include "certtool.h"
#include "utils.h"

CertTool::CertTool(QWidget *mainWidget, const QString &mkcertPath, QObject *parent)
    : QObject(parent)
    , m_mainWidget(mainWidget)
    , m_mkcertPath(mkcertPath)
    , m_certCaStatusLabel(nullptr)
    , m_certCarootLabel(nullptr)
    , m_certInstallCaBtn(nullptr)
    , m_certUninstallCaBtn(nullptr)
    , m_certOpenCarootBtn(nullptr)
    , m_certDomainsInput(nullptr)
    , m_certNameEdit(nullptr)
    , m_certOutputDir(nullptr)
    , m_certOutputBrowseBtn(nullptr)
    , m_certGenerateBtn(nullptr)
    , m_certOpenOutputBtn(nullptr)
    , m_certLogOutput(nullptr)
    , m_certProcess(nullptr)
    , m_certRunning(false)
{
}

QWidget *CertTool::createPage()
{
    QWidget *page = new QWidget();
    QVBoxLayout *mainLayout = new QVBoxLayout(page);
    mainLayout->setSpacing(10);

    QGroupBox *caGroup = new QGroupBox(QStringLiteral("本地 CA（根证书）"), page);
    QVBoxLayout *caLayout = new QVBoxLayout(caGroup);

    m_certCaStatusLabel = new QLabel(QStringLiteral("状态: 检测中…"), caGroup);
    caLayout->addWidget(m_certCaStatusLabel);

    QHBoxLayout *carootLayout = new QHBoxLayout();
    carootLayout->addWidget(new QLabel(QStringLiteral("CA 目录:"), caGroup));
    m_certCarootLabel = new QLabel(QStringLiteral("-"), caGroup);
    m_certCarootLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_certCarootLabel->setStyleSheet(QStringLiteral("color:#6c757d;"));
    carootLayout->addWidget(m_certCarootLabel, 1);
    caLayout->addLayout(carootLayout);

    QHBoxLayout *caBtnLayout = new QHBoxLayout();
    m_certInstallCaBtn = new QPushButton(QStringLiteral("安装到系统信任"), caGroup);
    m_certUninstallCaBtn = new QPushButton(QStringLiteral("卸载根证书"), caGroup);
    m_certUninstallCaBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_certOpenCarootBtn = new QPushButton(QStringLiteral("打开 CA 目录"), caGroup);
    connect(m_certInstallCaBtn, &QPushButton::clicked, this, &CertTool::onCertInstallCa);
    connect(m_certUninstallCaBtn, &QPushButton::clicked, this, &CertTool::onCertUninstallCa);
    connect(m_certOpenCarootBtn, &QPushButton::clicked, this, &CertTool::onCertOpenCaroot);
    caBtnLayout->addWidget(m_certInstallCaBtn);
    caBtnLayout->addWidget(m_certUninstallCaBtn);
    caBtnLayout->addWidget(m_certOpenCarootBtn);
    caBtnLayout->addStretch();
    caLayout->addLayout(caBtnLayout);

    QLabel *caHint = new QLabel(QStringLiteral("安装/卸载需要管理员权限，将弹出 UAC 授权窗口。"), caGroup);
    caHint->setStyleSheet(QStringLiteral("color:#6c757d;"));
    caLayout->addWidget(caHint);

    QGroupBox *genGroup = new QGroupBox(QStringLiteral("生成证书"), page);
    QVBoxLayout *genLayout = new QVBoxLayout(genGroup);

    genLayout->addWidget(new QLabel(QStringLiteral("域名 / IP（每行一个，支持通配符 *.example.com）:"), genGroup));
    m_certDomainsInput = new QTextEdit(genGroup);
    m_certDomainsInput->setPlaceholderText(QStringLiteral("localhost\n127.0.0.1\n::1\n*.example.com"));
    m_certDomainsInput->setAcceptRichText(false);
    m_certDomainsInput->setMaximumHeight(90);
    m_certDomainsInput->setStyleSheet(QStringLiteral(
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
    genLayout->addWidget(m_certDomainsInput);

    QHBoxLayout *quickLayout = new QHBoxLayout();
    quickLayout->addWidget(new QLabel(QStringLiteral("快速添加:"), genGroup));
    const QStringList quickNames = {
        QStringLiteral("localhost"), QStringLiteral("127.0.0.1"),
        QStringLiteral("::1"), QStringLiteral("*.localhost")
    };
    for (const QString &name : quickNames) {
        QPushButton *btn = new QPushButton(name, genGroup);
        connect(btn, &QPushButton::clicked, this, [this, name]() {
            const QStringList lines = m_certDomainsInput->toPlainText().split('\n', Qt::SkipEmptyParts);
            for (const QString &line : lines) {
                if (line.trimmed() == name)
                    return;
            }
            m_certDomainsInput->append(name);
        });
        quickLayout->addWidget(btn);
    }
    quickLayout->addStretch();
    genLayout->addLayout(quickLayout);

    QHBoxLayout *nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel(QStringLiteral("文件名:"), genGroup));
    m_certNameEdit = new QLineEdit(QStringLiteral("dev"), genGroup);
    m_certNameEdit->setPlaceholderText(QStringLiteral("dev → dev.pem / dev-key.pem"));
    nameLayout->addWidget(m_certNameEdit, 1);
    genLayout->addLayout(nameLayout);

    QHBoxLayout *dirLayout = new QHBoxLayout();
    dirLayout->addWidget(new QLabel(QStringLiteral("输出目录:"), genGroup));
    m_certOutputDir = new QLineEdit(genGroup);
    m_certOutputDir->setPlaceholderText(QStringLiteral("必填，点击「浏览」选择证书输出目录"));
    m_certOutputBrowseBtn = new QPushButton(QStringLiteral("浏览"), genGroup);
    connect(m_certOutputBrowseBtn, &QPushButton::clicked, this, &CertTool::onCertOutputBrowse);
    dirLayout->addWidget(m_certOutputDir, 1);
    dirLayout->addWidget(m_certOutputBrowseBtn);
    genLayout->addLayout(dirLayout);

    QHBoxLayout *genBtnLayout = new QHBoxLayout();
    m_certGenerateBtn = new QPushButton(QStringLiteral("生成证书"), genGroup);
    m_certOpenOutputBtn = new QPushButton(QStringLiteral("打开输出目录"), genGroup);
    connect(m_certGenerateBtn, &QPushButton::clicked, this, &CertTool::onCertGenerate);
    connect(m_certOpenOutputBtn, &QPushButton::clicked, this, &CertTool::onCertOpenOutputDir);
    genBtnLayout->addWidget(m_certGenerateBtn);
    genBtnLayout->addWidget(m_certOpenOutputBtn);
    genBtnLayout->addStretch();
    genLayout->addLayout(genBtnLayout);

    QGroupBox *logGroup = new QGroupBox(QStringLiteral("日志"), page);
    QVBoxLayout *logLayout = new QVBoxLayout(logGroup);
    m_certLogOutput = new QTextEdit(logGroup);
    m_certLogOutput->setReadOnly(true);
    m_certLogOutput->setStyleSheet(QStringLiteral(
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
    logLayout->addWidget(m_certLogOutput);

    mainLayout->addWidget(caGroup);
    mainLayout->addWidget(genGroup);
    mainLayout->addWidget(logGroup, 1);

    QTimer::singleShot(0, this, [this]() { refreshCertCaStatus(); });

    return page;
}

void CertTool::refreshCertCaStatus()
{
    QProcess *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus exitStatus) {
        proc->deleteLater();

        if (exitStatus != QProcess::NormalExit || exitCode != 0) {
            m_certCaStatusLabel->setText(QStringLiteral("状态: 检测失败"));
            return;
        }

        m_certCarootPath = QString::fromUtf8(proc->readAllStandardOutput()).trimmed();
        m_certCarootLabel->setText(m_certCarootPath.isEmpty() ? QStringLiteral("-") : m_certCarootPath);
        m_certCarootLabel->setToolTip(m_certCarootPath);
        m_certOpenCarootBtn->setEnabled(!m_certCarootPath.isEmpty());

        bool caExists = !m_certCarootPath.isEmpty()
            && QFileInfo::exists(m_certCarootPath + QStringLiteral("/rootCA.pem"));
        if (caExists) {
            m_certCaStatusLabel->setText(QStringLiteral(
                "状态: <span style='color:#198754;font-weight:bold;'>CA 已生成</span>"));
        } else {
            m_certCaStatusLabel->setText(QStringLiteral(
                "状态: <span style='color:#dc3545;font-weight:bold;'>CA 未生成</span>"
                "（点击「安装到系统信任」创建并信任）"));
        }
    });

    proc->start(m_mkcertPath, {QStringLiteral("-CAROOT")});
}

void CertTool::onCertInstallCa()
{
    QString path = QDir::toNativeSeparators(m_mkcertPath);
    QString args = QStringLiteral("-install");

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() != ERROR_CANCELLED) {
            QMessageBox::warning(m_mainWidget, QStringLiteral("错误"),
                QStringLiteral("无法以管理员身份启动 mkcert"));
        }
        return;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 60000);
        CloseHandle(sei.hProcess);
    }

    refreshCertCaStatus();
    m_certLogOutput->append(QStringLiteral("> mkcert -install（已请求管理员权限）"));
    Utils::logToFile(QStringLiteral("[CERT] mkcert -install requested"));
}

void CertTool::onCertUninstallCa()
{
    auto ret = QMessageBox::question(m_mainWidget, QStringLiteral("确认"),
        QStringLiteral("确定要从系统信任库卸载本地 CA 根证书吗？"));
    if (ret != QMessageBox::Yes)
        return;

    QString path = QDir::toNativeSeparators(m_mkcertPath);
    QString args = QStringLiteral("-uninstall");

    SHELLEXECUTEINFOW sei = {};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = reinterpret_cast<HWND>(m_mainWidget->winId());
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(path.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(args.utf16());
    sei.nShow = SW_SHOW;

    if (!ShellExecuteExW(&sei)) {
        if (GetLastError() != ERROR_CANCELLED) {
            QMessageBox::warning(m_mainWidget, QStringLiteral("错误"),
                QStringLiteral("无法以管理员身份启动 mkcert"));
        }
        return;
    }

    if (sei.hProcess) {
        WaitForSingleObject(sei.hProcess, 60000);
        CloseHandle(sei.hProcess);
    }

    refreshCertCaStatus();
    m_certLogOutput->append(QStringLiteral("> mkcert -uninstall（已请求管理员权限）"));
    Utils::logToFile(QStringLiteral("[CERT] mkcert -uninstall requested"));
}

void CertTool::onCertOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(m_mainWidget, QStringLiteral("选择输出目录"),
                                                    m_certOutputDir->text());
    if (!dir.isEmpty())
        m_certOutputDir->setText(dir);
}

void CertTool::onCertOpenOutputDir()
{
    QString dir = m_certOutputDir->text().trimmed();
    if (dir.isEmpty())
        return;
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void CertTool::onCertOpenCaroot()
{
    if (m_certCarootPath.isEmpty())
        return;
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_certCarootPath));
}

void CertTool::onCertGenerate()
{
    if (m_certRunning)
        return;

    QStringList domains;
    const QStringList lines = m_certDomainsInput->toPlainText().split('\n', Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const QStringList parts = line.split(QRegularExpression(QStringLiteral("[,;\\s]+")),
                                             Qt::SkipEmptyParts);
        for (const QString &part : parts)
            domains << part.trimmed();
    }

    if (domains.isEmpty()) {
        QMessageBox::information(m_mainWidget, QStringLiteral("提示"),
            QStringLiteral("请输入至少一个域名或 IP"));
        return;
    }

    QRegularExpression validRe(QStringLiteral("^[A-Za-z0-9*_.\\-:]+$"));
    for (const QString &d : domains) {
        if (!validRe.match(d).hasMatch()) {
            QMessageBox::warning(m_mainWidget, QStringLiteral("提示"),
                QStringLiteral("包含非法字符的条目: %1").arg(d));
            return;
        }
    }

    QString name = m_certNameEdit->text().trimmed();
    name.remove(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")));
    if (name.isEmpty())
        name = QStringLiteral("dev");

    QString outDir = m_certOutputDir->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::information(m_mainWidget, QStringLiteral("提示"),
            QStringLiteral("请先选择输出目录"));
        return;
    }
    if (!QDir().mkpath(outDir)) {
        QMessageBox::warning(m_mainWidget, QStringLiteral("错误"),
            QStringLiteral("无法创建输出目录: %1").arg(outDir));
        return;
    }

    QString certFile = outDir + QStringLiteral("/") + name + QStringLiteral(".pem");
    QString keyFile = outDir + QStringLiteral("/") + name + QStringLiteral("-key.pem");

    QStringList args;
    args << QStringLiteral("-cert-file") << certFile
         << QStringLiteral("-key-file") << keyFile;
    args << domains;

    if (m_certProcess) {
        m_certProcess->deleteLater();
        m_certProcess = nullptr;
    }
    m_certProcess = new QProcess(this);
    m_certProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_certProcess, &QProcess::readyReadStandardOutput,
            this, &CertTool::onCertProcessOutput);

    connect(m_certProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, certFile, keyFile](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                m_certRunning = false;
                m_certGenerateBtn->setEnabled(true);
                m_certDomainsInput->setEnabled(true);
                m_certNameEdit->setEnabled(true);
                m_certOutputBrowseBtn->setEnabled(true);

                bool ok = exitCode == 0
                    && QFileInfo::exists(certFile) && QFileInfo(certFile).size() > 0
                    && QFileInfo::exists(keyFile) && QFileInfo(keyFile).size() > 0;

                if (ok) {
                    m_certLogOutput->append(
                        QStringLiteral("证书生成成功:\n  %1\n  %2").arg(certFile, keyFile));
                    Utils::logToFile(QStringLiteral("[CERT] Generated: %1").arg(certFile));
                    QMessageBox::information(m_mainWidget, QStringLiteral("成功"),
                        QStringLiteral("证书生成成功：\n%1\n%2").arg(certFile, keyFile));
                } else {
                    m_certLogOutput->append(
                        QStringLiteral("生成失败，退出码: %1").arg(exitCode));
                    Utils::logToFile(QStringLiteral("[CERT] Generate failed, code=%1").arg(exitCode));
                }
            });

    connect(m_certProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                Q_UNUSED(error);
                m_certRunning = false;
                m_certGenerateBtn->setEnabled(true);
                m_certDomainsInput->setEnabled(true);
                m_certNameEdit->setEnabled(true);
                m_certOutputBrowseBtn->setEnabled(true);
                m_certLogOutput->append(
                    QStringLiteral("进程错误: ") + m_certProcess->errorString());
            });

    m_certRunning = true;
    m_certGenerateBtn->setEnabled(false);
    m_certDomainsInput->setEnabled(false);
    m_certNameEdit->setEnabled(false);
    m_certOutputBrowseBtn->setEnabled(false);

    m_certLogOutput->append(QStringLiteral("> mkcert %1").arg(args.join(' ')));
    m_certProcess->start(m_mkcertPath, args);
}

void CertTool::onCertProcessOutput()
{
    if (!m_certProcess)
        return;
    QString output = QString::fromUtf8(m_certProcess->readAllStandardOutput());
    m_certLogOutput->append(output.trimmed());
}
