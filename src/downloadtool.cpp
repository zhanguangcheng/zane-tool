#include <QApplication>
#include <QClipboard>
#include <QTimer>
#include <QHeaderView>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include <QRegularExpression>

#include "downloadtool.h"
#include "utils.h"

namespace {

qint64 parseAria2Size(const QString &text)
{
    static const QRegularExpression re(
        QStringLiteral("^([\\d.]+)\\s*(B|KiB|MiB|GiB|TiB|KB|MB|GB|TB|K|M|G)?$"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch()) return -1;

    bool ok = false;
    double value = m.captured(1).toDouble(&ok);
    if (!ok) return -1;

    QString unit = m.captured(2).toUpper();
    qint64 multiplier = 1;
    if (unit == QStringLiteral("K") || unit == QStringLiteral("KIB") || unit == QStringLiteral("KB"))
        multiplier = 1024;
    else if (unit == QStringLiteral("M") || unit == QStringLiteral("MIB") || unit == QStringLiteral("MB"))
        multiplier = 1024 * 1024;
    else if (unit == QStringLiteral("G") || unit == QStringLiteral("GIB") || unit == QStringLiteral("GB"))
        multiplier = 1024LL * 1024 * 1024;
    else if (unit == QStringLiteral("T") || unit == QStringLiteral("TIB") || unit == QStringLiteral("TB"))
        multiplier = 1024LL * 1024 * 1024 * 1024;

    return static_cast<qint64>(value * multiplier);
}

QString formatEtaText(qint64 seconds)
{
    if (seconds < 0) return QStringLiteral("—");
    if (seconds < 60)
        return QStringLiteral("%1s").arg(seconds);
    if (seconds < 3600)
        return QStringLiteral("%1m%2s").arg(seconds / 60).arg(seconds % 60);
    return QStringLiteral("%1h%2m").arg(seconds / 3600).arg((seconds % 3600) / 60);
}

} // namespace

DownloadTool::DownloadTool(const QString &aria2Path, QWidget *parent)
    : QWidget(parent)
    , m_aria2Path(aria2Path)
    , m_downloadUrlInput(nullptr)
    , m_downloadAddFileBtn(nullptr)
    , m_downloadPasteBtn(nullptr)
    , m_downloadClearBtn(nullptr)
    , m_downloadMaxConcurrent(nullptr)
    , m_downloadMaxConnections(nullptr)
    , m_downloadSpeedLimit(nullptr)
    , m_downloadAllowOverwrite(nullptr)
    , m_downloadOutputDir(nullptr)
    , m_downloadOutputBrowse(nullptr)
    , m_downloadProgressTable(nullptr)
    , m_downloadProgressBar(nullptr)
    , m_downloadStatusLabel(nullptr)
    , m_downloadStartBtn(nullptr)
    , m_downloadCancelBtn(nullptr)
    , m_downloadProcess(nullptr)
    , m_downloadEtaTimer(nullptr)
    , m_downloadCompleted(0)
    , m_downloadFailed(0)
    , m_downloadCancelling(false)
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    // ---- URL Input Group ----
    QGroupBox *urlGroup = new QGroupBox(QStringLiteral("下载地址"), this);
    QVBoxLayout *urlLayout = new QVBoxLayout(urlGroup);
    urlLayout->setSpacing(6);

    QLabel *urlHint = new QLabel(
        QStringLiteral("每行一个URL，可在URL后加空格或Tab指定自定义文件名"), urlGroup);
    urlHint->setStyleSheet(QStringLiteral("color: #6c757d; font-size: 12px;"));

    m_downloadUrlInput = new QTextEdit(urlGroup);
    m_downloadUrlInput->setPlaceholderText(QStringLiteral(
        "http://example.com/file.zip\n"
        "http://example.com/report.pdf 月报.pdf\n"
        "http://example.com/data 数据包.zip"));
    m_downloadUrlInput->setMinimumHeight(80);
    m_downloadUrlInput->setAcceptRichText(false);
    m_downloadUrlInput->setStyleSheet(QStringLiteral(
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

    QHBoxLayout *urlBtnRow = new QHBoxLayout();
    urlBtnRow->setSpacing(8);
    m_downloadAddFileBtn = new QPushButton(QStringLiteral("添加文本文件"), urlGroup);
    m_downloadAddFileBtn->setFixedHeight(30);
    m_downloadAddFileBtn->setCursor(Qt::PointingHandCursor);
    m_downloadAddFileBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_downloadAddFileBtn, &QPushButton::clicked, this, &DownloadTool::onDownloadAddFile);

    m_downloadPasteBtn = new QPushButton(QStringLiteral("从剪贴板粘贴"), urlGroup);
    m_downloadPasteBtn->setFixedHeight(30);
    m_downloadPasteBtn->setCursor(Qt::PointingHandCursor);
    m_downloadPasteBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background-color: #6c757d; color: #fff; border: none; "
        "  border-radius: 4px; font-size: 12px; padding: 0 12px; }"
        "QPushButton:hover { background-color: #5c636a; }"));
    connect(m_downloadPasteBtn, &QPushButton::clicked, this, &DownloadTool::onDownloadPaste);

    m_downloadClearBtn = new QPushButton(QStringLiteral("清空"), urlGroup);
    m_downloadClearBtn->setFixedHeight(30);
    m_downloadClearBtn->setCursor(Qt::PointingHandCursor);
    m_downloadClearBtn->setObjectName(QStringLiteral("dangerBtn"));
    connect(m_downloadClearBtn, &QPushButton::clicked, this, &DownloadTool::onDownloadClear);

    urlBtnRow->addWidget(m_downloadAddFileBtn);
    urlBtnRow->addWidget(m_downloadPasteBtn);
    urlBtnRow->addWidget(m_downloadClearBtn);
    urlBtnRow->addStretch();

    urlLayout->addWidget(urlHint);
    urlLayout->addWidget(m_downloadUrlInput);
    urlLayout->addLayout(urlBtnRow);
    mainLayout->addWidget(urlGroup);

    // ---- Settings Row ----
    QHBoxLayout *settingsRow = new QHBoxLayout();
    settingsRow->setSpacing(12);

    QGroupBox *settingsGroup = new QGroupBox(QStringLiteral("下载设置"), this);
    QGridLayout *settingsGrid = new QGridLayout(settingsGroup);
    settingsGrid->setSpacing(8);

    settingsGrid->addWidget(new QLabel(QStringLiteral("最大并发下载:"), settingsGroup), 0, 0);
    m_downloadMaxConcurrent = new QSpinBox(settingsGroup);
    m_downloadMaxConcurrent->setRange(1, 32);
    m_downloadMaxConcurrent->setValue(5);
    settingsGrid->addWidget(m_downloadMaxConcurrent, 0, 1);

    settingsGrid->addWidget(new QLabel(QStringLiteral("单文件连接数:"), settingsGroup), 1, 0);
    m_downloadMaxConnections = new QSpinBox(settingsGroup);
    m_downloadMaxConnections->setRange(1, 32);
    m_downloadMaxConnections->setValue(16);
    settingsGrid->addWidget(m_downloadMaxConnections, 1, 1);

    settingsGrid->addWidget(new QLabel(QStringLiteral("限速(KB/s):"), settingsGroup), 2, 0);
    m_downloadSpeedLimit = new QSpinBox(settingsGroup);
    m_downloadSpeedLimit->setRange(0, 999999);
    m_downloadSpeedLimit->setValue(0);
    m_downloadSpeedLimit->setSpecialValueText(QStringLiteral("不限速"));
    m_downloadSpeedLimit->setSuffix(QStringLiteral(" KB/s"));
    settingsGrid->addWidget(m_downloadSpeedLimit, 2, 1);

    m_downloadAllowOverwrite = new QCheckBox(QStringLiteral("允许覆盖已存在文件"), settingsGroup);
    settingsGrid->addWidget(m_downloadAllowOverwrite, 3, 0, 1, 2);

    QGroupBox *outDirGroup = new QGroupBox(QStringLiteral("输出目录"), this);
    QVBoxLayout *outDirLayout = new QVBoxLayout(outDirGroup);
    outDirLayout->setSpacing(8);

    m_downloadOutputDir = new QLineEdit(outDirGroup);
    m_downloadOutputDir->setPlaceholderText(
        QStringLiteral("必填，请点击浏览选择输出目录"));
    m_downloadOutputBrowse = new QPushButton(QStringLiteral("浏览"), outDirGroup);
    m_downloadOutputBrowse->setFixedHeight(30);
    m_downloadOutputBrowse->setCursor(Qt::PointingHandCursor);
    connect(m_downloadOutputBrowse, &QPushButton::clicked, this, &DownloadTool::onDownloadOutputBrowse);

    outDirLayout->addWidget(m_downloadOutputDir);
    outDirLayout->addWidget(m_downloadOutputBrowse);

    settingsRow->addWidget(settingsGroup);
    settingsRow->addWidget(outDirGroup);
    mainLayout->addLayout(settingsRow);

    // ---- Progress Table ----
    m_downloadProgressTable = new QTableWidget(0, 6, this);
    m_downloadProgressTable->setHorizontalHeaderLabels({
        QStringLiteral("文件名"), QStringLiteral("进度"), QStringLiteral("大小"),
        QStringLiteral("速度"), QStringLiteral("剩余时间"), QStringLiteral("状态")});
    m_downloadProgressTable->horizontalHeader()->setStretchLastSection(true);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(1, 160);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(2, 80);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(3, 90);
    m_downloadProgressTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Fixed);
    m_downloadProgressTable->horizontalHeader()->resizeSection(4, 70);
    m_downloadProgressTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_downloadProgressTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_downloadProgressTable->verticalHeader()->setVisible(false);
    m_downloadProgressTable->setMinimumHeight(100);
    mainLayout->addWidget(m_downloadProgressTable);

    m_downloadEtaTimer = new QTimer(this);
    m_downloadEtaTimer->setInterval(500);
    connect(m_downloadEtaTimer, &QTimer::timeout, this, &DownloadTool::onDownloadEtaTick);

    // ---- Overall Progress ----
    m_downloadProgressBar = new QProgressBar(this);
    m_downloadProgressBar->setRange(0, 100);
    m_downloadProgressBar->setValue(0);
    m_downloadProgressBar->setTextVisible(true);
    m_downloadProgressBar->setFixedHeight(16);
    mainLayout->addWidget(m_downloadProgressBar);

    m_downloadStatusLabel = new QLabel(QStringLiteral("就绪"), this);
    mainLayout->addWidget(m_downloadStatusLabel);

    // ---- Action Buttons ----
    QHBoxLayout *actionLayout = new QHBoxLayout();
    actionLayout->setSpacing(16);
    m_downloadStartBtn = new QPushButton(QStringLiteral("开始下载"), this);
    m_downloadStartBtn->setFixedHeight(42);
    m_downloadStartBtn->setFixedWidth(160);
    m_downloadStartBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_downloadStartBtn->setCursor(Qt::PointingHandCursor);
    connect(m_downloadStartBtn, &QPushButton::clicked, this, &DownloadTool::onDownloadStart);

    m_downloadCancelBtn = new QPushButton(QStringLiteral("取消"), this);
    m_downloadCancelBtn->setFixedHeight(42);
    m_downloadCancelBtn->setFixedWidth(160);
    m_downloadCancelBtn->setObjectName(QStringLiteral("dangerBtn"));
    m_downloadCancelBtn->setStyleSheet(QStringLiteral("font-size: 15px; font-weight: bold;"));
    m_downloadCancelBtn->setCursor(Qt::PointingHandCursor);
    m_downloadCancelBtn->setEnabled(false);
    connect(m_downloadCancelBtn, &QPushButton::clicked, this, &DownloadTool::onDownloadCancel);

    actionLayout->addStretch();
    actionLayout->addWidget(m_downloadStartBtn);
    actionLayout->addSpacing(16);
    actionLayout->addWidget(m_downloadCancelBtn);
    actionLayout->addStretch();
    mainLayout->addLayout(actionLayout);
}

// ==================== Download Slots ====================

void DownloadTool::onDownloadAddFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, QStringLiteral("选择文本文件"),
        QString(), QStringLiteral("文本文件 (*.txt);;所有文件 (*)"));
    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法打开文件: %1").arg(filePath));
        return;
    }

    QTextStream in(&file);
    QStringList lines;
    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (!line.isEmpty() && !line.startsWith('#'))
            lines.append(line);
    }
    file.close();

    if (!lines.isEmpty()) {
        QString existing = m_downloadUrlInput->toPlainText().trimmed();
        if (!existing.isEmpty())
            existing += '\n';
        m_downloadUrlInput->setPlainText(existing + lines.join('\n'));
    }
}

void DownloadTool::onDownloadPaste()
{
    QString text = QApplication::clipboard()->text().trimmed();
    if (text.isEmpty()) return;

    QString existing = m_downloadUrlInput->toPlainText().trimmed();
    if (!existing.isEmpty())
        existing += '\n';
    m_downloadUrlInput->setPlainText(existing + text);
}

void DownloadTool::onDownloadClear()
{
    m_downloadUrlInput->clear();
}

void DownloadTool::onDownloadOutputBrowse()
{
    QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("选择输出目录"));
    if (!dir.isEmpty())
        m_downloadOutputDir->setText(QDir::toNativeSeparators(dir));
}

void DownloadTool::setDownloadUiEnabled(bool enabled)
{
    m_downloadUrlInput->setReadOnly(!enabled);
    m_downloadAddFileBtn->setEnabled(enabled);
    m_downloadPasteBtn->setEnabled(enabled);
    m_downloadClearBtn->setEnabled(enabled);
    m_downloadMaxConcurrent->setEnabled(enabled);
    m_downloadMaxConnections->setEnabled(enabled);
    m_downloadSpeedLimit->setEnabled(enabled);
    m_downloadAllowOverwrite->setEnabled(enabled);
    m_downloadOutputDir->setEnabled(enabled);
    m_downloadOutputBrowse->setEnabled(enabled);
    m_downloadStartBtn->setEnabled(enabled);
    m_downloadCancelBtn->setEnabled(!enabled);
}

void DownloadTool::onDownloadStart()
{
    QString text = m_downloadUrlInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请输入下载地址"));
        return;
    }

    QStringList lines = text.split('\n', Qt::SkipEmptyParts);
    m_downloadEntries.clear();
    for (const QString &line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;

        DownloadEntry entry;
        int sepIdx = trimmed.indexOf(QRegularExpression(QStringLiteral("[ \t]")));
        if (sepIdx > 0) {
            entry.url = trimmed.left(sepIdx).trimmed();
            entry.filename = trimmed.mid(sepIdx + 1).trimmed();
        } else {
            entry.url = trimmed;
        }
        entry.row = -1;
        entry.completed = false;
        entry.failed = false;
        entry.percent = 0;
        entry.totalBytes = -1;
        entry.speedBytes = 0;
        entry.lastProgressMs = 0;

        if (entry.url.contains(QStringLiteral("://")))
            m_downloadEntries.append(entry);
    }

    if (m_downloadEntries.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("未检测到有效的下载地址"));
        return;
    }

    QString outputDir = m_downloadOutputDir->text().trimmed();
    if (outputDir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("提示"),
            QStringLiteral("请先选择输出目录"));
        return;
    }
    QDir().mkpath(outputDir);
    m_downloadOutputDirPath = outputDir;

    m_downloadTempFile = QDir::tempPath() + QStringLiteral("/zane_download_urls.txt");
    QFile tempFile(m_downloadTempFile);
    if (!tempFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        QMessageBox::warning(this, QStringLiteral("错误"),
            QStringLiteral("无法写入临时文件"));
        return;
    }

    QTextStream out(&tempFile);
    for (const auto &entry : m_downloadEntries) {
        out << entry.url << '\n';
        if (!entry.filename.isEmpty())
            out << "  out=" << entry.filename << '\n';
    }
    tempFile.close();

    m_downloadProgressTable->setRowCount(0);
    m_downloadBars.clear();
    for (int i = 0; i < m_downloadEntries.size(); i++) {
        DownloadEntry &entry = m_downloadEntries[i];
        int row = m_downloadProgressTable->rowCount();
        m_downloadProgressTable->insertRow(row);
        entry.row = row;

        QString displayName = entry.filename.isEmpty()
            ? entry.url.section('/', -1).section('?', 0, 0)
            : entry.filename;
        if (displayName.isEmpty()) displayName = entry.url;

        QTableWidgetItem *nameItem = new QTableWidgetItem(displayName);
        nameItem->setToolTip(entry.url);
        m_downloadProgressTable->setItem(row, 0, nameItem);

        QProgressBar *bar = new QProgressBar();
        bar->setRange(0, 100);
        bar->setValue(0);
        bar->setTextVisible(true);
        bar->setFixedHeight(16);
        QWidget *barContainer = new QWidget();
        QVBoxLayout *barLayout = new QVBoxLayout(barContainer);
        barLayout->setContentsMargins(4, 0, 4, 0);
        barLayout->addWidget(bar);
        barLayout->setAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setCellWidget(row, 1, barContainer);
        m_downloadBars.append(bar);

        QTableWidgetItem *sizeItem = new QTableWidgetItem(QStringLiteral("—"));
        sizeItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 2, sizeItem);

        QTableWidgetItem *speedItem = new QTableWidgetItem(QStringLiteral("—"));
        speedItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 3, speedItem);

        QTableWidgetItem *etaItem = new QTableWidgetItem(QStringLiteral("—"));
        etaItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 4, etaItem);

        QTableWidgetItem *statusItem = new QTableWidgetItem(QStringLiteral("等待中"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_downloadProgressTable->setItem(row, 5, statusItem);
    }

    m_downloadCompleted = 0;
    m_downloadFailed = 0;
    m_downloadCancelling = false;
    m_downloadStdoutBuffer.clear();
    m_gidToIndex.clear();
    m_downloadPendingUrl.clear();
    m_downloadProgressBar->setValue(0);

    QStringList args;
    args << QStringLiteral("--input-file") << QDir::toNativeSeparators(m_downloadTempFile);
    args << QStringLiteral("--dir") << QDir::toNativeSeparators(outputDir);
    args << QStringLiteral("--max-concurrent-downloads")
         << QString::number(m_downloadMaxConcurrent->value());
    args << QStringLiteral("--max-connection-per-server")
         << QString::number(m_downloadMaxConnections->value());

    int speedLimit = m_downloadSpeedLimit->value();
    if (speedLimit > 0)
        args << QStringLiteral("--max-overall-download-limit") << (QString::number(speedLimit) + 'K');

    args << (m_downloadAllowOverwrite->isChecked()
        ? QStringLiteral("--allow-overwrite=true")
        : QStringLiteral("--allow-overwrite=false"));
    args << QStringLiteral("--console-log-level=notice");
    args << QStringLiteral("--summary-interval=1");
    args << QStringLiteral("--enable-color=false");
    args << QStringLiteral("--enable-rpc=false");
    args << QStringLiteral("--show-console-readout=true");
    args << QStringLiteral("--stderr=true");

    if (m_downloadProcess) {
        m_downloadProcess->deleteLater();
        m_downloadProcess = nullptr;
    }

    m_downloadProcess = new QProcess(this);
    m_downloadProcess->setProcessChannelMode(QProcess::MergedChannels);

    connect(m_downloadProcess,
            QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this](int exitCode, QProcess::ExitStatus exitStatus) {
                Q_UNUSED(exitStatus);
                if (m_downloadCancelling) return;

                m_downloadEtaTimer->stop();
                if (!m_downloadStdoutBuffer.isEmpty())
                    onDownloadProcessOutput();

                for (auto &entry : m_downloadEntries) {
                    if (entry.completed || entry.failed || entry.row < 0)
                        continue;

                    QString displayName = entry.filename.isEmpty()
                        ? entry.url.section('/', -1).section('?', 0, 0)
                        : entry.filename;

                    bool fileExists = false;
                    if (!displayName.isEmpty()) {
                        QString filePath = m_downloadOutputDirPath
                            + QStringLiteral("/") + displayName;
                        fileExists = QFileInfo::exists(filePath)
                                     && QFileInfo(filePath).size() > 0;
                    }

                    if (fileExists || exitCode == 0) {
                        entry.completed = true;
                        m_downloadCompleted++;
                        QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 5);
                        if (s) s->setText(QStringLiteral("已完成"));
                        if (entry.row < m_downloadBars.size())
                            m_downloadBars[entry.row]->setValue(100);
                    } else {
                        entry.failed = true;
                        m_downloadFailed++;
                        QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 5);
                        if (s) s->setText(QStringLiteral("失败"));
                    }
                }

                m_downloadProgressBar->setValue(100);

                QString statusText = QStringLiteral("下载结束 — 成功: %1").arg(m_downloadCompleted);
                if (m_downloadFailed > 0)
                    statusText += QStringLiteral(" — 失败: %1").arg(m_downloadFailed);
                m_downloadStatusLabel->setText(statusText);

                Utils::logToFile(QStringLiteral("[DOWNLOAD] Finished: ok=%1 fail=%2 code=%3")
                    .arg(m_downloadCompleted).arg(m_downloadFailed).arg(exitCode));

                if (!m_downloadTempFile.isEmpty()) {
                    QFile::remove(m_downloadTempFile);
                    m_downloadTempFile.clear();
                }

                setDownloadUiEnabled(true);
            });

    connect(m_downloadProcess, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
                Q_UNUSED(error);
                if (!m_downloadCancelling)
                    m_downloadStatusLabel->setText(
                        QStringLiteral("进程错误: ") + m_downloadProcess->errorString());
            });

    connect(m_downloadProcess, &QProcess::readyReadStandardOutput,
            this, &DownloadTool::onDownloadProcessOutput);

    m_downloadProcess->start(m_aria2Path, args);
    m_downloadStatusLabel->setText(QStringLiteral("正在启动下载..."));
    m_downloadEtaTimer->start();

    Utils::logToFile(QStringLiteral("[DOWNLOAD] Start: %1 URLs, dir=%2, conc=%3")
        .arg(m_downloadEntries.size()).arg(outputDir)
        .arg(m_downloadMaxConcurrent->value()));

    setDownloadUiEnabled(false);
}

void DownloadTool::onDownloadCancel()
{
    m_downloadCancelling = true;
    m_downloadEtaTimer->stop();
    m_downloadStatusLabel->setText(QStringLiteral("正在取消..."));
    m_downloadCancelBtn->setEnabled(false);

    if (m_downloadProcess && m_downloadProcess->state() != QProcess::NotRunning) {
        m_downloadProcess->kill();
        m_downloadProcess->waitForFinished(3000);
    }

    if (!m_downloadTempFile.isEmpty()) {
        QFile::remove(m_downloadTempFile);
        m_downloadTempFile.clear();
    }

    Utils::logToFile(QStringLiteral("[DOWNLOAD] Cancelled"));
    setDownloadUiEnabled(true);
    m_downloadStatusLabel->setText(QStringLiteral("已取消"));
}

void DownloadTool::onDownloadEtaTick()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (auto &entry : m_downloadEntries) {
        if (entry.row < 0 || entry.completed || entry.failed)
            continue;
        if (entry.totalBytes <= 0 || entry.speedBytes <= 0
            || entry.percent >= 100 || entry.lastProgressMs <= 0)
            continue;

        qint64 remainAtUpdate = entry.totalBytes * (100 - entry.percent) / 100;
        qint64 elapsedBytes = entry.speedBytes * (now - entry.lastProgressMs) / 1000;
        qint64 remain = remainAtUpdate - elapsedBytes;
        if (remain < 0) remain = 0;

        QTableWidgetItem *etaItem = m_downloadProgressTable->item(entry.row, 4);
        if (etaItem)
            etaItem->setText(formatEtaText(remain / entry.speedBytes));
    }
}

void DownloadTool::onDownloadProcessOutput()
{
    if (!m_downloadProcess) return;

    m_downloadStdoutBuffer += QString::fromUtf8(
        m_downloadProcess->readAllStandardOutput());

    static const QRegularExpression progressRe(
        QStringLiteral("\\[#(\\S+)\\s+\\S+/(\\S+)\\((\\d+)%\\)[^\\]]*?(?:DL|SPD):(\\S+)(?:\\s*ETA:(\\S*))?\\]"));

    while (true) {
        int idx = m_downloadStdoutBuffer.indexOf('\n');
        if (idx < 0) {
            idx = m_downloadStdoutBuffer.indexOf('\r');
            if (idx < 0) break;
            if (idx + 1 < m_downloadStdoutBuffer.size()
                && m_downloadStdoutBuffer[idx + 1] == '\n')
                idx++;
        }

        QString line = m_downloadStdoutBuffer.left(idx).trimmed();
        m_downloadStdoutBuffer = m_downloadStdoutBuffer.mid(idx + 1);
        if (line.isEmpty()) continue;

        Utils::logToFile(QStringLiteral("[DOWNLOAD][RAW] ") + line);

        int dlPos = line.indexOf(QStringLiteral("Downloading:"));
        if (dlPos >= 0) {
            QString url = line.mid(dlPos + 12).trimmed();
            m_downloadPendingUrl = url;
            for (auto &entry : m_downloadEntries) {
                if (entry.row >= 0 && entry.url == url) {
                    QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 5);
                    if (s && s->text() == QStringLiteral("等待中"))
                        s->setText(QStringLiteral("下载中"));
                    break;
                }
            }
            continue;
        }

        int donePos = line.indexOf(QStringLiteral("Download complete:"));
        if (donePos >= 0) {
            QString path = line.mid(donePos + 18).trimmed();
            QString fileName = path.section(QRegularExpression(QStringLiteral("[/\\\\]")), -1);
            for (auto &entry : m_downloadEntries) {
                if (entry.row < 0 || entry.completed) continue;
                QString displayName = entry.filename.isEmpty()
                    ? entry.url.section('/', -1).section('?', 0, 0)
                    : entry.filename;
                if (displayName != fileName) continue;

                entry.completed = true;
                m_downloadCompleted++;
                QTableWidgetItem *s = m_downloadProgressTable->item(entry.row, 5);
                if (s) s->setText(QStringLiteral("已完成"));
                if (entry.row < m_downloadBars.size())
                    m_downloadBars[entry.row]->setValue(100);

                int totalPercent = 0;
                for (const auto &e : m_downloadEntries) {
                    if (e.row >= 0 && e.row < m_downloadBars.size())
                        totalPercent += m_downloadBars[e.row]->value();
                }
                m_downloadProgressBar->setValue(m_downloadEntries.size() > 0
                    ? totalPercent / m_downloadEntries.size() : 0);
                break;
            }
            continue;
        }

        QRegularExpressionMatch m = progressRe.match(line);
        if (m.hasMatch()) {
            QString gid = m.captured(1);
            int percent = m.captured(3).toInt();
            QString speed = m.captured(4);

            int entryIdx = -1;
            if (m_gidToIndex.contains(gid)) {
                entryIdx = m_gidToIndex[gid];
            } else if (!m_downloadPendingUrl.isEmpty()) {
                for (int i = 0; i < m_downloadEntries.size(); i++) {
                    if (m_downloadEntries[i].row >= 0
                        && m_downloadEntries[i].url == m_downloadPendingUrl) {
                        m_gidToIndex[gid] = i;
                        entryIdx = i;
                        m_downloadPendingUrl.clear();
                        break;
                    }
                }
            }

            if (entryIdx < 0) {
                for (int i = 0; i < m_downloadEntries.size(); i++) {
                    if (m_downloadEntries[i].row >= 0 && !m_downloadEntries[i].completed) {
                        bool alreadyMapped = false;
                        for (auto it = m_gidToIndex.cbegin(); it != m_gidToIndex.cend(); ++it) {
                            if (it.value() == i) { alreadyMapped = true; break; }
                        }
                        if (!alreadyMapped) {
                            m_gidToIndex[gid] = i;
                            entryIdx = i;
                            break;
                        }
                    }
                }
            }

            if (entryIdx < 0 || entryIdx >= m_downloadEntries.size()) continue;

            DownloadEntry &entry = m_downloadEntries[entryIdx];
            if (entry.row < 0 || entry.completed) continue;

            QTableWidgetItem *statusItem = m_downloadProgressTable->item(entry.row, 5);
            if (statusItem && statusItem->text() == QStringLiteral("等待中"))
                statusItem->setText(QStringLiteral("下载中"));

            QProgressBar *bar = entry.row < m_downloadBars.size()
                ? m_downloadBars[entry.row] : nullptr;
            if (bar && bar->value() < 100)
                bar->setValue(percent);

            qint64 parsedTotal = parseAria2Size(m.captured(2));
            if (parsedTotal > 0) entry.totalBytes = parsedTotal;
            entry.speedBytes = parseAria2Size(speed);
            if (entry.speedBytes < 0) entry.speedBytes = 0;
            entry.percent = percent;
            entry.lastProgressMs = QDateTime::currentMSecsSinceEpoch();

            QTableWidgetItem *sizeItem = m_downloadProgressTable->item(entry.row, 2);
            if (sizeItem && sizeItem->text() == QStringLiteral("—")
                && m.captured(2) != QStringLiteral("0B"))
                sizeItem->setText(m.captured(2));

            QTableWidgetItem *speedItem = m_downloadProgressTable->item(entry.row, 3);
            if (speedItem) speedItem->setText(speed + QStringLiteral("/s"));

            QTableWidgetItem *etaItem = m_downloadProgressTable->item(entry.row, 4);
            if (etaItem) {
                QString etaText = QStringLiteral("—");
                if (entry.totalBytes > 0 && entry.speedBytes > 0 && percent < 100) {
                    qint64 remain = entry.totalBytes * (100 - percent) / 100;
                    etaText = formatEtaText(remain / entry.speedBytes);
                }
                etaItem->setText(etaText);
            }

            if (percent >= 100 && !entry.completed) {
                entry.completed = true;
                m_downloadCompleted++;
                if (statusItem) statusItem->setText(QStringLiteral("已完成"));
                Utils::logToFile(QStringLiteral("[DOWNLOAD] OK: %1").arg(entry.url));
            }

            int totalPercent = 0;
            for (const auto &e : m_downloadEntries) {
                if (e.row >= 0 && e.row < m_downloadBars.size()) {
                    totalPercent += m_downloadBars[e.row]->value();
                }
            }
            int count = m_downloadEntries.size();
            m_downloadProgressBar->setValue(count > 0 ? totalPercent / count : 0);

            QString statusText = QStringLiteral("下载中 — 完成: %1/%2")
                .arg(m_downloadCompleted).arg(m_downloadEntries.size());
            if (m_downloadFailed > 0)
                statusText += QStringLiteral(" — 失败: %1").arg(m_downloadFailed);
            m_downloadStatusLabel->setText(statusText);
            continue;
        }

        if (line.contains(QStringLiteral("[ERROR]"), Qt::CaseInsensitive)
            || line.contains(QStringLiteral("error:"), Qt::CaseInsensitive)) {
            Utils::logToFile(QStringLiteral("[DOWNLOAD] ") + line);
        }
    }
}
